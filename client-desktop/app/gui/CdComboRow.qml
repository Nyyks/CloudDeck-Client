// Label (+ description) and a drop-down; stacks vertically when narrow (phones, the drawer).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import "CdTheme.js" as T
import "CdFocus.js" as F

GridLayout {
    id: row
    property string text: ""
    property string description: ""
    property alias model: box.model
    property alias currentIndex: box.currentIndex
    property alias combo: box
    signal activated(int index)

    Layout.fillWidth: true
    Layout.leftMargin: 14
    Layout.rightMargin: 14
    Layout.topMargin: 6
    Layout.bottomMargin: 6
    columns: width < 440 ? 1 : 2
    columnSpacing: 16
    rowSpacing: 8

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 3
        Text { Layout.fillWidth: true; text: row.text; color: T.text; font.pixelSize: 15; wrapMode: Text.Wrap }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: row.description
            color: T.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
    }
    AutoResizingComboBox {
        id: box
        Layout.alignment: row.columns === 1 ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignVCenter)
        onActivated: function(i) { row.activated(i) }
        onCountChanged: recalculateWidth()
        Component.onCompleted: recalculateWidth()
        onActiveFocusChanged: if (activeFocus) F.ensureVisible(box)
        CdFocusRing { shown: box.visualFocus; ringRadius: 6 }
    }
}
