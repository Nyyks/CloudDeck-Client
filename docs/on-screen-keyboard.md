# On-screen keyboard (Phase 6)

A full keyboard for phones, tablets and handhelds, on Android and on the
desktop client (for Windows or Linux handhelds with a touchscreen).

Unlike the system keyboard, it sends **real key presses** (Windows virtual-key
codes, the same path as a physical keyboard). So Esc, F1–F12, arrows, Tab,
Page Up/Down and shortcuts like Ctrl+C or Alt+Tab work in games and on the
desktop.

## Layout
US positions, six rows:
- Esc, F1–F12, Del
- the number row with Backspace
- Q row
- A row with Enter
- Shift, the Z row, ↑, PgUp
- Ctrl, Win, Alt, Space, ← ↓ →, PgDn, ✕ (close)

Held keys repeat after 450 ms, then every 50 ms. The VM's keyboard layout
decides which character a key types; the labels are US.

## Modifiers
- **Tap** Ctrl, Alt, Shift or Win to hold it for the next key. That covers
  Ctrl+C, Alt+Tab and Shift+letter.
- **Double-tap** to lock it until you tap it again.
- Modifiers are sent as real key presses, so the CloudDeck sees them as held.
  Closing the keyboard releases them.

## Opening it
- **In-stream menu:** *Full keyboard* on Android, *On-screen keyboard* on
  desktop. The system keyboard ("Keyboard (typing)") is still there on Android
  for plain text.
- **Gamepad:** **Select + L1 + R1 + Y**. **Select + L1 + R1 + B** opens the
  CloudDeck menu. These sit next to Moonlight's Select+L1+R1+X (stats) and
  Start+Select+L1+R1 (quit).
- **Android:** Back closes the keyboard first.

## Implementation
- Android: `clouddeck/StreamKeyboard.java`, an overlay view at the bottom
  46 % of the screen, sending through `NvConnection.sendKeyboardInput`.
- Desktop: `gui/StreamKeyboard.qml` in a non-focusable window owned by the
  stream window (ADR-0009), sending through `LiSendKeyboardEvent2`. The
  stream keeps keyboard focus, and touch or clicks reach the keyboard.
