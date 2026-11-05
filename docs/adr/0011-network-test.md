# ADR-0011: Network test and recommended settings (Phase 5, desktop)

- Status: Accepted (implemented; not yet verified)
- Date: 2026-11-05
- Note: number 0011 was first used by the file-upload design, which the owner
  dropped and which was reverted (commit f46ace9d).

## Context
The brief asks for a network test with auto-recommended settings and a
low-bandwidth mode. Only Sunshine's ports reach the VM (ADR-0004), so the test
goes through the agent tunnel. What matters for streaming is the CloudDeck →
PC direction and the round-trip time.

## Decision
1. **Agent operation `net/test`:** after `OK`, the client sends lines:
   - `ping` → `pong`;
   - `down <n>` → exactly *n* zero bytes. TLS doesn't compress, so zeros
     measure real throughput.

   Limits: 128 MiB per request, 512 MiB per connection, 30 s idle.
2. **Client (`NetTest` worker thread):**
   - Latency is the median of 5 pings; a sixth warm-up ping isn't counted.
   - Speed comes from transfers starting at 1 MiB and growing ×4 until one
     takes ≥ 1.5 s or reaches 64 MiB. The last one counts.
   - Typical use is a few tens of MB.
3. **Recommendation** (`NetRecommend::recommend`, unit-tested):
   - **Budget:** 70 % of the measured speed, between 1 and 150 Mbit/s. A
     single TCP stream isn't the stream's UDP path, and other traffic shares
     the line.
   - **Resolution:** the largest of 4K, 1440p, 1080p and 720p that fits this
     PC's screen and whose Moonlight default bitrate (at 60 FPS) is at most
     1.67 × the budget. Roughly 69 / 34 / 17 Mbit/s measured for 4K / 1440p /
     1080p.
   - **Frame rate:** 120 FPS on a ≥ 120 Hz screen when the budget carries 80 %
     of the 120 FPS default; 30 FPS below a 4 Mbit/s budget; otherwise 60.
   - **Bitrate:** the lower of the budget and Moonlight's default for the
     chosen mode, at least 2 Mbit/s.
   - **Latency notes:** above 40 ms (desktop fine, fast games slightly
     delayed) and above 80 ms (expect input delay).
4. **UI:** on the CloudDeck screen, *Test my connection*, then *Apply
   recommended settings*. Applying writes resolution, FPS and bitrate to
   Moonlight's settings and turns off the bitrate auto-adjust so the measured
   value sticks. **Low-bandwidth mode** applies 720p, 30 FPS, 3 Mbit/s.
   The test needs the VM running and this PC paired (an earlier connection).

## Consequences
- With *Fit the CloudDeck's resolution to the stream window* on, the window
  decides the resolution. The recommended bitrate and FPS still apply.
- TCP throughput through TLS can understate fast lines with high latency
  (window-limited). The recommendation then errs on the low side.
