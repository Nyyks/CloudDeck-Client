#!/usr/bin/env bash
# Local build of the Raspberry Pi .deb (Raspberry Pi OS Trixie, 64-bit; Pi 4 and Pi 5).
# Runs tools/deb/rpi-trixie-build.sh in an arm64 debian:trixie container through QEMU, so it
# works on an x86_64 PC with Docker (binfmt for arm64 registered). Expect about an hour.
# Usage: tools/build-deb-rpi.sh  ->  out/linux-arm64/CloudDeckClient-<version>-raspberrypi-trixie-arm64.deb
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/out/linux-arm64"
VERSION="${CI_VERSION:-$(tr -d ' \r\n' < "$REPO/client-desktop/app/version.txt")-dev.$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo local)}"
mkdir -p "$OUT"
rm -f "$OUT"/*.deb

echo "== USB helper (static Go, arm64)"
docker run --rm -v "$REPO/host/agent:/src" -v "$OUT:/out" -w /src -e CGO_ENABLED=0 -e GOOS=linux -e GOARCH=arm64 \
  golang:1.27 go build -trimpath -ldflags "-s -w -X main.version=$VERSION" -o /out/clouddeck-usb-helper ./cmd/clouddeck-usb-helper

echo "== Sources"
# On a Docker volume, not the Windows file system (much faster under QEMU). One container reads
# the repo read-only and fills the volume; nothing depends on piping stdin into docker.
docker volume create cdc-deb-rpi >/dev/null
docker run --rm -v "$REPO:/repo:ro" -v cdc-deb-rpi:/b alpine sh -euc '
  rm -rf /b/src && mkdir -p /b/src
  tar -C /repo --exclude=client-desktop/build --exclude=client-desktop/libs --exclude="client-desktop/deps*" \
      --exclude="*.o" -cf - client-desktop common shared tools/deb | tar -C /b/src -xf -
  for d in client-desktop common tools/deb; do [ -d "/b/src/$d" ] || { echo "sources incomplete: $d missing"; exit 1; }; done
  # Windows checkouts have CRLF; the container scripts and desktop files need LF.
  find /b/src/tools/deb -type f -exec sed -i "s/\r$//" {} +
  find /b/src/client-desktop/app/clouddeck/deploy/linux -type f -exec sed -i "s/\r$//" {} +'

echo "== Build (arm64 debian:trixie)"
docker run --rm --platform linux/arm64 -v cdc-deb-rpi:/b -v "$OUT:/out" \
  -e VERSION="$VERSION" -e SRC=/b/src -e OUT=/out -e USB_HELPER=/out/clouddeck-usb-helper \
  debian:trixie bash /b/src/tools/deb/rpi-trixie-build.sh
rm -f "$OUT/clouddeck-usb-helper"
ls -la "$OUT"
