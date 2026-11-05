#!/usr/bin/env bash
# Loads the in-stream menu QML (gui/StreamMenu*.qml, StreamNotice.qml) headlessly
# against stub singletons, so binding and type errors show up without a stream.
#   client-desktop/tests/qml/run.sh   (needs qmltestrunner from Qt 6 and the Material style)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
cp "$HERE/tst_menu.qml" "$HERE"/../../app/gui/StreamMenu.qml "$HERE"/../../app/gui/StreamMenuButton.qml \
   "$HERE"/../../app/gui/StreamNotice.qml "$HERE"/../../app/gui/ControllerProfiles.qml "$HERE"/../../app/gui/StreamKeyboard.qml "$HERE"/../../app/gui/StreamGamepad.qml "$W/"
RUNNER="$(command -v qmltestrunner || echo /usr/lib/qt6/bin/qmltestrunner)"
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Material "$RUNNER" -import "$HERE/imports" -input "$W/tst_menu.qml"
