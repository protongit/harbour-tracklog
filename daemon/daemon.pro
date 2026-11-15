TARGET = tracklogd
TEMPLATE = app

CONFIG += c++11 console
CONFIG -= app_bundle

QT += core dbus network positioning sql

INCLUDEPATH += ../common

SOURCES += \
    main.cpp \
    daemon.cpp \
    config.cpp \
    database.cpp \
    tracker.cpp \
    uploader.cpp

HEADERS += \
    daemon.h \
    config.h \
    database.h \
    tracker.h \
    uploader.h

target.path = /usr/bin
INSTALLS += target
