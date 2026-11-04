# Project brief: "CloudDeck-Client" — a Shadow-class client for my CloudDeck cloud PC

(Working name; rename if you find a better one. First action: save this entire brief verbatim to docs/BRIEF.md in the new repo so future sessions can re-read it.)

## Context
- I rent a cloud gaming VM from CloudDeck (clouddeck.app). It runs Bazzite (Fedora Atomic, immutable/rpm-ostree) with Sunshine as the streaming host.
- Today's workflow: log in to the CloudDeck web portal → click Start → wait → click Connect → copy the host address into Moonlight → Moonlight shows a PIN → I type the PIN into the portal to pair → stream. The portal notes the starting device and the streaming device must be on the same network, so starting the VM probably allowlists the caller's public IP. Verify this.
- I have full root access to the VM and may change anything server-side (Sunshine config, patched Sunshine builds, extra services, kernel modules, firewall). Ask me how to reach the VM (SSH details) when you need it.
- Goal: one client app, with release binaries for **Windows (x64), Linux (x64), and Android (arm64 + x86_64)**, that reaches feature parity with the Shadow PC client while streaming from my own Sunshine host.

## Non-negotiable approach
- Do NOT write a video streaming stack from scratch. Fork and extend the Moonlight ecosystem: moonlight-qt (Windows/Linux), moonlight-android, and the shared moonlight-common-c protocol library. Extend Sunshine on the host side (patches or a fork) plus a small companion host agent.
- These projects are GPLv3, so this project is GPLv3 as well. Keep LICENSE and THIRD_PARTY_NOTICES accurate. Before borrowing code from any other fork (Apollo/Vibepollo, Sunshine-Foundation, moonlight-vplus, moonlight-mic, Artemis, etc.), check its license and record it.
- Stay protocol-compatible with stock Sunshine where possible: every new capability must be negotiated (advertised via SDP/RTSP or the agent API) and degrade gracefully when the host doesn't support it.

## Repository and git
- Create a **private** GitHub repo with `gh repo create --private`. Never make it public or change its visibility without asking me.
- Monorepo layout (adjust if you find a better one, and record the decision in an ADR):
  - `client-desktop/` (moonlight-qt fork, via git subtree so upstream can be merged later)
  - `client-android/` (moonlight-android fork, subtree)
  - `common/` (moonlight-common-c fork, shared by both clients)
  - `host/agent/` (companion agent), `host/sunshine-patches/`, `host/install/` (Bazzite setup scripts)
  - `docs/`, `.github/workflows/`
- Commit whenever a self-contained step works (builds and its tests pass). Use small Conventional Commits (`feat(usb): …`, `fix(android): …`, `docs: …`). Push regularly. Use feature branches for big features and merge them when they're green.
- Never commit secrets: CloudDeck credentials, tokens, HAR captures, signing keys, pairing certificates. Add `.gitignore` entries and a gitleaks pre-commit hook on day one. Local secrets live in an untracked `.env` or the OS keychain.

## Phase 0 — Discovery and feasibility (stop and report to me before Phase 1)
Produce `docs/feasibility.md` covering:
1. **Host inventory:** Bazzite version, kernel, GPU and driver, Sunshine version and how it's installed/launched, session type (gamescope "Game Mode" vs KDE Plasma desktop, Wayland vs X11), PipeWire setup, whether the `usbip-core` and `vhci-hcd` kernel modules exist and load, firewalld zones and open ports, and whether CloudDeck runs its own provisioning or management agent on the VM (do not break it).
2. **CloudDeck API:** there is probably no public API. Reverse-engineer what the web portal uses for login, VM status, start, stop/restart, getting the connection address, and submitting the pairing PIN. I'll either log in in a browser for you to inspect, or give you a HAR export (keep it untracked). Document the endpoints, auth flow (cookies/JWT/refresh, 2FA if any), and the IP-allowlisting behaviour in `docs/clouddeck-api.md`. Put all of this behind a `CloudProvider` interface so a breaking portal change only touches one module.
3. **Feature feasibility:** for mic, USB, and multi-monitor, pick a concrete design, list the risks, and give a go/no-go with fallbacks.
4. A phased implementation plan with acceptance criteria. Then pause and ask me to approve or adjust it.

## Phase 1 — MVP (binaries early)
- Rebranded fork builds on all three platforms, with CI producing artifacts:
  - Windows: installer plus portable zip
  - Linux: AppImage (also consider a Flatpak, and document what its sandbox breaks, e.g. USB/IP)
  - Android: APK (debug-signed for now; keystore only in GitHub secrets)
- Use a GitHub Actions matrix. Trigger on push to main and on tags; tagged builds publish a GitHub Release.
- **CloudDeck integration:**
  - Sign in, with tokens stored in the OS keychain (Windows Credential Manager, libsecret/KWallet, Android Keystore). Never store the plain password if a refresh token works.
  - VM card showing live status, with Start / Stop / Restart buttons.
  - "Start and connect" waits until the VM is running and Sunshine is reachable, with progress and timeouts.
  - Fetch the host address automatically, and **pair automatically**: generate the PIN client-side and submit it through the portal API, so I never copy addresses or type PINs.
  - Optional auto-stop of the VM after the session ends or after N idle minutes.
