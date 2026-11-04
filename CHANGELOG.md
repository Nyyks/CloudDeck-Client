# Changelog

All notable changes are recorded here, following [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]
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
