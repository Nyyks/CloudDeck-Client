#!/usr/bin/env bash
# Runs inside an arm64 debian:trixie container (tools/build-deb-rpi.sh, CI linux-arm64.yml).
# Builds the desktop client against Raspberry Pi OS Trixie's own libraries: Raspberry Pi's
# FFmpeg has the V4L2 request decoders that drive the Pi 5's HEVC block (upstream FFmpeg, as in
# the old AppImage, only has V4L2 M2M: the Pi 4's H.264 and nothing on a Pi 5), plus Debian's
# Qt 6, SDL2 and Mesa as the desktop uses them.
#
# Env: VERSION, SRC (sources: client-desktop, common, ...), OUT, USB_HELPER (optional)
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
: "${VERSION:?}" "${SRC:?}" "${OUT:?}"

apt-get update -qq
apt-get install -y -qq --no-install-recommends ca-certificates wget >/dev/null
# Raspberry Pi OS archive: its packages (ffmpeg 8:*+rpt*, mesa, ...) carry a higher epoch or
# version than Debian's, so apt prefers them, as on a real Pi. The signing key comes from
# Raspberry Pi's keyring package, as on a Pi (the older raspberrypi.gpg.key has a SHA-1
# binding that Trixie's apt rejects); the package is pinned by hash.
KEYRING_DEB=raspberrypi-archive-keyring_2025.1+rpt1_all.deb
KEYRING_SHA256=2e727149d7acb8cc7f604e66d0049161039c8aa1eaf1175e54f9e69d963d60e4
wget -qO "/tmp/$KEYRING_DEB" "https://archive.raspberrypi.com/debian/pool/main/r/raspberrypi-archive-keyring/$KEYRING_DEB"
echo "$KEYRING_SHA256  /tmp/$KEYRING_DEB" | sha256sum -c --quiet
dpkg -i "/tmp/$KEYRING_DEB" >/dev/null
echo "deb [signed-by=/usr/share/keyrings/raspberrypi-archive-keyring.gpg] https://archive.raspberrypi.com/debian/ trixie main" \
  > /etc/apt/sources.list.d/raspi.list
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
  build-essential pkg-config dpkg-dev file \
  qmake6 qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-svg-dev \
  libsdl2-dev libsdl2-ttf-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libdrm-dev libegl-dev libgles-dev libgl-dev libopus-dev libssl-dev \
  libwayland-dev libx11-dev libxkbcommon-dev >/dev/null
echo "== FFmpeg in use (should be Raspberry Pi's +rpt build):"
dpkg-query -W -f '${Package} ${Version}\n' libavcodec-dev
dpkg-query -W -f '${Version}' libavcodec-dev | grep -q rpt || { echo "libavcodec-dev is not Raspberry Pi's build"; exit 1; }

BUILD=/tmp/build ROOT=/tmp/pkg
rm -rf "$BUILD" "$ROOT" && mkdir -p "$BUILD" "$ROOT"
cd "$BUILD"
# No LTO: too slow under emulation, and Debian's own builds don't use it either.
qmake6 "$SRC/client-desktop/moonlight-qt.pro" PREFIX=/usr CONFIG+=release
make -j"$(nproc)" release
make install INSTALL_ROOT="$ROOT"

# Stock Moonlight's package owns /usr/bin/moonlight: keep ours under /usr/lib and add a command.
install -d "$ROOT/usr/lib/clouddeck-client"
mv "$ROOT/usr/bin/moonlight" "$ROOT/usr/lib/clouddeck-client/moonlight"
ln -s ../lib/clouddeck-client/moonlight "$ROOT/usr/bin/clouddeck-client"
if [ -n "${USB_HELPER:-}" ]; then
  install -m 0755 "$USB_HELPER" "$ROOT/usr/lib/clouddeck-client/clouddeck-usb-helper"
fi
sed -i 's/^Exec=moonlight/Exec=clouddeck-client/' "$ROOT/usr/share/applications/io.github.nyyks.CloudDeckClient.desktop"
grep -q '^Exec=clouddeck-client' "$ROOT/usr/share/applications/io.github.nyyks.CloudDeckClient.desktop"
strip --strip-unneeded "$ROOT/usr/lib/clouddeck-client/moonlight"

# Library dependencies from the binary; QML modules and Qt plugins are loaded at run time.
mkdir -p /tmp/shl/debian
printf 'Source: clouddeck-client\n\nPackage: clouddeck-client\nArchitecture: any\n' > /tmp/shl/debian/control
SHLIBS=$(cd /tmp/shl && dpkg-shlibdeps -O --ignore-missing-info -e "$ROOT/usr/lib/clouddeck-client/moonlight" | sed -n 's/^shlibs:Depends=//p')
RUNTIME="qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtqml-workerscript, qt6-svg-plugins, qt6-wayland, qt6-qpa-plugins"
bash "$SRC/tools/deb/package.sh" "$ROOT" "$VERSION" arm64 "$SHLIBS, $RUNTIME" "$OUT"
for f in "$OUT"/*.deb; do
  mv "$f" "$OUT/CloudDeckClient-${VERSION}-raspberrypi-trixie-arm64.deb"
done
ls -la "$OUT"
