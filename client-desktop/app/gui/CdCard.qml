// A rounded settings section with an optional small heading.
import QtQuick 2.9
import QtQuick.Layouts 1.3
import "CdTheme.js" as T

Rectangle {
    id: card
    default property alias content: col.data
    property string title: ""
    property int pad: 12
    Layout.fillWidth: true
    implicitWidth: 320
    implicitHeight: col.implicitHeight + 2 * pad
    radius: 16
    color: T.card
    border.width: 1
    border.color: T.border

    ColumnLayout {
        id: col
        x: card.pad
        y: card.pad
        width: card.width - 2 * card.pad
        spacing: 2
        Text {
            visible: card.title !== ""
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.topMargin: 6
            Layout.bottomMargin: 4
            text: card.title
            color: T.muted
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
            font.capitalization: Font.AllUppercase
        }
    }
}
