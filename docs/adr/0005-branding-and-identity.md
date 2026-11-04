# ADR-0005: Branding and side-by-side identity

- Status: Accepted
- Date: 2026-11-04

## Context
The fork must install and run beside stock Moonlight without sharing settings,
paired hosts, or client certificates, and without either installer upgrading or
removing the other. Every edit to upstream files is a future merge conflict, so
the rebrand should touch as few upstream lines as possible.

## Decision
| Item | Value |
|---|---|
| Display name | **CloudDeck Client** |
| Reverse-DNS ID | `io.github.nyyks.CloudDeckClient` (desktop file, Wayland app_id, X11 WM_CLASS) |
| QSettings namespace | org `CloudDeck-Client`, app `CloudDeck Client` (own config dir, own pairing cert) |
| Windows MSI | own UpgradeCode `13a9f569-2a4e-4933-a953-da72914e9d10`, install folder `CloudDeck Client`, registry `HKCU\Software\CloudDeck-Client` |
| Android | `applicationId io.github.nyyks.clouddeckclient` (upstream asks forks to change it). The Java namespace stays `com.limelight`. ABIs: arm64-v8a and x86_64. |
| Update checker | disabled (it would offer stock Moonlight releases) |

**Kept on purpose:** the internal binary names (`Moonlight.exe`, `moonlight`) and
upstream's packaging scripts. CI renames the outputs to
`CloudDeckClient-<version>-<platform>.*`. Renaming the binaries would change
20+ lines across `build-arch.bat`, `build-appimage.sh`, and the WiX sources,
which upstream edits often. The visible process name on Windows comes from the
version resource (`QMAKE_TARGET_PRODUCT`), which is rebranded.

New branding code lives in `client-desktop/app/clouddeck/`. Upstream files only
gain an `#include` or a path change, each marked `CloudDeck-Client`.

## Consequences
- `grep -rn "CloudDeck-Client" client-*/` lists every upstream touch point when
  merging.
- Icons are generated from `tools/icon-gen/icon.svg` (plus `foreground.svg` for
  the Android adaptive icon) by `tools/icon-gen`. It overwrites upstream's asset
  files in place (`moonlight.ico`, `res/moonlight.svg`, mipmaps), so no build file
  changes. Rerun it after an upstream merge that touches those assets.
- Still Moonlight's: the Android TV banner (`atv_banner`).
