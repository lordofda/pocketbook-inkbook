import QtQuick

Rectangle {
    id: field

    property string label: ""
    property alias text: input.text
    property string placeholder: ""
    property bool password: false

    width: 100
    height: 116
    color: "transparent"

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.top: parent.top
        text: field.label
        font.pixelSize: 23
        color: "#333333"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: labelText.bottom
        anchors.topMargin: 8
        height: 70
        color: "#ffffff"
        border.color: "#777777"
        border.width: 2

        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: 12
            font.pixelSize: 27
            color: "#111111"
            echoMode: field.password ? TextInput.Password : TextInput.Normal
            selectByMouse: true
        }

        Text {
            anchors.fill: input
            text: field.placeholder
            font.pixelSize: input.font.pixelSize
            color: "#888888"
            visible: field.placeholder !== "" && input.text.length === 0
        }
    }
}
