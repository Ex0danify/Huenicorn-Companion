import QtQuick
import "../theme"

Rectangle {
    id: root

    property alias text: input.text
    property alias field: input
    property string placeholder

    signal accepted()

    implicitHeight: 52
    radius: Theme.radiusTile
    color: Theme.surfaceRaised
    border.width: 1
    border.color: input.activeFocus ? Theme.accent : Theme.border

    TextInput {
        id: input
        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
        verticalAlignment: TextInput.AlignVCenter
        color: Theme.text
        selectionColor: Theme.accentDeep
        font.pixelSize: Theme.fontBody
        selectByMouse: true
        clip: true
        onAccepted: root.accepted()
    }

    Text {
        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
        verticalAlignment: Text.AlignVCenter
        visible: input.text.length === 0
        text: root.placeholder
        color: Theme.textDim
        font.pixelSize: Theme.fontBody
        elide: Text.ElideRight
    }
}
