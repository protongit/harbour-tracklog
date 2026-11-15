#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QTimer>
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
    qInfo() << "check: creating client";
    TrackClient client;
    qInfo() << "check: creating history model";
    HistoryModel history;
    qInfo() << "check: creating tile store";
    TileStore tileStore;

    qInfo() << "check: creating engine";
    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("tiles"), new TileProvider(&tileStore));
    engine.rootContext()->setContextProperty(QStringLiteral("trackclient"), &client);
    engine.rootContext()->setContextProperty(QStringLiteral("history"), &history);
    engine.rootContext()->setContextProperty(QStringLiteral("tilestore"), &tileStore);

    int errors = 0;
    const QStringList files = qmlFiles();
    qInfo() << "checking" << files.size() << "QML files";
    for (const QString &file : files) {
        qInfo() << "check:" << file;
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
    bool checkMode = false;
    bool openHistory = false;
    bool exportGpx = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--check") == 0) {
            checkMode = true;
        } else if (qstrcmp(argv[i], "--open-history") == 0) {
            openHistory = true;
        } else if (qstrcmp(argv[i], "--export-gpx") == 0) {
            exportGpx = true;
        }
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

    if (exportGpx) {
        HistoryModel history;
        history.setPreset(-1);
        const QString path = history.exportGpx();
        qInfo().noquote() << "gpx export:" << (path.isEmpty() ? QStringLiteral("FAILED") : path);
        return path.isEmpty() ? 1 : 0;
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

    if (openHistory) {
        QTimer::singleShot(1500, [&view]() {
            if (view->rootObject()) {
                QMetaObject::invokeMethod(view->rootObject(), "openHistory");
            }
        });
        QTimer::singleShot(8000, qApp, SLOT(quit()));
    }

    return app->exec();
}
