// Settings page: mouse, touch and keyboard (Moonlight's input settings) and the
// configurable stream shortcuts (docs/adr/0012-shortcuts.md).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeckShortcuts 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("Mouse")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Desktop mouse by default")
            description: qsTr("The cursor can leave the stream, like on a local desktop. Good for working, not for most games. Ctrl+Alt+Shift+M or the menu switch it during a stream.")
            checked: StreamingPreferences.absoluteMouseMode
            onToggled: StreamingPreferences.absoluteMouseMode = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Swap the left and right mouse buttons")
            checked: StreamingPreferences.swapMouseButtons
            onToggled: StreamingPreferences.swapMouseButtons = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Reverse the scrolling direction")
            checked: StreamingPreferences.reverseScrollDirection
            onToggled: StreamingPreferences.reverseScrollDirection = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Touchscreen works like a trackpad")
            description: qsTr("Off: your finger moves the pointer directly to where you touch.")
            checked: !StreamingPreferences.absoluteTouchMode
            onToggled: StreamingPreferences.absoluteTouchMode = !checked
        }
    }

    CdCard {
        title: qsTr("Keyboard")
        // CloudDeck-Client: three modes (docs/keyboard-capture.md)
        CdChoiceRow {
            id: captureKeys
            Layout.fillWidth: true
            enabled: SystemProperties.hasDesktopEnvironment
            text: qsTr("Send Win, Alt+Tab and other system shortcuts to the CloudDeck")
            options: [{ text: qsTr("Never"), val: StreamingPreferences.CSK_OFF },
                      { text: qsTr("While the stream window is active"), val: StreamingPreferences.CSK_FOCUSED },
                      { text: qsTr("Always while streaming"), val: StreamingPreferences.CSK_ALWAYS }]
            value: SystemProperties.hasDesktopEnvironment ? StreamingPreferences.captureSysKeysMode : StreamingPreferences.CSK_ALWAYS
            onPicked: function(v) { StreamingPreferences.captureSysKeysMode = v }
        }
        CdText {
            Layout.fillWidth: true
            color: T.textSecondary
            text: {
                var m = captureKeys.value
                if (m === StreamingPreferences.CSK_OFF)
                    return qsTr("System shortcuts stay on this computer.")
                if (m === StreamingPreferences.CSK_ALWAYS && Qt.platform.os === "windows")
                    return qsTr("Also while another window on this PC is active: Win, Win+key, Alt+Tab, Alt+Esc and Ctrl+Esc go to the CloudDeck until the stream ends. Leave the stream window with the mouse or Ctrl+Alt+Shift+D.")
                if (m === StreamingPreferences.CSK_ALWAYS)
                    return qsTr("Linux can't take these keys from other windows, so this works like \"while the stream window is active\".")
                return qsTr("Whenever the stream window has the keyboard, also with the mouse released or in a window. Click another window to use them here again.")
            }
        }
        CdText {
            Layout.fillWidth: true
            color: T.textSecondary
            text: qsTr("Ctrl+Alt+Shift+K switches this on or off during a stream. Some keys, like Ctrl+Alt+Del or Win+L, can't be taken by any app.")
        }
    }

    CdCard {
        title: qsTr("Shortcuts during a stream")
        CdText {
            text: qsTr("All shortcuts are Ctrl+Alt+Shift plus a key. Changes apply to the next stream.")
        }
        CdButtonRow {
            visible: !shortcutsList.visible
            CdButton {
                text: qsTr("Change shortcuts...")
                onClicked: { shortcutsList.visible = true; firstShortcut.forceActiveFocus(Qt.TabFocusReason) }
            }
        }
        ColumnLayout {
            id: shortcutsList
            visible: false
            Layout.fillWidth: true
            spacing: 0
            Repeater {
                model: CloudDeckShortcuts.actions
                delegate: CdComboRow {
                    text: modelData.label
                    model: CloudDeckShortcuts.keys
                    currentIndex: CloudDeckShortcuts.keys.indexOf(modelData.key)
                    Component.onCompleted: if (index === 0) firstShortcut.target = combo
                    onActivated: function(i) {
                        var err = CloudDeckShortcuts.setKey(modelData.id, combo.currentText)
                        shortcutError.text = err
                        if (err !== "") combo.currentIndex = CloudDeckShortcuts.keys.indexOf(modelData.key)
                    }
                }
            }
            CdText { id: shortcutError; color: T.warnText; text: "" }
            CdButtonRow {
                CdButton {
                    text: qsTr("Reset to defaults")
                    onClicked: { CloudDeckShortcuts.resetDefaults(); shortcutError.text = "" }
                }
                CdButton {
                    text: qsTr("Done")
                    kind: "ghost"
                    onClicked: shortcutsList.visible = false
                }
            }
        }
        QtObject {
            id: firstShortcut
            property Item target: null
            function forceActiveFocus(reason) { if (target) target.forceActiveFocus(reason) }
        }
    }
}
