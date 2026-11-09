#!/usr/bin/env bash
# Generic Linux .deb (x86_64): the AppImage's contents under /opt/clouddeck-client, a
# clouddeck-client command and a menu entry. It bundles its own Qt, SDL and FFmpeg like the
# AppImage, so one package fits Debian, Ubuntu, Mint, Pop!_OS... alike. Run it on a Debian or
# Ubuntu system (CI: the AppImage job's runner); Depends lists the system libraries the bundle
# still uses (graphics drivers, X11/Wayland, audio, fonts).
#
# Usage: appimage-to-deb.sh <CloudDeckClient-*.AppImage> <version> <out-dir>
set -euo pipefail
APPIMAGE=$(realpath "$1") VERSION=$2 OUT=$(realpath -m "$3")
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cd "$WORK"
chmod +x "$APPIMAGE"
"$APPIMAGE" --appimage-extract >/dev/null   # -> squashfs-root (no FUSE needed)
ROOT="$WORK/root"
install -d "$ROOT/opt" "$ROOT/usr/bin" "$ROOT/usr/share/applications" "$ROOT/usr/share/icons/hicolor/scalable/apps"
mv squashfs-root "$ROOT/opt/clouddeck-client"
APP="$ROOT/opt/clouddeck-client"
[ -x "$APP/usr/bin/moonlight" ] || { echo "no usr/bin/moonlight in the AppImage"; exit 1; }

cat > "$ROOT/usr/bin/clouddeck-client" <<'EOF'
#!/bin/sh
exec /opt/clouddeck-client/AppRun "$@"
EOF
chmod 0755 "$ROOT/usr/bin/clouddeck-client"
DESKTOP=$(ls "$APP"/usr/share/applications/*.desktop | head -1)
sed 's/^Exec=.*/Exec=clouddeck-client/; s/^TryExec=.*//' "$DESKTOP" > "$ROOT/usr/share/applications/$(basename "$DESKTOP")"
cp "$APP"/usr/share/icons/hicolor/scalable/apps/*.svg "$ROOT/usr/share/icons/hicolor/scalable/apps/"
# The AppImage's top-level desktop file and icon links would point outside the package.
rm -f "$APP"/*.desktop "$APP"/.DirIcon "$APP"/*.svg "$APP"/*.png

# System libraries the bundle links against -> their Debian packages. Libraries that aren't
# installed on the build machine show up as "not found"; known ones are mapped by name (the
# libraries linuxdeploy leaves to the system), anything else stops the build.
LDD=$(find "$APP" -type f \( -name '*.so*' -o -perm -u+x \) -exec sh -c 'file -b "$1" | grep -q ^ELF && ldd "$1" 2>/dev/null' _ {} \;)
LIBS=$(echo "$LDD" | awk '$2 == "=>" && $3 ~ /^\// {print $3}' | grep -v "^$APP" | sort -u)
MISSING=$(echo "$LDD" | awk '$2 == "=>" && $3 == "not" {print $1}' | sort -u)
declare -A KNOWN=(
  [libOpenGL.so.0]=libopengl0 [libGLX.so.0]=libglx0 [libGL.so.1]=libgl1 [libEGL.so.1]=libegl1
  [libGLdispatch.so.0]=libglvnd0 [libgbm.so.1]=libgbm1 [libdrm.so.2]=libdrm2
  [libX11.so.6]=libx11-6 [libX11-xcb.so.1]=libx11-xcb1 [libxcb.so.1]=libxcb1 [libxcb-dri3.so.0]=libxcb-dri3-0
  [libXext.so.6]=libxext6 [libXfixes.so.3]=libxfixes3 [libXau.so.6]=libxau6 [libXdmcp.so.6]=libxdmcp6
  [libfontconfig.so.1]=libfontconfig1 [libfreetype.so.6]=libfreetype6 [libexpat.so.1]=libexpat1
  [libasound.so.2]=libasound2 [libpulse.so.0]=libpulse0 [libdbus-1.so.3]=libdbus-1-3
  [libwayland-client.so.0]=libwayland-client0 [libwayland-egl.so.1]=libwayland-egl1
  [libwayland-cursor.so.0]=libwayland-cursor0 [libxkbcommon.so.0]=libxkbcommon0
  [libSM.so.6]=libsm6 [libICE.so.6]=libice6 [libz.so.1]=zlib1g [libudev.so.1]=libudev1
  [libharfbuzz.so.0]=libharfbuzz0b
  # VA-API/VDPAU stay with the system so they match its GPU drivers
  [libva.so.2]=libva2 [libva-x11.so.2]=libva-x11-2 [libva-drm.so.2]=libva-drm2
  [libva-wayland.so.2]=libva-wayland2 [libvdpau.so.1]=libvdpau1
)
EXTRA="" UNKNOWN=""
for l in $MISSING; do
  if [ -n "${KNOWN[$l]:-}" ]; then EXTRA="$EXTRA ${KNOWN[$l]}"; else UNKNOWN="$UNKNOWN $l"; fi
done
[ -z "$UNKNOWN" ] || { echo "unresolved libraries:$UNKNOWN (install them on the build machine or add them to KNOWN)"; exit 1; }
PKGS=$( (for l in $LIBS; do dpkg -S "$(realpath "$l")" 2>/dev/null || dpkg -S "$l" 2>/dev/null; done | cut -d: -f1; \
         for p in $EXTRA; do echo "$p"; done) | sort -u | grep -v '^libc6-dev$' || true)
[ -n "$PKGS" ] || { echo "could not work out dependencies"; exit 1; }
DEPENDS=$(echo $PKGS | sed 's/ /, /g')
echo "Depends: $DEPENDS"

bash "$HERE/package.sh" "$ROOT" "$VERSION" amd64 "$DEPENDS" "$WORK/out"
mkdir -p "$OUT"
mv "$WORK"/out/*.deb "$OUT/CloudDeckClient-${VERSION}-linux-amd64.deb"
ls -la "$OUT"/*.deb
