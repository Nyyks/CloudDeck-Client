# Phase 0: Discovery and feasibility

Status: **complete pending your approval, 2026-11-04.**
Sources:
- the host inventory (`host/install/inventory.sh`, run by the owner on the VM at 14:34 CET)
- live read-only API calls plus one start → probe → stop cycle (14:38–14:41 CET)
- static analysis of the portal bundle

---

## 1. Host inventory

| Item | Finding |
|---|---|
| OS image | **CloudDeck's own image** `ghcr.io/clouddeck/clouddeck-os-bazzite:stable` (version stable-26.05.7, 2026-09-24), based on `bazzite-deck` F43 (43.20260420, Kinoite). Portal reports `os: clouddeck-26.04.1`. **No layered packages, no overrides.** CloudDeck ships the image updates, so rpm-ostree layering would also fight their update channel. |
| Kernel | `6.17.7-ba29.fc43.x86_64`; kargs include `amdgpu.ppfeaturemask=0xffffffff` |
| Virtualization | QEMU/KVM (Q35), `qemu-guest-agent` running; 8 vCPU, 15 GiB RAM, 250 GB btrfs `/var` |
| GPU | **AMD Radeon RX 7600 XT (Navi 33), PCI passthrough**, `amdgpu`, Mesa 26.0.4. VA-API **encode: H.264, HEVC Main/Main10, AV1**. |
| Connectors | `HDMI-A-1` connected (dummy/emulated EDID; modes up to 2560×1440@144, 1920×1080@240), **`DP-1`, `DP-2`, `DP-3` disconnected (free)**, plus a writeback connector |
| Session | **KDE Plasma 6.6.4, Wayland** (SDDM autologin `plasma.desktop`, user `user`/1000). The gamescope sessions are installed but not active. Xwayland is present. |
| Sunshine | **RPM `Sunshine-2026.914.233613-1.fc43`** in the image, `/usr/bin/sunshine` with file caps `cap_sys_admin,cap_sys_nice=p`. Runs as **user unit** `app-dev.lizardbyte.app.Sunshine.service` (alias `sunshine.service`), with CloudDeck drop-ins (`clouddeck-fast-start`, `-fast-stop`, `-no-sleep`). Config `~/.config/sunshine/sunshine.conf`: `sunshine_name = clouddeck-43ca6bbd2b`, `origin_web_ui_allowed = pc`, **`capture = kms`**. One app, "CloudDeck". The state file has web-UI `username`/`password` and 2 named devices (paired clients). |
| Sunshine watchdog | CloudDeck user unit `clouddeck-sunshine-watchdog.service` ("Restart Sunshine on encoder failure"), `/usr/libexec/sunshine-watchdog` |
| Audio | PipeWire 1.4.10, WirePlumber 0.5.12, pipewire-pulse. One sink: HDMI stereo. **No sources** apart from its monitor. |
| USB/IP | **`usbip-core`, `vhci-hcd`, `usbip-host` present** (`CONFIG_USBIP_VHCI_HCD=m`, 8 ports, 1 HC) and loadable (dry run). **`usbip`/`usbipd` userspace tools are in the image** (`usbip-utils 2.0`). Not loaded at boot. |
| Other kernel bits | `vkms=m`, `snd-aloop=m`, `/dev/uinput` with ACL for `user` |
| Network | `ens18` **public IPv4 77.90.1.249/26** (+ public IPv6 /64), `ens19` private 192.168.0.219/24 (likely the provider's management network) |
| VM firewall | firewalld zone `FedoraWorkstation` on both NICs: **TCP+UDP 1025–65535 open**, plus ssh. No rich or direct rules. **The VM does not do the IP allowlisting.** |
| **Upstream filter** | Probed from the starting IP. **Only TCP 47984, 47989, 48010 get through.** 22, 47990, 47985–48009 (except those three), 3240, 8080, 8443, 8989, 80, 443, 49000, 50123 all time out (filtered, not refused). UDP not probed; streaming works, so the Sunshine UDP ports (47998–48000) must be open. |
| CloudDeck components (leave alone) | `clouddeck-controller-service` (root, TCP 8989, probably the portal → VM channel, e.g. pairing-PIN submission via Sunshine's local web API), `clouddeck-gpu-powercap` (10 s timer), `clouddeck-growfs`, `clouddeck-flatpak-install`, `clouddeck-sunshine-watchdog`. A **"MoonlightWeb"/AppImage process** (TCP 8080/8443 public bind, 48001/48003 localhost) is most likely CloudDeck's browser-streaming bridge (`*.cldk.me`). `wondershaper.service` (bandwidth shaping) is present. |
| Owner's own tooling (leave alone) | `pictl-agent` (root systemd timer, `/usr/local/bin`) and `/usr/local/bin/rclone`. The owner confirmed on 2026-11-04 that both are theirs. Install scripts must not touch them. |
| SSH | `sshd` active on the VM but **unreachable from the internet** (port 22 is filtered upstream). |

### Constraints that follow
1. **No new inbound ports.** Anything the client must reach on the host has to
   share TCP 47984/47989/48010 or the existing Sunshine UDP ports.
2. **Don't move or re-credential Sunshine's web API.** The CloudDeck controller
   probably uses it for pairing. Keep `sunshine.conf`/state paths, the unit
   name, and the watchdog working.
3. **No rpm-ostree.** CloudDeck owns the image. Persistent, writable places:
   `/var/opt` (=`/opt`), `/var/usrlocal` (=`/usr/local`), `/etc`, `~user`.
   File capabilities on binaries under `/var/opt` survive updates (btrfs xattrs).
4. Kernel arguments (`rpm-ostree kargs`) are technically separate from layering,
   but they're still an rpm-ostree change. **We ask you before any kargs change.**

---

## 2. CloudDeck API: verified

Full details in [clouddeck-api.md](clouddeck-api.md). Verified live:
- **Cognito `USER_PASSWORD_AUTH` login:** no MFA challenge. Access/ID tokens
  last **3600 s**. **`REFRESH_TOKEN_AUTH` works** and does not rotate the
  refresh token. The ID token carries `custom:account`.
- `GET /accounts/{id}` → `machine_id`; `GET /machines/{id}` → `status`,
  `public_ip` (empty while off), `os`, `region`, `last_started`.
  Unauthenticated → **401**.
- `POST /start {}` → **204**. `starting` → `running` took **≈ 80 s**. `public_ip`
  is set, and **`<id[2:]>.my.clouddeck.app` is repointed by DNS to the VM's
  public IP** (77.90.1.249). Off → `52.29.233.180` (AWS; parking/wake endpoint).
- `POST /stop {}` → **204**. `stopping` → `off` took **≈ 75 s**.
- Sunshine `GET http://<host>:47989/serverinfo` is reachable right after
  `running` (`state=SUNSHINE_SERVER_FREE`, `appversion 7.1.431.-1`).
- Not yet tested: the pairing PIN submission (needs our client to drive the
  Moonlight handshake; Phase 1), and **allowlist behaviour from a second IP**
  (needs a second network, e.g. a phone hotspot; quick to do whenever you like).

**Go.**

---

## 3. Feature feasibility (revised for the port allowlist)

### 3.0 Host-agent transport: tunnel through Sunshine's HTTPS port (new, ADR-0004)
There is no free port, so the agent can't listen publicly. Instead, a small
Sunshine patch adds the path `/clouddeck/v1/*` on the **nvhttp HTTPS server
(TCP 47984)**. That server **already authenticates paired clients by their
certificate (mTLS)**, which is exactly the auth the brief asks for. Requests are
forwarded to the agent's Unix socket. Long-lived byte streams (USB/IP, file
upload) use `CONNECT`-style upgrades. Stock Sunshine returns 404 for that path,
which the client treats as "agent not installed", so it degrades cleanly.
Fallback if patching nvhttp proves painful: the agent takes 47984 and demuxes by
TLS SNI/ALPN in front of a relocated Sunshine. This is rejected for now because
it moves Sunshine's ports, which the CloudDeck controller and the watchdog may
depend on.

### 3.1 Microphone: **GO**
Design unchanged from the draft. It's now confirmed to be the *only* viable
shape: the de-facto fork protocol's separate mic port (47996 fallback) **would
be filtered upstream**. Ours rides on the existing audio UDP port.
- **Negotiation:** SDP `a=x-ss-mic.version:1` plus `a=rtpmap:96 opus/48000/1`,
  bit `SS_ENC_MICROPHONE`; `SETUP`/`PLAY streamid=mic`, aligned with
  logabell/moonlight-common-c-mic. The `Transport` reply returns the existing
  audio port.
- **Packet:** type byte `0x4D`, `sessionTag`, 64-bit seq, RTP timestamp,
  **AES-128-GCM** (nonce `"MIC1"‖seq`, header as AAD), with a replay window.
- **Codec:** Opus 48 kHz mono, 20 ms, about 32 kbps, in-band FEC.
- **Host sink:** patched Sunshine → PipeWire loopback whose source side is
  **"CloudDeck Microphone"** (`Audio/Source`), defined in a user-level drop-in
  `~/.config/pipewire/pipewire.conf.d/`. The VM currently has *no* input
  device, so this becomes the default source.

### 3.2 USB: **GO**
- Every kernel piece is present (vhci-hcd, usbip-host), plus `usbip` tools,
  with no layering needed.
- Transport goes over the 47984 tunnel (3.0). The agent bridges each tunnel
  stream into a `socketpair()` and attaches it through
  `/sys/devices/platform/vhci_hcd.0/attach`. The agent loads `vhci-hcd` on
  demand via `modprobe`.
- Capacity: 8 vhci ports (USB 2 and 3 each per the kernel config; enough for
  this use).
- Client sides are as in the draft: a Linux polkit helper plus `usbip-host`,
  usbipd-win on Windows, and a userspace server on Android.
- Remaining risks: isochronous devices over WAN; usbipd-win needs a UAC prompt
  per new device.

### 3.3 Multiple monitors: **GO for design B; design A becomes "A′"**
- *Design A as briefed* (one Sunshine instance per display, each on its own
  ports) is **no-go**: the extra port sets would be filtered upstream.
- **Design B (Phase 4a, primary):** one stream of a wide desktop, split
  client-side into one window per local monitor. On this host:
  - Plasma 6.6 supports **custom modes**, so the connected `HDMI-A-1` (emulated
    monitor) can be driven at e.g. 3840×1080 or 5120×1440 *without kargs*.
    *To test.*
  - Fallback: an EDID override on a free DP connector (needs kargs, so I ask
    first).
  - Encoding: VCN 4 on Navi 33 handles HEVC/AV1 up to 8K wide; H.264 is capped
    at 4096.
  - Input mapping is trivial.
  - Limits: one refresh rate for all monitors; Plasma sees one ultrawide output.
    Workaround for the taskbar: two virtual outputs side by side, capturing the
    union (see A′).
- **Design A′ (Phase 4b):** **one** Sunshine process with `channels = N`. Sunshine
  already runs concurrent sessions on the same ports, demuxed by
  `X-SS-Ping-Payload`/connect data. A patch adds a negotiated per-session
  `output` selection. The displays come from
  `DP-1..3` with EDID overrides (`drm.edid_firmware=` + `video=DP-n:e` +
  `firmware_class.path=/var/lib/clouddeck-client/firmware`). That's a **kargs
  change, so you must approve it**. Input goes only through the primary
  session, with absolute coordinates over the combined layout.
- The Android "remote screen" stretch goal also builds on A′.

---

## 4. Phased plan with acceptance criteria

Each phase ends with STATUS.md and CHANGELOG updates and a tagged release.

**Phase 1: MVP**
1. Subtree-import moonlight-qt v6.2.0, moonlight-android v12.2, and
   moonlight-common-c → `client-desktop/`, `client-android/`, `common/`
   (ADR-0001). *AC:* unmodified upstream builds pass in CI on all three
   platforms.
2. Rebrand (name, app ID, icons, no upstream update check). *AC:* installs side
   by side with stock Moonlight.
3. CI matrix: Windows MSI and portable zip, Linux AppImage, Android
   arm64-v8a/x86_64 debug APK. Tags publish a GitHub Release.
   *AC:* tag `v0.1.0` produces all artifacts.
4. `CloudProvider` (C++/Qt and Java) with shared fixtures. Path allowlist (no
   `/reset`, no billing). Refresh token in the OS keychain. *AC:* unit tests
   pass; after sign-in the password is nowhere on disk; restarting the app
   restores the session.
5. VM card plus "Start and connect". Timeouts: `running` ≤ 4 min (observed
   80 s), `serverinfo` reachable ≤ 2 min after that, and the DNS flip handled
   (resolve fresh, fall back to `public_ip`). *AC:* one click from `off` reaches a
   stream on the real VM.
6. Auto-pair (client-generated PIN → `POST /clients`). *AC:* after "reset
   pairings", one click re-pairs with no PIN typed.
7. Auto-stop on session end or N idle minutes. *AC:* `off` within N+2 min.
8. No regressions. *AC:* manual matrix (H.264/HEVC/AV1, 1440p144, gamepad,
   touch) on Windows, Linux, and Android.

**Phase 2: Host foundation + mic**
1. `host/install/install.sh`/`uninstall.sh` (idempotent, with backups).
   Patched Sunshine in `/var/opt/clouddeck-client/sunshine` via a **user-unit
   drop-in** (`ExecStart=`) that keeps the unit name, so the watchdog keeps
   working; setcap; reverting removes the drop-in. Go agent as a system unit
   (`/etc/systemd/system`), plus the 47984 tunnel (ADR-0004).
   *AC:* survives a reboot and a CloudDeck image update; uninstall restores stock
   Sunshine byte-for-byte config; portal pairing still works.
2. Mic. *AC:* the host's Discord/Steam hear the client mic, with E2E latency
   < 150 ms at 30 ms RTT; only GCM ciphertext on the wire; a stock host shows no
   mic UI.

**Phase 3: USB.** *AC:* an Xbox controller and a flash drive from each client OS
enumerate on the host. Killing the client detaches within 5 s, and the device
returns locally.

**Phase 4a: Multi-monitor (wide stream)** *AC:* 2×1080p60 local monitors, each
showing its half, with the cursor crossing seamlessly. **4b (A′, after your OK
for kargs):** *AC:* 2 independent outputs.

**Phase 5: Polish.** Quick Menu, network test, clipboard, file upload,
shortcuts, auto-reconnect, tray, Android virtual gamepad.

**Windowed mode** *(scope addition requested by the owner, 2026-11-05; done
with Phase 4a)*:
- **Inherited from Moonlight:** a windowed display mode (Settings → Display
  mode) and the Ctrl+Alt+Shift+X fullscreen toggle.
- **Added:** "Fit the CloudDeck's resolution to the stream window". The VM's
  desktop takes the window's exact pixel size, or the screen's in fullscreen.
  When a resize or fullscreen toggle settles (about 1 s), the stream restarts
  in place at the new size, keeping the same window, the running app and
  attached USB devices. ADR-0007 has the amendment.
- **Planned:** a fullscreen/windowed entry in the Quick Menu.

*AC:* resizing the window or toggling fullscreen leaves the remote desktop
unscaled and sharp within about 3 s. Quitting restores the VM's original
resolution.

**Phase 6: Controllers** *(scope addition requested by the owner, 2026-11-04)*

Baseline inherited from Moonlight:
- SDL gamepads on desktop, plus a mapping helper for unknown pads
- Android gamepad support and the configurable on-screen controller

The phase extends that baseline:
1. **Controller support:** verify and fill gaps for the pads you actually use
   (Xbox, DualSense/DualShock, Switch Pro, 8BitDo) on every client. That
   covers rumble, motion/gyro, touchpad, and the DualSense LED and trigger
   effects where Sunshine supports them, plus multiple pads and hot-plug.
2. **Remapping:**
   - per-controller button and axis remapping with deadzone and sensitivity,
     saved as profiles
   - an optional per-app profile
   - a UI on desktop and Android; the profile format is shared so a profile
     moves between clients
3. **On-screen controller:**
   - Android: a layout editor (position, size, opacity, which buttons), plus
     profiles
   - Windows/Linux touchscreens (tablets, 2-in-1s), if wanted
   - reachable from the Quick Menu

*AC:* each listed pad works with rumble on each client. A remap profile applies
in-game and survives restart. A custom on-screen layout can be created, saved,
and used in a stream. Detailed plan to be confirmed with you before the phase
starts.
