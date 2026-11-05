// Settings page: layout switch, VM behaviour, clipboard, shortcuts, advanced.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import CloudDeckShortcuts 1.0
import "CdTheme.js" as T
import "CdFocus.js" as F

ColumnLayout {
    id: page
    spacing: 16

    function focusStyle(reason) { styleSwitch.focusCurrent(reason) }

    readonly property var idleChoices: [0, 15, 30, 60, 120, 240]
    function idleLabel(m) { return m === 0 ? qsTr("Never") : (m < 60 ? qsTr("After %1 minutes").arg(m)
                                                                  : m === 60 ? qsTr("After 1 hour") : qsTr("After %1 hours").arg(m / 60)) }
    function idleValues() {
        var v = idleChoices.slice()
        if (v.indexOf(CloudDeck.autoStopIdleMinutes) < 0) { v.push(CloudDeck.autoStopIdleMinutes); v.sort(function(a, b) { return a - b }) }
        return v
    }

    CdCard {
        title: qsTr("Appearance")
        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 6
            Layout.bottomMargin: 8
            columns: width < 520 ? 1 : 2
            columnSpacing: 16
            rowSpacing: 10
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text { Layout.fillWidth: true; text: qsTr("Home screen layout"); color: T.text; font.pixelSize: 15 }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: T.muted
                    font.pixelSize: 13
                    text: qsTr("Console: pages in a sidebar. Launcher: one big button, settings in a drawer.")
                }
            }
            CdSegmented {
                id: styleSwitch
                Layout.preferredWidth: 260
                options: [{ label: qsTr("Console"), value: "console" }, { label: qsTr("Launcher"), value: "launcher" }]
                value: CloudDeck.uiStyle
                onPicked: function(v) { cdView.setUiStyle(v) }
            }
        }
    }

    CdCard {
        title: qsTr("Your CloudDeck")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Stop my CloudDeck when the stream ends")
            description: qsTr("Saves money when you only play for a while.")
            checked: CloudDeck.autoStopOnSessionEnd
            onToggled: CloudDeck.autoStopOnSessionEnd = checked
        }
        CdComboRow {
            id: idleRow
            property var values: page.idleValues()
            text: qsTr("Stop when idle")
            description: qsTr("Stops the CloudDeck when no stream has been running for this long.")
            model: values.map(page.idleLabel)
            currentIndex: Math.max(0, values.indexOf(CloudDeck.autoStopIdleMinutes))
            onActivated: function(i) { CloudDeck.autoStopIdleMinutes = values[i] }
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Share the clipboard")
            description: qsTr("Copied text moves between this device and the CloudDeck while you stream.")
            checked: CloudDeck.clipboardSync
            onToggled: CloudDeck.clipboardSync = checked
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

    CdCard {
        title: qsTr("Advanced")
        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 6
            columns: width < 440 ? 1 : 2
            columnSpacing: 16
            Text { Layout.fillWidth: true; text: qsTr("App to launch on the CloudDeck"); color: T.text; font.pixelSize: 15; wrapMode: Text.Wrap }
            TextField {
                id: appField
                Layout.preferredWidth: 220
                text: CloudDeck.appName
                font.pixelSize: 15
                onEditingFinished: CloudDeck.appName = text
                onActiveFocusChanged: if (activeFocus) F.ensureVisible(appField)
                CdFocusRing { shown: appField.activeFocus; ringRadius: 6 }
            }
        }
        CdButtonRow {
            CdButton {
                text: qsTr("All streaming settings...")
                onClicked: cdView.openMoonlightSettings()
            }
            CdButton {
                text: qsTr("Manage PCs")
                kind: "ghost"
                onClicked: cdView.managePcs()
            }
        }
    }
}
