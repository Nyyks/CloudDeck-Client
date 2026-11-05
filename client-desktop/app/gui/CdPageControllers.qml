// Settings page: controllers (docs/controller-profiles.md, docs/on-screen-controller.md).
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import CloudDeck 1.0
import CloudDeckControllers 1.0
import StreamingPreferences 1.0
import "CdTheme.js" as T

ColumnLayout {
    spacing: 16

    CdCard {
        title: qsTr("Button layout")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Nintendo button layout")
            description: qsTr("Swaps A/B and X/Y. You can also change it from the in-stream menu.")
            checked: StreamingPreferences.swapFaceButtons
            onToggled: {
                StreamingPreferences.swapFaceButtons = checked
                StreamingPreferences.save()
            }
        }
        CdText {
            text: qsTr("On a controller: Select+L1+R1+B opens the CloudDeck menu, Select+L1+R1+Y the on-screen keyboard.")
        }
    }

    CdCard {
        title: qsTr("On-screen controller")
        CdSwitch {
            Layout.fillWidth: true
            text: qsTr("Show the on-screen controller")
            description: qsTr("For touchscreens. You can also turn it on or off from the in-stream menu.")
            checked: CloudDeck.onScreenController
            onToggled: CloudDeck.onScreenController = checked
        }
        GridLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 6
            Layout.bottomMargin: 8
            columns: width < 440 ? 1 : 2
            Text { Layout.fillWidth: true; text: qsTr("Button style"); color: T.text; font.pixelSize: 15 }
            CdSegmented {
                Layout.preferredWidth: 260
                options: [{ label: qsTr("Xbox"), value: "xbox" }, { label: qsTr("PlayStation"), value: "playstation" }]
                value: CloudDeck.controllerStyle
                onPicked: function(v) { CloudDeck.controllerStyle = v }
            }
        }
    }

    CdCard {
        title: qsTr("Remapping")
        CdText {
            hint: false
            text: CloudDeckControllers.defaultProfile === ""
                  ? qsTr("No remapping.")
                  : qsTr("All controllers use \"%1\" unless set otherwise.").arg(CloudDeckControllers.defaultProfile)
        }
        Repeater {
            model: CloudDeckControllers.controllers
            delegate: RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 6
                spacing: 12
                Image { source: "qrc:/res/cd/gamepad.svg"; sourceSize.width: 40; sourceSize.height: 40; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                Text { Layout.fillWidth: true; text: modelData.name; color: T.text; font.pixelSize: 15; elide: Text.ElideRight }
                Rectangle {
                    radius: 8
                    color: modelData.profile !== "" ? T.accentSoft : T.selected
                    implicitWidth: prof.implicitWidth + 24
                    implicitHeight: 30
                    Text {
                        id: prof
                        anchors.centerIn: parent
                        text: modelData.profile !== "" ? modelData.profile : qsTr("Default profile")
                        color: modelData.profile !== "" ? T.accentText : "#d3d8e1"
                        font.pixelSize: 13
                    }
                }
            }
        }
        CdButtonRow {
            CdButton {
                text: qsTr("Edit profiles...")
                onClicked: cdView.openControllerProfiles()
            }
        }
    }
}
