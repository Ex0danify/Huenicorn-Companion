import QtQuick
import "../theme"

// Controlled switch: the owner binds `checked` and reacts to `toggled`
Item {
    id: root

    property bool checked: false
    property color onColor: Theme.accent

    signal toggled(bool checked)

    implicitWidth: 58
    implicitHeight: 34
    opacity: enabled ? 1 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? root.onColor : Theme.trackOff
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    Rectangle {
        width: 28; height: 28; radius: 14
        y: 3
        x: root.checked ? root.width - width - 3 : 3
        color: "white"
        Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
