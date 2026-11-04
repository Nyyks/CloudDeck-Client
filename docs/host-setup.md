# Host setup (CloudDeck VM)

SSH to the VM is blocked by CloudDeck's upstream filter, so you run these steps
yourself, in a terminal on the VM (Konsole, inside a stream).

## What gets installed (Phase 2, step 1)

A patched Sunshine, built from the **same version** as the stock one
(2026.914.233613). It runs *instead of* the stock binary through a launcher. With
no patches in the series yet, it behaves exactly like stock Sunshine; this step
only proves the install path. Nothing in the read-only OS image changes, and
stock Sunshine, its config, and your pairings stay as they are. The full list is
in [host-changes.md](host-changes.md).

**Fail-safes:** the VM stays streamable even if the patched build is broken. The
launcher starts **stock** Sunshine instead when:
- a CloudDeck image update changes the Sunshine version,
- the patched build crash-loops (5 starts in 2 minutes),
- the post-install health check fails, or
- you disable it by hand (`touch ~/.config/clouddeck-client/sunshine-disabled`).

## Install

1. In the VM's browser, download the latest **clouddeck-host-bundle** from the
   repo's Actions page (*Host* workflow, *Artifacts*). Later it will also be
   attached to releases.
2. In Konsole:
   ```sh
   cd ~/Downloads
   unzip clouddeck-host-bundle.zip          # GitHub wraps artifacts in a zip
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
