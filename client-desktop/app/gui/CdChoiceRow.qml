// A drop-down for one setting: options are [{ text, val }], `value` is the current val.
import QtQuick 2.9

CdComboRow {
    id: row
    property var options: []
    property var value
    signal picked(var value)

    function indexOfValue(v) {
        for (var i = 0; i < options.length; i++) if (options[i].val === v) return i
        return 0
    }

    model: options.map(function(o) { return o.text })
    currentIndex: indexOfValue(value)
    onActivated: function(i) { if (i >= 0 && i < options.length) row.picked(options[i].val) }
}
