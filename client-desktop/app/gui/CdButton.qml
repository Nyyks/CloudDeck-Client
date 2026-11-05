// Home-screen button. kind: "primary" (filled), "secondary" (outlined), "ghost", "link".
// At least 48 px high for touch; Return/Enter (keyboard, TV remote) and Space (gamepad A) click.
import QtQuick 2.9
import QtQuick.Controls 2.5
import "CdTheme.js" as T
import "CdFocus.js" as F

Button {
    id: control
    property string kind: "secondary"
    property color fill: T.accent
    property color fillText: "#ffffff"
    property real radius: 12
    property url iconUrl: ""
    property int iconSize: 18
    property bool stacked: false        // icon above the text (round Connect button)
    property int minSize: 48

    focusPolicy: Qt.StrongFocus
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    font.pixelSize: 15
    font.weight: Font.DemiBold
    leftPadding: kind === "link" ? 10 : 18
    rightPadding: leftPadding
    topPadding: 10
    bottomPadding: 10
    implicitWidth: Math.max(minSize, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(minSize, contentItem.implicitHeight + topPadding + bottomPadding)
    opacity: enabled ? 1 : 0.45

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    onActiveFocusChanged: if (activeFocus) F.ensureVisible(control)

    contentItem: Item {
        implicitWidth: grid.implicitWidth
        implicitHeight: grid.implicitHeight
        Grid {
            id: grid
            anchors.centerIn: parent
            columns: control.stacked ? 1 : 2
            spacing: control.stacked ? 6 : 10
            horizontalItemAlignment: Grid.AlignHCenter
            verticalItemAlignment: Grid.AlignVCenter
            Image {
                visible: control.iconUrl != ""
                source: control.iconUrl
                width: control.iconSize
                height: control.iconSize
                sourceSize.width: control.iconSize * 2
                sourceSize.height: control.iconSize * 2
            }
            Text {
                text: control.text
                font: control.font
                color: control.kind === "primary" ? control.fillText
                     : control.kind === "link" ? (control.hovered ? "#ffffff" : T.accentText) : T.text
            }
        }
    }

    background: Rectangle {
        implicitHeight: control.minSize
        radius: control.radius
        color: control.kind === "primary"
               ? (control.down ? Qt.darker(control.fill, 1.2) : control.hovered ? Qt.lighter(control.fill, 1.08) : control.fill)
               : control.kind === "secondary"
                 ? (control.down ? T.controlDown : control.hovered ? T.controlHover : T.control)
                 : (control.down ? T.controlDown : control.hovered ? T.rowHover : "transparent")
        border.width: control.kind === "secondary" ? 1 : 0
        border.color: T.border2
        CdFocusRing { shown: control.visualFocus; ringRadius: control.radius }
    }
}
