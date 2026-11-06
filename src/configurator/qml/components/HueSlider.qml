import QtQuick
import "../theme"

Item {
    id: root

    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0
    property real trackHeight: 14
    property real knobSize: 30
    // Above zero the track becomes a bar with square top corners and bottom corners of this
    // radius, meant to sit flush along the bottom of a rounded card
    property real barRadius: 0
    property color startColor: Theme.accentDeep
    property color endColor: "#e8f1fa"

    // Value shown while dragging, so labels can follow the handle
    readonly property real displayValue: dragging ? _drag : value
    property bool dragging: false
    property real _drag: value

    // Live feedback while dragging, committed once on release
    signal moved(real value)
    signal committed(real value)

    implicitHeight: barRadius > 0 ? trackHeight : Math.max(34, trackHeight)
    readonly property real _inset: Math.max(0, (trackHeight - knobSize) / 2)
    readonly property real _edge: barRadius > 0 ? 6 : _inset
    opacity: enabled ? 1 : 0.4

    readonly property real ratio: to > from ? Math.max(0, Math.min(1, (displayValue - from) / (to - from))) : 0

    Item {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: root.trackHeight
        clip: root.barRadius > 0

        // In bar mode the rectangle reaches above the clip, which hides its top corners
        Rectangle {
            width: parent.width
            height: root.barRadius > 0 ? root.trackHeight + root.barRadius : root.trackHeight
            y: root.trackHeight - height
            radius: root.barRadius > 0 ? root.barRadius : height / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: root.startColor }
                GradientStop { position: 1; color: root.endColor }
            }
        }
    }

    Rectangle {
        id: knob
        width: root.knobSize; height: root.knobSize; radius: knobSize / 2
        anchors.verticalCenter: parent.verticalCenter
        x: root._edge + root.ratio * (root.width - width - 2 * root._edge)
        color: "white"
        scale: area.pressed ? 1.1 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor

        function update(mx) {
            var r = Math.max(0, Math.min(1, (mx - root._edge - knob.width / 2) / (root.width - knob.width - 2 * root._edge)))
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
