// CloudDeck-Client: home screen. Sign in, then the home (CdHomeConsole.qml: sidebar pages, or a
// header with page chips on phones), built from the settings pages (CdPage*.qml). Works with touch,
// mouse, keyboard, gamepads and TV remotes. See docs/adr/0013-home-layouts.md.
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import QtQuick.Controls.Material 2.2

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import CloudDeck 1.0
import CloudDeckMic 1.0
import CloudDeckUsb 1.0
import CloudDeckDisplay 1.0
import CloudDeckShortcuts 1.0
import "CdTheme.js" as T

FocusScope {
    id: cdView
    objectName: qsTr("CloudDeck")
    Material.accent: T.accent

    property var activeLauncher: null
    property bool streamLaunched: false
    property string launchText: ""

    // ---- helpers for the layouts and pages
    function micMuteKey() {
        var a = CloudDeckShortcuts.actions
        for (var i = 0; i < a.length; i++) if (a[i].id === "micMute") return a[i].key
        return "U"
    }
    function connectLabel() {
        if (CloudDeck.machineStatus === "running") return qsTr("Connect")
        if (CloudDeck.machineStatus === "starting") return qsTr("Starting...")
        if (CloudDeck.machineStatus === "stopping") return qsTr("Stopping...")
        return qsTr("Start and connect")
    }
    function canConnect() { return !CloudDeck.busy && CloudDeck.machineStatus !== "stopping" }
    function headline() {
        switch (CloudDeck.machineStatus) {
        case "running": return qsTr("Ready to stream")
        case "off": return qsTr("Your CloudDeck is off")
        case "starting": return qsTr("Your CloudDeck is starting")
        case "stopping": return qsTr("Your CloudDeck is stopping")
        default: return qsTr("Checking your CloudDeck...")
        }
    }
    function statusLine() {
        switch (CloudDeck.machineStatus) {
        case "running": return qsTr("Running") + (CloudDeck.machineRegion ? "  ·  " + CloudDeck.machineRegion : "")
        case "off": return qsTr("Off  ·  starts in about a minute")
        case "starting": return qsTr("Starting...")
        case "stopping": return qsTr("Stopping...")
        case "": return qsTr("Checking...")
        default: return CloudDeck.machineStatus
        }
    }
    function fitsWindow() { return CloudDeckDisplay.matchResolution || CloudDeckDisplay.multiMonitor }
    function toggleFit() { if (!CloudDeckDisplay.multiMonitor) CloudDeckDisplay.matchResolution = !CloudDeckDisplay.matchResolution }
    function streamSummary() {
        var res = CloudDeckDisplay.multiMonitor ? qsTr("Several monitors")
                : CloudDeckDisplay.matchResolution ? qsTr("Fits the window")
                : StreamingPreferences.width + " × " + StreamingPreferences.height
        return res + "  ·  " + qsTr("%1 FPS").arg(StreamingPreferences.fps)
                   + "  ·  " + qsTr("%1 Mbit/s").arg(Math.round(StreamingPreferences.bitrateKbps / 100) / 10)
    }
    function usbForwardCount() {
        var n = 0, d = CloudDeckUsb.devices
        for (var i = 0; i < d.length; i++) if (d[i].forward) n++
        return n
    }
    function usbChipText() {
        var n = usbForwardCount()
        return n === 0 ? qsTr("No USB devices") : n === 1 ? qsTr("1 USB device") : qsTr("%1 USB devices").arg(n)
    }
    function confirmStop() { stopConfirm.open() }
    function confirmRestart() { restartConfirm.open() }
    function openControllerProfiles() { controllerProfiles.open() }
    function openMoonlightSettings() { navigateTo("qrc:/gui/SettingsView.qml", SettingsView) }
    function managePcs() { navigateTo("qrc:/gui/PcView.qml", PcView) }

    // Focus moved by a key keeps the ring visible; after a click or tap it stays hidden.
    function keyReason() {
        var it = Window.activeFocusItem
        return it && it.visualFocus ? Qt.TabFocusReason : Qt.OtherFocusReason
    }
    function moveFocus(forward) {
        var it = Window.activeFocusItem
        if (!it) return
        var next = it.nextItemInFocusChain(forward)
        if (next) next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
    }
    function openSettings() {
        if (homeLoader.item) homeLoader.item.openSettings("", Qt.TabFocusReason)
    }
    // Moonlight's toolbar gear (other pages) lands here: there is one settings menu.
    function openSettingsFromToolbar() {
        if (homeLoader.item) homeLoader.item.openSettings("", Qt.OtherFocusReason)
    }
    // After a language change, rebuild the home so every list picks up the new texts.
    function languageChanged() {
        if (typeof window !== "undefined") window.clearOnBack = true
        homeLoader.active = false
        homeLoader.active = Qt.binding(function() { return CloudDeck.signedIn })
        Qt.callLater(function() { if (homeLoader.item) homeLoader.item.openSettings("style", keyReason()) })
    }
    function focusDefault(reason) {
        var view = CloudDeck.signedIn ? homeLoader.item : signInLoader.item
        if (view) view.focusDefault(reason)
    }

    // ---- stream hand-off (mirrors CliStartStreamSegue.qml, without quitting the app)
    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "quitAfter": false
        })
        streamLaunched = true
        stackView.push(segue)
    }
    function onLaunchFailed(message) {
        launchText = ""
        errorDialog.text = message
        errorDialog.open()
        CloudDeck.streamEnded()
    }
    function onAppQuitRequired(appName) {
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    Connections {
        target: CloudDeck
        function onStreamReady(launcher, appName) {
            activeLauncher = launcher
            launchText = qsTr("Launching %1...").arg(appName)
            launcher.searchingComputer.connect(function() { launchText = qsTr("Connecting to the streaming host...") })
            launcher.searchingApp.connect(function() { launchText = qsTr("Loading app list...") })
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
        }
        function onSignedInChanged() { Qt.callLater(cdView.focusDefault, Qt.OtherFocusReason) }
    }

    StackView.onActivated: {
        // D-pad up/down step through the controls in Tab order, like SettingsView.
        SdlGamepadKeyNavigation.setUiNavMode(true)
        if (streamLaunched) {
            // We are back from a stream we launched.
            streamLaunched = false
            launchText = ""
            CloudDeck.streamEnded()
        }
        CloudDeck.refreshStatus()
        // Handhelds, TVs and anyone holding a controller see the focus ring straight away.
        var keyUi = !SystemProperties.hasDesktopEnvironment || SdlGamepadKeyNavigation.getConnectedGamepads() > 0
        Qt.callLater(cdView.focusDefault, keyUi ? Qt.TabFocusReason : Qt.OtherFocusReason)
    }
    StackView.onDeactivating: {
        SdlGamepadKeyNavigation.setUiNavMode(false)
        // The settings pages edit Moonlight's preferences directly; persist them (as SettingsView did).
        StreamingPreferences.save()
    }
    Component.onDestruction: StreamingPreferences.save()

    // Arrow keys (keyboards, TV remotes) move like the D-pad; Menu/Start opens the settings.
    Keys.onUpPressed: moveFocus(false)
    Keys.onDownPressed: moveFocus(true)
    Keys.onMenuPressed: openSettings()
    Keys.onHangupPressed: openSettings()

    Rectangle { anchors.fill: parent; color: T.bg }

    // ---------------- restoring
    Row {
        visible: CloudDeck.restoring
        anchors.centerIn: parent
        spacing: 12
        BusyIndicator { running: parent.visible }
        Text {
            text: qsTr("Signing in...")
            color: T.text
            font.pixelSize: 18
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ---------------- sign-in
    Loader {
        id: signInLoader
        anchors.fill: parent
        focus: !CloudDeck.signedIn
        active: !CloudDeck.signedIn && !CloudDeck.restoring
        source: "CdSignIn.qml"
    }

    // ---------------- home (layout chosen in the settings)
    Loader {
        id: homeLoader
        objectName: "homeLoader"
        anchors.fill: parent
        focus: CloudDeck.signedIn
        active: CloudDeck.signedIn
        source: "CdHomeConsole.qml"
    }

    ControllerProfiles { id: controllerProfiles; parent: Overlay.overlay }

    NavigableMessageDialog {
        id: stopConfirm
        text: qsTr("Stop your CloudDeck? Unsaved work on it will be lost.")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: CloudDeck.stopMachine()
    }
    NavigableMessageDialog {
        id: restartConfirm
        text: qsTr("Restart your CloudDeck? Unsaved work on it will be lost.")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: CloudDeck.restartMachine()
    }
    ErrorMessageDialog {
        id: errorDialog
    }
    NavigableMessageDialog {
        id: quitAppDialog
        property string appName: ""
        text: qsTr("%1 is already running on your CloudDeck. Quit it and start %2? Unsaved progress in it will be lost.")
              .arg(appName).arg(CloudDeck.appName)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName, "quitRunningAppFn": function() { activeLauncher.quitRunningApp() }}
            stackView.push(component.createObject(stackView, params))
        }
        onRejected: {
            launchText = ""
            CloudDeck.streamEnded()
        }
    }
}
