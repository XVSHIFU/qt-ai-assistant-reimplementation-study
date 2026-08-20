import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs 1.3
import "../Controls"

Item {
    id: root
    objectName: "chatHistoryPanel"
    property real scaleFactor: window.scale
    property string actionConversationId: ""
    property string actionConversationTitle: ""
    property bool actionConversationPinned: false
    property bool permanentDelete: false
    property bool cleanupExpired: false
    property string pendingExportId: ""
    readonly property bool deleteConfirmVisible: deletePopup.visible

    function formattedTime(value) {
        var date = new Date(value)
        return isNaN(date.getTime()) ? "" : Qt.formatDateTime(date, "MM-dd hh:mm")
    }

    function scrollForSmoke() {
        var maximum = Math.max(0, listView.contentHeight - listView.height)
        listView.contentY = Math.min(maximum, 110 * scaleFactor)
    }

    function openDeleteConfirmForSmoke() {
        actionConversationId = "smoke-markdown"
        actionConversationTitle = qsTr("Markdown 渲染验收")
        actionConversationPinned = true
        permanentDelete = false
        cleanupExpired = false
        deletePopup.open()
    }

    FileDialog {
        id: exportFolderDialog
        title: qsTr("选择导出目录")
        selectFolder: true
        onAccepted: {
            if (root.pendingExportId.length > 0)
                dialog_manager.exportChatById(root.pendingExportId, fileUrl.toString(),
                                              "markdown")
        }
    }

    function closeDeleteConfirmForSmoke() {
        deletePopup.close()
    }

    component DialogButton: Button {
        id: dialogButton
        property bool primary: false
        property bool destructive: false
        width: 72 * root.scaleFactor
        height: 32 * root.scaleFactor
        hoverEnabled: true
        background: Rectangle {
            radius: 9 * root.scaleFactor
            color: dialogButton.down
                   ? (dialogButton.destructive ? uiPreferences.dangerColor
                      : dialogButton.primary ? uiPreferences.accentHoverColor : uiPreferences.disabledSurfaceColor)
                   : dialogButton.hovered
                     ? (dialogButton.destructive ? "#B91C1C"
                        : dialogButton.primary ? "#6366F1" : "#F4F4F5")
                     : dialogButton.destructive ? uiPreferences.dangerColor
                       : dialogButton.primary ? theme.item.themeZeroColor : theme.item.colorW2
            border.width: dialogButton.visualFocus
                          ? Math.max(2, root.scaleFactor)
                          : (dialogButton.primary || dialogButton.destructive ? 0 : 1)
            border.color: dialogButton.visualFocus
                          ? (dialogButton.destructive ? "#7F1D1D" : theme.item.themeZeroColor)
                          : theme.item.colorW3
        }
        contentItem: Text {
            text: dialogButton.text
            color: dialogButton.primary || dialogButton.destructive
                   ? "white" : theme.item.colorB1
            font.pixelSize: 12 * root.scaleFactor
            font.family: fontManager.item.oppoSansM.name
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 14 * scaleFactor
        anchors.rightMargin: 14 * scaleFactor
        anchors.top: parent.top
        anchors.topMargin: 10 * scaleFactor
        visible: dialog_manager.storageUnavailable
        text: qsTr("本地存储不可用，历史修改已禁用")
        color: uiPreferences.dangerColor
        font.pixelSize: 11 * scaleFactor
        font.family: fontManager.item.oppoSansM.name
        elide: Text.ElideRight
    }

    ListView {
        id: listView
        clip: true
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8 * scaleFactor
        anchors.rightMargin: 8 * scaleFactor
        anchors.top: parent.top
        anchors.topMargin: dialog_manager.storageUnavailable ? 34 * scaleFactor : 8 * scaleFactor
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10 * scaleFactor
        spacing: 5 * scaleFactor
        currentIndex: dialog_manager.currentIndex
        model: dialog_manager.dialogInfoModel
        section.property: "historySection"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            width: listView.width
            height: 24 * scaleFactor
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 7 * scaleFactor
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3 * scaleFactor
                text: section === "pinned" ? qsTr("置顶")
                      : section === "deleted" ? qsTr("最近删除（保留 7 天）") : qsTr("最近")
                color: theme.item.colorW6
                font.pixelSize: 10 * scaleFactor
                font.bold: true
            }
        }
        onAtYEndChanged: {
            if (atYEnd && dialog_manager.historyHasMore)
                dialog_manager.loadMoreHistory()
        }

        delegate: Item {
            id: delegateItem
            width: listView.width
            height: 52 * scaleFactor
            readonly property string stableId: conversationId
            readonly property string stableTitle: title
            readonly property bool stablePinned: pinned
            readonly property bool stableDeleted: deletedAt.length > 0

            Button {
                id: rowButton
                anchors.fill: parent
                hoverEnabled: true
                enabled: dialog_manager.responseGenerated && dialog_manager.storageAvailable
                         && !stableDeleted
                Accessible.name: stableTitle

                background: Rectangle {
                    anchors.fill: parent
                    radius: 10 * scaleFactor
                    color: index === listView.currentIndex
                           ? theme.item.themeOneColor5
                           : rowButton.hovered
                             ? theme.item.themeOneColor6
                             : "transparent"
                    border.width: index === listView.currentIndex ? Math.max(1, scaleFactor) : 0
                    border.color: theme.item.themeOneColor2
                }

                contentItem: Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 9 * scaleFactor
                    anchors.right: parent.right
                    anchors.rightMargin: 70 * scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5 * scaleFactor

                    Text {
                        visible: stablePinned
                        text: "◆"
                        color: theme.item.themeZeroColor
                        font.pixelSize: 8 * scaleFactor
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                    }
                    Column {
                        width: parent.width - (stablePinned ? 15 * scaleFactor : 0)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2 * scaleFactor
                        Text {
                            width: parent.width
                            text: stableTitle
                            color: index === listView.currentIndex
                                   ? theme.item.colorB1 : theme.item.colorW5
                            font.pixelSize: 12 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                        Text {
                            width: parent.width
                            text: (conversationModel.length > 0 ? conversationModel + " · " : "")
                                  + root.formattedTime(stableDeleted ? deletedAt : updatedAt)
                            color: theme.item.colorW6
                            font.pixelSize: 9 * scaleFactor
                            elide: Text.ElideRight
                        }
                    }
                }

                onClicked: dialog_manager.selectChatById(stableId)
                ModernToolTip {
                    anchorItem: rowButton
                    visible: rowButton.hovered && stableTitle.length > 12
                    text: stableTitle
                }
            }

            Button {
                id: moreButton
                anchors.right: parent.right
                anchors.rightMargin: 6 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                width: 26 * scaleFactor
                height: 26 * scaleFactor
                hoverEnabled: true
                enabled: dialog_manager.responseGenerated
                visible: !stableDeleted && (rowButton.hovered || hovered
                                            || index === listView.currentIndex)
                Accessible.name: qsTr("会话操作")

                background: Rectangle {
                    radius: 7 * scaleFactor
                    color: moreButton.down ? theme.item.themeOneColor5
                                           : moreButton.hovered ? theme.item.colorW2 : "transparent"
                }
                contentItem: Item {
                    Row {
                        anchors.centerIn: parent
                        spacing: 2 * scaleFactor
                        Repeater {
                            model: 3
                            Rectangle {
                                width: 3 * scaleFactor
                                height: width
                                radius: width / 2
                                color: theme.item.colorW6
                            }
                        }
                    }
                }

                onClicked: {
                    root.actionConversationId = stableId
                    root.actionConversationTitle = stableTitle
                    root.actionConversationPinned = stablePinned
                    root.permanentDelete = false
                    root.cleanupExpired = false
                    actionMenu.conversationId = stableId
                    actionMenu.conversationTitle = stableTitle
                    actionMenu.pinned = stablePinned
                    var point = moreButton.mapToItem(root, moreButton.width, moreButton.height)
                    actionMenu.x = Math.max(4 * scaleFactor,
                                            Math.min(root.width - actionMenu.width - 4 * scaleFactor,
                                                     point.x - actionMenu.width))
                    actionMenu.y = Math.min(root.height - actionMenu.height - 4 * scaleFactor,
                                            point.y + 3 * scaleFactor)
                    actionMenu.open()
                }
            }

            Button {
                id: conversationExportButton
                objectName: "conversationExportButton"
                anchors.right: parent.right
                anchors.rightMargin: 38 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                width: 26 * scaleFactor
                height: 26 * scaleFactor
                visible: !stableDeleted
                opacity: rowButton.hovered || hovered || index === listView.currentIndex ? 1 : 0.72
                hoverEnabled: true
                Accessible.name: qsTr("导出此会话")
                background: Rectangle {
                    radius: 7 * scaleFactor
                    color: conversationExportButton.down ? theme.item.themeOneColor5
                           : conversationExportButton.hovered ? theme.item.themeOneColor6
                           : "transparent"
                    border.width: conversationExportButton.activeFocus ? Math.max(1, scaleFactor) : 0
                    border.color: theme.item.themeZeroColor
                }
                contentItem: Item {
                    Canvas {
                        anchors.centerIn: parent
                        width: 16 * scaleFactor
                        height: 16 * scaleFactor
                        property color iconColor: conversationExportButton.hovered
                                                  ? theme.item.themeZeroColor : theme.item.colorW6
                        onIconColorChanged: requestPaint()
                        Component.onCompleted: requestPaint()
                        onPaint: {
                            var context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            context.strokeStyle = iconColor
                            context.lineWidth = Math.max(1.4, 1.5 * scaleFactor)
                            context.lineCap = "round"
                            context.lineJoin = "round"
                            context.beginPath()
                            context.moveTo(width * 0.5, height * 0.12)
                            context.lineTo(width * 0.5, height * 0.62)
                            context.moveTo(width * 0.28, height * 0.43)
                            context.lineTo(width * 0.5, height * 0.66)
                            context.lineTo(width * 0.72, height * 0.43)
                            context.moveTo(width * 0.18, height * 0.84)
                            context.lineTo(width * 0.82, height * 0.84)
                            context.stroke()
                        }
                    }
                }
                ModernToolTip {
                    anchorItem: conversationExportButton
                    visible: conversationExportButton.hovered
                    text: qsTr("导出此会话为 Markdown")
                }
                onClicked: {
                    root.pendingExportId = stableId
                    exportFolderDialog.open()
                }
            }

            Button {
                anchors.right: parent.right
                anchors.rightMargin: 31 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                width: 27 * scaleFactor
                height: 24 * scaleFactor
                visible: stableDeleted
                text: qsTr("恢复")
                Accessible.name: qsTr("恢复会话")
                onClicked: dialog_manager.restoreChatById(stableId)
            }

            Button {
                anchors.right: parent.right
                anchors.rightMargin: 2 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                width: 27 * scaleFactor
                height: 24 * scaleFactor
                visible: stableDeleted
                text: qsTr("删")
                Accessible.name: qsTr("永久删除会话")
                onClicked: {
                    root.actionConversationId = stableId
                    root.actionConversationTitle = stableTitle
                    root.permanentDelete = true
                    root.cleanupExpired = false
                    deletePopup.open()
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            contentItem: Rectangle {
                visible: listView.contentHeight > listView.height
                implicitWidth: 4 * scaleFactor
                color: "#D7E0F3"
                radius: 2 * scaleFactor
            }
        }
    }

    Column {
        anchors.centerIn: listView
        visible: dialog_manager.dialogInfoModel.rowCount() === 0
                 && dialog_manager.storageAvailable
        spacing: 5 * scaleFactor
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("暂无历史会话")
            color: theme.item.colorW6
            font.pixelSize: 12 * scaleFactor
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("开始对话后会显示在这里")
            color: theme.item.colorW5
            font.pixelSize: 10 * scaleFactor
        }
    }

    MorePopup {
        id: actionMenu
        onPinBtnClicked: {
            if (dialog_manager.setChatPinned(conversationId, !pinned))
                close()
        }
        onRenameBtnClicked: {
            close()
            renameField.text = conversationTitle
            renamePopup.open()
            renameField.forceActiveFocus()
            renameField.selectAll()
        }
        onDeleteBtnClicked: {
            close()
            deletePopup.open()
        }
    }

    Popup {
        id: renamePopup
        width: 270 * scaleFactor
        height: 132 * scaleFactor
        x: (root.width - width) / 2
        y: 72 * scaleFactor
        padding: 14 * scaleFactor
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: theme.item.colorW1
            radius: 14 * scaleFactor
            border.color: theme.item.colorW3
        }
        contentItem: Column {
            spacing: 10 * scaleFactor
            Text {
                text: qsTr("重命名会话")
                color: theme.item.colorB1
                font.bold: true
                font.pixelSize: 14 * scaleFactor
                font.family: fontManager.item.oppoSansM.name
            }
            TextField {
                id: renameField
                width: 242 * scaleFactor
                height: 36 * scaleFactor
                maximumLength: 40
                selectByMouse: true
                onAccepted: renameSave.clicked()
                background: Rectangle {
                    radius: 9 * scaleFactor
                    color: theme.item.colorW2
                    border.color: renameField.activeFocus
                                  ? theme.item.themeZeroColor : theme.item.colorW3
                }
            }
            Row {
                x: parent.width - width
                spacing: 8 * scaleFactor
                DialogButton { text: qsTr("取消"); onClicked: renamePopup.close() }
                DialogButton {
                    id: renameSave
                    text: qsTr("保存")
                    primary: true
                    enabled: renameField.text.trim().length > 0
                    onClicked: {
                        if (dialog_manager.renameChatById(root.actionConversationId,
                                                          renameField.text))
                            renamePopup.close()
                    }
                }
            }
        }
    }

    Popup {
        id: deletePopup
        objectName: "deleteConversationConfirm"
        parent: Overlay.overlay
        width: Math.min(292 * scaleFactor, parent.width - 32 * scaleFactor)
        height: 184 * scaleFactor
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        padding: 16 * scaleFactor
        modal: true
        focus: true
        // Escape and a press on the dimmed area both mean "cancel". The
        // modal overlay consumes that press, so it can never reach a chat row.
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: cancelDeleteButton.forceActiveFocus()

        Overlay.modal: Rectangle {
            color: uiPreferences.overlayColor
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: 150; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80; easing.type: Easing.InCubic }
        }

        background: Rectangle {
            color: theme.item.colorW1
            radius: 16 * scaleFactor
            border.color: theme.item.colorW3
            border.width: Math.max(1, scaleFactor)
        }
        contentItem: Column {
            width: deletePopup.availableWidth
            spacing: 0

            Row {
                width: parent.width
                height: 38 * scaleFactor
                spacing: 11 * scaleFactor

                Rectangle {
                    width: 36 * scaleFactor
                    height: width
                    radius: width / 2
                    color: uiPreferences.errorSurfaceColor

                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        color: uiPreferences.dangerColor
                        font.bold: true
                        font.pixelSize: 17 * scaleFactor
                        font.family: fontManager.item.oppoSansM.name
                    }
                }

                Text {
                    width: parent.width - 47 * scaleFactor
                    height: parent.height
                    text: root.cleanupExpired ? qsTr("清理过期会话？")
                          : root.permanentDelete ? qsTr("永久删除会话？") : qsTr("移到最近删除？")
                    color: theme.item.colorB1
                    font.bold: true
                    font.pixelSize: 15 * scaleFactor
                    font.family: fontManager.item.oppoSansM.name
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Text {
                width: parent.width
                height: 27 * scaleFactor
                topPadding: 5 * scaleFactor
                text: "“" + root.actionConversationTitle + "”"
                color: theme.item.colorB1
                elide: Text.ElideRight
                maximumLineCount: 1
                font.pixelSize: 13 * scaleFactor
                font.family: fontManager.item.oppoSansM.name
            }

            Text {
                width: parent.width
                height: 38 * scaleFactor
                text: root.permanentDelete
                      ? qsTr("此操作会永久删除消息且无法恢复。")
                      : qsTr("会话将保留 7 天，可在“最近删除”中恢复。")
                color: theme.item.colorW6
                wrapMode: Text.WordWrap
                font.pixelSize: 12 * scaleFactor
                font.family: fontManager.item.oppoSansM.name
            }

            Item {
                width: parent.width
                height: 41 * scaleFactor

                Row {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 8 * scaleFactor

                    DialogButton {
                        id: cancelDeleteButton
                        text: qsTr("取消")
                        Accessible.name: qsTr("取消删除")
                        onClicked: deletePopup.close()
                    }
                    DialogButton {
                        text: root.permanentDelete ? qsTr("永久删除") : qsTr("移到最近删除")
                        destructive: true
                        Accessible.name: qsTr("确认删除会话")
                        onClicked: {
                            var succeeded = root.cleanupExpired
                                    ? dialog_manager.purgeExpiredDeletedHistory()
                                    : root.permanentDelete
                                      ? dialog_manager.purgeDeletedChatById(root.actionConversationId)
                                      : dialog_manager.deleteChatById(root.actionConversationId)
                            if (succeeded)
                                deletePopup.close()
                        }
                    }
                }
            }
        }
    }
}
