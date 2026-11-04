# ADR-0006: USB passthrough design (Phase 3)

- Status: Accepted (implementation in progress)
- Date: 2026-11-04
- Refines: feasibility §3.2 and ADR-0004

## Context
Facts the design depends on:
- **Host:** the VM's kernel has `vhci-hcd` and `usbip-core`, which are loadable
  without rpm-ostree.
- **Ports:** only TCP 47984/47989/48010 reach the VM (ADR-0004).
- **Auth:** Sunshine's HTTPS server (47984) already authenticates paired clients
  by certificate. Simple-Web-Server (pinned `546895a`) calls `on_upgrade` with
  the raw TLS socket only after Sunshine's verify callback accepted the
  certificate (`nvhttp.cpp`: verify, then read, then find_resource, then upgrade).

## Decision

```
client USB/IP server              TLS 47984 (mTLS, paired cert)           VM
 usbipd-win / usbipd / in-app  <──>  client relay  ══════════════>  Sunshine on_upgrade
        localhost:3240                                                    │ unix socket
                                                                   clouddeck-agent (root)
                                                                   OP_REQ_IMPORT ↔ reply
                                                                   socketpair → vhci_hcd attach
```

1. **The clients are dumb relays.** Each runs (or uses) a standard USB/IP server
   on `127.0.0.1:3240` and copies bytes between it and the tunnel:
   - Windows: **usbipd-win** (GPL-3.0). `usbipd bind` needs admin once per device.
   - Linux: `usbipd` from usbip-utils plus `usbip bind`, through a polkit/pkexec
     helper.
   - Android: an in-app userspace server based on cgutman/USBIPServerForAndroid
     (GPL-3.0).

   The USB/IP protocol logic therefore lives in one place: the agent.
2. **Tunnel (Sunshine patch 0002):** `GET /clouddeck/v1/<op>` with
   `Upgrade: clouddeck-agent/1` on 47984. Sunshine computes the SHA-256
   fingerprint of the verified client certificate, connects to
   `/run/clouddeck-client/agent.sock`, and sends one header line:
   `CDC1 <fingerprint> <path?query>\n`. It waits for the agent's
   `OK\n`/`ERR <msg>\n`, answers `101 Switching Protocols` (or 502), then
   relays bytes with asio async I/O on the server's own executor. The client
   must not send tunnel bytes before the 101.
   `GET /clouddeck/v1/hello` (no upgrade) returns the agent's JSON status;
   stock Sunshine returns 404, so features are hidden.
3. **Agent (Go, root, system unit `clouddeck-agent.service`)** handles
   `usb/attach?busid=`:
   - sends `OP_REQ_IMPORT` through the tunnel and parses `OP_REP_IMPORT`
     (device descriptor, speed);
   - picks a free vhci port of the right speed class (`hs` for ≤ USB 2,
     `ss` for USB 3);
   - creates a `socketpair`, writes `"<port> <fd> <devid> <speed>"` to
     `/sys/devices/platform/vhci_hcd.0/attach`, and copies bytes between the
     tunnel and its end of the pair;
   - on tunnel EOF, or on stop/uninstall, writes the port to `detach`, so the
     device disappears from the VM within seconds.
4. **Identity:** per-client state is keyed by certificate fingerprint. The
   agent's socket is `root:<desktop user>` 0660. Only Sunshine, running as the
   desktop user, connects to it; other processes of that user could too, but
   they can only attach devices that a paired client offers through a tunnel.
   This is accepted for a single-user VM.

## Consequences
- No new ports, and no second login (pairing = authorization).
- Isochronous devices (webcams, USB audio) will suffer over WAN. The UI warns
  about this.
- Unit-testable pieces: the USB/IP message codec, vhci status parsing, the
  tunnel header (Go tests), and the relay (C++ with a fake agent socket).

## Steps
1. Agent: protocol codec, vhci, unix server, systemd unit, install.sh
   integration (+ tests).
2. Sunshine patch 0002: hello + upgrade tunnel.
3. Windows client: usbipd-win detection/install hint, device list
   (`usbipd state`), bind, relay, UI.
4. Linux client: usbipd + pkexec helper.
5. Android client: userspace USB/IP server + relay.
