// Small pill toggle on the home screen (microphone, clipboard, ...).
import QtQuick 2.9
import QtQuick.Controls 2.5
import "CdTheme.js" as T
import "CdFocus.js" as F

Button {
    id: control
    property bool on: false
    focusPolicy: Qt.StrongFocus
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    leftPadding: 16
    rightPadding: 16
    topPadding: 8
    bottomPadding: 8
    font.pixelSize: 14
    implicitHeight: 44
    implicitWidth: label.implicitWidth + leftPadding + rightPadding
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    onActiveFocusChanged: if (activeFocus) F.ensureVisible(control)

    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: control.on ? T.accentText : T.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: height / 2
        color: control.on ? T.accentSoft : control.down ? T.controlDown : control.hovered ? T.rowHover : "transparent"
        border.width: 1
        border.color: control.on ? T.accentBorder : T.border2
        CdFocusRing { shown: control.visualFocus; ringRadius: height / 2 }
    }
}
