// Console sidebar entry (and the page chips on phones).
import QtQuick 2.9
import QtQuick.Controls 2.5
import "CdTheme.js" as T
import "CdFocus.js" as F

Button {
    id: control
    property url iconUrl: ""
    property bool selected: false
    property bool compact: false     // phone: chip in a horizontal strip
    focusPolicy: Qt.StrongFocus
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    leftPadding: 12
    rightPadding: compact ? 14 : 12
    implicitHeight: 48
    implicitWidth: row.implicitWidth + leftPadding + rightPadding
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    onActiveFocusChanged: if (activeFocus) F.ensureVisible(control)

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.verticalCenter: parent.verticalCenter
            spacing: control.compact ? 8 : 12
            Image {
                source: control.iconUrl
                visible: control.iconUrl != ""
                width: 18; height: 18
                sourceSize.width: 36; sourceSize.height: 36
                opacity: control.selected ? 1 : 0.7
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: control.text
                color: control.selected ? "#ffffff" : T.textSecondary
                font.pixelSize: 14
                font.weight: control.selected ? Font.DemiBold : Font.Normal
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
    background: Rectangle {
        radius: 10
        color: control.selected ? T.selected : control.down ? T.controlDown : control.hovered ? T.rowHover : "transparent"
        border.width: control.compact && !control.selected ? 1 : 0
        border.color: T.border2
        CdFocusRing { shown: control.visualFocus; ringRadius: 10 }
    }
}
