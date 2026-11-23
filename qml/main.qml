import QtQuick
import QtQuick.Window

Window {
    id: root

    visible: true
    width: deviceInfo.screenWidth
    height: deviceInfo.screenHeight - deviceInfo.panelHeight
    color: "#ffffff"
    title: "Komga Reader"

    property string headerTitle: {
        if (controller.view === "setup") return "Komga Reader"
        if (controller.view === "libraries") return "Libraries"
        if (controller.view === "series") return controller.selectedTitle
        if (controller.view === "books") return controller.selectedTitle
        return controller.selectedTitle
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            if (controller.view === "setup") controller.exit()
            else controller.back()
            event.accepted = true
        } else if (controller.view === "reader" && event.key === Qt.Key_Left) {
            controller.previousPage()
            event.accepted = true
        } else if (controller.view === "reader" && event.key === Qt.Key_Right) {
            controller.nextPage()
            event.accepted = true
        }
    }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 96
        color: "#f4f4f4"
        border.color: "#cccccc"
        border.width: 1

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 640
            text: root.headerTitle
            font.pixelSize: 34
            font.bold: true
            color: "#111111"
            elide: Text.ElideRight
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            ActionButton {
                width: 140
                height: 64
                text: "Back"
                active: controller.view !== "setup" && controller.view !== "libraries"
                onClicked: controller.back()
            }

            ActionButton {
                width: 180
                height: 64
                text: "In progress"
                active: controller.view === "libraries"
                onClicked: controller.openInProgress()
            }

            ActionButton {
                width: 140
                height: 64
                text: "Refresh"
                active: controller.view !== "setup" && controller.view !== "reader"
                onClicked: controller.refresh()
            }

            ActionButton {
                width: 120
                height: 64
                text: "Exit"
                onClicked: controller.exit()
            }
        }
    }

    Rectangle {
        id: footer
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 70
        color: "#eeeeee"
        border.color: "#cccccc"
        border.width: 1

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 48
            text: controller.busy ? "Working · " + controller.status : controller.status
            font.pixelSize: 22
            color: "#222222"
            elide: Text.ElideRight
        }
    }

    Item {
        id: content
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right

        Item {
            anchors.fill: parent
            visible: controller.view === "setup"

            Column {
                anchors.top: parent.top
                anchors.topMargin: 36
                anchors.left: parent.left
                anchors.leftMargin: 32
                anchors.right: parent.right
                anchors.rightMargin: 32
                spacing: 22

                Text {
                    width: parent.width
                    text: "Connect to Komga"
                    font.pixelSize: 40
                    font.bold: true
                    color: "#111111"
                    horizontalAlignment: Text.AlignLeft
                }

                Text {
                    width: parent.width
                    text: "Use the Komga HTTPS address and an API key. The server certificate is pinned on first connection."
                    font.pixelSize: 23
                    color: "#444444"
                    wrapMode: Text.WordWrap
                }

                InputField {
                    id: serverUrlInput
                    width: parent.width
                    label: "Server URL"
                    placeholder: "https://komga.example.com"
                    text: appSettings.serverUrl
                }

                InputField {
                    id: apiKeyInput
                    width: parent.width
                    label: "API key"
                    placeholder: "Paste Komga API key"
                    password: true
                    text: appSettings.apiKey
                }

                Row {
                    spacing: 18

                    ActionButton {
                        text: "Connect"
                        width: 220
                        active: !controller.busy
                        onClicked: controller.configure(serverUrlInput.text, apiKeyInput.text)
                    }

                    ActionButton {
                        text: "Clear"
                        width: 160
                        onClicked: {
                            appSettings.clearConnection()
                            controller.refresh()
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: "Status: " + controller.status
                    font.pixelSize: 21
                    color: "#555555"
                    wrapMode: Text.WordWrap
                }
            }
        }

        ListView {
            id: librariesList
            anchors.fill: parent
            anchors.margins: 24
            visible: controller.view === "libraries" || controller.view === "series" || controller.view === "books" || controller.view === "inProgress"
            clip: true
            spacing: 14
            model: controller.view === "libraries" ? controller.libraryModel
                                                   : controller.view === "series" ? controller.seriesModel
                                                                                  : controller.bookModel

            delegate: ListRow {
                width: librariesList.width
                itemId: model.itemId
                title: model.title
                subtitle: model.subtitle
                meta: model.meta
                badge: model.badge
                downloaded: model.downloaded
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    font.pixelSize: 14
                    color: "#cc0000"
                    text: parent.itemId + " | " + parent.title
                }
                onClicked: {
                    const id = model.itemId
                    controller.noteRowTap(id, index)
                    if (controller.view === "libraries") {
                        if (id !== "") controller.openLibraryId(id)
                        else controller.openLibrary(index)
                    } else if (controller.view === "series") {
                        if (id !== "") controller.openSeriesId(id)
                        else controller.openSeries(index)
                    } else {
                        if (id !== "") controller.openBookId(id)
                        else controller.openBook(index)
                    }
                }
            }
        }

        Item {
            anchors.fill: parent
            visible: controller.view === "reader"

            Image {
                id: pageImage
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: readerControls.top
                anchors.margins: 16
                source: controller.pageSource
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: false
                cache: false

                MouseArea {
                    anchors.fill: parent
                    onClicked: function (mouse) {
                        if (mouse.x < parent.width / 3) controller.previousPage()
                        else if (mouse.x > parent.width * 2 / 3) controller.nextPage()
                    }
                }
            }

            Text {
                anchors.centerIn: pageImage
                visible: controller.pageSource === ""
                text: "This book is not downloaded.\nPress Download to store it for offline reading."
                font.pixelSize: 30
                color: "#444444"
                horizontalAlignment: Text.AlignHCenter
            }

            Row {
                id: readerControls
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 18
                spacing: 12
                layoutDirection: Qt.LeftToRight

                ActionButton {
                    width: 130
                    height: 68
                    text: "Prev"
                    active: controller.pageSource !== ""
                    onClicked: controller.previousPage()
                }

                ActionButton {
                    width: 170
                    height: 68
                    text: controller.currentPage + " / " + Math.max(controller.pageCount, 1)
                    active: false
                }

                ActionButton {
                    width: 130
                    height: 68
                    text: "Next"
                    active: controller.pageSource !== ""
                    onClicked: controller.nextPage()
                }

                ActionButton {
                    width: 160
                    height: 68
                    text: "Download"
                    onClicked: controller.downloadCurrentBook()
                }

                ActionButton {
                    width: 150
                    height: 68
                    text: appSettings.grayscale ? "Gray" : "Color"
                    onClicked: controller.toggleGrayscale()
                }

                ActionButton {
                    width: 130
                    height: 68
                    text: "Delete"
                    active: controller.currentBookDownloaded
                    onClicked: controller.deleteCurrentBook()
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: controller.showTrustPrompt
            color: "#99000000"
            z: 10

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 80, 900)
                height: Math.min(parent.height - 100, 700)
                color: "#ffffff"
                border.color: "#333333"
                border.width: 2

                Column {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 28
                    spacing: 18

                    Text {
                        width: parent.width
                        text: "Trust this server?"
                        font.pixelSize: 36
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: "Verify the SHA-256 fingerprint in Komga's TLS certificate before trusting it."
                        font.pixelSize: 22
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: controller.discoveredFingerprint
                        font.pixelSize: 20
                        font.bold: true
                        color: "#004400"
                        wrapMode: Text.WrapAnywhere
                    }

                    Row {
                        spacing: 18

                        ActionButton {
                            width: 220
                            text: "Trust and continue"
                            onClicked: controller.trustDiscoveredCertificate()
                        }

                        ActionButton {
                            width: 160
                            text: "Cancel"
                            onClicked: controller.cancelTrust()
                        }
                    }
                }
            }
        }
    }
}
