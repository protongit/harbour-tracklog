#include "daemon.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGeoCoordinate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "tracker.h"
#include "tracklogpaths.h"
#include "uploader.h"

static QVariant accuracyVariant(double accuracy)
{
    return accuracy < 0.0 ? QVariant(QVariant::Double) : QVariant(accuracy);
}

Daemon::Daemon(QObject *parent)
    : QObject(parent)
{
}

Daemon::~Daemon()
{
}

bool Daemon::init()
{
    TrackLog::ensureDataDir();

    if (!m_db.open()) {
        qCritical() << "tracklogd: cannot open database:" << m_db.lastError();
        return false;
    }

    m_tracker = new Tracker(&m_db, &m_config, this);
    m_uploader = new Uploader(&m_db, &m_config, this);

    connect(m_tracker, &Tracker::pointRecorded, this, &Daemon::onPointRecorded);
    connect(m_tracker, &Tracker::statusChanged, this, &Daemon::broadcastStatus);
    connect(m_uploader, &Uploader::statusChanged, this, &Daemon::onUploadStatusChanged);

    m_statusTimer.setInterval(10000);
    connect(&m_statusTimer, &QTimer::timeout, this, &Daemon::broadcastStatus);
    m_statusTimer.start();

    m_retentionTimer.setInterval(24 * 3600 * 1000);
    connect(&m_retentionTimer, &QTimer::timeout, this, &Daemon::runRetention);
    m_retentionTimer.start();
    QTimer::singleShot(60000, this, SLOT(runRetention()));

    return true;
}

void Daemon::startTrackingIfConfigured()
{
    if (m_config.autoStart() && m_config.tracking()) {
        m_tracker->start();
    } else {
        qInfo() << "tracklogd: tracking disabled at startup";
    }
    m_uploader->kick();
}

void Daemon::Start()
{
    m_config.setTracking(true);
    m_tracker->start();
    broadcastStatus();
}

void Daemon::Stop()
{
    m_config.setTracking(false);
    m_tracker->stop();
    broadcastStatus();
}

void Daemon::Quit()
{
    qInfo() << "tracklogd: quit requested";
    if (m_tracker) {
        m_tracker->stop();
    }
    m_db.checkpoint();
    m_db.close();
    QCoreApplication::quit();
}

QVariantMap Daemon::GetStatus()
{
    return statusMap();
}

QVariantMap Daemon::GetConfig()
{
    return m_config.toMap();
}

bool Daemon::SetConfig(const QVariantMap &config)
{
    const bool changed = m_config.updateFromMap(config);
    if (!changed) {
        return true;
    }

    m_tracker->restartTimer();
    m_uploader->reset();
    m_uploader->kick();
    broadcastStatus();
    return true;
}

QVariantMap Daemon::TestUpload()
{
    QVariantMap result;

    TrackPoint point;
    const QVariantMap last = m_db.lastPoint();
    if (!last.isEmpty()) {
        point.latitude = last.value(QStringLiteral("lat")).toDouble();
        point.longitude = last.value(QStringLiteral("lon")).toDouble();
        point.accuracy = last.value(QStringLiteral("acc"), -1.0).toDouble();
    }
    point.timestamp = QDateTime::currentMSecsSinceEpoch() / 1000;
    point.speed = 0.0;

    const QString url = Uploader::buildUrl(m_config, point);
    if (url.isEmpty()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("La URL está vacía"));
        return result;
    }

    QNetworkAccessManager nam;
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString::fromLatin1(TrackLog::userAgent()));

    QNetworkReply *reply = nam.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(8000);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = reply->error() == QNetworkReply::NoError && code >= 200 && code < 300;
    QString error;
    if (!ok) {
        error = reply->errorString();
        if (code > 0) {
            error += QStringLiteral(" (HTTP %1)").arg(code);
        }
    }

    qInfo() << "tracklogd: upload test" << ok << code << error;

    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("code"), code);
    result.insert(QStringLiteral("error"), error);
    result.insert(QStringLiteral("url"), url);
    reply->deleteLater();
    return result;
}

QVariantList Daemon::GetPoints(qint64 from, qint64 to, int limit)
{
    QVariantList list;
    const QVector<TrackPoint> points = m_db.pointsBetween(
                QDateTime::fromMSecsSinceEpoch(from * 1000),
                QDateTime::fromMSecsSinceEpoch(to * 1000));
    int count = 0;
    for (const TrackPoint &point : points) {
        if (limit > 0 && count >= limit) {
            break;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("ts"), point.timestamp);
        entry.insert(QStringLiteral("lat"), point.latitude);
        entry.insert(QStringLiteral("lon"), point.longitude);
        entry.insert(QStringLiteral("alt"), point.altitude);
        entry.insert(QStringLiteral("speed"), point.speed);
        entry.insert(QStringLiteral("acc"), point.accuracy);
        entry.insert(QStringLiteral("uploaded"), point.uploaded);
        list.append(entry);
        ++count;
    }
    return list;
}

