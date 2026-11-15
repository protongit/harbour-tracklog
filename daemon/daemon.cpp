#include "daemon.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>

#include "tracker.h"
#include "tracklogpaths.h"
#include "uploader.h"

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

void Daemon::onPointRecorded(qint64 id, double latitude, double longitude, qint64 timestamp)
{
    m_lastLat = latitude;
    m_lastLon = longitude;
    m_lastTs = timestamp;
    emit PointAdded(id, latitude, longitude, timestamp);
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

QVariantMap Daemon::statusMap()
{
    QVariantMap status;

    status.insert(QStringLiteral("tracking"), m_tracker && m_tracker->active());
    status.insert(QStringLiteral("fixing"), m_tracker && m_tracker->hasFix());
    status.insert(QStringLiteral("accuracy"), m_tracker ? m_tracker->accuracy() : -1.0);
    status.insert(QStringLiteral("lastFixTs"), m_tracker ? m_tracker->lastFixTimestamp() : 0);
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
    status.insert(QStringLiteral("uploadEnabled"), m_config.uploadEnabled());
    status.insert(QStringLiteral("url"), m_config.url());
    status.insert(QStringLiteral("id"), m_config.deviceId());
    status.insert(QStringLiteral("traccar"), m_config.traccar());
    status.insert(QStringLiteral("autoStart"), m_config.autoStart());
    status.insert(QStringLiteral("db"), TrackLog::dbPath());
    return status;
}
