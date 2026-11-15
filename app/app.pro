TARGET = harbour-tracklog
TEMPLATE = app

CONFIG += sailfishapp c++11
QT += quick dbus

INCLUDEPATH += ../common

SOURCES += \
    main.cpp \
    client.cpp

HEADERS += client.h

DISTFILES += \
    qml/tracklog.qml \
    qml/pages/MainPage.qml \
    qml/pages/SettingsPage.qml \
    qml/cover/CoverPage.qml \
    qml/components/DetailRow.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256
