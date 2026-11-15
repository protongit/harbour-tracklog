#include "historymodel.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>
#include <QVector>
#include <QXmlStreamWriter>

#include <cmath>
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
    m_trips.clear();
    m_pointCount = 0;
    m_distanceKm = 0.0;
    m_maxSpeedKmh = 0.0;
    m_avgSpeedKmh = 0.0;
    m_ascentM = 0.0;

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
    double speedSum = 0.0;
    int speedCount = 0;
    double previousAltitude = -1000.0;

    while (query.next()) {
        const qint64 ts = query.value(0).toLongLong();
        const double lat = query.value(1).toDouble();
        const double lon = query.value(2).toDouble();
        const double speed = query.value(3).toDouble();
        const QVariant accValue = query.value(4);
        const double acc = accValue.isNull() ? -1.0 : accValue.toDouble();
        const bool uploaded = query.value(5).toInt() == 1;

        const double speedKmh = speed > 0.0 ? speed * 3.6 : 0.0;
        if (speedKmh > m_maxSpeedKmh) {
            m_maxSpeedKmh = speedKmh;
        }
        if (speedKmh > 0.5) {
            speedSum += speedKmh;
            ++speedCount;
        }

        QVariantMap point;
        point.insert(QStringLiteral("ts"), ts);
        point.insert(QStringLiteral("lat"), lat);
        point.insert(QStringLiteral("lon"), lon);
        point.insert(QStringLiteral("speed"), speedKmh);
        point.insert(QStringLiteral("acc"), acc);
        point.insert(QStringLiteral("uploaded"), uploaded);
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

    if (speedCount > 0) {
        m_avgSpeedKmh = speedSum / speedCount;
    }

    // Ascent only uses valid altitudes.
    QSqlQuery altitudeQuery(m_db);
    altitudeQuery.prepare(QStringLiteral(
        "SELECT alt FROM points WHERE ts >= ? AND ts <= ? ORDER BY ts ASC"));
    altitudeQuery.addBindValue(from);
    altitudeQuery.addBindValue(to);
    if (altitudeQuery.exec()) {
        while (altitudeQuery.next()) {
            if (altitudeQuery.value(0).isNull()) {
                previousAltitude = -1000.0;
                continue;
            }
            const double altitude = altitudeQuery.value(0).toDouble();
            if (!std::isfinite(altitude) || altitude < -100.0 || altitude > 9000.0) {
                previousAltitude = -1000.0;
                continue;
            }
            if (previousAltitude > -999.0 && altitude > previousAltitude) {
                m_ascentM += altitude - previousAltitude;
            }
            previousAltitude = altitude;
        }
    }

    // Split into trips on gaps longer than 30 minutes.
    QSettings tripNames(TrackLog::dataDir() + QStringLiteral("/trips.ini"),
                        QSettings::IniFormat);

    auto appendTrip = [this, &tripNames](qint64 start, qint64 end, int count, double km) {
        QVariantMap trip;
        trip.insert(QStringLiteral("startTs"), start);
        trip.insert(QStringLiteral("endTs"), end);
        trip.insert(QStringLiteral("count"), count);
        trip.insert(QStringLiteral("km"), km);
        QString label = QDateTime::fromMSecsSinceEpoch(start * 1000)
                .toString(QStringLiteral("dd/MM HH:mm"))
                + QStringLiteral(" – ")
                + QDateTime::fromMSecsSinceEpoch(end * 1000)
                  .toString(QStringLiteral("HH:mm"));
        const QString custom = tripNames.value(QString::number(start)).toString();
        if (!custom.isEmpty()) {
            label = custom;
        }
        trip.insert(QStringLiteral("label"), label);
        m_trips.append(trip);
    };

    qint64 tripStart = 0;
    qint64 tripLast = 0;
    int tripCount = 0;
    double tripKm = 0.0;
    double tripPreviousLat = 0.0;
    double tripPreviousLon = 0.0;
    bool tripHasPrevious = false;

    for (int i = 0; i < all.size(); ++i) {
        const QVariantMap &point = all.at(i);
        const qint64 ts = point.value(QStringLiteral("ts")).toLongLong();
        const double lat = point.value(QStringLiteral("lat")).toDouble();
        const double lon = point.value(QStringLiteral("lon")).toDouble();

        if (tripStart == 0) {
            tripStart = ts;
        } else if (ts - tripLast > 1800) {
            appendTrip(tripStart, tripLast, tripCount, tripKm);
            tripStart = ts;
            tripCount = 0;
            tripKm = 0.0;
            tripHasPrevious = false;
        }

        if (tripHasPrevious) {
            const double dLat = qDegreesToRadians(lat - tripPreviousLat);
            const double dLon = qDegreesToRadians(lon - tripPreviousLon);
            const double a = qSin(dLat / 2) * qSin(dLat / 2)
                    + qCos(qDegreesToRadians(tripPreviousLat)) * qCos(qDegreesToRadians(lat))
                    * qSin(dLon / 2) * qSin(dLon / 2);
            tripKm += 6371.0 * 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
        }
        tripPreviousLat = lat;
        tripPreviousLon = lon;
        tripHasPrevious = true;
        tripLast = ts;
        ++tripCount;
    }

    if (tripStart != 0) {
        appendTrip(tripStart, tripLast, tripCount, tripKm);
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

void HistoryModel::setTripName(qint64 startTs, const QString &name)
{
    QSettings names(TrackLog::dataDir() + QStringLiteral("/trips.ini"), QSettings::IniFormat);
    if (name.trimmed().isEmpty()) {
        names.remove(QString::number(startTs));
    } else {
        names.setValue(QString::number(startTs), name.trimmed());
    }
    names.sync();
    reload();
}

QVariantList HistoryModel::gpxFiles() const
{
    QVariantList list;
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
        return list;
    }
    QDir dir(base + QStringLiteral("/TrackLog"));
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.gpx"),
                                                  QDir::Files, QDir::Time);
    for (const QFileInfo &info : files) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), info.fileName());
        entry.insert(QStringLiteral("path"), info.absoluteFilePath());
        list.append(entry);
    }
    return list;
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

    QVector<QVariantList> rows;
    while (query.next()) {
        QVariantList row;
        row << query.value(0) << query.value(1) << query.value(2) << query.value(3);
        rows.append(row);
    }
    if (rows.isEmpty()) {
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

    const qint64 firstTs = rows.first().at(0).toLongLong();
    const QString path = dir + QStringLiteral("/tracklog-")
            + QDateTime::fromMSecsSinceEpoch(firstTs * 1000)
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

    xml.writeStartElement(QStringLiteral("metadata"));
    xml.writeTextElement(QStringLiteral("name"),
                         QStringLiteral("TrackLog %1").arg(m_rangeText));
    xml.writeTextElement(QStringLiteral("time"),
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("trk"));
    xml.writeTextElement(QStringLiteral("name"), QStringLiteral("TrackLog"));
    xml.writeStartElement(QStringLiteral("trkseg"));

    qint64 previousTs = 0;
    for (const QVariantList &row : rows) {
        const qint64 ts = row.at(0).toLongLong();
        const double lat = row.at(1).toDouble();
        const double lon = row.at(2).toDouble();
        const double alt = row.at(3).toDouble();

        // A long gap starts a new segment instead of a straight line.
        if (previousTs > 0 && ts - previousTs > 600) {
            xml.writeEndElement(); // trkseg
            xml.writeStartElement(QStringLiteral("trkseg"));
        }
        previousTs = ts;

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
