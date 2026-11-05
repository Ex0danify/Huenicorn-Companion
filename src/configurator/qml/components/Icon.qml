import QtQuick
import "../theme"

// Tiny stroke icons drawn on a canvas so no image assets are needed
Canvas {
    id: root

    property string kind: "screen"
    property color color: Theme.text
    // Only used by "wave"
    property real amplitude: 1

    implicitWidth: 26
    implicitHeight: 26

    onKindChanged: requestPaint()
    onColorChanged: requestPaint()
    onAmplitudeChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        var c = getContext("2d")
        c.reset()
        c.strokeStyle = root.color
        c.fillStyle = root.color
        c.lineWidth = Math.max(1.6, width / 13)
        c.lineCap = "round"
        c.lineJoin = "round"
        var w = width, h = height

        if (kind === "screen") {
            c.beginPath()
            c.roundedRect(w * 0.1, h * 0.16, w * 0.8, h * 0.54, 3, 3)
            c.stroke()
            c.beginPath()
            c.moveTo(w * 0.34, h * 0.86); c.lineTo(w * 0.66, h * 0.86)
            c.moveTo(w * 0.5, h * 0.7); c.lineTo(w * 0.5, h * 0.86)
            c.stroke()
        } else if (kind === "bulb") {
            c.beginPath()
            c.arc(w * 0.5, h * 0.4, w * 0.27, Math.PI * 0.8, Math.PI * 2.2)
            c.lineTo(w * 0.62, h * 0.7); c.lineTo(w * 0.38, h * 0.7)
            c.closePath()
            c.stroke()
            c.beginPath()
            c.moveTo(w * 0.4, h * 0.84); c.lineTo(w * 0.6, h * 0.84)
            c.stroke()
        } else if (kind === "sliders") {
            var ys = [0.24, 0.5, 0.76], xs = [0.65, 0.3, 0.55]
            for (var i = 0; i < 3; i++) {
                c.beginPath()
                c.moveTo(w * 0.12, h * ys[i]); c.lineTo(w * 0.88, h * ys[i])
                c.stroke()
                c.beginPath()
                c.arc(w * xs[i], h * ys[i], w * 0.09, 0, Math.PI * 2)
                c.fill()
            }
        } else if (kind === "play") {
            c.beginPath()
            c.moveTo(w * 0.3, h * 0.2); c.lineTo(w * 0.8, h * 0.5); c.lineTo(w * 0.3, h * 0.8)
            c.closePath()
            c.fill()
        } else if (kind === "chevron") {
            c.beginPath()
            c.moveTo(w * 0.3, h * 0.38); c.lineTo(w * 0.5, h * 0.62); c.lineTo(w * 0.7, h * 0.38)
            c.stroke()
        } else if (kind === "check") {
            c.beginPath()
            c.moveTo(w * 0.22, h * 0.54); c.lineTo(w * 0.43, h * 0.74); c.lineTo(w * 0.78, h * 0.28)
            c.stroke()
        } else if (kind === "wave") {
            c.beginPath()
            var steps = 40
            for (var s = 0; s <= steps; s++) {
                var t = s / steps
                var px = w * (0.08 + 0.84 * t)
                var py = h * 0.5 - Math.sin(t * Math.PI * 2 * (1 + amplitude)) * h * 0.1 * (0.2 + amplitude * 1.6)
                if (s === 0) c.moveTo(px, py); else c.lineTo(px, py)
            }
            c.stroke()
        }
    }
}
