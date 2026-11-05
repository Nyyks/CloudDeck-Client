// Settings page: display and stream quality (docs/adr/0007-multi-monitor.md).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import CloudDeckDisplay 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("Resolution")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Fit the CloudDeck's resolution to the stream window")
            description: qsTr("The CloudDeck's desktop follows the window, also when you resize it.")
            checked: CloudDeckDisplay.matchResolution || CloudDeckDisplay.multiMonitor
            enabled: !CloudDeckDisplay.multiMonitor
            onToggled: CloudDeckDisplay.matchResolution = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Use several monitors")
            description: qsTr("The stream covers the chosen monitors as one window; the CloudDeck sees one wide screen.")
            checked: CloudDeckDisplay.multiMonitor
            onToggled: CloudDeckDisplay.multiMonitor = checked
            Component.onCompleted: CloudDeckDisplay.refresh()
        }
        Repeater {
            model: CloudDeckDisplay.multiMonitor ? CloudDeckDisplay.screens : []
            delegate: CdSwitch {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                text: modelData.label
                checked: CloudDeckDisplay.monitors.indexOf(modelData.name) >= 0
                onToggled: CloudDeckDisplay.setMonitorChosen(modelData.name, checked)
            }
        }
        CdText {
            visible: CloudDeckDisplay.multiMonitor && text !== ""
            color: CloudDeckDisplay.summaryIsError ? T.warnText : T.muted
            text: CloudDeckDisplay.summary
        }
    }

    CdCard {
        title: qsTr("Quality")
        CdText { hint: false; text: cdView.streamSummary() }
        CdButtonRow {
            CdButton {
                text: qsTr("Use recommended")
                kind: CloudDeck.netTestHasRecommendation ? "primary" : "secondary"
                enabled: CloudDeck.netTestHasRecommendation
                onClicked: CloudDeck.applyRecommendation()
            }
            CdButton {
                text: qsTr("Low bandwidth")
                onClicked: CloudDeck.applyLowBandwidth()
            }
            CdButton {
                text: qsTr("All streaming settings...")
                kind: "ghost"
                onClicked: cdView.openMoonlightSettings()
            }
        }
        CdText {
            text: CloudDeck.netTestHasRecommendation ? CloudDeck.netTestRecommendation
                                                     : qsTr("Test your connection to get a recommendation.")
        }
    }
}
