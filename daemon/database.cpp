#include "database.h"

#include <QDate>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "tracklogpaths.h"

static QVariant accuracyVariant(double accuracy)
{
    if (accuracy < 0.0) {
        return QVariant(QVariant::Double);
    }
    return QVariant(accuracy);
}

static double variantAccuracy(const QVariant &value)
{
    return value.isNull() ? -1.0 : value.toDouble();
}

Database::Database()
{
}

Database::~Database()
{
    close();
}

bool Database::open()
{
    TrackLog::ensureDataDir();

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("tracklog"));
    m_db.setDatabaseName(TrackLog::dbPath());
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    exec(QStringLiteral("PRAGMA busy_timeout=5000"));

    return migrate();
}

void Database::close()
{
    if (m_db.isOpen()) {
        checkpoint();
        m_db.close();
    }
}

int Database::userVersion() const
{
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool Database::migrate()
{
    int version = userVersion();

    if (version < 1) {
        exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS points ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " ts INTEGER NOT NULL,"
            " lat REAL NOT NULL,"
            " lon REAL NOT NULL,"
            " alt REAL,"
            " speed REAL,"
            " bearing REAL,"
            " acc REAL,"
            " battery INTEGER DEFAULT -1,"
            " charging INTEGER DEFAULT -1,"
            " provider TEXT,"
            " uploaded INTEGER NOT NULL DEFAULT 0,"
            " upload_ts INTEGER,"
            " error TEXT)"));
        exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_points_ts ON points(ts)"));
        exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_points_uploaded ON points(uploaded, ts)"));

        ensureColumn(QStringLiteral("provider"), QStringLiteral("TEXT"));
        ensureColumn(QStringLiteral("battery"), QStringLiteral("INTEGER DEFAULT -1"));
        ensureColumn(QStringLiteral("charging"), QStringLiteral("INTEGER DEFAULT -1"));

        // Unknown accuracy was stored as 0 before schema version 1.
        exec(QStringLiteral("UPDATE points SET acc = NULL WHERE acc IS NOT NULL AND acc <= 0"));
        exec(QStringLiteral("PRAGMA user_version=1"));
        version = 1;
    }

    return version >= 1;
}

void Database::ensureColumn(const QString &name, const QString &definition)
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(points)"))) {
        return;
    }
    while (query.next()) {
        if (query.value(1).toString() == name) {
            return;
        }
    }
    QSqlQuery alter(m_db);
    alter.exec(QStringLiteral("ALTER TABLE points ADD COLUMN %1 %2").arg(name, definition));
}

bool Database::exec(const QString &sql) const
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

qint64 Database::insertPoint(const TrackPoint &point)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO points (ts, lat, lon, alt, speed, bearing, acc, battery, charging, uploaded)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0)"));
    query.addBindValue(point.timestamp);
    query.addBindValue(point.latitude);
    query.addBindValue(point.longitude);
    query.addBindValue(point.altitude);
    query.addBindValue(point.speed);
    query.addBindValue(point.bearing);
    query.addBindValue(accuracyVariant(point.accuracy));
    query.addBindValue(point.battery);
    query.addBindValue(point.charging);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

QVector<TrackPoint> Database::pendingPoints(int limit)
{
    QVector<TrackPoint> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, ts, lat, lon, alt, speed, bearing, acc FROM points"
        " WHERE uploaded = 0 ORDER BY ts ASC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        TrackPoint point;
        point.id = query.value(0).toLongLong();
        point.timestamp = query.value(1).toLongLong();
        point.latitude = query.value(2).toDouble();
        point.longitude = query.value(3).toDouble();
        point.altitude = query.value(4).toDouble();
        point.speed = query.value(5).toDouble();
        point.bearing = query.value(6).toDouble();
        point.accuracy = variantAccuracy(query.value(7));
        result.append(point);
    }
    return result;
}

void Database::markUploaded(qint64 id)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE points SET uploaded = 1, upload_ts = ?, error = NULL WHERE id = ?"));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch() / 1000);
    query.addBindValue(id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

void Database::setPointError(qint64 id, const QString &error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE points SET error = ? WHERE id = ?"));
    query.addBindValue(error.left(200));
    query.addBindValue(id);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
    }
}

static int countQuery(QSqlDatabase &db, const QString &sql, const QVariantList &binds = QVariantList())
{
    QSqlQuery query(db);
    query.prepare(sql);
    for (const QVariant &value : binds) {
        query.addBindValue(value);
    }
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int Database::pendingCount() const
{
    QSqlDatabase db = m_db;
    return countQuery(db, QStringLiteral("SELECT COUNT(*) FROM points WHERE uploaded = 0"));
}

int Database::pointsToday() const
{
    const qint64 start = QDateTime(QDate::currentDate(), QTime(0, 0)).toMSecsSinceEpoch() / 1000;
    QSqlDatabase db = m_db;
    return countQuery(db, QStringLiteral("SELECT COUNT(*) FROM points WHERE ts >= ?"),
                      QVariantList() << start);
}

int Database::uploadedToday() const
{
    const qint64 start = QDateTime(QDate::currentDate(), QTime(0, 0)).toMSecsSinceEpoch() / 1000;
    QSqlDatabase db = m_db;
    return countQuery(db,
                      QStringLiteral("SELECT COUNT(*) FROM points WHERE uploaded = 1 AND ts >= ?"),
                      QVariantList() << start);
}

int Database::pointsSince(const QDateTime &from) const
{
    QSqlDatabase db = m_db;
    return countQuery(db, QStringLiteral("SELECT COUNT(*) FROM points WHERE ts >= ?"),
                      QVariantList() << from.toMSecsSinceEpoch() / 1000);
}

QVector<TrackPoint> Database::pointsBetween(const QDateTime &from, const QDateTime &to) const
{
    QVector<TrackPoint> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, ts, lat, lon, alt, speed, bearing, acc, uploaded FROM points"
        " WHERE ts >= ? AND ts <= ? ORDER BY ts ASC"));
    query.addBindValue(from.toMSecsSinceEpoch() / 1000);
    query.addBindValue(to.toMSecsSinceEpoch() / 1000);
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        TrackPoint point;
        point.id = query.value(0).toLongLong();
        point.timestamp = query.value(1).toLongLong();
        point.latitude = query.value(2).toDouble();
        point.longitude = query.value(3).toDouble();
        point.altitude = query.value(4).toDouble();
        point.speed = query.value(5).toDouble();
        point.bearing = query.value(6).toDouble();
        point.accuracy = variantAccuracy(query.value(7));
        point.uploaded = query.value(8).toBool();
        result.append(point);
    }
    return result;
}

QVariantMap Database::lastPoint() const
{
    QVariantMap map;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT ts, lat, lon, acc, battery, charging FROM points ORDER BY ts DESC LIMIT 1"))
            && query.next()) {
        map.insert(QStringLiteral("ts"), query.value(0).toLongLong());
        map.insert(QStringLiteral("lat"), query.value(1).toDouble());
        map.insert(QStringLiteral("lon"), query.value(2).toDouble());
        map.insert(QStringLiteral("acc"), variantAccuracy(query.value(3)));
        map.insert(QStringLiteral("battery"), query.value(4).toInt());
        map.insert(QStringLiteral("charging"), query.value(5).toInt());
    }
    return map;
}

int Database::pruneBefore(qint64 timestamp)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM points WHERE ts < ? AND uploaded = 1"));
    query.addBindValue(timestamp);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.numRowsAffected();
}

void Database::checkpoint()
{
    exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
}

QString Database::lastError() const
{
    return m_lastError;
}
