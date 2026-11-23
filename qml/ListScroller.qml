import QtQuick

Item {
    id: scroller

    property var view: null
    property real maxScroll: view ? Math.max(0, view.contentHeight - view.height) : 0
    property real scrollFraction: maxScroll > 0 ? Math.max(0, Math.min(1, view.contentY / maxScroll)) : 0

    width: 78

    function scrollBy(delta) {
        if (!view || view.count === 0) {
            return
        }
        let index = view.indexAt(view.contentX, view.contentY + 1)
        if (index < 0) {
            index = view.indexAt(view.contentX, view.contentY + view.height / 2)
        }
        if (index < 0) {
            index = 0
        }
        const next = Math.max(0, Math.min(view.count - 1, index + delta))
        view.currentIndex = next
        view.positionViewAtIndex(next, ListView.Beginning)
    }

    function setFraction(fraction) {
        if (!view) {
            return
        }
        view.contentY = Math.max(0, Math.min(1, fraction)) * maxScroll
    }

    Rectangle {
        id: upButton
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: 66
        height: 62
        radius: 6
        visible: scroller.view && scroller.view.count > 0
        enabled: scroller.view && !scroller.view.atYBeginning
        color: enabled ? "#f0f0f0" : "#fafafa"
        border.color: "#777777"
        border.width: 2

        Text {
            anchors.centerIn: parent
            text: "Up"
            font.pixelSize: 24
            color: parent.enabled ? "#111111" : "#aaaaaa"
        }

        MouseArea {
            anchors.fill: parent
            enabled: parent.enabled
            onClicked: scroller.scrollBy(-1)
        }
    }

    Rectangle {
        id: downButton
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: 66
        height: 62
        radius: 6
        visible: scroller.view && scroller.view.count > 0
        enabled: scroller.view && !scroller.view.atYEnd
        color: enabled ? "#f0f0f0" : "#fafafa"
        border.color: "#777777"
        border.width: 2

        Text {
            anchors.centerIn: parent
            text: "Down"
            font.pixelSize: 24
            color: parent.enabled ? "#111111" : "#aaaaaa"
        }

        MouseArea {
            anchors.fill: parent
            enabled: parent.enabled
            onClicked: scroller.scrollBy(1)
        }
    }

    Rectangle {
        id: track
        anchors.top: upButton.bottom
        anchors.topMargin: 10
        anchors.bottom: downButton.top
        anchors.bottomMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: 16
        radius: 8
        visible: scroller.view && scroller.view.count > 0
        color: "#dddddd"
        border.color: "#888888"
        border.width: 1

        Rectangle {
            id: handle
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: scroller.maxScroll > 0 ? scroller.scrollFraction * Math.max(0, track.height - height) : 0
            width: parent.width
            height: {
                if (!scroller.view || scroller.view.contentHeight <= 0) {
                    return parent.height
                }
                return Math.max(36, Math.min(parent.height, parent.height * scroller.view.height / scroller.view.contentHeight))
            }
            radius: 8
            color: "#666666"
        }

        MouseArea {
            anchors.fill: parent

            function moveTo(mouseY) {
                const travel = parent.height - handle.height
                if (travel <= 0) {
                    return
                }
                scroller.setFraction((mouseY - handle.height / 2) / travel)
            }

            onPressed: moveTo(mouse.y)
            onPositionChanged: if (pressed) moveTo(mouse.y)
        }
    }
}
