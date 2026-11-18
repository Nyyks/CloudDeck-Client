// CloudDeck-Client: see evdevkeyboardgrab.h.
#include "evdevkeyboardgrab.h"

#include <QtGlobal>

#ifndef Q_OS_LINUX

bool EvdevKeyboardGrab::needed() { return false; }
void EvdevKeyboardGrab::setActive(SDL_Window*, bool) {}
void EvdevKeyboardGrab::stop() {}
void EvdevKeyboardGrab::threadMain() {}
void EvdevKeyboardGrab::wake() {}

#else

#include "SDL_compat.h"

#include <linux/input.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <bitset>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef HAS_WAYLAND
#include <wayland-client.h>
#endif

namespace {

constexpr Uint32 kHeldGraceMs = 3000; // a key held this long doesn't hold up the grab any more
constexpr Uint32 kStallMs = 2000;     // our keys sat in SDL's queue this long: the stream loop is stuck

template <size_t N>
bool testBit(const unsigned char (&bits)[N], int bit)
{
    return bit >= 0 && size_t(bit / 8) < N && ((bits[bit / 8] >> (bit % 8)) & 1);
}

// A keyboard: letter keys, and no pointer axes. A device that also moves a pointer (some
// keyboards with a built-in touchpad) stays with the compositor: grabbing it would take the
// pointer too.
bool isKeyboard(int fd)
{
    unsigned char types[EV_MAX / 8 + 1] = {};
    unsigned char keys[KEY_MAX / 8 + 1] = {};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) < 0 || !testBit(types, EV_KEY)) return false;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return false;
    for (int k : {KEY_A, KEY_Z, KEY_SPACE, KEY_ENTER}) {
        if (!testBit(keys, k)) return false;
    }
    if (testBit(types, EV_ABS)) return false;
    if (testBit(types, EV_REL)) {
        unsigned char rel[REL_MAX / 8 + 1] = {};
        if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel)), rel) >= 0 && (testBit(rel, REL_X) || testBit(rel, REL_Y))) return false;
    }
    return true;
}

bool anyKeyHeld(int fd)
{
    unsigned char keys[KEY_MAX / 8 + 1] = {};
    if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) < 0) return false;
    for (unsigned char b : keys) {
        if (b) return true;
    }
    return false;
}

