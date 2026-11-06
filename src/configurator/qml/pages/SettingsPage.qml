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

        Column {
            width: parent.width
            spacing: 12
            // The open list has to paint over the sections below
            z: languageDropdown.open ? 10 : 0

            Text { text: qsTr("Language"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }

            Dropdown {
                id: languageDropdown
                objectName: "languageDropdown"
                width: parent.width
                model: [{ label: qsTr("System"), value: "auto", icon: "globe" }].concat(
                    localization.languages.map(function (l) { return { label: l.name, value: l.code, flag: l.flag } }))
                current: localization.language
                onPicked: (value) => localization.language = value
            }
        }

        // Same layout with or without a daemon, the skeletons give way to the real controls
        Card {
            width: parent.width
            height: daemonSettings.height + 44

            Column {
                id: daemonSettings
                x: 22; y: 22
                width: parent.width - 44
                spacing: 24
                readonly property bool ready: backend.loaded
                // Values stay on screen, greyed out, once the daemon reported them
                readonly property bool showValues: backend.hasLoaded

                Column {
                    objectName: "daemonSettingsHint"
                    width: parent.width
                    spacing: 12
                    visible: !daemonSettings.ready

                    Text {
                        width: parent.width
                        text: qsTr("Available while light sync is running.")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall + 1
                        font.italic: true
                        wrapMode: Text.WordWrap
                    }
                    Rectangle { width: parent.width; height: 1; color: Theme.border }
                }

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
                        objectName: "resolutionChips"
                        width: parent.width
                        spacing: 8
                        enabled: daemonSettings.ready
                        opacity: enabled || !daemonSettings.showValues ? 1 : 0.4

                        Repeater {
                            model: daemonSettings.showValues ? 0 : 3
                            delegate: Rectangle {
                                width: 92; height: 38; radius: 19
                                color: Theme.surfaceRaised
                                opacity: 0.5
                                Text { anchors.centerIn: parent; text: "—"; color: Theme.textDim; font.pixelSize: Theme.fontSmall + 1 }
                            }
                        }

                        Repeater {
                            model: backend.subsampleCandidates
                            delegate: Rectangle {
                                id: chip
                                required property var modelData
                                readonly property bool selected: modelData.width === backend.subsampleWidth
                                width: chipText.implicitWidth + 28
                                height: 38
                                radius: 19
                                color: selected ? "white" : (chipArea.containsMouse && chip.enabled ? Theme.surfaceHover : Theme.surfaceRaised)
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: qsTr("%1  %2%").arg(chip.modelData.label).arg(chip.modelData.percent)
                                    color: chip.selected ? "#14181d" : Theme.textDim
                                    font.pixelSize: Theme.fontSmall + 1
                                    font.bold: chip.selected
                                }
                                MouseArea {
                                    id: chipArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: chip.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
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

                    Row {
                        id: segmentSkeleton
                        width: parent.width
                        spacing: 8
                        visible: !daemonSettings.showValues
                        opacity: 0.5

                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                width: (segmentSkeleton.width - 2 * segmentSkeleton.spacing) / 3
                                height: 52
                                radius: Theme.radiusTile
                                color: Theme.surfaceRaised
                                Text { anchors.centerIn: parent; text: "—"; color: Theme.textDim; font.pixelSize: Theme.fontBody }
                            }
                        }
                    }

                    Segmented {
                        objectName: "interpolationTiles"
                        visible: daemonSettings.showValues
                        enabled: daemonSettings.ready
                        opacity: enabled ? 1 : 0.4
                        tileColor: Theme.surfaceRaised
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
                            visible: daemonSettings.showValues
                            opacity: daemonSettings.ready ? 1 : 0.4
                            text: qsTr("%1%").arg(Math.round(smooth.displayValue))
                            color: Theme.textDim
                            font.pixelSize: Theme.fontBody
                        }
                    }

                    Item {
                        width: parent.width
                        height: smooth.implicitHeight
                        visible: !daemonSettings.showValues
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 14
                            radius: 7
                            opacity: 0.35
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0; color: Theme.accentDeep }
                                GradientStop { position: 1; color: "#e8f1fa" }
                            }
                        }
                    }

                    HueSlider {
                        id: smooth
                        objectName: "smoothingSlider"
                        visible: daemonSettings.showValues
                        enabled: daemonSettings.ready
                        width: parent.width
                        from: 0
                        to: 95
                        stepSize: 1
                        value: backend.smoothing
                        onCommitted: (v) => backend.setSmoothing(v)
                    }
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

                Flow {
                    width: parent.width
                    spacing: 8

                    PillButton {
                        objectName: "changeBinaryButton"
                        text: qsTr("Change binary...")
                        fill: Theme.surfaceHover
                        textColor: Theme.text
                        fontSize: Theme.fontBody
                        implicitHeight: 42
                        enabled: backend.canChangeBinary
                        onClicked: backend.chooseDaemon()
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

                Text {
                    width: parent.width
                    visible: !backend.canChangeBinary
                    text: qsTr("Stop light sync to change the binary. The new one is used the next time it starts.")
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSmall + 1
                    wrapMode: Text.WordWrap
                }
            }
        }

        Card {
            width: parent.width
            height: startup.height + 40

            Column {
                id: startup
                x: 22; y: 20
                width: parent.width - 44
                spacing: 14

                Text { text: qsTr("Startup"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }

                Repeater {
                    model: [
                        { objectName: "autostartDaemonToggle", label: qsTr("Start light sync on app start"), key: "autostartDaemon" },
                        { objectName: "launchAtLoginToggle", label: qsTr("Launch at login"), key: "launchAtLogin" },
                        { objectName: "stopOnQuitToggle", label: qsTr("Stop daemon when quitting"), key: "stopDaemonOnQuit" }
                    ]
                    delegate: Row {
                        id: toggleRow
                        required property var modelData
                        width: startup.width
                        spacing: 12

                        Text {
                            width: parent.width - 58 - 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: toggleRow.modelData.label
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall + 1
                            wrapMode: Text.WordWrap
                        }

                        Toggle {
                            objectName: toggleRow.modelData.objectName
                            checked: preferences[toggleRow.modelData.key]
                            onToggled: (value) => preferences[toggleRow.modelData.key] = value
                        }
                    }
                }
            }
        }

        UpdateCard {
            width: parent.width
        }
    }
}
