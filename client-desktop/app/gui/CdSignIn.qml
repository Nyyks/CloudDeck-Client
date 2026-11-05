// Sign-in card shown before the home screen.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import "CdTheme.js" as T
import "CdFocus.js" as F

FocusScope {
    id: signIn

    function submit() {
        if (CloudDeck.mfaRequired) {
            CloudDeck.submitMfaCode(mfaField.text)
            mfaField.text = ""
        } else {
            CloudDeck.signIn(emailField.text, passwordField.text)
            passwordField.text = ""
        }
    }
    function focusDefault(reason) {
        if (CloudDeck.mfaRequired) mfaField.forceActiveFocus(reason)
        else if (emailField.text === "") emailField.forceActiveFocus(reason)
        else passwordField.forceActiveFocus(reason)
    }

    Connections {
        target: CloudDeck
        function onMfaRequiredChanged() { signIn.focusDefault(Qt.OtherFocusReason) }
    }

    Rectangle { anchors.fill: parent; color: T.bg }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, card.height + 64)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Rectangle {
            id: card
            width: Math.min(440, flick.width - 32)
            height: col.implicitHeight + 64
            x: (flick.width - width) / 2
            y: Math.max(32, (flick.height - height) / 2)
            radius: 20
            color: T.card
            border.width: 1
            border.color: T.border

            ColumnLayout {
                id: col
                x: 32
                y: 32
                width: parent.width - 64
                spacing: 14

                Image {
                    source: "qrc:/res/clouddeck-glyph.svg"
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    sourceSize.width: 96
                    sourceSize.height: 96
                }
                Text {
                    text: CloudDeck.mfaRequired ? qsTr("Enter your code") : qsTr("Sign in to CloudDeck")
                    color: T.text
                    font.pixelSize: 26
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: T.muted
                    font.pixelSize: 14
                    text: CloudDeck.mfaRequired
                          ? qsTr("Type the verification code from your authenticator app or email.")
                          : qsTr("Your password is used once and never stored. A session token is kept in %1.").arg(CloudDeck.keychainBackend)
                }
                TextField {
                    id: emailField
                    focus: !CloudDeck.mfaRequired
                    Layout.fillWidth: true
                    visible: !CloudDeck.mfaRequired
                    font.pixelSize: 16
                    placeholderText: qsTr("Email")
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    text: CloudDeck.email
                    onAccepted: passwordField.forceActiveFocus(Qt.TabFocusReason)
                    onActiveFocusChanged: if (activeFocus) F.ensureVisible(emailField)
                }
                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    visible: !CloudDeck.mfaRequired
                    font.pixelSize: 16
                    placeholderText: qsTr("Password")
                    echoMode: TextInput.Password
                    onAccepted: signIn.submit()
                    onActiveFocusChanged: if (activeFocus) F.ensureVisible(passwordField)
                }
                TextField {
                    id: mfaField
                    focus: CloudDeck.mfaRequired
                    Layout.fillWidth: true
                    visible: CloudDeck.mfaRequired
                    font.pixelSize: 16
                    placeholderText: qsTr("Verification code")
                    inputMethodHints: Qt.ImhDigitsOnly
                    onAccepted: signIn.submit()
                }
                CdButton {
                    id: signInButton
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    kind: "primary"
                    enabled: !CloudDeck.busy
                    text: CloudDeck.mfaRequired ? qsTr("Verify") : qsTr("Sign in")
                    onClicked: signIn.submit()
                }
                CdProgress { Layout.fillWidth: true }
                CdButton {
                    kind: "link"
                    leftPadding: 0
                    font.pixelSize: 13
                    font.weight: Font.Normal
                    text: qsTr("Use another streaming host (Manage PCs)")
                    onClicked: cdView.managePcs()
                }
            }
        }
    }
}
