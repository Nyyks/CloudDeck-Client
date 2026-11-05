#include "quickmenu.h"

#include "cloudsession.h"
#include "micstreamer.h"
#include "streaming/session.h"

#include <QCoreApplication>

static QString tr(const char* s) { return QCoreApplication::translate("QuickMenu", s); }

static bool isMenuHotkey(const SDL_KeyboardEvent& k)
{
    return (k.keysym.mod & KMOD_CTRL) && (k.keysym.mod & KMOD_ALT) && (k.keysym.mod & KMOD_SHIFT) &&
           (k.keysym.sym == SDLK_o || k.keysym.scancode == SDL_SCANCODE_O);
}

void QuickMenu::open()
{
    if (m_Open) return;
    m_Open = true;
    m_Selected = 0;
    m_ConfirmShutdown = false;
    // The hotkey's modifiers already reached the CloudDeck; release them there.
    m_Session->m_InputHandler->raiseAllKeys();
    m_SavedColor = m_Session->getOverlayManager().getOverlayColor(Overlay::OverlayStatusUpdate);
    m_SavedFontSize = m_Session->getOverlayManager().getOverlayFontSize(Overlay::OverlayStatusUpdate);
    m_Session->getOverlayManager().setOverlayStyle(Overlay::OverlayStatusUpdate, {0xF0, 0xF0, 0xF0, 0xFF}, 26);
    m_Items = buildItems();
    render();
}

void QuickMenu::close()
{
    if (!m_Open) return;
    m_Open = false;
    m_ConfirmShutdown = false;
    // Give the status overlay back (it also shows the gamepad mouse-mode notice).
    Overlay::OverlayManager& om = m_Session->getOverlayManager();
    om.setOverlayStyle(Overlay::OverlayStatusUpdate, m_SavedColor, m_SavedFontSize);
    if (m_Session->m_MouseEmulationRefCount > 0) {
        om.updateOverlayText(Overlay::OverlayStatusUpdate, "Gamepad mouse mode active\nLong press Start to deactivate");
    }
    else {
        om.setOverlayState(Overlay::OverlayStatusUpdate, false);
    }
}

QVector<QuickMenu::Item> QuickMenu::buildItems()
{
    Session* s = m_Session;
    SdlInputHandler* in = s->m_InputHandler;
    QVector<Item> items;

    if (!s->m_Spanning) {
        items.append({[s]() {
                          return (SDL_GetWindowFlags(s->m_Window) & SDL_WINDOW_FULLSCREEN) ? tr("Switch to a window")
                                                                                           : tr("Switch to fullscreen");
                      },
                      [in]() { in->performSpecialKeyCombo(SdlInputHandler::KeyComboToggleFullScreen); }});
    }
    if (MicStreamer::get()->enabled()) {
        items.append({[]() { return MicStreamer::get()->muted() ? tr("Unmute the microphone") : tr("Mute the microphone"); },
                      []() { MicStreamer::get()->toggleMute(); }, false});
    }
    items.append({[s]() {
                      return s->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug) ? tr("Hide performance stats")
                                                                                            : tr("Show performance stats");
                  },
                  [in]() { in->performSpecialKeyCombo(SdlInputHandler::KeyComboToggleStatsOverlay); }, false});
    items.append({[in]() {
                      return in->m_PointerRegionLockActive ? tr("Let the mouse leave the window")
                                                           : tr("Keep the mouse inside the window");
                  },
                  [in]() { in->performSpecialKeyCombo(SdlInputHandler::KeyComboTogglePointerRegionLock); }, false});
    items.append({[]() { return tr("Reconnect the stream"); },
                  [s]() {
                      if (!s->m_Restarting) s->restartStreamAt(s->m_StreamConfig.width, s->m_StreamConfig.height);
                  }});
    items.append({[]() { return tr("Disconnect"); },
                  [in]() { in->performSpecialKeyCombo(SdlInputHandler::KeyComboQuit); }});
    if (CloudSession::ownsActiveStream()) {
        items.append({[this]() {
                          return m_ConfirmShutdown ? tr("Press Enter again to shut down your CloudDeck")
                                                   : tr("Shut down my CloudDeck");
                      },
                      [this, in]() {
                          if (!m_ConfirmShutdown) {
                              m_ConfirmShutdown = true;
                              render();
                              return;
                          }
                          CloudSession::requestStopAfterStream();
                          in->performSpecialKeyCombo(SdlInputHandler::KeyComboQuit);
                          close();
                      },
                      false});
    }
    items.append({[]() { return tr("Close this menu"); }, []() {}});
    return items;
}

