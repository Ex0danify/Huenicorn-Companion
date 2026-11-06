import QtQuick
import "../theme"

Item {
    id: root

    property string label
    property string icon
    property bool selected: false

    signal clicked()

    implicitWidth: 76
    implicitHeight: 92
    opacity: enabled ? 1 : 0.4
    Behavior on opacity { NumberAnimation { duration: 140 } }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8

        Rectangle {
            width: 62; height: 62; radius: 31
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.selected ? "white" : (area.containsMouse && root.enabled ? Theme.surfaceHover : Theme.surfaceRaised)
            Behavior on color { ColorAnimation { duration: 140 } }

            Icon {
                anchors.centerIn: parent
                kind: root.icon
                color: root.selected ? "#14181d" : Theme.textDim
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: root.selected ? "white" : Theme.textDim
            font.pixelSize: Theme.fontSmall + 1
            font.bold: root.selected
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
