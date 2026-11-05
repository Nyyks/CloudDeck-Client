pragma Singleton
import QtQuick 2.15
// Stub of CloudSession (app/clouddeck/cloudsession.h) for the home-screen tests.
QtObject {
    signal streamReady(var launcher, string appName)
    property bool signedIn: true
    property bool restoring: false
    property bool mfaRequired: false
    property string email: "player@example.com"
    property string keychainBackend: "the system keychain"
    property string machineStatus: "running"
    property string machineRegion: "Frankfurt"
    property string hostAddress: "203.0.113.10"
    property bool busy: false
    property bool connecting: false
    property string progressText: ""
    property string errorText: ""
    property string appName: "CloudDeck"
    property bool autoStopOnSessionEnd: false
    property int autoStopIdleMinutes: 30
    property bool clipboardSync: true
    property bool autoReconnect: true
    property string controllerStyle: "xbox"
    property bool onScreenController: false
    property bool gamepadMouseAtStart: false
    property bool netTestRunning: false
    property string netTestStatus: "23 ms · 187 Mbit/s"
    property string netTestRecommendation: "Recommended: 1080p, 60 FPS, 30 Mbit/s"
    property bool netTestHasRecommendation: true

    property int connects: 0
    property int netTests: 0
    function signIn(e, p) {}
    function submitMfaCode(c) {}
    function signOut() {}
    function refreshStatus() {}
    function startMachine() {}
    function stopMachine() {}
    function restartMachine() {}
    function startAndConnect(cm) { connects++ }
    function cancelConnect() {}
    function streamEnded() {}
    function clearError() { errorText = "" }
    function runNetworkTest(cm) { netTests++ }
    function applyRecommendation() {}
    function applyLowBandwidth() {}
}
