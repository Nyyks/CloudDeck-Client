#!/usr/bin/env bash
# Loads the CloudDeck QML (in-stream menu, home screen layouts) headlessly against stub
# singletons, so binding, type and navigation errors show up without a stream or a VM.
#   client-desktop/tests/qml/run.sh             (needs qmltestrunner from Qt 6 and the Material style)
#   CDC_SHOTS=/some/dir client-desktop/tests/qml/run.sh   also keeps the home-screen screenshots
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
cp "$HERE"/tst_*.qml "$HERE"/../../app/gui/*.qml "$HERE"/../../app/gui/*.js "$W/"
rm -f "$W/main.qml"
# The home screen loads icons from qrc:/res; tests read them from disk instead.
mkdir -p "$W/shots" "$W/res"
cp -r "$HERE"/../../app/res/cd "$W/res/"
cp "$HERE"/../../app/res/clouddeck-glyph.svg "$W/res/"
sed -i 's|qrc:/res/|res/|g' "$W"/*.qml
RUNNER="$(command -v qmltestrunner || echo /usr/lib/qt6/bin/qmltestrunner)"
cd "$W"
rc=0
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Material QT_QUICK_CONTROLS_MATERIAL_THEME=Dark \
QT_QUICK_CONTROLS_MATERIAL_VARIANT=Dense \
    "$RUNNER" -import "$HERE/imports" -input "$W" || rc=$?
if [ -n "${CDC_SHOTS:-}" ]; then mkdir -p "$CDC_SHOTS"; cp "$W"/shots/*.png "$CDC_SHOTS"/ 2>/dev/null || true; fi
exit $rc
