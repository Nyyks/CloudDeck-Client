#!/usr/bin/env bash
# CloudDeck-Client host install for the CloudDeck VM (Bazzite / Fedora 43).
# Idempotent; undo with ./uninstall.sh. Every change is listed in docs/host-changes.md.
#
# What it does (nothing in the read-only OS image, no rpm-ostree):
#   /var/opt/clouddeck-client/releases/<build>/   patched Sunshine (from the bundle tarball)
#   /var/opt/clouddeck-client/sunshine            symlink to the active release
#   /var/opt/clouddeck-client/libexec/            sunshine-launch, sunshine-healthcheck
#   ~USER/.config/systemd/user/app-dev.lizardbyte.app.Sunshine.service.d/zz-clouddeck-client.conf
#   /var/lib/clouddeck-client/                    install state, change log, backups
# Stock Sunshine (/usr/bin/sunshine), its config and pairing state stay untouched.
# The launcher falls back to stock Sunshine automatically if anything is wrong.
#
# Usage: sudo ./install.sh [--user NAME] [--force] [--no-restart]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BASE=/var/opt/clouddeck-client
STATE_DIR=/var/lib/clouddeck-client
UNIT=app-dev.lizardbyte.app.Sunshine.service
TARGET_USER=""
FORCE=0
RESTART=1

die() { echo "ERROR: $*" >&2; exit 1; }
info() { echo "==> $*"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --user) TARGET_USER="$2"; shift 2 ;;
        --force) FORCE=1; shift ;;
        --no-restart) RESTART=0; shift ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) die "unknown option $1" ;;
    esac
done

[ "$(id -u)" -eq 0 ] || die "run as root: sudo $0"

# ---------------------------------------------------------------- preflight
. /etc/os-release
[ "${ID:-}" = bazzite ] || [[ " ${ID_LIKE:-} " == *" fedora "* ]] || die "not a Bazzite/Fedora system (ID=$ID)"
[ "${VERSION_ID:-}" = 43 ] || [ "$FORCE" -eq 1 ] || die "built for Fedora 43, this is $VERSION_ID (use --force to override)"
STOCK_VER="$(rpm -q --qf '%{VERSION}' Sunshine 2>/dev/null)" || die "stock Sunshine RPM not found"
[ -f "/usr/lib/systemd/user/$UNIT" ] || die "Sunshine user unit $UNIT not found"
command -v setcap >/dev/null || die "setcap not found"

if [ -z "$TARGET_USER" ]; then
    TARGET_USER="$(ps -o user= -C sunshine 2>/dev/null | head -n1 | tr -d ' ')"
    [ -n "$TARGET_USER" ] || TARGET_USER="$(getent passwd 1000 | cut -d: -f1)"
fi
id "$TARGET_USER" >/dev/null 2>&1 || die "user $TARGET_USER does not exist (use --user)"
TARGET_UID="$(id -u "$TARGET_USER")"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
info "desktop user: $TARGET_USER (uid $TARGET_UID), stock Sunshine $STOCK_VER"

# ---------------------------------------------------------------- bundle
shopt -s nullglob
tarballs=("$HERE"/clouddeck-sunshine-*.tar.zst)
[ "${#tarballs[@]}" -eq 1 ] || die "expected exactly one clouddeck-sunshine-*.tar.zst next to install.sh, found ${#tarballs[@]}"
TARBALL="${tarballs[0]}"
NAME="$(basename "$TARBALL" .tar.zst)"
if [ -f "$TARBALL.sha256" ]; then
    (cd "$HERE" && sha256sum -c --quiet "$(basename "$TARBALL").sha256") || die "checksum mismatch for $TARBALL"
else
    [ "$FORCE" -eq 1 ] || die "missing $TARBALL.sha256 (use --force to skip)"
fi
info "bundle: $NAME"

# ---------------------------------------------------------------- stage release
REL="$BASE/releases/$NAME"
TS="$(date +%Y%m%d-%H%M%S)"
BACKUP="$STATE_DIR/backup/$TS"
mkdir -p "$BASE/releases" "$BASE/libexec" "$STATE_DIR"
changed=0

if [ -d "$REL" ]; then
    info "release already present: $REL"
