# ADR-0003: Sunshine changes as a patch series, not a full fork

- Status: Accepted (host facts confirmed by the 2026-11-04 inventory)
- Date: 2026-11-04

## Host facts (inventory)
- Sunshine is the RPM `Sunshine-2026.914.233613-1.fc43` baked into CloudDeck's
  image (`/usr/bin/sunshine`, caps `cap_sys_admin,cap_sys_nice=p`). It runs as
  user unit `app-dev.lizardbyte.app.Sunshine.service` with CloudDeck drop-ins,
  and is supervised by `clouddeck-sunshine-watchdog.service`.
- So the **pinned upstream tag is `v2026.914.233613`** (same version as the
  host), and the build container is **Fedora 43**.
- The override is a user-unit drop-in
  (`~/.config/systemd/user/app-dev.lizardbyte.app.Sunshine.service.d/zz-clouddeck-client.conf`
  with `ExecStart=` + `ExecStart=/var/opt/clouddeck-client/sunshine/bin/sunshine`).
  The unit name, config path, and state path stay as they are, so CloudDeck's
  watchdog and controller keep working. Uninstalling just removes the drop-in.
- On a CloudDeck image update that bumps Sunshine, the installer warns about the
  version mismatch. We then rebase the series and rebuild.

## Context
We need small host-side protocol extensions (mic stream, combined-desktop
absolute pointer, maybe a PipeWire capture backend). Sunshine moves fast
(calendar-versioned releases) and has 16 submodules. Bazzite ships Sunshine in
a way still to be determined (image package, Flatpak, or Homebrew).

## Decision
- `host/sunshine-patches/` holds a numbered `git format-patch` series against a
  **pinned upstream tag**, plus `series` and `UPSTREAM_TAG` files.
- CI builds the patched Sunshine in a container matching the VM's Fedora release
  and produces a tarball installed under `/opt/clouddeck/sunshine/`. A user
  systemd unit override then points at it. The stock Sunshine stays installed,
  and uninstalling restores it.
- Every patch is gated by negotiation. A patched host talking to a stock client
  behaves exactly like stock Sunshine.

## Consequences
Rebasing on a new Sunshine tag means applying the series and fixing what
conflicts. The patches stay few and focused, so the conflict surface stays
small. If a patch looks generally useful (for example GCM mic per the #4078
review), we offer it upstream.
