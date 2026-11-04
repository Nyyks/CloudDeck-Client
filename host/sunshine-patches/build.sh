#!/usr/bin/env bash
# Builds Sunshine at UPSTREAM_TAG with our patch series for the CloudDeck VM
# (Bazzite / Fedora 43, AMD). Mirrors upstream packaging/linux/copr/Sunshine.spec
# (same deps, compiler and feature flags), but:
#   - installs to PREFIX instead of /usr, so it runs beside the stock RPM
#     without rpm-ostree (docs/adr/0003)
#   - CUDA off (the VM has an AMD GPU)
# Run inside a fedora:43 container as root:
#   host/sunshine-patches/build.sh <repo-root> <out-dir>
set -euo pipefail

REPO="$(cd "$1" && pwd)"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"
HERE="$REPO/host/sunshine-patches"
TAG="$(tr -d ' \r\n' < "$HERE/UPSTREAM_TAG")"
PREFIX=/var/opt/clouddeck-client/sunshine
PY=3.14
GCC=14
WORK="${WORK:-/tmp/sunshine-build}"

echo "== deps (Fedora BuildRequires from upstream spec)"
dnf -y install --setopt=install_weak_deps=False \
  cmake desktop-file-utils git libcap-devel libcurl-devel libdrm-devel libevdev-devel libva-devel \
  libX11-devel libxcb-devel libXcursor-devel libXfixes-devel libXi-devel libXinerama-devel libXrandr-devel \
  libXtst-devel openssl-devel pipewire-devel systemd-rpm-macros wget which appstream glslc \
  libappstream-glib vulkan-loader-devel libatomic xz libgudev-devel mesa-libGL-devel mesa-libgbm-devel \
  miniupnpc-devel nodejs-npm numactl-devel opus-devel pulseaudio-libs-devel qt6-qtbase-devel \
  qt6-qtsvg-devel systemd-udev uv "gcc$GCC" "gcc$GCC-c++" patch zstd file

echo "== source $TAG"
rm -rf "$WORK"
git clone --quiet --depth 1 --branch "$TAG" --recurse-submodules --shallow-submodules \
  https://github.com/LizardByte/Sunshine.git "$WORK"
cd "$WORK"
git config user.name "CloudDeck-Client CI"
git config user.email "ci@invalid"

echo "== patches"
APPLIED=0
while IFS= read -r p; do
  p="${p%%#*}"; p="$(echo "$p" | tr -d '[:space:]')"
  [ -z "$p" ] && continue
  echo "applying $p"
  git am --3way "$HERE/$p"
  APPLIED=$((APPLIED + 1))
done < "$HERE/series"
echo "$APPLIED patch(es) applied"
shopt -s nullglob
patch_files=("$HERE"/*.patch)
shopt -u nullglob
PATCH_ID="$(cat "$HERE/series" "${patch_files[@]}" | sha256sum | cut -c1-8)"

echo "== python deps for glad"
uv python install "$PY"
uv sync --locked --only-group glad --python "$PY" --no-install-project

echo "== configure"
export CC="gcc-$GCC" CXX="g++-$GCC"
COMMIT="$(git rev-parse HEAD)"
export BRANCH=master BUILD_VERSION="$TAG" COMMIT
cmake -B build -G "Unix Makefiles" -S . \
  -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF -DBUILD_WERROR=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DSUNSHINE_ASSETS_DIR="$PREFIX/share/sunshine" \
  -DSUNSHINE_EXECUTABLE_PATH="$PREFIX/bin/sunshine" \
  -DSUNSHINE_ENABLE_DRM=ON -DSUNSHINE_ENABLE_KWIN=ON -DSUNSHINE_ENABLE_PORTAL=ON \
  -DSUNSHINE_ENABLE_WAYLAND=ON -DSUNSHINE_ENABLE_X11=ON -DSUNSHINE_ENABLE_CUDA=OFF \
  -DSUNSHINE_PUBLISHER_NAME="CloudDeck-Client (patched LizardByte Sunshine)" \
  -DSUNSHINE_PUBLISHER_WEBSITE=https://github.com/Nyyks/CloudDeck-Client \
  -DSUNSHINE_PUBLISHER_ISSUE_URL=https://github.com/Nyyks/CloudDeck-Client/issues \
  -DGLAD_SKIP_PIP_INSTALL=ON -DPython_EXECUTABLE="$WORK/.venv/bin/python"

echo "== build"
make -j"$(nproc)" -C build

echo "== stage"
STAGE="$WORK/stage"
rm -rf "$STAGE"
DESTDIR="$STAGE" make -C build install
ROOT="$STAGE$PREFIX"
test -x "$ROOT/bin/sunshine" || { echo "sunshine binary missing from install"; exit 1; }
# Keep only what runs on the host; the stock RPM keeps owning udev rules, units and
# desktop files (install.sh adds its own drop-ins instead).
find "$STAGE" -path "$STAGE$PREFIX" -prune -o -type f -print | sed "s|^$STAGE||" > "$OUT/dropped-files.txt" || true
cat > "$ROOT/BUILD-INFO" <<EOF
upstream_tag=$TAG
upstream_commit=$(git rev-parse HEAD)
patches_applied=$APPLIED
patch_id=$PATCH_ID
built=$(date -u +%FT%TZ)
builder=fedora:$(rpm -E %fedora) gcc-$GCC
EOF
cp "$REPO/LICENSE" "$ROOT/LICENSE.CloudDeck-Client"
cp "$WORK/LICENSE" "$ROOT/LICENSE.Sunshine"
cp "$HERE/series" "$ROOT/PATCH-SERIES"

echo "== runtime libraries (must all exist on the VM image)"
ldd "$ROOT/bin/sunshine" | awk '{print $1}' | sort > "$OUT/needed-libs.txt"
if ldd "$ROOT/bin/sunshine" | grep -q 'not found'; then
  ldd "$ROOT/bin/sunshine" | grep 'not found'; exit 1
fi

NAME="clouddeck-sunshine-${TAG#v}-p${APPLIED}.${PATCH_ID}-fc$(rpm -E %fedora)-x86_64"
tar --zstd -C "$STAGE" -cf "$OUT/$NAME.tar.zst" "${PREFIX#/}"
( cd "$OUT" && sha256sum "$NAME.tar.zst" > "$NAME.tar.zst.sha256" )
"$ROOT/bin/sunshine" --version 2>&1 | head -3 || true
echo "== done: $OUT/$NAME.tar.zst"
