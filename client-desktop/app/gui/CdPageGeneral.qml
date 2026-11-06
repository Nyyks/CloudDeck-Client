// Settings page: appearance, VM behaviour, clipboard, and the rest of Moonlight's settings
// (host, UI, notices, advanced), so there is one settings menu (docs/adr/0013-home-layouts.md).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import "CdTheme.js" as T
import "CdFocus.js" as F

ColumnLayout {
    id: page
    spacing: 16

    // After a language change the home is rebuilt and focus returns to the language list.
    function focusStyle(reason) { languageRow.combo.forceActiveFocus(reason) }

    readonly property var idleChoices: [0, 15, 30, 60, 120, 240]
    function idleLabel(m) { return m === 0 ? qsTr("Never") : (m < 60 ? qsTr("After %1 minutes").arg(m)
                                                                  : m === 60 ? qsTr("After 1 hour") : qsTr("After %1 hours").arg(m / 60)) }
    function idleValues() {
        var v = idleChoices.slice()
        if (v.indexOf(CloudDeck.autoStopIdleMinutes) < 0) { v.push(CloudDeck.autoStopIdleMinutes); v.sort(function(a, b) { return a - b }) }
        return v
    }

    // Same list as Moonlight's SettingsView (languages with a complete translation).
    readonly property var languages: [
        { text: qsTr("Automatic"), val: StreamingPreferences.LANG_AUTO },
        { text: "Deutsch", val: StreamingPreferences.LANG_DE },
        { text: "English", val: StreamingPreferences.LANG_EN },
        { text: "Français", val: StreamingPreferences.LANG_FR },
        { text: "简体中文", val: StreamingPreferences.LANG_ZH_CN },
        { text: "Norwegian Bokmål", val: StreamingPreferences.LANG_NB_NO },
        { text: "русский", val: StreamingPreferences.LANG_RU },
        { text: "Español", val: StreamingPreferences.LANG_ES },
        { text: "日本語", val: StreamingPreferences.LANG_JA },
        { text: "Tiếng Việt", val: StreamingPreferences.LANG_VI },
        { text: "ภาษาไทย", val: StreamingPreferences.LANG_TH },
        { text: "한국어", val: StreamingPreferences.LANG_KO },
        { text: "Magyar", val: StreamingPreferences.LANG_HU },
        { text: "Nederlands", val: StreamingPreferences.LANG_NL },
        { text: "Svenska", val: StreamingPreferences.LANG_SV },
        { text: "Türkçe", val: StreamingPreferences.LANG_TR },
        { text: "繁體中文", val: StreamingPreferences.LANG_ZH_TW },
        { text: "Português", val: StreamingPreferences.LANG_PT },
        { text: "Português do Brasil", val: StreamingPreferences.LANG_PT_BR },
        { text: "Ελληνικά", val: StreamingPreferences.LANG_EL },
        { text: "Italiano", val: StreamingPreferences.LANG_IT },
        { text: "Język polski", val: StreamingPreferences.LANG_PL },
        { text: "Čeština", val: StreamingPreferences.LANG_CS },
        { text: "Български", val: StreamingPreferences.LANG_BG },
        { text: "தமிழ்", val: StreamingPreferences.LANG_TA }
    ]

    CdCard {
        title: qsTr("Appearance")
        CdChoiceRow {
            id: languageRow
            text: qsTr("Language")
            options: page.languages
            value: StreamingPreferences.language
            onPicked: function(v) {
                if (StreamingPreferences.language === v) return
                StreamingPreferences.language = v
                languageNote.text = StreamingPreferences.retranslate() ? "" : qsTr("Restart the app for this change to take effect.")
                if (languageNote.text === "") cdView.languageChanged()
            }
        }
        CdText { id: languageNote; color: T.warnText; text: "" }
        CdChoiceRow {
            visible: SystemProperties.hasDesktopEnvironment
            text: qsTr("Window when the app starts")
            options: [{ text: qsTr("Windowed"), val: StreamingPreferences.UI_WINDOWED },
                      { text: qsTr("Maximized"), val: StreamingPreferences.UI_MAXIMIZED },
                      { text: qsTr("Fullscreen"), val: StreamingPreferences.UI_FULLSCREEN }]
            value: StreamingPreferences.uiDisplayMode
            onPicked: function(v) { StreamingPreferences.uiDisplayMode = v }
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
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Optimize game settings for streaming")
            checked: StreamingPreferences.gameOptimizations
            onToggled: StreamingPreferences.gameOptimizations = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Quit the app on the CloudDeck when the stream ends")
            description: qsTr("Closes the app or game you were streaming. Unsaved progress is lost.")
            checked: StreamingPreferences.quitAppAfter
            onToggled: StreamingPreferences.quitAppAfter = checked
        }
    }

    CdCard {
        title: qsTr("While streaming")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Show performance stats")
            description: qsTr("Also from the menu, Ctrl+Alt+Shift+S or Select+L1+R1+X.")
            checked: StreamingPreferences.showPerformanceOverlay
            onToggled: StreamingPreferences.showPerformanceOverlay = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Warn about a poor connection")
            checked: StreamingPreferences.connectionWarnings
            onToggled: StreamingPreferences.connectionWarnings = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Warn about configuration problems")
            checked: StreamingPreferences.configurationWarnings
            onToggled: StreamingPreferences.configurationWarnings = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Keep the display awake")
            checked: StreamingPreferences.keepAwake
            onToggled: StreamingPreferences.keepAwake = checked
        }
        CdSwitch {
            Layout.fillWidth: true
            visible: SystemProperties.hasDiscordIntegration
            text: qsTr("Discord Rich Presence")
            description: qsTr("Shows the app you're streaming in your Discord status.")
            checked: StreamingPreferences.richPresence
            onToggled: StreamingPreferences.richPresence = checked
        }
    }

    // Advanced options stay folded away until asked for (less to read for most people).
    property bool showAdvanced: false
    CdButtonRow {
        Layout.leftMargin: 0
        CdButton {
            kind: "ghost"
            text: page.showAdvanced ? qsTr("Hide advanced settings") : qsTr("Show advanced settings...")
            onClicked: page.showAdvanced = !page.showAdvanced
        }
    }

    CdCard {
        visible: page.showAdvanced
        title: qsTr("Advanced")
        CdChoiceRow {
            text: qsTr("Video decoder")
            options: [{ text: qsTr("Automatic (recommended)"), val: StreamingPreferences.VDS_AUTO },
                      { text: qsTr("Force software decoding"), val: StreamingPreferences.VDS_FORCE_SOFTWARE },
                      { text: qsTr("Force hardware decoding"), val: StreamingPreferences.VDS_FORCE_HARDWARE }]
            value: StreamingPreferences.videoDecoderSelection
            onPicked: function(v) { StreamingPreferences.videoDecoderSelection = v }
        }
        CdChoiceRow {
            text: qsTr("Video codec")
            options: [{ text: qsTr("Automatic (recommended)"), val: StreamingPreferences.VCC_AUTO },
                      { text: "H.264", val: StreamingPreferences.VCC_FORCE_H264 },
                      { text: "HEVC (H.265)", val: StreamingPreferences.VCC_FORCE_HEVC },
                      { text: "AV1", val: StreamingPreferences.VCC_FORCE_AV1 }]
            value: StreamingPreferences.videoCodecConfig
            onPicked: function(v) { StreamingPreferences.videoCodecConfig = v }
        }
        CdChoiceRow {
            visible: SystemProperties.isDarwin
            text: qsTr("Renderer")
            options: [{ text: qsTr("Automatic (recommended)"), val: StreamingPreferences.RS_AUTO },
                      { text: "Vulkan", val: StreamingPreferences.RS_VULKAN },
                      { text: "Metal", val: StreamingPreferences.RS_METAL },
                      { text: "AVSampleBufferDisplayLayer", val: StreamingPreferences.RS_AVSBDL }]
            value: StreamingPreferences.rendererSelection
            onPicked: function(v) { StreamingPreferences.rendererSelection = v }
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("YUV 4:4:4 colour")
            description: qsTr("Sharper text on the desktop; not recommended for fast games.")
            checked: StreamingPreferences.enableYUV444
            onToggled: {
                StreamingPreferences.enableYUV444 = checked
                if (StreamingPreferences.autoAdjustBitrate)
                    StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height,
                                                                                              StreamingPreferences.fps, StreamingPreferences.enableYUV444)
            }
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Unlock the bitrate limit (experimental)")
            description: qsTr("Very high bitrates, only for wired connections.")
            checked: StreamingPreferences.unlockBitrate
            onToggled: {
                StreamingPreferences.unlockBitrate = checked
                StreamingPreferences.bitrateKbps = Math.min(StreamingPreferences.bitrateKbps, checked ? 500000 : 150000)
            }
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Find PCs on the local network")
            description: qsTr("Only matters for Manage PCs; your CloudDeck is found without it.")
            checked: StreamingPreferences.enableMdns
            onToggled: {
                StreamingPreferences.enableMdns = checked
                ComputerManager.stopPollingAsync()
                ComputerManager.startPolling()
            }
        }
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Detect blocked connections")
            checked: StreamingPreferences.detectNetworkBlocking
            onToggled: StreamingPreferences.detectNetworkBlocking = checked
        }
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
                text: qsTr("Manage PCs")
                kind: "ghost"
                onClicked: cdView.managePcs()
            }
        }
    }
}
