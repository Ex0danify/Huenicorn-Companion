import QtQuick
import QtQuick.Window
import "theme"
import "components"
import "pages"

Window {
    id: window

    width: 480
    height: 840
    minimumWidth: 420
    minimumHeight: 660
    title: qsTr("Huenicorn")
    color: Theme.background
    visible: false
    onVisibleChanged: if(!visible) backend.flush()

    property int tab: 0
    property int previousTab: 0
    onTabChanged: if(tab !== 3) previousTab = tab
    // Nothing to talk to and nothing to start
    readonly property bool daemonMissing: backend.daemonPath === "" && !backend.running
    property string toastText: ""

    // Lets tools and tests preselect a channel on the screen page
    function selectChannel(id) { layoutPage.selectedId = id }

    function toast(text) {
        toastText = text
        toastTimer.restart()
    }

    Timer {
        id: toastTimer
        interval: 2600
        onTriggered: window.toastText = ""
    }

    Connections {
        target: backend
        function onSaved(ok) { if(!ok) window.toast(qsTr("Could not save profile")) }
        function onRequestFailed(message) { window.toast(message) }
        // Screen and Lights are disabled without a daemon
        function onDaemonStateChanged() { if(!backend.running && (window.tab === 1 || window.tab === 2)) window.tab = 0 }
    }

    // Soft glow behind the header
    Canvas {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 420
        onWidthChanged: requestPaint()
        onPaint: {
            var c = getContext("2d")
            c.reset()
            var g = c.createRadialGradient(width / 2, 0, 0, width / 2, 0, Math.max(width, 380) * 0.95)
            g.addColorStop(0, "rgba(70, 130, 190, 0.55)")
            g.addColorStop(0.55, "rgba(40, 80, 125, 0.18)")
            g.addColorStop(1, "rgba(12, 14, 17, 0)")
            c.fillStyle = g
            c.fillRect(0, 0, width, height)
        }
    }

    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.margins: Theme.gutter
        height: 76

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            Rectangle {
                width: 46; height: 46; radius: 23
                color: Qt.rgba(1, 1, 1, 0.92)
                Icon { anchors.centerIn: parent; kind: "bulb"; color: "#14181d" }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Row {
                    spacing: 6
                    Text {
                        text: backend.running ? qsTr("Connected") : (backend.busy ? backend.statusText : qsTr("Not connected"))
                        color: backend.running ? Theme.success : (backend.daemonState === "error" ? Theme.danger : Theme.textDim)
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        visible: backend.running
                        text: qsTr("to daemon")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontBody
                    }
                }
                Text {
                    text: qsTr("Huenicorn")
                    color: Theme.text
                    font.pixelSize: 21
                    font.bold: true
                }
            }
        }

        IconButton {
            id: settingsButton
            objectName: "settingsButton"
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            visible: !backend.setup.active && !window.daemonMissing
            icon: "cog"
            active: window.tab === 3
            onClicked: window.tab = window.tab === 3 ? window.previousTab : 3
        }

        Text {
            anchors { right: settingsButton.visible ? settingsButton.left : parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
            visible: backend.running && !backend.setup.active
            text: backend.saving ? qsTr("Saving…") : (backend.dirty ? qsTr("Not saved") : qsTr("Saved"))
            color: Theme.textDim
            font.pixelSize: Theme.fontBody
        }
    }

    Item {
        id: content
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: tabs.top }
        anchors.topMargin: 6
        visible: !backend.setup.active && !window.daemonMissing

        SyncPage { anchors.fill: parent; visible: window.tab === 0 }
        LayoutPage { id: layoutPage; anchors.fill: parent; visible: window.tab === 1 }
        LightsPage { anchors.fill: parent; visible: window.tab === 2 }
        SettingsPage { anchors.fill: parent; visible: window.tab === 3 }
    }

    SetupPage {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.topMargin: 6
        visible: backend.setup.active
    }

    InstallPage {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.topMargin: 6
        visible: window.daemonMissing && !backend.setup.active
    }

    Rectangle {
        id: tabs
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 112
        visible: !backend.setup.active && !window.daemonMissing
        color: Theme.background

        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Theme.border }

        Row {
            anchors.centerIn: parent
            spacing: 14

            TabButton { label: qsTr("Sync"); icon: "play"; selected: window.tab === 0; onClicked: window.tab = 0 }
            TabButton { objectName: "tabScreen"; enabled: backend.running; label: qsTr("Screen"); icon: "screen"; selected: window.tab === 1; onClicked: window.tab = 1 }
            TabButton { objectName: "tabLights"; enabled: backend.running; label: qsTr("Lights"); icon: "bulb"; selected: window.tab === 2; onClicked: window.tab = 2 }
        }
    }

    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: tabs.top; bottomMargin: 16 }
        width: toastLabel.implicitWidth + 40
        height: 44
        radius: 22
        color: Theme.surfaceHover
        border.width: 1
        border.color: Theme.border
        opacity: window.toastText !== "" ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }

        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: window.toastText
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
    }
}
