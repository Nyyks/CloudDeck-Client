#!/usr/bin/env bash
# Builds common/ (moonlight-common-c) with CMake and runs the mic protocol tests.
# Linux / WSL:  bash tests/mic-protocol/run.sh            (validate against the vector)
#               bash tests/mic-protocol/run.sh --print    (print the vector instead)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B="${BUILD_DIR:-$HOME/cdc-mic-tests}"
mkdir -p "$B"
cmake -S "$ROOT/common" -B "$B/common" -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF > "$B/cmake.log"
cmake --build "$B/common" -j"$(nproc)" > "$B/build.log"
LIB="$(find "$B/common" -name 'libmoonlight-common-c.a' | head -n1)"
ENETLIB="$(find "$B/common" -name 'libenet.a' | head -n1)"
gcc -std=gnu11 -D_GNU_SOURCE -g -Wall -Wno-unused-parameter -DHAS_SOCKLEN_T -DLC_DEBUG \
  -I"$ROOT/common/src" -I"$ROOT/common/enet/include" -I"$ROOT/common/nanors" -I"$ROOT/common/nanors/deps/obl" \
  "$ROOT/tests/mic-protocol/test_mic.c" "$LIB" ${ENETLIB:+"$ENETLIB"} -lcrypto -lpthread -o "$B/test_mic"
if [ "${1:-}" = --print ]; then "$B/test_mic"; else "$B/test_mic" "$ROOT/shared/fixtures/mic/vector1.hex"; fi
