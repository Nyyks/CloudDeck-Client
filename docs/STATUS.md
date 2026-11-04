# Feature parity vs the Shadow PC client

Legend: **done** (works and tested on the real VM) · **partial** · **blocked** ·
**not started** · **not planned**. Nothing counts as done until it's tested
against the real VM.

Last updated: 2026-11-04 (Phase 0).

| Area | Feature | Win | Linux | Android | Notes |
|---|---|---|---|---|---|
| Account | Sign in, session restored from keychain | not started | not started | not started | Cognito, refresh token only |
| VM | Live status, Start / Stop / Restart | not started | not started | not started | Restart = stop+start (no API) |
| VM | One-click start and connect | not started | not started | not started | |
| VM | Automatic host address | not started | not started | not started | `<id[2:]>.my.clouddeck.app` |
| VM | Automatic pairing (no PIN typing) | not started | not started | not started | via `POST /clients {pin}` |
| VM | Auto-stop on session end / idle | not started | not started | not started | |
| Stream | Codecs (H.264/HEVC/AV1), HDR, bitrate/fps | not started | not started | not started | Inherited from Moonlight; untested in our build |
| Input | Gamepads, touch modes | not started | not started | not started | Inherited from Moonlight; untested in our build |
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
| UX | Android virtual gamepad / multi-touch | n/a | n/a | not started | |
| Release | CI builds + GitHub Releases | not started | not started | not started | |
