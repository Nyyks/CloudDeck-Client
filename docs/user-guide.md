# User guide

> Status: early Phase 1 build. See [STATUS.md](STATUS.md) for what's verified.

## First start

1. Install it: the MSI or portable zip on Windows, the AppImage on Linux, the
   APK on Android. It installs alongside stock Moonlight and shares nothing with it.
2. Sign in with your CloudDeck email and password. The password is sent to
   CloudDeck's login service once and never stored. A session token is kept in
   your OS keychain:
   - Windows: Credential Manager
   - Linux: Secret Service (GNOME Keyring / KWallet)
   - Android: Keystore

   If no keychain is available, you sign in on each launch.
3. Press **Start and connect**. The app:
   - starts your CloudDeck (about 1–2 minutes),
   - waits until its address and streaming host are reachable,
   - pairs automatically the first time: no address to copy, no PIN to type,
   - launches the `CloudDeck` app and starts streaming.

## Buttons

| Button | Does |
|---|---|
| Start and connect / Connect | Everything above. Skips the steps that are already done. |
| Start / Stop | Power the VM on or off (Stop asks first). |
| Restart | Stop, wait until it's off, start again. CloudDeck has no restart call. |
| Manage PCs | The classic Moonlight host list, for other hosts or troubleshooting. |
| Sign out | Forgets the session token. |

The app never offers CloudDeck's **Reset** (factory wipe). It can't send that
request at all.

## Options

- **Stop my CloudDeck when the stream ends:** turns the VM off as soon as you
  leave the stream.
- **Stop after idle minutes** (desktop): turns the VM off if you haven't
  streamed for N minutes. 0 = never.
- **Reconnect automatically if the connection drops** (desktop, on by
  default): after a network drop the stream reconnects in the same window.
  - It makes up to 5 attempts over about 20 s, showing *Reconnecting (n of 5)*
    under the menu button.
  - Your app keeps running on the CloudDeck.
  - Clipboard sync and forwarded USB devices come back with the stream.
- **App to launch:** the Sunshine app name. The default is `CloudDeck`.

## Microphone

Needs the CloudDeck host update on the VM ([host-setup.md](host-setup.md)).

1. On the CloudDeck screen, turn on **Send my microphone to the CloudDeck**.
   On desktop, pick a device and press **Test** to check the level meter.
   Android asks for the microphone permission.
2. Start a stream. On the VM, the microphone appears as **CloudDeck Microphone**;
   choose it in Discord, Steam or the game if it isn't picked automatically.
3. During a stream on desktop, **Ctrl+Alt+Shift+U** mutes and unmutes (the key is
   configurable), or set up push-to-talk (see below).

The audio is encrypted with the stream's session key. If the host doesn't
have the update, the setting shows *Host does not support the microphone* and
streaming works as usual.

## USB devices (Windows and Linux)

Needs the CloudDeck host update on the VM, plus:
- **Windows:** [usbipd-win](https://github.com/dorssel/usbipd-win) (the app offers
  **Install usbipd-win**).
- **Linux:** the kernel's `usbip-host` module (Fedora ships it; on Ubuntu it's in
  `linux-modules-extra`) and `pkexec`. When a stream with forwarded devices
  starts, you're asked for your password once.

1. On the CloudDeck screen under **USB devices**, check each device you want on
   your CloudDeck. The first time, Windows asks for administrator approval to
   *share* the device with usbipd.
2. Start a stream. Checked devices move to the VM ("on your CloudDeck") and
   come back to your PC when the stream ends.
3. While forwarded, a device can't be used on this PC. Don't forward the
   keyboard or mouse you're using. Webcams and USB audio may work poorly over
   the internet.

## In-stream menu (desktop)

During a stream there are two ways to open the menu:
- Click the round **CloudDeck button** at the top centre. It's shown while your
  cursor is free: *Desktop* mouse mode, or after Ctrl+Alt+Shift+Z.
- Press **Ctrl+Alt+Shift+O**, in any mode.

Click back into the stream, press Esc, or press the hotkey again to close it.

| Section | What you can change |
|---|---|
| Display | Fullscreen; fit the CloudDeck's resolution to this window; performance stats |
| Mouse | **Game**: captured by the stream, for games. **Desktop**: a free cursor that can leave the window, for working. Remembered for next time. |
| Audio | Mute the CloudDeck's sound. Send my microphone (turning it on or off reconnects for a second). Mute my microphone. Choose the microphone. |
| USB devices | Tick or untick devices. They move to or from the CloudDeck right away. |
| Session | Reconnect; disconnect; shut down my CloudDeck (click twice) |