else
    tmp="$(mktemp -d "$BASE/releases/.tmp.XXXXXX")"
    tar --zstd -xf "$TARBALL" -C "$tmp"
    src="$tmp/var/opt/clouddeck-client/sunshine"
    [ -x "$src/bin/sunshine" ] || { rm -rf "$tmp"; die "tarball has no bin/sunshine"; }
    mv "$src" "$REL"
    rm -rf "$tmp"
    changed=1
    info "extracted to $REL"
fi

BUILT_FOR="$(sed -n 's/^upstream_tag=v\{0,1\}//p' "$REL/BUILD-INFO")"
if [ "$BUILT_FOR" != "$STOCK_VER" ]; then
    msg="bundle is built for Sunshine $BUILT_FOR but the image has $STOCK_VER"
    [ "$FORCE" -eq 1 ] || die "$msg. The launcher would refuse it; rebuild the patches for $STOCK_VER."
    echo "WARNING: $msg (the launcher will run stock Sunshine until they match)"
fi

missing="$(ldd "$REL/bin/sunshine" | grep 'not found' || true)"
if [ -n "$missing" ]; then
    echo "$missing" >&2
    die "patched Sunshine is missing the libraries above on this host"
fi

# Same file capabilities as the stock binary (KMS capture needs CAP_SYS_ADMIN).
setcap cap_sys_admin,cap_sys_nice=p "$REL/bin/sunshine"
getcap "$REL/bin/sunshine" | grep -q cap_sys_admin || die "setcap failed"

