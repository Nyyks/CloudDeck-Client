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

## Amendment: one settings menu (2026-11-05)
The owner asked for Moonlight's own settings to live in the CloudDeck settings, so
there is only one settings menu.
- **Desktop:** every option of Moonlight's SettingsView is now on a CloudDeck page,
  in the same style and with the same navigation:
  - Display: resolution, frame rate (with custom values), bitrate, display mode,
    V-Sync, frame pacing, HDR.
  - Audio & mic: speaker layout, muting the CloudDeck's own speakers, muting when
    the window isn't active.
  - Controllers: keep controller 1 connected, background input, controller as mouse.
  - Mouse & keyboard (new page): desktop mouse, button swap, scroll direction,
    touch as trackpad, system shortcuts, and the stream shortcuts (moved from General).
  - General: language, app window mode, quitting the app after a stream, game
    optimizations, warnings, Discord, keep awake, performance stats, decoder,
    codec, renderer, YUV 4:4:4, unlocked bitrate, mDNS and blocked-connection detection.
  - Moonlight's toolbar gear on the other pages (PC list, app grid) now returns
    to the CloudDeck screen and opens its settings. `SettingsView.qml` is only
    used when there is no CloudDeck screen (CLI modes).
- **Android:** Moonlight's preference list (`StreamSettings.SettingsFragment`) is
  embedded as the **Stream** page or tab, so it keeps every device-specific option
  and its checks. Options the CloudDeck pages already show (Nintendo layout,
  on-screen controller, hold Start for mouse) are hidden there to avoid two
  switches for one setting. Moonlight's PC list still opens its own settings activity.

## Amendment 2: one layout (2026-11-05)
After testing, the owner preferred the Console layout and asked to remove the Launcher.
The Launcher is gone on both platforms (`CdHomeLauncher.qml`, `cd_home_launcher.xml`),
along with the *Home screen layout* switch and the `uiStyle` setting. A stored "launcher"
value is ignored. The touch, keyboard, gamepad and TV-remote rules above still apply.
