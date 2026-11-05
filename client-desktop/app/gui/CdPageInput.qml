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
        CdSwitch {
            id: captureKeys
            Layout.fillWidth: true
            enabled: SystemProperties.hasDesktopEnvironment
            text: qsTr("Send system shortcuts like Alt+Tab to the CloudDeck")
            description: qsTr("Some, like Ctrl+Alt+Del on Windows, can't be captured by any app.")
            checked: StreamingPreferences.captureSysKeysMode !== StreamingPreferences.CSK_OFF || !SystemProperties.hasDesktopEnvironment
            onToggled: StreamingPreferences.captureSysKeysMode = checked ? StreamingPreferences.CSK_FULLSCREEN : StreamingPreferences.CSK_OFF
        }
        CdChoiceRow {
            visible: captureKeys.checked && captureKeys.enabled
            text: qsTr("When")
            options: [{ text: qsTr("In fullscreen"), val: StreamingPreferences.CSK_FULLSCREEN },
                      { text: qsTr("Always"), val: StreamingPreferences.CSK_ALWAYS }]
            value: StreamingPreferences.captureSysKeysMode
            onPicked: function(v) { StreamingPreferences.captureSysKeysMode = v }
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