QString Daemon::ExportGpx(qint64 from, qint64 to)
{
    const QVector<TrackPoint> points = m_db.pointsBetween(
                QDateTime::fromMSecsSinceEpoch(from * 1000),
                QDateTime::fromMSecsSinceEpoch(to * 1000));
    if (points.isEmpty()) {
        return QString();
    }

    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
        return QString();
    }
    const QString dir = base + QStringLiteral("/TrackLog");
    if (!QDir().mkpath(dir)) {
        return QString();
    }

    const QString path = dir + QStringLiteral("/tracklog-")
            + QDateTime::fromMSecsSinceEpoch(points.first().timestamp * 1000)
              .toString(QStringLiteral("yyyyMMdd-HHmm"))
            + QStringLiteral(".gpx");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QString();
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("gpx"));
    xml.writeDefaultNamespace(QStringLiteral("http://www.topografix.com/GPX/1/1"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.1"));
    xml.writeAttribute(QStringLiteral("creator"), QStringLiteral("TrackLog"));
    xml.writeStartElement(QStringLiteral("trk"));
    xml.writeTextElement(QStringLiteral("name"), QStringLiteral("TrackLog"));
    xml.writeStartElement(QStringLiteral("trkseg"));

    qint64 previous = 0;
    for (const TrackPoint &point : points) {
        // Start a new segment after a long gap so the track is not drawn as a
        // straight line across the hole.
        if (previous > 0 && point.timestamp - previous > 600 && !xml.hasError()) {
            xml.writeEndElement(); // trkseg
            xml.writeStartElement(QStringLiteral("trkseg"));
        }
        previous = point.timestamp;

        xml.writeStartElement(QStringLiteral("trkpt"));
        xml.writeAttribute(QStringLiteral("lat"), QString::number(point.latitude, 'f', 7));
        xml.writeAttribute(QStringLiteral("lon"), QString::number(point.longitude, 'f', 7));
        xml.writeTextElement(QStringLiteral("ele"), QString::number(point.altitude, 'f', 1));
        xml.writeTextElement(QStringLiteral("time"),
                             QDateTime::fromMSecsSinceEpoch(point.timestamp * 1000, Qt::UTC)
                             .toString(Qt::ISODate));
        xml.writeEndElement(); // trkpt
    }

    xml.writeEndElement(); // trkseg
    xml.writeEndElement(); // trk
    xml.writeEndElement(); // gpx
    xml.writeEndDocument();
    file.close();
    return path;
}

int Daemon::ImportGpx(const QString &path)
{
    if (!path.startsWith(QDir::homePath())) {
        qWarning() << "tracklogd: refusing to import outside home:" << path;
        return 0;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }

    QXmlStreamReader xml(&file);
    int imported = 0;

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("trkpt")) {
            continue;
        }

        const QString latText = xml.attributes().value(QStringLiteral("lat")).toString();
        const QString lonText = xml.attributes().value(QStringLiteral("lon")).toString();
        if (latText.isEmpty() || lonText.isEmpty()) {
            continue;
        }

        TrackPoint point;
        point.latitude = latText.toDouble();
        point.longitude = lonText.toDouble();
        point.accuracy = -1.0;
        point.battery = -1;
        point.charging = -1;

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isEndElement() && xml.name() == QLatin1String("trkpt")) {
                break;
            }
            if (!xml.isStartElement()) {
                continue;
            }
            const QStringRef name = xml.name();
            if (name == QLatin1String("time")) {
                point.timestamp = QDateTime::fromString(xml.readElementText(),
                                                        Qt::ISODate).toMSecsSinceEpoch() / 1000;
            } else if (name == QLatin1String("ele")) {
                point.altitude = xml.readElementText().toDouble();
            }
        }

        if (point.timestamp <= 0) {
            continue;
        }
        if (m_db.insertPoint(point) > 0) {
            ++imported;
        }
    }

    qInfo() << "tracklogd: imported" << imported << "points from" << path;
    if (imported > 0) {
        m_uploader->kick();
    }
    return imported;
}

void Daemon::runRetention()
{
    const int days = m_config.retentionDays();
    if (days <= 0) {
        return;
    }
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() / 1000 - days * 86400;
    const int removed = m_db.pruneBefore(cutoff);
    if (removed > 0) {
        qInfo() << "tracklogd: retention removed" << removed << "points";
    }
}

