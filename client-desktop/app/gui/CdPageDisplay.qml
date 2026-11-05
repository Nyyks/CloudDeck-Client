// Settings page: display and stream quality. Includes Moonlight's video settings
// (docs/adr/0013-home-layouts.md, "one settings menu"; docs/adr/0007-multi-monitor.md).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import CloudDeckDisplay 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("Quality")
        CdStreamQuality { Layout.fillWidth: true }
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
        }
        CdText {
            text: CloudDeck.netTestHasRecommendation ? CloudDeck.netTestRecommendation
                                                     : qsTr("Test your connection (Network) to get a recommendation.")
        }
    }

    CdCard {
        title: qsTr("The CloudDeck's screen")
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
        title: qsTr("Picture")
        CdChoiceRow {
            visible: SystemProperties.hasDesktopEnvironment
            enabled: !SystemProperties.rendererAlwaysFullScreen
            text: qsTr("Display mode")
            description: qsTr("Fullscreen is usually fastest; borderless windowed works better with Alt+Tab and overlays.")
            options: {
                var o = [{ text: qsTr("Fullscreen"), val: StreamingPreferences.WM_FULLSCREEN },
                         { text: qsTr("Borderless windowed"), val: StreamingPreferences.WM_FULLSCREEN_DESKTOP },
                         { text: qsTr("Windowed"), val: StreamingPreferences.WM_WINDOWED }]
                for (var i = 0; i < o.length; i++) {
                    if (o[i].val === StreamingPreferences.recommendedFullScreenMode) {
                        var r = o.splice(i, 1)[0]
                        r.text += " " + qsTr("(recommended)")
                        o.unshift(r)
                        break
                    }
                }
                return o
            }
            value: StreamingPreferences.windowMode
            onPicked: function(v) { StreamingPreferences.windowMode = v }
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("V-Sync")
            description: qsTr("Off allows lower latency but can show tearing.")
            checked: StreamingPreferences.enableVsync
            onToggled: StreamingPreferences.enableVsync = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Frame pacing")
            description: qsTr("Reduces micro-stutter by holding back frames that arrive too early. Needs V-Sync.")
            enabled: StreamingPreferences.enableVsync
            checked: StreamingPreferences.enableVsync && StreamingPreferences.framePacing
            onToggled: StreamingPreferences.framePacing = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("HDR")
            description: SystemProperties.supportsHdr ? qsTr("Some games need an HDR display on the CloudDeck to turn HDR on.")
                                                      : qsTr("HDR streaming isn't supported on this PC.")
            enabled: SystemProperties.supportsHdr
            checked: SystemProperties.supportsHdr && StreamingPreferences.enableHdr
            onToggled: StreamingPreferences.enableHdr = checked
        }
    }
}
