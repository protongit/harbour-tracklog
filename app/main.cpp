#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtQuick>

#include <cstdio>

#include <sailfishapp.h>

#include "client.h"
#include "historymodel.h"
#include "tilestore.h"
#include "tracklogpaths.h"

static QFile *appLog = nullptr;

static void rotateLog()
{
    const QString path = TrackLog::dataDir() + QStringLiteral("/app.log");
    QFile current(path);
    if (current.exists() && current.size() > 256 * 1024) {
        QFile::remove(path + QStringLiteral(".1"));
        current.rename(path + QStringLiteral(".1"));
    }
}

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "I";
    switch (type) {
    case QtDebugMsg: level = "D"; break;
    case QtInfoMsg: level = "I"; break;
    case QtWarningMsg: level = "W"; break;
    case QtCriticalMsg: level = "C"; break;
    case QtFatalMsg: level = "F"; break;
    }

    QString line = QDateTime::currentDateTime().toString(Qt::ISODate)
            + QStringLiteral(" [") + QLatin1String(level) + QStringLiteral("] ")
            + message;
    if (context.file) {
        line += QStringLiteral(" (") + QLatin1String(context.file)
                + QLatin1Char(':') + QString::number(context.line) + QLatin1Char(')');
    }
    line += QLatin1Char('\n');

    if (appLog) {
        appLog->write(line.toUtf8());
        appLog->flush();
    }
    const QByteArray utf8 = line.toUtf8();
    fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
    fflush(stderr);
}

static QStringList qmlFiles()
{
    QStringList files;
    const QString dir = SailfishApp::pathTo(QStringLiteral("qml")).toLocalFile();
    QDirIterator it(dir, QStringList() << QStringLiteral("*.qml"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files << it.next();
    }
    files.sort();
    return files;
}

static int runQmlCheck()
{
    TrackClient client;
    HistoryModel history;
    TileStore tileStore;

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("tiles"), new TileProvider(&tileStore));
    engine.rootContext()->setContextProperty(QStringLiteral("trackclient"), &client);
    engine.rootContext()->setContextProperty(QStringLiteral("history"), &history);
    engine.rootContext()->setContextProperty(QStringLiteral("tilestore"), &tileStore);

    int errors = 0;
    const QStringList files = qmlFiles();
    qInfo() << "checking" << files.size() << "QML files";
    for (const QString &file : files) {
        QQmlComponent component(&engine, QUrl::fromLocalFile(file));
        if (component.isError()) {
            ++errors;
            qWarning().noquote() << "QML ERROR" << file << component.errorString();
            continue;
        }
        QObject *object = component.create();
        if (!object || component.isError()) {
            ++errors;
            qWarning().noquote() << "QML CREATE ERROR" << file << component.errorString();
        }
        delete object;
    }
    qInfo() << "qml check finished, errors:" << errors;
    return errors == 0 ? 0 : 1;
}

int main(int argc, char *argv[])
{
    const bool checkMode = QCoreApplication::arguments().contains(QStringLiteral("--check"));
    if (checkMode) {
        qputenv("QT_QPA_PLATFORM", "minimal");
    }

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));

    TrackLog::ensureDataDir();
    rotateLog();
    appLog = new QFile(TrackLog::dataDir() + QStringLiteral("/app.log"));
    appLog->open(QIODevice::WriteOnly | QIODevice::Append);
    qInstallMessageHandler(messageHandler);
    qInfo() << "harbour-tracklog starting";

    if (checkMode) {
        return runQmlCheck();
    }

    QScopedPointer<QQuickView> view(SailfishApp::createView());

    TrackClient client;
    HistoryModel history;
    TileStore tileStore;

    view->engine()->addImageProvider(QStringLiteral("tiles"), new TileProvider(&tileStore));

    view->rootContext()->setContextProperty(QStringLiteral("trackclient"), &client);
    view->rootContext()->setContextProperty(QStringLiteral("history"), &history);
    view->rootContext()->setContextProperty(QStringLiteral("tilestore"), &tileStore);

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/tracklog.qml")));
    view->show();

    return app->exec();
}
