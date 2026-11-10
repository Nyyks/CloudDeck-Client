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
    $$PWD/systemkeyforwarder.cpp \
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
    $$PWD/systemkeyforwarder.h \
    $$PWD/usbforwarder.h \
    $$PWD/usbtunnel.h

linux {
    # Secret Service (keychain) over D-Bus; no libsecret dependency.
    QT += dbus
    HEADERS += $$PWD/secretstore_prompt.h

    # In-stream menu on native Wayland (wlr-layer-shell overlay surfaces), where layer-shell-qt
    # is installed (Debian/Raspberry Pi OS: liblayershellqtinterface-dev). The AppImage is built
    # without Wayland and runs through XWayland, where the X11 path is used.
    !disable-wayland:exists(/usr/include/LayerShellQt/window.h) {
        DEFINES += HAVE_LAYERSHELLQT
        LIBS += -lLayerShellQtInterface
        CONFIG += link_pkgconfig
        PKGCONFIG += wayland-client
        message(CloudDeck: in-stream menu with wlr-layer-shell on Wayland)
    }
}

win32 {
    # Credential Manager (CredWriteW/CredReadW)
    LIBS += advapi32.lib
    # System shortcuts in "Always" mode (SetWindowsHookEx, SendInput)
    LIBS += user32.lib
}
