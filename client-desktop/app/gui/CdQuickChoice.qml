// A home-screen chip that opens a short list of choices (resolution, display mode).
// Options are [{ text, val }]; the chip shows `label` and the menu marks `value`.
import QtQuick 2.9
import QtQuick.Controls 2.5

CdChip {
    id: chip
    property var options: []
    property var value
    property string label: ""
    signal picked(var value)

    on: true
    text: label + "  ▾"
    onClicked: menu.open()

    Menu {
        id: menu
        y: chip.height + 4
        Instantiator {
            model: chip.options
            delegate: MenuItem {
                text: modelData.text
                checkable: true
                checked: modelData.val === chip.value
                onTriggered: chip.picked(modelData.val)
            }
            onObjectAdded: function(index, object) { menu.insertItem(index, object) }
            onObjectRemoved: function(index, object) { menu.removeItem(object) }
        }
    }
}
