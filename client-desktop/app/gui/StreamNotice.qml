// CloudDeck-Client: status notice under the menu button, e.g. while reconnecting
// (docs/adr/0009-quick-menu.md).
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root
    property QtObject overlay

    color: "#1d222b"
    border.color: "#3b5bdb"
    border.width: 1
    Material.theme: Material.Dark
    Material.accent: "#4dabf7"

    Row {
        anchors.centerIn: parent
        spacing: 10
        BusyIndicator {
            width: 24
            height: 24
            running: root.visible
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: root.overlay ? root.overlay.noticeText : ""
            color: "white"
            font.pixelSize: 13
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
