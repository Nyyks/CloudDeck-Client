// CloudDeck-Client: see systemkeyforwarder.h.
#include "systemkeyforwarder.h"

#include <QtGlobal>

#ifndef Q_OS_WIN32

bool SystemKeyForwarder::supported() { return false; }
void SystemKeyForwarder::start(SDL_Window*) {}
void SystemKeyForwarder::stop() {}
bool SystemKeyForwarder::handle(bool, unsigned long, unsigned long) { return false; }
void SystemKeyForwarder::threadMain() {}
void SystemKeyForwarder::send(unsigned long, bool, bool) {}
void SystemKeyForwarder::holdRemote(unsigned long) {}
void SystemKeyForwarder::releaseRemote(unsigned long) {}
void SystemKeyForwarder::releaseAll() {}
char SystemKeyForwarder::remoteModifiers() const { return 0; }

#else

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Limelight.h>
#include <QDebug>
#include "SDL_compat.h"
#include <SDL_syswm.h>

namespace {
std::atomic<SystemKeyForwarder*> s_Self{nullptr};
constexpr WORD kMaskKey = 0xE8; // unassigned VK: an Alt release after it doesn't open menu bars

bool isModifier(unsigned long vk) { return vk >= VK_LSHIFT && vk <= VK_RMENU; }
bool held(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
bool extendedModifier(unsigned long vk) { return vk == VK_RCONTROL || vk == VK_RMENU; }

LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam)
{
    SystemKeyForwarder* self = s_Self.load();
    if (code == HC_ACTION && self) {
        const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        if (self->handle(down, k->vkCode, k->flags)) {
            return 1; // swallowed: Windows never sees it
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
}

bool SystemKeyForwarder::supported() { return true; }

void SystemKeyForwarder::start(SDL_Window* streamWindow)
{
    if (running() || !streamWindow) return;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(streamWindow, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return;
    SystemKeyForwarder* expected = nullptr;
    if (!s_Self.compare_exchange_strong(expected, this)) return; // one stream at a time
    m_Window = info.info.win.window;
    m_ThreadId = 0;
    m_Thread = std::thread(&SystemKeyForwarder::threadMain, this);
    for (int i = 0; i < 1000 && m_ThreadId == 0; i++) Sleep(1); // until it can take WM_QUIT
    qInfo() << "CloudDeck: forwarding system shortcuts to the CloudDeck while other windows are active";
}

void SystemKeyForwarder::stop()
{
    if (!running()) return;
    PostThreadMessageW(m_ThreadId, WM_QUIT, 0, 0);
    m_Thread.join();
    s_Self = nullptr;
    m_ThreadId = 0;
}

void SystemKeyForwarder::threadMain()
{
    // A low-level hook runs on the thread that installed it, through that thread's message
    // loop. Its own thread keeps system-wide typing from waiting on a busy stream thread.
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE); // create the message queue
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
    if (!hook) qWarning() << "CloudDeck: SetWindowsHookEx(WH_KEYBOARD_LL) failed:" << GetLastError();
    m_ThreadId = GetCurrentThreadId();
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (hook) UnhookWindowsHookEx(hook);
    releaseAll();
}

bool SystemKeyForwarder::handle(bool down, unsigned long vk, unsigned long flags)
{
    if (flags & LLKHF_INJECTED) return false; // ours (the mask key), or other software's
    if (vk > 0xFF) return false;
    const bool extended = flags & LLKHF_EXTENDED;

    // The stream window (or its menu/overlays, which it owns) is in front: SDL's grab has it.
    const HWND stream = static_cast<HWND>(m_Window);
    const HWND front = GetForegroundWindow();
    if (front && (front == stream || GetAncestor(front, GA_ROOTOWNER) == stream)) {
        if (m_Active) {
            releaseAll();
            m_Active = false;
        }
        return false;
    }
    m_Active = true;

    // Finish what we started: a key whose press went to the CloudDeck releases there too.
    if (!down && !isModifier(vk) && vk != VK_LWIN && vk != VK_RWIN && m_SentDown[vk]) {
        send(vk, false, extended);
        return true;
    }

    // Win and anything pressed while it's held (Win+E, Win+Shift+S, ...).
    if (vk == VK_LWIN || vk == VK_RWIN) {
        if (down && !m_WinDown[vk == VK_RWIN]) {
            // Modifiers already held locally belong to the combo too.
            for (int m : {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU}) {
                if (held(m)) holdRemote(m);
            }
        }
        m_WinDown[vk == VK_RWIN] = down;
        send(vk, down, true);
        return true;
    }
    if (m_WinDown[0] || m_WinDown[1]) {
        if (isModifier(vk)) {
            if (down) holdRemote(vk);
            else releaseRemote(vk);
        }
        else {
            send(vk, down, extended);
        }
        return true;
    }

    // Alt+Tab, Alt+Shift+Tab, Alt+Esc, Ctrl+Esc.
    const bool alt = flags & LLKHF_ALTDOWN;
    const bool ctrl = held(VK_CONTROL);
    if (down && ((vk == VK_TAB && alt) || (vk == VK_ESCAPE && (alt || ctrl)))) {
        if (alt) {
            holdRemote(held(VK_RMENU) ? VK_RMENU : VK_LMENU);
            m_SwallowedWithAlt = true;
        }
        if (ctrl) holdRemote(held(VK_RCONTROL) ? VK_RCONTROL : VK_LCONTROL);
        if (held(VK_SHIFT)) holdRemote(held(VK_RSHIFT) ? VK_RSHIFT : VK_LSHIFT);
        send(vk, true, false);
        return true;
    }

    // A modifier we pressed on the CloudDeck is released: release it there; locally it passes
    // through (the local app saw it go down). After a swallowed Alt+Tab, replay Alt's release
    // behind a mask key so the local app doesn't take it as a lone Alt tap (menu bar).
    if (!down && isModifier(vk)) {
        releaseRemote(vk);
        if ((vk == VK_LMENU || vk == VK_RMENU) && m_SwallowedWithAlt) {
            m_SwallowedWithAlt = false;
            INPUT in[3] = {};
            for (INPUT& i : in) i.type = INPUT_KEYBOARD;
            in[0].ki.wVk = kMaskKey;
            in[1].ki.wVk = kMaskKey;
            in[1].ki.dwFlags = KEYEVENTF_KEYUP;
            in[2].ki.wVk = static_cast<WORD>(vk);
            in[2].ki.dwFlags = KEYEVENTF_KEYUP | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
            SendInput(3, in, sizeof(INPUT));
            return true;
        }
    }
    return false;
}

char SystemKeyForwarder::remoteModifiers() const
{
    char m = 0;
    if (m_RemoteMods[0] || m_RemoteMods[1]) m |= MODIFIER_SHIFT;
    if (m_RemoteMods[2] || m_RemoteMods[3]) m |= MODIFIER_CTRL;
    if (m_RemoteMods[4] || m_RemoteMods[5]) m |= MODIFIER_ALT;
    if (m_WinDown[0] || m_WinDown[1]) m |= MODIFIER_META;
    return m;
}

void SystemKeyForwarder::send(unsigned long vk, bool down, bool extended)
{
    m_SentDown[vk & 0xFF] = down;
    LiSendKeyboardEvent2(static_cast<short>(vk), down ? KEY_ACTION_DOWN : KEY_ACTION_UP,
                         remoteModifiers() | (extended ? MODIFIER_EXTENDED : 0), 0);
}

void SystemKeyForwarder::holdRemote(unsigned long vk)
{
    bool& h = m_RemoteMods[vk - VK_LSHIFT];
    if (h) return;
    h = true;
    send(vk, true, extendedModifier(vk));
}

void SystemKeyForwarder::releaseRemote(unsigned long vk)
{
    bool& h = m_RemoteMods[vk - VK_LSHIFT];
    if (!h) return;
    h = false;
    send(vk, false, extendedModifier(vk));
}

void SystemKeyForwarder::releaseAll()
{
    for (unsigned long vk = 0; vk <= 0xFF; vk++) {
        if (m_SentDown[vk] && !isModifier(vk) && vk != VK_LWIN && vk != VK_RWIN) send(vk, false, false);
    }
    for (unsigned long vk = VK_LSHIFT; vk <= VK_RMENU; vk++) releaseRemote(vk);
    for (int i = 0; i < 2; i++) {
        if (m_WinDown[i]) {
            m_WinDown[i] = false;
            send(i ? VK_RWIN : VK_LWIN, false, true);
        }
    }
    m_SwallowedWithAlt = false;
}

#endif
