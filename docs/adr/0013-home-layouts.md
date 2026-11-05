# ADR-0013: Two home layouts, Console and Launcher (Phase 7)

- Status: Accepted (implemented; not yet verified by the owner)
- Date: 2026-11-05

## Context
- Phase 7 asks for a less cluttered, more modern client. Three directions were
  mocked up (private canvas): A "Console", B "Dashboard", C "Launcher".
- The owner liked A and C equally and asked for both, with a setting to switch
  between them.
- The interface has to work with touch and with controllers on phones,
  handhelds and TVs, not only with a mouse.

## Decision
- **One setting, two layouts, shared pages.** `uiStyle` is `console` (the
  default) or `launcher`. Desktop stores it in QSettings as
  `clouddeck/uiStyle`; Android stores it in the `clouddeck` shared preferences.
  Both layouts are built from the same settings sections, so a setting exists
  once and looks the same in both. The switch is in *General* (desktop) or
  *Account* (Android) → *Appearance → Home screen layout*. After a switch, focus
  lands back on the switch in the other layout.
- **Console (A):** a sidebar with pages and a Home page with a round Connect
  button, the stream summary, quick toggles and tiles (Restart/Start, Stop,
  Test connection or Microphone/Stream settings). Below 720 px (desktop) or
  720 dp (Android) the sidebar becomes a header with a strip of page chips.
  - Desktop pages: Home, Display, Audio & mic, Controllers, USB devices,
    Network, General.
  - Android pages: Home, Stream, Devices, Account. Android has no clipboard,
    USB, network test or multi-monitor.
- **Launcher (C):** the CloudDeck mark, a large "Start and connect" pill, quick
  chips, and links for Restart, Stop and Test connection or Manage PCs. All
  settings are in a panel from the right with Stream, Devices and Account tabs.
  On phones the panel fills the screen.
- **Moonlight's toolbar is hidden on the desktop home screen.** Its settings
  (resolution, codec and so on) are behind *All streaming settings…*. *Manage
  PCs* is in General or Account.

## Input and accessibility
- Touch targets are at least 48 px or dp, and the whole of a switch row toggles.
- **Focus is always visible** for keys: a 3 px ring (#9db4ff) on every control.
  - Desktop shows it only for Tab or D-pad focus (`visualFocus`), never after a
    click or tap.
  - Android uses the `state_focused` background; touch mode hides it on its own.
- **Desktop navigation:**

  | Input | Action |
  |---|---|
  | D-pad up/down, arrow keys, Tab | Step through controls. The home turns on `SdlGamepadKeyNavigation` Tab mode, like SettingsView. |
  | A, Space, Return | Activate. |
  | Left / Right | Console: back to the sidebar / into the page. Launcher: step along a row. |
  | B / Escape | Sidebar, then Home, then Moonlight's quit prompt. In the launcher it closes the panel. |
  | X / Menu / Start | Open the settings. |

  Focus scrolls into view (`CdFocus.js`). Connect gets the first focus, with
  the ring shown when a gamepad is connected or there's no desktop
  environment (handheld or TV sessions).
- **Android navigation:**

  | Input | Action |
  |---|---|
  | D-pad, gamepad A/B, TV remote | Native focus search. Connect gets the first focus. |
  | Back / B | Closes the panel, then returns to Home (`OnBackInvokedCallback` on API 33+). |
  | Menu / Start | Opens the settings. |
  | Rotation, resizing | Picks the phone or wide layout again. |

## Consequences
- Desktop QML is in `client-desktop/app/gui/Cd*.qml`, with `CloudDeckView.qml`
  as the host.
  - `tests/qml/tst_home.qml` loads both layouts against stub singletons.
  - It drives them with keys only and saves screenshots (`CDC_SHOTS=dir`).
- Android:
  - Activity: `CloudDeckActivity`.
  - Layouts: `cd_home_console.xml` (plus `layout-w720dp`),
    `cd_home_launcher.xml`, `cd_section_*.xml` and `clouddeck_styles.xml`.
- New settings sit next to the old ones: the on-screen controller on/off and
  its style on desktop (`clouddeck/osc/*`), plus Nintendo layout and on-screen
  controller on Android (Moonlight's own keys).
- The remaining Moonlight screens are not restyled yet: PC list, app grid and
  streaming settings.
