pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#0c0e11"
    readonly property color surface: "#1a1d22"
    readonly property color surfaceRaised: "#262a31"
    readonly property color surfaceHover: "#353a43"
    readonly property color border: Qt.rgba(1, 1, 1, 0.07)

    readonly property color text: "#f4f6f8"
    readonly property color textDim: "#98a2ad"
    readonly property color textOnAccent: "#0d1b2a"

    readonly property color accent: "#7cc4ff"
    readonly property color accentDeep: "#4a8fd0"
    readonly property color success: "#79c800"
    readonly property color danger: "#ff6161"
    readonly property color warning: "#ffb020"
    readonly property color trackOff: "#3a3f48"

    readonly property int radiusCard: 22
    readonly property int radiusTile: 14
    readonly property int gutter: 24

    readonly property int fontTitle: 22
    readonly property int fontBody: 15
    readonly property int fontSmall: 12
}
