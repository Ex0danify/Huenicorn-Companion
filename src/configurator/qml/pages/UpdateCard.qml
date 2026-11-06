import QtQuick
import "../theme"
import "../components"

Card {
    id: card

    readonly property string phase: updater.phase
    readonly property bool hasUpdate: phase === "available"

    height: content.height + 40

    Column {
        id: content
        x: 22; y: 20
        width: parent.width - 44
        spacing: 12

        Text { text: qsTr("Updates"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }

        Row {
            width: parent.width
            spacing: 12
            Text { width: 80; text: qsTr("Installed"); color: Theme.textDim; font.pixelSize: Theme.fontSmall + 1 }
            Text { text: updater.currentVersion; color: Theme.text; font.pixelSize: Theme.fontSmall + 1 }
        }

        Text {
            width: parent.width
            visible: text.length > 0
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSmall + 1
            color: card.phase === "failed" ? Theme.danger : (card.hasUpdate ? Theme.accent : Theme.textDim)
            text: {
                switch (card.phase) {
                case "checking": return qsTr("Checking for updates...")
                case "upToDate": return qsTr("You are up to date.")
                case "available": return updater.canInstall
                    ? qsTr("Version %1 is available.").arg(updater.latestVersion)
                    : qsTr("Version %1 is available, but there is no installable package for this system.").arg(updater.latestVersion)
                case "downloading": return qsTr("Downloading version %1...").arg(updater.latestVersion)
                case "installing": return qsTr("Installing... confirm the password prompt if one appears.")
                case "installed": return qsTr("Version %1 is installed. Restart the app to use it.").arg(updater.latestVersion)
                case "failed": return updater.errorText
                default: return ""
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 6
            radius: 3
            color: Theme.trackOff
            visible: card.phase === "downloading"

            Rectangle {
                width: parent.width * updater.progress
                height: parent.height
                radius: 3
                color: Theme.accent
            }
        }

        Flickable {
            width: parent.width
            height: Math.min(notes.implicitHeight, 140)
            visible: card.hasUpdate && updater.releaseNotes.length > 0
            contentHeight: notes.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Text {
                id: notes
                width: parent.width
                text: updater.releaseNotes
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall + 1
            }
        }

        Flow {
            width: parent.width
            spacing: 8

            PillButton {
                visible: card.hasUpdate && updater.canInstall
                text: qsTr("Install")
                fontSize: Theme.fontBody
                implicitHeight: 42
                onClicked: updater.install()
            }

            PillButton {
                visible: card.hasUpdate && !updater.canInstall
                text: qsTr("Open release page")
                fill: Theme.surfaceHover
                textColor: Theme.text
                fontSize: Theme.fontBody
                implicitHeight: 42
                onClicked: Qt.openUrlExternally(updater.releaseUrl)
            }

            PillButton {
                visible: card.hasUpdate
                text: qsTr("Skip this version")
                fill: Theme.surfaceHover
                textColor: Theme.text
                fontSize: Theme.fontBody
                implicitHeight: 42
                onClicked: updater.skipVersion()
            }

            PillButton {
                visible: card.phase === "installed"
                text: qsTr("Restart now")
                fontSize: Theme.fontBody
                implicitHeight: 42
                onClicked: updater.restart()
            }

            PillButton {
                visible: card.phase !== "installed"
                text: qsTr("Check for updates")
                fill: Theme.surfaceHover
                textColor: Theme.text
                fontSize: Theme.fontBody
                implicitHeight: 42
                enabled: !updater.busy
                onClicked: updater.check()
            }
        }

        Row {
            width: parent.width
            spacing: 12

            Text {
                width: parent.width - 58 - 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Check for updates at startup")
                color: Theme.text
                font.pixelSize: Theme.fontSmall + 1
            }

            Toggle {
                checked: updater.autoCheck
                onToggled: (value) => updater.autoCheck = value
            }
        }
    }
}
