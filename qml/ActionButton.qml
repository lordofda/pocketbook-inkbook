import QtQuick

Rectangle {
    id: button

    property string text: ""
    property bool active: true
    signal clicked()

    width: 220
    height: 76
    radius: 6
    color: active ? "#f2f2f2" : "#d8d8d8"
    border.color: "#555555"
    border.width: 2

    Text {
        anchors.centerIn: parent
        text: button.text
        font.pixelSize: 28
        color: "#111111"
    }

    MouseArea {
        anchors.fill: parent
        enabled: button.active
        onClicked: button.clicked()
    }
}
