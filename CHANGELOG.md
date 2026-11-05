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
- Multiple monitors as one wide stream, and matching the VM's resolution to the stream
  (ADR-0007). The agent switches KWin's mode for the stream and restores it afterwards.
  Not yet verified on the VM.
- `host/tools/display-test.sh`: a 15-second, self-reverting check of KWin custom modes.
### Fixed
- Start and connect timed out when the host was already known under a new VM IP.
- HTTP/2 errors with the CloudFront API endpoint (desktop now uses HTTP/1.1).

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
