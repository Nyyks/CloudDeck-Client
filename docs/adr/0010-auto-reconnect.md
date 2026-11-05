# ADR-0010: Auto-reconnect (Phase 5, desktop)

- Status: Accepted (implemented; not yet verified)
- Date: 2026-11-05

## Context
Moonlight ends the session on any connection failure and shows an error.
CloudDeck runs over the internet, often on Wi-Fi, so short drops are common.
ADR-0007 already restarts a stream in place: same window, `resume` of the
running app, decoder recreated.

## Decision
1. **When:** `clConnectionTerminated` sets the next attempt time instead of
   ending the session, for every error except:
   - a graceful end (the host quit or the VM stopped);
   - host-side faults (DRM-protected content, unexpected early termination,
     encoder frame conversion).

   The setting is *Reconnect automatically if the connection drops*, on by
   default.
2. **How:**
   - The SDL loop starts the ADR-0007 restart with `reconnecting = true`.
   - A failed attempt schedules the next one after 2, 4, 6 and 8 s, with the
     window unchanged.
   - After **5** failed attempts the session ends with the last error.
   - A stream that stays up for 60 s starts a fresh count.
3. **Feedback:** a small notice under the menu button reads "Connection lost.
   Reconnecting (n of 5)…". It's a Qt window, because Moonlight's overlay
   needs a decoder and the decoder is gone while reconnecting. Input is
   dropped while waiting. After success, errors queued by failed attempts are
   cleared so no stale dialog appears later.
4. **Side channels after any in-place restart:**
   - a clipboard sync whose tunnel died is restarted;
   - USB devices marked for forwarding but no longer attached are attached
     again;
   - the display lease is re-acquired by the restart itself.

## Consequences
- The in-stream menu's *Reconnect* button uses the same path.
- If the VM is genuinely gone (e.g. stopped from elsewhere), the attempts take
  about 20 s before the session ends.
