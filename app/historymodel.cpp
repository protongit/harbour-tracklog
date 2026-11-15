#include "historymodel.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <QVector>
#include <QXmlStreamWriter>
#include <QtMath>

#include "tracklogpaths.h"

HistoryModel::HistoryModel(QObject *parent)
    : QObject(parent)
{
    setPreset(7);
}

HistoryModel::~HistoryModel()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QString HistoryModel::databasePath() const
{
    return TrackLog::dbPath();
}

bool HistoryModel::ensureDatabase()
{
    if (m_db.isOpen()) {
        return true;
    }
    if (!QFileInfo::exists(databasePath())) {
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("history"));
    m_db.setDatabaseName(databasePath());
    m_db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!m_db.open()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
    return true;
}

void HistoryModel::setPreset(int days)
{
    const QDate today = QDate::currentDate();
    m_to = QDateTime(today, QTime(23, 59, 59));

    if (days == 0) {
        m_from = QDateTime(today, QTime(0, 0));
        m_rangeText = QStringLiteral("Hoy");
    } else if (days > 0) {
        m_from = QDateTime(today.addDays(-(days - 1)), QTime(0, 0));
        m_rangeText = QStringLiteral("Últimos %1 días").arg(days);
    } else {
        m_from = QDateTime(QDate(2000, 1, 1), QTime(0, 0));
        m_rangeText = QStringLiteral("Todo");
    }
    reload();
}

void HistoryModel::setRange(const QDateTime &from, const QDateTime &to)
{
    m_from = from;
    m_to = to;
    m_rangeText = from.toString(QStringLiteral("dd/MM/yy"))
            + QStringLiteral(" – ") + to.toString(QStringLiteral("dd/MM/yy"));
    reload();
}

void HistoryModel::setRangeMs(qint64 fromMs, qint64 toMs)
{
    setRange(QDateTime::fromMSecsSinceEpoch(fromMs),
             QDateTime::fromMSecsSinceEpoch(toMs));
}

void HistoryModel::reload()
{
    m_points.clear();
    m_days.clear();
    m_pointCount = 0;
    m_distanceKm = 0.0;

    if (!ensureDatabase()) {
        emit changed();
        return;
    }

    const qint64 from = m_from.toMSecsSinceEpoch() / 1000;
    const qint64 to = m_to.toMSecsSinceEpoch() / 1000;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT ts, lat, lon, speed, acc, uploaded FROM points"
        " WHERE ts >= ? AND ts <= ? ORDER BY ts ASC"));
    query.addBindValue(from);
    query.addBindValue(to);
    if (!query.exec()) {
        emit changed();
        return;
    }

    QVector<QVariantMap> all;
    double previousLat = 0.0;
    double previousLon = 0.0;
    bool havePrevious = false;

    while (query.next()) {
        const qint64 ts = query.value(0).toLongLong();
        const double lat = query.value(1).toDouble();
        const double lon = query.value(2).toDouble();

        QVariantMap point;
        point.insert(QStringLiteral("ts"), ts);
        point.insert(QStringLiteral("lat"), lat);
        point.insert(QStringLiteral("lon"), lon);
        point.insert(QStringLiteral("speed"), query.value(3).toDouble());
        point.insert(QStringLiteral("acc"), query.value(4).toDouble());
        point.insert(QStringLiteral("uploaded"), query.value(5).toInt() == 1);
        all.append(point);
        ++m_pointCount;

        if (havePrevious) {
            const double dLat = qDegreesToRadians(lat - previousLat);
            const double dLon = qDegreesToRadians(lon - previousLon);
            const double a = qSin(dLat / 2) * qSin(dLat / 2)
                    + qCos(qDegreesToRadians(previousLat)) * qCos(qDegreesToRadians(lat))
                    * qSin(dLon / 2) * qSin(dLon / 2);
            m_distanceKm += 6371.0 * 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
        }
        previousLat = lat;
        previousLon = lon;
        havePrevious = true;
    }

    const int maxPoints = 1500;
    const int step = all.size() > maxPoints ? (all.size() / maxPoints) + 1 : 1;
    for (int i = 0; i < all.size(); i += step) {
        m_points.append(all.at(i));
    }
    if (!all.isEmpty() && step > 1) {
        const QVariantMap last = all.last();
        if (m_points.isEmpty() || m_points.last().toMap().value(QStringLiteral("ts")).toLongLong()
                != last.value(QStringLiteral("ts")).toLongLong()) {
            m_points.append(last);
        }
    }

    QSqlQuery daysQuery(m_db);
    daysQuery.prepare(QStringLiteral(
        "SELECT date(ts, 'unixepoch', 'localtime') AS day, COUNT(*), MIN(ts), MAX(ts)"
        " FROM points WHERE ts >= ? AND ts <= ?"
        " GROUP BY day ORDER BY day DESC"));
    daysQuery.addBindValue(from);
    daysQuery.addBindValue(to);
    if (daysQuery.exec()) {
        while (daysQuery.next()) {
            QVariantMap day;
            day.insert(QStringLiteral("date"), daysQuery.value(0).toString());
            day.insert(QStringLiteral("count"), daysQuery.value(1).toInt());
            day.insert(QStringLiteral("firstTs"), daysQuery.value(2).toLongLong());
            day.insert(QStringLiteral("lastTs"), daysQuery.value(3).toLongLong());
            m_days.append(day);
        }
    }

    emit changed();
}

QString HistoryModel::exportGpx()
{
    if (!ensureDatabase()) {
        return QString();
    }

    const qint64 from = m_from.toMSecsSinceEpoch() / 1000;
    const qint64 to = m_to.toMSecsSinceEpoch() / 1000;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT ts, lat, lon, alt FROM points"
        " WHERE ts >= ? AND ts <= ? ORDER BY ts ASC"));
    query.addBindValue(from);
    query.addBindValue(to);
    if (!query.exec()) {
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
            + m_from.toString(QStringLiteral("yyyyMMdd-HHmm"))
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

    xml.writeStartElement(QStringLiteral("metadata"));
    xml.writeTextElement(QStringLiteral("name"),
                         QStringLiteral("TrackLog %1").arg(m_rangeText));
    xml.writeTextElement(QStringLiteral("time"),
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("trk"));
    xml.writeTextElement(QStringLiteral("name"), QStringLiteral("TrackLog"));
    xml.writeStartElement(QStringLiteral("trkseg"));

    while (query.next()) {
        const qint64 ts = query.value(0).toLongLong();
        const double lat = query.value(1).toDouble();
        const double lon = query.value(2).toDouble();
        const double alt = query.value(3).toDouble();

        xml.writeStartElement(QStringLiteral("trkpt"));
        xml.writeAttribute(QStringLiteral("lat"), QString::number(lat, 'f', 7));
        xml.writeAttribute(QStringLiteral("lon"), QString::number(lon, 'f', 7));
        xml.writeTextElement(QStringLiteral("ele"), QString::number(alt, 'f', 1));
        xml.writeTextElement(QStringLiteral("time"),
                             QDateTime::fromMSecsSinceEpoch(ts * 1000, Qt::UTC)
                             .toString(Qt::ISODate));
        xml.writeEndElement();
    }

    xml.writeEndElement(); // trkseg
    xml.writeEndElement(); // trk
    xml.writeEndElement(); // gpx
    xml.writeEndDocument();
    file.close();

    return path;
}
