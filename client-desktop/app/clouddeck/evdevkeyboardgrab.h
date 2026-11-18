// CloudDeck-Client: system shortcuts on Wayland compositors without keyboard-shortcuts-inhibit
// (docs/keyboard-capture.md), above all labwc on Raspberry Pi OS.
//
// SDL's keyboard grab on Wayland is the keyboard-shortcuts-inhibit protocol. Where the compositor
// doesn't offer it, the grab does nothing and the compositor keeps Win, Alt+Tab and its other
// bindings. There, while the stream window has the keyboard, this takes the keyboards one level
// lower: it grabs their /dev/input event devices (EVIOCGRAB), so the compositor sees no keys at
// all, and feeds the keys to the stream as ordinary SDL key events. Losing focus, turning capture
// off or ending the stream gives the keyboards back.
//
// Needs read access to /dev/input/event*: the "input" group (the Raspberry Pi OS default user is
// in it). Without it nothing is grabbed and the stream gets keys from the compositor as before.
//
// Linux only; elsewhere every call is a no-op.
#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

struct SDL_Window;

class EvdevKeyboardGrab
{
public:
    EvdevKeyboardGrab() = default;
    ~EvdevKeyboardGrab() { stop(); }
    EvdevKeyboardGrab(const EvdevKeyboardGrab&) = delete;
    EvdevKeyboardGrab& operator=(const EvdevKeyboardGrab&) = delete;

    // True when SDL runs on native Wayland and the compositor has no keyboard-shortcuts-inhibit.
    // CLOUDDECK_EVDEV_KEYS=1 or =0 forces it on or off. Decided once per process.
    static bool needed();

    // Main thread. Grab while active (the stream window has the keyboard and capture is on).
    // A grab starts once no key is held, so the compositor never misses a key release.
    void setActive(SDL_Window* window, bool active);
    void stop();

    // Main thread: true once after a grab found keyboards it wasn't allowed to open.
    bool takeAccessDenied() { return m_AccessDenied.exchange(false); }

private:
    void threadMain();
    void wake();

    std::thread m_Thread;
    int m_WakeFd = -1;
    std::atomic<bool> m_Want{false};
    std::atomic<bool> m_Quit{false};
    std::atomic<bool> m_AccessDenied{false};
    std::atomic<uint32_t> m_WindowId{0};
};
