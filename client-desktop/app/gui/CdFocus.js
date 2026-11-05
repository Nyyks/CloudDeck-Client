.pragma library
// Scrolls the nearest Flickable so a control that just got keyboard or gamepad focus is
// fully visible (D-pad and TV-remote navigation through long settings pages).
function ensureVisible(item) {
    if (!item) return
    var f = item.parent
    while (f && !(f.flickableDirection !== undefined && f.contentItem !== undefined && f.contentY !== undefined))
        f = f.parent
    if (!f || f.contentHeight <= f.height) return
    var pos = item.mapToItem(f.contentItem, 0, 0)
    var margin = 24
    var maxY = Math.max(0, f.contentHeight - f.height)
    if (pos.y - margin < f.contentY)
        f.contentY = Math.max(0, pos.y - margin)
    else if (pos.y + item.height + margin > f.contentY + f.height)
        f.contentY = Math.min(maxY, pos.y + item.height + margin - f.height)
}
