#!/usr/bin/env bash
# Local build of the desktop client as an aarch64 AppImage (Raspberry Pi 4/5 with a 64-bit
# OS, other ARM64 Linux). It mirrors the x86_64 CI job (.github/workflows/build.yml,
# linux-appimage) inside an arm64 Ubuntu 22.04 container run through QEMU, so it works on an
# x86_64 PC with Docker. Expect about an hour. Differences from x86_64:
#   - no VA-API, VDPAU, Vulkan or libplacebo (not available on the Pi);
#   - FFmpeg gets the V4L2 memory-to-memory decoders, and the app keeps libdrm so decoded
#     frames reach EGL as DRM PRIME buffers (hardware H.264/HEVC where the Pi has it,
#     otherwise software decoding);
#   - no LTO (too slow under emulation).
# Usage: tools/build-linux-arm64.sh   ->  out/linux-arm64/CloudDeckClient-<version>-linux-aarch64.AppImage
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/out/linux-arm64"
VERSION="${CI_VERSION:-$(cat "$REPO/client-desktop/app/version.txt")-dev.$(git -C "$REPO" rev-parse --short HEAD)}"
mkdir -p "$OUT"

echo "== USB helper (static Go, arm64)"
docker run --rm -v "$REPO/host/agent:/src" -v "$OUT:/out" -w /src -e CGO_ENABLED=0 -e GOOS=linux -e GOARCH=arm64 \
  golang:1.27 go build -trimpath -ldflags "-s -w -X main.version=$VERSION" -o /out/clouddeck-usb-helper ./cmd/clouddeck-usb-helper

echo "== Sources"
# Build on a Docker volume, not on the Windows file system (much faster under QEMU).
docker volume create cdc-arm64-build >/dev/null
tar -C "$REPO" --exclude='client-desktop/build' --exclude='client-desktop/libs' --exclude='client-desktop/deps*' \
    --exclude='*.o' -cf - client-desktop .github/scripts | \
  docker run --rm -i --platform linux/arm64 -v cdc-arm64-build:/b ubuntu:22.04 \
    bash -c 'rm -rf /b/src && mkdir -p /b/src && tar -C /b/src -xf -'

echo "== Build (arm64 container)"
docker run --rm --platform linux/arm64 -v cdc-arm64-build:/b -v "$OUT:/out" -e VERSION="$VERSION" ubuntu:22.04 bash -euo pipefail -c '
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends ca-certificates git wget file build-essential pkg-config cmake ninja-build \
  autoconf automake libtool python3-pip \
  qt6-base-dev qt6-declarative-dev libqt6svg6-dev qml6-module-qtquick-controls qml6-module-qtquick-templates \
  qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtquick-window qml6-module-qtquick \
  qt6-qpa-plugins libqt6svg6 \
  libgbm-dev libdrm-dev libfreetype-dev libasound2-dev libdbus-1-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev \
  libglu1-mesa-dev libibus-1.0-dev libpulse-dev libudev-dev libx11-dev libxcursor-dev libxext-dev libxi-dev \
  libxinerama-dev libxkbcommon-dev libxrandr-dev libxss-dev libxt-dev libxv-dev libxxf86vm-dev libxcb-dri3-dev \
  libx11-xcb-dev libxfixes-dev libxtst-dev wayland-protocols libopus-dev libgl-dev libpipewire-0.3-dev liburing-dev \
  libssl-dev linux-libc-dev >/dev/null
pip3 install -q meson
N=$(nproc)

# Dependencies (same versions as build-linux-deps.sh), installed into /usr/local and cached on the volume.
if [ ! -f /b/deps-done ]; then
  mkdir -p /b/deps && cd /b/deps && rm -rf ./*
  clone() { git init -q "$3"; git -C "$3" fetch -q --depth 1 "$1" "$2"; git -C "$3" checkout -q FETCH_HEAD
            if [ "${4:-}" = recursive ]; then git -C "$3" submodule update -q --init --recursive --depth 1; fi; }
  clone https://github.com/libsdl-org/SDL.git release-3.4.18 SDL
  (cd SDL && cmake -G Ninja -DSDL_KMSDRM=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_INSTALL_DOCS=OFF -S . -B build && cmake --build build && cmake --install build)
  clone https://github.com/libsdl-org/sdl2-compat.git release-2.32.74 sdl2-compat
  (cd sdl2-compat && cmake -G Ninja -DSDL2COMPAT_TESTS=OFF -S . -B build && cmake --build build && cmake --install build)
  clone https://github.com/libsdl-org/SDL_ttf.git a883e490e30fb44a5336ea3dcb990c6982c5216f SDL_ttf recursive
  (cd SDL_ttf && ./autogen.sh >/dev/null && ./configure -q && make -s -j$N && make -s install)
  git clone -q --branch 1.5.4 --depth 1 https://code.videolan.org/videolan/dav1d.git
  (cd dav1d && meson setup build -Ddefault_library=static -Dbuildtype=release -Denable_tools=false -Denable_tests=false && ninja -C build && ninja -C build install)
  clone https://github.com/FFmpeg/FFmpeg.git n9.0.2 FFmpeg
  (cd FFmpeg && ./configure --enable-pic --disable-static --enable-shared --disable-all --disable-autodetect \
     --enable-avcodec --enable-avformat --enable-swscale \
     --enable-decoder=h264 --enable-decoder=hevc --enable-decoder=av1 \
     --enable-libdrm --enable-v4l2-m2m --enable-decoder=h264_v4l2m2m --enable-decoder=hevc_v4l2m2m \
     --enable-libdav1d --enable-decoder=libdav1d && make -s -j$N && make -s install)
  ldconfig
  touch /b/deps-done
fi
ldconfig

# linuxdeploy for aarch64 (run extracted: no FUSE in the container)
mkdir -p /b/bin && cd /b/bin
[ -x linuxdeploy-aarch64.AppImage ] || wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-aarch64.AppImage
[ -x linuxdeploy-plugin-qt-aarch64.AppImage ] || wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-aarch64.AppImage
chmod +x linuxdeploy*.AppImage
export PATH=/b/bin:$PATH APPIMAGE_EXTRACT_AND_RUN=1

cd /b/src/client-desktop
CI_VERSION="$VERSION" CDC_NO_LTO=1 CDC_APPIMAGE_LIBDRM=1 CDC_USB_HELPER=/out/clouddeck-usb-helper scripts/build-appimage.sh
mv build/installer-release/*.AppImage "/out/CloudDeckClient-${VERSION}-linux-aarch64.AppImage"
'
ls -la "$OUT"
