.pragma library
// CloudDeck-Client home screen palette (Phase 7, docs/adr/0013-home-layouts.md).
// Shared by the Console and Launcher layouts; colours follow the Phase 7 mock-ups.

var bg = "#0f1115"            // Console ground
var bgLauncher = "#0b0c0f"    // Launcher ground
var surface = "#14171d"       // sidebar, tiles
var card = "#181b22"
var drawer = "#15171c"
var border = "#23272f"
var border2 = "#2a2f39"
var control = "#1d2129"
var controlHover = "#232834"
var controlDown = "#2b3240"
var rowHover = "#1c2029"
var selected = "#1f2430"
var inset = "#0f1115"

var text = "#e9ecf2"
var textSecondary = "#aeb5c2"
var muted = "#8a93a3"

var accent = "#4c6ef5"
var accentSoft = "#1a2140"
var accentBorder = "#2f3b66"
var accentText = "#9db4ff"
var launcherAccent = "#e9ecf2"
var launcherAccentText = "#0b0c0f"

var focus = "#9db4ff"         // focus ring for keyboard, gamepad and TV remote navigation
var warnText = "#ffd43b"
var errorText = "#ff8787"

// Below this width (px) both layouts switch to the phone arrangement.
var narrowWidth = 720

function statusDot(s) {
    if (s === "running") return "#3ddc84"
    if (s === "starting" || s === "stopping") return "#fab005"
    if (s === "error" || s === "failed") return "#fa5252"
    return "#8a93a3"
}
function statusSoft(s) {
    if (s === "running") return "#12291c"
    if (s === "starting" || s === "stopping") return "#2e2610"
    if (s === "error" || s === "failed") return "#2f1517"
    return "#1f2329"
}
function statusInk(s) {
    if (s === "running") return "#6fe3a1"
    if (s === "starting" || s === "stopping") return "#ffd43b"
    if (s === "error" || s === "failed") return "#ff8787"
    return "#c9cfda"
}
