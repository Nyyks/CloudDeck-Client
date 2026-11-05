import QtQuick 2.15
import QtQuick.Window 2.15
import QtTest 1.15
import QtQuick.Controls 2.15

import CloudDeck 1.0
import SdlGamepadKeyNavigation 1.0

// Home screen (Phase 7): both layouts load, switch, and can be driven with keys only
// (keyboard, TV remote, gamepad through SdlGamepadKeyNavigation's Tab/Space/Escape).
// With CDC_SHOTS set, run.sh keeps the screenshots taken here.
Item {
    id: top
    width: 1280
    height: 800

    // main.qml context the view expects
    property var stackView: null
    property int navigations: 0
    property bool clearOnBack: false
    function navigateTo(url, type) { navigations++ }

    property var view: null

    TestCase {
        name: "Home"
        when: windowShown

        function shot(name) { wait(350); grabImage(top).save("shots/" + name + ".png") }
        function focused() { return top.Window.activeFocusItem }
        function focusedText() { var f = focused(); return f && f.text !== undefined ? f.text : "" }
        function home() { return findChild(view, "homeLoader").item }

        function make(w, h) {
            top.width = w; top.height = h
            var c = Qt.createComponent("CloudDeckView.qml")
            compare(c.status, Component.Ready, c.errorString())
            view = c.createObject(top, { width: w, height: h })
            verify(view !== null)
            tryVerify(function() { return home() !== null && home() !== undefined })
            view.forceActiveFocus()
        }
        function cleanup() {
            if (view) { view.destroy(); view = null }
            wait(20)
            CloudDeck.signedIn = true
            CloudDeck.machineStatus = "running"
        }

        function test_console_pages() {
            make(1280, 800)
            verify(home().pages !== undefined, "console layout")
            shot("console-home")
            var pages = ["display", "audio", "controllers", "input", "usb", "network", "general"]
            for (var i = 0; i < pages.length; i++) {
                home().page = pages[i]
                shot("console-" + pages[i])
            }
            CloudDeck.machineStatus = "off"
            home().page = "home"
            shot("console-home-off")
        }

        function test_console_phone() {
            make(390, 844)
            verify(home().narrow)
            shot("console-phone")
            home().page = "controllers"
            shot("console-phone-controllers")
        }

        function test_console_keys() {
            make(1280, 800)
            home().focusDefault(Qt.TabFocusReason)
            compare(focusedText(), "Connect")
            verify(focused().visualFocus, "focus ring shown")
            shot("console-focus")
            keyClick(Qt.Key_Space)                       // gamepad A
            compare(CloudDeck.connects, 1)
            keyClick(Qt.Key_Return)                      // keyboard / TV remote OK
            compare(CloudDeck.connects, 2)
            keyClick(Qt.Key_Tab)                         // D-pad down
            compare(focusedText(), "Microphone on")
            keyClick(Qt.Key_Down)                        // arrow key, same as Tab
            compare(focusedText(), "Clipboard shared")
            keyClick(Qt.Key_Left)                        // back to the sidebar
            compare(focusedText(), "Home")
            keyClick(Qt.Key_Tab)
            compare(focusedText(), "Display")
            keyClick(Qt.Key_Space)
            compare(home().page, "display")
            keyClick(Qt.Key_Right)                       // into the page
            verify(focused() !== null && focusedText() !== "Display", "focus moved into the page")
            shot("console-display-focus")
            keyClick(Qt.Key_Escape)                      // B: back to the sidebar
            compare(focusedText(), "Display")
            keyClick(Qt.Key_Escape)                      // B again: Home
            compare(home().page, "home")
        }

        function test_sign_in() {
            CloudDeck.signedIn = false
            var c = Qt.createComponent("CloudDeckView.qml")
            view = c.createObject(top, { width: 1280, height: 800 })
            wait(50)
            shot("sign-in")
        }
    }
}
