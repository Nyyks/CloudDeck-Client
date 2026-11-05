pragma Singleton
import QtQuick 2.15
QtObject {
    property bool uiNavMode: false
    property int gamepads: 0
    function enable() {}
    function disable() {}
    function notifyWindowFocus(f) {}
    function setUiNavMode(on) { uiNavMode = on }
    function getConnectedGamepads() { return gamepads }
}
