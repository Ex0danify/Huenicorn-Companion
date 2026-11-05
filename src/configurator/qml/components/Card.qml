import QtQuick
import "../theme"

Rectangle {
    property bool raised: false

    radius: Theme.radiusCard
    color: raised ? Theme.surfaceRaised : Theme.surface
    border.width: 1
    border.color: Theme.border
}
