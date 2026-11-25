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
    property string lastView: controller.view

    function dismissKeyboard() {
        if (listFilter) listFilter.hideKeyboard()
        else deviceInfo.hideKeyboard()
    }

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
            if (controller.view !== root.lastView) {
                root.lastView = controller.view
                root.dismissKeyboard()
                if (controller.view === "reader" && readerArea) readerArea.nativeDismissed = false
            }
            if (controller.view !== "reader") root.readerChrome = false
        }
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            root.dismissKeyboard()
            if (deleteAccountDialog.visible) {
                deleteAccountDialog.visible = false
            } else if (controller.showTrustPrompt) {
                controller.cancelTrust()
            } else if (controller.view === "setup") {
                controller.openAccounts()
            } else {
                controller.back()
            }
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
                onClicked: {
                    root.dismissKeyboard()
                    controller.goHome()
                }
            }

            ActionButton {
                width: 90
                height: 60
                text: "Back"
                active: controller.view !== "setup" && controller.view !== "libraries"
                onClicked: {
                    root.dismissKeyboard()
                    controller.back()
                }
            }

            ActionButton {
                width: 110
                height: 60
                text: "Refresh"
                active: controller.view !== "setup" && controller.view !== "reader" && controller.view !== "accounts"
                onClicked: {
                    root.dismissKeyboard()
                    controller.refresh()
                }
            }

            ActionButton {
                width: 120
                height: 60
                text: "Accounts"
                active: controller.view !== "accounts"
                onClicked: {
                    root.dismissKeyboard()
                    controller.openAccounts()
                }
            }

            ActionButton {
                width: 90
                height: 60
                text: appSettings.hideStatusBar ? "Bar:off" : "Bar:on"
                onClicked: {
                    root.dismissKeyboard()
                    controller.toggleStatusBar()
                }
            }

            ActionButton {
                width: 90
                height: 60
                text: "Exit"
                onClicked: {
                    root.dismissKeyboard()
                    controller.exit()
                }
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
            id: setupView
            anchors.fill: parent
            visible: controller.view === "setup"
            property string loadedAccountId: ""

            function loadAccountFields() {
                accountNameInput.text = appSettings.activeAccountName
                serverUrlInput.text = appSettings.serverUrl
                apiKeyInput.text = appSettings.apiKey
                loadedAccountId = appSettings.activeAccountId
            }

            Component.onCompleted: setupView.loadAccountFields()

            Connections {
                target: appSettings
                function onConfigurationChanged() {
                    if (setupView.loadedAccountId !== appSettings.activeAccountId) {
                        setupView.loadAccountFields()
                    }
                }
            }

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
                    text: "Name this account, then enter the Komga HTTPS address and API key. Save stores the details; Connect saves and tests them. The server certificate is pinned per account on first connection."
                    font.pixelSize: 23
                    color: "#444444"
                    wrapMode: Text.WordWrap
                }

                InputField {
                    id: accountNameInput
                    width: parent.width
                    label: "Account name"
                    placeholder: "Home Komga"
                }

                Row {
                    spacing: 18

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Default: " + appSettings.defaultAccountName
                        font.pixelSize: 22
                        color: "#444444"
                    }

                    ActionButton {
                        width: 210
                        height: 56
                        text: appSettings.defaultAccountId === appSettings.activeAccountId
                              ? "Default account"
                              : "Set as default"
                        active: appSettings.defaultAccountId !== appSettings.activeAccountId
                        onClicked: controller.setDefaultAccount(appSettings.activeAccountId)
                    }
                }

                InputField {
                    id: serverUrlInput
                    width: parent.width
                    label: "Server URL"
                    placeholder: "https://komga.example.com"
                }

                InputField {
                    id: apiKeyInput
                    width: parent.width
                    label: "API key"
                    placeholder: "Paste Komga API key"
                    password: true
                }

                Row {
                    spacing: 18

                    ActionButton {
                        text: "Save"
                        width: 160
                        onClicked: controller.saveAccount(serverUrlInput.text, apiKeyInput.text, accountNameInput.text)
                    }

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
                            accountNameInput.text = ""
                            serverUrlInput.text = ""
                            apiKeyInput.text = ""
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
            id: accountsView
            anchors.fill: parent
            visible: controller.view === "accounts"

            property string deleteAccountId: ""
            property string deleteAccountName: ""

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
                    text: "Active: " + appSettings.activeAccountName + " · Default: " + appSettings.defaultAccountName
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
                anchors.leftMargin: 24
                anchors.rightMargin: 110
                anchors.bottomMargin: 24
                clip: true
                spacing: 14
                model: appSettings.accounts

                delegate: AccountRow {
                    width: accountList.width
                    accountId: modelData.id
                    name: modelData.name
                    serverUrl: modelData.serverUrl
                    active: modelData.active
                    isDefault: modelData.isDefault
                    configured: modelData.configured
                    onUseClicked: controller.switchAccount(modelData.id)
                    onDefaultClicked: controller.setDefaultAccount(modelData.id)
                    onEditClicked: controller.editAccount(modelData.id)
                    onDeleteClicked: {
                        accountsView.deleteAccountId = modelData.id
                        accountsView.deleteAccountName = modelData.name !== "" ? modelData.name : modelData.id
                        deleteAccountDialog.visible = true
                    }
                }
            }

            ListScroller {
                anchors.top: accountActions.bottom
                anchors.topMargin: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                anchors.right: parent.right
                anchors.rightMargin: 24
                view: accountList
            }

            Rectangle {
                id: deleteAccountDialog
                anchors.fill: parent
                visible: false
                color: "#99000000"
                z: 20

                MouseArea {
                    anchors.fill: parent
                    onClicked: deleteAccountDialog.visible = false
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 80, 760)
                    height: 280
                    color: "#ffffff"
                    border.color: "#333333"
                    border.width: 2

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {}
                    }

                    Column {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 28
                        spacing: 18

                        Text {
                            width: parent.width
                            text: "Delete account?"
                            font.pixelSize: 36
                            font.bold: true
                        }

                        Text {
                            width: parent.width
                            text: "Delete '" + accountsView.deleteAccountName + "'? Its connection settings will be removed."
                            font.pixelSize: 22
                            wrapMode: Text.WordWrap
                        }

                        Row {
                            spacing: 18

                            ActionButton {
                                width: 180
                                text: "Delete"
                                onClicked: {
                                    const id = accountsView.deleteAccountId
                                    deleteAccountDialog.visible = false
                                    controller.deleteAccount(id)
                                }
                            }

                            ActionButton {
                                width: 160
                                text: "Cancel"
                                onClicked: deleteAccountDialog.visible = false
                            }
                        }
                    }
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
                anchors.rightMargin: 90
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
                    seriesId: model.seriesId
                    title: model.title
                    subtitle: model.subtitle
                    meta: model.meta
                    badge: model.badge
                    downloaded: model.downloaded
                    showDelete: controller.view === "offline"
                    showOpenSeries: controller.view === "books" || controller.view === "inProgress" || controller.view === "onDeck"
                    showDownload: !model.downloaded && (controller.view === "books" || controller.view === "inProgress" || controller.view === "onDeck")
                    onClicked: {
                        const id = model.itemId
                        root.dismissKeyboard()
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
                    onOpenSeriesClicked: {
                        root.dismissKeyboard()
                        controller.openSeriesForBookId(model.seriesId)
                    }
                    onDownloadClicked: {
                        root.dismissKeyboard()
                        controller.downloadBookId(model.itemId)
                    }
                }
            }

            ListScroller {
                anchors.top: listFilter.bottom
                anchors.topMargin: 14
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                view: librariesList
            }
        }

        Item {
            id: readerArea
            anchors.fill: parent
            visible: controller.view === "reader"

            property string nativePath: ""
            property bool nativeDismissed: false

            function syncNativePage() {
                if (controller.pagePath !== readerArea.nativePath) {
                    readerArea.nativePath = controller.pagePath
                    readerArea.nativeDismissed = false
                }
                if (controller.view !== "reader" || controller.pagePath === "" || !controller.colorPage) {
                    nativeReader.close()
                } else if (!readerArea.nativeDismissed) {
                    nativeReader.show(controller.pagePath)
                }
            }

            function resumeNativePage() {
                readerArea.nativeDismissed = false
                readerArea.syncNativePage()
            }

            Connections {
                target: controller
                function onStateChanged() { readerArea.syncNativePage() }
            }

            Connections {
                target: nativeReader
                function onPreviousRequested() { controller.previousPage() }
                function onNextRequested() { controller.nextPage() }
                function onClosed() { readerArea.nativeDismissed = true }
                function onLoadFailed() { root.readerChrome = true }
            }

            Image {
                id: pageImage
                visible: controller.pageSource !== "" && controller.colorPage
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
                        root.dismissKeyboard()
                        if (mouse.x < parent.width / 3) controller.previousPage()
                        else if (mouse.x > parent.width * 2 / 3) controller.nextPage()
                        else root.readerChrome = !root.readerChrome
                    }
                }
            }

            Text {
                anchors.centerIn: pageImage
                visible: controller.view === "reader" &&
                         (controller.pageSource === "" || (!controller.colorPage && !nativeReader.active))
                text: controller.pageSource === ""
                      ? "This book is not downloaded.\nPress Download to store it for offline reading."
                      : "This offline copy was cached in grayscale.\nPress Download to rebuild it for vivid color."
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
                spacing: 6
                layoutDirection: Qt.LeftToRight

                ActionButton {
                    width: 90
                    height: 64
                    text: "Prev"
                    active: controller.pageSource !== ""
                    onClicked: controller.previousPage()
                }

                ActionButton {
                    width: 120
                    height: 64
                    text: controller.currentPage + " / " + Math.max(controller.pageCount, 1)
                    active: false
                }

                ActionButton {
                    width: 90
                    height: 64
                    text: "Next"
                    active: controller.pageSource !== ""
                    onClicked: controller.nextPage()
                }

                ActionButton {
                    width: 120
                    height: 64
                    text: "Download"
                    onClicked: controller.downloadCurrentBook()
                }

                ActionButton {
                    width: 90
                    height: 64
                    text: "Sync"
                    active: controller.currentBookDownloaded && !controller.busy
                    onClicked: controller.syncCurrentProgress()
                }

                ActionButton {
                    width: 140
                    height: 64
                    text: "Open series"
                    active: controller.currentSeriesId !== ""
                    onClicked: controller.openCurrentSeries()
                }

                ActionButton {
                    width: 100
                    height: 64
                    text: "Vivid"
                    active: controller.colorPage && !nativeReader.active
                    onClicked: readerArea.resumeNativePage()
                }

                ActionButton {
                    width: 90
                    height: 64
                    text: "Delete"
                    active: controller.currentBookDownloaded
                    onClicked: controller.deleteCurrentBook()
                }
            }

            Rectangle {
                id: completionBanner
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: root.readerChrome ? readerControls.top : parent.bottom
                anchors.bottomMargin: root.readerChrome ? 12 : 24
                width: Math.min(parent.width - 48, 980)
                height: 76
                visible: controller.view === "reader" && controller.bookCompleted
                color: "#eef5ff"
                border.color: "#4477aa"
                border.width: 2
                radius: 6
                z: 3

                Row {
                    anchors.centerIn: parent
                    spacing: 12

                    Text {
                        width: 110
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Finished"
                        font.pixelSize: 24
                        font.bold: true
                        color: "#224466"
                    }

                    Text {
                        width: 420
                        anchors.verticalCenter: parent.verticalCenter
                        text: controller.nextBookId !== "" ? "Next: " + controller.nextBookTitle
                                                           : "No next book found"
                        font.pixelSize: 22
                        color: "#333333"
                        elide: Text.ElideRight
                    }

                    ActionButton {
                        width: 130
                        height: 56
                        text: "Open next"
                        active: controller.nextBookId !== ""
                        onClicked: controller.openNextBook()
                    }

                    ActionButton {
                        width: 140
                        height: 56
                        text: "Open series"
                        active: controller.currentSeriesId !== ""
                        onClicked: controller.openCurrentSeries()
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: controller.showTrustPrompt
            color: "#99000000"
            z: 10

            MouseArea {
                anchors.fill: parent
                onClicked: {}
            }

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
