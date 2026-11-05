// "Starting your CloudDeck..." progress line with Cancel, plus the last error.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 8
    RowLayout {
        visible: CloudDeck.connecting || cdView.launchText !== ""
        Layout.fillWidth: true
        spacing: 12
        BusyIndicator { running: parent.visible; implicitWidth: 32; implicitHeight: 32 }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: T.text
            font.pixelSize: 14
            text: CloudDeck.connecting ? CloudDeck.progressText : cdView.launchText
        }
        CdButton {
            visible: CloudDeck.connecting
            kind: "ghost"
            text: qsTr("Cancel")
            onClicked: CloudDeck.cancelConnect()
        }
    }
    RowLayout {
        visible: CloudDeck.errorText !== ""
        Layout.fillWidth: true
        spacing: 12
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: T.errorText
            font.pixelSize: 14
            text: CloudDeck.errorText
        }
        CdButton {
            kind: "ghost"
            text: qsTr("Dismiss")
            onClicked: CloudDeck.clearError()
        }
    }
}
