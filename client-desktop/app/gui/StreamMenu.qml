// CloudDeck-Client: in-stream menu panel (docs/adr/0009-quick-menu.md).
// Shown in its own window below the round button; closes when it loses focus.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15

import CloudDeckMic 1.0
import CloudDeckUsb 1.0

Pane {
    id: root
    property QtObject overlay
    property bool confirmShutDown: false

    padding: 0
    focus: true
    Material.theme: Material.Dark
    Material.accent: "#4dabf7"
    background: Rectangle { color: "#1d222b"; border.color: "#2f3746"; border.width: 1 }

    Keys.onEscapePressed: overlay.closeMenu()
    Shortcut { sequence: "Ctrl+Alt+Shift+" + overlay.menuKey; onActivated: overlay.closeMenu() }

    // Close when the user clicks back into the stream (or anywhere else).
    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window.active && overlay.menuOpen) overlay.closeMenu()
        }
    }
    Connections {
        target: overlay
        function onChanged() { if (!overlay.menuOpen) root.confirmShutDown = false }
    }

    component SectionLabel: Label {
        Layout.topMargin: 14
        Layout.leftMargin: 16
        font.pixelSize: 12
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: "#8b95a7"
    }
    component SwitchRow: Switch {
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8
    }
    component Hint: Label {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        wrapMode: Text.Wrap
        font.pixelSize: 12
        color: "#8b95a7"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#3b5bdb" }
                GradientStop { position: 1; color: "#0b7285" }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 6
                spacing: 12
                Image {
                    source: "qrc:/res/clouddeck-glyph.svg"
                    sourceSize.width: 64
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    fillMode: Image.PreserveAspectFit
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Label { text: "CloudDeck"; font.pixelSize: 17; font.bold: true; color: "white" }
                    Label { text: overlay.streamInfo; font.pixelSize: 12; color: "#dbe4ff" }
                }
                ToolButton {
                    text: "✕"
                    font.pixelSize: 16
                    Material.foreground: "white"
                    onClicked: overlay.closeMenu()
                }
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 0

                // ---- display
                SectionLabel { text: qsTr("Display") }
                SwitchRow {
                    visible: overlay.canToggleFullscreen
                    text: qsTr("Fullscreen")
                    checked: overlay.fullscreen
                    onToggled: overlay.fullscreen = checked
                }
                SwitchRow {
                    visible: overlay.canFitToWindow
                    text: qsTr("Fit the CloudDeck's resolution to this window")
                    checked: overlay.fitToWindow
                    onToggled: overlay.fitToWindow = checked
                }
                SwitchRow {
                    text: qsTr("Performance stats")
                    checked: overlay.statsVisible
                    onToggled: overlay.statsVisible = checked
                }

                // ---- mouse
                SectionLabel { text: qsTr("Mouse") }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    spacing: 8
                    ButtonGroup { id: mouseGroup }
                    Button {
                        Layout.fillWidth: true
                        checkable: true
                        ButtonGroup.group: mouseGroup
                        text: qsTr("Game")
                        checked: !overlay.desktopMouse
                        highlighted: checked
                        onClicked: overlay.desktopMouse = false
                    }
                    Button {
                        Layout.fillWidth: true
                        checkable: true
                        ButtonGroup.group: mouseGroup
                        text: qsTr("Desktop")
                        checked: overlay.desktopMouse
                        highlighted: checked
                        onClicked: overlay.desktopMouse = true
                    }
                }
                Hint {
                    text: overlay.desktopMouse
                          ? qsTr("Free cursor: it can leave the stream window, like on a local desktop. Best for working.")
                          : qsTr("The mouse is captured by the stream. Best for games. Ctrl+Alt+Shift+Z releases it.")
                }

                // ---- audio
                SectionLabel { text: qsTr("Audio") }
                SwitchRow {
                    text: qsTr("Mute the CloudDeck's sound")
                    checked: overlay.audioMuted
                    onToggled: overlay.audioMuted = checked
                }
                SwitchRow {
                    text: qsTr("Send my microphone")
                    checked: overlay.micEnabled
                    onToggled: overlay.micEnabled = checked
                }
                Hint {
                    visible: !overlay.micEnabled
                    text: qsTr("Turning the microphone on or off reconnects the stream for a second.")
                }
                SwitchRow {
                    visible: overlay.micEnabled
                    text: qsTr("Mute my microphone")
                    checked: CloudDeckMic.muted
                    onToggled: CloudDeckMic.muted = checked
                }
                ComboBox {
                    visible: overlay.micEnabled
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    model: [qsTr("System default microphone")].concat(CloudDeckMic.devices)
                    currentIndex: Math.max(0, CloudDeckMic.devices.indexOf(CloudDeckMic.device) + 1)
                    onActivated: CloudDeckMic.device = index === 0 ? "" : CloudDeckMic.devices[index - 1]
                }

                // ---- USB
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: qsTr("USB devices"); Layout.fillWidth: true }
                    ToolButton {
                        visible: CloudDeckUsb.supported
                        Layout.topMargin: 8
                        text: qsTr("Refresh")
                        font.pixelSize: 12
                        onClicked: CloudDeckUsb.refresh()
                    }
                }
                Hint {
                    visible: !CloudDeckUsb.supported || CloudDeckUsb.devices.length === 0
                    text: !CloudDeckUsb.supported ? qsTr("USB forwarding isn't available on this PC.")
                                                  : qsTr("No USB devices found.")
                }
                Repeater {
                    model: CloudDeckUsb.supported ? CloudDeckUsb.devices : []
                    delegate: CheckBox {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        text: modelData.description +
                              (modelData.attached ? "  ·  " + qsTr("on your CloudDeck")
                                                  : (modelData.forward ? "  ·  " + qsTr("connecting...") : ""))
                        checked: modelData.forward
                        onToggled: CloudDeckUsb.setForward(modelData.instanceId, checked)
                    }
                }
                Hint {
                    visible: CloudDeckUsb.supported && CloudDeckUsb.status !== ""
                    text: CloudDeckUsb.status
                }

                // ---- session
                SectionLabel { text: qsTr("Session") }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    spacing: 8
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Reconnect")
                        onClicked: { overlay.closeMenu(); overlay.reconnect() }
                    }
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Disconnect")
                        onClicked: overlay.disconnectStream()
                    }
                }
                Button {
                    visible: overlay.canShutDown
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    text: root.confirmShutDown ? qsTr("Click again to shut down your CloudDeck") : qsTr("Shut down my CloudDeck")
                    Material.background: root.confirmShutDown ? "#c92a2a" : "#3a2328"
                    Material.foreground: "white"
                    onClicked: {
                        if (!root.confirmShutDown) { root.confirmShutDown = true; return }
                        overlay.shutDown()
                    }
                }

                // ---- footer
                Rectangle { Layout.fillWidth: true; Layout.topMargin: 14; Layout.preferredHeight: 1; color: "#2f3746" }
                SwitchRow {
                    text: qsTr("Show the menu button during streams")
                    checked: overlay.showButton
                    onToggled: overlay.showButton = checked
                }
                Hint {
                    Layout.bottomMargin: 14
                    text: qsTr("Ctrl+Alt+Shift+%1 opens and closes this menu.").arg(overlay.menuKey)
                }
            }
        }
    }
}
