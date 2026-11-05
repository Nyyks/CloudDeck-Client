// Home layout C, "Launcher": the CloudDeck mark, one big "Start and connect" pill, quick chips,
// and every setting in a drawer (tabs Stream / Devices / Account). The drawer covers the whole
// screen on phones and can be swiped closed. Navigation: D-pad in Tab order, left/right step
// through rows, Menu/Start opens the settings, B/Escape closes them. See docs/adr/0013-home-layouts.md.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import ComputerManager 1.0
import CloudDeck 1.0
import CloudDeckMic 1.0
import "CdTheme.js" as T

FocusScope {
    id: home
    readonly property bool narrow: width < T.narrowWidth
    property string tab: "stream"
    property int returnReason: Qt.OtherFocusReason

    // API used by CloudDeckView
    function focusDefault(reason) { drawer.close(); startBtn.forceActiveFocus(reason) }
    function openSettings(what, reason) {
        returnReason = reason
        tab = what === "style" ? "account" : what === "usb" ? "devices" : (what && what !== "" ? what : tab)
        drawer.focusWhat = what === "style" ? "style" : ""
        drawer.focusReason = reason
        drawer.open()
    }

    Rectangle { anchors.fill: parent; color: T.bgLauncher }

    Item {
        id: main
        anchors.fill: parent
        anchors.margins: home.narrow ? 16 : 32

        Keys.onLeftPressed: cdView.moveFocus(false)
        Keys.onRightPressed: cdView.moveFocus(true)

        // ---- top row
        RowLayout {
            id: topRow
            width: parent.width
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: CloudDeck.email
                color: T.muted
                font.pixelSize: 13
                elide: Text.ElideRight
            }
            CdButton {
                id: settingsBtn
                kind: "ghost"
                text: home.narrow ? "" : qsTr("Settings")
                iconUrl: "qrc:/res/cd/gear.svg"
                onClicked: home.openSettings("", visualFocus ? Qt.TabFocusReason : Qt.OtherFocusReason)
            }
        }

        // ---- centre
        Flickable {
            id: centreFlick
            anchors.top: topRow.bottom
            anchors.bottom: linksRow.top
            width: parent.width
            contentWidth: width
            contentHeight: Math.max(height, centre.implicitHeight + 32)
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: centre
                width: Math.min(parent.width, 560)
                x: (parent.width - width) / 2
                y: Math.max(16, (centreFlick.height - implicitHeight) / 2)
                spacing: 18

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/res/clouddeck-glyph.svg"
                    Layout.preferredWidth: home.narrow ? 56 : 72
                    Layout.preferredHeight: Layout.preferredWidth
                    sourceSize.width: 144
                    sourceSize.height: 144
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Your CloudDeck")
                    color: "#f1f3f6"
                    font.pixelSize: home.narrow ? 30 : 40
                    font.weight: Font.Bold
                }
                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: T.statusDot(CloudDeck.machineStatus)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: cdView.statusLine()
                        color: T.muted
                        font.pixelSize: 15
                    }
                }
                CdButton {
                    id: startBtn
                    focus: true
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 8
                    Layout.preferredWidth: Math.min(320, centre.width)
                    Layout.preferredHeight: 64
                    kind: "primary"
                    fill: T.launcherAccent
                    fillText: T.launcherAccentText
                    radius: 32
                    iconUrl: "qrc:/res/cd/play-dark.svg"
                    iconSize: 22
                    font.pixelSize: 19
                    font.weight: Font.Bold
                    text: cdView.connectLabel()
                    enabled: cdView.canConnect()
                    onClicked: CloudDeck.startAndConnect(ComputerManager)
                }
                Flow {
                    id: chips
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: centre.width
                    spacing: 10
                    CdChip {
                        on: cdView.fitsWindow()
                        text: qsTr("Fit to window")
                        onClicked: cdView.toggleFit()
                    }
                    CdChip {
                        on: CloudDeckMic.enabled
                        text: on ? qsTr("Mic on") : qsTr("Mic off")
                        onClicked: CloudDeckMic.enabled = !CloudDeckMic.enabled
                    }
                    CdChip {
                        on: cdView.usbForwardCount() > 0
                        text: cdView.usbChipText()
                        onClicked: home.openSettings("usb", visualFocus ? Qt.TabFocusReason : Qt.OtherFocusReason)
                    }
                }
                CdProgress { Layout.fillWidth: true }
            }
        }

        // ---- bottom links
        Row {
            id: linksRow
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: home.narrow ? 4 : 20
            CdButton {
                readonly property bool off: CloudDeck.machineStatus === "off"
                kind: "link"
                font.pixelSize: 14
                font.weight: Font.Normal
                text: off ? (home.narrow ? qsTr("Start") : qsTr("Start without connecting")) : qsTr("Restart")
                enabled: !CloudDeck.busy && (off || CloudDeck.machineStatus === "running")
                onClicked: off ? CloudDeck.startMachine() : cdView.confirmRestart()
            }
            CdButton {
                kind: "link"
                font.pixelSize: 14
                font.weight: Font.Normal
                text: qsTr("Stop")
                enabled: !CloudDeck.busy && CloudDeck.machineStatus === "running"
                onClicked: cdView.confirmStop()
            }
            CdButton {
                kind: "link"
                font.pixelSize: 14
                font.weight: Font.Normal
                text: CloudDeck.netTestRunning ? qsTr("Testing...") : qsTr("Test connection")
                enabled: !CloudDeck.netTestRunning
                onClicked: {
                    CloudDeck.runNetworkTest(ComputerManager)
                    home.openSettings("stream", visualFocus ? Qt.TabFocusReason : Qt.OtherFocusReason)
                }
            }
        }
    }

    // ---------------- settings drawer
    Drawer {
        id: drawer
        property string focusWhat: ""
        property int focusReason: Qt.OtherFocusReason
        edge: Qt.RightEdge
        width: home.narrow ? home.width : Math.min(420, home.width)
        height: home.height
        interactive: home.visible && opened  // swipe to close; opening is a button, not an edge swipe
        focus: true
        dim: true
        modal: true
        Overlay.modal: Rectangle { color: "#99000000" }
        padding: 0
        topPadding: 0
        bottomPadding: 0

        onOpened: {
            if (focusWhat === "style")
                Qt.callLater(function() { if (accountTab.item) accountTab.item.focusStyle(drawer.focusReason) })
            else
                tabs.focusCurrent(focusReason)
        }
        onClosed: settingsBtn.forceActiveFocus(home.returnReason)

        background: Rectangle {
            color: T.drawer
            Rectangle { width: 1; height: parent.height; color: T.border }
        }

        contentItem: FocusScope {
            Keys.onUpPressed: cdView.moveFocus(false)
            Keys.onDownPressed: cdView.moveFocus(true)
            Keys.onMenuPressed: drawer.close()
            Keys.onHangupPressed: drawer.close()

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: home.narrow ? 16 : 24
                anchors.topMargin: 24
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Settings")
                        color: T.text
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }
                    CdButton {
                        id: closeBtn
                        kind: "ghost"
                        iconUrl: "qrc:/res/cd/close.svg"
                        iconSize: 16
                        text: ""
                        onClicked: drawer.close()
                    }
                }
                CdSegmented {
                    id: tabs
                    Layout.fillWidth: true
                    options: [{ label: qsTr("Stream"), value: "stream" },
                              { label: qsTr("Devices"), value: "devices" },
                              { label: qsTr("Account"), value: "account" }]
                    value: home.tab
                    onPicked: function(v) { home.tab = v; drawerFlick.contentY = 0 }
                }
                Flickable {
                    id: drawerFlick
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: tabCol.implicitHeight + 16
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true
                    ScrollBar.vertical: ScrollBar { }

                    ColumnLayout {
                        id: tabCol
                        width: drawerFlick.width
                        spacing: 16

                        // Stream
                        Loader { Layout.fillWidth: true; active: home.tab === "stream"; visible: active; source: "CdPageDisplay.qml" }
                        Loader { Layout.fillWidth: true; active: home.tab === "stream"; visible: active; source: "CdPageNetwork.qml" }
                        // Devices
                        Loader { Layout.fillWidth: true; active: home.tab === "devices"; visible: active; source: "CdPageAudio.qml" }
                        Loader { Layout.fillWidth: true; active: home.tab === "devices"; visible: active; source: "CdPageControllers.qml" }
                        Loader { Layout.fillWidth: true; active: home.tab === "devices"; visible: active; source: "CdPageUsb.qml" }
                        // Account
                        CdCard {
                            visible: home.tab === "account"
                            title: qsTr("Account")
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 14
                                Layout.rightMargin: 6
                                spacing: 12
                                Image { source: "qrc:/res/cd/account.svg"; sourceSize.width: 40; sourceSize.height: 40; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                                Text { Layout.fillWidth: true; text: CloudDeck.email; color: T.text; font.pixelSize: 15; elide: Text.ElideRight }
                                CdButton { kind: "ghost"; text: qsTr("Sign out"); onClicked: { drawer.close(); CloudDeck.signOut() } }
                            }
                        }
                        Loader { id: accountTab; Layout.fillWidth: true; active: home.tab === "account"; visible: active; source: "CdPageGeneral.qml" }
                    }
                }
            }
        }
    }
}
