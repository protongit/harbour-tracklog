#ifndef TRACKER_H
#define TRACKER_H

#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QObject>
#include <QTimer>

class Config;
class Database;

class Tracker : public QObject
{
    Q_OBJECT
public:
    explicit Tracker(Database *db, Config *config, QObject *parent = nullptr);

    bool active() const;
    bool hasFix() const;
    double accuracy() const;
    qint64 lastFixTimestamp() const;
    int recordedCount() const;
    int ignoredCount() const;

    void start();
    void stop();
    void restartTimer();

signals:
    void pointRecorded(qint64 id, double latitude, double longitude, qint64 timestamp);
    void statusChanged();

private slots:
    void onPositionUpdated(const QGeoPositionInfo &info);
    void onTick();

private:
    bool recordCurrent();
    void setSourceActive(bool active);

    Database *m_db;
    Config *m_config;
    QGeoPositionInfoSource *m_source = nullptr;
    QGeoPositionInfo m_latest;
    QGeoCoordinate m_lastRecorded;
    QTimer m_tick;
    bool m_haveLast = false;
    bool m_recordNext = false;
    bool m_active = false;
    int m_recorded = 0;
    int m_ignored = 0;
};

#endif // TRACKER_H