// Linux key code -> SDL scancode (USB HID usage), as SDL's own evdev table, for the keys the
// stream maps to Windows virtual keys.
SDL_Scancode toSdl(int code)
{
    static const char kRow1[] = "QWERTYUIOP", kRow2[] = "ASDFGHJKL", kRow3[] = "ZXCVBNM";
    if (code >= KEY_1 && code <= KEY_9) return SDL_Scancode(SDL_SCANCODE_1 + (code - KEY_1));
    if (code >= KEY_Q && code <= KEY_P) return SDL_Scancode(SDL_SCANCODE_A + (kRow1[code - KEY_Q] - 'A'));
    if (code >= KEY_A && code <= KEY_L) return SDL_Scancode(SDL_SCANCODE_A + (kRow2[code - KEY_A] - 'A'));
    if (code >= KEY_Z && code <= KEY_M) return SDL_Scancode(SDL_SCANCODE_A + (kRow3[code - KEY_Z] - 'A'));
    if (code >= KEY_F1 && code <= KEY_F10) return SDL_Scancode(SDL_SCANCODE_F1 + (code - KEY_F1));
    if (code >= KEY_F13 && code <= KEY_F24) return SDL_Scancode(SDL_SCANCODE_F13 + (code - KEY_F13));
    switch (code) {
    case KEY_ESC: return SDL_SCANCODE_ESCAPE;
    case KEY_0: return SDL_SCANCODE_0;
    case KEY_MINUS: return SDL_SCANCODE_MINUS;
    case KEY_EQUAL: return SDL_SCANCODE_EQUALS;
    case KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
    case KEY_TAB: return SDL_SCANCODE_TAB;
    case KEY_LEFTBRACE: return SDL_SCANCODE_LEFTBRACKET;
    case KEY_RIGHTBRACE: return SDL_SCANCODE_RIGHTBRACKET;
    case KEY_ENTER: return SDL_SCANCODE_RETURN;
    case KEY_LEFTCTRL: return SDL_SCANCODE_LCTRL;
    case KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
    case KEY_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
    case KEY_GRAVE: return SDL_SCANCODE_GRAVE;
    case KEY_LEFTSHIFT: return SDL_SCANCODE_LSHIFT;
    case KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
    case KEY_COMMA: return SDL_SCANCODE_COMMA;
    case KEY_DOT: return SDL_SCANCODE_PERIOD;
    case KEY_SLASH: return SDL_SCANCODE_SLASH;
    case KEY_RIGHTSHIFT: return SDL_SCANCODE_RSHIFT;
    case KEY_KPASTERISK: return SDL_SCANCODE_KP_MULTIPLY;
    case KEY_LEFTALT: return SDL_SCANCODE_LALT;
    case KEY_SPACE: return SDL_SCANCODE_SPACE;
    case KEY_CAPSLOCK: return SDL_SCANCODE_CAPSLOCK;
    case KEY_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;
    case KEY_SCROLLLOCK: return SDL_SCANCODE_SCROLLLOCK;
    case KEY_KP7: return SDL_SCANCODE_KP_7;
    case KEY_KP8: return SDL_SCANCODE_KP_8;
    case KEY_KP9: return SDL_SCANCODE_KP_9;
    case KEY_KPMINUS: return SDL_SCANCODE_KP_MINUS;
    case KEY_KP4: return SDL_SCANCODE_KP_4;
    case KEY_KP5: return SDL_SCANCODE_KP_5;
    case KEY_KP6: return SDL_SCANCODE_KP_6;
    case KEY_KPPLUS: return SDL_SCANCODE_KP_PLUS;
    case KEY_KP1: return SDL_SCANCODE_KP_1;
    case KEY_KP2: return SDL_SCANCODE_KP_2;
    case KEY_KP3: return SDL_SCANCODE_KP_3;
    case KEY_KP0: return SDL_SCANCODE_KP_0;
    case KEY_KPDOT: return SDL_SCANCODE_KP_PERIOD;
    case KEY_ZENKAKUHANKAKU: return SDL_SCANCODE_LANG5;
    case KEY_102ND: return SDL_SCANCODE_NONUSBACKSLASH;
    case KEY_F11: return SDL_SCANCODE_F11;
    case KEY_F12: return SDL_SCANCODE_F12;
    case KEY_RO: return SDL_SCANCODE_INTERNATIONAL1;
    case KEY_KATAKANA: return SDL_SCANCODE_LANG3;
    case KEY_HIRAGANA: return SDL_SCANCODE_LANG4;
    case KEY_HENKAN: return SDL_SCANCODE_INTERNATIONAL4;
    case KEY_KATAKANAHIRAGANA: return SDL_SCANCODE_INTERNATIONAL2;
    case KEY_MUHENKAN: return SDL_SCANCODE_INTERNATIONAL5;
    case KEY_KPENTER: return SDL_SCANCODE_KP_ENTER;
    case KEY_RIGHTCTRL: return SDL_SCANCODE_RCTRL;
    case KEY_KPSLASH: return SDL_SCANCODE_KP_DIVIDE;
    case KEY_SYSRQ: return SDL_SCANCODE_PRINTSCREEN;
    case KEY_RIGHTALT: return SDL_SCANCODE_RALT;
    case KEY_HOME: return SDL_SCANCODE_HOME;
    case KEY_UP: return SDL_SCANCODE_UP;
    case KEY_PAGEUP: return SDL_SCANCODE_PAGEUP;
    case KEY_LEFT: return SDL_SCANCODE_LEFT;
    case KEY_RIGHT: return SDL_SCANCODE_RIGHT;
    case KEY_END: return SDL_SCANCODE_END;
    case KEY_DOWN: return SDL_SCANCODE_DOWN;
    case KEY_PAGEDOWN: return SDL_SCANCODE_PAGEDOWN;
    case KEY_INSERT: return SDL_SCANCODE_INSERT;
    case KEY_DELETE: return SDL_SCANCODE_DELETE;
    case KEY_MUTE: return SDL_SCANCODE_MUTE;
    case KEY_VOLUMEDOWN: return SDL_SCANCODE_VOLUMEDOWN;
    case KEY_VOLUMEUP: return SDL_SCANCODE_VOLUMEUP;
    case KEY_KPEQUAL: return SDL_SCANCODE_KP_EQUALS;
    case KEY_PAUSE: return SDL_SCANCODE_PAUSE;
    case KEY_KPCOMMA: return SDL_SCANCODE_KP_COMMA;
    case KEY_HANGEUL: return SDL_SCANCODE_LANG1;
    case KEY_HANJA: return SDL_SCANCODE_LANG2;
    case KEY_YEN: return SDL_SCANCODE_INTERNATIONAL3;
    case KEY_LEFTMETA: return SDL_SCANCODE_LGUI;
    case KEY_RIGHTMETA: return SDL_SCANCODE_RGUI;
    case KEY_COMPOSE: return SDL_SCANCODE_APPLICATION;
    case KEY_NEXTSONG: return SDL_SCANCODE_AUDIONEXT;
    case KEY_PLAYPAUSE: return SDL_SCANCODE_AUDIOPLAY;
    case KEY_PREVIOUSSONG: return SDL_SCANCODE_AUDIOPREV;
    case KEY_STOPCD: return SDL_SCANCODE_AUDIOSTOP;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

Uint16 modifierOf(int code)
{
    switch (code) {
    case KEY_LEFTSHIFT: return KMOD_LSHIFT;
    case KEY_RIGHTSHIFT: return KMOD_RSHIFT;
    case KEY_LEFTCTRL: return KMOD_LCTRL;
    case KEY_RIGHTCTRL: return KMOD_RCTRL;
    case KEY_LEFTALT: return KMOD_LALT;
    case KEY_RIGHTALT: return KMOD_RALT;
    case KEY_LEFTMETA: return KMOD_LGUI;
    case KEY_RIGHTMETA: return KMOD_RGUI;
    default: return 0;
    }
}

struct Device
{
    std::string path;
    int fd = -1;
    bool grabbed = false;
    bool grabRefused = false;  // another program holds it (EVIOCGRAB: EBUSY): left alone
    bool gone = false;         // unplugged
    Uint32 heldSince = 0;      // waiting for a key to come up before grabbing
    std::bitset<KEY_CNT> down; // keys we've sent down from this device
};

// Thread-side state.
struct Worker
{
    std::vector<std::unique_ptr<Device>> devices;
    std::vector<std::string> ignored; // not keyboards (until /dev/input changes)
    uint32_t windowId = 0;
    Uint16 mods = 0;
    bool quiet = false; // shutting down: the stream has already released every key

    // The key goes to the stream's SDL queue as if SDL had read it from the compositor, so the
    // in-stream shortcuts and the menu see it too. SDL_PushEvent is thread-safe.
    void push(int code, bool down, bool repeat)
    {
        if (quiet) return;
        const SDL_Scancode sc = toSdl(code);
        if (sc == SDL_SCANCODE_UNKNOWN) return;
        SDL_Event e;
        SDL_zero(e);
        e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.timestamp = SDL_GetTicks();
        e.key.windowID = windowId;
        e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.key.repeat = repeat ? 1 : 0;
        e.key.keysym.scancode = sc;
        e.key.keysym.sym = SDL_GetKeyFromScancode(sc); // the local layout, for Ctrl+Alt+Shift+<letter>
        e.key.keysym.mod = mods;
        SDL_PushEvent(&e);
    }

    void key(Device& d, int code, int value)
    {
        if (code < 0 || code >= KEY_CNT) return;
        if (value == 0) {
            if (!d.down[code]) return;
            d.down[code] = false;
            mods &= ~modifierOf(code);
            push(code, false, false);
        }
        else if (value == 1) {
            d.down[code] = true;
            mods |= modifierOf(code); // like SDL: a modifier's own press already carries it
            push(code, true, false);
        }
        else if (value == 2 && d.down[code]) {
            push(code, true, true);
        }
    }

    void releaseKeys(Device& d)
    {
        for (int code = 0; code < KEY_CNT; code++) {
            if (d.down[code]) key(d, code, 0);
        }
    }

    void drain(Device& d)
    {
        input_event evs[64];
        while (read(d.fd, evs, sizeof(evs)) > 0) {}
    }

    void ungrab(Device& d)
    {
        if (d.grabbed) ioctl(d.fd, EVIOCGRAB, 0);
        d.grabbed = false;
        d.heldSince = 0;
        releaseKeys(d);
    }

    void closeAll()
    {
        for (auto& d : devices) {
            ungrab(*d);
            close(d->fd);
        }
        devices.clear();
        mods = 0;
    }

    bool anyGrabbed() const
    {
        for (const auto& d : devices) {
            if (d->grabbed) return true;
        }
        return false;
    }

    bool anyWaiting() const
    {
        for (const auto& d : devices) {
            if (!d->grabbed && !d->grabRefused) return true;
        }
        return false;
    }

    // Opens keyboards that aren't open yet. Returns false if an event device couldn't be opened
    // for lack of permission.
    bool scan()
    {
        bool allowed = true;
        DIR* dir = opendir("/dev/input");
        if (!dir) return true;
        while (dirent* e = readdir(dir)) {
            if (strncmp(e->d_name, "event", 5) != 0) continue;
            const std::string path = std::string("/dev/input/") + e->d_name;
            bool known = false;
            for (const auto& d : devices) known = known || d->path == path;
            for (const auto& p : ignored) known = known || p == path;
            if (known) continue;
            const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) {
                if (errno == EACCES || errno == EPERM) allowed = false;
                continue;
            }
            if (!isKeyboard(fd)) {
                close(fd);
                ignored.push_back(path);
                continue;
            }
            auto d = std::make_unique<Device>();
            d->path = path;
            d->fd = fd;
            devices.push_back(std::move(d));
        }
        closedir(dir);
        return allowed;
    }

    // Grabs every open keyboard that has no key held: the compositor must see the release of a
    // key it saw pressed, or it would think the key is stuck after we give the keyboard back.
    void grabIdle(Uint32 now)
    {
        for (auto& d : devices) {
            if (d->grabbed || d->grabRefused) continue;
            if (anyKeyHeld(d->fd)) {
                if (!d->heldSince) d->heldSince = now ? now : 1;
                if (now - d->heldSince < kHeldGraceMs) continue;
            }
            if (ioctl(d->fd, EVIOCGRAB, 1) == 0) {
                d->grabbed = true;
                drain(*d); // keys read before the grab went to the compositor already
                char name[128] = "?";
                ioctl(d->fd, EVIOCGNAME(sizeof(name)), name);
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "System keys: grabbed %s (%s)", d->path.c_str(), name);
            }
            else {
                d->grabRefused = true;
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "System keys: can't grab %s: %s", d->path.c_str(), strerror(errno));
            }
        }
    }

    // True if one of our keys has been waiting in SDL's queue for too long: the stream's loop
    // isn't running, and a grab now would leave this computer without a keyboard.
    static bool streamStalled(Uint32 now)
    {
        SDL_Event e;
        return SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_KEYDOWN, SDL_KEYUP) == 1 &&
               now - e.key.timestamp > kStallMs;
    }
};

