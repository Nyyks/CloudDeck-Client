# Feature parity vs the Shadow PC client

Legend:
- **done**: works and was tested on the real VM
- **partial**: implemented, but not yet verified end-to-end on the VM
- **blocked**
- **not started**
- **not planned**

Nothing counts as done until it's tested against the real VM.

Last updated: 2026-11-05 (v1.0.0). Phase 1 verified by the owner on real devices with
v0.1.0-alpha.1. The Linux AppImage was built but its end-to-end run wasn't
explicitly confirmed, so Linux stays *partial* where it differs.

| Area | Feature | Win | Linux | Android | Notes |
|---|---|---|---|---|---|
| Release | CI builds (MSI + portable zip, AppImage, APK); tagged releases | done | partial | done | |
| Release | Side-by-side with stock Moonlight | done | partial | done | ADR-0005 |
| Release | Icons | done | partial | done | `tools/icon-gen`; the official CloudDeck mark at the owner's request (private use, see THIRD_PARTY_NOTICES). The Android TV banner is still Moonlight's. |
| Release | Own version number | done | partial | done | 0.1.0 from `client-desktop/app/version.txt` |
| Account | Sign in, session restored from keychain | done | partial | done | Linux: Secret Service path untested on a real desktop |
| Account | MFA challenge | partial | partial | partial | This account has no MFA, so it's untested |
| VM | Live status, Start / Stop / Restart | done | partial | done | Restart = stop+start (no API) |
| VM | One-click start and connect | done | partial | done | Re-verified 2026-11-04 after the fix for hosts that are already known (new VM IP); 2026-11-05: a second stream in the same run no longer hangs on the app list; 2026-11-05: the owner sometimes needs a second try on Windows (works after a restart), not reproduced yet |
| VM | Automatic host address | done | partial | done | |
| VM | Automatic pairing (no PIN typing) | done | partial | done | |
| VM | Auto-stop on session end | done | partial | done | |
| VM | Auto-stop after N idle minutes | partial | partial | not started | Not part of the owner's test; desktop only |
| Stream | Codecs (H.264/HEVC/AV1), HDR, bitrate/fps | done | partial | done | Inherited from Moonlight |
| Input | Gamepads, touch modes | done | partial | done | Inherited from Moonlight; extended in Phase 6 |
| Audio | Microphone passthrough | done | partial | partial | Protocol v1 implemented end to end: common-c sender (19 tests + shared vector), Sunshine patch 0001 (host decoder tests the same vector), desktop SDL capture with mute hotkey Ctrl+Alt+Shift+U and level meter, Android `VOICE_COMMUNICATION` + AEC. Verified by the owner on Windows on 2026-11-04; Linux and Android capture still untested. An Android mute button isn't done yet. |
| Audio | Push-to-talk | not planned | not planned | not planned | Dropped by the owner on 2026-11-05: better handled inside the VM (games, Discord). A first version was removed |
| Host | Patched Sunshine install with automatic fallback to stock | done | — | — | Installed on the VM by the owner (bundle -id4d49f); streams work through it. CloudDeck default app list preserved |
| USB | USB device passthrough | done | partial | not planned | ADR-0006. Host: Go agent + Sunshine patch 0002 tunnel. Windows: usbipd-win, **verified by the owner on 2026-11-05**. Linux: pkexec helper on the kernel usbip-host (no network listener); agent and helper interop-tested with fake kernels, not yet run on a real Linux PC (owner tests later). Android not planned (owner doesn't need it). |
| Display | Multiple monitors, one wide stream (4a) | partial | partial | n/a | ADR-0007. One borderless window over the chosen monitors; the host agent switches the VM to the combined resolution via kscreen-doctor and restores it when the stream ends. Agent and layout unit-tested; **not yet run on the VM**. Linux via XWayland. |
| Display | Windowed mode | done | partial | n/a | Inherited from Moonlight (Display → Picture → Display mode; Ctrl+Alt+Shift+X). Used by the owner on Windows on 2026-11-05 |
| Display | Fit the VM resolution to the window or screen, following resizes | done | partial | partial | Android (2026-11-05): the VM follows the stream resolution for every stream, plus a resolution choice in the in-stream menu (reconnects); not yet verified on the VM. ADR-0007 amendment: an in-place stream restart at the new size about 1 s after a resize settles, keeping USB and the running app. Verified by the owner on Windows on 2026-11-05 (windowed resizes, fullscreen toggle, reconnect). Off by default. |
| Display | Multiple monitors as separate outputs (4b) | not started | not started | n/a | Needs kernel arguments (EDID overrides): owner approval first |
| Display | Tablet as extra screen | not started | n/a | not started | Stretch goal |
| UX | In-stream menu | done | partial | done | Rev. 3 (2026-11-05, not yet verified): tidier layout with quick tiles on both. Android: button + Back open a dialog (mic mute, stats, keyboard, disconnect, shut down), verified by the owner on 2026-11-05. ADR-0009 rev. 2: round CloudDeck button at the top centre (when the cursor is free) or Ctrl+Alt+Shift+O. Panel with display, mouse mode (game/desktop), audio, mic, live USB device switching, reconnect, disconnect, shut down. Rev. 1 (text OSD) was replaced after the owner's feedback. Verified by the owner on Windows on 2026-11-05 |
| UX | Network test, auto-recommended settings, low-bandwidth mode | done | partial | not started | ADR-0011: latency + CloudDeck→PC speed through the agent (net/test), recommended resolution/FPS/bitrate for this screen, one-click apply; low-bandwidth preset 720p30 3 Mbit/s. Verified by the owner on Windows on 2026-11-05 |
| UX | Clipboard sync (text) | done | partial | not started | ADR-0008: two-way via the agent and Plasma's Klipper (busctl as the desktop user), on by default, up to 100 KiB of text. Verified by the owner on Windows on 2026-11-05 |
| UX | File upload / drag-and-drop | not planned | not planned | not planned | Dropped by the owner on 2026-11-05 (not needed). A first version (agent + systemd-run --user, commit 6d36b2fa) failed with "broken pipe" on the VM and was reverted |
| UX | Auto-reconnect | done | partial | not started | ADR-0010: in-place reconnect after network drops, 5 attempts (2–8 s apart), notice under the menu button; restores clipboard sync and dropped USB devices. On by default. Verified by the owner on Windows on 2026-11-05 |
| UX | Configurable shortcuts | partial | partial | n/a | ADR-0012: the letter of every Ctrl+Alt+Shift shortcut (Moonlight's and CloudDeck's), conflict check, reset. Not yet verified |
| UX | Tray icon | not planned | not planned | n/a | Skipped by the owner on 2026-11-05 |
| Controllers | Controller as mouse (Select+L1+R1+A, hold Start, menu tile, start as mouse) | partial | partial | partial | docs/controller-mouse.md. Builds on Moonlight's mouse emulation; one on/off path that releases held mouse buttons. Desktop and Android build; not yet verified |
| UX | One settings menu (Moonlight's settings inside the CloudDeck settings) | partial | partial | partial | ADR-0013 amendment. Desktop: all SettingsView options ported to the CloudDeck pages (new Mouse & keyboard page); the toolbar gear opens them. Android: Moonlight's preference list embedded as the Stream page. Desktop pages checked in headless screenshots; not yet verified by the owner |
| Controllers | Extended controller support (rumble, gyro, DualSense features, hot-plug) | not started | not started | not started | Phase 6 (scope addition) |
| Controllers | Button/axis remapping with profiles | partial | partial | not started | docs/controller-profiles.md: buttons (incl. disable), stick deadzone/sensitivity/invert, trigger deadzone, swap sticks; default + per-controller; shared fixture tests. Xbox/Nintendo toggle in both in-stream menus. Desktop and Android, not yet verified |
| Controllers | On-screen controller with layout editor | partial | partial | partial | Android: Moonlight's editor (move/resize/colour) plus Xbox or PlayStation button style (labels and colours) and named layout profiles (save/load/delete), all from the in-stream menu. Not yet verified. Desktop (owner: in all builds, e.g. Linux phones): docs/on-screen-controller.md, a virtual Xbox-type pad drawn in masked windows, Xbox/PS style, multi-touch; no layout editor. Desktop: works per the owner on Windows (2026-11-05); Android parts not yet verified |
| Controllers | On-screen keyboard (Android, handhelds) | partial | partial | partial | docs/on-screen-keyboard.md: full layout with latching/locking modifiers, F-keys, arrows; real key presses; from the in-stream menu or Select+L1+R1+Y (Select+L1+R1+B opens the menu). Not yet verified |
| UX | UI overhaul: new home screen (sidebar pages, tiles, focus rings) | partial | partial | partial | Phase 7, ADR-0013. Mock-up direction A; a second "Launcher" layout (C) was built, then removed at the owner's request after testing. Android checked on an emulator (preview mode, every page, no crashes). Shared settings pages; touch, keyboard, gamepad and TV-remote navigation with visible focus rings. Desktop: both layouts load and pass key-only navigation tests headlessly (tests/qml/tst_home.qml, screenshots checked at 1280×800 and 390×844); the app builds. Android: builds, not yet run on a device. Not yet verified by the owner. Moonlight's own screens (PC list, app grid, streaming settings) are not restyled yet |
