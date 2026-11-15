#include <QtQuick>

#include <sailfishapp.h>

#include "client.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());

    TrackClient client;
    view->rootContext()->setContextProperty(QStringLiteral("trackclient"), &client);

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/tracklog.qml")));
    view->show();

    return app->exec();
}
