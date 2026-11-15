#include "config.h"

#include <QDir>

#include "tracklogpaths.h"

Config::Config()
    : m_settings(TrackLog::settingsPath(), QSettings::IniFormat)
{
    TrackLog::ensureDataDir();
    load();
}

void Config::load()
{
    if (!m_settings.contains(QStringLiteral("capture/intervalSec"))) {
        m_settings.setValue(QStringLiteral("capture/intervalSec"), 60);
        m_settings.setValue(QStringLiteral("capture/minAccuracyM"), 120);
        m_settings.setValue(QStringLiteral("capture/minDistanceM"), 0);
        m_settings.setValue(QStringLiteral("capture/autoStart"), true);
        m_settings.setValue(QStringLiteral("capture/tracking"), true);
        m_settings.setValue(QStringLiteral("upload/enabled"), false);
        m_settings.setValue(QStringLiteral("upload/url"), QString());
        m_settings.setValue(QStringLiteral("upload/id"), QStringLiteral("tracklog"));
        m_settings.setValue(QStringLiteral("upload/traccar"), false);
        m_settings.sync();
    }
}

QVariant Config::value(const QString &key, const QVariant &fallback) const
{
    return m_settings.value(key, fallback);
}

bool Config::uploadEnabled() const
{
    return value(QStringLiteral("upload/enabled"), false).toBool();
}

QString Config::url() const
{
    return value(QStringLiteral("upload/url"), QString()).toString();
}

QString Config::deviceId() const
{
    return value(QStringLiteral("upload/id"), QStringLiteral("tracklog")).toString();
}

bool Config::traccar() const
{
    return value(QStringLiteral("upload/traccar"), false).toBool();
}

int Config::intervalSec() const
{
    const int v = value(QStringLiteral("capture/intervalSec"), 60).toInt();
    return v < 5 ? 5 : v;
}

int Config::minAccuracyM() const
{
    const int v = value(QStringLiteral("capture/minAccuracyM"), 120).toInt();
    return v < 1 ? 1 : v;
}

int Config::minDistanceM() const
{
    const int v = value(QStringLiteral("capture/minDistanceM"), 0).toInt();
    return v < 0 ? 0 : v;
}

bool Config::autoStart() const
{
    return value(QStringLiteral("capture/autoStart"), true).toBool();
}

int Config::pauseBelowBattery() const
{
    const int v = value(QStringLiteral("capture/pauseBelowBattery"), 0).toInt();
    return (v < 0 || v > 100) ? 0 : v;
}

int Config::retentionDays() const
{
    const int v = value(QStringLiteral("capture/retentionDays"), 0).toInt();
    return v < 0 ? 0 : v;
}

bool Config::geofenceEnabled() const
{
    return value(QStringLiteral("geofence/enabled"), false).toBool();
}

double Config::geofenceLat() const
{
    return value(QStringLiteral("geofence/lat"), 0.0).toDouble();
}

double Config::geofenceLon() const
{
    return value(QStringLiteral("geofence/lon"), 0.0).toDouble();
}

int Config::geofenceRadius() const
{
    const int v = value(QStringLiteral("geofence/radius"), 200).toInt();
    return v < 10 ? 10 : v;
}

QString Config::geofenceName() const
{
    return value(QStringLiteral("geofence/name"), QStringLiteral("Casa")).toString();
}

bool Config::tracking() const
{
    return value(QStringLiteral("capture/tracking"), true).toBool();
}

void Config::setTracking(bool tracking)
{
    m_settings.setValue(QStringLiteral("capture/tracking"), tracking);
    m_settings.sync();
}

void Config::sync()
{
    m_settings.sync();
}

QVariantMap Config::toMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("uploadEnabled"), uploadEnabled());
    map.insert(QStringLiteral("url"), url());
    map.insert(QStringLiteral("id"), deviceId());
    map.insert(QStringLiteral("traccar"), traccar());
    map.insert(QStringLiteral("intervalSec"), intervalSec());
    map.insert(QStringLiteral("minAccuracyM"), minAccuracyM());
    map.insert(QStringLiteral("minDistanceM"), minDistanceM());
    map.insert(QStringLiteral("pauseBelowBattery"), pauseBelowBattery());
    map.insert(QStringLiteral("retentionDays"), retentionDays());
    map.insert(QStringLiteral("autoStart"), autoStart());
    map.insert(QStringLiteral("tracking"), tracking());
    map.insert(QStringLiteral("geofenceEnabled"), geofenceEnabled());
    map.insert(QStringLiteral("geofenceLat"), geofenceLat());
    map.insert(QStringLiteral("geofenceLon"), geofenceLon());
    map.insert(QStringLiteral("geofenceRadius"), geofenceRadius());
    map.insert(QStringLiteral("geofenceName"), geofenceName());
    return map;
}

bool Config::updateFromMap(const QVariantMap &map)
{
    bool changed = false;

    const struct { const char *key; const char *setting; } stringKeys[] = {
        { "url", "upload/url" },
        { "id", "upload/id" },
        { "geofenceName", "geofence/name" }
    };
    for (const auto &entry : stringKeys) {
        const QString key = QString::fromLatin1(entry.key);
        if (map.contains(key)) {
            const QString value = map.value(key).toString();
            if (m_settings.value(QString::fromLatin1(entry.setting)).toString() != value) {
                m_settings.setValue(QString::fromLatin1(entry.setting), value);
                changed = true;
            }
        }
    }

    const struct { const char *key; const char *setting; } boolKeys[] = {
        { "uploadEnabled", "upload/enabled" },
        { "traccar", "upload/traccar" },
        { "autoStart", "capture/autoStart" },
        { "geofenceEnabled", "geofence/enabled" }
    };
    for (const auto &entry : boolKeys) {
        const QString key = QString::fromLatin1(entry.key);
        if (map.contains(key)) {
            const bool value = map.value(key).toBool();
            if (m_settings.value(QString::fromLatin1(entry.setting)).toBool() != value) {
                m_settings.setValue(QString::fromLatin1(entry.setting), value);
                changed = true;
            }
        }
    }

    const struct { const char *key; const char *setting; } intKeys[] = {
        { "intervalSec", "capture/intervalSec" },
        { "minAccuracyM", "capture/minAccuracyM" },
        { "minDistanceM", "capture/minDistanceM" },
        { "pauseBelowBattery", "capture/pauseBelowBattery" },
        { "retentionDays", "capture/retentionDays" },
        { "geofenceRadius", "geofence/radius" }
    };
    for (const auto &entry : intKeys) {
        const QString key = QString::fromLatin1(entry.key);
        if (map.contains(key)) {
            const int value = map.value(key).toInt();
            if (m_settings.value(QString::fromLatin1(entry.setting)).toInt() != value) {
                m_settings.setValue(QString::fromLatin1(entry.setting), value);
                changed = true;
            }
        }
    }

    const struct { const char *key; const char *setting; } doubleKeys[] = {
        { "geofenceLat", "geofence/lat" },
        { "geofenceLon", "geofence/lon" }
    };
    for (const auto &entry : doubleKeys) {
        const QString key = QString::fromLatin1(entry.key);
        if (map.contains(key)) {
            const double value = map.value(key).toDouble();
            if (!qFuzzyCompare(m_settings.value(QString::fromLatin1(entry.setting)).toDouble() + 1.0,
                               value + 1.0)) {
                m_settings.setValue(QString::fromLatin1(entry.setting), value);
                changed = true;
            }
        }
    }

    if (changed) {
        m_settings.sync();
    }
    return changed;
}
