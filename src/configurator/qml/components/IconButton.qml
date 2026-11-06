import QtQuick
import "../theme"

Item {
    id: root

    property string icon
    property bool active: false

    signal clicked()

    implicitWidth: 40
    implicitHeight: 40

    Icon {
        anchors.centerIn: parent
        kind: root.icon
        color: root.active || area.containsMouse ? "white" : Theme.textDim
        Behavior on color { ColorAnimation { duration: 140 } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
