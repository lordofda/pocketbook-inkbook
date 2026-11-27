import QtQuick

Rectangle {
    id: row

    property string itemId: ""
    property string seriesId: ""
    property string title: ""
    property string subtitle: ""
    property string meta: ""
    property string badge: ""
    property bool downloaded: false
    property bool showDelete: false
    property bool showOpenSeries: false
    property bool showDownload: false
    property bool showSync: false
    property int reservedWidth: (row.showDelete ? 132 : 0) + (row.showOpenSeries ? 132 : 0) + (row.showDownload ? 132 : 0)
    signal clicked()
    signal deleteClicked()
    signal openSeriesClicked()
    signal downloadClicked()
    signal syncClicked()

    width: 100
    height: 132
    color: "#ffffff"
    border.color: "#bbbbbb"
    border.width: 1

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 12
        width: parent.width - 160 - row.reservedWidth
        text: row.title
        font.pixelSize: 30
        font.bold: true
        color: "#111111"
        elide: Text.ElideRight
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 52
        width: parent.width - 36 - row.reservedWidth
        text: row.subtitle
        font.pixelSize: 22
        color: "#444444"
        elide: Text.ElideRight
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        width: parent.width - 160 - row.reservedWidth
        text: row.meta
        font.pixelSize: 21
        color: "#666666"
        elide: Text.ElideRight
    }

    Text {
        z: row.showSync ? 2 : 0
        anchors.right: parent.right
        anchors.rightMargin: 18 + row.reservedWidth
        anchors.verticalCenter: parent.verticalCenter
        text: row.badge !== "" ? row.badge : (row.downloaded ? "OFFLINE" : "")
        font.pixelSize: 19
        font.bold: true
        color: row.badge === "SYNC" ? "#995500" : (row.downloaded ? "#006600" : "#666666")

        MouseArea {
            anchors.fill: parent
            visible: row.showSync && row.badge === "SYNC"
            enabled: visible
            z: 2
            preventStealing: true
            onClicked: row.syncClicked()
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.max(0, parent.width - row.reservedWidth)
        onClicked: row.clicked()
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12
        visible: row.showDelete || row.showOpenSeries || row.showDownload
        z: 2

        Rectangle {
            visible: row.showOpenSeries
            width: 120
            height: 72
            color: "#eaf2fb"
            border.color: "#5588bb"
            border.width: 1
            radius: 4

            Text {
                anchors.centerIn: parent
                text: "Open series"
                font.pixelSize: 20
                color: "#114477"
            }

            MouseArea {
                anchors.fill: parent
                visible: row.showOpenSeries
                enabled: row.showOpenSeries
                preventStealing: true
                onClicked: row.openSeriesClicked()
            }
        }

        Rectangle {
            visible: row.showDownload
            width: 120
            height: 72
            color: "#edf6ed"
            border.color: "#558855"
            border.width: 1
            radius: 4

            Text {
                anchors.centerIn: parent
                text: "Download"
                font.pixelSize: 19
                color: "#115511"
            }

            MouseArea {
                anchors.fill: parent
                visible: row.showDownload
                enabled: row.showDownload
                preventStealing: true
                onClicked: row.downloadClicked()
            }
        }

        Rectangle {
            visible: row.showDelete
            width: 120
            height: 72
            color: "#f0f0f0"
            border.color: "#999999"
            border.width: 1
            radius: 4

            Text {
                anchors.centerIn: parent
                text: "Delete"
                font.pixelSize: 24
                color: "#661111"
            }

            MouseArea {
                anchors.fill: parent
                visible: row.showDelete
                enabled: row.showDelete
                preventStealing: true
                onClicked: row.deleteClicked()
            }
        }
    }
}
