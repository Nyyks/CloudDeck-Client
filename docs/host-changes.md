# Server-side change log

Every change made on the CloudDeck VM is recorded here: what, why, how to undo
it, and whether it survives Bazzite updates. The install scripts back up every
file they replace to `/var/lib/clouddeck-client/backup/<timestamp>/`, and log
each run to `/var/lib/clouddeck-client/changes.log`.

## Changes made by `host/install/install.sh`

| Path | Change | Undo | Survives OS update? |
|---|---|---|---|
| `/var/opt/clouddeck-client/releases/<build>/` | New: patched Sunshine tree; `bin/sunshine` gets file caps `cap_sys_admin,cap_sys_nice=p` (same as stock) | `uninstall.sh --purge` | Yes (`/var`) |
| `/var/opt/clouddeck-client/sunshine` | New symlink to the active release | `uninstall.sh --purge` | Yes |
| `/var/opt/clouddeck-client/libexec/{sunshine-launch,sunshine-healthcheck}` | New helper scripts | `uninstall.sh --purge` | Yes |
| `/var/opt/clouddeck-client/uninstall.sh` | Copy of the uninstaller | `uninstall.sh --purge` | Yes |
| `~user/.config/systemd/user/app-dev.lizardbyte.app.Sunshine.service.d/zz-clouddeck-client.conf` | New user-unit drop-in: `ExecStart` → `sunshine-launch` | `uninstall.sh` | Yes (`/home`) |
| `~user/.config/clouddeck-client/`, `~user/.local/state/clouddeck-client/` | Launcher state (disable flag, health-check result, start times) | `uninstall.sh --purge` | Yes |
| `/var/lib/clouddeck-client/` | Install state, change log, backups | `uninstall.sh --purge` | Yes |
| `/var/opt/clouddeck-client/bin/clouddeck-agent` | Host agent binary (static Go) | `uninstall.sh --purge` | Yes |
| `/etc/systemd/system/clouddeck-agent.service` (enabled) | Agent system unit (root; needed for `vhci_hcd` sysfs and `modprobe vhci-hcd`) | `uninstall.sh` | Yes (`/etc`) |
| `/etc/clouddeck-client/agent.env` | `CDC_SOCKET_GROUP=<desktop user's group>`, `CDC_DESKTOP_USER=<desktop user>` | `uninstall.sh` | Yes |
| `/run/clouddeck-client/agent.sock` (runtime) | Agent socket, `root:<user group>` 0660; only Sunshine connects | Gone when the agent stops | No (tmpfs) |
| Kernel module `vhci-hcd` (runtime) | Loaded by the agent at start (in the stock kernel, no rpm-ostree) | `modprobe -r vhci-hcd` or reboot | No |
| PipeWire (runtime only) | Patch 0001 loads `module-null-sink` `sink-clouddeck-mic` and `module-remap-source` `clouddeck-mic` via `pactl` on the first mic stream | `pactl unload-module` or reboot; nothing is written to disk | No (gone after reboot; recreated on the next mic stream) |
| Display mode of `HDMI-A-1` (runtime, during streams) | Only while a CloudDeck-Client stream with multiple monitors or "match resolution" runs: the agent runs `kscreen-doctor` as the desktop user to switch modes, and switches back when the stream ends (ADR-0007) | Automatic at stream end, agent stop or uninstall; leftovers are restored at the next agent start | KWin persists the mode only if the VM is cut off mid-stream; the agent then restores it after boot |
| KWin custom modes (e.g. 3840×1080@60) | Added by the agent when a stream needs a size the emulated monitor doesn't list. Stored by KWin in `~user/.local/share/kscreen/` | System Settings → Display → remove the custom mode | Yes (`/home`) |
| Clipboard (runtime, during streams) | With clipboard sync on, the agent reads and sets the clipboard through Plasma's Klipper (`busctl --user` as the desktop user, ADR-0008). Nothing is written to disk by the agent; Klipper keeps its usual history | Turn the setting off in the client | — |
| `/var/lib/clouddeck-client/display-restore.json` (only during streams) | The mode to switch back to, for crash recovery | Deleted after each restore | Yes |

**Not touched:** the OS image (no rpm-ostree), `/usr/bin/sunshine`,
`~/.config/sunshine/*` (config, apps, pairing state), CloudDeck's own units and
drop-ins (`clouddeck-*`, `/etc/systemd/user/...clouddeck-fast-start.conf`),
firewalld, kernel modules, and the owner's `pictl-agent`.

## Applied changes

| Date | Change | Reversible by | Survives update? |
|---|---|---|---|
| 2026-11-04 | The owner ran `install.sh` from bundle `…-id4d49f`: patched Sunshine (mic, patch 0001) with launcher, drop-in and health check | `uninstall.sh` | Yes |
| 2026-11-05 (reported) | The owner ran `install.sh` from bundle `phase3-usb-test-1` (`…-ia17c08`): adds patch 0002 (agent tunnel), `clouddeck-agent` and its unit, and loads `vhci-hcd` | `uninstall.sh` | Yes |
