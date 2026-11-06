import QtQuick
import "../theme"

// Single choice list. Entries: { label, value, flag?, icon? }
Item {
    id: root

    property var model: []
    property var current: null
    property bool open: false
    // Longer lists scroll inside the dropdown
    property int maxVisible: 4
    readonly property int rowHeight: 46
    readonly property int visibleRows: Math.min(model.length, maxVisible)

    readonly property var currentEntry: {
        for (var i = 0; i < model.length; i++) {
            if (model[i].value === current) return model[i]
        }
        return model.length > 0 ? model[0] : null
    }

    signal picked(var value)

    implicitHeight: 52
    // Keeps the open list above whatever follows
    z: open ? 10 : 0

    Keys.onEscapePressed: root.open = false
    onVisibleChanged: if (!visible) open = false

    onOpenChanged: {
        if (!open) return
        // Bring the current entry into view
        for (var i = 0; i < model.length; i++) {
            if (model[i].value === current) {
                flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, (i - (visibleRows - 1) / 2) * rowHeight))
                return
            }
        }
        flick.contentY = 0
    }

    Rectangle {
        id: header
        width: parent.width
        height: root.implicitHeight
        radius: Theme.radiusTile
        color: headerArea.containsMouse || root.open ? Theme.surfaceHover : Theme.surfaceRaised
        border.width: 1
        border.color: root.open ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: 120 } }

        Row {
            anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
            spacing: 12

            Item {
                width: 28
                height: parent.height
                Text {
                    anchors.centerIn: parent
                    visible: !!root.currentEntry && !!root.currentEntry.flag
                    text: root.currentEntry && root.currentEntry.flag ? root.currentEntry.flag : ""
                    font.pixelSize: 22
                }
                Icon {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    visible: !!root.currentEntry && !root.currentEntry.flag && !!root.currentEntry.icon
                    kind: root.currentEntry && root.currentEntry.icon ? root.currentEntry.icon : "globe"
                    color: Theme.textDim
                }
            }

            Text {
                width: parent.width - 28 - 24 - 2 * parent.spacing
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                text: root.currentEntry ? root.currentEntry.label : ""
                color: Theme.text
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
            }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 24
                kind: "chevron"
                color: Theme.textDim
                rotation: root.open ? 180 : 0
                Behavior on rotation { NumberAnimation { duration: 120 } }
            }
        }

        MouseArea {
            id: headerArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.open = !root.open
                if (root.open) root.forceActiveFocus()
            }
        }
    }

    // Catches clicks anywhere else so the list closes like a normal dropdown
    MouseArea {
        visible: root.open
        x: -4000; y: -4000
        width: 8000; height: 8000
        onClicked: root.open = false
    }

    Rectangle {
        id: list
        visible: root.open
        y: header.height + 6
        width: parent.width
        height: root.visibleRows * root.rowHeight + 12
        radius: Theme.radiusTile
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border

        Flickable {
            id: flick
            objectName: "dropdownFlick"
            x: 6; y: 6
            width: parent.width - 12
            height: root.visibleRows * root.rowHeight
            contentHeight: entries.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: entries
                width: flick.width

                Repeater {
                    model: root.model

                    delegate: Rectangle {
                        id: entry
                        required property var modelData
                        readonly property bool selected: modelData.value === root.current

                        width: entries.width
                        height: root.rowHeight
                        radius: Theme.radiusTile - 4
                        color: entryArea.containsMouse ? Theme.surfaceHover : "transparent"

                        Row {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 12

                            Item {
                                width: 28
                                height: parent.height
                                Text {
                                    anchors.centerIn: parent
                                    visible: !!entry.modelData.flag
                                    text: entry.modelData.flag ? entry.modelData.flag : ""
                                    font.pixelSize: 22
                                }
                                Icon {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    visible: !entry.modelData.flag && !!entry.modelData.icon
                                    kind: entry.modelData.icon ? entry.modelData.icon : "globe"
                                    color: Theme.textDim
                                }
                            }

                            Text {
                                width: parent.width - 28 - 24 - 2 * parent.spacing
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: entry.modelData.label
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                font.bold: entry.selected
                                elide: Text.ElideRight
                            }

                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 24
                                height: 24
                                kind: "check"
                                color: Theme.accent
                                visible: entry.selected
                            }
                        }

                        MouseArea {
                            id: entryArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.open = false
                                root.picked(entry.modelData.value)
                            }
                        }
                    }
                }
            }
        }

        // Thin indicator, only when there is more to scroll to
        Rectangle {
            visible: flick.contentHeight > flick.height
            x: parent.width - width - 3
            width: 3
            radius: 1.5
            color: Theme.textDim
            opacity: 0.6
            y: 6 + (flick.height - height) * (flick.contentY / Math.max(1, flick.contentHeight - flick.height))
            height: Math.max(20, flick.height * flick.height / Math.max(1, flick.contentHeight))
        }
    }
}
