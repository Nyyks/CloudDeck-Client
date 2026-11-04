# Feature parity vs the Shadow PC client

Legend:
- **done**: works and was tested on the real VM
- **partial**: implemented, but not yet verified end-to-end on the VM
- **blocked**
- **not started**
- **not planned**

Nothing counts as done until it's tested against the real VM.

Last updated: 2026-11-04. Phase 1 verified by the owner on real devices with
v0.1.0-alpha.1. The Linux AppImage was built but its end-to-end run wasn't
explicitly confirmed, so Linux stays *partial* where it differs.

| Area | Feature | Win | Linux | Android | Notes |
|---|---|---|---|---|---|
| Release | CI builds (MSI + portable zip, AppImage, APK); tagged releases | done | partial | done | |
| Release | Side-by-side with stock Moonlight | done | partial | done | ADR-0005 |
| Release | Own icons | done | partial | done | `tools/icon-gen`. The Android TV banner is still Moonlight's. |
| Release | Own version number | partial | partial | partial | 0.1.0 from `client-desktop/app/version.txt`, pending the v0.1.0 build |
| Account | Sign in, session restored from keychain | done | partial | done | Linux: Secret Service path untested on a real desktop |
| Account | MFA challenge | partial | partial | partial | This account has no MFA, so it's untested |
| VM | Live status, Start / Stop / Restart | done | partial | done | Restart = stop+start (no API) |
| VM | One-click start and connect | done | partial | done | |
| VM | Automatic host address | done | partial | done | |
| VM | Automatic pairing (no PIN typing) | done | partial | done | |
| VM | Auto-stop on session end | done | partial | done | |
| VM | Auto-stop after N idle minutes | partial | partial | not started | Not part of the owner's test; desktop only |
| Stream | Codecs (H.264/HEVC/AV1), HDR, bitrate/fps | done | partial | done | Inherited from Moonlight |
| Input | Gamepads, touch modes | done | partial | done | Inherited from Moonlight; extended in Phase 6 |
| Audio | Microphone passthrough | partial | partial | partial | Protocol v1 implemented end to end: common-c sender (19 tests + shared vector), Sunshine patch 0001 (host decoder tests the same vector), desktop SDL capture with mute hotkey Ctrl+Alt+Shift+U and level meter, Android `VOICE_COMMUNICATION` + AEC. **Not yet tested against the VM.** Push-to-talk and an Android mute button aren't done yet (planned with the Quick Menu). |
| Host | Patched Sunshine install with automatic fallback to stock | partial | — | — | `host/install`, launcher fail-safe tests pass; not yet installed on the VM |
| USB | USB device passthrough | not started | not started | not started | Phase 3, design: feasibility §3.2 |
| Display | Multiple monitors | not started | not started | n/a | Phase 4, design: feasibility §3.3 |
| Display | Tablet as extra screen | not started | n/a | not started | Stretch goal |
| UX | In-stream Quick Menu | not started | not started | not started | Phase 5 |
| UX | Network test, auto-recommended settings, low-bandwidth mode | not started | not started | not started | Phase 5 |
| UX | Clipboard sync (text) | not started | not started | not started | Phase 5 |
| UX | File upload / drag-and-drop | not started | not started | not started | Phase 5 |
| UX | Configurable shortcuts, auto-reconnect | not started | not started | not started | Phase 5 |
| UX | Tray icon | not started | not started | n/a | Phase 5 |
| Controllers | Extended controller support (rumble, gyro, DualSense features, hot-plug) | not started | not started | not started | Phase 6 (scope addition) |
| Controllers | Button/axis remapping with profiles | not started | not started | not started | Phase 6 (scope addition) |
| Controllers | On-screen controller with layout editor | not started | not started | partial | Phase 6. Android has Moonlight's on-screen controls; editor/profiles to come. Desktop touchscreens optional. |
