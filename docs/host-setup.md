# Host setup (CloudDeck VM)

SSH to the VM is blocked by CloudDeck's upstream filter, so you run these steps
yourself, in a terminal on the VM (Konsole, inside a stream).

## What gets installed

A patched Sunshine, built from the **same version** as the stock one
(2026.914.233613). It runs *instead of* the stock binary through a launcher.
Current patches:

| Patch | Adds |
|---|---|
| 0001 | Encrypted microphone stream. The first stream with the mic on creates the PipeWire source **CloudDeck Microphone**, which then persists until reboot. Disable with `systemctl --user set-environment CLOUDDECK_MIC=0` followed by a Sunshine restart. |
| 0002 | Tunnel from Sunshine's HTTPS port to the host agent, for paired clients only. |

Plus the **host agent** (`clouddeck-agent.service`, root). It handles:
- USB passthrough (ADR-0006);
- switching the display mode during multi-monitor or "fit to window" streams
  (ADR-0007), restored when the stream ends;
- clipboard sync through Plasma's Klipper (ADR-0008);
- the network test (ADR-0011).

Logs: `journalctl -u clouddeck-agent -b`.

To check by hand that KWin accepts a wide mode, run `./display-test.sh 3840 1080 60`
(as yourself, not with sudo) in Konsole inside a stream, from the unpacked
bundle. You don't need to install anything first. It switches for 15 seconds
and always switches back.

Nothing in the read-only OS image changes, and stock Sunshine, its config, and
your pairings stay as they are. The full list is in
[host-changes.md](host-changes.md).

**Fail-safes:** the VM stays streamable even if the patched build is broken. The
launcher starts **stock** Sunshine instead when:
- a CloudDeck image update changes the Sunshine version,
- the patched build crash-loops (5 starts in 2 minutes),
- the post-install health check fails, or
- you disable it by hand (`touch ~/.config/clouddeck-client/sunshine-disabled`).

## Install

1. In the VM's browser (signed in to GitHub), open the repo's **Releases** and
   download `clouddeck-host-*.tar.gz` and its `.sha256` from the newest
   release that has them (currently *Phase 5 network test 1*).
2. In Konsole:
   ```sh
   cd ~/Downloads
   sha256sum -c clouddeck-host-*.tar.gz.sha256
   tar xzf clouddeck-host-*.tar.gz
   cd clouddeck-host-*/
   sudo ./install.sh
   ```
3. About 15 s later Sunshine restarts and **your stream drops**. Reconnect with
   CloudDeck Client.
4. Check the result:
   ```sh
   cat ~/.config/clouddeck-client/last-healthcheck      # "ok ..." expected
   journalctl --user -u app-dev.lizardbyte.app.Sunshine -b | grep clouddeck-client
   ```
   `starting PATCHED Sunshine` means the patched build is active.
   `starting STOCK Sunshine (...)` shows why it fell back.

Rerunning `install.sh` is safe: it only changes what's different.

## Uninstall

```sh
sudo /var/opt/clouddeck-client/uninstall.sh            # back to stock, keep files
sudo /var/opt/clouddeck-client/uninstall.sh --purge    # also delete everything we added
```

## After a CloudDeck image update

If CloudDeck ships a newer Sunshine, the launcher sees the version mismatch and
runs stock Sunshine. Streaming keeps working; only the CloudDeck-Client host
features (mic etc., once added) pause. To get them back, the patch series gets
rebased on the new version (`host/sunshine-patches/UPSTREAM_TAG`), then you
install the new bundle.
