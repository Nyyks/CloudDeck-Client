// A settings row: label, optional description, and a switch. The whole row is the target
// (touch); Space or Return toggles it (gamepad A, keyboard, TV-remote OK).
import QtQuick 2.9
import QtQuick.Controls 2.5
import "CdTheme.js" as T
import "CdFocus.js" as F

Switch {
    id: control
    property string description: ""
    property color descriptionColor: T.muted

    focusPolicy: Qt.StrongFocus
    leftPadding: 14
    rightPadding: 14 + 46 + 16
    topPadding: 12
    bottomPadding: 12
    spacing: 0
    implicitWidth: 320
    implicitHeight: Math.max(56, contentItem.implicitHeight + topPadding + bottomPadding)
    opacity: enabled ? 1 : 0.45

    Keys.onReturnPressed: { toggle(); toggled() }
    Keys.onEnterPressed: { toggle(); toggled() }
    onActiveFocusChanged: if (activeFocus) F.ensureVisible(control)

    indicator: Rectangle {
        x: control.width - width - 14
        y: (control.height - height) / 2
        width: 46
        height: 26
        radius: 13
        color: control.checked ? T.accent : "#2b3240"
        Behavior on color { ColorAnimation { duration: 120 } }
        Rectangle {
            width: 20; height: 20; radius: 10
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: "#ffffff"
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }

    contentItem: Column {
        spacing: 3
        Text {
            width: parent.width
            text: control.text
            color: T.text
            font.pixelSize: 15
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: control.description
            color: control.descriptionColor
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
    }

    background: Rectangle {
        radius: 10
        color: control.down ? T.controlDown : control.hovered ? T.rowHover : "transparent"
        CdFocusRing { shown: control.visualFocus; ringRadius: 10 }
    }
}
