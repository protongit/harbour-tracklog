#include <QtQuick>

#include <sailfishapp.h>

#include "client.h"
#include "historymodel.h"
#include "tilestore.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
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
