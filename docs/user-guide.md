# User guide

How to use CloudDeck-Client on Windows, Linux (including the Raspberry Pi) and Android.
What's verified on each platform is in [STATUS.md](STATUS.md).

**Contents:**
- [At a glance](#at-a-glance)
- [Install and first start](#install-and-first-start)
- [Home screen](#home-screen)
- [Settings](#settings)
- [In-stream menu](#in-stream-menu)
- [Controllers](#controllers)
- [Microphone](#microphone)
- [USB devices](#usb-devices-windows-and-linux)
- [Display and multiple monitors](#display-and-multiple-monitors-windows-and-linux)
- [Clipboard](#clipboard-windows-and-linux)
- [Network test](#network-test-windows-and-linux)
- [Shortcuts](#shortcuts-windows-and-linux)
- [Troubleshooting](#troubleshooting)

## At a glance

| I want to… | Do this |
|---|---|
| Start streaming | Press **Connect** on Home. It starts the VM if needed. |
| Change the resolution | Use the Resolution chip on Home (desktop) or the Resolution tile (Android). In a stream, use the menu. |
| Open the menu in a stream | Click the round button, press Ctrl+Alt+Shift+O, or press Select+L1+R1+B on a controller. |
| Use a controller as a mouse | Press Select+L1+R1+A, or use the *Controller* tile in the menu. |
| Type Esc, F-keys or Ctrl on a touchscreen | *Full keyboard* in the menu, or Select+L1+R1+Y. |
| Use my microphone on the VM | Turn on the microphone on Home and choose *CloudDeck Microphone* on the VM. |
| Save money | Turn on *Stop my CloudDeck when the stream ends*. |
| Check for a new version | Open **About**: on desktop it's under *Sign out*, on Android on the Account page. |

## Install and first start

1. Install the client from the [latest release](../../../releases/latest):
   - Windows: the MSI or the portable zip.
     - The MSI installs for your user only, into `%LOCALAPPDATA%\Programs\CloudDeck Client`, and needs no admin rights.
     - If you have 1.0.0 installed, uninstall it first; it was installed for all users.
   - Linux: the AppImage (x86_64, or aarch64 for a Raspberry Pi 4/5 with a 64-bit OS). Run `chmod +x` on it, then start it.
   - Android: the APK.

   It installs next to stock Moonlight and shares nothing with it.
2. Sign in with your CloudDeck email and password. The password goes to CloudDeck's login once
   and is never stored. A session token is kept in your OS keychain:
   - Windows: Credential Manager
   - Linux: Secret Service (GNOME Keyring or KWallet)
   - Android: Keystore

   Without a keychain, you sign in on every launch.
3. Press **Start and connect**. The client:
   1. starts your CloudDeck (about 1–2 minutes);
   2. waits until its address and streaming host answer;
   3. pairs automatically the first time (no address to copy, no PIN to type);
   4. launches the `CloudDeck` app and streams.

For microphone, USB, clipboard, display switching and the network test, install the server side
on the VM once ([host-setup.md](host-setup.md)). Streaming works without it.

## Home screen

- **Connect:** the round button. It skips any steps that are already done.
- **Tiles:**
  - Start, Stop (asks first) and Restart. Restart stops the VM, waits until it's off, then starts it again.
  - Controller profiles.
  - Resolution on Android.
- **Quick chips:**
  - Desktop: resolution, display mode, microphone, clipboard, USB devices, auto-reconnect.
  - Android: microphone, on-screen controller.
- **Your CloudDeck:** live status: Running, Stopped, Starting or Stopping.

The client never offers CloudDeck's **Reset** (factory wipe); it can't send that request at all.

Everything works with touch, mouse, keyboard, a gamepad or a TV remote:

| Input | Does |
|---|---|
| D-pad or arrow keys | Move between controls. A ring shows where you are. |
| A, Enter or Space | Press the control. |
| B, Back or Escape | Back to the sidebar, then to Home. |
| Start, Menu or X | Open the settings. |

On desktop, left and right move between the sidebar and the page. On phones the sidebar
becomes a strip of chips under the header.

## Settings

All of Moonlight's streaming settings are part of these pages; there's no second settings menu.

**Desktop**

| Page | What's there |
|---|---|
| Video | Quality (resolution, frame rate, bitrate, low bandwidth), the CloudDeck's screen (fit to window, several monitors), display mode, picture (HDR, V-Sync, frame pacing) |
| Audio | Speakers (stereo or surround, muting) and microphone (device, test) |
| Controls | Tabs: **Keyboard & mouse** (mouse mode, system shortcuts, in-stream shortcuts, touchscreen), **Gamepad** (button layout, remapping, controller as mouse, on-screen controller), **USB devices** (devices to forward) |
| Network | Connection test, recommended settings, reconnect automatically |
| General | Language, appearance, stop when the stream ends or when idle, clipboard, stats, app to launch. *Show advanced settings…* reveals codec, renderer and the rest |

**About** (the link under *Sign out*) shows the version and has *Check for updates*, GitHub and
*Buy me a coffee*.

**Android**

| Page | What's there |
|---|---|
| Home | Connect, tiles, quick toggles |
| Stream | Moonlight's streaming settings |
| Devices | Controllers (layout, on-screen controller, controller as mouse), microphone |
| Account | Stop when the stream ends, account, other hosts (*Manage PCs*), About |

**Useful options:**
- *Stop my CloudDeck when the stream ends:* turns the VM off as soon as you leave the stream.
- *Stop when idle* (desktop): turns the VM off if you haven't streamed for the chosen time (15 minutes to 4 hours).
- *Reconnect automatically* (desktop, on by default):
  - after a network drop, the stream reconnects in the same window, with up to 5 attempts over about 20 s;
  - your app keeps running, and clipboard and USB devices come back with the stream.
- *App to launch:* the Sunshine app name; `CloudDeck` by default.

## In-stream menu

**Opening it:**
- Click the round CloudDeck button at the top centre. It's dimmed while the stream holds your mouse;
  release the mouse (Ctrl+Alt+Shift+Z) or use a shortcut below to open the menu.
- Press **Ctrl+Alt+Shift+O** (desktop).
- Press **Select+L1+R1+B** on a controller.
- Press **Back** (Android).

**Closing it:** press Esc, click back into the stream, or press the same key again.
While it's open, keys and clicks go to the menu, not the CloudDeck.

| Tile or section | Does |
|---|---|
| Fullscreen (desktop) | Switch between window and fullscreen |
| Resolution | Restarts the stream at another size. Your app keeps running. |
| Stats | Performance overlay |
| Sound / Microphone | Mute the CloudDeck's sound; mute or choose your microphone |
| Controller | Switch controllers between *As gamepad* and *As mouse* |
| On-screen controller / pad | Show or hide it; on Android also edit, save and load layouts |
| Keyboard / Full keyboard | Typing keyboard, or the full one with Esc, F-keys and Ctrl |
| Display and mouse (desktop) | **Game mouse** is captured by the stream. **Desktop mouse** can leave the window. |
| USB devices (desktop) | Tick devices to move them to or from the CloudDeck right away |
| Reconnect, Disconnect, Shut down | Shut down needs a second click |

You can hide the menu button at the bottom of the menu; the shortcut keeps working.

## Controllers

- **Layout:** Xbox, or Nintendo (swaps A/B and X/Y). Set it in the settings (desktop:
  Controls → Gamepad; Android: Devices).
- **Remapping profiles** (desktop: Controls → Gamepad → Remapping → *Edit profiles…*; Android: *Controller profiles*):
  - send any button as another one, or disable it;
  - per stick: deadzone, sensitivity, invert up/down;
  - trigger deadzone and swapped sticks.

  Use one profile for all controllers or one per controller. A handheld's built-in controls
  count as a controller. Profiles apply from the next stream.
- **Controller as mouse** ([controller-mouse.md](controller-mouse.md)):
  - **Select+L1+R1+A** or the menu tile switches it.
  - As a mouse, the sticks move the pointer, A clicks, B right-clicks and the D-pad scrolls.
    On desktop, X middle-clicks.
  - Streams can also start in mouse mode, and holding Start can switch it too (both are options).
- **On-screen controller:** for touchscreens.
  - Xbox (A B X Y) or PlayStation (✕ ○ □ △) style.
  - On Android, edit the layout and save several of them by name.
  - On desktop (tablets, 2-in-1s, Linux phones), turn it on from the menu.
- **On-screen keyboard:** a full keyboard with Esc, F-keys, arrows, Ctrl, Alt, Win and Shift.
  - Tap a modifier to hold it for the next key (Ctrl, then C = Ctrl+C); double-tap to keep it held.
  - Keys are sent like a real keyboard, so they also work in games.
- **Combos:**

  | Combo | Does |
  |---|---|
  | Select + L1 + R1 + B | CloudDeck menu |
  | Select + L1 + R1 + A | Controller as mouse / gamepad |
  | Select + L1 + R1 + Y | On-screen keyboard |
  | Select + L1 + R1 + X | Performance stats |
  | Start + Select + L1 + R1 | Disconnect |

## Microphone

Needs the server side on the VM.

1. On Home, turn the microphone on.
   - Desktop: pick a device under Audio and press **Test** to see the level.
   - Android: allow the permission when it asks.
2. Start a stream. On the VM the microphone is **CloudDeck Microphone**. Choose it in Discord,
   Steam or the game if it isn't picked automatically.
3. Mute it from the menu, or with **Ctrl+Alt+Shift+U** on desktop.

The audio is encrypted with the stream's session key. Without the server side, the setting
shows *Host does not support the microphone* and streaming works as usual.

## USB devices (Windows and Linux)

Needs the server side on the VM, plus:
- **Windows:** [usbipd-win](https://github.com/dorssel/usbipd-win). The client offers *Install usbipd-win*.
- **Linux:** the `usbip-host` kernel module and `pkexec`.
  - Fedora ships the module; on Ubuntu it's in `linux-modules-extra`.
  - You're asked for your password once per stream that forwards devices.

**Using it:**
1. Under Controls → USB devices, tick each device you want on your CloudDeck. The first time, Windows asks
   for administrator approval to share the device with usbipd.
2. Start a stream. Ticked devices move to the VM and come back when the stream ends.
3. While a device is forwarded, you can't use it on this PC.

**Limits:**
- Don't forward the keyboard or mouse you're using.
- Webcams and USB audio may work poorly over the internet.

## Display and multiple monitors (Windows and Linux)

Needs the server side on the VM.

- **Window or fullscreen:** set *Display mode* on Home or under Video. During a stream, press
  **Ctrl+Alt+Shift+X**.
- **Fit the CloudDeck's resolution to the stream window:** the VM's desktop takes the exact size
  of your window, or of your screen in fullscreen, so nothing is scaled.
  - After you resize or toggle fullscreen, the stream restarts at the new size about a second
    later. Your app keeps running and USB devices stay attached.
  - When the stream ends, the VM goes back to its previous resolution.
  - With this on, the Resolution setting is ignored.
  - Exclusive *Fullscreen* doesn't follow resizes; use *Borderless windowed* or *Windowed*.
- **Several monitors:** under Video → *Use several monitors*, tick the monitors to use. The stream becomes one wide desktop (e.g.
  3840×1080 for two 1080p monitors) in a borderless window across exactly those monitors.
  - The VM sees **one ultrawide screen**:
    - fullscreen games and *maximize* stretch across all the monitors;
    - the taskbar spans them all;
    - use window snapping for apps, and single-monitor mode for games.
  - The monitors need the same display scaling (e.g. all 100%).
  - Monitors of different heights leave areas of the VM's desktop off-screen.
  - Above 4096 pixels wide, HEVC or AV1 is used.
  - Limit: about 5120×1440 at 60 FPS, or 3840×1080 at 120 FPS.
  - The fullscreen toggle does nothing in this mode; quit with **Ctrl+Alt+Shift+Q**.

## Clipboard (Windows and Linux)

Needs the server side on the VM. On by default:
- Text you copy on your PC, also in other apps while the stream runs, can be pasted on the
  CloudDeck, and the other way round.
- When a stream starts, your PC's clipboard is copied to the CloudDeck.
- Text only, up to 100 KB. Images and files aren't shared.
- On the VM it needs Plasma's clipboard (*Clipboard* in the system tray), which is there by default.

## Network test (Windows and Linux)

Under **Network**, with your CloudDeck running:
- **Test my connection** measures latency and speed in a few seconds and uses a few tens of MB.
- **Apply recommended settings** sets resolution, frame rate and bitrate for your connection
  and screen. For example, 1080p, 60 FPS and 20 Mbit/s on a 1920×1200 laptop with a fast line.
- **Low bandwidth** (under Video → Quality): 1280×720, 30 FPS, 3 Mbit/s, for hotel Wi-Fi or hotspots.

## Shortcuts (Windows and Linux)

All in-stream shortcuts are **Ctrl+Alt+Shift + a key**. To change the keys, go to
Controls → Keyboard & mouse → *Change shortcuts…*. Changes apply from the next stream.

| Key | Action |
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

To send Win or Alt+Tab to the CloudDeck, set *Send system shortcuts like Alt+Tab to the
CloudDeck* to *Always* or *In fullscreen* (Controls → Keyboard & mouse).

## Troubleshooting

- **"streaming host is not reachable":** CloudDeck only lets in the network the VM was started
  from. If you started it from another device or network, press Restart from this one.
- **Stuck on "Waiting for network address":** your DNS still has the address from when the VM
  was off. After 15 seconds the client connects to the VM's IP address directly. If it still
  fails, wait a minute and try again.
- **Windows MSI says a newer or other version is installed:** uninstall CloudDeck Client from
  *Settings → Apps* first (1.0.0 was installed for all users), then run the new MSI.
- **The VM features are missing** (microphone, USB, clipboard): install or update the server
  side ([host-setup.md](host-setup.md)). Check `~/.config/clouddeck-client/last-healthcheck` on the VM.
- **Something else:** *Manage PCs* opens the classic Moonlight host list. If your release is
  public, open an issue on GitHub with the client log.
