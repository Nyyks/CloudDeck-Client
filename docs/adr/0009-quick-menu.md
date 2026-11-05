# ADR-0009: In-stream Quick Menu (Phase 5, desktop)

- Status: Accepted (implemented; not yet verified)
- Date: 2026-11-05

## Context
- The brief asks for a Shadow-style menu during a stream, opened by a hotkey
  on desktop.
- While streaming, Moonlight hands the main thread to an SDL event loop and
  Qt is paused, so no QML can be shown over the stream.
- Every renderer (D3D11, DXVA2, VAAPI/EGL, Vulkan, SDL) already composites
  Moonlight's two text overlays: performance stats and a status line.

## Decision
- **Rendering:** the menu is text in the existing **status overlay**, drawn
  white at 26 pt while the menu is open. A small
  `OverlayManager::setOverlayStyle()` addition makes that possible. On close,
  the original style and content (the gamepad mouse-mode notice) come back.
  No renderer changes.
- **Opening:** **Ctrl+Alt+Shift+O**, consistent with Moonlight's
  Ctrl+Alt+Shift shortcuts. The modifiers that already reached the VM are
  released.
- **Navigation:**
  - Keyboard: Up/Down (or Tab), Enter/Space to select, 1–9 for direct
    choice, Esc or the hotkey to close.
  - Gamepad: D-pad, A to select, B to close.

  There's no gamepad combo to *open* it, to avoid clashing with Moonlight's
  Start+Select+L1+R1 quit combo.
- **Modality:** while the menu is open, key presses and mouse clicks go to the
  menu. Gamepad button *releases* still reach the VM, so no button stays stuck.
  Mouse motion passes through.
- **Items** reuse the existing actions (the key-combo handlers, MicStreamer,
  the fit-to-window restart):
  - fullscreen ↔ window (hidden with multiple monitors);
  - mute/unmute mic (when the mic is enabled);
  - show/hide performance stats;
  - keep the mouse inside the window;
  - **reconnect the stream** in place (the ADR-0007 restart at the same size);
  - disconnect;
  - **shut down my CloudDeck**, which needs a second Enter. It's shown only
    for streams started from the CloudDeck screen: it disconnects, and
    CloudSession stops the VM when it sees the stream end.
- **Not in the menu (yet):** USB device selection and the monitor layout.
  They need the full UI and stay on the CloudDeck screen. Android gets an
  on-screen button later.

## Consequences
- The status overlay is shared, so a gamepad mouse-mode notice is hidden while
  the menu is open and comes back when it closes.
- The overlay font (ModeSeven) is ASCII-only, so the menu text is ASCII.
