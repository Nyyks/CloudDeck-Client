#include "streamoverlay.h"

#include "cloudsession.h"
#include "displaysettings.h"
#include "onscreencontroller.h"
#include "micstreamer.h"
#include "shortcuts.h"
#include "streaming/session.h"
#include "utils.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>
#include <QRegion>
#include <QScreen>
#include <QSettings>
#include <QtDebug>

#include <SDL_syswm.h>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

static constexpr int kButtonSize = 40;   // logical px
static constexpr int kMargin = 10;       // logical px from the top edge
static constexpr int kMenuWidth = 400;   // logical px
static constexpr int kMenuMaxHeight = 660;

static bool isMenuHotkey(const SDL_KeyboardEvent& k)
{
    return (k.keysym.mod & KMOD_CTRL) && (k.keysym.mod & KMOD_ALT) && (k.keysym.mod & KMOD_SHIFT) &&
           (k.keysym.sym == Shortcuts::keycodeFor("menu") || k.keysym.scancode == Shortcuts::scancodeFor("menu"));
}

// Device pixel ratio of the screen containing a native-pixel point. Qt 6 keeps a
// screen's top-left in native coordinates and scales only its size.
static qreal dprAt(const QPoint& nativePoint)
{
    for (QScreen* s : QGuiApplication::screens()) {
        const QRect g = s->geometry();
        const QRect native(g.topLeft(), (QSizeF(g.size()) * s->devicePixelRatio()).toSize());
        if (native.contains(nativePoint)) return s->devicePixelRatio();
    }
    return QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
}

StreamOverlay::StreamOverlay(Session* session) : m_Session(session)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    WId parent = 0;
    if (SDL_GetWindowWMInfo(m_Session->m_Window, &info)) {
#ifdef Q_OS_WIN32
        if (info.subsystem == SDL_SYSWM_WINDOWS) parent = (WId)info.info.win.window;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
        if (info.subsystem == SDL_SYSWM_X11) parent = (WId)info.info.x11.window;
#endif
    }
    if (parent == 0) {
        qWarning() << "CloudDeck menu: no native stream window to attach to (e.g. native Wayland); the in-stream menu is unavailable";
        return;
    }
    m_SdlParent = QWindow::fromWinId(parent);

    m_Button = createView("StreamMenuButton.qml", false);
    m_Menu = createView("StreamMenu.qml", true);
    m_Notice = createView("StreamNotice.qml", false);
    m_Keyboard = createView("StreamKeyboard.qml", false); // doesn't take focus: keys go to the stream
    m_Osc = new OnScreenController(m_Session, m_SdlParent, m_Session->m_QtWindow ? qmlEngine(m_Session->m_QtWindow) : nullptr);
    if (m_Menu) m_Menu->setOpacity(0.97);
    if (m_Button) m_Button->setOpacity(0.85);
    updateGeometry();
    if (m_Osc && OnScreenController::enabledSetting()) { // left on last time
        m_OscWanted = true;
        m_Osc->setVisible(true);
    }
    updateButtonVisibility();
}

StreamOverlay::~StreamOverlay()
{
    delete m_Osc; // removes the virtual pad from the CloudDeck
    delete m_Keyboard;
    delete m_Notice;
    delete m_Menu;
    delete m_Button;
    delete m_SdlParent;
}

QQuickView* StreamOverlay::createView(const char* qmlFile, bool focusable)
{
    QQmlEngine* engine = m_Session->m_QtWindow ? qmlEngine(m_Session->m_QtWindow) : nullptr;
    auto* v = engine ? new QQuickView(engine, nullptr) : new QQuickView();
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
    if (!focusable) flags |= Qt::WindowDoesNotAcceptFocus;
    v->setFlags(flags);
    v->setTransientParent(m_SdlParent); // owned by the stream window: above it, not above other apps
    v->setResizeMode(QQuickView::SizeRootObjectToView);
    v->setColor(QColor("#1d222b"));
    v->setInitialProperties({{"overlay", QVariant::fromValue(static_cast<QObject*>(this))}});
    v->setSource(QUrl(QStringLiteral("qrc:/gui/") + QLatin1String(qmlFile)));
    if (v->status() != QQuickView::Ready) {
        for (const QQmlError& e : v->errors()) qWarning() << "CloudDeck menu:" << e.toString();
        delete v;
        return nullptr;
    }
    return v;
}

