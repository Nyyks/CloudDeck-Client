# CloudDeck-Client

A Shadow-style client for a personal [CloudDeck](https://clouddeck.app) cloud PC (Bazzite with
Sunshine), built on Moonlight. Sign in, press **Connect**, and it starts the VM, pairs and streams.
It adds microphone and USB passthrough, clipboard sync, fit-to-window and multi-monitor streams,
auto-reconnect, a network test, controller profiles, on-screen controls and an in-stream menu.

Unofficial; not affiliated with CloudDeck, Moonlight or LizardByte.

## Install the clients

Download from the [latest release](../../releases/latest):

| Platform | File | Notes |
|---|---|---|
| Windows 10/11 (x64) | `…-windows-x64.msi` or `…-windows-x64-portable.zip` | The MSI adds Start menu and desktop shortcuts. |
| Linux (x64) | `…-linux-x86_64.AppImage` | `chmod +x` it and run it. |
| Android 5+ (phones, handhelds, TV) | `…-android-arm64-x86_64-debug.apk` | Allow installs from unknown sources. |

All clients install next to stock Moonlight and share nothing with it. Sign in with your CloudDeck
email and password; the password is used once and a session token goes to the OS keychain.
See the [user guide](docs/user-guide.md).

## Install the server side (on the CloudDeck VM)

The VM features (microphone, USB, clipboard, display switching, network test) need a patched
Sunshine and a small host agent. Streaming works without them, just without those features.
Run these in Konsole on the VM, inside a stream (SSH to the VM is blocked):

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

Nothing in the read-only Bazzite image changes, and stock Sunshine, its settings and pairings
stay. If the patched Sunshine ever fails, or a CloudDeck image update brings a new Sunshine
version, stock Sunshine starts instead. To undo everything:
`sudo /var/opt/clouddeck-client/uninstall.sh --purge`.
Details: [host setup](docs/host-setup.md), [what changes on the VM](docs/host-changes.md).

## Repository

| Path | What |
|---|---|
| `client-desktop/` | moonlight-qt fork (Windows, Linux) |
| `client-android/` | moonlight-android fork |
| `common/` | moonlight-common-c fork shared by both clients |
| `host/` | host agent (Go), Sunshine patches, VM install scripts |
| `docs/` | user guide, building, design notes and ADRs, [status](docs/STATUS.md) |

Building: [docs/building.md](docs/building.md). Development setup:

```sh
git clone --recurse-submodules <repo>
git config core.hooksPath .githooks   # gitleaks pre-commit hook (requires gitleaks)
```

## License

GPL-3.0-only, like the upstream projects. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