#ifdef HAS_WAYLAND
bool waylandHasShortcutsInhibit()
{
    // Ask the compositor on a connection of our own; SDL's stays untouched.
    wl_display* d = wl_display_connect(nullptr);
    if (!d) return false;
    bool found = false;
    static const wl_registry_listener listener = {
        [](void* data, wl_registry*, uint32_t, const char* iface, uint32_t) {
            if (strcmp(iface, "zwp_keyboard_shortcuts_inhibit_manager_v1") == 0) *static_cast<bool*>(data) = true;
        },
        [](void*, wl_registry*, uint32_t) {},
    };
    wl_registry* reg = wl_display_get_registry(d);
    wl_registry_add_listener(reg, &listener, &found);
    wl_display_roundtrip(d);
    wl_registry_destroy(reg);
    wl_display_disconnect(d);
    return found;
}
#endif

} // namespace

bool EvdevKeyboardGrab::needed()
{
    static int s_Needed = -1;
    if (s_Needed >= 0) return s_Needed == 1;

    const char* force = SDL_getenv("CLOUDDECK_EVDEV_KEYS");
    if (force && *force) {
        s_Needed = force[0] == '1' ? 1 : 0;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "System keys: evdev grab %s (CLOUDDECK_EVDEV_KEYS)",
                    s_Needed ? "on" : "off");
        return s_Needed == 1;
    }
    const char* driver = SDL_GetCurrentVideoDriver();
    if (!driver) return false; // ask again once SDL's video is up
    s_Needed = 0;
