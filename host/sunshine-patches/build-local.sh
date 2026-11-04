#!/usr/bin/env bash
# Local build of the patched Sunshine and the VM host bundle, using Docker.
#   host/sunshine-patches/build-local.sh            # build -> out/host/
#   CLEAN=1 host/sunshine-patches/build-local.sh    # drop the cached checkout first
# Source checkout, build tree and ccache live in Docker volumes, so rebuilds
# after a patch change only recompile what changed.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE=clouddeck-sunshine-builder:fc43
OUT="$REPO/out/host"
mkdir -p "$OUT"

# Git Bash on Windows: Docker needs C:/... paths, and MSYS must not rewrite
# the container-side paths.
export MSYS_NO_PATHCONV=1
HOST_REPO="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
HOST_OUT="$HOST_REPO/out/host"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1 || [ "${REBUILD_IMAGE:-0}" = 1 ]; then
  docker build -t "$IMAGE" -f "$HOST_REPO/host/sunshine-patches/Containerfile" "$HOST_REPO/host/sunshine-patches"
fi
if [ "${CLEAN:-0}" = 1 ]; then docker volume rm -f cdc-sunshine-work >/dev/null; fi

docker run --rm \
  -v "$HOST_REPO:/src:ro" \
  -v "$HOST_OUT:/out" \
  -v cdc-sunshine-work:/work \
  -v cdc-sunshine-ccache:/root/.cache/ccache \
  -v cdc-sunshine-uv:/root/.cache/uv \
  -e WORK=/work/sunshine \
  "$IMAGE" bash /src/host/sunshine-patches/build.sh /src /out

# Assemble the bundle the VM installer expects (same layout as CI).
cd "$OUT"
TB="$(ls -t clouddeck-sunshine-*.tar.zst | head -n1)"
NAME="$(basename "$TB" .tar.zst)"
B="clouddeck-host-${NAME#clouddeck-sunshine-}"
rm -rf "$B" && mkdir -p "$B/files"
cp "$REPO/host/install/install.sh" "$REPO/host/install/uninstall.sh" "$REPO/LICENSE" "$B/"
cp "$REPO/host/install/files/"* "$B/files/"
cp "$TB" "$TB.sha256" "$B/"
chmod +x "$B"/*.sh "$B"/files/sunshine-*
tar -czf "$B.tar.gz" "$B"
sha256sum "$B.tar.gz" > "$B.tar.gz.sha256"
echo "bundle: $OUT/$B.tar.gz"
