# Feature parity vs the Shadow PC client

Legend:
- **done**: works and was tested on the real VM
- **partial**: implemented, but not yet verified end-to-end on the VM
- **blocked**
- **not started**
- **not planned**

Nothing counts as done until it's tested against the real VM.

Last updated: 2026-11-04 (Phase 1 in progress).

| Area | Feature | Win | Linux | Android | Notes |
|---|---|---|---|---|---|
| Release | CI builds (MSI + portable zip, AppImage, APK) | partial | partial | partial | Green in CI for the upstream baseline; artifacts not yet installed on real devices |
| Release | Side-by-side with stock Moonlight | partial | partial | partial | Own settings namespace, UpgradeCode, desktop ID, applicationId (ADR-0005). Not yet tried on a machine with Moonlight installed. |
| Release | Own icons | not started | partial | not started | SVG for the Linux desktop entry only; `.ico`, Android mipmaps, and the in-app logo are still Moonlight's |
| Account | Sign in, session restored from keychain | partial | partial | partial | Code done: Cognito plus refresh token in Credential Manager / Secret Service / Android Keystore. Login + refresh verified with a script, not yet in the app. |
| Account | MFA challenge | partial | partial | partial | Handled in code; this account has no MFA, so it's untested |
| VM | Live status, Start / Stop / Restart | partial | partial | partial | API calls verified with a script; Restart = stop+start (no API) |
| VM | One-click start and connect | partial | partial | partial | Waits for running, the DNS flip, and Sunshine `serverinfo`; not yet run end-to-end |
| VM | Automatic host address | partial | partial | partial | `<id[2:]>.my.clouddeck.app`, verified with a script |
| VM | Automatic pairing (no PIN typing) | partial | partial | partial | PIN generated in the client and sent via `POST /clients`; **untested**, needs a real pairing |
| VM | Auto-stop on session end | partial | partial | partial | |
| VM | Auto-stop after N idle minutes | partial | partial | not started | Desktop only so far |
| Stream | Codecs (H.264/HEVC/AV1), HDR, bitrate/fps | partial | partial | partial | Inherited from Moonlight, unchanged; untested in our build |
| Input | Gamepads, touch modes | partial | partial | partial | Inherited from Moonlight, unchanged; untested in our build |
| Audio | Microphone passthrough | not started | not started | not started | Design: feasibility §3.1 |
| USB | USB device passthrough | not started | not started | not started | Design: feasibility §3.2 |
| Display | Multiple monitors | not started | not started | n/a | Design: feasibility §3.3 |
| Display | Tablet as extra screen | not started | n/a | not started | Stretch goal |
| UX | In-stream Quick Menu | not started | not started | not started | |
| UX | Network test, auto-recommended settings, low-bandwidth mode | not started | not started | not started | |
| UX | Clipboard sync (text) | not started | not started | not started | |
| UX | File upload / drag-and-drop | not started | not started | not started | |
| UX | Configurable shortcuts, auto-reconnect | not started | not started | not started | |
| UX | Tray icon | not started | not started | n/a | |
| UX | Android virtual gamepad / multi-touch | n/a | n/a | partial | Moonlight's on-screen controls, inherited |