# Use CloudDeck's defaults, not upstream's. Sunshine copies <assets>/apps.json into
# ~/.config/sunshine when that file is missing (CloudDeck recreates it at boot), and
# CloudDeck's image ships its own defaults there (the "CloudDeck" app). Symlink every
# top-level default file our build shares with the stock assets, so the patched build
# always uses the stock version, including after CloudDeck image updates.
STOCK_ASSETS=/usr/share/sunshine
if [ -d "$STOCK_ASSETS" ]; then
    for f in "$REL/share/sunshine"/*; do
        name="$(basename "$f")"
        [ -f "$STOCK_ASSETS/$name" ] || continue
        [ -L "$f" ] && continue
        [ -f "$f" ] || continue
        mv "$f" "$f.upstream"
        ln -s "$STOCK_ASSETS/$name" "$f"
        info "defaults: $name -> $STOCK_ASSETS/$name"
    done
fi

# Repair: if the user's apps.json is exactly upstream's default (written by an earlier
# install of ours), put CloudDeck's default back. A customized file is never touched.
USER_APPS="$TARGET_HOME/.config/sunshine/apps.json"
if [ -f "$USER_APPS" ] && [ -f "$REL/share/sunshine/apps.json.upstream" ] && [ -f "$STOCK_ASSETS/apps.json" ] &&
   cmp -s "$USER_APPS" "$REL/share/sunshine/apps.json.upstream"; then
    mkdir -p "$BACKUP"; cp -a "$USER_APPS" "$BACKUP/apps.json.user"
    install -m 0644 -o "$TARGET_USER" -g "$(id -gn "$TARGET_USER")" "$STOCK_ASSETS/apps.json" "$USER_APPS"
    changed=1
    info "restored CloudDeck's apps.json (upstream default backed up to $BACKUP/)"
fi

# Activate release (atomic symlink swap).
if [ "$(readlink "$BASE/sunshine" 2>/dev/null)" != "$REL" ]; then
    ln -sfn "$REL" "$BASE/.sunshine.new" && mv -Tf "$BASE/.sunshine.new" "$BASE/sunshine"
    changed=1
    info "active release -> $REL"
fi

for f in sunshine-launch sunshine-healthcheck; do
    if ! cmp -s "$HERE/files/$f" "$BASE/libexec/$f"; then
        install -m 0755 -o root -g root "$HERE/files/$f" "$BASE/libexec/$f"
        changed=1
    fi
done
# Keep a copy of the uninstaller on the host so removal never needs the bundle.
install -m 0755 -o root -g root "$HERE/uninstall.sh" "$BASE/uninstall.sh"

# ---------------------------------------------------------------- host agent (USB passthrough)
# Optional in the bundle: older bundles have no agent.
if [ -f "$HERE/clouddeck-agent" ]; then
    agent_changed=0
    install -d -m 0755 "$BASE/bin" /etc/clouddeck-client
    if ! cmp -s "$HERE/clouddeck-agent" "$BASE/bin/clouddeck-agent"; then
        install -m 0755 -o root -g root "$HERE/clouddeck-agent" "$BASE/bin/clouddeck-agent"
        agent_changed=1
    fi
    env_line="$(printf 'CDC_SOCKET_GROUP=%s\nCDC_DESKTOP_USER=%s' "$(id -gn "$TARGET_USER")" "$TARGET_USER")"
    if [ "$(cat /etc/clouddeck-client/agent.env 2>/dev/null)" != "$env_line" ]; then
        echo "$env_line" > /etc/clouddeck-client/agent.env
        agent_changed=1
    fi
    if ! cmp -s "$HERE/files/clouddeck-agent.service" /etc/systemd/system/clouddeck-agent.service; then
        install -m 0644 "$HERE/files/clouddeck-agent.service" /etc/systemd/system/clouddeck-agent.service
        agent_changed=1
    fi
    systemctl daemon-reload
    systemctl enable clouddeck-agent.service >/dev/null 2>&1
    if [ "$agent_changed" -eq 1 ] || ! systemctl is-active --quiet clouddeck-agent.service; then
        # Restarting detaches any forwarded USB devices; clients reattach.
        systemctl restart clouddeck-agent.service
        changed=1
    fi
    sleep 1
    systemctl is-active --quiet clouddeck-agent.service && info "host agent running" ||
        echo "WARNING: clouddeck-agent failed to start: journalctl -u clouddeck-agent -b" >&2
fi

# ---------------------------------------------------------------- user unit drop-in
DROPDIR="$TARGET_HOME/.config/systemd/user/$UNIT.d"
DROPIN="$DROPDIR/zz-clouddeck-client.conf"
if ! cmp -s "$HERE/files/zz-clouddeck-client.conf" "$DROPIN"; then
    if [ -e "$DROPIN" ]; then
        mkdir -p "$BACKUP"; cp -a "$DROPIN" "$BACKUP/"
        info "backed up existing drop-in to $BACKUP/"
    fi
    install -d -o "$TARGET_USER" -g "$(id -gn "$TARGET_USER")" "$TARGET_HOME/.config/systemd" \
        "$TARGET_HOME/.config/systemd/user" "$DROPDIR"
    install -m 0644 -o "$TARGET_USER" -g "$(id -gn "$TARGET_USER")" "$HERE/files/zz-clouddeck-client.conf" "$DROPIN"
    changed=1
    info "installed $DROPIN"
fi

# ---------------------------------------------------------------- record state
cat > "$STATE_DIR/install-state" <<EOF
installed_at=$(date -Is)
release=$NAME
user=$TARGET_USER
dropin=$DROPIN
stock_sunshine=$STOCK_VER
EOF
echo "$(date -Is) install $NAME user=$TARGET_USER changed=$changed" >> "$STATE_DIR/changes.log"

# ---------------------------------------------------------------- switch over
as_user() {
    runuser -u "$TARGET_USER" -- env XDG_RUNTIME_DIR="/run/user/$TARGET_UID" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$TARGET_UID/bus" "$@"
}

as_user systemctl --user daemon-reload
if [ "$RESTART" -eq 0 ]; then
    info "done. Sunshine was NOT restarted (--no-restart); it switches on its next start."
    exit 0
fi
if [ "$changed" -eq 0 ] && pgrep -u "$TARGET_UID" -f "^$BASE/sunshine/bin/sunshine" >/dev/null; then
    info "already installed and running the patched build; nothing to do."
    exit 0
fi

as_user systemctl --user reset-failed clouddeck-sunshine-switch.service 2>/dev/null || true
as_user systemd-run --user --unit=clouddeck-sunshine-switch --on-active=15 \
    --description="CloudDeck-Client: switch Sunshine to the patched build" \
    "$BASE/libexec/sunshine-healthcheck" >/dev/null
cat <<EOF

==> Installed. In 15 seconds Sunshine restarts onto the patched build.
    YOUR STREAM WILL DROP for a few seconds. Reconnect afterwards.
    If the patched build isn't serving within 45 s, it falls back to stock Sunshine on its own.

    Check the result after reconnecting:
      cat ~/.config/clouddeck-client/last-healthcheck
      journalctl --user -u $UNIT -b | grep clouddeck-client
EOF
