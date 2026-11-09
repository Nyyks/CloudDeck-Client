# Unit tests for CloudDeck-Client additions (network-free parts).
# Build & run:  qmake6 clouddeck-tests.pro && make && ./tst_clouddeckapi
QT += testlib core
QT -= gui
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TARGET = tst_clouddeckapi

INCLUDEPATH += $$PWD/../../app
SOURCES += \
    tst_clouddeckapi.cpp \
    $$PWD/../../app/clouddeck/clouddeckapi.cpp \
    $$PWD/../../app/clouddeck/spanlayout.cpp \
    $$PWD/../../app/clouddeck/netrecommend.cpp \
    $$PWD/../../app/clouddeck/controllerprofile.cpp
HEADERS += \
    $$PWD/../../app/clouddeck/clouddeckapi.h \
    $$PWD/../../app/clouddeck/spanlayout.h \
    $$PWD/../../app/clouddeck/netrecommend.h \
    $$PWD/../../app/clouddeck/controllerprofile.h

DEFINES += FIXTURE_DIR=\\\"$$clean_path($$PWD/../../../shared/fixtures/clouddeck)\\\"

# Linux: the secret store's private-file fallback (Secret Service needs D-Bus)
linux {
    QT += dbus
    SOURCES += $$PWD/../../app/clouddeck/secretstore.cpp
    HEADERS += $$PWD/../../app/clouddeck/secretstore.h $$PWD/../../app/clouddeck/secretstore_prompt.h
}
