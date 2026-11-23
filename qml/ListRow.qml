import QtQuick

Rectangle {
    id: row

    property string itemId: ""
    property string title: ""
    property string subtitle: ""
    property string meta: ""
    property string badge: ""
    property bool downloaded: false
    signal clicked()

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
        width: parent.width - 160
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
        width: parent.width - 160
        text: row.meta
        font.pixelSize: 21
        color: "#666666"
        elide: Text.ElideRight
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        text: row.downloaded ? "OFFLINE" : row.badge
        font.pixelSize: 19
        font.bold: true
        color: row.downloaded ? "#006600" : "#666666"
    }

    MouseArea {
        anchors.fill: parent
        preventStealing: true
        onPressed: row.clicked()
    }
}
