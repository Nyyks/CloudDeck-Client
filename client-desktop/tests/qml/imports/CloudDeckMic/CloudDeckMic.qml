pragma Singleton
import QtQuick 2.15
QtObject {
    property bool muted: false
    property bool enabled: true
    property var devices: ["Mic A", "Mic B"]
    property string device: ""
    property bool testing: false
    property real level: 0
    property string status: "Microphone ready"
    function refreshDevices() {}
    function startTest() { testing = true; level = 0.4 }
    function stopTest() { testing = false; level = 0 }
}
