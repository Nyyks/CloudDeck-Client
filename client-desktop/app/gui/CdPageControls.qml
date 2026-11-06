// Settings page "Controls": keyboard and mouse, gamepads, USB devices as tabs (one category,
// like Shadow's settings), so the sidebar stays short.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

ColumnLayout {
    id: page
    spacing: 16
    property string tab: "input"

    CdSegmented {
        Layout.fillWidth: true
        Layout.maximumWidth: 620
        options: [{ label: qsTr("Keyboard & mouse"), value: "input" },
                  { label: qsTr("Gamepad"), value: "gamepad" },
                  { label: qsTr("USB devices"), value: "usb" }]
        value: page.tab
        onPicked: function(v) { page.tab = v }
    }
    Loader {
        Layout.fillWidth: true
        source: page.tab === "gamepad" ? "CdPageControllers.qml" : page.tab === "usb" ? "CdPageUsb.qml" : "CdPageInput.qml"
    }
}