void Daemon::onPointRecorded(qint64 id, double latitude, double longitude, qint64 timestamp,
                             double accuracy)
{
    Q_UNUSED(accuracy)
    m_lastLat = latitude;
    m_lastLon = longitude;
    m_lastTs = timestamp;

    emit PointAdded(id, latitude, longitude, timestamp);
    checkGeofence(latitude, longitude);
    broadcastStatus();
    m_uploader->kick();
}

void Daemon::onUploadStatusChanged()
{
    broadcastStatus();
}

void Daemon::broadcastStatus()
{
    emit StatusChanged(statusMap());
}

void Daemon::checkGeofence(double latitude, double longitude)
{
    if (!m_config.geofenceEnabled()) {
        m_insideFence = -1;
        return;
    }

    const QGeoCoordinate center(m_config.geofenceLat(), m_config.geofenceLon());
    if (!center.isValid()) {
        return;
    }

    const QGeoCoordinate current(latitude, longitude);
    const bool inside = current.distanceTo(center) <= m_config.geofenceRadius();
    const QString name = m_config.geofenceName();

    if (m_insideFence == 0 && inside) {
        notify(QStringLiteral("Has llegado a ") + name,
               QStringLiteral("TrackLog ha detectado la entrada"));
    } else if (m_insideFence == 1 && !inside) {
        notify(QStringLiteral("Has salido de ") + name,
               QStringLiteral("TrackLog ha detectado la salida"));
    }
    m_insideFence = inside ? 1 : 0;
}

void Daemon::notify(const QString &summary, const QString &body)
{
    QDBusInterface notifications(QStringLiteral("org.freedesktop.Notifications"),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QStringLiteral("org.freedesktop.Notifications"),
                                 QDBusConnection::sessionBus(), this);
    notifications.setTimeout(3000);
    notifications.call(QStringLiteral("Notify"),
                       QStringLiteral("TrackLog"), 0u, QString(),
                       summary, body, QStringList(), QVariantMap(), 5000);
}

QVariantMap Daemon::statusMap()
{
    QVariantMap status;

    status.insert(QStringLiteral("tracking"), m_tracker && m_tracker->active());
    status.insert(QStringLiteral("fixing"), m_tracker && m_tracker->hasFix());
    status.insert(QStringLiteral("stationary"), m_tracker && m_tracker->stationary());
    status.insert(QStringLiteral("accuracy"), m_tracker ? m_tracker->accuracy() : -1.0);
    status.insert(QStringLiteral("lastFixTs"), m_tracker ? m_tracker->lastFixTimestamp() : 0);
    status.insert(QStringLiteral("battery"), m_tracker ? m_tracker->battery() : -1);
    status.insert(QStringLiteral("charging"), m_tracker && m_tracker->charging());
    status.insert(QStringLiteral("lastLat"), m_lastLat);
    status.insert(QStringLiteral("lastLon"), m_lastLon);
    status.insert(QStringLiteral("lastTs"), m_lastTs);
    status.insert(QStringLiteral("pointsToday"), m_db.pointsToday());
    status.insert(QStringLiteral("uploadedToday"), m_db.uploadedToday());
    status.insert(QStringLiteral("pending"), m_db.pendingCount());
    status.insert(QStringLiteral("recorded"), m_tracker ? m_tracker->recordedCount() : 0);
    status.insert(QStringLiteral("ignored"), m_tracker ? m_tracker->ignoredCount() : 0);
    status.insert(QStringLiteral("lastError"), m_uploader ? m_uploader->lastError() : QString());
    status.insert(QStringLiteral("uploadedSession"), m_uploader ? m_uploader->uploadedCount() : 0);
    status.insert(QStringLiteral("intervalSec"), m_config.intervalSec());
    status.insert(QStringLiteral("minAccuracyM"), m_config.minAccuracyM());
    status.insert(QStringLiteral("minDistanceM"), m_config.minDistanceM());
    status.insert(QStringLiteral("pauseBelowBattery"), m_config.pauseBelowBattery());
    status.insert(QStringLiteral("retentionDays"), m_config.retentionDays());
    status.insert(QStringLiteral("uploadEnabled"), m_config.uploadEnabled());
    status.insert(QStringLiteral("url"), m_config.url());
    status.insert(QStringLiteral("id"), m_config.deviceId());
    status.insert(QStringLiteral("traccar"), m_config.traccar());
    status.insert(QStringLiteral("autoStart"), m_config.autoStart());
    status.insert(QStringLiteral("geofenceEnabled"), m_config.geofenceEnabled());
    status.insert(QStringLiteral("geofenceLat"), m_config.geofenceLat());
    status.insert(QStringLiteral("geofenceLon"), m_config.geofenceLon());
    status.insert(QStringLiteral("geofenceRadius"), m_config.geofenceRadius());
    status.insert(QStringLiteral("geofenceName"), m_config.geofenceName());
    status.insert(QStringLiteral("db"), TrackLog::dbPath());
    return status;
}
