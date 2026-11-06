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

### Amendment (2026-11-05): fit to the window
"Set the CloudDeck's resolution to match the stream" became **"Fit the
CloudDeck's resolution to the stream window"**. It applies to windowed mode
(the owner asked for windowed mode) and to fullscreen.

1. **Initial size.** The stream starts at the window's pixel size. In
   fullscreen it uses the screen's native mode instead of the configured
   resolution. The size is clamped to the host limits and made even.
2. **Resizes.** A resize or fullscreen toggle arms a check, which runs once the
   size has been stable for 1 s and no mouse button is held. If the window's
   pixel size differs from the stream's, the session restarts **in place**:
   - The decoder is dropped and input is ignored.
   - A worker thread stops the connection and opens a new display lease
     *before* releasing the old one, so the host goes straight to the new size
     and still remembers the original mode.
   - The worker then sends `resume` (the app keeps running) and reconnects.
   - A render reset recreates the decoder in the same SDL window.

   Mic streaming restarts with the connection. USB forwarding is untouched.
   The default bitrate is rescaled; a custom bitrate stays.
3. **Exclusive fullscreen** (`WM_FULLSCREEN`) never triggers restarts. It picks
   its display mode from the stream.
4. **Quitting during a restart** interrupts it, and cleanup waits for it to
   finish, so connection start and stop never run at the same time.

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

## Amendment: every stream follows its resolution (2026-11-05)
Testing on a 1280×720 handheld showed the stream picture was a scaled copy of whatever mode
the VM had. So the display lease is now taken for every stream, not only for multiple monitors
or fit-to-window:
- Desktop: `Session` always acquires it before `/launch`. A failure is still only fatal when
  spanning monitors.
- Android: `clouddeck/DisplayLease.java` and `AgentTunnel.java` are ports of the desktop tunnel
  (same mTLS identity, pinned host certificate, `display/set`, keepalive). `CloudDeckActivity`
  acquires the lease before the stream and releases it when the stream ends.
- Hosts without the agent (stock Sunshine, Manage PCs) answer 404 and stream as before.

The in-stream menus also offer a resolution choice. Desktop restarts the stream in place;
Android ends the stream and reconnects at once, keeping the lease.

## Amendment 2: no separate outputs for now (2026-11-06)
The owner decided not to change the VM's boot settings, so Phase 4b (separate KWin outputs
through kernel-argument EDID overrides) is not planned. A wrong kernel argument can leave the VM
unbootable or unreachable. Multiple monitors stay one wide VM output: fullscreen games and
maximized windows span every streamed monitor. The user guide advises single-monitor mode for
games and window snapping for apps. Revisit only if the owner asks.
