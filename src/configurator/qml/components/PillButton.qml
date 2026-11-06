import QtQuick
import "../theme"

Rectangle {
    id: root

    property string text
    property color fill: Theme.success
    property color textColor: "#10200a"
    property int fontSize: 17
    property string icon

    signal clicked()

    implicitWidth: content.width + 64
    implicitHeight: 56
    radius: height / 2
    opacity: enabled ? 1 : 0.45
    color: !enabled ? fill : (area.pressed ? Qt.darker(fill, 1.15) : (area.containsMouse ? Qt.lighter(fill, 1.08) : fill))
    Behavior on color { ColorAnimation { duration: 120 } }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 8

        Icon {
            visible: root.icon.length > 0
            anchors.verticalCenter: parent.verticalCenter
            width: 22; height: 22
            kind: root.icon
            color: root.textColor
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.textColor
            font.bold: true
            font.pixelSize: root.fontSize
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