QRect StreamOverlay::sdlWindowRect() const
{
    int x, y, w, h;
    SDL_GetWindowPosition(m_Session->m_Window, &x, &y);
    SDL_GetWindowSize(m_Session->m_Window, &w, &h);
    return QRect(x, y, w, h);
}

void StreamOverlay::place(QWindow* w, const QRect& r)
{
#ifdef Q_OS_WIN32
    // Native pixels straight to Win32; Qt follows from WM_MOVE/WM_SIZE.
    w->winId(); // make sure it exists
    SetWindowPos((HWND)w->winId(), nullptr, r.x(), r.y(), r.width(), r.height(), SWP_NOZORDER | SWP_NOACTIVATE);
#else
    const qreal dpr = dprAt(r.center());
    w->setGeometry(QRect(QPoint(qRound(r.x() / dpr), qRound(r.y() / dpr)),
                         QSize(qRound(r.width() / dpr), qRound(r.height() / dpr))));
#endif
}

void StreamOverlay::updateGeometry()
{
    const QRect win = sdlWindowRect();
    const qreal dpr = dprAt(win.center());
    const int button = qRound(kButtonSize * dpr), margin = qRound(kMargin * dpr);
    if (m_Button) {
        place(m_Button, QRect(win.x() + (win.width() - button) / 2, win.y() + margin, button, button));
        m_Button->setMask(QRegion(0, 0, kButtonSize, kButtonSize, QRegion::Ellipse));
    }
    if (m_Osc) m_Osc->updateGeometry(win, dpr);
    if (m_Keyboard) {
        const int height = qMin(qRound(360 * dpr), win.height() * 45 / 100);
        place(m_Keyboard, QRect(win.x(), win.y() + win.height() - height, win.width(), height));
    }
    if (m_Notice) {
        const int width = qMin(qRound(380 * dpr), win.width() - 2 * margin), height = qRound(40 * dpr);
        place(m_Notice, QRect(win.x() + (win.width() - width) / 2, win.y() + 2 * margin + button, width, height));
    }
    if (m_Menu) {
        const int width = qMin(qRound(kMenuWidth * dpr), win.width() - 2 * margin);
        const int top = win.y() + 2 * margin + button;
        const int height = qMin(qRound(kMenuMaxHeight * dpr), win.y() + win.height() - margin - top);
        place(m_Menu, QRect(win.x() + (win.width() - width) / 2, top, width, qMax(height, qRound(200 * dpr))));
    }
}

