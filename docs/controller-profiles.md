# Controller remapping profiles (Phase 6)

One profile format, used the same way by the desktop and Android clients, so a
profile means the same thing everywhere.

## Format (version 1)
```json
{
  "version": 1,
  "name": "Racing",
  "buttons": { "a": "b", "b": "a", "back": "none", "paddle1": "leftShoulder" },
  "leftStick":  { "deadzone": 0.1, "sensitivity": 1.0, "invertY": false },
  "rightStick": { "deadzone": 0.0, "sensitivity": 2.0, "invertY": true },
  "triggerDeadzone": 0.1,
  "swapSticks": false
}
```

**Buttons** use these names: `a` `b` `x` `y` `back` `guide` `start`
`leftStick` `rightStick` `leftShoulder` `rightShoulder` `dpadUp` `dpadDown`
`dpadLeft` `dpadRight` `misc` `paddle1`–`paddle4` `touchpad`. They follow the
Xbox positions: `a` is the bottom face button.
- A pressed source button sends its target button.
- `none` disables a button.
- Buttons that aren't listed stay unchanged.
- Unknown names are ignored.

**Sticks** use Moonlight's convention (−32767…32767, +Y = up) and are processed
in this order:
1. **Deadzone (radial).** Below it the stick reads 0. Above it, the range is
   stretched back to 0…1.
2. **Sensitivity** (0.5–2.0) is a response curve, `out = in^(1/sensitivity)`.
   Above 1, full deflection comes sooner.
3. **Invert Y.**
4. **Swap sticks**, after both sticks are shaped.

**Triggers** (0…255) use the same deadzone-and-stretch rule.

Values are clamped: deadzones to 0–0.5, sensitivity to 0.5–2.0.

## Where profiles apply
- **Desktop:** the button translation in `gamepad.cpp`, after the Xbox/Nintendo
  face-button swap. Sticks and triggers are shaped when the state is sent,
  after Moonlight's single-controller merge.
- **Android:** the same points in `ControllerHandler`.
- **Choice:** the profile is chosen when a controller connects:
  - its own assignment (by the name the OS reports);
  - otherwise the default profile;
  - otherwise no remapping.
- **Storage:** desktop settings `clouddeck/controller/{profiles,defaultProfile,assignments}`;
  Android shared preferences with the same keys.

## Tests
`shared/fixtures/controller/profile-v1.json` with the expected results in
`cases-v1.json`. Those were computed by an independent implementation, and both
clients' unit tests must reproduce them exactly.

## On-screen controller (Android)
Moonlight's on-screen controller already has a layout editor: tap the gear to
move buttons, tap again to resize, then again to finish. CloudDeck adds, in the
in-stream menu (`clouddeck/OnScreenController.java`):
- **Button style: Xbox or PlayStation.**
  - Xbox: A/B/X/Y in green/red/blue/yellow, LB/RB, LT/RT, View/Menu/Guide.
  - PlayStation: ✕ ○ □ △ in their colours, L1/R1, L2/R2, Share/Options/PS.

  Labels follow the button a key *sends*, so they stay correct with the
  Nintendo layout switch, which also rebuilds the layout.
- **Layout profiles:** *Save layout as…*, *Load a saved layout…* and *Delete…*.
  A profile is a copy of Moonlight's `OSC` preferences (positions, sizes,
  colours) under `OSC_profile_<name>`.
- *Edit on-screen controller layout* starts Moonlight's editor.
