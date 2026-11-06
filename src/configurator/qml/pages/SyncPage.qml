import QtQuick
import "../theme"
import "../components"

Item {
    id: page

    readonly property var smoothingPresets: [
        { label: qsTr("Off"), value: 0, amplitude: 0 },
        { label: qsTr("Subtle"), value: 15, amplitude: 0.3 },
        { label: qsTr("Moderate"), value: 40, amplitude: 0.8 },
        { label: qsTr("High"), value: 65, amplitude: 1.3 },
        { label: qsTr("Extreme"), value: 90, amplitude: 1.9 }
    ]

    function nearestPreset(percent) {
        var best = smoothingPresets[0].value
        for (var i = 0; i < smoothingPresets.length; i++) {
            if (Math.abs(smoothingPresets[i].value - percent) < Math.abs(best - percent))
                best = smoothingPresets[i].value
        }
        return best
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; bottomMargin: 12 }
        contentHeight: column.height + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            x: Theme.gutter
            width: flick.width - 2 * Theme.gutter
            spacing: 22

            EmptyState {
                width: parent.width
                visible: !backend.loaded
            }

            Card {
                id: areaCard
                visible: backend.loaded
                width: parent.width
                height: areaHeader.height + (expanded ? areaList.height + 12 : 0) + 36
                color: Theme.accent
                clip: true
                property bool expanded: false
                Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                Row {
                    id: areaHeader
                    x: 22; y: 18
                    width: parent.width - 44
                    height: 64
                    spacing: 16

                    Rectangle {
                        width: 54; height: 54; radius: 27
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(0, 0, 0, 0.12)
                        Icon { anchors.centerIn: parent; kind: "screen"; color: Theme.textOnAccent }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 54 - 16 - 36 - 16
                        spacing: 2
                        Text {
                            width: parent.width
                            text: backend.currentAreaName || qsTr("No entertainment area selected")
                            color: Theme.textOnAccent
                            font.pixelSize: 20
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: backend.activeChannelCount === 1 ? qsTr("1 light syncing") : qsTr("%1 lights syncing").arg(backend.activeChannelCount)
                            color: Qt.rgba(0.05, 0.1, 0.16, 0.7)
                            font.pixelSize: Theme.fontBody
                        }
                    }

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        kind: "chevron"
                        color: Theme.textOnAccent
                        rotation: areaCard.expanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: 160 } }
                    }
                }

                MouseArea {
                    x: 0; y: 0
                    width: parent.width
                    height: areaHeader.height + 36
                    enabled: backend.areas.length > 1
                    cursorShape: Qt.PointingHandCursor
                    onClicked: areaCard.expanded = !areaCard.expanded
                }

                Column {
                    id: areaList
                    visible: areaCard.expanded
                    x: 22
                    y: areaHeader.height + 28
                    width: parent.width - 44
                    spacing: 6

                    Repeater {
                        model: backend.areas
                        delegate: Rectangle {
                            id: areaRow
                            required property var modelData
                            readonly property bool current: modelData.id === backend.currentAreaId
                            width: areaList.width
                            height: 46
                            radius: 12
                            color: current ? Qt.rgba(0, 0, 0, 0.14) : (rowArea.containsMouse ? Qt.rgba(0, 0, 0, 0.07) : "transparent")

                            Text {
                                anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter; right: parent.right; rightMargin: 44 }
                                text: areaRow.modelData.name
                                color: Theme.textOnAccent
                                font.pixelSize: Theme.fontBody
                                font.bold: areaRow.current
                                elide: Text.ElideRight
                            }
                            Icon {
                                anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                                visible: areaRow.current
                                kind: "check"
                                color: Theme.textOnAccent
                            }
                            MouseArea {
                                id: rowArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { backend.selectArea(areaRow.modelData.id); areaCard.expanded = false }
                            }
                        }
                    }
                }
            }

            Column {
                visible: backend.loaded
                width: parent.width
                spacing: 12

                Text {
                    text: qsTr("Color transitions")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                }

                Segmented {
                    width: parent.width
                    model: page.smoothingPresets
                    current: page.nearestPreset(backend.smoothing)
                    onPicked: (value) => backend.setSmoothing(value)
                }
            }

            Column {
                visible: backend.loaded
                width: parent.width
                spacing: 8

                Row {
                    width: parent.width
                    Text {
                        width: parent.width - hz.width
                        text: qsTr("Refresh rate")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }
                    Text {
                        id: hz
                        text: qsTr("%1 Hz").arg(Math.round(rate.displayValue))
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                    }
                }

                HueSlider {
                    id: rate
                    width: parent.width
                    from: 1
                    to: Math.max(2, backend.maxRefreshRate)
                    stepSize: 1
                    value: backend.refreshRate
                    onCommitted: (v) => backend.setRefreshRate(v)
                }
            }
        }
    }

    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 56 + 8

        PillButton {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 2 * Theme.gutter, 300)
            objectName: "syncButton"
            // Nothing to start without a binary, only an external daemon can show up
            readonly property bool reconnectOnly: !backend.running && !backend.busy && !backend.canStart
            enabled: !backend.busy
            text: backend.running ? qsTr("Stop light sync")
                : backend.daemonState === "starting" ? qsTr("Starting...")
                : backend.daemonState === "stopping" ? qsTr("Stopping...")
                : reconnectOnly ? qsTr("Reconnect")
                : qsTr("Start light sync")
            fill: backend.running ? Theme.danger : (reconnectOnly ? Theme.surfaceHover : Theme.success)
            textColor: backend.running ? "#2a0808" : (reconnectOnly ? Theme.text : "#10200a")
            onClicked: reconnectOnly ? backend.reconnect() : backend.toggleDaemon()
        }
    }
}