While the menu is open, your keys and clicks go to the menu, not the
CloudDeck. You can hide the button at the bottom of the menu; the hotkey keeps
working.

## Shortcuts and push-to-talk (desktop)

All in-stream shortcuts are **Ctrl+Alt+Shift + a key**. On the CloudDeck screen,
*Shortcuts during a stream → Change…* lets you pick the key for each one.

| Default | Action |
|---|---|
| O | Open the CloudDeck menu |
| Q | Disconnect |
| E | Disconnect and close the app |
| X | Fullscreen / window |
| U | Mute / unmute the microphone |
| Z | Release / capture the mouse |
| M | Game / desktop mouse mode |
| S | Performance stats |
| C | Show / hide the local cursor |
| D | Minimize the stream |
| V | Type the clipboard text |
| L | Keep the mouse in the window (desktop mode) |
| K | Capture system keys (Win, Alt+Tab) |

**Push-to-talk:** choose a key or a mouse side button. While streaming with the
microphone on, your voice is only sent while you hold it, and the CloudDeck
doesn't see the key. Changes apply to the next stream.

## Network test (desktop)

On the CloudDeck screen under **Network**, with your CloudDeck running:
- **Test my connection** measures the latency and the speed from your
  CloudDeck to this PC, in a few seconds and a few tens of MB.
- **Apply recommended settings** then sets the resolution, frame rate and
  bitrate that fit your connection and screen. Example: 1080p at 60 FPS and
  20 Mbit/s on a 1920×1200 laptop with a fast line.
- **Low-bandwidth mode** sets 1280×720 at 30 FPS and 3 Mbit/s, for hotel Wi-Fi
  or mobile hotspots.

You can change everything afterwards in Settings. With *Fit the CloudDeck's
resolution to the stream window* on, the window decides the resolution.

## Clipboard (desktop)

Needs the CloudDeck host update on the VM.

*Share the clipboard (text) with my CloudDeck while streaming* is on by default:
- Text you copy on your PC, also in other apps while the stream runs, can be
  pasted on the CloudDeck.
- Text you copy on the CloudDeck can be pasted on your PC.
- When a stream starts, your PC's clipboard is copied to the CloudDeck.

It's text only, up to 100 KB. Images and files aren't shared. It needs Plasma's
clipboard (the system tray's *Clipboard* entry), which is there by default.

## Display and multiple monitors (desktop)

Needs the CloudDeck host update on the VM.

- **Windowed mode:** Settings → *Display mode* → *Windowed*, or press
  **Ctrl+Alt+Shift+X** during a stream to switch between window and fullscreen.
- **Fit the CloudDeck's resolution to the stream window:** the CloudDeck's
  desktop takes the exact size of your stream window, or of your screen in
  fullscreen, so nothing is scaled or blurry.
  - When you resize the window or toggle fullscreen, the stream reconnects at
    the new size about a second after you let go, which takes a moment.
  - The app keeps running and USB devices stay attached.
  - When the stream ends, the CloudDeck goes back to its previous resolution.
  - With this on, the *Resolution* setting is ignored. A default bitrate
    follows the size.
  - *Display mode: Fullscreen* (exclusive) doesn't follow resizes. Use
    *Borderless windowed* or *Windowed*.
- **Use several monitors:** tick the monitors to use. The stream becomes one
  wide desktop, e.g. 3840×1080 for two 1080p monitors side by side. It's shown
  in one borderless window covering exactly those monitors, and the mouse moves
  across them as on a local PC.
  - The monitors need the same display scaling (e.g. all 100%).
  - Monitors of different heights work, but the CloudDeck desktop then has
    areas that are outside your screens.
  - Above 4096 pixels wide, HEVC or AV1 is used (H.264 can't go that wide).
  - Limit: about 5120×1440 at 60 FPS, or 3840×1080 at 120 FPS.

  On the CloudDeck it's one ultrawide screen: the taskbar spans all monitors,
  and *maximize* fills all of them (use window snapping instead). The
  fullscreen toggle does nothing while streaming to several monitors; quit with
  **Ctrl+Alt+Shift+Q**. To have Windows keys like Win or Alt+Tab go to the
  CloudDeck, set *Capture system keyboard shortcuts* to *always* in Settings.

## Troubleshooting

- *"streaming host is not reachable"*: CloudDeck only lets in the network the
  VM was started from. If you started it from another device or network,
  press Restart from this one.
- *"still does not point to your CloudDeck"*: your DNS resolver cached the
  address from when the VM was off. Wait a minute and try again.
