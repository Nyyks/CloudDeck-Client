# Third-party notices

This project is licensed GPL-3.0. The table below records every upstream project
whose code is (or is planned to be) included or borrowed, with its license as
checked on the date shown. **Rows marked "imported" are in the tree; the rest are planned.**

| Project | License (checked) | Use | State |
|---|---|---|---|
| [moonlight-stream/moonlight-qt](https://github.com/moonlight-stream/moonlight-qt) | GPL-3.0 (2026-11-04) | Base of `client-desktop/` (v6.2.0, subtree) | imported |
| [moonlight-stream/moonlight-android](https://github.com/moonlight-stream/moonlight-android) | GPL-3.0 (2026-11-04) | Base of `client-android/` (v12.2, subtree) | imported |
| [moonlight-stream/moonlight-common-c](https://github.com/moonlight-stream/moonlight-common-c) | GPL-3.0 (2026-11-04) | Base of `common/` (f900dd47, subtree) | imported |
| [cgutman/enet](https://github.com/cgutman/enet) | MIT (upstream ENet) | Submodule of common | imported (submodule) |
| [sleepybishop/nanors](https://github.com/sleepybishop/nanors) | MIT | Submodule of common | imported (submodule) |
| [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) | GPL-3.0 (2026-11-04) | Patched host build | planned |
| [dorssel/usbipd-win](https://github.com/dorssel/usbipd-win) | GPL-3.0 (2026-11-04) | Windows USB/IP server, installed separately | planned |
| [cgutman/USBIPServerForAndroid](https://github.com/cgutman/USBIPServerForAndroid) | GPL-3.0 (2026-11-04) | Base for the Android USB/IP server | planned |

## Studied for reference only (no code copied)

| Project | License (checked) | Notes |
|---|---|---|
| LizardByte/Sunshine PR #4078 | GPL-3.0 | Mic design review discussion |
| [logabell/moonlight-common-c-mic](https://github.com/logabell/moonlight-common-c-mic) | GPL-3.0 | Negotiated-mic protocol, for interop alignment |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo), [Nonary/Vibepollo](https://github.com/Nonary/Vibepollo) | GPL-3.0 | Sunshine forks (Windows-focused) |
| [AlkaidLab/foundation-sunshine](https://github.com/AlkaidLab/foundation-sunshine) | GPL-3.0 | Mic/clipboard/USB features; Windows-only host |
| [qiin2333/moonlight-vplus](https://github.com/qiin2333/moonlight-vplus) | GPL-3.0 | Client counterpart of foundation-sunshine |
| [ClassicOldSong/moonlight-android](https://github.com/ClassicOldSong/moonlight-android) (Artemis) | GPL-3.0 | Android fork |
| [JimothySnicket/moonlight-mic](https://github.com/JimothySnicket/moonlight-mic) | MIT | Plaintext side-channel; rejected design |
| [vadimgrn/usbip-win2](https://github.com/vadimgrn/usbip-win2) | BSD-2-Clause | USB/IP *client*: wrong direction, not used |
| [gitleaks/gitleaks](https://github.com/gitleaks/gitleaks) | MIT | Dev tool only, not distributed |

## Bundled via the upstream clients (licenses as shipped upstream)
moonlight-qt also brings [cgutman/qmdnsengine](https://github.com/cgutman/qmdnsengine)
(MIT) and [SDL_GameControllerDB](https://github.com/gabomdq/SDL_GameControllerDB) (zlib)
as submodules, plus the prebuilt Windows dependencies from
[moonlight-qt-deps](https://github.com/moonlight-stream/moonlight-qt-deps) (FFmpeg, SDL,
OpenSSL, Opus, etc.; each under its own license). moonlight-android's dependencies are
listed in `client-android/app/build.gradle` (BouncyCastle MIT, OkHttp Apache-2.0,
jmDNS Apache-2.0, jcodec BSD-2-Clause).

## Trademarks
The app icon and the in-stream menu button use the **CloudDeck mark**: the two brackets
traced from clouddeck.app's favicon into `tools/icon-gen/icon.svg`,
`tools/icon-gen/foreground.svg` and `client-desktop/app/res/clouddeck-glyph.svg`. The
mark belongs to CloudDeck and is **not** covered by this project's GPL-3.0 license. It's
used at the owner's request in this private, personal client. Replace it with the
project's own icon before distributing builds publicly. The previous own icon is in the
git history, commit `b2fda0ae`.
