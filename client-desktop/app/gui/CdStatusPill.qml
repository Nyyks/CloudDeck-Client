// VM status pill ("Running", "Stopped", ...).
import QtQuick 2.9
import "CdTheme.js" as T

Rectangle {
    id: pill
    property string status: ""
    property string extra: ""
    function label(s) {
        switch (s) {
        case "running": return qsTr("Running")
        case "off": return qsTr("Stopped")
        case "starting": return qsTr("Starting")
        case "stopping": return qsTr("Stopping")
        case "": return qsTr("Unknown")
        default: return s
        }
    }
    implicitWidth: row.implicitWidth + 28
    implicitHeight: 34
    radius: height / 2
    color: T.statusSoft(status)
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Rectangle { width: 8; height: 8; radius: 4; color: T.statusDot(pill.status); anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: pill.label(pill.status) + (pill.extra !== "" ? "  ·  " + pill.extra : "")
            color: T.statusInk(pill.status)
            font.pixelSize: 13
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
