#ifndef DATABASE_H
#define DATABASE_H

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>
#include <QVector>

struct TrackPoint
{
    qint64 id = 0;
    qint64 timestamp = 0;   // seconds since epoch
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    double bearing = 0.0;
    double accuracy = -1.0; // negative: unknown
    int battery = -1;
    int charging = -1;
    bool uploaded = false;
};

class Database
{
public:
    Database();
    ~Database();

    bool open();
    void close();

    qint64 insertPoint(const TrackPoint &point);

    QVector<TrackPoint> pendingPoints(int limit);
    void markUploaded(qint64 id);
    void setPointError(qint64 id, const QString &error);

    int pendingCount() const;
    int pointsToday() const;
    int uploadedToday() const;
    int pointsSince(const QDateTime &from) const;

    QVector<TrackPoint> pointsBetween(const QDateTime &from, const QDateTime &to) const;
    QVariantMap lastPoint() const;

    int pruneBefore(qint64 timestamp);
    void checkpoint();

    QString lastError() const;

private:
    bool exec(const QString &sql) const;
    bool migrate();
    void ensureColumn(const QString &name, const QString &definition);
    int userVersion() const;

    QSqlDatabase m_db;
    QString m_lastError;
};

#endif // DATABASE_H
