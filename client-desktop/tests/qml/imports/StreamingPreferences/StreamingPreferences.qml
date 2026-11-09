pragma Singleton
import QtQuick 2.15
// Stub of Moonlight's StreamingPreferences (app/settings/streamingpreferences.h).
QtObject {
    enum WindowMode { WM_FULLSCREEN, WM_FULLSCREEN_DESKTOP, WM_WINDOWED }
    enum AudioConfig { AC_STEREO, AC_51_SURROUND, AC_71_SURROUND }
    enum VideoDecoderSelection { VDS_AUTO, VDS_FORCE_HARDWARE, VDS_FORCE_SOFTWARE }
    enum VideoCodecConfig { VCC_AUTO, VCC_FORCE_H264, VCC_FORCE_HEVC, VCC_FORCE_AV1 }
    enum RendererSelection { RS_AUTO, RS_VULKAN, RS_METAL, RS_AVSBDL }
    enum CaptureSysKeysMode { CSK_OFF, CSK_FULLSCREEN, CSK_ALWAYS, CSK_FOCUSED }
    enum UIDisplayMode { UI_WINDOWED, UI_MAXIMIZED, UI_FULLSCREEN }
    enum Language { LANG_AUTO, LANG_EN, LANG_FR, LANG_ZH_CN, LANG_DE, LANG_NB_NO, LANG_RU, LANG_ES, LANG_JA, LANG_VI,
                    LANG_TH, LANG_KO, LANG_HU, LANG_NL, LANG_SV, LANG_TR, LANG_UK, LANG_ZH_TW, LANG_PT, LANG_PT_BR,
                    LANG_EL, LANG_IT, LANG_HI, LANG_PL, LANG_CS, LANG_HE, LANG_CKB, LANG_LT, LANG_ET, LANG_BG, LANG_EO, LANG_TA }

    property int width: 1920
    property int height: 1080
    property int fps: 60
    property int bitrateKbps: 20000
    property bool autoAdjustBitrate: true
    property bool unlockBitrate: false
    property bool enableYUV444: false
    property bool enableVsync: true
    property bool framePacing: false
    property bool enableHdr: false
    property int windowMode: 1
    property int recommendedFullScreenMode: 1
    property int audioConfig: 0
    property bool playAudioOnHost: false
    property bool muteOnFocusLoss: false
    property bool gameOptimizations: true
    property bool quitAppAfter: false
    property int language: 0
    property int uiDisplayMode: 0
    property bool connectionWarnings: true
    property bool configurationWarnings: true
    property bool richPresence: true
    property bool keepAwake: true
    property bool absoluteMouseMode: false
    property int captureSysKeysMode: 0
    property bool absoluteTouchMode: false
    property bool swapMouseButtons: false
    property bool reverseScrollDirection: false
    property bool swapFaceButtons: false
    property bool multiController: true
    property bool gamepadMouse: true
    property bool backgroundGamepad: false
    property int videoDecoderSelection: 0
    property int videoCodecConfig: 0
    property int rendererSelection: 0
    property bool enableMdns: true
    property bool detectNetworkBlocking: true
    property bool showPerformanceOverlay: false
    function getDefaultBitrate(w, h, f, yuv) { return Math.round(w * h * f / 6220.8 / 500) * 500 }
    function retranslate() { return true }
    function save() {}
}
