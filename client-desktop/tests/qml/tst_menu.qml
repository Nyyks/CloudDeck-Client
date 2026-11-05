import QtQuick 2.15
import QtTest 1.15
import QtQuick.Controls 2.15

Item {
    width: 400; height: 660
    QtObject {
        id: stub
        signal changed()
        property bool menuOpen: true
        property string streamInfo: "1920 × 1080 · 60 FPS"
        property bool canToggleFullscreen: true
        property bool fullscreen: false
        property bool canFitToWindow: true
        property bool fitToWindow: false
        property bool statsVisible: false
        property bool desktopMouse: false
        property bool audioMuted: false
        property bool micEnabled: true
        property bool showButton: true
        property bool canShutDown: true
        property string menuKey: "O"
        property bool nintendoLayout: false
        property bool keyboardVisible: false
        property bool controllerVisible: false
        property bool controllerPlaystation: false
        function toggleController() {}
        property int keys: 0
        function toggleKeyboard() {}
        function sendKey(vk, down, mods) { keys++ }
        property string noticeText: "Connection lost. Reconnecting (1 of 5)..."
        property int closes: 0
        property int shutdowns: 0
        function closeMenu() { closes++ }
        function toggleMenu() {}
        function reconnect() {}
        function disconnectStream() {}
        function shutDown() { shutdowns++ }
    }
    TestCase {
        name: "StreamMenu"
        when: windowShown
        function test_loads() {
            var c = Qt.createComponent("StreamMenu.qml")
            compare(c.status, Component.Ready, c.errorString())
            var m = c.createObject(parent, { overlay: stub, width: 400, height: 660 })
            verify(m !== null)
            wait(50)
            m.destroy()
            var b = Qt.createComponent("StreamMenuButton.qml")
            compare(b.status, Component.Ready, b.errorString())
            var bo = b.createObject(parent, { overlay: stub, width: 40, height: 40 })
            verify(bo !== null)
            bo.destroy()
            var n = Qt.createComponent("StreamNotice.qml")
            compare(n.status, Component.Ready, n.errorString())
            var no = n.createObject(parent, { overlay: stub, width: 380, height: 40 })
            verify(no !== null)
            no.destroy()
            var kc = Qt.createComponent("StreamKeyboard.qml")
            compare(kc.status, Component.Ready, kc.errorString())
            var kb = kc.createObject(parent, { overlay: stub, width: 800, height: 300 })
            verify(kb !== null)
            kb.tapMod(kb.modCtrl)
            compare(kb.latched, kb.modCtrl)
            kb.keyUp(67)
            compare(kb.latched, 0)
            verify(stub.keys >= 3) // ctrl down, c up, ctrl up
            kb.destroy()
            var gc = Qt.createComponent("StreamGamepad.qml")
            compare(gc.status, Component.Ready, gc.errorString())
            var oscStub = Qt.createQmlObject('import QtQuick 2.15; QtObject { property bool playstation: true; property int last: 0; function button(f, d) { last = f } function stick(w, x, y) {} function trigger(w, v) {} }', parent)
            var gp = gc.createObject(parent, { osc: oscStub, unit: 4,
                shapes: [{type: "button", id: "a", flag: 4096, round: true, x: 0, y: 0, w: 9, h: 9},
                         {type: "stick", which: 0, x: 10, y: 0, w: 26, h: 26}, {type: "dpad", x: 0, y: 30, w: 26, h: 26},
                         {type: "trigger", which: 0, x: 0, y: 60, w: 22, h: 9}] })
            verify(gp !== null)
            wait(30)
            gp.destroy()
            var cp = Qt.createComponent("ControllerProfiles.qml")
            compare(cp.status, Component.Ready, cp.errorString())
            var dlgObj = cp.createObject(parent, {})
            verify(dlgObj !== null)
            dlgObj.open()
            wait(50)
            tryVerify(function() { return dlgObj.profileData.name === "Racing" }, 3000, "profile loaded when opened")
            dlgObj.save()
            compare(dlgObj.error, "")
            dlgObj.destroy()
        }
    }
}
