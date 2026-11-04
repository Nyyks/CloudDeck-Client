// CloudDeck-Client: home screen. Sign in, VM card, and one-click "Start and connect".
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.2

import ComputerManager 1.0
import CloudDeck 1.0

Item {
    id: root
    objectName: qsTr("CloudDeck")

    property var activeLauncher: null
    property bool streamLaunched: false

    function statusColor(s) {
        if (s === "running") return "#40c057"
        if (s === "off") return "#868e96"
        if (s === "starting" || s === "stopping") return "#fab005"
        if (s === "error" || s === "failed") return "#fa5252"
        return "#868e96"
    }
    function statusLabel(s) {
        switch (s) {
        case "running": return qsTr("Running")
        case "off": return qsTr("Stopped")
        case "starting": return qsTr("Starting")
        case "stopping": return qsTr("Stopping")
        case "": return qsTr("Unknown")
        default: return s
        }
    }

    // --- stream hand-off (mirrors CliStartStreamSegue.qml, without quitting the app)
    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "quitAfter": false
        })
        streamLaunched = true
        stackView.push(segue)
    }
    function onLaunchFailed(message) {
        launchStatus.text = ""
        errorDialog.text = message
        errorDialog.open()
        CloudDeck.streamEnded()
    }
    function onAppQuitRequired(appName) {
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    Connections {
        target: CloudDeck
        function onStreamReady(launcher, appName) {
            activeLauncher = launcher
            launchStatus.text = qsTr("Launching %1...").arg(appName)
            launcher.searchingComputer.connect(function() { launchStatus.text = qsTr("Connecting to the streaming host...") })
            launcher.searchingApp.connect(function() { launchStatus.text = qsTr("Loading app list...") })
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
        }
    }

    StackView.onActivated: {
        if (streamLaunched) {
            // We are back from a stream we launched.
            streamLaunched = false
            launchStatus.text = ""
            CloudDeck.streamEnded()
        }
        CloudDeck.refreshStatus()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.min(parent.width - 32, 560)
            x: (parent.width - width) / 2
            spacing: 16

            Item { Layout.preferredHeight: 16 }

            // ---------------- restoring
            RowLayout {
                visible: CloudDeck.restoring
                Layout.alignment: Qt.AlignHCenter
                BusyIndicator { running: parent.visible }
                Label { text: qsTr("Signing in..."); font.pointSize: 14 }
            }

            // ---------------- sign-in
            Pane {
                visible: !CloudDeck.signedIn && !CloudDeck.restoring
                Layout.fillWidth: true
                Material.elevation: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label { text: qsTr("Sign in to CloudDeck"); font.pointSize: 18; font.bold: true }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        opacity: 0.8
                        text: qsTr("Your password is used once and never stored. A session token is kept in %1.")
                              .arg(CloudDeck.keychainBackend)
                    }
                    TextField {
                        id: emailField
                        Layout.fillWidth: true
                        visible: !CloudDeck.mfaRequired
                        placeholderText: qsTr("Email")
                        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                        text: CloudDeck.email
                        onAccepted: passwordField.forceActiveFocus()
                    }
                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        visible: !CloudDeck.mfaRequired
                        placeholderText: qsTr("Password")
                        echoMode: TextInput.Password
                        onAccepted: signInButton.clicked()
                    }
                    TextField {
                        id: mfaField
                        Layout.fillWidth: true
                        visible: CloudDeck.mfaRequired
                        placeholderText: qsTr("Verification code")
                        inputMethodHints: Qt.ImhDigitsOnly
                        onAccepted: signInButton.clicked()
                    }
                    Button {
                        id: signInButton
                        Layout.fillWidth: true
                        highlighted: true
                        enabled: !CloudDeck.busy
                        text: CloudDeck.mfaRequired ? qsTr("Verify") : qsTr("Sign in")
                        onClicked: {
                            if (CloudDeck.mfaRequired) {
                                CloudDeck.submitMfaCode(mfaField.text)
                                mfaField.text = ""
                            } else {
                                CloudDeck.signIn(emailField.text, passwordField.text)
                                passwordField.text = ""
                            }
                        }
                    }
                }
            }

            // ---------------- VM card
            Pane {
                visible: CloudDeck.signedIn
                Layout.fillWidth: true
                Material.elevation: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Your CloudDeck"); font.pointSize: 18; font.bold: true; Layout.fillWidth: true }
                        Rectangle {
                            width: 12; height: 12; radius: 6
                            color: statusColor(CloudDeck.machineStatus)
                        }
                        Label { text: statusLabel(CloudDeck.machineStatus); font.pointSize: 13 }
                    }
                    Label {
                        opacity: 0.7
                        text: CloudDeck.hostAddress + (CloudDeck.machineRegion ? "  ·  " + CloudDeck.machineRegion : "")
                    }

                    Button {
                        Layout.fillWidth: true
                        highlighted: true
                        font.pointSize: 14
                        enabled: !CloudDeck.busy && CloudDeck.machineStatus !== "stopping"
                        text: CloudDeck.machineStatus === "running" ? qsTr("Connect") : qsTr("Start and connect")
                        onClicked: CloudDeck.startAndConnect(ComputerManager)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Start")
                            enabled: !CloudDeck.busy && CloudDeck.machineStatus === "off"
                            onClicked: CloudDeck.startMachine()
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Stop")
                            enabled: !CloudDeck.busy && CloudDeck.machineStatus === "running"
                            onClicked: stopConfirm.open()
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Restart")
                            enabled: !CloudDeck.busy && CloudDeck.machineStatus === "running"
                            onClicked: restartConfirm.open()
                        }
                    }

                    RowLayout {
                        visible: CloudDeck.connecting || launchStatus.text !== ""
                        Layout.fillWidth: true
                        BusyIndicator { running: parent.visible; implicitWidth: 32; implicitHeight: 32 }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: CloudDeck.connecting ? CloudDeck.progressText : launchStatus.text
                        }
                        Button {
                            visible: CloudDeck.connecting
                            text: qsTr("Cancel")
                            flat: true
                            onClicked: CloudDeck.cancelConnect()
                        }
                    }
                    Label { id: launchStatus; visible: false; text: "" }
                }
            }

            Label {
                visible: CloudDeck.errorText !== ""
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#ff8787"
                text: CloudDeck.errorText
            }

            // ---------------- options
            Pane {
                visible: CloudDeck.signedIn
                Layout.fillWidth: true
                Material.elevation: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    Label { text: qsTr("Options"); font.pointSize: 14; font.bold: true }
                    Switch {
                        text: qsTr("Stop my CloudDeck when the stream ends")
                        checked: CloudDeck.autoStopOnSessionEnd
                        onToggled: CloudDeck.autoStopOnSessionEnd = checked
                    }
                    RowLayout {
                        Label { text: qsTr("Stop after idle minutes (0 = never):"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                        SpinBox {
                            from: 0; to: 240; stepSize: 5; editable: true
                            value: CloudDeck.autoStopIdleMinutes
                            onValueModified: CloudDeck.autoStopIdleMinutes = value
                        }
                    }
                    RowLayout {
                        Label { text: qsTr("App to launch:"); Layout.fillWidth: true }
                        TextField {
                            text: CloudDeck.appName
                            onEditingFinished: CloudDeck.appName = text
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: qsTr("Manage PCs")
                            flat: true
                            onClicked: navigateTo("qrc:/gui/PcView.qml", PcView)
                        }
                        Item { Layout.fillWidth: true }
                        Label { text: CloudDeck.email; opacity: 0.7 }
                        Button {
                            text: qsTr("Sign out")
                            flat: true
                            onClicked: CloudDeck.signOut()
                        }
                    }
                }
            }
        }
    }

    NavigableMessageDialog {
        id: stopConfirm
        text: qsTr("Stop your CloudDeck? Unsaved work on it will be lost.")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: CloudDeck.stopMachine()
    }
    NavigableMessageDialog {
        id: restartConfirm
        text: qsTr("Restart your CloudDeck? Unsaved work on it will be lost.")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: CloudDeck.restartMachine()
    }
    ErrorMessageDialog {
        id: errorDialog
    }
    NavigableMessageDialog {
        id: quitAppDialog
        property string appName: ""
        text: qsTr("%1 is already running on your CloudDeck. Quit it and start %2? Unsaved progress in it will be lost.")
              .arg(appName).arg(CloudDeck.appName)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName, "quitRunningAppFn": function() { activeLauncher.quitRunningApp() }}
            stackView.push(component.createObject(stackView, params))
        }
        onRejected: {
            launchStatus.text = ""
            CloudDeck.streamEnded()
        }
    }
}
