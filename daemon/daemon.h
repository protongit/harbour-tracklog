#ifndef DAEMON_H
#define DAEMON_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "config.h"
#include "database.h"

class Tracker;
class Uploader;

class Daemon : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hensip.tracklog.Control")

public:
    explicit Daemon(QObject *parent = nullptr);
    ~Daemon() override;

    bool init();
    void startTrackingIfConfigured();

public slots:
    void Start();
    void Stop();
    void Quit();
    QVariantMap GetStatus();
    QVariantMap GetConfig();
    bool SetConfig(const QVariantMap &config);
    QVariantMap TestUpload();
    QVariantList GetPoints(qint64 from, qint64 to, int limit);
    QString ExportGpx(qint64 from, qint64 to);
    int ImportGpx(const QString &path);

signals:
    void StatusChanged(const QVariantMap &status);
    void PointAdded(qint64 id, double latitude, double longitude, qint64 timestamp);

private slots:
    void onPointRecorded(qint64 id, double latitude, double longitude, qint64 timestamp,
                         double accuracy);
    void onUploadStatusChanged();
    void broadcastStatus();
    void runRetention();

private:
    QVariantMap statusMap();
    void notify(const QString &summary, const QString &body);
    void checkGeofence(double latitude, double longitude);

    Config m_config;
    Database m_db;
    Tracker *m_tracker = nullptr;
    Uploader *m_uploader = nullptr;
    QTimer m_statusTimer;
    QTimer m_retentionTimer;
    double m_lastLat = 0.0;
    double m_lastLon = 0.0;
    qint64 m_lastTs = 0;
    int m_insideFence = -1;
};

#endif // DAEMON_H
