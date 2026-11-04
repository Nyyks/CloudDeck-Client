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

# SKIP_BUILD=1: only re-assemble the bundle (installer changes, same Sunshine build)
if [ "${SKIP_BUILD:-0}" != 1 ]; then
  if ! docker image inspect "$IMAGE" >/dev/null 2>&1 || [ "${REBUILD_IMAGE:-0}" = 1 ]; then
    docker build -t "$IMAGE" -f "$HOST_REPO/host/sunshine-patches/Containerfile" "$HOST_REPO/host/sunshine-patches"
  fi
  if [ "${CLEAN:-0}" = 1 ]; then docker volume rm -f cdc-sunshine-work >/dev/null; fi

  # Memory/CPU caps: Docker Desktop and other WSL distros share one VM, so an
  # uncapped -j16 C++ build can exhaust the host's RAM.
  docker run --rm --memory "${DOCKER_MEMORY:-10g}" -e JOBS="${JOBS:-8}" \
    -v "$HOST_REPO:/src:ro" \
    -v "$HOST_OUT:/out" \
    -v cdc-sunshine-work:/work \
    -v cdc-sunshine-ccache:/root/.cache/ccache \
    -v cdc-sunshine-uv:/root/.cache/uv \
    -e WORK=/work/sunshine \
    -e RUN_TESTS="${RUN_TESTS:-0}" \
    "$IMAGE" bash /src/host/sunshine-patches/build.sh /src /out
fi

# Host agent: test + static build in the official Go image (always; ~10 s with cache).
docker run --rm -v "$HOST_REPO:/src" -v cdc-go-cache:/root/.cache -w /src/host/agent \
  -e CGO_ENABLED=0 golang:1.27 sh -c \
  "go vet ./... && go test -count=1 ./... && go build -trimpath -ldflags '-s -w -X main.version=$(git -C "$REPO" rev-parse --short=7 HEAD)' -o /src/out/host/clouddeck-agent ./cmd/clouddeck-agent"

# Assemble the bundle the VM installer expects (same layout as CI). The "-i" part
# identifies the installer revision, so a bundle with new scripts gets a new name.
cd "$OUT"
TB="$(ls -t clouddeck-sunshine-*.tar.zst | head -n1)"
NAME="$(basename "$TB" .tar.zst)"
INST_ID="$(cat "$REPO/host/install/install.sh" "$REPO/host/install/uninstall.sh" "$REPO/host/install/files/"* "$OUT/clouddeck-agent" | sha256sum | cut -c1-6)"
B="clouddeck-host-${NAME#clouddeck-sunshine-}-i$INST_ID"
rm -rf "$B" && mkdir -p "$B/files"
cp "$REPO/host/install/install.sh" "$REPO/host/install/uninstall.sh" "$REPO/LICENSE" "$B/"
cp "$REPO/host/install/files/"* "$B/files/"
cp "$TB" "$TB.sha256" "$B/"
cp "$OUT/clouddeck-agent" "$B/"
chmod +x "$B"/*.sh "$B"/files/sunshine-* "$B/clouddeck-agent"
tar -czf "$B.tar.gz" "$B"
sha256sum "$B.tar.gz" > "$B.tar.gz.sha256"
echo "bundle: $OUT/$B.tar.gz"
