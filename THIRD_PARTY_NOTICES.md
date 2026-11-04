# Third-party notices

This project is licensed GPL-3.0. The table below records every upstream project
whose code is (or is planned to be) included or borrowed, with its license as
checked on the date shown. **No third-party code has been imported yet.**
Entries are marked *planned* until the import commit lands.

| Project | License (checked) | Use | State |
|---|---|---|---|
| [moonlight-stream/moonlight-qt](https://github.com/moonlight-stream/moonlight-qt) | GPL-3.0 (2026-11-04) | Base of `client-desktop/` | planned |
| [moonlight-stream/moonlight-android](https://github.com/moonlight-stream/moonlight-android) | GPL-3.0 (2026-11-04) | Base of `client-android/` | planned |
| [moonlight-stream/moonlight-common-c](https://github.com/moonlight-stream/moonlight-common-c) | GPL-3.0 (2026-11-04) | Base of `common/` | planned |
| [cgutman/enet](https://github.com/cgutman/enet) | MIT (upstream ENet) | Submodule of common | planned |
| [sleepybishop/nanors](https://github.com/sleepybishop/nanors) | MIT | Submodule of common | planned |
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
