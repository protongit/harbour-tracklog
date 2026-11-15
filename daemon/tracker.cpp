#include "tracker.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDebug>
#include <QGeoCoordinate>

#include <cmath>

#include "config.h"
#include "database.h"

Tracker::Tracker(Database *db, Config *config, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_config(config)
{
    m_tick.setSingleShot(false);
    connect(&m_tick, &QTimer::timeout, this, &Tracker::onTick);

    m_batteryTimer.setInterval(5 * 60 * 1000);
    connect(&m_batteryTimer, &QTimer::timeout, this, &Tracker::refreshBattery);

    m_mce = new QDBusInterface(QStringLiteral("com.nokia.mce"),
                               QStringLiteral("/com/nokia/mce/request"),
                               QStringLiteral("com.nokia.mce.request"),
                               QDBusConnection::systemBus(), this);
    m_mce->setTimeout(2000);

    m_source = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_source) {
        m_source->setUpdateInterval(5000);
        connect(m_source, &QGeoPositionInfoSource::positionUpdated,
                this, &Tracker::onPositionUpdated);
        connect(m_source, &QGeoPositionInfoSource::updateTimeout, this, [this]() {
            emit statusChanged();
        });
    } else {
        qWarning() << "tracklogd: no position source available";
    }
}

bool Tracker::active() const
{
    return m_active;
}

bool Tracker::hasFix() const
{
    return m_latest.isValid() && m_latest.coordinate().isValid();
}

bool Tracker::stationary() const
{
    return m_stationaryCount >= 3;
}

double Tracker::accuracy() const
{
    if (!hasFix()) {
        return -1.0;
    }
    const double value = m_latest.attribute(QGeoPositionInfo::HorizontalAccuracy);
    return std::isfinite(value) && value > 0.0 ? value : -1.0;
}

qint64 Tracker::lastFixTimestamp() const
{
    if (!m_latest.isValid()) {
        return 0;
    }
    return m_latest.timestamp().toMSecsSinceEpoch() / 1000;
}

int Tracker::recordedCount() const
{
    return m_recorded;
}

int Tracker::ignoredCount() const
{
    return m_ignored;
}

int Tracker::battery() const
{
    return m_battery;
}

bool Tracker::charging() const
{
    return m_charging == 1;
}

void Tracker::setSourceActive(bool active)
{
    if (!m_source) {
        return;
    }
    if (active) {
        m_source->startUpdates();
    } else {
        m_source->stopUpdates();
    }
}

void Tracker::refreshBattery()
{
    if (!m_mce || !m_mce->isValid()) {
        return;
    }

    QDBusMessage level = m_mce->call(QStringLiteral("get_battery_level"));
    if (level.type() == QDBusMessage::ReplyMessage && !level.arguments().isEmpty()) {
        const int value = level.arguments().first().toString().toInt();
        if (value >= 0 && value <= 100) {
            m_battery = value;
        }
    }

    QDBusMessage state = m_mce->call(QStringLiteral("get_battery_state"));
    if (state.type() == QDBusMessage::ReplyMessage && !state.arguments().isEmpty()) {
        const QString value = state.arguments().first().toString();
        if (value == QLatin1String("charging") || value == QLatin1String("full")) {
            m_charging = 1;
        } else if (value == QLatin1String("discharging")
                   || value == QLatin1String("not_charging")) {
            m_charging = 0;
        }
    }

    emit statusChanged();
}

void Tracker::updateTimerInterval()
{
    if (!m_active) {
        return;
    }
    const int base = m_config->intervalSec();
    int interval = base;
    if (m_stationaryCount >= 3) {
        interval = qMin(base * 4, 600);
    }
    if (!m_tick.isActive() || m_tick.interval() != interval * 1000) {
        m_tick.start(interval * 1000);
        qInfo() << "tracklogd: capture interval now" << interval << "s";
    }
}

void Tracker::start()
{
    if (!m_source) {
        qWarning() << "tracklogd: cannot start tracking without position source";
        return;
    }
    m_recordNext = true;
    m_active = true;
    m_stationaryCount = 0;
    setSourceActive(true);
    refreshBattery();
    m_batteryTimer.start();
    updateTimerInterval();
    qInfo() << "tracklogd: tracking started, interval" << m_config->intervalSec() << "s";
    emit statusChanged();
}

void Tracker::stop()
{
    m_tick.stop();
    m_batteryTimer.stop();
    m_active = false;
    setSourceActive(false);
    qInfo() << "tracklogd: tracking stopped";
    emit statusChanged();
}

void Tracker::restartTimer()
{
    m_stationaryCount = 0;
    updateTimerInterval();
}

void Tracker::onPositionUpdated(const QGeoPositionInfo &info)
{
    if (!info.isValid() || !info.coordinate().isValid()) {
        return;
    }
    m_latest = info;
    if (m_recordNext) {
        m_recordNext = false;
        recordCurrent();
    }
    emit statusChanged();
}

void Tracker::onTick()
{
    recordCurrent();
}

bool Tracker::recordCurrent()
{
    if (!hasFix()) {
        return false;
    }

    const int pauseBelow = m_config->pauseBelowBattery();
    if (pauseBelow > 0 && m_battery >= 0 && m_battery < pauseBelow && m_charging != 1) {
        emit statusChanged();
        return false;
    }

    const QGeoCoordinate coordinate = m_latest.coordinate();
    double acc = m_latest.attribute(QGeoPositionInfo::HorizontalAccuracy);
    const bool accValid = std::isfinite(acc) && acc > 0.0;
    if (accValid && acc > m_config->minAccuracyM()) {
        ++m_ignored;
        emit statusChanged();
        return false;
    }

    // Without a reported accuracy, require at least 25 m of movement so a
    // stationary phone does not fill the database with duplicates.
    const double minimum = qMax(static_cast<double>(m_config->minDistanceM()),
                                accValid ? acc : 25.0);

    if (m_haveLast) {
        const double distance = m_lastRecorded.distanceTo(coordinate);
        if (distance < minimum) {
            ++m_stationaryCount;
            updateTimerInterval();
            emit statusChanged();
            return false;
        }
    }

    TrackPoint point;
    point.timestamp = QDateTime::currentMSecsSinceEpoch() / 1000;
    point.latitude = coordinate.latitude();
    point.longitude = coordinate.longitude();
    point.altitude = coordinate.altitude();
    point.speed = m_latest.attribute(QGeoPositionInfo::GroundSpeed);
    point.bearing = m_latest.attribute(QGeoPositionInfo::Direction);
    point.accuracy = accValid ? acc : -1.0;
    point.battery = m_battery;
    point.charging = m_charging;

    const qint64 id = m_db->insertPoint(point);
    if (id <= 0) {
        qWarning() << "tracklogd: failed to store point" << m_db->lastError();
        return false;
    }

    m_lastRecorded = coordinate;
    m_haveLast = true;
    m_stationaryCount = 0;
    ++m_recorded;
    qInfo() << "tracklogd: recorded point" << id << coordinate.latitude()
            << coordinate.longitude() << "acc" << point.accuracy << "battery" << m_battery;
    emit pointRecorded(id, coordinate.latitude(), coordinate.longitude(),
                       point.timestamp, point.accuracy);
    updateTimerInterval();
    emit statusChanged();
    return true;
}
