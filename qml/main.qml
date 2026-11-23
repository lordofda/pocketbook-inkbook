import QtQuick
import QtQuick.Window

Window {
    id: root

    visible: true
    width: deviceInfo.screenWidth
    height: deviceInfo.screenHeight - (appSettings.hideStatusBar ? 0 : deviceInfo.panelHeight)
    color: "#ffffff"
    title: "Komga Reader"

    property bool readerChrome: false

    property string headerTitle: {
        if (controller.view === "setup") return "Komga Reader"
        if (controller.view === "libraries") return "Libraries"
        if (controller.view === "offline") return "Offline books"
        if (controller.view === "accounts") return "Accounts"
        if (controller.view === "recent") return controller.selectedTitle
        if (controller.view === "onDeck") return controller.selectedTitle
        if (controller.view === "series") return controller.selectedTitle
        if (controller.view === "books") return controller.selectedTitle
        return controller.selectedTitle
    }

    Connections {
        target: controller
        function onStateChanged() {
            if (controller.view !== "reader") root.readerChrome = false
        }
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            if (controller.view === "setup") controller.openAccounts()
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
        visible: controller.view !== "reader" || root.readerChrome
        height: 96
        color: "#f4f4f4"
        border.color: "#cccccc"
        border.width: 1

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 700
            text: root.headerTitle
            font.pixelSize: 34
            font.bold: true
            color: "#111111"
            elide: Text.ElideRight
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            ActionButton {
                width: 90
                height: 60
                text: "Home"
                active: controller.view !== "setup" && controller.view !== "libraries"
                onClicked: controller.goHome()
            }

            ActionButton {
                width: 90
                height: 60
                text: "Back"
                active: controller.view !== "setup" && controller.view !== "libraries"
                onClicked: controller.back()
            }

            ActionButton {
                width: 110
                height: 60
                text: "Refresh"
                active: controller.view !== "setup" && controller.view !== "reader" && controller.view !== "accounts"
                onClicked: controller.refresh()
            }

            ActionButton {
                width: 120
                height: 60
                text: "Accounts"
                active: controller.view !== "accounts"
                onClicked: controller.openAccounts()
            }

            ActionButton {
                width: 90
                height: 60
                text: appSettings.hideStatusBar ? "Bar:off" : "Bar:on"
                onClicked: controller.toggleStatusBar()
            }

            ActionButton {
                width: 90
                height: 60
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
        visible: controller.view !== "reader" || root.readerChrome
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
        anchors.top: header.visible ? header.bottom : parent.top
        anchors.bottom: footer.visible ? footer.top : parent.bottom
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
                    text: "Name this account, then enter the Komga HTTPS address and API key. The server certificate is pinned per account on first connection."
                    font.pixelSize: 23
                    color: "#444444"
                    wrapMode: Text.WordWrap
                }

                InputField {
                    id: accountNameInput
                    width: parent.width
                    label: "Account name"
                    placeholder: "Home Komga"
                    text: appSettings.activeAccountName
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
                        onClicked: controller.configure(serverUrlInput.text, apiKeyInput.text, accountNameInput.text)
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

        Item {
            anchors.fill: parent
            visible: controller.view === "accounts"

            Row {
                id: accountActions
                anchors.top: parent.top
                anchors.topMargin: 24
                anchors.left: parent.left
                anchors.leftMargin: 24
                spacing: 14

                ActionButton {
                    width: 220
                    height: 64
                    text: "New account"
                    onClicked: controller.newAccount()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Active: " + appSettings.activeAccountName
                    font.pixelSize: 24
                    color: "#444444"
                }
            }

            ListView {
                id: accountList
                anchors.top: accountActions.bottom
                anchors.topMargin: 18
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 24
                clip: true
                spacing: 14
                model: appSettings.accounts

                delegate: AccountRow {
                    width: accountList.width
                    accountId: modelData.id
                    name: modelData.name
                    serverUrl: modelData.serverUrl
                    active: modelData.active
                    configured: modelData.configured
                    onUseClicked: controller.switchAccount(modelData.id)
                    onEditClicked: controller.editAccount(modelData.id)
                    onDeleteClicked: controller.deleteAccount(modelData.id)
                }
            }
        }

        Item {
            anchors.fill: parent
            anchors.margins: 24
            visible: controller.view === "libraries" || controller.view === "series" || controller.view === "books" || controller.view === "inProgress" || controller.view === "offline" || controller.view === "recent" || controller.view === "onDeck"

            Row {
                id: homeSections
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                visible: controller.view === "libraries"
                height: 64
                spacing: 12

                ActionButton {
                    width: 160
                    height: 64
                    text: "In progress"
                    onClicked: controller.openInProgress()
                }

                ActionButton {
                    width: 160
                    height: 64
                    text: "Offline"
                    onClicked: controller.openOffline()
                }

                ActionButton {
                    width: 160
                    height: 64
                    text: "Recent"
                    onClicked: controller.openRecent()
                }

                ActionButton {
                    width: 160
                    height: 64
                    text: "On deck"
                    onClicked: controller.openOnDeck()
                }
            }

            FilterBar {
                id: listFilter
                anchors.top: homeSections.visible ? homeSections.bottom : parent.top
                anchors.topMargin: homeSections.visible ? 14 : 0
                anchors.left: parent.left
                anchors.right: parent.right
                externalText: controller.filterText
            }

            ListView {
                id: librariesList
                anchors.top: listFilter.bottom
                anchors.topMargin: 14
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                clip: true
                spacing: 14
                model: controller.view === "libraries" ? controller.libraryModel
                                                       : controller.view === "series" ? controller.seriesModel
                                                                                      : controller.view === "offline" ? controller.offlineModel
                                                                                                                       : controller.view === "recent" ? controller.newSeriesModel
                                                                                                                                                      : controller.view === "onDeck" ? controller.onDeckModel
                                                                                                                                                                                     : controller.bookModel

                delegate: ListRow {
                    width: librariesList.width
                    itemId: model.itemId
                    title: model.title
                    subtitle: model.subtitle
                    meta: model.meta
                    badge: model.badge
                    downloaded: model.downloaded
                    showDelete: controller.view === "offline"
                    onClicked: {
                        const id = model.itemId
                        controller.noteRowTap(id, index)
                        if (controller.view === "libraries") {
                            if (id !== "") controller.openLibraryId(id)
                            else controller.openLibrary(index)
                        } else if (controller.view === "series") {
                            if (id !== "") controller.openSeriesId(id)
                            else controller.openSeries(index)
                        } else if (controller.view === "offline") {
                            if (id !== "") controller.openOfflineBookId(id)
                        } else if (controller.view === "recent") {
                            if (id !== "") controller.openRecentSeriesId(id)
                        } else if (controller.view === "onDeck") {
                            if (id !== "") controller.openOnDeckBookId(id)
                        } else {
                            if (id !== "") controller.openBookId(id)
                            else controller.openBook(index)
                        }
                    }
                    onDeleteClicked: controller.deleteOfflineBook(model.itemId)
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
                anchors.bottom: root.readerChrome ? readerControls.top : parent.bottom
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
                        else root.readerChrome = !root.readerChrome
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
                visible: controller.view === "reader" && root.readerChrome
                spacing: 10
                layoutDirection: Qt.LeftToRight

                ActionButton {
                    width: 100
                    height: 64
                    text: "Prev"
                    active: controller.pageSource !== ""
                    onClicked: controller.previousPage()
                }

                ActionButton {
                    width: 140
                    height: 64
                    text: controller.currentPage + " / " + Math.max(controller.pageCount, 1)
                    active: false
                }

                ActionButton {
                    width: 100
                    height: 64
                    text: "Next"
                    active: controller.pageSource !== ""
                    onClicked: controller.nextPage()
                }

                ActionButton {
                    width: 130
                    height: 64
                    text: "Download"
                    onClicked: controller.downloadCurrentBook()
                }

                ActionButton {
                    width: 110
                    height: 64
                    text: "Sync"
                    active: controller.currentBookDownloaded && !controller.busy
                    onClicked: controller.syncCurrentProgress()
                }

                ActionButton {
                    width: 110
                    height: 64
                    text: appSettings.grayscale ? "Gray" : "Color"
                    onClicked: controller.toggleGrayscale()
                }

                ActionButton {
                    width: 110
                    height: 64
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
