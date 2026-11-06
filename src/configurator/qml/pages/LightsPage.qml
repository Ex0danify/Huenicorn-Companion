import QtQuick
import "../theme"
import "../components"

Flickable {
    id: page

    contentHeight: column.height + 8
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    function gammaText(v) {
        return (v > 0 ? "+" : "") + v.toLocaleString(Qt.locale(), "f", 1)
    }

    function averageGamma() {
        const list = backend.channels
        if (list.length === 0)
            return 0
        let sum = 0
        for (const channel of list)
            sum += channel.gamma
        return sum / list.length
    }

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

        Card {
            objectName: "globalGammaCard"
            visible: backend.loaded && backend.channels.length > 0
            width: parent.width
            height: 88

            Column {
                x: 20; y: 14
                width: parent.width - 40
                spacing: 6

                Row {
                    width: parent.width
                    Text {
                        width: parent.width - globalGammaValue.width
                        text: qsTr("Gamma for all lights")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }
                    Text {
                        id: globalGammaValue
                        text: page.gammaText(globalGamma.displayValue)
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                    }
                }

                HueSlider {
                    id: globalGamma
                    objectName: "globalGamma"
                    width: parent.width
                    from: -1
                    to: 1
                    stepSize: 0.1
                    value: page.averageGamma()
                    onCommitted: (v) => backend.setAllChannelsGamma(v)
                }
            }
        }

        Repeater {
            model: backend.channels

            delegate: Card {
                id: card
                required property var modelData
                width: column.width
                height: 76 + 62

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

                Column {
                    anchors { top: header.bottom; left: parent.left; right: parent.right; leftMargin: 20; rightMargin: 20 }
                    spacing: 4

                    Row {
                        width: parent.width
                        Text {
                            width: parent.width - gammaValue.width
                            text: qsTr("Gamma")
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSmall + 1
                        }
                        Text {
                            id: gammaValue
                            text: page.gammaText(gammaSlider.displayValue)
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSmall + 1
                        }
                    }

                    HueSlider {
                        id: gammaSlider
                        objectName: "gammaSlider"
                        width: parent.width
                        from: -1
                        to: 1
                        stepSize: 0.1
                        value: card.modelData.gamma
                        onCommitted: (v) => backend.setChannelGamma(card.modelData.channelId, v)
                    }
                }
            }
        }
    }
}
