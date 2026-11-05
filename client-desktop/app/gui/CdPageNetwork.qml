// Settings page: network test and reconnecting (docs/adr/0010, docs/adr/0011).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import ComputerManager 1.0
import CloudDeck 1.0

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("Connection test")
        CdText { text: qsTr("Measures the latency and speed to your CloudDeck and suggests stream settings.") }
        CdButtonRow {
            CdButton {
                kind: "primary"
                text: CloudDeck.netTestRunning ? qsTr("Testing...") : qsTr("Test my connection")
                enabled: !CloudDeck.netTestRunning
                onClicked: CloudDeck.runNetworkTest(ComputerManager)
            }
            CdButton {
                visible: CloudDeck.netTestHasRecommendation
                text: qsTr("Apply recommended settings")
                onClicked: CloudDeck.applyRecommendation()
            }
        }
        CdText {
            hint: false
            text: CloudDeck.netTestStatus
        }
        CdText {
            text: CloudDeck.netTestHasRecommendation ? CloudDeck.netTestRecommendation : ""
        }
    }

    CdCard {
        title: qsTr("When the connection drops")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Reconnect automatically")
            description: qsTr("Retries a few times before giving up; the stream shows a notice meanwhile.")
            checked: CloudDeck.autoReconnect
            onToggled: CloudDeck.autoReconnect = checked
        }
    }
}
