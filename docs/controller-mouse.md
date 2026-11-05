# Controller as mouse

A controller can drive the CloudDeck's mouse pointer, for example to use the
desktop or a launcher from the sofa. It builds on Moonlight's mouse emulation and
works on desktop and Android.

## Switching

| How | Where |
|---|---|
| **Select+L1+R1+A** on the controller | Both. Switches that controller. Always available. |
| Hold **Start** for about a second | Both. Moonlight's original gesture; can be turned off. |
| **Controller** tile in the in-stream menu | Both. Switches every connected controller. |
| **Start streams with controllers as a mouse** | Settings → Controllers (desktop) or Devices (Android). Each controller starts as a mouse when it connects. |

While a controller is a mouse, desktop shows "Controller mouse mode" in the
stream's status area; Android shows a short message when it switches.

## Buttons in mouse mode

| Input | Mouse |
|---|---|
| Left or right stick | Move the pointer (desktop: the stick pushed further wins) |
| A | Left click (hold to drag) |
| B | Right click |
| X (desktop) | Middle click |
| L1 / R1 (desktop) | Back / forward |
| D-pad | Scroll |

The other CloudDeck combos keep working in mouse mode: Select+L1+R1+B opens the
menu and Select+L1+R1+Y the keyboard.

## Settings

| Setting | Desktop | Android |
|---|---|---|
| Start as a mouse | `clouddeck/gamepadMouseAtStart` (QSettings) | `cdGamepadMouseAtStart` (default shared preferences) |
| Hold Start | Moonlight's `gamepadMouse` | Moonlight's `checkbox_mouse_emulation` |

## Implementation
- **Desktop:** `SdlInputHandler::setGamepadMouseMode` in `streaming/input/gamepad.cpp`
  is the single on/off path. It releases any mouse buttons still held when the
  mode switches off.
- **Android:** `GenericControllerContext.setMouseEmulation` in `ControllerHandler.java`
  does the same.
