#!/usr/bin/env bash
# Undo host/install/install.sh: Sunshine goes back to the stock RPM exactly as before.
# A copy of this script is kept at /var/opt/clouddeck-client/uninstall.sh.
#
# Usage: sudo ./uninstall.sh [--purge] [--no-restart]
#   default  : remove the unit drop-in (stock Sunshine runs again); keep files for reinstall
#   --purge  : also delete /var/opt/clouddeck-client, /var/lib/clouddeck-client and the
#              user's ~/.config/clouddeck-client and ~/.local/state/clouddeck-client
set -euo pipefail

BASE=/var/opt/clouddeck-client
STATE_DIR=/var/lib/clouddeck-client
UNIT=app-dev.lizardbyte.app.Sunshine.service
PURGE=0
RESTART=1

die() { echo "ERROR: $*" >&2; exit 1; }
info() { echo "==> $*"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --purge) PURGE=1; shift ;;
        --no-restart) RESTART=0; shift ;;
        -h|--help) sed -n '2,8p' "$0"; exit 0 ;;
        *) die "unknown option $1" ;;
    esac
done
[ "$(id -u)" -eq 0 ] || die "run as root: sudo $0"

TARGET_USER=""
DROPIN=""
if [ -f "$STATE_DIR/install-state" ]; then
    TARGET_USER="$(sed -n 's/^user=//p' "$STATE_DIR/install-state")"
    DROPIN="$(sed -n 's/^dropin=//p' "$STATE_DIR/install-state")"
fi
[ -n "$TARGET_USER" ] || TARGET_USER="$(getent passwd 1000 | cut -d: -f1)"
TARGET_UID="$(id -u "$TARGET_USER")"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
[ -n "$DROPIN" ] || DROPIN="$TARGET_HOME/.config/systemd/user/$UNIT.d/zz-clouddeck-client.conf"

if [ -e "$DROPIN" ]; then
    BK="$STATE_DIR/backup/uninstall-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$BK"
    cp -a "$DROPIN" "$BK/" 2>/dev/null || true
    rm -f "$DROPIN"
    rmdir --ignore-fail-on-non-empty "$(dirname "$DROPIN")" 2>/dev/null || true
    info "removed $DROPIN (stock Sunshine will run)"
else
    info "no drop-in found; Sunshine already runs stock"
fi

as_user() {
    runuser -u "$TARGET_USER" -- env XDG_RUNTIME_DIR="/run/user/$TARGET_UID" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$TARGET_UID/bus" "$@"
}
as_user systemctl --user daemon-reload || true
[ -d "$STATE_DIR" ] && echo "$(date -Is) uninstall purge=$PURGE" >> "$STATE_DIR/changes.log"

# Host agent: stop it (this detaches every forwarded USB device) and remove its unit.
if [ -f /etc/systemd/system/clouddeck-agent.service ]; then
    systemctl disable --now clouddeck-agent.service >/dev/null 2>&1 || true
    rm -f /etc/systemd/system/clouddeck-agent.service
    rm -rf /etc/clouddeck-client
    systemctl daemon-reload
    info "stopped and removed clouddeck-agent"
fi

if [ "$PURGE" -eq 1 ]; then
    # Only remove the patched files once nothing runs from them.
    if [ "$RESTART" -eq 0 ] && pgrep -u "$TARGET_UID" -f "^$BASE/" >/dev/null; then
        die "patched Sunshine is still running; rerun without --no-restart to purge"
    fi
fi

if [ "$RESTART" -eq 1 ]; then
    if [ "$PURGE" -eq 1 ]; then
        # Restart now, synchronously, so the files below are no longer in use.
        info "restarting Sunshine onto the stock build now (your stream will drop briefly)"
        as_user systemctl --user restart "$UNIT"
    else
        as_user systemctl --user reset-failed clouddeck-sunshine-revert.service 2>/dev/null || true
        as_user systemd-run --user --unit=clouddeck-sunshine-revert --on-active=15 \
            --description="CloudDeck-Client: switch Sunshine back to stock" \
            systemctl --user restart "$UNIT" >/dev/null
        info "Sunshine restarts onto the stock build in 15 s; your stream will drop briefly."
    fi
fi

if [ "$PURGE" -eq 1 ]; then
    rm -rf "$BASE" "$STATE_DIR" "$TARGET_HOME/.config/clouddeck-client" "$TARGET_HOME/.local/state/clouddeck-client"
    info "purged $BASE, $STATE_DIR and the user's clouddeck-client state"
fi
info "done"
