pragma Singleton
import QtQuick 2.15
QtObject {
    property bool multiMonitor: false
    property var monitors: []
    property bool matchResolution: true
    property var screens: [{ name: "DP-1", label: "Monitor 1 (2560 × 1440)" }, { name: "HDMI-1", label: "Monitor 2 (1920 × 1080)" }]
    property string summary: ""
    property bool summaryIsError: false
    function setMonitorChosen(n, on) {}
    function refresh() {}
}
