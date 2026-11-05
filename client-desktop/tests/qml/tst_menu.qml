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
        property string noticeText: "Connection lost. Reconnecting (1 of 5)..."
        property int closes: 0
        property int shutdowns: 0
        function closeMenu() { closes++ }
        function toggleMenu() {}
        function reconnect() {}
        function disconnectStream() {}
        function shutDown() { shutdowns++ }
        function uploadFiles(urls) {}
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
        }
    }
}
