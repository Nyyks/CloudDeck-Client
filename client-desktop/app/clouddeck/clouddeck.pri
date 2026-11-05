# CloudDeck-Client additions to the desktop app. Included once from app.pro, so
# upstream's file lists stay untouched (docs/adr/0005).

SOURCES += \
    $$PWD/agenttunnel.cpp \
    $$PWD/clipboardsync.cpp \
    $$PWD/clouddeckapi.cpp \
    $$PWD/clouddeckprovider.cpp \
    $$PWD/cloudsession.cpp \
    $$PWD/controllerprofile.cpp \
    $$PWD/controllersettings.cpp \
    $$PWD/displaylease.cpp \
    $$PWD/displaysettings.cpp \
    $$PWD/micstreamer.cpp \
    $$PWD/netrecommend.cpp \
    $$PWD/nettest.cpp \
    $$PWD/onscreencontroller.cpp \
    $$PWD/streamoverlay.cpp \
    $$PWD/secretstore.cpp \
    $$PWD/shortcuts.cpp \
    $$PWD/spanlayout.cpp \
    $$PWD/usbforwarder.cpp \
    $$PWD/usbtunnel.cpp

HEADERS += \
    $$PWD/agenttunnel.h \
    $$PWD/branding.h \
    $$PWD/clipboardsync.h \
    $$PWD/clouddeckapi.h \
    $$PWD/cloudprovider.h \
    $$PWD/clouddeckprovider.h \
    $$PWD/cloudsession.h \
    $$PWD/controllerprofile.h \
    $$PWD/controllersettings.h \
    $$PWD/displaylease.h \
    $$PWD/displaysettings.h \
    $$PWD/micstreamer.h \
    $$PWD/netrecommend.h \
    $$PWD/nettest.h \
    $$PWD/onscreencontroller.h \
    $$PWD/streamoverlay.h \
    $$PWD/secretstore.h \
    $$PWD/shortcuts.h \
    $$PWD/spanlayout.h \
    $$PWD/usbforwarder.h \
    $$PWD/usbtunnel.h

linux {
    # Secret Service (keychain) over D-Bus; no libsecret dependency.
    QT += dbus
    HEADERS += $$PWD/secretstore_prompt.h
}

win32 {
    # Credential Manager (CredWriteW/CredReadW)
    LIBS += advapi32.lib
}
