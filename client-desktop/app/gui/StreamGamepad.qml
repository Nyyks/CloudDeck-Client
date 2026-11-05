// CloudDeck-Client: one part of the desktop on-screen controller
// (docs/on-screen-controller.md). The window is masked to these shapes; the
// shapes come from OnScreenController (C++) in "units" (1 % of the stream height).
import QtQuick 2.15

Item {
    id: root
    property QtObject osc
    property var shapes: []
    property real partWidth: 30
    property real partHeight: 58
    property real unit: 4 // logical px per unit, set by C++

    readonly property bool ps: osc ? osc.playstation : false
    readonly property var labelsXbox: ({a: "A", b: "B", x: "X", y: "Y", lb: "LB", rb: "RB", back: "View", start: "Menu", guide: "Guide"})
    readonly property var labelsPs: ({a: "✕", b: "○", x: "□", y: "△", lb: "L1", rb: "R1", back: "Share", start: "Options", guide: "PS"})
    readonly property var colorsXbox: ({a: "#5dc21e", b: "#e8312f", x: "#2d7fe0", y: "#f4c20d"})
    readonly property var colorsPs: ({a: "#7eb6ff", b: "#ff6b6b", x: "#f38bd6", y: "#40e0b0"})

    Repeater {
        model: root.shapes
        delegate: Item {
            id: shapeItem
            x: modelData.x * root.unit
            y: modelData.y * root.unit
            width: modelData.w * root.unit
            height: modelData.h * root.unit

            // ---- buttons
            Rectangle {
                visible: modelData.type === "button"
                anchors.fill: parent
                radius: modelData.round ? width / 2 : root.unit * 1.5
                color: tap.pressed ? "#4dabf7" : "#2b3240"
                border.color: "#55ffffff"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: modelData.id ? (root.ps ? root.labelsPs : root.labelsXbox)[modelData.id] || "" : ""
                    color: (root.ps ? root.colorsPs : root.colorsXbox)[modelData.id] || "white"
                    font.pixelSize: Math.max(8, Math.min(parent.height * 0.5, root.unit * 3.6))
                    font.bold: true
                }
                TapHandler {
                    id: tap
                    enabled: modelData.type === "button"
                    gesturePolicy: TapHandler.WithinBounds
                    onPressedChanged: root.osc.button(modelData.flag, pressed)
                }
            }

            // ---- triggers (press = fully pulled)
            Rectangle {
                visible: modelData.type === "trigger"
                anchors.fill: parent
                radius: root.unit * 1.5
                color: trig.pressed ? "#4dabf7" : "#2b3240"
                border.color: "#55ffffff"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: root.ps ? (modelData.which === 0 ? "L2" : "R2") : (modelData.which === 0 ? "LT" : "RT")
                    color: "white"
                    font.pixelSize: Math.max(8, root.unit * 3.4)
                    font.bold: true
                }
                TapHandler {
                    id: trig
                    enabled: modelData.type === "trigger"
                    gesturePolicy: TapHandler.WithinBounds
                    onPressedChanged: root.osc.trigger(modelData.which, pressed ? 1 : 0)
                }
            }

            // ---- sticks
            Rectangle {
                visible: modelData.type === "stick"
                anchors.fill: parent
                radius: width / 2
                color: "#2b3240"
                border.color: "#55ffffff"
                border.width: 1
                Rectangle {
                    id: knob
                    width: parent.width * 0.42
                    height: width
                    radius: width / 2
                    color: stickPoint.active ? "#4dabf7" : "#46506a"
                    x: (parent.width - width) / 2 + stickItem.dx * (parent.width - width) / 2
                    y: (parent.height - height) / 2 + stickItem.dy * (parent.height - height) / 2
                }
                Item {
                    id: stickItem
                    anchors.fill: parent
                    property real dx: 0
                    property real dy: 0
                    function update(p) {
                        var r = width / 2
                        var vx = (p.x - r) / (r * 0.75), vy = (p.y - r) / (r * 0.75) // full deflection before the edge
                        var m = Math.sqrt(vx * vx + vy * vy)
                        if (m > 1) { vx /= m; vy /= m }
                        dx = vx; dy = vy
                        root.osc.stick(modelData.which, vx, vy)
                    }
                    PointHandler {
                        id: stickPoint
                        enabled: modelData.type === "stick"
                        onActiveChanged: {
                            if (!active) { stickItem.dx = 0; stickItem.dy = 0; root.osc.stick(modelData.which, 0, 0) }
                        }
                        onPointChanged: if (active) stickItem.update(point.position)
                    }
                }
            }

            // ---- D-pad (8 directions)
            Item {
                id: dpad
                visible: modelData.type === "dpad"
                anchors.fill: parent
                property int flags: 0
                Rectangle { x: parent.width / 3; width: parent.width / 3; height: parent.height; color: "#2b3240"; border.color: "#55ffffff" }
                Rectangle { y: parent.height / 3; height: parent.height / 3; width: parent.width; color: "#2b3240"; border.color: "#55ffffff" }
                Rectangle { // pressed highlight
                    visible: dpad.flags !== 0
                    x: parent.width / 3; y: parent.height / 3; width: parent.width / 3; height: width
                    color: "#4dabf7"
                }
                function setFlags(f) {
                    var all = [1, 2, 4, 8] // up, down, left, right
                    for (var i = 0; i < all.length; i++) {
                        if ((f & all[i]) !== (flags & all[i])) root.osc.button(all[i], (f & all[i]) !== 0)
                    }
                    flags = f
                }
                PointHandler {
                    enabled: modelData.type === "dpad"
                    onActiveChanged: if (!active) dpad.setFlags(0)
                    onPointChanged: {
                        if (!active) return
                        var cx = dpad.width / 2, cy = dpad.height / 2
                        var vx = point.position.x - cx, vy = point.position.y - cy
                        var dead = dpad.width / 8, f = 0
                        if (vy < -dead && Math.abs(vy) > Math.abs(vx) * 0.4) f |= 1
                        if (vy > dead && Math.abs(vy) > Math.abs(vx) * 0.4) f |= 2
                        if (vx < -dead && Math.abs(vx) > Math.abs(vy) * 0.4) f |= 4
                        if (vx > dead && Math.abs(vx) > Math.abs(vy) * 0.4) f |= 8
                        dpad.setFlags(f)
                    }
                }
            }
        }
    }
}
