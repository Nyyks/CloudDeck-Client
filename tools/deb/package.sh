#!/usr/bin/env bash
# Turns a staged file tree into clouddeck-client_<version>_<arch>.deb (docs/building.md).
# Shared by the Raspberry Pi build (native, linked against Raspberry Pi OS) and the generic
# x86_64 package (the AppImage contents under /opt).
#
# Usage: package.sh <root> <version> <arch> <depends> <out-dir>
#   <root>     staged tree (usr/..., opt/...); DEBIAN/ is written into it
#   <depends>  Debian Depends: value
set -euo pipefail
ROOT=$1 VERSION=$2 ARCH=$3 DEPENDS=$4 OUT=$5
HERE="$(cd "$(dirname "$0")" && pwd)"
# Debian versions can't contain "-dev." the way our CI versions do; "~" sorts before releases.
DEBVER="${VERSION//-/\~}"

install -d "$ROOT/DEBIAN" "$ROOT/usr/share/doc/clouddeck-client"
install -m 0644 "$HERE/copyright" "$ROOT/usr/share/doc/clouddeck-client/copyright"
SIZE=$(du -sk --exclude=DEBIAN "$ROOT" | cut -f1)
cat > "$ROOT/DEBIAN/control" <<EOF
Package: clouddeck-client
Version: $DEBVER
Architecture: $ARCH
Maintainer: CloudDeck-Client <https://github.com/Nyyks/CloudDeck-Client>
Installed-Size: $SIZE
Depends: $DEPENDS
Section: games
Priority: optional
Homepage: https://github.com/Nyyks/CloudDeck-Client
Description: Client for a CloudDeck cloud PC, based on Moonlight
 Signs in to CloudDeck, starts the cloud PC, pairs and streams it with one button.
 Adds microphone and USB passthrough, clipboard sync, fit-to-window streams and an
 in-stream menu. Installs next to stock Moonlight and shares nothing with it.
 Unofficial; not affiliated with CloudDeck, Moonlight or LizardByte.
EOF
# Refresh the menu and icon caches after install and removal.
for s in postinst postrm; do
  cat > "$ROOT/DEBIAN/$s" <<'EOF'
#!/bin/sh
set -e
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
exit 0
EOF
  chmod 0755 "$ROOT/DEBIAN/$s"
done
find "$ROOT" -path "$ROOT/DEBIAN" -prune -o -type d -exec chmod 0755 {} +
mkdir -p "$OUT"
dpkg-deb --root-owner-group --build "$ROOT" "$OUT/clouddeck-client_${DEBVER}_${ARCH}.deb"
