# ADR-0004: Reach the host agent through Sunshine's HTTPS port

- Status: Proposed (implement in Phase 2)
- Date: 2026-11-04

## Context
A port probe on 2026-11-04 showed that CloudDeck's upstream filter forwards
**only TCP 47984, 47989 and 48010** (plus the Sunshine UDP ports) to the VM.
The VM's own firewall is wide open, but we can't change the upstream filter. The
agent (USB/IP, clipboard, file upload, monitor layout, VM-side mic sink control)
therefore can't have its own listening port.

TCP 47984 is Sunshine's nvhttp HTTPS server. It already requires a client
certificate, and it checks that certificate against the paired-client list in
`sunshine_state.json`.

## Options
1. **Patch Sunshine:** route `/clouddeck/v1/*` on 47984 to the agent's Unix
   socket (`/run/clouddeck-client/agent.sock`), with an HTTP `Upgrade` for raw
   byte streams. Auth reuses Sunshine's mTLS check.
2. **Front Sunshine with the agent:** the agent binds 47984, peeks at the TLS
   ClientHello (SNI/ALPN `clouddeck-agent`), and forwards everything else to
   Sunshine moved to localhost. No Sunshine patch, but Sunshine's base port
   moves, which risks the CloudDeck controller/watchdog and confuses
   `serverinfo`'s `HttpsPort`.
3. Use the RTSP port 48010: plaintext and short-lived by design; rejected.

## Decision
Option 1. The patch is small (one route, one socket proxy) and lives in
`host/sunshine-patches/`. Clients detect the agent with
`GET /clouddeck/v1/hello`: 200 means it's present, 404 (stock Sunshine) means
no agent, so the features stay hidden.

The agent gets the verified client-cert fingerprint from Sunshine in a header
it trusts only on the Unix socket. That's needed for per-client state, for
example which USB devices belong to which client.

## Consequences
- Agent features require the patched Sunshine. The Phase 2 install ships both
  together.
- USB/IP latency gains one TLS hop and one local socket copy, which is
  negligible next to WAN RTT.
- If CloudDeck ever opens more ports, option 2 or a dedicated port can be added
  without changing the client protocol (same paths, different port).