#ifdef HAS_WAYLAND
    if (strcmp(driver, "wayland") == 0 && !waylandHasShortcutsInhibit()) {
        s_Needed = 1;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "System keys: the compositor has no keyboard-shortcuts-inhibit; grabbing the keyboards' event devices instead");
    }
#endif
    return s_Needed == 1;
}

void EvdevKeyboardGrab::wake()
{
    const uint64_t one = 1;
    if (m_WakeFd >= 0 && write(m_WakeFd, &one, sizeof(one)) < 0) {
        // Non-blocking eventfd: only fails when the counter is already huge, i.e. awake anyway.
    }
}

void EvdevKeyboardGrab::setActive(SDL_Window* window, bool active)
{
    if (active && !m_Thread.joinable()) {
        m_WakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (m_WakeFd < 0) return;
        m_Quit = false;
        m_Thread = std::thread(&EvdevKeyboardGrab::threadMain, this);
    }
    if (window) m_WindowId = SDL_GetWindowID(window);
    if (m_Want.exchange(active) != active) wake();
}

void EvdevKeyboardGrab::stop()
{
    if (!m_Thread.joinable()) return;
    m_Want = false;
    m_Quit = true;
    wake();
    m_Thread.join();
    close(m_WakeFd);
    m_WakeFd = -1;
}

