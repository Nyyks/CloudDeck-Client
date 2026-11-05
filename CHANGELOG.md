# Changelog

All notable changes are recorded here, following [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]
### Added
- Host install (`host/install/`): patched Sunshine with an automatic fallback to stock,
  plus the Go host agent as a system unit. The CloudDeck default app list is kept.
- Microphone passthrough (protocol v1, docs/protocol-extensions.md): desktop and Android
  capture, plus host Sunshine patch 0001. Verified on Windows.
- USB passthrough (ADR-0006): agent tunnel (Sunshine patch 0002) and vhci attach.
  - Windows uses usbipd-win (verified).
  - Linux uses a pkexec helper on usbip-host (tested with fake kernels only).
- Multiple monitors as one wide stream, and fitting the VM's resolution to the stream window
  (follows resizes and fullscreen toggles with an in-place stream restart)
  (ADR-0007). The agent switches KWin's mode for the stream and restores it afterwards.
  Fit to window is verified on Windows. Multiple monitors are not verified yet.
- `host/tools/display-test.sh`: a 15-second, self-reverting check of KWin custom modes.
- Two-way text clipboard sync during streams (ADR-0008), through the agent and Plasma's Klipper.
  On by default. Verified on Windows.
- In-stream menu on desktop (ADR-0009): a round CloudDeck button at the top centre or Ctrl+Alt+Shift+O
  opens a panel with display, mouse mode, audio and mic settings, live USB device switching,
  reconnect, disconnect, and shutting down the VM.
- Auto-reconnect on desktop (ADR-0010): network drops reconnect in place (5 attempts), restoring
  clipboard sync and USB devices.
- Network test with recommended settings and a low-bandwidth preset (ADR-0011).
- Configurable Ctrl+Alt+Shift shortcuts on desktop (ADR-0012).
- Controller remapping profiles on desktop and Android (docs/controller-profiles.md) and a live Xbox/Nintendo
  layout switch in the in-stream menus.
- On-screen controller button style (Xbox or PlayStation) and saved layouts on Android.
- On-screen controller on desktop too (docs/on-screen-controller.md), for touchscreen devices.
- Full on-screen keyboard on Android and desktop (docs/on-screen-keyboard.md); gamepad combos
  Select+L1+R1+Y (keyboard) and Select+L1+R1+B (CloudDeck menu).
- In-stream menu on Android: a CloudDeck button or Back opens mic mute, stats, keyboard,
  disconnect and shut down (ADR-0009).
- The official CloudDeck mark as the app icon and the menu button (private use; see THIRD_PARTY_NOTICES).
### Fixed
- Start and connect timed out when the host was already known under a new VM IP.
- HTTP/2 errors with the CloudFront API endpoint (desktop now uses HTTP/1.1).
- A second stream in the same client run hung on "Loading app list", then failed with
  "app not found".

## [0.1.0] - 2026-11-04
Phase 1 MVP. Verified end-to-end on Windows and Android against the real CloudDeck VM.
### Changed
- Own version number (0.1.0) from `client-desktop/app/version.txt`, shared by all clients.
  CI checks that release tags match it.
### Planned
- Phase 6 (controllers: extended support, remapping, on-screen controller) added to the scope.

## [0.1.0-alpha.1] - 2026-11-04
### Added
- Phase 1: moonlight-qt v6.2.0, moonlight-android v12.2, and moonlight-common-c imported as
  subtrees. Both clients build the shared `common/`.
- CI matrix: Windows MSI + portable zip, Linux AppImage, Android APK; tags publish a release.
- Rebrand for side-by-side installs (ADR-0005).
- CloudDeck integration on desktop and Android: sign-in (refresh token in the OS keychain),
  VM card with Start/Stop/Restart, one-click start-and-connect, automatic pairing via the
  portal, optional auto-stop. Unit tests on both platforms share fixtures.
- Docs: architecture, building, user guide.
- Repository skeleton, GPL-3.0 license, gitleaks pre-commit hook.
- Phase 0 docs: brief, feasibility draft, reverse-engineered CloudDeck portal API, ADRs 0001–0003.
- Read-only host inventory script (`host/install/inventory.sh`).
- Phase 0 completed: host inventory, live-verified portal API (auth, refresh, start/stop timings),
  upstream port-allowlist finding, ADR-0004 (agent tunnel over Sunshine HTTPS).
### Fixed
- `inventory.sh` now redacts `Bearer`/`Basic` tokens and URL token parameters, and skips the
  multi-hundred-KB rpm-ostree image metadata.
