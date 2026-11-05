import QtQuick
import "../theme"

// Row of selectable tiles. Entries: { label, value, amplitude? }
Item {
    id: root

    property var model: []
    property var current: null
    property int spacing: 8

    signal picked(var value)

    implicitHeight: 72

    Row {
        anchors.fill: parent
        spacing: root.spacing

        Repeater {
            model: root.model

            delegate: Rectangle {
                id: tile
                required property var modelData
                readonly property bool selected: modelData.value === root.current

                width: (root.width - (root.model.length - 1) * root.spacing) / Math.max(1, root.model.length)
                height: root.height
                radius: Theme.radiusTile
                color: selected ? Theme.surfaceHover : (area.containsMouse ? Theme.surfaceRaised : Theme.surface)
                Behavior on color { ColorAnimation { duration: 120 } }

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Icon {
                        visible: tile.modelData.amplitude !== undefined
                        anchors.horizontalCenter: parent.horizontalCenter
                        kind: "wave"
                        amplitude: tile.modelData.amplitude !== undefined ? tile.modelData.amplitude : 0
                        color: tile.selected ? Theme.text : Theme.textDim
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: tile.modelData.label
                        color: tile.selected ? Theme.text : Theme.textDim
                        font.pixelSize: Theme.fontBody
                        font.bold: tile.selected
                    }
                }

                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.picked(tile.modelData.value)
                }
            }
        }
    }
}
