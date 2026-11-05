#!/bin/bash
# CloudDeck-Client: manual check that KWin on the VM can switch to a wide mode
# (Phase 4a, docs/adr/0007-multi-monitor.md). Run it as the desktop user in a
# terminal *inside a stream*:
#
#   bash display-test.sh [width height fps]      (default 3840 1080 60)
#
# It switches the display, waits 15 seconds and ALWAYS switches back (also on
# Ctrl+C). During those 15 seconds the stream should show the desktop squeezed
# into your window, twice as wide. A custom mode it had to add stays in KWin's
# list (harmless; System Settings > Display can remove it).
set -u
W="${1:-3840}"; H="${2:-1080}"; FPS="${3:-60}"
export LANG=C.UTF-8 QT_QPA_PLATFORM=wayland

json() { kscreen-doctor -j 2>/dev/null | sed -n '/^{/,$p'; }
# Prints "<output> <current mode id>" for the primary output.
current() {
    json | python3 -c '
import json,sys
outs=[o for o in json.load(sys.stdin)["outputs"] if o.get("enabled",True) and o["connected"]]
outs.sort(key=lambda o: o.get("priority") or 99)
print(outs[0]["name"], outs[0]["currentModeId"])'
}
# Prints the id of a W x H mode at ~FPS on output $1, or nothing.
find_mode() {
    json | python3 -c '
import json,sys
o=[o for o in json.load(sys.stdin)["outputs"] if o["name"]==sys.argv[1]][0]
m=[m for m in o["modes"] if m["size"]["width"]==int(sys.argv[2]) and m["size"]["height"]==int(sys.argv[3]) and abs(m["refreshRate"]-int(sys.argv[4]))<=0.5]
print(m[0]["id"] if m else "")' "$1" "$W" "$H" "$FPS"
}

read -r OUT ORIG < <(current) || { echo "kscreen-doctor failed (run this in the Plasma session)"; exit 1; }
echo "output $OUT, current mode id $ORIG"
restore() { kscreen-doctor "output.$OUT.mode.$ORIG" >/dev/null 2>&1 && echo "restored mode $ORIG"; }
trap 'restore; exit' INT TERM

ID="$(find_mode "$OUT")"
if [ -z "$ID" ]; then
    for blank in full reduced; do
        echo "adding custom mode ${W}x${H}@${FPS} ($blank blanking)"
        kscreen-doctor "output.$OUT.addCustomMode.$W.$H.$((FPS * 1000)).$blank" && ID="$(find_mode "$OUT")"
        [ -n "$ID" ] && break
    done
fi
[ -n "$ID" ] || { echo "RESULT: FAIL - KWin did not accept ${W}x${H}@${FPS}"; exit 1; }

echo "switching to mode $ID (${W}x${H}@${FPS}) for 15 seconds..."
kscreen-doctor "output.$OUT.mode.$ID" >/dev/null 2>&1
sleep 2
read -r _ NOW < <(current)
sleep 13
restore
if [ "$NOW" = "$ID" ]; then
    echo "RESULT: OK - KWin switched to ${W}x${H}@${FPS}"
else
    echo "RESULT: FAIL - KWin stayed on mode $NOW"
fi
