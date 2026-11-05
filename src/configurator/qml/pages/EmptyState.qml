import QtQuick
import "../theme"
import "../components"

// Shown on every page while the daemon is not available
Card {
    id: root

    property string title: qsTr("Light sync is not running")
    property string hint: backend.statusText

    height: 140

    Column {
        anchors.centerIn: parent
        width: parent.width - 48
        spacing: 8

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.title
            color: Theme.text
            font.pixelSize: 18
            font.bold: true
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.hint
            color: backend.daemonState === "error" ? Theme.danger : Theme.textDim
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
    }
}
