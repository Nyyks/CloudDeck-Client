pragma Singleton
import QtQuick 2.15
QtObject {
    property bool hasDesktopEnvironment: true
    property bool hasBrowser: false
    property bool usesMaterial3Theme: true
    property string versionString: "1.0.1"
    property bool supportsHdr: false
    property bool rendererAlwaysFullScreen: false
    property bool hasDiscordIntegration: false
    property bool isDarwin: false
    property size maximumResolution: Qt.size(0, 0)
    function refreshDisplays() {}
    function getNativeResolution(i) { return i === 0 ? Qt.size(2560, 1600) : Qt.size(0, 0) }
    function getSafeAreaResolution(i) { return i === 0 ? Qt.size(2560, 1600) : Qt.size(0, 0) }
    function getRefreshRate(i) { return i === 0 ? 120 : 0 }
}
