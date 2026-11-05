# ADR-0009: In-stream menu (Phase 5, desktop)

- Status: Accepted. Revision 2: a GUI menu replaced the text overlay menu
  after the owner's feedback.
- Date: 2026-11-05

## Context
- The brief asks for a Shadow-style menu during a stream.
- **Revision 1** drew a text list with Moonlight's status overlay. The owner
  found it "looks like an old monitor OSD", that it couldn't be opened with
  the mouse, and that it couldn't change USB devices or audio settings. Its
  "let the mouse leave" item also didn't work: Moonlight's pointer region lock
  only applies in remote-desktop (absolute) mouse mode, and the default game
  mode captures the mouse anyway.
- While streaming, Moonlight hands the main thread to an SDL loop and Qt is
  paused, so no QML runs unless something processes Qt events.

## Decision
1. **Two Qt Quick windows owned by the stream's SDL window:**
   - Each sits on a foreign `QWindow` wrapping the SDL HWND/X11 window, set as
     the transient parent. So they stay above the stream but not above other
     apps, and they minimize with it.
   - They share the app's QML engine, so the QML singletons (CloudDeckMic,
     CloudDeckUsb, …) are available.
   - **Button:** a 40 px circle at the top centre with the CloudDeck cloud.
     The window is masked to a circle, partly translucent through window
     opacity, and doesn't take focus.
   - **Menu:** a dark panel that drops down from the button and closes when it
     loses focus (a click back into the stream), with Esc, or with the hotkey.
   - Neither window needs a transparent swap chain, which Qt's D3D11 backend
     doesn't reliably provide.
2. **Processing Qt events:** while the overlay exists, the SDL loop calls
   `StreamOverlay::pump()`:
   - `processEvents` for at most 2 ms every 33 ms (6 ms every 8 ms while the
     menu is open);
   - the SDL wait timeout shortens to match.

   Pumping only starts after the stream window exists. Moonlight's own windows
   are hidden by then and its gamepad navigation is disabled.
3. **Opening:**
   - Click the button. It's visible only while the cursor is free: desktop
     mouse mode, or capture released with Ctrl+Alt+Shift+Z.
   - Or press **Ctrl+Alt+Shift+O**, which works in any mode.

   Opening releases the mouse capture and the hotkey's modifiers. Closing
   refocuses the stream and recaptures.
4. **Modality:** while the menu is open, the SDL loop drops key, click, wheel
   and gamepad-press events. Gamepad button releases still reach the VM, so no
   button stays stuck.
5. **Contents:**
   - **Display:** fullscreen; fit the CloudDeck to this window (ADR-0007;
     turning it on fits immediately); performance stats.
   - **Mouse:** *Game* (captured) or *Desktop* (free cursor, absolute mode).
     It's saved as the "remote desktop mouse" preference. This replaces the
     broken region-lock item.
   - **Audio:**
     - mute the CloudDeck's sound (a separate flag from mute-on-focus-loss);
     - send my microphone (negotiated at connection start, so toggling it
       reconnects in place);
     - mute the mic;
     - choose the microphone (switches live).
   - **USB devices:** ticking or unticking attaches or detaches **during the
     stream**. UsbForwarder now remembers the running stream. The worker gains
     `stopOne()`, which on Linux sends `release <busid>` to the helper.
   - **Session:**
     - reconnect (in place);
     - disconnect;
     - shut down my CloudDeck (click twice; only for streams started from the
       CloudDeck screen).
   - Footer: show or hide the button.

## Consequences
- Wayland-native SDL has no window to attach to. There, the overlay is
  disabled and the hotkey does nothing; the AppImage runs on XWayland.
- Qt event processing during a stream costs a few hundred µs every 33 ms.
  Watch for frame-pacing effects at very high frame rates.
- In game mouse mode the button is hidden (the cursor can't reach it). Use the
  hotkey.

## Android (2026-11-05)
- **Opening:** a 40 dp round button at the top centre with the CloudDeck mark,
  or the **Back** gesture/button. Back used to end the stream; *Disconnect* is
  now in the menu. Pressing Back again closes the menu.
- **Contents:**
  - mute my microphone (when the mic is streaming);
  - performance stats (live: the decoder reads the flag every frame);
  - hide the menu button (Back still opens the menu);
  - keyboard (the system keyboard, as with the 3-finger tap);
  - disconnect;
  - shut down my CloudDeck (tap twice; only for streams started from the
    CloudDeck screen).
- The menu is a dark dialog built in code (`clouddeck/StreamMenu.java`) with
  the CloudDeck header. The button hides in picture-in-picture.
- Display, mouse and USB settings aren't on Android: the stream size is the
  phone's screen and there's no USB forwarding.
