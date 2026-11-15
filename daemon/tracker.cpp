#include "tracker.h"

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

void Tracker::start()
{
    if (!m_source) {
        qWarning() << "tracklogd: cannot start tracking without position source";
        return;
    }
    m_recordNext = true;
    m_active = true;
    setSourceActive(true);
    m_tick.start(m_config->intervalSec() * 1000);
    qInfo() << "tracklogd: tracking started, interval" << m_config->intervalSec() << "s";
    emit statusChanged();
}

void Tracker::stop()
{
    m_tick.stop();
    m_active = false;
    setSourceActive(false);
    qInfo() << "tracklogd: tracking stopped";
    emit statusChanged();
}

void Tracker::restartTimer()
{
    if (m_tick.isActive()) {
        m_tick.start(m_config->intervalSec() * 1000);
    }
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

    const QGeoCoordinate coordinate = m_latest.coordinate();
    double acc = m_latest.attribute(QGeoPositionInfo::HorizontalAccuracy);
    const bool accValid = std::isfinite(acc) && acc > 0.0;
    if (accValid && acc > m_config->minAccuracyM()) {
        ++m_ignored;
        emit statusChanged();
        return false;
    }

    if (m_haveLast) {
        const double distance = m_lastRecorded.distanceTo(coordinate);
        const double minimum = qMax(static_cast<double>(m_config->minDistanceM()),
                                    accValid ? acc : 0.0);
        if (distance < minimum) {
            emit statusChanged();
            return false;
        }
    }

    TrackPoint point;
    point.timestamp = (QDateTime::currentMSecsSinceEpoch() / 1000);
    point.latitude = coordinate.latitude();
    point.longitude = coordinate.longitude();
    point.altitude = coordinate.altitude();
    point.speed = m_latest.attribute(QGeoPositionInfo::GroundSpeed);
    point.bearing = m_latest.attribute(QGeoPositionInfo::Direction);
    point.accuracy = accValid ? acc : 0.0;

    const qint64 id = m_db->insertPoint(point);
    if (id <= 0) {
        qWarning() << "tracklogd: failed to store point" << m_db->lastError();
        return false;
    }

    m_lastRecorded = coordinate;
    m_haveLast = true;
    ++m_recorded;
    qInfo() << "tracklogd: recorded point" << id << coordinate.latitude()
            << coordinate.longitude() << "acc" << acc;
    emit pointRecorded(id, coordinate.latitude(), coordinate.longitude(), point.timestamp);
    emit statusChanged();
    return true;
}
