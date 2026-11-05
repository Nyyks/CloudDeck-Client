# Changelog

All notable changes are recorded here, following [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

## [1.0.0] - 2026-11-05
First stable release: every phase of the brief is implemented. Windows is verified by the owner;
Android was tested by the owner, and the fixes made after that test were checked on an emulator
against a local Sunshine. Linux, multiple monitors and the Raspberry Pi build are not verified yet
(see docs/STATUS.md).

### Added
- Resolution in the in-stream menu: desktop restarts the stream in place at the new size; Android
  reconnects at it. Also an on-screen controller tile in the Android menu.
- Raspberry Pi / ARM64 Linux AppImage (CI: `linux-arm64.yml`; locally: `tools/build-linux-arm64.sh`). Not verified on a Pi yet.
- Controller as mouse (docs/controller-mouse.md): Select+L1+R1+A switches a controller between gamepad
  and mouse, a menu tile does it for all controllers, and controllers can start as a mouse. Desktop and Android.
- One settings menu: Moonlight's streaming settings are part of the CloudDeck settings (ADR-0013 amendment);
  new Mouse & keyboard page on desktop; Moonlight's list is the Stream page on Android.
- Tidier in-stream menus with quick toggle tiles on desktop and Android (ADR-0009 rev. 3).
- New home screen (ADR-0013, Phase 7): sidebar pages, a round Connect button and tiles, at least
  48 px touch targets and visible focus rings; works with D-pad, gamepad and TV remote on desktop and
  Android. (A second "Launcher" layout was tried and removed at the owner's request.)
  - Desktop has headless QML navigation tests with screenshots.
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
- Android: disconnecting from a stream crashed the app. moonlight-common-c's connection cleanup
  asserted on the microphone lock, which lives for the whole process; it is now exempt from that
  check (debug builds abort on it; reproduced and verified against a local Sunshine on an emulator).
- The CloudDeck's desktop now switches to the stream resolution for every stream on desktop and
  Android, not only with "Fit to window", so e.g. a 1280×720 handheld no longer gets a scaled
  1920×1080 desktop (ADR-0007 amendment).
- Windows: the .msi failed when run directly ("Could not write value DesktopShortcutInstallState");
  it now creates the desktop shortcut by default.
- Android: opening the Stream settings crashed the app (Moonlight preferences that others depend on were
  removed without detaching them).
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
