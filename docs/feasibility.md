# Phase 0: Discovery and feasibility

Status: **DRAFT, 2026-11-04.** Section 2 (portal API) is done from static
analysis. Section 1 (host inventory) is **pending SSH access to the VM**. The
go/no-go calls in section 3 are provisional until section 1 is filled in; each
one names the inventory facts it depends on.

---

## 1. Host inventory: PENDING

To be collected with [`host/install/inventory.sh`](../host/install/inventory.sh),
a strictly read-only script (no installs, no writes, no service restarts, no
module loading). Fields to fill in:

| Item | Value | Why it matters |
|---|---|---|
| Bazzite image / version / Fedora base | ? | Sunshine build target (Fedora release of the CI container) |
| Kernel | ? | usbip modules, kargs for EDID |
| GPU / driver | ? | Encoder, KMS capture, virtual display approach |
| Virtualization (KVM/…, GPU passthrough vs vGPU) | ? | Spare connectors for EDID override |
| Sunshine version, install method, launch unit | ? | Whether we replace the binary (and how that survives updates) |
| Session: gamescope vs Plasma, Wayland vs X11 | ? | Multi-monitor feasibility (Q3) |
| PipeWire / WirePlumber | ? | Virtual mic node |
| `usbip-core`, `vhci-hcd` present / loadable | ? | USB passthrough (Q2) |
| firewalld zones, open ports, nftables | ? | Where the IP allowlist lives (see 2) |
| CloudDeck agents on the VM | ? | Must not be broken; the bundle hints at a WebRTC streaming agent (`*.cldk.me`) |

---

## 2. CloudDeck API

Documented in [clouddeck-api.md](clouddeck-api.md). Summary:

- **Auth:** AWS Cognito, `USER_PASSWORD_AUTH`, pool `eu-central-1_DQtJnTkIf`.
  The API takes `Bearer <AccessToken>`. Refresh tokens work, so **we never store
  the password**.
- **Endpoints:** `GET /accounts/{id}` → `machine_id`; `GET /machines/{id}`;
  `POST /machines/{id}/start {session?, keyboard_layout?}`; `POST …/stop`;
  `POST …/clients {pin}` (pairing). There is no restart endpoint (we do stop, then start).
- **Danger:** `POST /machines/{id}/reset` is a **factory wipe**. The client uses a
  path allowlist and can never call it.
- **Host address** is deterministic: `machine.id[2:] + ".my.clouddeck.app"`.
- **Auto-pairing is feasible with no host changes:** the client generates the
  PIN, starts the Moonlight pairing handshake, and POSTs the PIN to the portal
  at the same time.
- **IP allowlist:** the portal text confirms that the starting device's network
  is what's allowed. Whether it's enforced on the VM or upstream is
  **the most important open question**, because it decides which ports the agent
  and extra Sunshine instances can use.

**Go** for the Phase 1 CloudDeck integration. Remaining risk: undocumented API
changes, contained by the `CloudProvider` module plus fixture tests.

---

## 3. Feature feasibility

### 3.1 Microphone passthrough: **GO** (high confidence)

**Prior art studied**
- LizardByte/Sunshine **PR #4078** (closed unmerged, 2026-03-14). Maintainer
  guidance from the review: configure via **RTSP/SDP, not the control stream**;
  the client sends **RTSP PLAY** for the mic stream; **AES-GCM** encryption;
  keep the protocol at two ports; mic packets must be distinguishable from
  20-byte ping packets; carry a client identifier outside the encrypted payload
  (for multiple clients); optional FEC/duplication.
- `logabell/moonlight-common-c-mic` (GPL-3.0): advertises with
  `a=rtpmap:96 opus/48000/{1,2}` in DESCRIBE, then `SETUP streamid=mic` (port in
  the `Transport` reply, fallback 47996), `PLAY streamid=mic`, an
  `X-SS-Ping-Payload` header, and an `SS_ENC_MICROPHONE` bit in
  `x-ss-general.encryption{Supported,Requested}`. Packets use an RTP-like header,
  are **AES-CBC** encrypted (no integrity) with an IV of `riKeyId + seq`, and
  carry 20 ms frames. The matching hosts are logabell's Apollo/Vibepollo forks
  (Windows).
