// Segmented choice (UI style, controller style, settings tabs). Left/Right move between
// segments; Space or Return picks one.
import QtQuick 2.9
import QtQuick.Controls 2.5
import "CdTheme.js" as T
import "CdFocus.js" as F

Rectangle {
    id: seg
    property var options: []        // [{ label, value }]
    property var value
    signal picked(var value)

    function focusCurrent(reason) {
        for (var i = 0; i < rep.count; i++)
            if (seg.options[i].value === seg.value) { rep.itemAt(i).forceActiveFocus(reason); return }
        if (rep.count > 0) rep.itemAt(0).forceActiveFocus(reason)
    }
    function segment(i) { return rep.itemAt(i) }

    implicitWidth: Math.max(1, options.length) * 120 + 8
    implicitHeight: 48
    radius: 12
    color: T.inset
    border.width: 1
    border.color: T.border

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4
        Repeater {
            id: rep
            model: seg.options
            delegate: Button {
                id: b
                readonly property bool current: seg.value === modelData.value
                width: (row.width - (rep.count - 1) * row.spacing) / Math.max(1, rep.count)
                height: row.height
                text: modelData.label
                focusPolicy: Qt.StrongFocus
                topInset: 0
                bottomInset: 0
                leftInset: 0
                rightInset: 0
                onClicked: seg.picked(modelData.value)
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                Keys.onLeftPressed: {
                    if (index > 0) rep.itemAt(index - 1).forceActiveFocus(Qt.TabFocusReason)
                    else event.accepted = false
                }
                Keys.onRightPressed: {
                    if (index < rep.count - 1) rep.itemAt(index + 1).forceActiveFocus(Qt.TabFocusReason)
                    else event.accepted = false
                }
                onActiveFocusChanged: if (activeFocus) F.ensureVisible(seg)
                contentItem: Text {
                    text: b.text
                    color: b.current ? "#ffffff" : T.textSecondary
                    font.pixelSize: 14
                    font.weight: b.current ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: 9
                    color: b.current ? T.controlDown : b.hovered ? T.rowHover : "transparent"
                    CdFocusRing { shown: b.visualFocus; ringRadius: 9 }
                }
            }
        }
    }
}
