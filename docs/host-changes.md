# Server-side change log

Every change made on the CloudDeck VM is recorded here: what, why, how to undo
it, and whether it survives Bazzite updates. The install scripts back up every
file they replace to `/var/lib/clouddeck-client/backup/<timestamp>/`, and log
each run to `/var/lib/clouddeck-client/changes.log`.

## Changes made by `host/install/install.sh` (Phase 2 step 1, not yet applied)

| Path | Change | Undo | Survives OS update? |
|---|---|---|---|
| `/var/opt/clouddeck-client/releases/<build>/` | New: patched Sunshine tree; `bin/sunshine` gets file caps `cap_sys_admin,cap_sys_nice=p` (same as stock) | `uninstall.sh --purge` | Yes (`/var`) |
| `/var/opt/clouddeck-client/sunshine` | New symlink to the active release | `uninstall.sh --purge` | Yes |
| `/var/opt/clouddeck-client/libexec/{sunshine-launch,sunshine-healthcheck}` | New helper scripts | `uninstall.sh --purge` | Yes |
| `/var/opt/clouddeck-client/uninstall.sh` | Copy of the uninstaller | `uninstall.sh --purge` | Yes |
| `~user/.config/systemd/user/app-dev.lizardbyte.app.Sunshine.service.d/zz-clouddeck-client.conf` | New user-unit drop-in: `ExecStart` → `sunshine-launch` | `uninstall.sh` | Yes (`/home`) |
| `~user/.config/clouddeck-client/`, `~user/.local/state/clouddeck-client/` | Launcher state (disable flag, health-check result, start times) | `uninstall.sh --purge` | Yes |
| `/var/lib/clouddeck-client/` | Install state, change log, backups | `uninstall.sh --purge` | Yes |

**Not touched:** the OS image (no rpm-ostree), `/usr/bin/sunshine`,
`~/.config/sunshine/*` (config, apps, pairing state), CloudDeck's own units and
drop-ins (`clouddeck-*`, `/etc/systemd/user/...clouddeck-fast-start.conf`),
firewalld, kernel modules, and the owner's `pictl-agent`.

## Applied changes

| Date | Change | Reversible by | Survives update? |
|---|---|---|---|
| — | *None yet.* | — | — |
