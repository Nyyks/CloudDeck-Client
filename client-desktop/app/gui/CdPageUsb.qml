// Settings page: USB forwarding (docs/adr/0006-usb-passthrough.md).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeckUsb 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("USB devices")
        CdText { text: qsTr("Devices you switch on are attached to your CloudDeck while you stream.") }
        CdText { hint: false; text: CloudDeckUsb.status }
        Repeater {
            model: CloudDeckUsb.devices
            delegate: CdSwitch {
                Layout.fillWidth: true
                text: modelData.description
                description: modelData.warning !== ""
                             ? modelData.warning
                             : modelData.busid + (modelData.attached ? "  ·  " + qsTr("on your CloudDeck") : "")
                descriptionColor: modelData.warning !== "" ? T.warnText : T.muted
                checked: modelData.forward
                onToggled: CloudDeckUsb.setForward(modelData.instanceId, checked)
            }
        }
        CdText {
            visible: CloudDeckUsb.supported && CloudDeckUsb.devices.length === 0
            text: qsTr("No USB devices found.")
        }
        CdButtonRow {
            CdButton {
                visible: CloudDeckUsb.supported
                text: qsTr("Refresh")
                onClicked: CloudDeckUsb.refresh()
            }
            CdButton {
                visible: Qt.platform.os === "windows" && CloudDeckUsb.supported && !CloudDeckUsb.serverInstalled
                kind: "primary"
                text: qsTr("Install usbipd-win")
                onClicked: CloudDeckUsb.installServer()
            }
        }
    }
}
