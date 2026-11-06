import QtQuick
import "../theme"
import "../components"

Flickable {
    id: page

    property int selectedId: -1

    function channelById(id) {
        for (var i = 0; i < backend.channels.length; i++) {
            if (backend.channels[i].channelId === id)
                return backend.channels[i]
        }
        return null
    }

    readonly property var selected: channelById(selectedId)

    contentHeight: column.height + 8
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: column
        x: Theme.gutter
        width: page.width - 2 * Theme.gutter
        spacing: 18

        Column {
            width: parent.width
            spacing: 4
            Text { text: qsTr("Screen layout"); color: Theme.text; font.pixelSize: Theme.fontTitle; font.bold: true }
            Text {
                width: parent.width
                text: qsTr("Drag a light to choose which part of the screen it follows.")
                color: Theme.textDim
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }
        }

        EmptyState {
            width: parent.width
            visible: !backend.loaded
        }

        // Stylized monitor, there is no live frame to show
        Rectangle {
            id: screen
            visible: backend.loaded
            width: parent.width
            height: Math.min(width * backend.displayHeight / Math.max(1, backend.displayWidth), 380)
            radius: 14
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.12)
            gradient: Gradient {
                GradientStop { position: 0; color: "#1f2f44" }
                GradientStop { position: 1; color: "#0e1520" }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: page.selectedId = -1
            }

            Repeater {
                model: backend.channels
                delegate: ChannelRect {
                    required property var modelData
                    channel: modelData
                    screenWidth: screen.width
                    screenHeight: screen.height
                    selected: modelData.channelId === page.selectedId
                    onPicked: page.selectedId = modelData.channelId
                    onCommitted: (x0, y0, x1, y1) => backend.setChannelRect(modelData.channelId, x0, y0, x1, y1)
                }
            }
        }

        Card {
            visible: backend.loaded
            width: parent.width
            height: page.selected ? detail.height + 40 : 76

            Text {
                visible: !page.selected
                anchors.centerIn: parent
                text: qsTr("Select a light on the screen to adjust it")
                color: Theme.textDim
                font.pixelSize: Theme.fontBody
            }

            Column {
                id: detail
                visible: page.selected !== null
                x: 22; y: 20
                width: parent.width - 44
                spacing: 18

                Row {
                    width: parent.width
                    spacing: 12

                    Rectangle {
                        width: 14; height: 14; radius: 7
                        anchors.verticalCenter: parent.verticalCenter
                        color: page.selected ? page.selected.color : "transparent"
                    }
                    Column {
                        width: parent.width - 14 - 58 - 24
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            width: parent.width
                            text: page.selected ? page.selected.name : ""
                            color: Theme.text
                            font.pixelSize: 17
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: page.selected ? qsTr("Channel %1").arg(page.selected.channelId) : ""
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSmall + 1
                        }
                    }
                    Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.selected ? page.selected.active : false
                        onToggled: (on) => backend.setChannelActive(page.selected.channelId, on)
                    }
                }
            }
        }
    }
}
