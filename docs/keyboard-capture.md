# System shortcuts (keyboard capture)

Desktop clients, 1.2.0 and later. Controls → Keyboard & mouse → *Send Win, Alt+Tab and other
system shortcuts to the CloudDeck*:

| Mode | Win, Alt+Tab, Alt+F4, Ctrl+Esc… go to |
|---|---|
| **Never** (default) | this computer |
| **While the stream window is active** | the CloudDeck whenever the stream window has the keyboard, also with the mouse released and in windowed mode. Click another window to use them locally again. |
| **Always while streaming** | the CloudDeck for as long as a stream runs. On Windows this includes when another window on this PC is active (see below). On Linux it works like *While the stream window is active*. |

**During a stream:**
- **Ctrl+Alt+Shift+K**, or the **System keys** tile in the CloudDeck menu, switches capture off
  and back on in the chosen mode, for this stream only.
- The menu's *Display, mouse and keyboard* card has the same three modes. A change there applies
  at once and is saved.
- To leave the stream window while capture is on, use the mouse, the CloudDeck menu or
  **Ctrl+Alt+Shift+D** (minimize).

**Never captured:** Ctrl+Alt+Del and Win+L on Windows. No app can take them.

**Raspberry Pi OS (labwc) and other Wayland desktops without shortcut inhibiting:** the stream
reads the keyboard directly while it has the keyboard (see below). That needs the user in the
`input` group, which the Raspberry Pi OS default user is. If it isn't, the stream says so once
and the desktop keeps Win and Alt+Tab:
`sudo usermod -aG input $USER`, then sign out and back in. While capture is on there, *every* key
goes to the CloudDeck, volume and media keys included.

