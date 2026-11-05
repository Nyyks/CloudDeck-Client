pragma Singleton
import QtQuick 2.15
QtObject {
    property int width: 1920
    property int height: 1080
    property int fps: 60
    property int bitrateKbps: 20000
    property bool swapFaceButtons: false
    function save() {}
}
