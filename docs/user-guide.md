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
- **App to launch:** the Sunshine app name. The default is `CloudDeck`.

## Microphone

Needs the CloudDeck host update on the VM ([host-setup.md](host-setup.md)).

1. On the CloudDeck screen, turn on **Send my microphone to the CloudDeck**.
   On desktop, pick a device and press **Test** to check the level meter.
   Android asks for the microphone permission.
2. Start a stream. On the VM, the microphone appears as **CloudDeck Microphone**;
   choose it in Discord, Steam or the game if it isn't picked automatically.
3. During a stream on desktop, **Ctrl+Alt+Shift+U** mutes and unmutes.

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

## Troubleshooting

- *"streaming host is not reachable"*: CloudDeck only lets in the network the
  VM was started from. If you started it from another device or network,
  press Restart from this one.
- *"still does not point to your CloudDeck"*: your DNS resolver cached the
  address from when the VM was off. Wait a minute and try again.
