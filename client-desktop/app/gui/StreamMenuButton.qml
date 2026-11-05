// CloudDeck-Client: the round menu button at the top centre of the stream
// (docs/adr/0009-quick-menu.md). The window itself is masked to a circle.
import QtQuick 2.15

Item {
    id: root
    property QtObject overlay

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: mouse.containsMouse || (root.overlay && root.overlay.menuOpen) ? "#3b5bdb" : "#1d222b"
        Behavior on color { ColorAnimation { duration: 120 } }

        Image {
            anchors.centerIn: parent
            width: parent.width * 0.5
            height: width
            source: "qrc:/res/clouddeck-glyph.svg"
            sourceSize.width: 96
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.overlay.toggleMenu()
    }
}
