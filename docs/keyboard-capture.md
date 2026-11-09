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

## How it works

- **The stream window:** SDL's keyboard grab (`SDL_SetWindowKeyboardGrab`). SDL only grabs while
  the window has keyboard focus, so focus decides.
  - Win32 uses a low-level hook.
  - X11 uses a keyboard grab.
  - Wayland uses the keyboard-shortcuts-inhibit protocol, and the compositor may ask once.
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
