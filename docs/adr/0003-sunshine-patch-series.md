# ADR-0003: Sunshine changes as a patch series, not a full fork

- Status: Proposed (depends on how Sunshine is installed on the VM; see feasibility §1)
- Date: 2026-11-04

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
