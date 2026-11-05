// CloudDeck-Client: in-stream menu (Phase 5, docs/adr/0009-quick-menu.md).
//
// Two small Qt Quick windows owned by the stream's SDL window, so they stay above
// it but not above other apps:
//   - a round button at the top centre (CloudDeck logo, translucent), shown while
//     the cursor is free (desktop mouse mode or capture released);
//   - the menu panel that drops down from it: display, mouse, audio, microphone,
//     USB devices and session controls.
// Ctrl+Alt+Shift+O opens or closes the menu from the keyboard.
//
// Qt is normally paused while Moonlight streams. While this overlay exists, the SDL
// loop calls pump(), which processes Qt events briefly (about 30 times a second,
// more while the menu is open).
#pragma once

#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <SDL.h>

class FileUploader;
class QQuickView;
class QWindow;
class Session;

class StreamOverlay : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool menuOpen READ menuOpen NOTIFY changed)
    Q_PROPERTY(QString streamInfo READ streamInfo NOTIFY changed)
    Q_PROPERTY(bool canToggleFullscreen READ canToggleFullscreen NOTIFY changed)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY changed)
    Q_PROPERTY(bool canFitToWindow READ canFitToWindow NOTIFY changed)
    Q_PROPERTY(bool fitToWindow READ fitToWindow WRITE setFitToWindow NOTIFY changed)
    Q_PROPERTY(bool statsVisible READ statsVisible WRITE setStatsVisible NOTIFY changed)
    Q_PROPERTY(bool desktopMouse READ desktopMouse WRITE setDesktopMouse NOTIFY changed)
    Q_PROPERTY(bool audioMuted READ audioMuted WRITE setAudioMuted NOTIFY changed)
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled NOTIFY changed)
    Q_PROPERTY(bool showButton READ showButton WRITE setShowButton NOTIFY changed)
    Q_PROPERTY(bool canShutDown READ canShutDown NOTIFY changed)

public:
    explicit StreamOverlay(Session* session);
    ~StreamOverlay() override;

    // SDL loop: call every iteration; processes Qt events when due.
    void pump();
    // SDL loop: every event. Returns true if it was consumed (hotkey, input while the
    // menu is open). Window events are only observed.
    bool handleEvent(const SDL_Event& event);
    // How long the SDL loop may block waiting for events.
    int maxWaitMs() const { return m_MenuOpen ? 8 : 33; }

    bool menuOpen() const { return m_MenuOpen; }
    QString streamInfo() const;
    bool canToggleFullscreen() const;
    bool fullscreen() const;
    void setFullscreen(bool on);
    bool canFitToWindow() const;
    bool fitToWindow() const;
    void setFitToWindow(bool on);
    bool statsVisible() const;
    void setStatsVisible(bool on);
    bool desktopMouse() const;
    void setDesktopMouse(bool on);
    bool audioMuted() const;
    void setAudioMuted(bool on);
    bool micEnabled() const;
    void setMicEnabled(bool on);
    bool showButton() const;
    void setShowButton(bool on);
    bool canShutDown() const;

    // A small status pill under the button, e.g. while reconnecting. It works even
    // when the stream has no decoder to draw Moonlight's own overlay.
    void showNotice(const QString& text, bool busy = true);
    void hideNotice();
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY changed)
    QString noticeText() const { return m_NoticeText; }
    Q_PROPERTY(bool noticeBusy READ noticeBusy NOTIFY changed)
    bool noticeBusy() const { return m_NoticeBusy; }

    // File upload (docs/adr/0011-file-upload.md): dropped files and the menu's picker.
    void uploadPaths(const QStringList& localPaths);
    Q_INVOKABLE void uploadFiles(const QVariantList& urls);

    Q_INVOKABLE void openMenu();
    Q_INVOKABLE void closeMenu();
    Q_INVOKABLE void toggleMenu() { m_MenuOpen ? closeMenu() : openMenu(); }
    Q_INVOKABLE void reconnect();
    Q_INVOKABLE void disconnectStream();
    Q_INVOKABLE void shutDown();

signals:
    void changed();

private:
    QRect sdlWindowRect() const; // native pixels
    void place(QWindow* w, const QRect& nativeRect);
    void updateGeometry();
    void updateButtonVisibility();
    QQuickView* createView(const char* qmlFile, bool focusable);

    Session* m_Session;
    QWindow* m_SdlParent = nullptr; // foreign wrapper of the SDL window (owner)
    QQuickView* m_Button = nullptr;
    QQuickView* m_Menu = nullptr;
    QQuickView* m_Notice = nullptr;
    QString m_NoticeText;
    bool m_NoticeBusy = true;
    int m_NoticeGeneration = 0;
    FileUploader* m_Uploader = nullptr;
    bool m_MenuOpen = false;
    bool m_RecaptureOnClose = false;
    Uint32 m_LastPump = 0;
    Uint32 m_OpenedAt = 0;
};
