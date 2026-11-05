#!/usr/bin/env bash
# Tests the fail-safe decisions of files/sunshine-launch with stub binaries.
# Run: bash host/install/tests/test-launch.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LAUNCH="$HERE/../files/sunshine-launch"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

fails=0
mk_stub() { printf '#!/bin/sh\necho "%s $*"\n' "$2" > "$1"; chmod +x "$1"; }

setup() {
    rm -rf "${T:?}/base" "${T:?}/home"
    mkdir -p "$T/base/sunshine/bin" "$T/home"
    mk_stub "$T/base/sunshine/bin/sunshine" PATCHED
    mk_stub "$T/stock" STOCK
    printf 'upstream_tag=v2026.914.233613\npatch_id=abcd1234\n' > "$T/base/sunshine/BUILD-INFO"
}

run() { # expected-word  [env...]
    local expect="$1"; shift
    local out
    out="$(env -i PATH="$PATH" HOME="$T/home" CDC_BASE="$T/base" CDC_STOCK="$T/stock" "$@" sh "$LAUNCH" --arg 2>/dev/null)"
    if [ "${out%% *}" = "$expect" ] && [ "$out" = "$expect --arg" ]; then
        echo "ok   - $CASE"
    else
        echo "FAIL - $CASE: expected '$expect --arg', got '$out'"; fails=$((fails + 1))
    fi
}

CASE="matching version runs patched build (args passed through)"; setup
run PATCHED CDC_STOCK_VERSION=2026.914.233613

CASE="version mismatch falls back to stock"; setup
run STOCK CDC_STOCK_VERSION=2026.999.000000

CASE="no Sunshine RPM falls back to stock"; setup
run STOCK CDC_STOCK_VERSION=

CASE="disabled flag falls back to stock"; setup
mkdir -p "$T/home/.config/clouddeck-client"; echo "test" > "$T/home/.config/clouddeck-client/sunshine-disabled"
run STOCK CDC_STOCK_VERSION=2026.914.233613

CASE="missing patched binary falls back to stock"; setup
rm "$T/base/sunshine/bin/sunshine"
run STOCK CDC_STOCK_VERSION=2026.914.233613

CASE="missing BUILD-INFO falls back to stock"; setup
rm "$T/base/sunshine/BUILD-INFO"
run STOCK CDC_STOCK_VERSION=2026.914.233613

CASE="crash loop (5th start within 120 s) falls back and disables"; setup
for _ in 1 2 3 4; do
    env -i PATH="$PATH" HOME="$T/home" CDC_BASE="$T/base" CDC_STOCK="$T/stock" CDC_STOCK_VERSION=2026.914.233613 sh "$LAUNCH" >/dev/null 2>&1
done
run STOCK CDC_STOCK_VERSION=2026.914.233613
CASE="crash loop leaves the disabled flag behind"
if [ -f "$T/home/.config/clouddeck-client/sunshine-disabled" ]; then echo "ok   - $CASE"; else echo "FAIL - $CASE"; fails=$((fails + 1)); fi

CASE="old starts (>120 s ago) don't count"; setup
mkdir -p "$T/home/.local/state/clouddeck-client"
old=$(( $(date +%s) - 600 )); for _ in 1 2 3 4 5 6; do echo "$old"; done > "$T/home/.local/state/clouddeck-client/sunshine-starts"
run PATCHED CDC_STOCK_VERSION=2026.914.233613

[ "$fails" -eq 0 ] && echo "all launcher tests passed" || { echo "$fails launcher test(s) failed"; exit 1; }