void QuickMenu::render()
{
    const Session* s = m_Session;
    QString text = tr("CloudDeck Quick Menu") + "\n";
    QString info = tr("%1x%2 at %3 FPS").arg(s->m_StreamConfig.width).arg(s->m_StreamConfig.height).arg(s->m_StreamConfig.fps);
    if (MicStreamer::get()->enabled()) {
        info += MicStreamer::get()->muted() ? tr(" - mic muted") : tr(" - mic on");
    }
    text += info + "\n\n";
    for (int i = 0; i < m_Items.size(); i++) {
        text += (i == m_Selected ? "> " : "  ") + QString::number(i + 1) + "  " + m_Items[i].label() + "\n";
    }
    text += "\n" + tr("Up/Down: choose   Enter: select   Esc: close");

    Overlay::OverlayManager& om = m_Session->getOverlayManager();
    om.updateOverlayText(Overlay::OverlayStatusUpdate, text.toUtf8().constData());
    om.setOverlayState(Overlay::OverlayStatusUpdate, true);
}

void QuickMenu::move(int delta)
{
    if (m_Items.isEmpty()) return;
    m_ConfirmShutdown = false;
    m_Selected = (m_Selected + delta + m_Items.size()) % m_Items.size();
    render();
}

void QuickMenu::activateSelected()
{
    if (m_Selected < 0 || m_Selected >= m_Items.size()) return;
    Item item = m_Items[m_Selected];
    bool wasConfirming = m_ConfirmShutdown;
    if (item.closesMenu) {
        close(); // before the action: it may end the stream or restart it
    }
    item.activate();
    if (m_Open) {
        if (wasConfirming == m_ConfirmShutdown) m_ConfirmShutdown = false;
        render();
    }
}

bool QuickMenu::handleEvent(const SDL_Event& event)
{
    if (!m_Open) {
        if (event.type == SDL_KEYDOWN && !event.key.repeat && isMenuHotkey(event.key)) {
            open();
            return true;
        }
        return false;
    }

    switch (event.type) {
    case SDL_KEYDOWN:
        if (isMenuHotkey(event.key)) {
            if (!event.key.repeat) close();
            return true;
        }
        switch (event.key.keysym.sym) {
        case SDLK_UP: case SDLK_KP_8: move(-1); break;
        case SDLK_DOWN: case SDLK_KP_2: case SDLK_TAB: move(1); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE:
            if (!event.key.repeat) activateSelected();
            break;
        case SDLK_ESCAPE: close(); break;
        default:
            if (!event.key.repeat && event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_9) {
                int i = event.key.keysym.sym - SDLK_1;
                if (i < m_Items.size()) {
                    if (i != m_Selected) m_ConfirmShutdown = false;
                    m_Selected = i;
                    activateSelected();
                }
            }
            break;
        }
        return true;
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
    case SDL_TEXTEDITING:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
        return true; // the menu is modal for keys and clicks
    case SDL_CONTROLLERBUTTONDOWN:
        switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: move(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move(1); break;
        case SDL_CONTROLLER_BUTTON_A: activateSelected(); break;
        case SDL_CONTROLLER_BUTTON_B: close(); break;
        default: break;
        }
        return true;
    // Button releases pass through so the CloudDeck never sees a button stuck down.
    default:
        return false;
    }
}