void StreamOverlay::updateButtonVisibility()
{
    if (!m_Button) return;
    SdlInputHandler* in = m_Session->m_InputHandler;
    const Uint32 flags = SDL_GetWindowFlags(m_Session->m_Window);
    // The button can only be clicked while the cursor is free: desktop mouse mode (the
    // local cursor appears over the button) or capture released. While the stream holds the
    // mouse it stays visible, dimmed, as a reminder that Ctrl+Alt+Shift+<menu key> or
    // Select+L1+R1+B opens the menu (and frees the cursor). Same in windowed and fullscreen.
    const bool cursorFree = in && (in->m_AbsoluteMouseMode || !in->isCaptureActive());
    const bool visible = showButton() && !(flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
    m_Button->setOpacity(cursorFree ? 0.85 : 0.4);
    // Switching between windowed and fullscreen restyles the stream window (and its topmost
    // state); show the button afresh so it sits above the stream in the new mode.
    const Uint32 mode = flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    const bool modeChanged = mode != m_LastWindowMode;
    m_LastWindowMode = mode;
    if (visible && (modeChanged || !m_Button->isVisible())) {
        if (m_Button->isVisible()) m_Button->hide();
        updateGeometry();
        m_Button->show();
    }
    else if (!visible && m_Button->isVisible()) {
        m_Button->hide();
    }
}

void StreamOverlay::pump()
{
    if (!m_Button && !m_Menu) return;
    const Uint32 now = SDL_GetTicks();
    if (now - m_LastPump < (Uint32)maxWaitMs()) return;
    const bool notify = m_MenuOpen && now - m_LastPump >= 250;
    m_LastPump = now;
    QCoreApplication::processEvents(QEventLoop::AllEvents, m_MenuOpen ? 6 : 2);
    if (notify) emit changed(); // state can also change through hotkeys
    updateButtonVisibility();
}

bool StreamOverlay::handleEvent(const SDL_Event& event)
{
    if (!m_Button && !m_Menu) return false;

    if (event.type == SDL_WINDOWEVENT) {
        switch (event.window.event) {
        case SDL_WINDOWEVENT_MOVED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_SHOWN:
            updateGeometry();
            if (m_Osc && m_OscWanted && !m_Osc->isVisible()) m_Osc->setVisible(true); // back from minimized
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_HIDDEN:
            closeMenu();
            if (m_Keyboard) m_Keyboard->hide();
            if (m_Osc) m_Osc->setVisible(false);
            break;
        default:
            break;
        }
        updateButtonVisibility();
        return false;
    }

    if (event.type == SDL_KEYDOWN && isMenuHotkey(event.key)) {
        if (!event.key.repeat) toggleMenu();
        return true;
    }

    if (m_MenuOpen) {
        switch (event.type) {
        case SDL_KEYDOWN: case SDL_KEYUP: case SDL_TEXTINPUT: case SDL_TEXTEDITING:
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
        case SDL_CONTROLLERBUTTONDOWN:
            return true; // the menu is modal; button releases still reach the CloudDeck
        default:
            break;
        }
    }
    return false;
}

void StreamOverlay::openMenu()
{
    if (m_MenuOpen || !m_Menu) return;
    m_MenuOpen = true;
    m_OpenedAt = SDL_GetTicks();
    SdlInputHandler* in = m_Session->m_InputHandler;
    in->raiseAllKeys(); // the hotkey's modifiers already reached the CloudDeck
    m_RecaptureOnClose = in->isCaptureActive();
    if (m_RecaptureOnClose) in->setCaptureActive(false); // free the cursor for the menu
    updateGeometry();
    emit changed();
    m_Menu->show();
    m_Menu->raise();
    m_Menu->requestActivate();
}

void StreamOverlay::closeMenu()
{
    if (!m_MenuOpen) return;
    m_MenuOpen = false;
    if (m_Menu) m_Menu->hide();
    SDL_RaiseWindow(m_Session->m_Window);
    if (m_RecaptureOnClose && m_Session->m_InputHandler) m_Session->m_InputHandler->setCaptureActive(true);
    m_RecaptureOnClose = false;
    emit changed();
}

QString StreamOverlay::streamInfo() const
{
    return tr("%1 × %2 · %3 FPS").arg(m_Session->m_StreamConfig.width).arg(m_Session->m_StreamConfig.height)
        .arg(m_Session->m_StreamConfig.fps);
}

bool StreamOverlay::canToggleFullscreen() const { return !m_Session->m_Spanning; }
bool StreamOverlay::fullscreen() const { return SDL_GetWindowFlags(m_Session->m_Window) & SDL_WINDOW_FULLSCREEN; }
void StreamOverlay::setFullscreen(bool on)
{
    if (on != fullscreen() && canToggleFullscreen()) {
        m_Session->toggleFullscreen();
        emit changed();
    }
}

bool StreamOverlay::canFitToWindow() const { return !m_Session->m_Spanning; }
bool StreamOverlay::fitToWindow() const { return m_Session->m_FitToWindow; }
void StreamOverlay::setFitToWindow(bool on)
{
    DisplaySettings::get()->setMatchResolution(on);
    m_Session->m_FitToWindow = on && !m_Session->m_Spanning;
    if (m_Session->m_FitToWindow) m_Session->m_ResizeCheckAt = SDL_GetTicks() + 300; // fit now
    emit changed();
}

bool StreamOverlay::statsVisible() const { return m_Session->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug); }
void StreamOverlay::setStatsVisible(bool on)
{
    m_Session->getOverlayManager().setOverlayState(Overlay::OverlayDebug, on);
    emit changed();
}

bool StreamOverlay::desktopMouse() const { return m_Session->m_InputHandler && m_Session->m_InputHandler->m_AbsoluteMouseMode; }
void StreamOverlay::setDesktopMouse(bool on)
{
    SdlInputHandler* in = m_Session->m_InputHandler;
    if (!in || in->m_AbsoluteMouseMode == on) return;
    // Capture is released while the menu is open; closing it recaptures in the new mode.
    in->m_AbsoluteMouseMode = on;
    m_Session->m_Preferences->absoluteMouseMode = on; // remembered for the next stream
    m_Session->m_Preferences->save();
    emit changed();
}

