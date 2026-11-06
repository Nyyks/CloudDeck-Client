// Home layout A, "Console": sidebar with pages, a big round Connect button on Home.
// Phones (narrow windows) get a header and a strip of page chips instead of the sidebar.
// Navigation: D-pad up/down (Tab order), left = back to the sidebar, right = into the page,
// B/Escape = sidebar, then Home, then the usual quit prompt. See docs/adr/0013-home-layouts.md.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import ComputerManager 1.0
import CloudDeck 1.0
import CloudDeckMic 1.0
import CloudDeckDisplay 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import "CdTheme.js" as T

FocusScope {
    id: home
    property string page: "home"
    readonly property bool narrow: width < T.narrowWidth
    readonly property var pages: [
        { id: "home", label: qsTr("Home"), icon: "home" },
        { id: "display", label: qsTr("Video"), icon: "display" },
        { id: "audio", label: qsTr("Audio"), icon: "mic" },
        { id: "controls", label: qsTr("Controls"), icon: "gamepad" },
        { id: "network", label: qsTr("Network"), icon: "network" },
        { id: "general", label: qsTr("General"), icon: "gear" },
        { id: "about", label: qsTr("About"), icon: "account", hidden: true } // linked under the account
    ]
    readonly property var pageFiles: ({
        display: "CdPageDisplay.qml", audio: "CdPageAudio.qml", controls: "CdPageControls.qml",
        network: "CdPageNetwork.qml", general: "CdPageGeneral.qml", about: "CdPageAbout.qml"
    })

    function pageIndex(id) {
        for (var i = 0; i < pages.length; i++) if (pages[i].id === id) return i
        return 0
    }
    function pageLabel(id) { return pages[pageIndex(id)].label }
    function navItem(id) { return (narrow ? chipNav : sideNav).itemAt(pageIndex(id)) }

    // API used by CloudDeckView
    function focusDefault(reason) { page = "home"; connectBtn.forceActiveFocus(reason) }
    function openSettings(what, reason) {
        page = "general"
        if (what === "style")
            Qt.callLater(function() { if (pageLoader.item) pageLoader.item.focusStyle(reason) })
        else
            navItem("general").forceActiveFocus(reason)
    }
    function showPage(id, reason) {
        page = id
        if (reason !== undefined) navItem(id).forceActiveFocus(reason)
    }
    // Controls page on a given tab ("input", "gamepad", "usb")
    function showControls(tab) {
        page = "controls"
        Qt.callLater(function() { if (pageLoader.item) pageLoader.item.tab = tab })
    }
    function focusContent(reason) {
        if (page === "home") { connectBtn.forceActiveFocus(reason); return }
        var it = pageCol.nextItemInFocusChain(true)
        for (var p = it; p; p = p.parent) if (p === contentScope) { it.forceActiveFocus(reason); return }
    }

    onPageChanged: content.contentY = 0

    Keys.onEscapePressed: {
        if (contentScope.activeFocus && !narrow) navItem(page).forceActiveFocus(Qt.TabFocusReason)
        else if (page !== "home") showPage("home", Qt.TabFocusReason)
        else event.accepted = false
    }

    Rectangle { anchors.fill: parent; color: T.bg }

    // ---------------- sidebar (wide)
    Rectangle {
        id: sidebar
        visible: !home.narrow
        width: visible ? 232 : 0
        height: parent.height
        color: T.surface
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: T.border }

        Keys.onRightPressed: home.focusContent(Qt.TabFocusReason)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            anchors.topMargin: 24
            spacing: 4

            Row {
                Layout.leftMargin: 8
                Layout.bottomMargin: 20
                spacing: 10
                Image { source: "qrc:/res/clouddeck-glyph.svg"; width: 28; height: 28; sourceSize.width: 56; sourceSize.height: 56 }
                Text {
                    text: "CloudDeck"
                    color: T.text
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Repeater {
                id: sideNav
                model: home.pages
                delegate: CdNavItem {
                    Layout.fillWidth: true
                    visible: !modelData.hidden
                    text: modelData.label
                    iconUrl: "qrc:/res/cd/" + modelData.icon + ".svg"
                    selected: home.page === modelData.id
                    onClicked: home.page = modelData.id
                }
            }
            Item { Layout.fillHeight: true }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: acct.implicitHeight + 20
                radius: 10
                color: T.card
                RowLayout {
                    id: acct
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10
                    Rectangle {
                        width: 32; height: 32; radius: 16
                        color: T.controlDown
                        Text {
                            anchors.centerIn: parent
                            text: CloudDeck.email.length > 0 ? CloudDeck.email.charAt(0).toUpperCase() : "?"
                            color: T.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: CloudDeck.email
                            color: T.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Row {
                            spacing: 4
                            CdButton {
                                kind: "link"
                                leftPadding: 0
                                minSize: 36
                                font.pixelSize: 12
                                text: qsTr("About")
                                onClicked: home.showPage("about")
                            }
                            CdButton {
                                id: signOutBtn
                                kind: "link"
                                minSize: 36
                                font.pixelSize: 12
                                text: qsTr("Sign out")
                                onClicked: CloudDeck.signOut()
                            }
                        }
                    }
                }
            }
        }
    }

    // ---------------- header + page chips (narrow)
    Column {
        id: topBar
        visible: home.narrow
        x: 0
        width: parent.width
        height: visible ? implicitHeight : 0
        topPadding: 16
        spacing: 12
        Row {
            x: 16
            spacing: 10
            Image { source: "qrc:/res/clouddeck-glyph.svg"; width: 26; height: 26; sourceSize.width: 52; sourceSize.height: 52 }
            Text {
                text: "CloudDeck"
                color: T.text
                font.pixelSize: 18
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Flickable {
            width: parent.width
            height: 56
            contentWidth: chipRow.width + 32
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Row {
                id: chipRow
                x: 16
                y: 4
                spacing: 8
                Repeater {
                    id: chipNav
                    model: home.pages
                    delegate: CdNavItem {
                        compact: true
                        visible: !modelData.hidden
                        text: modelData.label
                        iconUrl: "qrc:/res/cd/" + modelData.icon + ".svg"
                        selected: home.page === modelData.id
                        onClicked: home.page = modelData.id
                        onActiveFocusChanged: {
                            if (!activeFocus) return
                            var f = parent.parent.parent // Flickable
                            var px = x + chipRow.x
                            if (px < f.contentX) f.contentX = Math.max(0, px - 16)
                            else if (px + width > f.contentX + f.width) f.contentX = px + width + 16 - f.width
                        }
                    }
                }
            }
        }
    }

    // ---------------- page content
    FocusScope {
        id: contentScope
        focus: true
        x: sidebar.width
        y: topBar.height
        width: parent.width - x
        height: parent.height - y

        Keys.onLeftPressed: {
            if (!home.narrow) home.navItem(home.page).forceActiveFocus(Qt.TabFocusReason)
            else event.accepted = false
        }
        Keys.onRightPressed: cdView.moveFocus(true)

        Flickable {
            id: content
            anchors.fill: parent
            contentWidth: width
            contentHeight: pageCol.implicitHeight + 2 * pageCol.y
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: ScrollBar { }

            ColumnLayout {
                id: pageCol
                width: Math.min(content.width - 2 * x, 880)
                x: home.narrow ? 16 : 48
                y: home.narrow ? 8 : 40
                spacing: 24

                Text {
                    visible: home.page !== "home"
                    text: home.pageLabel(home.page)
                    color: T.text
                    font.pixelSize: home.narrow ? 24 : 30
                    font.weight: Font.Bold
                }

                // ---- Home
                ColumnLayout {
                    visible: home.page === "home"
                    Layout.fillWidth: true
                    spacing: 24

                    GridLayout {
                        Layout.fillWidth: true
                        columns: home.narrow ? 1 : 2
                        rowSpacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: qsTr("Your CloudDeck"); color: T.text; font.pixelSize: home.narrow ? 26 : 34; font.weight: Font.Bold }
                            Text {
                                Layout.fillWidth: true
                                text: (CloudDeck.machineRegion ? CloudDeck.machineRegion + "  ·  " : "") + CloudDeck.hostAddress
                                color: T.muted
                                font.pixelSize: 14
                                elide: Text.ElideRight
                            }
                        }
                        CdStatusPill {
                            Layout.alignment: home.narrow ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignVCenter)
                            status: CloudDeck.machineStatus
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: connectGrid.implicitHeight + 2 * (home.narrow ? 24 : 40)
                        radius: 20
                        color: T.card
                        border.width: 1
                        border.color: T.border

                        GridLayout {
                            id: connectGrid
                            anchors.fill: parent
                            anchors.margins: home.narrow ? 24 : 40
                            columns: home.narrow ? 1 : 2
                            columnSpacing: 40
                            rowSpacing: 20

                            CdButton {
                                id: connectBtn
                                focus: true
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                readonly property int size: home.narrow ? 148 : 168
                                Layout.preferredWidth: size
                                Layout.preferredHeight: size
                                kind: "primary"
                                radius: size / 2
                                stacked: true
                                iconUrl: "qrc:/res/cd/play.svg"
                                iconSize: 40
                                font.pixelSize: CloudDeck.machineStatus === "running" ? 18 : 15
                                font.weight: Font.Bold
                                text: cdView.connectLabel()
                                enabled: cdView.canConnect()
                                onClicked: CloudDeck.startAndConnect(ComputerManager)
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 12
                                Text {
                                    Layout.fillWidth: true
                                    text: cdView.headline()
                                    color: T.text
                                    font.pixelSize: 22
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: home.narrow ? Text.AlignHCenter : Text.AlignLeft
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: cdView.streamSummary()
                                    color: T.textSecondary
                                    font.pixelSize: 15
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: home.narrow ? Text.AlignHCenter : Text.AlignLeft
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    CdQuickChoice {
                                        enabled: !CloudDeckDisplay.multiMonitor
                                        label: cdView.resolutionLabel()
                                        options: cdView.resolutionChoices()
                                        value: cdView.resolutionValue()
                                        onPicked: function(v) { cdView.setResolution(v) }
                                    }
                                    CdQuickChoice {
                                        visible: SystemProperties.hasDesktopEnvironment
                                        label: cdView.displayModeLabel()
                                        options: cdView.displayModeChoices()
                                        value: StreamingPreferences.windowMode
                                        onPicked: function(v) { cdView.setDisplayMode(v) }
                                    }
                                    CdChip {
                                        on: CloudDeckMic.enabled
                                        text: on ? qsTr("Microphone on") : qsTr("Microphone off")
                                        onClicked: CloudDeckMic.enabled = !CloudDeckMic.enabled
                                    }
                                    CdChip {
                                        on: CloudDeck.clipboardSync
                                        text: on ? qsTr("Clipboard shared") : qsTr("Clipboard off")
                                        onClicked: CloudDeck.clipboardSync = !CloudDeck.clipboardSync
                                    }
                                    CdChip {
                                        on: cdView.usbForwardCount() > 0
                                        text: cdView.usbChipText()
                                        onClicked: home.showControls("usb")
                                    }
                                    CdChip {
                                        on: CloudDeck.autoReconnect
                                        text: qsTr("Auto-reconnect")
                                        onClicked: CloudDeck.autoReconnect = !CloudDeck.autoReconnect
                                    }
                                }
                                CdProgress { Layout.fillWidth: true }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: home.narrow ? 2 : 3
                        columnSpacing: 16
                        rowSpacing: 16
                        CdTile {
                            Layout.fillWidth: true
                            readonly property bool off: CloudDeck.machineStatus === "off"
                            text: off ? qsTr("Start") : qsTr("Restart")
                            subtitle: off ? qsTr("Turn the VM on without connecting") : qsTr("Stop and start the VM")
                            iconUrl: "qrc:/res/cd/restart.svg"
                            enabled: !CloudDeck.busy && (off || CloudDeck.machineStatus === "running")
                            onClicked: off ? CloudDeck.startMachine() : cdView.confirmRestart()
                        }
                        CdTile {
                            Layout.fillWidth: true
                            text: qsTr("Stop")
                            subtitle: qsTr("Turn the VM off")
                            iconUrl: "qrc:/res/cd/power.svg"
                            enabled: !CloudDeck.busy && CloudDeck.machineStatus === "running"
                            onClicked: cdView.confirmStop()
                        }
                        CdTile {
                            Layout.fillWidth: true
                            Layout.columnSpan: home.narrow ? 2 : 1
                            text: CloudDeck.netTestRunning ? qsTr("Testing...") : qsTr("Test my connection")
                            subtitle: CloudDeck.netTestStatus !== "" ? CloudDeck.netTestStatus : qsTr("Latency and speed to your CloudDeck")
                            iconUrl: "qrc:/res/cd/speed.svg"
                            enabled: !CloudDeck.netTestRunning
                            onClicked: CloudDeck.runNetworkTest(ComputerManager)
                        }
                    }

                    // Account on phones (the sidebar holds it otherwise)
                    RowLayout {
                        visible: home.narrow
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: CloudDeck.email; color: T.muted; font.pixelSize: 13; elide: Text.ElideRight }
                        CdButton { kind: "link"; text: qsTr("About"); onClicked: home.showPage("about") }
                        CdButton { kind: "link"; text: qsTr("Sign out"); onClicked: CloudDeck.signOut() }
                    }
                }

                // ---- settings pages
                Loader {
                    id: pageLoader
                    Layout.fillWidth: true
                    active: home.page !== "home"
                    visible: active
                    source: active ? home.pageFiles[home.page] : ""
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }
}
