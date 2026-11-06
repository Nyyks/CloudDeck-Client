# CloudDeck-Client

A Shadow-style client for a personal [CloudDeck](https://clouddeck.app) cloud PC (Bazzite with
Sunshine), built on Moonlight. Sign in, press **Connect**, and the client starts the VM, pairs
and streams. You never have to copy an address or type a PIN.

Unofficial; not affiliated with CloudDeck, Moonlight or LizardByte.

## Features

| Feature | What it does | Where |
|---|---|---|
| **One-button connect** | Starts the VM if it's off, waits for it, pairs automatically on first use and launches the stream. Start, Stop and Restart from the home screen. Can stop the VM when you leave the stream. | all |
| **Home screen for every device** | Sidebar pages on desktop and tablets, a chip strip on phones. Works with touch, mouse, keyboard, gamepad or TV remote. All of Moonlight's streaming settings are on the same pages. | all |
| **Resolution that fits** | Pick the resolution from the home screen or the in-stream menu. On desktop the VM's desktop can also follow your window or screen size exactly, so nothing is scaled, and goes back to its own resolution afterwards. | all |
| **In-stream menu** | Quick tiles for fullscreen, resolution, stats, mute, microphone, controller mode, keyboards, USB devices, reconnect and shutdown. Opens with a round button, Ctrl+Alt+Shift+O, or Select+L1+R1+B on a controller. | all |
| **Microphone passthrough** | Your mic shows up on the VM as *CloudDeck Microphone*, encrypted with the stream. Mute it from the menu or a hotkey. | all |
| **Controllers** | Remapping profiles per controller (buttons, deadzones, sensitivity). Xbox, PlayStation or Nintendo layout. Controller as a mouse (Select+L1+R1+A). On-screen controller and a full on-screen keyboard with Esc, F-keys and Ctrl. | all |
| **USB passthrough** | Forwards USB devices to the VM during a stream, using usbipd-win on Windows and usbip on Linux. They come back when the stream ends. | Windows, Linux |
| **Clipboard sync** | Text copied on either side can be pasted on the other while streaming. | Windows, Linux |
| **Network test** | Measures latency and speed to your CloudDeck and applies matching quality settings. Also has a low-bandwidth preset. | Windows, Linux |
| **Auto-reconnect** | Reconnects in place after a network drop, up to 5 attempts, while your app keeps running. Can stop the VM after it has been idle for a while. | Windows, Linux |
| **Multiple monitors** | One stream across several monitors, as one wide desktop. | Windows, Linux |
| **Side by side with Moonlight** | Own app identity and settings; shares nothing with stock Moonlight. No admin rights needed on Windows. | all |

The VM features need the small server-side update below; streaming itself works without it.
What's verified on which platform: [docs/STATUS.md](docs/STATUS.md).

## Install the client

Download from the [latest release](../../releases/latest):

| Platform | File | Notes |
|---|---|---|
| Windows 10/11 (x64) | `…-windows-x64.msi` or `…-windows-x64-portable.zip` | The MSI installs for your user (`%LOCALAPPDATA%\Programs`), without admin rights. If you have 1.0.0 installed, uninstall it first: it was a per-machine install. |
| Linux (x64) | `…-linux-x86_64.AppImage` | `chmod +x` it and run it. |
| Raspberry Pi 4/5, ARM64 Linux | `…-linux-aarch64.AppImage` | Needs a 64-bit OS. |
| Android 5+ (phones, handhelds, TV) | `…-android.apk` | Allow installs from unknown sources. |

Sign in with your CloudDeck email and password. The password is used once; a session token
is kept in the OS keychain. The [user guide](docs/user-guide.md) explains everything else. The client's
About page checks for new versions.

## Install the server side (on the CloudDeck VM)

Microphone, USB, clipboard, display switching and the network test need a patched Sunshine
and a small host agent. Run these in Konsole on the VM, inside a stream (SSH to the VM is blocked):

1. In the VM's browser, download `clouddeck-host-*.tar.gz` and its `.sha256` from the
   [latest release](../../releases/latest).
2. Install:
   ```sh
   cd ~/Downloads
   sha256sum -c clouddeck-host-*.tar.gz.sha256
   tar xzf clouddeck-host-*.tar.gz
   cd clouddeck-host-*/
   sudo ./install.sh
   ```
3. About 15 seconds later Sunshine restarts and the stream drops. Reconnect.
4. Check: `cat ~/.config/clouddeck-client/last-healthcheck` should start with `ok`.

Nothing in the read-only Bazzite image changes. Stock Sunshine, its settings and pairings stay.
If the patched Sunshine fails, or a CloudDeck image update brings a new Sunshine version, stock
Sunshine starts instead. To undo everything: `sudo /var/opt/clouddeck-client/uninstall.sh --purge`.
Details: [host setup](docs/host-setup.md), [what changes on the VM](docs/host-changes.md).

## Documentation

| Doc | For |
|---|---|
| [User guide](docs/user-guide.md) | Using the clients: connecting, settings, in-stream menu, controllers, USB, monitors |
| [Host setup](docs/host-setup.md) · [Host changes](docs/host-changes.md) | Installing, updating and removing the VM side |
| [Status](docs/STATUS.md) | What works and what's verified, per platform |
| [Changelog](CHANGELOG.md) | What changed in each release |
| [Building](docs/building.md) · [Architecture](docs/architecture.md) · [ADRs](docs/adr/) | Development |

## Repository

| Path | What |
|---|---|
| `client-desktop/` | moonlight-qt fork (Windows, Linux x64 and ARM64) |
| `client-android/` | moonlight-android fork |
| `common/` | moonlight-common-c fork shared by both clients |
| `host/` | host agent (Go), Sunshine patches, VM install scripts |
| `docs/` | user guide, building, design notes and ADRs |

Development setup:

```sh
git clone --recurse-submodules <repo>
git config core.hooksPath .githooks   # gitleaks pre-commit hook (requires gitleaks)
```

## Support

If CloudDeck-Client is useful to you, you can [buy me a coffee](https://buymeacoffee.com/nyyks).

## License

GPL-3.0-only, like the upstream projects. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
