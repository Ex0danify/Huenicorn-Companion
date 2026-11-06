import QtQuick
import "../theme"

Item {
    id: root

    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0
    // A track taller than the knob turns the slider into a bar the knob sits inside
    property real trackHeight: 14
    property color startColor: Theme.accentDeep
    property color endColor: "#e8f1fa"

    // Value shown while dragging, so labels can follow the handle
    readonly property real displayValue: dragging ? _drag : value
    property bool dragging: false
    property real _drag: value

    // Live feedback while dragging, committed once on release
    signal moved(real value)
    signal committed(real value)

    implicitHeight: Math.max(34, trackHeight)
    readonly property real _inset: Math.max(0, (trackHeight - knob.height) / 2)
    opacity: enabled ? 1 : 0.4

    readonly property real ratio: to > from ? Math.max(0, Math.min(1, (displayValue - from) / (to - from))) : 0

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: root.trackHeight
        radius: height / 2
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: root.startColor }
            GradientStop { position: 1; color: root.endColor }
        }
    }

    Rectangle {
        id: knob
        width: 30; height: 30; radius: 15
        anchors.verticalCenter: parent.verticalCenter
        x: root._inset + root.ratio * (root.width - width - 2 * root._inset)
        color: "white"
        scale: area.pressed ? 1.1 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor

        function update(mx) {
            var r = Math.max(0, Math.min(1, (mx - root._inset - knob.width / 2) / (root.width - knob.width - 2 * root._inset)))
            var v = root.from + r * (root.to - root.from)
            if (root.stepSize > 0)
                v = Math.round(v / root.stepSize) * root.stepSize
            root._drag = v
            root.moved(v)
        }

        onPressed: (m) => { root.dragging = true; update(m.x) }
        onPositionChanged: (m) => { if (pressed) update(m.x) }
        onReleased: { root.dragging = false; root.committed(root._drag) }
    }
}