int StreamOverlay::systemKeysMode() const
{
    // The saved choice; Ctrl+Alt+Shift+K and the tile only switch this stream on or off.
    return m_Session->m_Preferences->captureSysKeysMode;
}

void StreamOverlay::setSystemKeysMode(int mode)
{
    const auto m = static_cast<StreamingPreferences::CaptureSysKeysMode>(mode);
    m_Session->m_Preferences->captureSysKeysMode = m; // remembered for next time
    m_Session->m_Preferences->save();
    if (m_Session->m_InputHandler) m_Session->m_InputHandler->setSystemKeysMode(m);
    emit changed();
}

bool StreamOverlay::systemKeysOn() const
{
    return m_Session->m_InputHandler &&
           m_Session->m_InputHandler->systemKeysMode() != StreamingPreferences::CSK_OFF;
}

bool StreamOverlay::canCaptureSystemKeys() const
{
    // Without a desktop environment they always go to the CloudDeck.
    return WMUtils::isRunningDesktopEnvironment();
}

void StreamOverlay::toggleSystemKeys()
{
    SdlInputHandler* in = m_Session->m_InputHandler;
    if (!in) return;
    in->setSystemKeysMode(in->toggledSystemKeysMode());
    emit changed();
}

bool StreamOverlay::audioMuted() const { return m_Session->m_UserAudioMuted; }
void StreamOverlay::setAudioMuted(bool on)
{
    m_Session->m_UserAudioMuted = on;
    emit changed();
}

bool StreamOverlay::micEnabled() const { return m_Session->m_StreamConfig.enableMicrophone; }
void StreamOverlay::setMicEnabled(bool on)
{
    if (on == micEnabled()) return;
    // The microphone is negotiated when the stream starts: reconnect in place.
    MicStreamer::get()->setEnabled(on);
    m_Session->m_StreamConfig.enableMicrophone = on;
    reconnect();
    emit changed();
}

bool StreamOverlay::showButton() const { return QSettings().value("clouddeck/overlay/showButton", true).toBool(); }
void StreamOverlay::setShowButton(bool on)
{
    QSettings().setValue("clouddeck/overlay/showButton", on);
    updateButtonVisibility();
    emit changed();
}

bool StreamOverlay::canShutDown() const { return CloudSession::ownsActiveStream(); }

void StreamOverlay::reconnect()
{
    if (!m_Session->m_Restarting) {
        m_Session->restartStreamAt(m_Session->m_StreamConfig.width, m_Session->m_StreamConfig.height);
    }
}

void StreamOverlay::disconnectStream()
{
    closeMenu();
    SDL_Event quit = {};
    quit.type = SDL_QUIT;
    quit.quit.timestamp = SDL_GetTicks();
    SDL_PushEvent(&quit);
}

void StreamOverlay::shutDown()
{
    CloudSession::requestStopAfterStream();
    disconnectStream();
}

void StreamOverlay::showNotice(const QString& text)
{
    m_NoticeText = text;
    emit changed();
    if (!m_Notice) return;
    updateGeometry();
    m_Notice->show();
}

void StreamOverlay::hideNotice()
{
    m_NoticeText.clear();
    emit changed();
    if (m_Notice) m_Notice->hide();
}

QString StreamOverlay::menuKey() const { return Shortcuts::keyFor("menu"); }

// Xbox / Nintendo face-button layout (A/B and X/Y swapped), live and remembered.
bool StreamOverlay::nintendoLayout() const { return m_Session->m_InputHandler && m_Session->m_InputHandler->m_SwapFaceButtons; }
void StreamOverlay::setNintendoLayout(bool on)
{
    if (!m_Session->m_InputHandler || m_Session->m_InputHandler->m_SwapFaceButtons == on) return;
    m_Session->m_InputHandler->m_SwapFaceButtons = on;
    m_Session->m_Preferences->swapFaceButtons = on;
    m_Session->m_Preferences->save();
    emit changed();
}

bool StreamOverlay::keyboardVisible() const { return m_Keyboard && m_Keyboard->isVisible(); }

void StreamOverlay::toggleKeyboard()
{
    if (!m_Keyboard) return;
    if (m_Keyboard->isVisible()) {
        m_Keyboard->hide();
    }
    else {
        updateGeometry();
        m_Keyboard->show();
        m_Keyboard->raise();
    }
    emit changed();
}

