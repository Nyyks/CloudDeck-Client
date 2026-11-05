// CloudDeck-Client: full on-screen keyboard for touch devices and handhelds
// (Phase 6, docs/on-screen-keyboard.md). Sends real key presses (Windows virtual-key
// codes), so Esc, F-keys, arrows and shortcuts work. Modifiers latch for one key
// (tap) or lock (double tap). Same layout as the Android client.
import QtQuick 2.15

Rectangle {
    id: root
    property QtObject overlay
    color: "#1d222b"

    readonly property int modShift: 0x01
    readonly property int modCtrl: 0x02
    readonly property int modAlt: 0x04
    readonly property int modMeta: 0x08
    property int latched: 0
    property int locked: 0
    property int lastMod: 0
    property double lastModTime: 0

    function mods() { return latched | locked }
    function modVk(m) { return m === modShift ? 0xA0 : m === modCtrl ? 0xA2 : m === modAlt ? 0xA4 : 0x5B }
    function sendMod(m, down) { overlay.sendKey(modVk(m), down, down ? (mods() | m) : (mods() & ~m)) }

    function tapMod(m) {
        var now = Date.now()
        if (locked & m) { locked &= ~m; latched &= ~m; sendMod(m, false) }
        else if (m === lastMod && now - lastModTime < 400) { locked |= m; latched &= ~m }
        else if (latched & m) { latched &= ~m; sendMod(m, false) }
        else { latched |= m; sendMod(m, true) }
        lastMod = m
        lastModTime = now
    }
    function keyUp(vk) {
        overlay.sendKey(vk, false, mods())
        var used = latched & ~locked
        var all = [modShift, modCtrl, modAlt, modMeta]
        for (var i = 0; i < all.length; i++) if (used & all[i]) sendMod(all[i], false)
        latched = 0
    }
    function releaseAll() {
        var all = [modShift, modCtrl, modAlt, modMeta]
        for (var i = 0; i < all.length; i++) if (mods() & all[i]) sendMod(all[i], false)
        latched = 0
        locked = 0
    }

    // [label, vk, width, modifier]; vk -1 closes the keyboard
    readonly property var rows: [
        [["Esc", 27, 1.2], ["F1", 112], ["F2", 113], ["F3", 114], ["F4", 115], ["F5", 116], ["F6", 117],
         ["F7", 118], ["F8", 119], ["F9", 120], ["F10", 121], ["F11", 122], ["F12", 123], ["Del", 46, 1.2]],
        [["`", 0xC0], ["1", 49], ["2", 50], ["3", 51], ["4", 52], ["5", 53], ["6", 54], ["7", 55], ["8", 56],
         ["9", 57], ["0", 48], ["-", 0xBD], ["=", 0xBB], ["⌫", 8, 1.6]],
        [["Tab", 9, 1.5], ["Q", 81], ["W", 87], ["E", 69], ["R", 82], ["T", 84], ["Y", 89], ["U", 85],
         ["I", 73], ["O", 79], ["P", 80], ["[", 0xDB], ["]", 0xDD], ["\\", 0xDC, 1.1]],
        [["Caps", 20, 1.75], ["A", 65], ["S", 83], ["D", 68], ["F", 70], ["G", 71], ["H", 72], ["J", 74],
         ["K", 75], ["L", 76], [";", 0xBA], ["'", 0xDE], ["Enter", 13, 1.85]],
        [["Shift", 0xA0, 2.2, 0x01], ["Z", 90], ["X", 88], ["C", 67], ["V", 86], ["B", 66], ["N", 78],
         ["M", 77], [",", 0xBC], [".", 0xBE], ["/", 0xBF], ["↑", 38], ["PgUp", 33, 1.2]],
        [["Ctrl", 0xA2, 1.4, 0x02], ["Win", 0x5B, 1.2, 0x08], ["Alt", 0xA4, 1.2, 0x04], ["", 32, 5],
         ["←", 37], ["↓", 40], ["→", 39], ["PgDn", 34, 1.2], ["✕", -1, 1.2]],
    ]

    Column {
        anchors.fill: parent
        anchors.margins: 4
        Repeater {
            model: root.rows
            delegate: Row {
                id: rowItem
                property var keys: modelData
                property real unit: width / keys.reduce(function(s, k) { return s + (k[2] || 1) }, 0)
                width: parent.width
                height: parent.height / root.rows.length
                Repeater {
                    model: rowItem.keys
                    delegate: Rectangle {
                        id: keyRect
                        property int vk: modelData[1]
                        property int mod: modelData[3] || 0
                        width: rowItem.unit * (modelData[2] || 1)
                        height: rowItem.height
                        radius: 6
                        border.color: "#1d222b"
                        border.width: 2
                        color: area.pressed ? "#4dabf7"
                               : (mod && (root.locked & mod)) ? "#3b5bdb"
                               : (mod && (root.latched & mod)) ? "#2f4a9e" : "#2b3240"
                        Text {
                            anchors.centerIn: parent
                            text: modelData[0]
                            color: "white"
                            font.pixelSize: Math.min(keyRect.height * 0.4, modelData[0].length > 2 ? 13 : 18)
                        }
                        Timer {
                            id: repeat
                            interval: 50
                            repeat: true
                            property bool started: false
                            onTriggered: root.overlay.sendKey(keyRect.vk, true, root.mods())
                        }
                        Timer { id: repeatDelay; interval: 450; onTriggered: repeat.start() }
                        MouseArea {
                            id: area
                            anchors.fill: parent
                            onPressed: {
                                if (keyRect.vk < 0) return
                                if (keyRect.mod) { root.tapMod(keyRect.mod); return }
                                root.overlay.sendKey(keyRect.vk, true, root.mods())
                                repeatDelay.start()
                            }
                            onReleased: {
                                repeatDelay.stop()
                                repeat.stop()
                                if (keyRect.vk < 0) { root.releaseAll(); root.overlay.toggleKeyboard(); return }
                                if (keyRect.mod) return
                                root.keyUp(keyRect.vk)
                            }
                            onCanceled: { repeatDelay.stop(); repeat.stop(); if (!keyRect.mod && keyRect.vk >= 0) root.keyUp(keyRect.vk) }
                        }
                    }
                }
            }
        }
    }
}
