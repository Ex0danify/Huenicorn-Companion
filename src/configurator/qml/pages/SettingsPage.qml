import QtQuick
import "../theme"
import "../components"

Flickable {
    id: page

    contentHeight: column.height + 8
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: column
        x: Theme.gutter
        width: page.width - 2 * Theme.gutter
        spacing: 24

        Text { text: qsTr("Settings"); color: Theme.text; font.pixelSize: Theme.fontTitle; font.bold: true }

        EmptyState {
            width: parent.width
            visible: !backend.loaded
        }

        Column {
            visible: backend.loaded
            width: parent.width
            spacing: 24

            Column {
                width: parent.width
                spacing: 12

                Text { text: qsTr("Capture resolution"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }
                Text {
                    width: parent.width
                    text: qsTr("Lower values use less CPU. Higher values follow fine detail better.")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSmall + 1
                    wrapMode: Text.WordWrap
                }

                Flow {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: backend.subsampleCandidates
                        delegate: Rectangle {
                            id: chip
                            required property var modelData
                            readonly property bool selected: modelData.width === backend.subsampleWidth
                            width: chipText.implicitWidth + 28
                            height: 38
                            radius: 19
                            color: selected ? "white" : (chipArea.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
                            Behavior on color { ColorAnimation { duration: 120 } }

                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: chip.modelData.label + "  " + chip.modelData.percent + "%"
                                color: chip.selected ? "#14181d" : Theme.textDim
                                font.pixelSize: Theme.fontSmall + 1
                                font.bold: chip.selected
                            }
                            MouseArea {
                                id: chipArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.setSubsampleWidth(chip.modelData.width)
                            }
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 12

                Text { text: qsTr("Interpolation"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }

                Segmented {
                    width: parent.width
                    implicitHeight: 52
                    height: 52
                    model: backend.interpolations.map(function (i) { return { label: i.name, value: i.value } })
                    current: backend.interpolation
                    onPicked: (value) => backend.setInterpolation(value)
                }
            }

            Column {
                width: parent.width
                spacing: 8

                Row {
                    width: parent.width
                    Text {
                        width: parent.width - smoothValue.width
                        text: qsTr("Transition smoothing")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }
                    Text {
                        id: smoothValue
                        text: Math.round(smooth.displayValue) + "%"
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                    }
                }

                HueSlider {
                    id: smooth
                    width: parent.width
                    from: 0
                    to: 95
                    stepSize: 1
                    value: backend.smoothing
                    onCommitted: (v) => backend.setSmoothing(v)
                }
            }
        }

        Card {
            width: parent.width
            height: info.height + 40

            Column {
                id: info
                x: 22; y: 20
                width: parent.width - 44
                spacing: 12

                Text { text: qsTr("Daemon"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }

                Repeater {
                    model: [
                        { k: qsTr("Version"), v: backend.version || "-" },
                        { k: qsTr("Binary"), v: backend.daemonPath || "-" },
                        { k: qsTr("REST port"), v: String(backend.port) }
                    ]
                    delegate: Row {
                        required property var modelData
                        width: info.width
                        spacing: 12
                        Text { width: 80; text: modelData.k; color: Theme.textDim; font.pixelSize: Theme.fontSmall + 1 }
                        Text {
                            width: parent.width - 92
                            text: modelData.v
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall + 1
                            elide: Text.ElideMiddle
                        }
                    }
                }

                PillButton {
                    text: qsTr("Open Web-Configurator")
                    fill: Theme.surfaceHover
                    textColor: Theme.text
                    fontSize: Theme.fontBody
                    implicitHeight: 42
                    enabled: backend.running
                    onClicked: backend.openWebConfigurator()
                }
            }
        }

        UpdateCard {
            width: parent.width
        }
    }
}
