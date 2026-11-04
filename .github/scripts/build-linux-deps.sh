#!/usr/bin/env bash
# Builds the AppImage dependency stack exactly as upstream moonlight-qt v6.2.0's
# build-appimage.yml does, but installs each component twice: into / (so later
# components find it) and into $STAGE (so CI can cache the result).
# Run from client-desktop/. Usage: build-linux-deps.sh <stage-dir>
set -euo pipefail
STAGE="$1"
mkdir -p "$STAGE" deps
cd deps

inst_cmake() { sudo cmake --install build; sudo DESTDIR="$STAGE" cmake --install build; }
inst_make()  { sudo make install; sudo make DESTDIR="$STAGE" install; }
inst_ninja() { sudo ninja -C build install; sudo DESTDIR="$STAGE" ninja -C build install; }
clone() { # url ref dir [recursive]
  git init -q "$3"; git -C "$3" fetch -q --depth 1 "$1" "$2"; git -C "$3" checkout -q FETCH_HEAD
  if [ "${4:-}" = recursive ]; then git -C "$3" submodule update -q --init --recursive --depth 1; fi
}

clone https://github.com/libsdl-org/SDL.git release-3.4.18 SDL
(cd SDL && cmake -DSDL_KMSDRM=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_INSTALL_DOCS=OFF -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -S . -B build \
  && cmake --build build -j"$(nproc)" && inst_cmake)

clone https://github.com/libsdl-org/sdl2-compat.git release-2.32.74 sdl2-compat
(cd sdl2-compat && cmake -DSDL2COMPAT_TESTS=OFF -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -S . -B build \
  && cmake --build build -j"$(nproc)" && inst_cmake)

clone https://github.com/libsdl-org/SDL_ttf.git a883e490e30fb44a5336ea3dcb990c6982c5216f SDL_ttf recursive
(cd SDL_ttf && ./autogen.sh && ./configure && make -j"$(nproc)" && inst_make)

clone https://github.com/intel/libva.git 2.24.1 libva
(cd libva && ./autogen.sh && ./configure --enable-x11=yes --enable-glx=no --enable-drm=no --enable-wayland=no \
  && make -j"$(nproc)" && inst_make)

clone https://github.com/haasn/libplacebo.git 92b5ac6db79f4d680eb656692f7bf51e9606f42a libplacebo recursive
(cd libplacebo && git apply ../../app/deploy/linux/appimage/*.patch \
  && meson setup build -Dvulkan=enabled -Dopengl=disabled -Ddemos=false && ninja -C build && inst_ninja)

git clone -q --branch 1.5.4 --depth 1 https://code.videolan.org/videolan/dav1d.git
(cd dav1d && meson setup build -Ddefault_library=static -Dbuildtype=release -Denable_tools=false -Denable_tests=false \
  && ninja -C build && inst_ninja)

clone https://github.com/FFmpeg/FFmpeg.git n9.0.2 FFmpeg
(cd FFmpeg && ./configure --enable-pic --enable-lto --disable-static --enable-shared --disable-all --disable-autodetect \
  --enable-avcodec --enable-avformat --enable-swscale \
  --enable-decoder=h264 --enable-decoder=hevc --enable-decoder=av1 \
  --enable-vaapi --enable-hwaccel=h264_vaapi --enable-hwaccel=hevc_vaapi --enable-hwaccel=av1_vaapi \
  --enable-vdpau --enable-hwaccel=h264_vdpau --enable-hwaccel=hevc_vdpau --enable-hwaccel=av1_vdpau \
  --enable-libdrm --enable-vulkan --enable-hwaccel=h264_vulkan --enable-hwaccel=hevc_vulkan --enable-hwaccel=av1_vulkan \
  --enable-libdav1d --enable-decoder=libdav1d \
  && make -j"$(nproc)" && inst_make)

sudo chown -R "$(id -u):$(id -g)" "$STAGE"
echo "Dependencies staged in $STAGE"
