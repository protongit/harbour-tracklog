TARGET = harbour-tracklog
TEMPLATE = app

CONFIG += sailfishapp c++11
QT += quick dbus network sql

INCLUDEPATH += ../common

SOURCES += \
    main.cpp \
    client.cpp \
    historymodel.cpp \
    tilestore.cpp

HEADERS += \
    client.h \
    historymodel.h \
    tilestore.h

DISTFILES += \
    qml/tracklog.qml \
    qml/pages/MainPage.qml \
    qml/pages/HistoryPage.qml \
    qml/pages/SettingsPage.qml \
    qml/cover/CoverPage.qml \
    qml/components/DetailRow.qml \
    qml/components/MapView.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256
