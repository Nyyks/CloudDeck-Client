# CloudDeck-Client additions to the desktop app. Included once from app.pro, so
# upstream's file lists stay untouched (docs/adr/0005).

SOURCES += \
    $$PWD/clouddeckapi.cpp \
    $$PWD/clouddeckprovider.cpp \
    $$PWD/cloudsession.cpp \
    $$PWD/secretstore.cpp

HEADERS += \
    $$PWD/branding.h \
    $$PWD/clouddeckapi.h \
    $$PWD/cloudprovider.h \
    $$PWD/clouddeckprovider.h \
    $$PWD/cloudsession.h \
    $$PWD/secretstore.h

linux {
    # Secret Service (keychain) over D-Bus; no libsecret dependency.
    QT += dbus
    HEADERS += $$PWD/secretstore_prompt.h
}

win32 {
    # Credential Manager (CredWriteW/CredReadW)
    LIBS += advapi32.lib
}
