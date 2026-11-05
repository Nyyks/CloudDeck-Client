pragma Singleton
import QtQuick 2.15
QtObject {
    property bool supported: true
    property bool serverInstalled: true
    property string status: ""
    property var devices: [{ description: "Xbox Controller", busid: "1-2", instanceId: "x", forward: true, attached: true, warning: "" },
                           { description: "USB Stick", busid: "1-3", instanceId: "y", forward: false, attached: false, warning: "" }]
    property int setCalls: 0
    function refresh() {}
    function setForward(id, on) { setCalls++ }
}
