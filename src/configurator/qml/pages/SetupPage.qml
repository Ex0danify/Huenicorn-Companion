import QtQuick
import "../theme"
import "../components"

// First-run wizard, shown while the daemon has no bridge configured
Item {
    id: page

    readonly property var setup: backend.setup
    property bool manualCredentials: false

    Connections {
        target: page.setup
        function onBridgesChanged() { addressBox.text = page.setup.bridgeAddress }
        function onStepChanged() { if (page.setup.step !== "pair") page.manualCredentials = false }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            x: Theme.gutter
            width: flick.width - 2 * Theme.gutter
            spacing: 18

            // Step 1: bridge address
            Card {
                visible: page.setup.step === "bridge"
                width: parent.width
                height: bridgeColumn.height + 44

                Column {
                    id: bridgeColumn
                    x: 22; y: 22
                    width: parent.width - 44
                    spacing: 14

                    Text {
                        width: parent.width
                        text: qsTr("Connect your Hue bridge")
                        color: Theme.text
                        font.pixelSize: 20
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        width: parent.width
                        text: qsTr("Huenicorn needs the address of your bridge. It looks for the bridge on your network first.")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        width: parent.width
                        visible: page.setup.discovering
                        text: qsTr("Looking for your bridge…")
                        color: Theme.accent
                        font.pixelSize: Theme.fontBody
                    }

                    // Only worth a list when the choice is not obvious
                    Repeater {
                        model: page.setup.bridges.length > 1 ? page.setup.bridges : []

                        delegate: Rectangle {
                            id: candidate
                            required property string modelData
                            width: bridgeColumn.width
                            height: 44
                            radius: Theme.radiusTile
                            color: addressBox.text === modelData ? Theme.surfaceHover : Theme.surfaceRaised

                            Text {
                                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 16 }
                                text: candidate.modelData
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: addressBox.text = candidate.modelData
                            }
                        }
                    }

                    TextBox {
                        id: addressBox
                        width: parent.width
                        placeholder: qsTr("Bridge address, e.g. 192.168.1.20")
                        onAccepted: page.setup.submitBridge(text)
                    }

                    Text {
                        width: parent.width
                        visible: page.setup.error.length > 0
                        text: page.setup.error
                        color: Theme.danger
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        width: parent.width
                        spacing: 10

                        PillButton {
                            text: qsTr("Search again")
                            fill: Theme.surfaceRaised
                            textColor: Theme.text
                            enabled: !page.setup.busy
                            onClicked: page.setup.discover()
                        }
                        PillButton {
                            text: qsTr("Next")
                            enabled: !page.setup.busy && addressBox.text.trim().length > 0
                            onClicked: page.setup.submitBridge(addressBox.text)
                        }
                    }
                }
            }

            // Step 2: pairing
            Card {
                visible: page.setup.step === "pair"
                width: parent.width
                height: pairColumn.height + 44

                Column {
                    id: pairColumn
                    x: 22; y: 22
                    width: parent.width - 44
                    spacing: 14

                    Text {
                        width: parent.width
                        text: qsTr("Press the pairing button")
                        color: Theme.text
                        font.pixelSize: 20
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        width: parent.width
                        text: qsTr("Press the pairing button on your bridge, then click Pair. Huenicorn keeps trying for about 30 seconds.")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        visible: !page.manualCredentials && page.setup.error.length > 0
                        text: page.setup.error
                        color: Theme.danger
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        width: parent.width
                        spacing: 10

                        PillButton {
                            text: qsTr("Back")
                            fill: Theme.surfaceRaised
                            textColor: Theme.text
                            onClicked: page.setup.back()
                        }
                        PillButton {
                            text: page.setup.pairing ? qsTr("Waiting for the button…") : qsTr("Pair")
                            enabled: !page.setup.pairing
                            onClicked: page.setup.pair()
                        }
                    }

                    Text {
                        text: page.manualCredentials ? qsTr("Hide credentials") : qsTr("I already have credentials")
                        color: Theme.accent
                        font.pixelSize: Theme.fontBody
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.manualCredentials = !page.manualCredentials
                        }
                    }

                    Column {
                        width: parent.width
                        visible: page.manualCredentials
                        spacing: 10

                        TextBox { id: userBox; width: parent.width; placeholder: qsTr("Username") }
                        TextBox {
                            id: keyBox
                            width: parent.width
                            placeholder: qsTr("Client key (32 hexadecimal characters)")
                            onAccepted: page.setup.submitCredentials(userBox.text, keyBox.text)
                        }
                        Text {
                            width: parent.width
                            visible: page.setup.error.length > 0
                            text: page.setup.error
                            color: Theme.danger
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.WordWrap
                        }
                        PillButton {
                            text: qsTr("Use these credentials")
                            enabled: !page.setup.busy
                            onClicked: page.setup.submitCredentials(userBox.text, keyBox.text)
                        }
                    }
                }
            }

            // Step 3: waiting for the daemon to switch over
            Card {
                visible: page.setup.step === "finishing"
                width: parent.width
                height: finishColumn.height + 44

                Column {
                    id: finishColumn
                    x: 22; y: 22
                    width: parent.width - 44
                    spacing: 14

                    Text {
                        width: parent.width
                        text: qsTr("Almost there")
                        color: Theme.text
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: page.setup.error.length > 0 ? page.setup.error : qsTr("Your bridge is paired. Starting the light sync…")
                        color: page.setup.error.length > 0 ? Theme.danger : Theme.textDim
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                    PillButton {
                        visible: page.setup.error.length > 0
                        text: qsTr("Try again")
                        onClicked: page.setup.retry()
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.setup.step !== "finishing"
                text: qsTr("Cancel setup")
                color: Theme.textDim
                font.pixelSize: Theme.fontBody
                MouseArea {
                    anchors { fill: parent; margins: -8 }
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.setup.cancel()
                }
            }
        }
    }
}
