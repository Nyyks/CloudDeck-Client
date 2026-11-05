// Settings page: microphone (docs/protocol-extensions.md section 1).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeckMic 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("Microphone")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Send my microphone to the CloudDeck")
            description: qsTr("Mute it during a stream with Ctrl+Alt+Shift+%1 or from the menu.").arg(cdView.micMuteKey())
            checked: CloudDeckMic.enabled
            onToggled: CloudDeckMic.enabled = checked
        }
        CdComboRow {
            text: qsTr("Input device")
            model: [qsTr("System default")].concat(CloudDeckMic.devices)
            currentIndex: Math.max(0, CloudDeckMic.devices.indexOf(CloudDeckMic.device) + 1)
            onActivated: function(i) { CloudDeckMic.device = i === 0 ? "" : CloudDeckMic.devices[i - 1] }
            Component.onCompleted: CloudDeckMic.refreshDevices()
        }
        CdButtonRow {
            CdButton {
                text: CloudDeckMic.testing ? qsTr("Stop test") : qsTr("Test the microphone")
                onClicked: CloudDeckMic.testing ? CloudDeckMic.stopTest() : CloudDeckMic.startTest()
            }
        }
        Rectangle {
            visible: CloudDeckMic.testing || CloudDeckMic.level > 0
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 6
            height: 8
            radius: 4
            color: T.inset
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, CloudDeckMic.level))
                height: parent.height
                radius: 4
                color: T.accent
            }
        }
        CdText { text: CloudDeckMic.status }
    }
}
