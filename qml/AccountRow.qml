import QtQuick

Rectangle {
    id: row

    property string accountId: ""
    property string name: ""
    property string serverUrl: ""
    property bool active: false
    property bool isDefault: false
    property bool configured: false

    signal useClicked()
    signal editClicked()
    signal deleteClicked()

    width: 100
    height: 136
    color: row.active ? "#eef6ee" : "#ffffff"
    border.color: row.active ? "#448844" : "#bbbbbb"
    border.width: row.active ? 2 : 1

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 12
        width: parent.width - 360
        text: row.name
        font.pixelSize: 30
        font.bold: true
        color: "#111111"
        elide: Text.ElideRight
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 54
        width: parent.width - 360
        text: row.serverUrl === "" ? "Not configured" : row.serverUrl
        font.pixelSize: 21
        color: "#555555"
        elide: Text.ElideRight
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        text: row.active ? "ACTIVE" : (row.isDefault ? "DEFAULT" : (row.configured ? "READY" : "SETUP NEEDED"))
        font.pixelSize: 19
        font.bold: true
        color: row.active ? "#006600" : (row.isDefault ? "#114477" : "#777777")
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.max(0, parent.width - 340)
        onClicked: row.useClicked()
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        z: 2
        spacing: 10

        ActionButton {
            width: 90
            height: 58
            text: "Use"
            active: !row.active
            onClicked: row.useClicked()
        }

        ActionButton {
            width: 90
            height: 58
            text: "Edit"
            onClicked: row.editClicked()
        }

        ActionButton {
            width: 100
            height: 58
            text: "Delete"
            onClicked: row.deleteClicked()
        }
    }
}
