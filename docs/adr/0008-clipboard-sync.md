# ADR-0008: Two-way text clipboard sync (Phase 5)

- Status: Accepted (implemented; not yet verified on the VM)
- Date: 2026-11-05

## Context
- Moonlight only *types* the local clipboard into the host as keystrokes
  (Ctrl+Alt+Shift+V). Nothing comes back from the host.
- The VM runs Plasma 6 on Wayland. A process outside the focused window can't
  read or write the Wayland clipboard directly. Plasma's clipboard service,
  **Klipper**, can, and it is reachable on the session bus:
  `org.kde.klipper /klipper org.kde.klipper.klipper`, with
  `getClipboardContents` and `setClipboardContents(s)`.
- During a stream the client's Qt event loop is paused (the SDL loop owns the
  main thread). SDL has a clipboard API and an `SDL_CLIPBOARDUPDATE` event.

## Decision
1. **Transport:** `GET /clouddeck/v1/clipboard/sync` upgrades to a tunnel
   (ADR-0004/0006 transport, paired clients only), held open for the stream
   with a keepalive every 30 s. Both directions carry one JSON object per line,
   `{"text":"..."}`. After `OK` the agent sends `{"max_bytes":102400}`.
2. **VM side (agent):** runs `busctl --user -- call org.kde.klipper …` as the
   desktop user (the same mechanism as kscreen-doctor in ADR-0007; `busctl`
   ships with systemd). It polls `getClipboardContents` every 700 ms and sends
   changes. The client's texts are applied with `setClipboardContents`.
   - One mutex covers each Get or Set together with the "last text" check, so
     a client update is never echoed back.
   - The VM's clipboard at stream start is **not** sent. The client's
     clipboard wins when a stream starts.
3. **Client side:**
   - A worker thread holds the tunnel. The SDL loop owns the local clipboard.
   - On `SDL_CLIPBOARDUPDATE`, which also fires for copies in other local apps
     while streaming, the loop offers the text to the worker.
   - Text from the VM arrives as a registered SDL event. The loop applies it
     with `SDL_SetClipboardText` and remembers it, so the resulting update
     isn't sent back.
   - The sync lives for the whole session, including fit-to-window restarts.
4. **Limits:**
   - Text only, up to 100 KiB of UTF-8. The limit comes from `busctl` passing
     the text as one argument, and Linux capping a single argument at 128 KiB.
     Larger texts are skipped and logged.
5. **Setting:** "Share the clipboard (text) with my CloudDeck while streaming",
   **on by default** like Shadow. The data stays between the user's PC and
   their own VM, inside the paired TLS tunnel.

## Consequences
- Without Klipper (clipboard applet removed), the agent answers
  `ERR klipper getClipboardContents: …`. The client logs it and streams
  normally.
- Images and files aren't synced. Files belong to the planned file upload.
- Polling costs one short `busctl` process every 700 ms while a stream runs.

## Tests
- `host/agent/internal/clipboard`, against a fake Klipper:
  - both directions, with newlines, Unicode and a leading `-`;
  - no echo;
  - the VM's initial clipboard not sent;
  - the size limit and malformed lines;
  - the exact `busctl` arguments.
- `internal/agent`: the server operation, the status line and shutdown.
