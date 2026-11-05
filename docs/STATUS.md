# Feature parity vs the Shadow PC client

Legend:
- **done**: works and was tested on the real VM
- **partial**: implemented, but not yet verified end-to-end on the VM
- **blocked**
- **not started**
- **not planned**

Nothing counts as done until it's tested against the real VM.

Last updated: 2026-11-05 (Phase 4a implemented). Phase 1 verified by the owner on real devices with
v0.1.0-alpha.1. The Linux AppImage was built but its end-to-end run wasn't
explicitly confirmed, so Linux stays *partial* where it differs.

| Area | Feature | Win | Linux | Android | Notes |
|---|---|---|---|---|---|
| Release | CI builds (MSI + portable zip, AppImage, APK); tagged releases | done | partial | done | |
| Release | Side-by-side with stock Moonlight | done | partial | done | ADR-0005 |
| Release | Own icons | done | partial | done | `tools/icon-gen`. The Android TV banner is still Moonlight's. |
| Release | Own version number | done | partial | done | 0.1.0 from `client-desktop/app/version.txt` |
| Account | Sign in, session restored from keychain | done | partial | done | Linux: Secret Service path untested on a real desktop |
| Account | MFA challenge | partial | partial | partial | This account has no MFA, so it's untested |
| VM | Live status, Start / Stop / Restart | done | partial | done | Restart = stop+start (no API) |
| VM | One-click start and connect | done | partial | done | Re-verified 2026-11-04 after the fix for hosts that are already known (new VM IP); 2026-11-05: a second stream in the same run no longer hangs on the app list |
| VM | Automatic host address | done | partial | done | |
| VM | Automatic pairing (no PIN typing) | done | partial | done | |
| VM | Auto-stop on session end | done | partial | done | |
| VM | Auto-stop after N idle minutes | partial | partial | not started | Not part of the owner's test; desktop only |
| Stream | Codecs (H.264/HEVC/AV1), HDR, bitrate/fps | done | partial | done | Inherited from Moonlight |
| Input | Gamepads, touch modes | done | partial | done | Inherited from Moonlight; extended in Phase 6 |
| Audio | Microphone passthrough | done | partial | partial | Protocol v1 implemented end to end: common-c sender (19 tests + shared vector), Sunshine patch 0001 (host decoder tests the same vector), desktop SDL capture with mute hotkey Ctrl+Alt+Shift+U and level meter, Android `VOICE_COMMUNICATION` + AEC. Verified by the owner on Windows on 2026-11-04; Linux and Android capture still untested. Push-to-talk and an Android mute button aren't done yet (planned with the Quick Menu). |
| Host | Patched Sunshine install with automatic fallback to stock | done | — | — | Installed on the VM by the owner (bundle -id4d49f); streams work through it. CloudDeck default app list preserved |
| USB | USB device passthrough | done | partial | not planned | ADR-0006. Host: Go agent + Sunshine patch 0002 tunnel. Windows: usbipd-win, **verified by the owner on 2026-11-05**. Linux: pkexec helper on the kernel usbip-host (no network listener); agent and helper interop-tested with fake kernels, not yet run on a real Linux PC (owner tests later). Android not planned (owner doesn't need it). |
| Display | Multiple monitors, one wide stream (4a) | partial | partial | n/a | ADR-0007. One borderless window over the chosen monitors; the host agent switches the VM to the combined resolution via kscreen-doctor and restores it when the stream ends. Agent and layout unit-tested; **not yet run on the VM**. Linux via XWayland. |
| Display | Windowed mode | done | partial | n/a | Inherited from Moonlight (Settings → Display mode; Ctrl+Alt+Shift+X). Used by the owner on Windows on 2026-11-05 |
| Display | Fit the VM resolution to the window or screen, following resizes | done | partial | not started | ADR-0007 amendment: an in-place stream restart at the new size about 1 s after a resize settles, keeping USB and the running app. Verified by the owner on Windows on 2026-11-05 (windowed resizes, fullscreen toggle, reconnect). Off by default. |
| Display | Multiple monitors as separate outputs (4b) | not started | not started | n/a | Needs kernel arguments (EDID overrides): owner approval first |
| Display | Tablet as extra screen | not started | n/a | not started | Stretch goal |
| UX | In-stream menu | partial | partial | not started | ADR-0009 rev. 2: round CloudDeck button at the top centre (when the cursor is free) or Ctrl+Alt+Shift+O. Panel with display, mouse mode (game/desktop), audio, mic, live USB device switching, reconnect, disconnect, shut down. Rev. 1 (text OSD) was replaced after the owner's feedback. Not yet verified |
| UX | Network test, auto-recommended settings, low-bandwidth mode | not started | not started | not started | Phase 5 |
| UX | Clipboard sync (text) | done | partial | not started | ADR-0008: two-way via the agent and Plasma's Klipper (busctl as the desktop user), on by default, up to 100 KiB of text. Verified by the owner on Windows on 2026-11-05 |
| UX | File upload / drag-and-drop | not started | not started | not started | Phase 5 |
| UX | Configurable shortcuts, auto-reconnect | not started | not started | not started | Phase 5 |
| UX | Tray icon | not started | not started | n/a | Phase 5 |
| Controllers | Extended controller support (rumble, gyro, DualSense features, hot-plug) | not started | not started | not started | Phase 6 (scope addition) |
| Controllers | Button/axis remapping with profiles | not started | not started | not started | Phase 6 (scope addition) |
| Controllers | On-screen controller with layout editor | not started | not started | partial | Phase 6. Android has Moonlight's on-screen controls; editor/profiles to come. Desktop touchscreens optional. |
