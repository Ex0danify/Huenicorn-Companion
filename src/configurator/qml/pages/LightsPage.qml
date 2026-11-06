import QtQuick
import "../theme"
import "../components"

Flickable {
    id: page

    function gammaText(v) {
        return qsTr("Gamma %1").arg((v > 0 ? "+" : "") + v.toLocaleString(Qt.locale(), "f", 1))
    }

    contentHeight: column.height + 8
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: column
        x: Theme.gutter
        width: page.width - 2 * Theme.gutter
        spacing: 12

        Column {
            width: parent.width
            spacing: 4
            Text { text: qsTr("Lights"); color: Theme.text; font.pixelSize: Theme.fontTitle; font.bold: true }
            Text {
                text: backend.loaded ? qsTr("%1 of %2 syncing").arg(backend.activeChannelCount).arg(backend.channels.length) : ""
                color: Theme.textDim
                font.pixelSize: Theme.fontBody
            }
        }

        EmptyState {
            width: parent.width
            visible: !backend.loaded
        }

        // Sized by count so a value update keeps the cards, and with them hover and drag state
        Repeater {
            model: backend.channels.length

            delegate: Card {
                id: card
                required property int index
                readonly property var modelData: backend.channels[index] || ({
                    channelId: -1, name: "", active: false, color: "#000000", gamma: 0
                })
                width: column.width
                height: 76 + 28 + 1

                Row {
                    id: header
                    height: 76
                    anchors { left: parent.left; right: parent.right; leftMargin: 20; rightMargin: 20 }
                    spacing: 14

                    Rectangle {
                        width: 40; height: 40; radius: 20
                        anchors.verticalCenter: parent.verticalCenter
                        color: card.modelData.active ? Qt.rgba(Qt.color(card.modelData.color).r, Qt.color(card.modelData.color).g, Qt.color(card.modelData.color).b, 0.25) : Theme.surfaceRaised
                        Icon {
                            anchors.centerIn: parent
                            kind: "bulb"
                            color: card.modelData.active ? card.modelData.color : Theme.textDim
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 40 - 58 - 28
                        Text {
                            width: parent.width
                            text: card.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontBody + 1
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: qsTr("Channel %1").arg(card.modelData.channelId)
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSmall + 1
                        }
                    }

                    Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: card.modelData.active
                        onToggled: (on) => backend.setChannelActive(card.modelData.channelId, on)
                    }
                }

                HueSlider {
                    id: gammaSlider
                    objectName: "gammaSlider"
                    anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 1 }
                    trackHeight: 28
                    knobSize: 22
                    barRadius: Theme.radiusCard - 1
                    from: -1
                    to: 1
                    stepSize: 0.1
                    resetValue: 0
                    value: card.modelData.gamma
                    popupText: page.gammaText(displayValue)
                    onCommitted: (v) => backend.setChannelGamma(card.modelData.channelId, v)
                }
            }
        }
    }
}
