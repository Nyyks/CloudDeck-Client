// Stream quality: resolution, frame rate and bitrate. Ported from Moonlight's SettingsView
// ("Basic Settings"): the same lists (native resolutions and refresh rates of the attached
// displays, decoder limit, custom values) and the same automatic bitrate.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import StreamingPreferences 1.0
import SystemProperties 1.0
import CloudDeckDisplay 1.0
import "CdTheme.js" as T
import "CdFocus.js" as F

ColumnLayout {
    id: q
    spacing: 2

    property var resolutions: []   // [{ text, w, h, custom }]
    property var rates: []         // [{ text, fps, custom }]
    readonly property bool fitting: CloudDeckDisplay.matchResolution || CloudDeckDisplay.multiMonitor
    readonly property int defaultBitrate: StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height,
                                                                                 StreamingPreferences.fps, StreamingPreferences.enableYUV444)

    function autoBitrate() {
        if (StreamingPreferences.autoAdjustBitrate) {
            StreamingPreferences.bitrateKbps = StreamingPreferences.getDefaultBitrate(StreamingPreferences.width, StreamingPreferences.height,
                                                                                      StreamingPreferences.fps, StreamingPreferences.enableYUV444)
            slider.value = StreamingPreferences.bitrateKbps
        }
    }
    function setResolution(w, h) {
        if (StreamingPreferences.width !== w || StreamingPreferences.height !== h) {
            StreamingPreferences.width = w
            StreamingPreferences.height = h
            autoBitrate()
        }
    }
    function setFps(f) {
        if (StreamingPreferences.fps !== f) {
            StreamingPreferences.fps = f
            autoBitrate()
        }
    }

    function buildResolutions() {
        var list = [{ text: qsTr("720p"), w: 1280, h: 720, custom: false },
                    { text: qsTr("1080p"), w: 1920, h: 1080, custom: false },
                    { text: qsTr("1440p"), w: 2560, h: 1440, custom: false },
                    { text: qsTr("4K"), w: 3840, h: 2160, custom: false }]
        function add(prefix, rect) {
            var at = 0
            for (var j = 0; j < list.length; j++) {
                if (rect.width === list[j].w && rect.height === list[j].h) return
                if (rect.width * rect.height > list[j].w * list[j].h) at = j + 1
            }
            list.splice(at, 0, { text: prefix + " (" + rect.width + "×" + rect.height + ")", w: rect.width, h: rect.height, custom: false })
        }
        SystemProperties.refreshDisplays()
        for (var d = 0; ; d++) {
            var screenRect = SystemProperties.getNativeResolution(d)
            if (screenRect.width === 0) break
            add(qsTr("Native"), screenRect)
            add(qsTr("Native (excluding notch)"), SystemProperties.getSafeAreaResolution(d))
        }
        var maxPixels = SystemProperties.maximumResolution.width * SystemProperties.maximumResolution.height
        if (maxPixels > 0) list = list.filter(function(r) { return r.w * r.h <= maxPixels })
        var found = false
        for (var i = 0; i < list.length; i++)
            if (list[i].w === StreamingPreferences.width && list[i].h === StreamingPreferences.height) found = true
        if (found) list.push({ text: qsTr("Custom..."), w: 0, h: 0, custom: true })
        else list.push({ text: qsTr("Custom (%1×%2)").arg(StreamingPreferences.width).arg(StreamingPreferences.height),
                         w: StreamingPreferences.width, h: StreamingPreferences.height, custom: true })
        resolutions = list
    }
    function resolutionIndex() {
        for (var i = 0; i < resolutions.length; i++)
            if (!resolutions[i].custom && resolutions[i].w === StreamingPreferences.width && resolutions[i].h === StreamingPreferences.height) return i
        return resolutions.length - 1
    }

    function buildRates() {
        var list = [{ text: qsTr("30 FPS"), fps: 30, custom: false }, { text: qsTr("60 FPS"), fps: 60, custom: false }]
        function add(rate) {
            var at = 0
            for (var j = 0; j < list.length; j++) {
                if (list[j].fps === rate) return
                if (rate > list[j].fps) at = j + 1
            }
            list.splice(at, 0, { text: qsTr("%1 FPS").arg(rate), fps: rate, custom: false })
        }
        for (var d = 0; ; d++) {
            var r = SystemProperties.getRefreshRate(d)
            if (r === 0) break
            add(r)
        }
        var found = false
        for (var i = 0; i < list.length; i++) if (list[i].fps === StreamingPreferences.fps) found = true
        if (found) list.push({ text: qsTr("Custom..."), fps: 0, custom: true })
        else list.push({ text: qsTr("Custom (%1 FPS)").arg(StreamingPreferences.fps), fps: StreamingPreferences.fps, custom: true })
        rates = list
    }
    function rateIndex() {
        for (var i = 0; i < rates.length; i++)
            if (!rates[i].custom && rates[i].fps === StreamingPreferences.fps) return i
        return rates.length - 1
    }

    Component.onCompleted: { buildResolutions(); buildRates() }

    CdComboRow {
        id: resRow
        text: qsTr("Resolution")
        description: q.fitting ? qsTr("Not used while the CloudDeck fits the window.") : ""
        enabled: !q.fitting
        model: q.resolutions.map(function(r) { return r.text })
        currentIndex: q.resolutionIndex()
        onActivated: function(i) {
            if (q.resolutions[i].custom) customResDialog.open()
            else q.setResolution(q.resolutions[i].w, q.resolutions[i].h)
        }
    }
    CdComboRow {
        id: fpsRow
        text: qsTr("Frame rate")
        model: q.rates.map(function(r) { return r.text })
        currentIndex: q.rateIndex()
        onActivated: function(i) {
            if (q.rates[i].custom) customFpsDialog.open()
            else q.setFps(q.rates[i].fps)
        }
    }

    // ---- bitrate
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        Layout.topMargin: 8
        Text { Layout.fillWidth: true; text: qsTr("Video bitrate"); color: T.text; font.pixelSize: 15 }
        Text {
            text: qsTr("%1 Mbit/s").arg(slider.value / 1000.0)
            color: T.accentText
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        focusPolicy: Qt.StrongFocus
        from: 500
        to: StreamingPreferences.unlockBitrate ? 500000 : 150000
        stepSize: 500
        snapMode: Slider.SnapOnRelease
        value: StreamingPreferences.bitrateKbps
        onMoved: {
            StreamingPreferences.autoAdjustBitrate = false
            StreamingPreferences.bitrateKbps = value
        }
        onActiveFocusChanged: if (activeFocus) F.ensureVisible(slider)
        CdFocusRing { shown: slider.visualFocus; ringRadius: 8 }
    }
    CdText { text: qsTr("Lower it on slow connections; raise it for a sharper picture.") }
    CdButtonRow {
        visible: StreamingPreferences.bitrateKbps !== q.defaultBitrate
        CdButton {
            kind: "ghost"
            text: qsTr("Use the default (%1 Mbit/s)").arg(q.defaultBitrate / 1000.0)
            onClicked: {
                StreamingPreferences.bitrateKbps = q.defaultBitrate
                StreamingPreferences.autoAdjustBitrate = true
                slider.value = q.defaultBitrate
            }
        }
    }

    // ---- custom values (as in Moonlight)
    NavigableDialog {
        id: customResDialog
        parent: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        function valid() {
            return widthField.acceptableInput && heightField.acceptableInput
        }
        onOpened: {
            widthField.text = "" + StreamingPreferences.width
            heightField.text = "" + StreamingPreferences.height
            widthField.forceActiveFocus()
        }
        onRejected: resRow.currentIndex = q.resolutionIndex()
        onAccepted: {
            if (!valid()) { resRow.currentIndex = q.resolutionIndex(); return }
            q.setResolution(parseInt(widthField.text), parseInt(heightField.text))
            q.buildResolutions()
            resRow.currentIndex = q.resolutionIndex()
        }
        ColumnLayout {
            Label {
                text: qsTr("Resolutions your PC or the CloudDeck don't support may cause streaming errors.")
                wrapMode: Label.WordWrap
                Layout.maximumWidth: 320
            }
            Label { text: qsTr("Enter a custom resolution:"); font.bold: true }
            RowLayout {
                TextField {
                    id: widthField
                    maximumLength: 5
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 256; top: 8192 }
                    Keys.onReturnPressed: customResDialog.accept()
                    Keys.onEnterPressed: customResDialog.accept()
                }
                Label { text: "×"; font.bold: true }
                TextField {
                    id: heightField
                    maximumLength: 5
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 256; top: 8192 }
                    Keys.onReturnPressed: customResDialog.accept()
                    Keys.onEnterPressed: customResDialog.accept()
                }
            }
        }
    }
    NavigableDialog {
        id: customFpsDialog
        parent: Overlay.overlay
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: { fpsField.text = "" + StreamingPreferences.fps; fpsField.forceActiveFocus() }
        onRejected: fpsRow.currentIndex = q.rateIndex()
        onAccepted: {
            if (!fpsField.acceptableInput) { fpsRow.currentIndex = q.rateIndex(); return }
            q.setFps(parseInt(fpsField.text))
            q.buildRates()
            fpsRow.currentIndex = q.rateIndex()
        }
        ColumnLayout {
            Label { text: qsTr("Enter a custom frame rate:"); font.bold: true }
            TextField {
                id: fpsField
                maximumLength: 4
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 10; top: 9999 }
                Keys.onReturnPressed: customFpsDialog.accept()
                Keys.onEnterPressed: customFpsDialog.accept()
            }
        }
    }
}
