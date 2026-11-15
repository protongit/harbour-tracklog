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
    map.insert(QStringLiteral("autoStart"), autoStart());
    map.insert(QStringLiteral("tracking"), tracking());
    return map;
}

bool Config::updateFromMap(const QVariantMap &map)
{
    bool changed = false;

    const struct { const char *key; const char *setting; } stringKeys[] = {
        { "url", "upload/url" },
        { "id", "upload/id" }
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
        { "autoStart", "capture/autoStart" }
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
        { "minDistanceM", "capture/minDistanceM" }
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

    if (changed) {
        m_settings.sync();
    }
    return changed;
}