- The stream itself inherits everything Moonlight already does (codecs, HDR, bitrate/fps, controllers, touch modes) and must not regress it.

## Phase 2 — Microphone passthrough
- Client capture: WASAPI on Windows, PipeWire/PulseAudio on Linux, AudioRecord on Android (VOICE_COMMUNICATION source with platform echo cancellation). Encode with Opus.
- Transport must be **encrypted** with the session's keys, like the other streams. Upstream Sunshine rejected plaintext mic proposals. Study upstream Sunshine PR #4078 and its discussion (advertise via SDP, client requests the mic stream via RTSP, avoid adding new ports), plus existing mic forks, before designing. Document the wire format in `docs/protocol-extensions.md`.
- Host: patched Sunshine (or the agent, if that's cleaner) decodes the audio into a PipeWire **virtual source** that games, Discord, and Steam see as a normal microphone.
- Client UI: input device picker, mute toggle with a hotkey, optional push-to-talk, and a level meter.

## Phase 3 — USB passthrough (USB/IP)
- The client exports devices as a USB/IP server, and the host attaches them via `vhci-hcd`.
  - Linux client: kernel `usbip-host` through a small privileged helper (polkit), not by running the whole app as root.
  - Windows client: usbipd-win, bundled or installed on demand (check license and signing). Note that usbip-win2 is a client and the wrong direction here.
  - Android client: a userspace USB/IP server on the Android USB Host API (study cgutman's UsbIpServerForAndroid).
- NAT: the host can't dial into the client. The client opens an outbound TLS connection to the host agent, the agent exposes it on localhost, and the host runs `usbip attach` against it. Authenticate with the existing Sunshine pairing certificate (mTLS) so there's no second login.
- UI: list local devices with checkboxes and per-device "always forward". Exclude the local keyboard and mouse by default. Warn that forwarded devices disappear locally, and that isochronous devices (webcams, USB audio) may perform poorly over WAN. Detach everything cleanly on disconnect or crash.

## Phase 4 — Multiple monitors
- Host: N virtual displays (start with 2, design for 4). Find what actually works on this GPU and session: EDID/connector overrides, KWin virtual outputs, or a dummy-display approach. This probably requires the KDE desktop session rather than gamescope; confirm in Phase 0. Each display is served by its own Sunshine instance (own config, state file, ports, output name), managed by the agent.
- Client: one stream window per remote display, placed on my chosen local monitors, each at that monitor's native resolution and refresh rate. Keyboard and mouse input from any window must drive one desktop so the cursor moves seamlessly across windows (map absolute coordinates into the combined layout). Audio only from the primary stream. Layout editor in settings.
- Stretch: "remote screen" mode, where a second device (e.g. an Android tablet) joins an active session as an extra monitor.

## Phase 5 — Shadow-style polish
- In-stream **Quick Menu** (hotkey on desktop, on-screen button on Android): fullscreen/windowed, cursor lock, mic mute, USB devices, monitor layout, stats overlay, restart stream, disconnect, and shut down VM.
- Network test with auto-recommended settings, plus a low-bandwidth mode.
- Two-way text clipboard sync. File drag-and-drop or upload to the host via the agent.
- Configurable shortcuts, auto-reconnect, tray icon on desktop, and multi-touch / virtual gamepad on Android.

## Host agent
- Single static binary (Rust or Go; justify the choice in an ADR). Runs as a systemd service.
- Respect Bazzite's immutability: no rpm-ostree layering unless unavoidable (ask me first). Install under /usr/local or /opt, with units in /etc/systemd/system.
- `host/install/install.sh` must be idempotent and have a matching uninstall. Back up every config it touches. The setup must survive Bazzite updates and reboots.
- Record every server-side change in `docs/host-changes.md`.

## Docs (keep them current; update docs in the same commit as the code)
- README (what this is, status, quick start)
- `docs/architecture.md` (with a diagram)
- `docs/feasibility.md`, `docs/clouddeck-api.md`, `docs/protocol-extensions.md`
- `docs/host-setup.md`, `docs/building.md`, `docs/user-guide.md`
- `docs/adr/` (one file per significant decision)
- `docs/STATUS.md`: a feature-parity matrix vs the Shadow client (done / partial / blocked / not planned)
- CHANGELOG.md

## How to work
- Plan before coding each phase. Keep a task list. Be honest in STATUS.md: never mark a stub or untested path as done.
- Test against the real VM wherever possible. Unit-test protocol code (packet formats, crypto framing, USB/IP messages).
- Stop and ask me before: changing repo visibility, anything on the VM that could leave it unbootable or unreachable, rpm-ostree changes, changing CloudDeck account settings, or any action that could cost money.
- When something turns out infeasible, say so early, propose the best fallback, and record it in an ADR.
