// CloudDeck-Client: "Always" mode for system shortcuts (docs/keyboard-capture.md).
//
// While a stream runs and another window on this PC is active, Win, Win+<key>, Alt+Tab,
// Alt+Shift+Tab, Alt+Esc and Ctrl+Esc are taken away from Windows and sent to the CloudDeck.
// While the stream window itself is active, SDL's own keyboard grab does that job.
//
// Windows only: X11 window managers hold passive grabs on these keys and Wayland doesn't let
// clients take them at all, so on Linux "Always" behaves like "While the stream window is active".
//
// No <windows.h> here: this header reaches the input code, where its min/max macros break things.
#pragma once

#include <atomic>
#include <thread>

struct SDL_Window;

class SystemKeyForwarder
{
public:
    SystemKeyForwarder() = default;
    ~SystemKeyForwarder() { stop(); }
    SystemKeyForwarder(const SystemKeyForwarder&) = delete;
    SystemKeyForwarder& operator=(const SystemKeyForwarder&) = delete;

    // True where "Always" can reach beyond the stream window.
    static bool supported();

    // streamWindow: the stream's SDL window; its own keys are left to SDL's grab.
    void start(SDL_Window* streamWindow);
    void stop();
    bool running() const { return m_Thread.joinable(); }

    // Called from the low-level hook (hook thread). Returns true to swallow the key.
    bool handle(bool down, unsigned long vk, unsigned long flags);

private:
    void threadMain();
    void send(unsigned long vk, bool down, bool extended);
    void holdRemote(unsigned long vk);     // modifier down on the CloudDeck (once)
    void releaseRemote(unsigned long vk);  // modifier up on the CloudDeck, if we pressed it
    void releaseAll();
    char remoteModifiers() const;

    void* m_Window = nullptr;
    std::thread m_Thread;
    std::atomic<unsigned long> m_ThreadId{0};
    // Touched only on the hook thread:
    bool m_WinDown[2] = {false, false};  // LWIN, RWIN held on the CloudDeck
    bool m_RemoteMods[6] = {};           // LSHIFT RSHIFT LCTRL RCTRL LALT RALT (index vk - VK_LSHIFT)
    bool m_SentDown[256] = {};           // keys currently pressed on the CloudDeck by us
    bool m_SwallowedWithAlt = false;     // mask the next local Alt release (no menu-bar flash)
    bool m_Active = false;               // forwarding (stream window not in front)
};
