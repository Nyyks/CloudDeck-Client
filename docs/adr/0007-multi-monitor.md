# ADR-0007: Multiple monitors, part A: one wide stream (Phase 4a)

- Status: Accepted (implemented; not yet verified on the VM)
- Date: 2026-11-05
- Refines: feasibility §3.3, design B

## Context
- Only TCP 47984/47989/48010 and Sunshine's UDP ports reach the VM (ADR-0004),
  so one Sunshine instance per display (the brief's design A) is ruled out.
- The VM has one connected output, `HDMI-A-1`, with an emulated EDID. Its
  modes go up to 2560×1440@144 and 1920×1080@240, and it currently runs 1920×1080.
- KWin 6.6 supports custom modes. `kscreen-doctor` can add them:
  - `output.<name>.addCustomMode.<w>.<h>.<mHz>.<full|reduced>`
  - `output.<name>.mode.<id>`

  This needs no kernel arguments.
- Sunshine captures with KMS and picks up the output's size when a stream
  starts. Prep commands only run on `launch`, not on `resume`, and stock
  Moonlight clients would trigger them too.

## Decision
1. **One stream, one window.** The client asks for a single stream whose
   resolution is the bounding box of the chosen local monitors, in native
   pixels. It shows that stream in **one borderless window** that covers exactly
   those monitors.
   - The mouse and keyboard need no mapping. The cursor crosses monitors
     because it is one window and one remote desktop.
   - Audio is unchanged.
2. **The host matches the size through the agent, not prep commands.**
   - Before `/launch` or `/resume`, the client opens a
     `GET /clouddeck/v1/display/set?width=&height=&fps=` tunnel (ADR-0006
     transport; paired clients only).
   - The agent runs `kscreen-doctor` **as the desktop user** inside their
     Wayland session. It selects a matching mode, or adds a custom mode (full
     blanking first, then reduced), switches to it, and checks that KWin
     really switched.
   - The tunnel stays open for the stream, with a client keepalive every 30 s.
     When it closes (stream end, client exit, crash, network loss), the agent
     switches back to the mode that was active before.
   - Nested leases restore only when the last one ends.
   - Stock Moonlight clients never send this, so nothing changes for them.
3. **Crash safety.**
   - The mode to restore is written to
     `/var/lib/clouddeck-client/display-restore.json` before anything changes.
   - At start, the agent restores a leftover mode, retrying until the Plasma
     session exists.
   - A stream that arrives before that restore adopts the remembered mode as
     its own "original".
   - On agent stop or uninstall, every lease is restored.
4. **Limits**, enforced on both sides:
   - at most 7680×4320;
   - a pixel rate of at most 3840×2160×60, which allows 5120×1440@60,
     3840×1080@120 and 5760×1080@60.

   Above 4096 px the client drops H.264, which AMD VCN can't encode at that
   width.
5. **Settings** (desktop):
   - "Set the CloudDeck's resolution to match the stream": the same lease for
     one monitor. Off by default until verified.
   - "Use several monitors", with a checkbox per local monitor. Mixed display
     scaling and overlapping (mirrored) monitors are rejected. Non-rectangular
     arrangements get a warning.

## Consequences
- Plasma sees **one ultrawide output**:
  - the panel spans all monitors;
  - "maximize" fills all of them;
  - all monitors share one refresh rate.

  Phase 4b (design A′: several KWin outputs, per-session output selection)
  removes this, but needs kernel arguments for EDID overrides. **That needs the
  owner's approval first.**
- Custom modes stay in KWin's list after use. They are harmless, and the next
  stream at that size reuses them.
- The span window isn't SDL-fullscreen, which is limited to one display:
  - on Windows, the taskbar hides only while the window has focus;
  - "Capture system keys: fullscreen only" doesn't apply (use "always").
- The Linux AppImage runs through XWayland. Native Wayland can't position
  windows.
- Android: not applicable. The tablet-as-extra-screen stretch goal belongs to 4b.

## Tests
- Agent (`host/agent/internal/display`, `internal/agent/display_test.go`), with
  a fake `kscreen-doctor`:
  - existing and custom modes;
  - reduced-blanking fallback;
  - KWin ignoring a switch;
  - nested leases;
  - restore after a crash, and a stream arriving first;
  - shutdown restore;
  - the tunnel held until close.
- Client (`tests/clouddeck`, span layout): side-by-side, a negative origin,
  uniform 2× scaling, mixed scaling, a non-rectangle, the limits and fps cap.
- On the VM: `host/tools/display-test.sh` (switches for 15 s, always switches
  back), then a real two-monitor stream.
