// Body or hint text inside a settings card (wraps, indented like the rows).
import QtQuick 2.9
import QtQuick.Layouts 1.3
import "CdTheme.js" as T

Text {
    property bool hint: true
    Layout.fillWidth: true
    Layout.leftMargin: 14
    Layout.rightMargin: 14
    Layout.topMargin: 2
    Layout.bottomMargin: 6
    visible: text !== ""
    wrapMode: Text.Wrap
    color: hint ? T.muted : T.text
    font.pixelSize: hint ? 13 : 15
    lineHeight: 1.15
}