- AlkaidLab/foundation-sunshine (GPL-3.0) supports mic, **but only on Windows hosts**.
- `JimothySnicket/moonlight-mic`, `AorsiniYT/moonmic` (MIT), and other side-channel
  UDP designs: plaintext and new ports, so rejected.

**Design**
1. **Negotiation (backward compatible):** the patched Sunshine adds to the SDP
   `a=x-ss-mic.version:1` and `a=rtpmap:96 opus/48000/1`, and sets a new
   `SS_ENC_MICROPHONE` bit. Before finalizing, we align the bit value and the
   `streamid=mic` SETUP/PLAY flow with logabell's fork, so either side
   interoperates with that fork family where possible. A stock client never sees
   anything new; a stock host never advertises it, and the client hides the mic
   UI.
2. **Transport, with no new port:** `SETUP streamid=mic` returns the **existing
   audio port** (UDP 48000) in `Transport: server_port=`. The client sends mic
   packets from its existing audio socket, which has already punched the NAT
   path with audio pings.
3. **Packet:** `u8 type=0x4D ('M') | u8 version=1 | u16 flags | u32 sessionTag |
   u64 seq | u32 rtpTimestamp | ciphertext | 16-byte GCM tag`.
   - The type byte and the minimum length (>32 bytes) distinguish it from the
     20-byte pings.
   - `sessionTag` is a 32-bit value from the RTSP SETUP reply. It lets one host
     port demultiplex several clients.
4. **Crypto:** AES-128-GCM keyed with the session's launch key (`rikey`, the
   key the existing streams use). Nonce = `"MIC1"` (4 bytes) ‖ `seq` (8 bytes,
   strictly increasing, never reused per session). The 20-byte header is AAD.
   Packets that are replayed or reordered beyond a 64-packet window are dropped.
   *Open item:* confirm that no other stream derives a GCM nonce with the same
   prefix from `rikey`. If one does, derive a separate mic key with
   HKDF-SHA256(rikey, "clouddeck-mic-v1") instead.
5. **Codec:** Opus, 48 kHz mono, 20 ms frames, 32 kbps VBR, in-band FEC enabled,
   `packet_loss_perc` driven by the existing loss stats. An optional
   "duplicate packets" setting covers lossy Wi-Fi.
6. **Host sink:** patched Sunshine decodes the audio (it already links libopus)
   and plays it into a PipeWire node. A user-level drop-in in
   `~/.config/pipewire/pipewire.conf.d/` (no rpm-ostree) creates a loopback whose
   capture side is `Audio/Sink` "CloudDeck Mic (internal)" and whose playback
   side is **`Audio/Source` "CloudDeck Microphone"**. Games, Steam, and Discord
   see that as a normal mic. It's silent when no client is connected.
7. **Client capture:** WASAPI shared mode on Windows (Qt Multimedia or SDL3
   recording, both of which wrap it), PipeWire/Pulse via SDL3 on Linux, and
   `AudioRecord(VOICE_COMMUNICATION)` with `AcousticEchoCanceler` on Android.
   Opus encoding uses the libopus already bundled by both clients.

**Risks:** Sunshine runs as the desktop user on Bazzite, so the PipeWire node
must live in that user's session (fine). Echo can occur when the host's audio
plays out of local speakers into the mic (platform AEC on Android; desktop
relies on OS/hardware AEC, which a headset avoids). Upstream divergence is
limited because the patch stays small and self-contained.

**Fallback:** if Sunshine patching proves fragile across updates, the agent does
the decoding instead. The RTSP negotiation stays in Sunshine (a minimal patch);
Sunshine forwards the decrypted Opus frames to the agent over a Unix socket.

**Depends on inventory:** PipeWire version, how Sunshine is launched (user unit).

### 3.2 USB passthrough (USB/IP): **GO, conditional**

**Design** (as in the brief, with two refinements that avoid installing anything
on the host image):
- **Host attach without the `usbip` binary:** the agent does the
  `OP_REQ_IMPORT` / `OP_REP_IMPORT` handshake itself, then writes
  `"<port> <sockfd> <devid> <speed>"` to
  `/sys/devices/platform/vhci_hcd.0/attach`. That's exactly what `usbip attach`
  does, so we don't need `usbip-utils` layered on Bazzite.