**Android and ChromeOS:** see [the Android section](#android-and-chromeos).

## How it works

- **The stream window:** SDL's keyboard grab (`SDL_SetWindowKeyboardGrab`). SDL only grabs while
  the window has keyboard focus, so focus decides.
  - Win32 uses a low-level hook.
  - X11 uses a keyboard grab.
  - Wayland uses the keyboard-shortcuts-inhibit protocol, and the compositor may ask once.
- **Wayland without keyboard-shortcuts-inhibit** (labwc, including Raspberry Pi OS's 0.20, which
  doesn't offer the protocol): SDL's grab does nothing there, so `clouddeck/evdevkeyboardgrab.cpp`
  takes the keyboards one level lower while the stream window has the keyboard.
  - It asks the compositor for the protocol at the first grab, on a Wayland connection of its own.
    `CLOUDDECK_EVDEV_KEYS=1` or `=0` forces the evdev grab on or off.
  - It grabs every keyboard's `/dev/input/event*` node (`EVIOCGRAB`), on its own thread, so the
    compositor sees no keys at all. It sends them to the stream as SDL key events, so the
    in-stream shortcuts and the menu keep working.
  - Devices that also move a pointer stay with the compositor, because grabbing them would take
    the pointer too. So do devices without letter keys, such as media-key nodes and power buttons.
  - A grab only starts once no key is held, so the compositor never misses a release. If a key
    stays held for 3 s, the grab starts anyway.
  - It lets go when the window loses focus, when the in-stream menu opens (the menu takes the
    keyboard), when capture is switched off and when the stream ends. Keys still held then are
    released on the CloudDeck.
  - Safety net: if the stream's loop stops taking keys for 2 s, the keyboard goes back to the
    desktop until the loop runs again.
  - Hot-plugged keyboards are picked up through inotify on `/dev/input`.
  - Tested under the Raspberry Pi archive's labwc 0.20 with Raspberry Pi OS's `rc.xml`, using a
    uinput keyboard read through libinput. Without the grab, labwc took Alt+Tab and switched
    windows. With it, Alt+Tab, Super and Super+E reached the stream window and labwc saw none of them.
- **Upstream Moonlight's rule:** it also requires a captured mouse, and in its "fullscreen" mode a
  fullscreen window. The CloudDeck modes drop both conditions. The old mode is still available
  as `--capture-system-keys fullscreen` on the command line, and saved settings that used it now
  read as *While the stream window is active*.
- **Always, on Windows:** `clouddeck/systemkeyforwarder.cpp` installs a `WH_KEYBOARD_LL` hook on
  its own thread for the length of the stream.
  - While another window is in front, the hook takes Win and Win+key, Alt+Tab, Alt+Shift+Tab,
    Alt+Esc and Ctrl+Esc away from Windows.
  - It sends them to the CloudDeck with the modifiers they need. Modifiers it pressed on the
    CloudDeck are released when you let go of them.
  - Its own thread means a busy stream thread never slows down typing elsewhere.
  - A swallowed Alt+Tab replays Alt's release behind an unassigned key (VK 0xE8), so the local
    app doesn't open its menu bar.
  - Alt+F4 stays local, so it still closes the window you're in.
- **Linux has no "beyond the window" mode:**
  - X11 window managers already own Win and Alt+Tab through passive grabs.
  - Wayland doesn't let clients take keys from other windows at all.

## Android and ChromeOS

Android has no setting for this. While the stream holds the mouse, it asks the system for the keys:
- Android 16 QPR1 and later use `setKeyboardCaptureEnabled`.
- Some Samsung devices use `SemWindowManager.requestMetaKeyEvent`.

**Chromebooks** run an older Android, and ChromeOS decides itself. Its accelerator handler
(`ash/accelerators/pre_target_accelerator_handler.cc`) works like this:
- **Alt+Tab** and the other "preferred" shortcuts go to an app first only while the app's window
  is **fullscreen**. A window becomes fullscreen through the full-screen key, or when a
  maximized app hides the system bars.
- **Search (Win) and Search+key** always stay with ChromeOS. The only exception is an enterprise
  policy (`SystemShortcutBehavior`).
- Power, lock and other reserved keys always stay with ChromeOS.

**Android keeps Alt+Tab itself** (Android 13 to 16, before QPR1; owner's test on a Chromebook,
2026-11-18). Android's own `PhoneWindowManager.interceptKeyBeforeDispatching` does this:
- The first Tab pressed with Alt alone is consumed and opens the system's recents. On a
  Chromebook that's ChromeOS's window switcher.
- Further Tabs while Alt stays held pass through to the app, so a fast second Tab "worked".
- The Alt release is consumed as well.

No app can get around this before Android 16 QPR1. Accessibility key filters
(`KeyboardInterceptor`) ask the same policy first.

**The stand-in shortcut** (`clouddeck/AltTabSubstitute.java`): Settings → Devices → Keyboard →
*Alt+Tab on the CloudDeck*, with **Ctrl+Tab** (default), **Alt+`** or **Off**. The card is hidden
on Android 16 QPR1 and later, where the stream takes Alt+Tab itself.
- **Ctrl+Tab:** the first Tab with Ctrl held, and no Alt or Meta, releases Ctrl on the CloudDeck
  and holds Alt there instead, until Ctrl comes up.
  - Every Tab meanwhile is sent as a Tab, and Shift goes backwards. So holding Ctrl and tapping
    Tab moves through the CloudDeck's windows like Alt+Tab.
  - The CloudDeck then never gets Ctrl+Tab itself, for example to switch browser tabs.
  - Neither Android nor ChromeOS uses Ctrl+Tab, so it also works in a window.
- **Alt+`:** with Alt held, ` is sent as Tab. Alt goes to the CloudDeck as usual.
- On focus loss the stream releases what the stand-in holds. It also releases Shift, Ctrl, Alt
  and Win on the CloudDeck if any was down. After an Alt+Tab that Android kept, Alt used to stay
  held there.
- On ChromeOS the stream names the shortcut once, in a toast when the first stream starts.
- Unit-tested in `AltTabSubstituteTest`.

What the app does on ChromeOS:
- Every ChromeOS app window counts as multi-window. Moonlight used to drop its immersive flags
  in multi-window mode, so a maximized stream window never became fullscreen. On ChromeOS it now
  asks to hide the system bars in every window except picture-in-picture. It doesn't use the
  layout flags, so a windowed stream stays clear of the caption bar.
- It asks for fullscreen when the stream starts, through `requestFullscreenMode` on Android 14
  and later. Fullscreen still helps the other shortcuts ChromeOS would otherwise take.

The owner's test (2026-11-18): a single Alt+Tab still went to ChromeOS, and a second Tab with Alt
held reached the CloudDeck, as the Android source predicts. With the stand-in shortcut (v1.2.2),
Ctrl+Tab switches windows on the CloudDeck: verified by the owner on the same Chromebook.