void EvdevKeyboardGrab::threadMain()
{
    Worker w;
    // New keyboards while grabbing. IN_ATTRIB: udev sets the node's permissions after creating it.
    const int inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotifyFd >= 0) inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE | IN_ATTRIB);

    bool opened = false;    // keyboards opened for this activation
    bool rescan = false;
    bool stalled = false;   // let go because the stream loop stopped taking keys
    bool deniedShown = false;
    std::vector<pollfd> fds;

    while (!m_Quit) {
        const Uint32 now = SDL_GetTicks();
        const bool want = m_Want;

        if (stalled && !Worker::streamStalled(now)) {
            stalled = false;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "System keys: the stream takes keys again");
        }
        if (want && !stalled) {
            if (!opened) {
                opened = true;
                rescan = true;
                w.windowId = m_WindowId;
            }
            if (rescan) {
                rescan = false;
                const bool allowed = w.scan();
                if (!allowed && w.devices.empty() && !deniedShown) {
                    deniedShown = true;
                    m_AccessDenied = true;
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "System keys: no access to /dev/input (add the user to the \"input\" group); the compositor keeps Win and Alt+Tab");
                }
            }
            w.grabIdle(now);
            if (w.anyGrabbed() && Worker::streamStalled(now)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "System keys: the stream stopped taking keys; giving the keyboard back");
                w.closeAll();
                w.ignored.clear();
                opened = false;
                stalled = true;
            }
        }
        else if (opened) {
            w.closeAll();
            w.ignored.clear();
            opened = false;
        }

        fds.clear();
        fds.push_back({m_WakeFd, POLLIN, 0});
        fds.push_back({inotifyFd, POLLIN, 0});
        for (const auto& d : w.devices) fds.push_back({d->fd, POLLIN, 0});
        int timeout = -1;
        if (stalled) timeout = 500;
        else if (opened) timeout = w.anyWaiting() ? 50 : 1000; // waiting for keys to come up / stall check
        if (poll(fds.data(), fds.size(), timeout) < 0 && errno != EINTR) break;

        if (fds[0].revents & POLLIN) {
            uint64_t n;
            while (read(m_WakeFd, &n, sizeof(n)) > 0) {}
        }
        if (inotifyFd >= 0 && (fds[1].revents & POLLIN)) {
            char buf[4096];
            while (read(inotifyFd, buf, sizeof(buf)) > 0) {}
            w.ignored.clear();
            rescan = true;
        }
        for (size_t i = 2; i < fds.size(); i++) {
            if (!fds[i].revents) continue;
            Device& d = *w.devices[i - 2];
            bool gone = (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
            input_event evs[64];
            for (;;) {
                const ssize_t n = read(d.fd, evs, sizeof(evs));
                if (n < 0) {
                    if (errno != EAGAIN && errno != EINTR) gone = true; // ENODEV: unplugged
                    break;
                }
                if (n == 0) break;
                if (!d.grabbed) continue; // not ours (yet): the compositor has these keys
                for (ssize_t k = 0; k < n / ssize_t(sizeof(input_event)); k++) {
                    if (evs[k].type == EV_KEY) w.key(d, evs[k].code, evs[k].value);
                }
            }
            d.gone = gone;
        }
        for (size_t i = 0; i < w.devices.size();) {
            Device& d = *w.devices[i];
            if (d.gone) {
                d.grabbed = false;
                w.releaseKeys(d);
                close(d.fd);
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "System keys: %s went away", d.path.c_str());
                w.devices.erase(w.devices.begin() + i);
            }
            else {
                i++;
            }
        }
    }

    w.quiet = true; // the stream has released its keys itself by now
    w.closeAll();
    if (inotifyFd >= 0) close(inotifyFd);
}

#endif