- **Transport:** the client dials the agent over TLS (mTLS with the client's
  Sunshine pairing certificate; the agent reads the paired-client cert list from
  Sunshine's state file and accepts only those). The kernel can't speak TLS, so
  the agent terminates TLS and bridges the plaintext into a `socketpair()`, and
  hands one end of that pair to vhci. Each device gets its own TLS connection
  (one USB/IP connection per device, as the protocol expects).
- **Linux client:** a small helper (`clouddeck-usb-helper`, polkit action
  `app.clouddeck.usb.export`) binds the device to `usbip-host` through
  `/sys/bus/usb/drivers/usbip-host/{match_busid,bind}` and writes the
  bridged socket fd to `/sys/bus/usb/devices/<busid>/usbip_sockfd`. Unbinding on
  disconnect, or when the helper is killed, gives the device back to its driver.
- **Windows client:** usbipd-win (GPL-3.0, signed MSI by its author), installed
  on demand. The client connects to `localhost:3240` and bridges that
  connection into the TLS tunnel. `usbipd bind` needs admin: one UAC prompt per
  new device, remembered per device. usbip-win2 (BSD-2) is a client, so it's not
  used.
- **Android client:** a userspace server based on cgutman/USBIPServerForAndroid
  (GPL-3.0; last pushed 2024, so expect to maintain it ourselves).

**Risks**
1. **Firewall:** if the allowlist and port forwarding live upstream and only
   Sunshine ports are open, the agent needs a port we can reuse. Candidate:
   TCP 47990 (the Sunshine web UI), with the web UI moved to localhost-only
   behind the agent. *Depends on the port probe.*
2. `vhci-hcd` missing from the Bazzite kernel. Fedora ships
   `CONFIG_USBIP_VHCI_HCD=m`, so this is unlikely. *Depends on the inventory.*
3. WAN latency: bulk and interrupt devices (gamepads, dongles, flight sticks,
   storage, license keys) work well. Isochronous devices (webcams, USB audio)
   will be poor; the UI warns about this.
4. Windows: usbipd-win is a separate installed product with its own driver. We
   bundle its MSI or download it on demand (pinned version plus hash).

**Fallback:** if the vhci modules are absent, loading an out-of-tree module would
need rpm-ostree, so that's no-go without your OK. If ports are blocked and 47990
can't be reused, tunnel the traffic through the Sunshine control channel. That's
much harder; treat it as no-go in that case.

### 3.3 Multiple monitors: **PROVISIONAL GO via fallback; native N-instance depends on inventory**

**Facts that hold regardless of the inventory**
- gamescope (Game Mode) drives exactly one output, so multi-monitor needs the
  **Plasma Desktop session** (`session: "desktop"` in `/start`).
- Sunshine can run several instances, each with its own config, state, base
  `port`, and `output_name`. Sunshine can disable input per instance
  (`keyboard`/`mouse`/`controller = disabled`).

**Design A, native (target):**
- N virtual outputs on the host, one Sunshine instance per output, managed by
  the agent.
- Candidate output sources, in order of preference:
  1. **KWin virtual outputs** (Plasma 6 `zkde_screencast` "virtual output",
     streamed via PipeWire). This needs a Sunshine capture backend that consumes
     a PipeWire/portal stream. Check whether the installed Sunshine has one;
     otherwise we patch one in.
  2. **EDID override** on unused GPU connectors (`drm.edid_firmware=` plus
     `video=<conn>:e`). This needs `rpm-ostree kargs` (**asks you first**) and a
     GPU with spare connectors, and works with KMS capture.
  3. **vkms:** no hardware acceleration and cross-GPU copies, so last resort only.
- **Input:** only the primary instance accepts input. The client maps absolute
  coordinates from any window into the combined layout. Sunshine is patched so
  the primary instance's absolute-pointer range spans the whole virtual desktop
  rather than one output. The cursor then crosses windows seamlessly. Audio only
  from the primary.
- **Needs extra ports** (each instance has its own port block), so this depends on
  the firewall finding.

**Design B, fallback (works even with only Sunshine's ports open):**
- **One** Sunshine instance captures one **wide virtual output** (for example
  3840×1080 for 2×1080p, or 5120×1440 for 2×1440p). The client splits the
  decoded frame into one borderless window per local monitor.
- Pros: one stream, one port set, trivial input mapping, and frame-locked
  monitors.
- Cons: both monitors must share a refresh rate and height. Encoder width limits
  apply (H.264 is capped at 4096 px wide, so it uses HEVC/AV1 for widths up to
  8192). One large stream also costs more bitrate.
- Plasma would see the remote desktop as one ultrawide display (taskbar
  spanning), unless we create two virtual outputs and capture the union.

**Recommendation:** build B first (Phase 4a), because it has the fewest
dependencies and gives real value; build A afterwards (Phase 4b) if the
inventory and port probe allow it. The Android "remote screen" stretch goal
needs design A.

---

## 4. Phased plan with acceptance criteria

Each phase ends with STATUS.md and CHANGELOG updates and a tagged release.

**Phase 0 (this doc).** Done when: the inventory is filled in, the API is
verified live (read-only, plus one start/stop with your permission), ports are
probed, and you have approved this plan.

**Phase 1: MVP**
1. Subtree-import moonlight-qt v6.2.0 → `client-desktop/`, moonlight-android
   v12.2 → `client-android/`, and moonlight-common-c → `common/` (ADR-0001).
   Both clients build against `common/`.
   *AC:* unmodified upstream builds pass in CI on all three platforms.
2. Rebrand (name, app ID, icons, update URL) so the app can sit alongside stock
   Moonlight. *AC:* both apps install side by side on each OS.
3. CI matrix: Windows MSI and portable zip, Linux AppImage, Android
   arm64-v8a/x86_64 debug APK. Tags publish a GitHub Release. *AC:* tag `v0.1.0`
   produces all artifacts; a fresh Windows VM installs and runs the build.
4. `CloudProvider` in C++ and Java with shared fixture tests, and token storage
   in the keychain (QtKeychain on desktop; Android Keystore plus
   EncryptedSharedPreferences). *AC:* unit tests pass; after sign-in the
   password appears nowhere on disk; restarting the app restores the session.
5. VM card (live status, Start/Stop/Restart) and "Start and connect" with
   progress and timeouts (start ≤ 5 min, Sunshine reachable ≤ 2 min after
   `running`). *AC:* against the real VM, one click from `off` reaches a streaming
   session with no address typing.
6. Auto-pair. *AC:* after "reset pairings", one click re-pairs with no PIN
   typed.
7. Optional auto-stop (on session end, or after N idle minutes). *AC:* the VM
   reaches `off` within N+1 min of idle.
8. No regressions. *AC:* a manual matrix (H.264/HEVC/AV1, HDR, 4K60/1080p120,
   gamepad, touch) passes on Windows, Linux, and Android.

**Phase 2: Mic.** Sunshine patch series plus host install script, and client
capture/UI. *AC:* Discord on the host hears the client mic with E2E latency
< 150 ms on a 30 ms RTT link. Wireshark shows only GCM ciphertext on 48000/UDP.
A stock Sunshine host shows no mic UI and nothing breaks. Protocol unit tests
pass.

**Phase 3: USB.** Agent (TLS, mTLS, vhci attach), Linux helper, Windows
usbipd-win integration, Android server. *AC:* an Xbox controller and a USB flash
drive forwarded from each client OS enumerate on the host. Killing the client
detaches within 5 s, and the device returns to the local OS.

**Phase 4a: Multi-monitor (wide stream).** *AC:* 2×1080p60 local monitors, each
showing its half, with the cursor crossing seamlessly. **4b (native):** *AC:* 2
independent outputs at different refresh rates.

**Phase 5: Polish.** Quick Menu, network test, clipboard, file upload, shortcuts,
auto-reconnect, tray, Android virtual gamepad. Each gets its own AC in its
phase plan.

## Decisions needed from you

1. SSH access to the VM (host, port, user, key) for the read-only inventory.
2. Permission for live API verification: read-only calls, plus one
   start → probe → stop cycle (and whether starting costs money or counts against
   usage on your plan).
3. Approval or adjustment of this plan (especially building the 4a fallback before 4b).
