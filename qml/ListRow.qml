import QtQuick

Rectangle {
    id: row

    property string itemId: ""
    property string title: ""
    property string subtitle: ""
    property string meta: ""
    property string badge: ""
    property bool downloaded: false
    property bool showDelete: false
    signal clicked()
    signal deleteClicked()

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
        width: parent.width - 160 - (row.showDelete ? 136 : 0)
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
        width: parent.width - 36
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
        width: parent.width - 160 - (row.showDelete ? 136 : 0)
        text: row.meta
        font.pixelSize: 21
        color: "#666666"
        elide: Text.ElideRight
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 18 + (row.showDelete ? 136 : 0)
        anchors.verticalCenter: parent.verticalCenter
        text: row.badge !== "" ? row.badge : (row.downloaded ? "OFFLINE" : "")
        font.pixelSize: 19
        font.bold: true
        color: row.badge === "SYNC" ? "#995500" : (row.downloaded ? "#006600" : "#666666")
    }

    MouseArea {
        anchors.fill: parent
        preventStealing: true
        onPressed: row.clicked()
    }

    MouseArea {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 12
        width: row.showDelete ? 120 : 0
        height: parent.height
        visible: row.showDelete
        enabled: row.showDelete
        preventStealing: true
        z: 2

        Rectangle {
            anchors.fill: parent
            color: "#f0f0f0"
            border.color: "#999999"
            border.width: 1
            radius: 4
        }

        Text {
            anchors.centerIn: parent
            text: "Delete"
            font.pixelSize: 24
            color: "#661111"
        }

        onClicked: row.deleteClicked()
    }
}
