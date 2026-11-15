#ifndef TRACKLOGPATHS_H
#define TRACKLOGPATHS_H

#include <QDir>
#include <QString>

namespace TrackLog {

inline QString dataDir()
{
    return QDir::homePath() + QStringLiteral("/.local/share/org.hensip/tracklog");
}

inline QString dbPath()
{
    return dataDir() + QStringLiteral("/tracklog.db");
}

inline QString settingsPath()
{
    return dataDir() + QStringLiteral("/settings.ini");
}

inline QString logPath()
{
    return dataDir() + QStringLiteral("/tracklogd.log");
}

inline QString tilesDir()
{
    return dataDir() + QStringLiteral("/tiles");
}

inline bool ensureDataDir()
{
    return QDir().mkpath(dataDir());
}

inline const char *dbusService()
{
    return "org.hensip.tracklog";
}

inline const char *dbusPath()
{
    return "/org/hensip/tracklog";
}

inline const char *dbusInterface()
{
    return "org.hensip.tracklog.Control";
}

inline const char *userAgent()
{
    return "harbour-tracklog/0.1";
}

} // namespace TrackLog

#endif // TRACKLOGPATHS_H
