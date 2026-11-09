// CloudDeck-Client: in-stream menu panel (docs/adr/0009-quick-menu.md, rev. 3).
// Shown in its own window below the round button; closes when it loses focus.
// Layout: quick toggle tiles on top, then one card per topic, session actions at the bottom.
// Built from the home screen's controls (Cd*.qml) so both look the same.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15

import CloudDeckMic 1.0
import StreamingPreferences 1.0
import CloudDeckUsb 1.0
import "CdTheme.js" as T
import "CdFocus.js" as F

Pane {
    id: root
    property QtObject overlay
    property bool confirmShutDown: false

    padding: 0
    focus: true
    Material.theme: Material.Dark
    Material.accent: T.accent
    background: Rectangle { color: T.bg; border.color: T.border; border.width: 1 }

    function moveFocus(forward) {
        var it = root.Window.activeFocusItem
        if (!it) return
        var next = it.nextItemInFocusChain(forward)
        if (next) next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
    }

    Keys.onEscapePressed: overlay.closeMenu()
    Keys.onUpPressed: moveFocus(false)
    Keys.onDownPressed: moveFocus(true)
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

    // A square quick toggle: icon, name, state line.
    component QuickTile: Button {
        id: tile
        property url iconUrl: ""
        property bool on: false
        property string status: on ? qsTr("On") : qsTr("Off")
        Layout.fillWidth: true
        Layout.preferredWidth: 100
        Layout.preferredHeight: 84
        focusPolicy: Qt.StrongFocus
        topInset: 0; bottomInset: 0; leftInset: 0; rightInset: 0
        padding: 10
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        onActiveFocusChanged: if (activeFocus) F.ensureVisible(tile)
        contentItem: Column {
            spacing: 4
            Image {
                source: tile.iconUrl
                width: 20; height: 20
                sourceSize.width: 40; sourceSize.height: 40
                opacity: tile.on ? 1 : 0.75
            }
            Text {
                width: parent.width
                text: tile.text
                color: T.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: tile.status
                color: tile.on ? T.accentText : T.muted
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
        background: Rectangle {
            radius: 12
            color: tile.on ? T.accentSoft : tile.down ? T.controlDown : tile.hovered ? T.rowHover : T.surface
            border.width: 1
            border.color: tile.on ? T.accentBorder : T.border
            CdFocusRing { shown: tile.visualFocus; ringRadius: 12 }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: T.surface
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: T.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 6
                spacing: 12
                Image {
                    source: "qrc:/res/clouddeck-glyph.svg"
                    sourceSize.width: 48
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    fillMode: Image.PreserveAspectFit
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Text { text: "CloudDeck"; font.pixelSize: 16; font.weight: Font.Bold; color: T.text }
                    Text { text: overlay.streamInfo; font.pixelSize: 12; color: T.muted; elide: Text.ElideRight; Layout.fillWidth: true }
                }
                CdButton {
                    kind: "ghost"
                    iconUrl: "qrc:/res/cd/close.svg"
                    iconSize: 16
                    text: ""
                    onClicked: overlay.closeMenu()
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: col.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            ColumnLayout {
                id: col
                x: 12
                y: 12
                width: flick.width - 24
                spacing: 12

                // ---- quick toggles
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8
                    QuickTile {
                        visible: overlay.canToggleFullscreen
                        text: qsTr("Fullscreen")
                        iconUrl: "qrc:/res/cd/fullscreen.svg"
                        on: overlay.fullscreen
                        onClicked: overlay.fullscreen = !overlay.fullscreen
                    }
                    QuickTile {
                        text: qsTr("Sound")
                        iconUrl: "qrc:/res/cd/speaker.svg"
                        on: !overlay.audioMuted
                        status: on ? qsTr("On") : qsTr("Muted")
                        onClicked: overlay.audioMuted = !overlay.audioMuted
                    }
                    QuickTile {
                        text: qsTr("Microphone")
                        iconUrl: "qrc:/res/cd/mic.svg"
                        on: overlay.micEnabled && !CloudDeckMic.muted
                        status: !overlay.micEnabled ? qsTr("Off") : CloudDeckMic.muted ? qsTr("Muted") : qsTr("On")
                        onClicked: {
                            if (!overlay.micEnabled) overlay.micEnabled = true
                            else CloudDeckMic.muted = !CloudDeckMic.muted
                        }
                    }
                    QuickTile {
                        text: qsTr("Stats")
                        iconUrl: "qrc:/res/cd/stats.svg"
                        on: overlay.statsVisible
                        onClicked: overlay.statsVisible = !overlay.statsVisible
                    }
                    QuickTile {
                        text: qsTr("Keyboard")
                        iconUrl: "qrc:/res/cd/keyboard.svg"
                        on: overlay.keyboardVisible
                        status: on ? qsTr("Shown") : qsTr("Esc, F-keys…")
                        onClicked: { overlay.toggleKeyboard(); overlay.closeMenu() }
                    }
                    QuickTile {
                        // CloudDeck-Client: Win, Alt+Tab... on/off for this stream (Ctrl+Alt+Shift+K)
                        visible: overlay.canCaptureSystemKeys
                        text: qsTr("System keys")
                        iconUrl: "qrc:/res/cd/keyboard.svg"
                        on: overlay.systemKeysOn
                        status: on ? qsTr("To CloudDeck") : qsTr("On this PC")
                        onClicked: overlay.toggleSystemKeys()
                    }
                    QuickTile {
                        visible: overlay.hasGamepad
                        text: qsTr("Controller")
                        iconUrl: "qrc:/res/cd/mouse.svg"
                        on: overlay.controllerMouse
                        status: on ? qsTr("As mouse") : qsTr("As gamepad")
                        onClicked: overlay.controllerMouse = !overlay.controllerMouse
                    }
                }
                CdText {
                    Layout.leftMargin: 4
                    visible: !overlay.micEnabled
                    text: qsTr("Turning the microphone on reconnects the stream for a second.")
                }

                // ---- display and mouse
                CdCard {
                    title: qsTr("Display, mouse and keyboard")
                    CdChoiceRow {
                        visible: overlay.canFitToWindow
                        text: qsTr("Resolution")
                        description: qsTr("The stream restarts at the new size; your app keeps running.")
                        options: overlay.resolutionChoices
                        value: overlay.resolution
                        onPicked: function(v) { overlay.setResolution(v) }
                    }
                    CdSegmented {
                        Layout.fillWidth: true
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.topMargin: 6
                        options: [{ label: qsTr("Game mouse"), value: false }, { label: qsTr("Desktop mouse"), value: true }]
                        value: overlay.desktopMouse
                        onPicked: function(v) { overlay.desktopMouse = v }
                    }
                    CdText {
                        Layout.topMargin: 6
                        text: overlay.desktopMouse
                              ? qsTr("The cursor can leave the stream window, like on a local desktop.")
                              : qsTr("The stream captures the mouse. Ctrl+Alt+Shift+Z releases it.")
                    }
                    // CloudDeck-Client: system shortcuts (docs/keyboard-capture.md), saved
                    CdChoiceRow {
                        visible: overlay.canCaptureSystemKeys
                        text: qsTr("Win, Alt+Tab and other system keys")
                        description: Qt.platform.os === "windows"
                                     ? qsTr("Always also covers other active windows. Saved; the System keys tile switches them for this stream.")
                                     : qsTr("Saved; the System keys tile switches them for this stream.")
                        options: [{ text: qsTr("On this PC"), val: StreamingPreferences.CSK_OFF },
                                  { text: qsTr("CloudDeck, window active"), val: StreamingPreferences.CSK_FOCUSED },
                                  { text: qsTr("CloudDeck, always"), val: StreamingPreferences.CSK_ALWAYS }]
                        value: overlay.systemKeysMode
                        onPicked: function(v) { overlay.systemKeysMode = v }
                    }
                }

                // ---- controller
                CdCard {
                    title: qsTr("Controller")
                    CdSwitch {
                        Layout.fillWidth: true
                        text: qsTr("Nintendo button layout")
                        description: qsTr("Swaps A/B and X/Y")
                        checked: overlay.nintendoLayout
                        onToggled: overlay.nintendoLayout = checked
                    }
                    CdSwitch {
                        Layout.fillWidth: true
                        text: qsTr("On-screen controller")
                        description: qsTr("For touchscreens")
                        checked: overlay.controllerVisible
                        onToggled: overlay.toggleController()
                    }
                    CdSegmented {
                        visible: overlay.controllerVisible
                        Layout.fillWidth: true
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.bottomMargin: 6
                        options: [{ label: qsTr("Xbox"), value: false }, { label: qsTr("PlayStation"), value: true }]
                        value: overlay.controllerPlaystation
                        onPicked: function(v) { overlay.controllerPlaystation = v }
                    }
                    CdText {
                        text: qsTr("Select+L1+R1+A: controller as mouse · +B: this menu · +Y: keyboard")
                    }
                }

                // ---- microphone device
                CdCard {
                    visible: overlay.micEnabled
                    title: qsTr("Microphone")
                    CdComboRow {
                        text: qsTr("Input device")
                        model: [qsTr("System default")].concat(CloudDeckMic.devices)
                        currentIndex: Math.max(0, CloudDeckMic.devices.indexOf(CloudDeckMic.device) + 1)
                        onActivated: function(i) { CloudDeckMic.device = i === 0 ? "" : CloudDeckMic.devices[i - 1] }
                    }
                }

                // ---- USB
                CdCard {
                    title: qsTr("USB devices")
                    CdText {
                        visible: !CloudDeckUsb.supported || CloudDeckUsb.devices.length === 0
                        text: !CloudDeckUsb.supported ? qsTr("USB forwarding isn't available on this PC.")
                                                      : qsTr("No USB devices found.")
                    }
                    Repeater {
                        model: CloudDeckUsb.supported ? CloudDeckUsb.devices : []
                        delegate: CdSwitch {
                            Layout.fillWidth: true
                            text: modelData.description
                            description: modelData.attached ? qsTr("On your CloudDeck")
                                                            : (modelData.forward ? qsTr("Connecting...") : modelData.busid)
                            checked: modelData.forward
                            onToggled: CloudDeckUsb.setForward(modelData.instanceId, checked)
                        }
                    }
                    CdText { visible: CloudDeckUsb.supported && CloudDeckUsb.status !== ""; text: CloudDeckUsb.status }
                    CdButtonRow {
                        visible: CloudDeckUsb.supported
                        CdButton { kind: "ghost"; text: qsTr("Refresh"); onClicked: CloudDeckUsb.refresh() }
                    }
                }

                // ---- session
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    CdButton {
                        Layout.fillWidth: true
                        text: qsTr("Reconnect")
                        iconUrl: "qrc:/res/cd/restart.svg"
                        onClicked: { overlay.closeMenu(); overlay.reconnect() }
                    }
                    CdButton {
                        Layout.fillWidth: true
                        text: qsTr("Disconnect")
                        onClicked: overlay.disconnectStream()
                    }
                }
                CdButton {
                    visible: overlay.canShutDown
                    Layout.fillWidth: true
                    kind: root.confirmShutDown ? "primary" : "secondary"
                    fill: "#c92a2a"
                    iconUrl: "qrc:/res/cd/power.svg"
                    text: root.confirmShutDown ? qsTr("Click again to shut down") : qsTr("Shut down my CloudDeck")
                    onClicked: {
                        if (!root.confirmShutDown) { root.confirmShutDown = true; return }
                        overlay.shutDown()
                    }
                }

                // ---- footer
                CdSwitch {
                    Layout.fillWidth: true
                    text: qsTr("Show the menu button")
                    description: qsTr("Ctrl+Alt+Shift+%1 or Select+L1+R1+B opens this menu.").arg(overlay.menuKey)
                    checked: overlay.showButton
                    onToggled: overlay.showButton = checked
                }
            }
        }
    }
}
