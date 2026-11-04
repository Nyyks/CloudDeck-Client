# ADR-0002: Host agent in Go

- Status: Proposed (confirm at Phase 1 start)
- Date: 2026-11-04

## Context
The host agent must be a single static binary run by systemd on Bazzite. It
can't depend on rpm-ostree layering. Its jobs: a TLS/mTLS listener (cert
allowlist from Sunshine's paired clients), a USB/IP import handshake and vhci
sysfs attach with fd passing, management of multiple Sunshine instances, a
clipboard/file-upload API, and PipeWire node setup (via config drop-ins and
`pw-cli`/`pactl`, not linked libraries).

## Options
- **Go:** `CGO_ENABLED=0` produces a static binary trivially. `crypto/tls` has
  first-class mTLS and peer-cert inspection. `syscall`/`x/sys/unix` covers
  socketpair, SCM_RIGHTS, and sysfs. Cross-compiling to x86_64 from any CI
  runner is easy, and it's fast to write. Cons: about 10 MB binary, GC (not
  relevant at these data rates).
- **Rust:** a smaller binary and no GC, but static linking needs musl targets
  and a heavier toolchain in CI, and iteration is slower. No capability Go lacks
  for this workload.

## Decision
Go, with no cgo. Anything latency-critical (mic decode, capture) lives in the
patched Sunshine, not the agent, so GC pauses never touch the media path. USB/IP
bridging is a plain byte copy between the TLS stream and a socketpair.

## Consequences
The agent is pure Go, so it's unit-testable on Windows and Linux CI runners. The
USB/IP message codec gets table-driven tests against captured protocol bytes.
