import QtQuick
import "../theme"

// One light's capture region on the screen preview. Geometry is normalized (0..1).
Item {
    id: root

    required property var channel
    property bool selected: false
    property real screenWidth: 100
    property real screenHeight: 100

    signal picked()
    signal committed(real x0, real y0, real x1, real y1)

    readonly property real minSize: 0.04

    // Edited locally while dragging, the model catches up on release
    property real nx0: channel.x0
    property real ny0: channel.y0
    property real nx1: channel.x1
    property real ny1: channel.y1

    x: nx0 * screenWidth
    y: ny0 * screenHeight
    width: Math.max(2, (nx1 - nx0) * screenWidth)
    height: Math.max(2, (ny1 - ny0) * screenHeight)
    z: selected ? 2 : 1

    function commit() {
        root.committed(root.x / screenWidth, root.y / screenHeight,
                       (root.x + root.width) / screenWidth, (root.y + root.height) / screenHeight)
    }

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: Qt.rgba(Qt.color(channel.color).r, Qt.color(channel.color).g, Qt.color(channel.color).b,
                       !channel.active ? 0.06 : (root.selected ? 0.42 : 0.26))
        border.width: root.selected ? 2 : 1
        border.color: channel.active ? channel.color : Theme.textDim
        opacity: channel.active ? 1 : 0.6
    }

    Rectangle {
        id: badge
        x: 6; y: 6
        width: 22; height: 22; radius: 11
        color: channel.active ? channel.color : Theme.trackOff
        visible: root.width > 40 && root.height > 40

        Text {
            anchors.centerIn: parent
            text: channel.channelId
            color: "#0c0e11"
            font.pixelSize: 12
            font.bold: true
        }
    }

    Text {
        anchors { left: badge.right; leftMargin: 6; right: parent.right; rightMargin: 8; verticalCenter: badge.verticalCenter }
        visible: root.width > 130 && root.height > 40
        text: channel.name
        color: Theme.text
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideRight
    }

    MouseArea {
        anchors.fill: parent
        drag.target: root
        drag.axis: Drag.XAndYAxis
        drag.minimumX: 0
        drag.maximumX: root.screenWidth - root.width
        drag.minimumY: 0
        drag.maximumY: root.screenHeight - root.height
        cursorShape: Qt.SizeAllCursor
        onPressed: root.picked()
        onReleased: if (drag.active) root.commit()
    }

    // Corner handles for the selected region
    Repeater {
        model: root.selected ? ["tl", "tr", "bl", "br"] : []

        delegate: Rectangle {
            id: handle
            required property string modelData
            readonly property bool atLeft: modelData.charAt(1) === "l"
            readonly property bool atTop: modelData.charAt(0) === "t"

            width: 16; height: 16; radius: 8
            color: "white"
            border.width: 2
            border.color: channel.color
            x: atLeft ? -width / 2 : root.width - width / 2
            y: atTop ? -height / 2 : root.height - height / 2

            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                cursorShape: Qt.SizeFDiagCursor
                onPositionChanged: (m) => {
                    if (!pressed) return
                    var p = mapToItem(root.parent, m.x, m.y)
                    var nx = Math.max(0, Math.min(1, p.x / root.screenWidth))
                    var ny = Math.max(0, Math.min(1, p.y / root.screenHeight))
                    if (handle.atLeft) root.nx0 = Math.min(nx, root.nx1 - root.minSize)
                    else root.nx1 = Math.max(nx, root.nx0 + root.minSize)
                    if (handle.atTop) root.ny0 = Math.min(ny, root.ny1 - root.minSize)
                    else root.ny1 = Math.max(ny, root.ny0 + root.minSize)
                }
                onReleased: root.commit()
            }
        }
    }
}