void StreamOverlay::sendKey(int vk, bool down, int modifiers)
{
    // Same encoding as Moonlight's own keyboard path (keyboard.cpp).
    LiSendKeyboardEvent2((short)(0x8000 | (vk & 0xFF)), down ? KEY_ACTION_DOWN : KEY_ACTION_UP, (char)modifiers, 0);
}

bool StreamOverlay::controllerVisible() const { return m_Osc && m_Osc->isVisible(); }

void StreamOverlay::toggleController()
{
    if (!m_Osc) return;
    m_OscWanted = !m_Osc->isVisible();
    OnScreenController::setEnabledSetting(m_OscWanted); // remembered for the next stream
    if (m_OscWanted) updateGeometry();
    m_Osc->setVisible(m_OscWanted);
    emit changed();
}

bool StreamOverlay::controllerPlaystation() const { return OnScreenController::styleSetting() == QLatin1String("playstation"); }

void StreamOverlay::setControllerPlaystation(bool on)
{
    OnScreenController::setStyleSetting(on ? "playstation" : "xbox");
    if (m_Osc) m_Osc->refreshStyle();
    emit changed();
}

// Stream resolution from the menu. Fixed sizes also become the default for the next stream.
QString StreamOverlay::resolution() const
{
    if (m_Session->m_FitToWindow) return QStringLiteral("fit");
    return QStringLiteral("%1x%2").arg(m_Session->m_StreamConfig.width).arg(m_Session->m_StreamConfig.height);
}

QVariantList StreamOverlay::resolutionChoices() const
{
    QVariantList list;
    auto add = [&list](const QString& text, const QString& value) {
        for (const QVariant& v : list) if (v.toMap().value("val") == value) return;
        list.append(QVariantMap{{"text", text}, {"val", value}});
    };
    if (canFitToWindow()) add(tr("Fit to window"), QStringLiteral("fit"));
    add(QStringLiteral("720p (1280 × 720)"), QStringLiteral("1280x720"));
    add(QStringLiteral("1080p (1920 × 1080)"), QStringLiteral("1920x1080"));
    add(QStringLiteral("1440p (2560 × 1440)"), QStringLiteral("2560x1440"));
    add(QStringLiteral("4K (3840 × 2160)"), QStringLiteral("3840x2160"));
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(m_Session->m_Window), &mode) == 0) {
        const int w = mode.w & ~7;
        add(tr("This screen (%1 × %2)").arg(w).arg(mode.h), QStringLiteral("%1x%2").arg(w).arg(mode.h));
    }
    if (!m_Session->m_FitToWindow) {
        const QString cur = resolution();
        add(tr("Current (%1 × %2)").arg(m_Session->m_StreamConfig.width).arg(m_Session->m_StreamConfig.height), cur);
    }
    return list;
}

void StreamOverlay::setResolution(const QString& value)
{
    if (m_Session->m_Spanning || m_Session->m_Restarting) return;
    if (value == QLatin1String("fit")) {
        setFitToWindow(true);
        return;
    }
    const QStringList wh = value.split('x');
    const int w = wh.value(0).toInt() & ~7, h = wh.value(1).toInt() & ~1;
    if (w < 256 || h < 256) return;
    if (m_Session->m_FitToWindow) setFitToWindow(false);
    m_Session->m_Preferences->width = w; // remembered for the next stream
    m_Session->m_Preferences->height = h;
    m_Session->m_Preferences->save();
    if (w != m_Session->m_StreamConfig.width || h != m_Session->m_StreamConfig.height) {
        closeMenu();
        m_Session->restartStreamAt(w, h);
    }
    emit changed();
}

// Controller mouse mode (docs/controller-mouse.md)
bool StreamOverlay::hasGamepad() const { return m_Session->m_InputHandler && m_Session->m_InputHandler->hasGamepad(); }
bool StreamOverlay::controllerMouse() const { return m_Session->m_InputHandler && m_Session->m_InputHandler->anyGamepadMouseMode(); }
void StreamOverlay::setControllerMouse(bool on)
{
    if (!m_Session->m_InputHandler) return;
    m_Session->m_InputHandler->setAllGamepadMouseMode(on);
    emit changed();
}

void StreamOverlay::onStreamRestarted()
{
    if (m_Osc && m_Osc->isVisible()) {
        m_Osc->setVisible(false);
        m_Osc->setVisible(true);
    }
}
