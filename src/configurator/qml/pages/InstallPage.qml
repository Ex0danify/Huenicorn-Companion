import QtQuick
import "../theme"
import "../components"

// Offered while no daemon binary can be found
Item {
    id: page

    readonly property string phase: installer.phase

    function recheck() {
        if (phase !== "building" && phase !== "installed")
            installer.refresh()
    }

    Component.onCompleted: if (visible) recheck()
    onVisibleChanged: if (visible) recheck()

    Flickable {
        anchors.fill: parent
        contentHeight: column.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            x: Theme.gutter
            width: parent.width - 2 * Theme.gutter
            spacing: 18

            Card {
                width: parent.width
                height: content.height + 44

                Column {
                    id: content
                    x: 22; y: 22
                    width: parent.width - 44
                    spacing: 14

                    Text {
                        width: parent.width
                        text: qsTr("Huenicorn is not installed")
                        color: Theme.text
                        font.pixelSize: 20
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("The Huenicorn daemon does the actual light sync. This app can build version %1 from source and install it for your user.").arg(installer.version)
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }

                    // Something needed for the build is missing
                    Column {
                        width: parent.width
                        spacing: 10
                        visible: page.phase === "missing"

                        Text {
                            width: parent.width
                            text: qsTr("Needed to build it (%1 session)").arg(installer.session)
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: installer.requirements
                            delegate: Row {
                                required property var modelData
                                spacing: 10
                                Text {
                                    width: 18
                                    text: modelData.met ? "✓" : "✗"
                                    color: modelData.met ? Theme.success : Theme.danger
                                    font.pixelSize: Theme.fontBody
                                }
                                Text {
                                    text: modelData.label
                                    color: modelData.met ? Theme.textDim : Theme.text
                                    font.pixelSize: Theme.fontBody
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            text: installer.installHint !== ""
                                ? qsTr("Install the missing items yourself, then check again:")
                                : qsTr("Install the missing items with your package manager, then check again.")
                            color: Theme.textDim
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            width: parent.width
                            height: hint.implicitHeight + 24
                            visible: installer.installHint !== ""
                            radius: 12
                            color: Theme.surfaceRaised
                            border.width: 1
                            border.color: Theme.border

                            TextEdit {
                                id: hint
                                x: 12; y: 12
                                width: parent.width - 24
                                readOnly: true
                                selectByMouse: true
                                text: installer.installHint
                                color: Theme.text
                                font.family: "monospace"
                                font.pixelSize: Theme.fontSmall + 1
                                wrapMode: TextEdit.WrapAnywhere
                            }
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            PillButton {
                                visible: installer.installHint !== ""
                                text: qsTr("Copy command")
                                fill: Theme.surfaceHover
                                textColor: Theme.text
                                fontSize: Theme.fontBody
                                implicitHeight: 42
                                onClicked: { hint.selectAll(); hint.copy(); hint.deselect() }
                            }
                            PillButton {
                                text: qsTr("Check again")
                                fontSize: Theme.fontBody
                                implicitHeight: 42
                                onClicked: installer.refresh()
                            }
                        }
                    }

                    // Everything is there, ask before running anything
                    Column {
                        width: parent.width
                        spacing: 10
                        visible: page.phase === "ready" || page.phase === "failed"

                        Text {
                            width: parent.width
                            visible: page.phase === "failed"
                            text: installer.errorText
                            color: Theme.danger
                            font.pixelSize: Theme.fontSmall + 1
                            wrapMode: Text.WrapAnywhere
                        }

                        Repeater {
                            model: [
                                { name: qsTr("Source"), value: installer.repoUrl },
                                { name: qsTr("Version"), value: "v" + installer.version + " (" + installer.commit.substring(0, 10) + ")" },
                                { name: qsTr("Install to"), value: installer.installPath }
                            ]
                            delegate: Row {
                                required property var modelData
                                width: column.width - 44
                                spacing: 12
                                Text { width: 80; text: modelData.name; color: Theme.textDim; font.pixelSize: Theme.fontSmall + 1 }
                                Text {
                                    width: parent.width - 92
                                    text: modelData.value
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSmall + 1
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            text: qsTr("This downloads the source, checks integrity and then compiles everything on this computer. It takes a few minutes.")
                            color: Theme.textDim
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.WordWrap
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            PillButton {
                                text: page.phase === "failed" ? qsTr("Try again") : qsTr("Build and install")
                                fontSize: Theme.fontBody
                                implicitHeight: 42
                                onClicked: installer.build()
                            }
                            PillButton {
                                visible: page.phase === "failed"
                                text: qsTr("Check again")
                                fill: Theme.surfaceHover
                                textColor: Theme.text
                                fontSize: Theme.fontBody
                                implicitHeight: 42
                                onClicked: installer.refresh()
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 12
                        visible: page.phase === "building"

                        Text {
                            width: parent.width
                            text: installer.step
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            width: parent.width
                            height: 6
                            radius: 3
                            color: Theme.trackOff

                            Rectangle {
                                width: parent.width * installer.progress
                                height: parent.height
                                radius: 3
                                color: Theme.accent
                            }
                        }

                        PillButton {
                            text: qsTr("Cancel")
                            fill: Theme.surfaceHover
                            textColor: Theme.text
                            fontSize: Theme.fontBody
                            implicitHeight: 42
                            onClicked: installer.cancel()
                        }
                    }

                    Text {
                        width: parent.width
                        visible: page.phase === "installed"
                        text: qsTr("Installed to %1.").arg(installer.installPath)
                        color: Theme.success
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WrapAnywhere
                    }

                    Text {
                        width: parent.width
                        visible: page.phase !== "building" && page.phase !== "installed"
                        text: qsTr("Already installed somewhere else? Pick it in the tray menu under Settings, Choose daemon binary.")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall + 1
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
