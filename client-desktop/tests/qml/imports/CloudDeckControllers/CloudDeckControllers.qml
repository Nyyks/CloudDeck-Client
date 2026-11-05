pragma Singleton
import QtQuick 2.15
QtObject {
    property var profiles: ["Racing"]
    property string defaultProfile: ""
    property var buttonNames: ["a", "b", "x", "y"]
    property var controllers: [{ name: "Xbox Wireless Controller", profile: "" }]
    property int saves: 0
    function buttonLabel(id) { return id.toUpperCase() }
    function profile(name) {
        return { name: name, buttons: { a: "b", b: "a", x: "x", y: "none" }, leftDeadzone: 0.1, leftSensitivity: 1.0, leftInvertY: false,
                 rightDeadzone: 0, rightSensitivity: 1.5, rightInvertY: true, triggerDeadzone: 0.05, swapSticks: false }
    }
    function saveProfile(orig, data) { saves++; return "" }
    function deleteProfile(name) {}
    function newProfileName() { return "My profile" }
    function setControllerProfile(c, p) {}
    function refreshControllers() {}
}
