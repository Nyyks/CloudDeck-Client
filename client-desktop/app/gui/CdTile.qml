// Quick-action tile: title, short status line, optional icon. "active" tints it.
import QtQuick 2.9
import QtQuick.Controls 2.5
import "CdTheme.js" as T
import "CdFocus.js" as F

Button {
    id: control
    property string subtitle: ""
    property url iconUrl: ""
    property bool active: false

    focusPolicy: Qt.StrongFocus
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    padding: 16
    implicitWidth: 160
    implicitHeight: Math.max(76, col.implicitHeight + topPadding + bottomPadding)
    opacity: enabled ? 1 : 0.45

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    onActiveFocusChanged: if (activeFocus) F.ensureVisible(control)

    contentItem: Item {
        implicitWidth: 120
        implicitHeight: col.implicitHeight
        Column {
            id: col
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Image {
                visible: control.iconUrl != ""
                source: control.iconUrl
                width: 22; height: 22
                sourceSize.width: 44; sourceSize.height: 44
            }
            Text {
                width: parent.width
                text: control.text
                color: T.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: text !== ""
                text: control.subtitle
                color: control.active ? T.accentText : T.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        radius: 14
        color: control.active ? T.accentSoft : control.down ? T.controlDown : control.hovered ? T.rowHover : T.surface
        border.width: 1
        border.color: control.active ? T.accentBorder : T.border
        CdFocusRing { shown: control.visualFocus; ringRadius: 14 }
    }
}
