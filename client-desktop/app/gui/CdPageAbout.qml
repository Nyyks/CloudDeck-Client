// About: version, update check, project link, donation link, license and credits.
import QtQuick 2.9
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import SystemProperties 1.0
import "CdTheme.js" as T

ColumnLayout {
    id: page
    spacing: 16

    readonly property string repoUrl: "https://github.com/Nyyks/CloudDeck-Client"
    readonly property string releasesApi: "https://api.github.com/repos/Nyyks/CloudDeck-Client/releases/latest"
    readonly property string coffeeUrl: "https://buymeacoffee.com/nyyks"
    property string updateText: ""
    property string updateUrl: ""
    property bool checking: false

    // "1.2.3" > "1.2.0"? Pre-release suffixes are ignored for the comparison.
    function newer(a, b) {
        var x = a.replace(/^v/, "").split("-")[0].split("."), y = b.replace(/^v/, "").split("-")[0].split(".")
        for (var i = 0; i < 3; i++) {
            var p = parseInt(x[i] || "0"), q = parseInt(y[i] || "0")
            if (p !== q) return p > q
        }
        return false
    }
    function checkForUpdates() {
        checking = true
        updateText = qsTr("Checking...")
        updateUrl = ""
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE) return
            checking = false
            if (xhr.status === 200) {
                var r = JSON.parse(xhr.responseText)
                if (newer(r.tag_name, SystemProperties.versionString)) {
                    updateText = qsTr("Version %1 is available.").arg(r.tag_name.replace(/^v/, ""))
                    updateUrl = r.html_url
                } else {
                    updateText = qsTr("You have the latest version.")
                }
            } else if (xhr.status === 404) {
                updateText = qsTr("Couldn't check: the project's releases aren't public.")
            } else {
                updateText = qsTr("Couldn't reach GitHub (%1).").arg(xhr.status || qsTr("no connection"))
            }
        }
        xhr.open("GET", releasesApi)
        xhr.setRequestHeader("Accept", "application/vnd.github+json")
        xhr.setRequestHeader("User-Agent", "CloudDeck-Client")  // GitHub's API rejects requests without one
        xhr.send()
    }

    CdCard {
        title: qsTr("CloudDeck Client")
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 4
            spacing: 14
            Image { source: "qrc:/res/clouddeck-glyph.svg"; Layout.preferredWidth: 40; Layout.preferredHeight: 40; sourceSize.width: 80; sourceSize.height: 80 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: qsTr("Version %1").arg(SystemProperties.versionString); color: T.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: T.muted
                    font.pixelSize: 13
                    text: qsTr("An unofficial client for your CloudDeck, built on Moonlight.")
                }
            }
        }
        CdButtonRow {
            CdButton {
                kind: "primary"
                text: page.checking ? qsTr("Checking...") : qsTr("Check for updates")
                enabled: !page.checking
                onClicked: page.checkForUpdates()
            }
            CdButton {
                visible: page.updateUrl !== ""
                text: qsTr("Download")
                onClicked: Qt.openUrlExternally(page.updateUrl)
            }
        }
        CdText { text: page.updateText }
    }

    CdCard {
        title: qsTr("Project")
        CdButtonRow {
            CdButton { text: qsTr("GitHub"); onClicked: Qt.openUrlExternally(page.repoUrl) }
            CdButton { text: qsTr("Buy me a coffee"); onClicked: Qt.openUrlExternally(page.coffeeUrl) }
        }
        CdText {
            text: qsTr("Free software under the GNU General Public License v3. Built on Moonlight (moonlight-qt, " +
                       "moonlight-common-c) and Sunshine. Not affiliated with CloudDeck, Moonlight or LizardByte.")
        }
        CdButtonRow {
            CdButton { kind: "ghost"; text: qsTr("License and notices"); onClicked: Qt.openUrlExternally(page.repoUrl + "/blob/main/THIRD_PARTY_NOTICES.md") }
        }
    }
}
