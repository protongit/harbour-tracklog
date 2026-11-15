#ifndef TRACKER_H
#define TRACKER_H

#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QObject>
#include <QTimer>

class Config;
class Database;
class QDBusInterface;

class Tracker : public QObject
{
    Q_OBJECT
public:
    explicit Tracker(Database *db, Config *config, QObject *parent = nullptr);

    bool active() const;
    bool hasFix() const;
    bool stationary() const;
    double accuracy() const;
    qint64 lastFixTimestamp() const;
    int recordedCount() const;
    int ignoredCount() const;
    int battery() const;
    bool charging() const;

    void start();
    void stop();
    void restartTimer();

signals:
    void pointRecorded(qint64 id, double latitude, double longitude, qint64 timestamp,
                       double accuracy);
    void statusChanged();

private slots:
    void onPositionUpdated(const QGeoPositionInfo &info);
    void onTick();
    void refreshBattery();

private:
    bool recordCurrent();
    void setSourceActive(bool active);
    void updateTimerInterval();

    Database *m_db;
    Config *m_config;
    QGeoPositionInfoSource *m_source = nullptr;
    QDBusInterface *m_mce = nullptr;
    QGeoPositionInfo m_latest;
    QGeoCoordinate m_lastRecorded;
    QTimer m_tick;
    QTimer m_batteryTimer;
    bool m_haveLast = false;
    bool m_recordNext = false;
    bool m_active = false;
    int m_recorded = 0;
    int m_ignored = 0;
    int m_stationaryCount = 0;
    int m_battery = -1;
    int m_charging = -1;
};

#endif // TRACKER_H
