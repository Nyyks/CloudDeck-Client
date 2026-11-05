// CloudDeck-Client: controller remapping profiles editor (Phase 6, docs/controller-profiles.md).
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import CloudDeckControllers 1.0

Dialog {
    id: dlg
    title: qsTr("Controller profiles")
    modal: true
    standardButtons: Dialog.Close
    width: Math.min(parent ? parent.width - 40 : 640, 640)
    height: Math.min(parent ? parent.height - 40 : 720, 720)
    anchors.centerIn: parent

    property string editing: ""      // name of the profile being edited ("" = new)
    property var profileData: ({})
    property string error: ""

    function load(name) {
        editing = name
        profileData = CloudDeckControllers.profile(name)
        error = ""
    }
    function save() {
        error = CloudDeckControllers.saveProfile(editing, profileData)
        if (error === "") editing = profileData.name
    }
    function setField(k, v) { var d = profileData; d[k] = v; profileData = d }
    function setButton(k, v) { var d = profileData; d.buttons[k] = v; profileData = d }

    onOpened: {
        CloudDeckControllers.refreshControllers()
        if (CloudDeckControllers.profiles.length > 0) load(CloudDeckControllers.profiles[0])
        else load(CloudDeckControllers.newProfileName())
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 6

            // ---- which profile
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: picker
                    Layout.fillWidth: true
                    model: CloudDeckControllers.profiles
                    currentIndex: CloudDeckControllers.profiles.indexOf(dlg.editing)
                    onActivated: dlg.load(currentText)
                }
                Button { text: qsTr("New"); onClicked: { dlg.editing = ""; dlg.profileData = CloudDeckControllers.profile(CloudDeckControllers.newProfileName()) } }
                Button {
                    text: qsTr("Delete")
                    enabled: CloudDeckControllers.profiles.indexOf(dlg.editing) >= 0
                    onClicked: {
                        CloudDeckControllers.deleteProfile(dlg.editing)
                        if (CloudDeckControllers.profiles.length > 0) dlg.load(CloudDeckControllers.profiles[0])
                        else dlg.load(CloudDeckControllers.newProfileName())
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Name:") }
                TextField {
                    Layout.fillWidth: true
                    text: dlg.profileData.name || ""
                    onEditingFinished: dlg.setField("name", text)
                }
            }

            // ---- buttons
            Label { text: qsTr("Buttons"); font.bold: true; Layout.topMargin: 8 }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.7
                text: qsTr("Each button on the left sends the button on the right. The Xbox/Nintendo layout switch in the stream menu applies before this.")
            }
            Repeater {
                model: CloudDeckControllers.buttonNames
                delegate: RowLayout {
                    Layout.fillWidth: true
                    Label { text: CloudDeckControllers.buttonLabel(modelData); Layout.fillWidth: true }
                    ComboBox {
                        Layout.preferredWidth: 240
                        property var targets: CloudDeckControllers.buttonNames.concat(["none"])
                        model: targets.map(function(t) { return CloudDeckControllers.buttonLabel(t) })
                        currentIndex: dlg.profileData.buttons ? targets.indexOf(dlg.profileData.buttons[modelData]) : 0
                        onActivated: dlg.setButton(modelData, targets[index])
                    }
                }
            }

            // ---- sticks and triggers
            Label { text: qsTr("Sticks and triggers"); font.bold: true; Layout.topMargin: 8 }
            Repeater {
                model: [{key: "left", label: qsTr("Left stick")}, {key: "right", label: qsTr("Right stick")}]
                delegate: ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: modelData.label }
                    RowLayout {
                        Label { text: qsTr("Deadzone %1%").arg(Math.round((dlg.profileData[modelData.key + "Deadzone"] || 0) * 100)); Layout.preferredWidth: 140 }
                        Slider {
                            Layout.fillWidth: true; from: 0; to: 0.5; stepSize: 0.01
                            value: dlg.profileData[modelData.key + "Deadzone"] || 0
                            onMoved: dlg.setField(modelData.key + "Deadzone", value)
                        }
                    }
                    RowLayout {
                        Label { text: qsTr("Sensitivity %1%").arg(Math.round((dlg.profileData[modelData.key + "Sensitivity"] || 1) * 100)); Layout.preferredWidth: 140 }
                        Slider {
                            Layout.fillWidth: true; from: 0.5; to: 2.0; stepSize: 0.05
                            value: dlg.profileData[modelData.key + "Sensitivity"] || 1
                            onMoved: dlg.setField(modelData.key + "Sensitivity", value)
                        }
                    }
                    CheckBox {
                        text: qsTr("Invert up/down")
                        checked: !!dlg.profileData[modelData.key + "InvertY"]
                        onToggled: dlg.setField(modelData.key + "InvertY", checked)
                    }
                }
            }
            RowLayout {
                Label { text: qsTr("Trigger deadzone %1%").arg(Math.round((dlg.profileData.triggerDeadzone || 0) * 100)); Layout.preferredWidth: 140 }
                Slider {
                    Layout.fillWidth: true; from: 0; to: 0.5; stepSize: 0.01
                    value: dlg.profileData.triggerDeadzone || 0
                    onMoved: dlg.setField("triggerDeadzone", value)
                }
            }
            CheckBox {
                text: qsTr("Swap left and right stick")
                checked: !!dlg.profileData.swapSticks
                onToggled: dlg.setField("swapSticks", checked)
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Label { text: dlg.error; color: "#ffd43b"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Button { text: qsTr("Save profile"); highlighted: true; onClicked: dlg.save() }
            }

            // ---- assignment
            Label { text: qsTr("Which controllers use which profile"); font.bold: true; Layout.topMargin: 12 }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("All controllers (default):"); Layout.fillWidth: true }
                ComboBox {
                    Layout.preferredWidth: 240
                    model: [qsTr("No remapping")].concat(CloudDeckControllers.profiles)
                    currentIndex: CloudDeckControllers.profiles.indexOf(CloudDeckControllers.defaultProfile) + 1
                    onActivated: CloudDeckControllers.defaultProfile = index === 0 ? "" : CloudDeckControllers.profiles[index - 1]
                }
            }
            Repeater {
                model: CloudDeckControllers.controllers
                delegate: RowLayout {
                    Layout.fillWidth: true
                    Label { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                    ComboBox {
                        Layout.preferredWidth: 240
                        model: [qsTr("Use the default")].concat(CloudDeckControllers.profiles)
                        currentIndex: CloudDeckControllers.profiles.indexOf(modelData.profile) + 1
                        onActivated: CloudDeckControllers.setControllerProfile(modelData.name, index === 0 ? "" : CloudDeckControllers.profiles[index - 1])
                    }
                }
            }
            Label {
                visible: CloudDeckControllers.controllers.length === 0
                opacity: 0.7
                text: qsTr("Connect a controller to give it its own profile.")
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.7
                text: qsTr("Profiles apply from the next stream (or when a controller is connected during one).")
            }
        }
    }
}
