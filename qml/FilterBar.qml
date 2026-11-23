import QtQuick

Item {
    id: bar

    property alias text: input.text
    property string externalText: ""

    onExternalTextChanged: {
        if (input.text !== externalText) {
            input.text = externalText
        }
    }

    height: 112

    Rectangle {
        id: searchBox
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#ffffff"
        border.color: "#777777"
        border.width: 2
        radius: 4

        TextInput {
            id: input
            anchors.fill: parent
            anchors.margins: 12
            font.pixelSize: 26
            color: "#111111"
            selectByMouse: true
            onTextChanged: controller.setFilterText(input.text)
        }

        Text {
            anchors.fill: input
            text: "Search title, series, or metadata..."
            font.pixelSize: input.font.pixelSize
            color: "#888888"
            visible: input.text.length === 0
        }
    }

    Flow {
        anchors.top: searchBox.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 3

        Repeater {
            model: ["All", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"]

            Rectangle {
                width: modelData === "All" ? 62 : 30
                height: 36
                color: modelData === "All" ? "#e0e8f0" : "#f2f2f2"
                border.color: "#888888"
                border.width: 1
                radius: 3

                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: modelData === "All" ? 18 : 21
                    font.bold: true
                    color: "#222222"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        input.text = modelData === "All" ? "" : modelData
                        controller.setFilterText(input.text)
                    }
                }
            }
        }
    }
}
