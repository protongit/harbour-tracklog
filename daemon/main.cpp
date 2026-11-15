#include <QCoreApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDebug>
#include <QFile>

#include <cstdio>

#include "daemon.h"
#include "tracklogpaths.h"

static QFile *logFile = nullptr;

static void rotateLog()
{
    QFile current(TrackLog::logPath());
    if (current.exists() && current.size() > 512 * 1024) {
        QFile::remove(TrackLog::logPath() + QStringLiteral(".1"));
        current.rename(TrackLog::logPath() + QStringLiteral(".1"));
    }
}

static void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    const char *level = "I";
    switch (type) {
    case QtDebugMsg: level = "D"; break;
    case QtInfoMsg: level = "I"; break;
    case QtWarningMsg: level = "W"; break;
    case QtCriticalMsg: level = "C"; break;
    case QtFatalMsg: level = "F"; break;
    }

    const QString line = QDateTime::currentDateTime().toString(Qt::ISODate)
            + QStringLiteral(" [") + QLatin1String(level) + QStringLiteral("] ")
            + message + QLatin1Char('\n');

    if (logFile) {
        logFile->write(line.toUtf8());
        logFile->flush();
    }
    const QByteArray utf8 = line.toUtf8();
    fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
    fflush(stderr);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("org.hensip"));
    QCoreApplication::setApplicationName(QStringLiteral("tracklog"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    TrackLog::ensureDataDir();
    rotateLog();

    logFile = new QFile(TrackLog::logPath());
    logFile->open(QIODevice::WriteOnly | QIODevice::Append);
    qInstallMessageHandler(messageHandler);

    qInfo() << "tracklogd starting, pid" << QCoreApplication::applicationPid();

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "no session bus available";
        return 2;
    }

    Daemon daemon;
    if (!daemon.init()) {
        qCritical() << "daemon initialisation failed";
        return 3;
    }

    if (!bus.registerObject(QString::fromLatin1(TrackLog::dbusPath()), &daemon,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        qCritical() << "cannot register D-Bus object";
        return 4;
    }

    if (!bus.registerService(QString::fromLatin1(TrackLog::dbusService()))) {
        qWarning() << "D-Bus name already taken, another instance is running";
        return 0;
    }

    daemon.startTrackingIfConfigured();

    qInfo() << "tracklogd ready";
    return app.exec();
}
