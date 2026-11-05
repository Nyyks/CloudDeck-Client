pragma Singleton
import QtQuick 2.15
QtObject {
    property bool supported: true
    property string status: ""
    property var devices: [{ description: "Xbox Controller", busid: "1-2", instanceId: "x", forward: true, attached: true },
                           { description: "USB Stick", busid: "1-3", instanceId: "y", forward: false, attached: false }]
    property int setCalls: 0
    function refresh() {}
    function setForward(id, on) { setCalls++ }
}
