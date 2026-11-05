// Visible focus ring for keyboard, gamepad and TV-remote navigation. Shown only for
// Tab/D-pad focus (Control.visualFocus), never after a mouse click or a tap.
import QtQuick 2.9
import "CdTheme.js" as T

Rectangle {
    property bool shown: false
    property real ringRadius: 12
    anchors.fill: parent
    anchors.margins: -4
    radius: ringRadius + 4
    color: "transparent"
    border.width: 3
    border.color: T.focus
    visible: shown
}
