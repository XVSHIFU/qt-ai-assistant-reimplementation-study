import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    // 自适应窗口的 DPI 缩放
    property real scaleFactor: window.scale

    Image {
        id: icon
        width: 20 * scaleFactor
        height: 20 * scaleFactor
        anchors.top: parent.top
        anchors.topMargin: 16 * scaleFactor
        anchors.left: parent.left
        anchors.leftMargin: 16 * scaleFactor
        source: "qrc:/Image/Icon/chatHistory.svg"
    }

    Text {
        anchors.verticalCenter: icon.verticalCenter
        anchors.left: icon.right
        anchors.leftMargin: 6 * scaleFactor
        text: qsTr("History")
        font.pixelSize: 14 * scaleFactor
        font.family: fontManager.item.oppoSansM.name
        color: theme.item.colorB1
    }

    // 以便调试listView布局
    // Rectangle {
    //     anchors.fill: listView
    //     color: "#F4F6F7"
    // }

    ListView {
        id: listView
        clip: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 46 * scaleFactor
        width: (126 - 4 * 2) * scaleFactor
        height: (590 - 46 - 52) * scaleFactor
        spacing: 4 * scaleFactor

        currentIndex: dialog_manager.currentIndex
        model: dialog_manager.dialogInfoModel

        delegate: Item {
            id: delegateItem
            width: 106 * scaleFactor
            height: 30 * scaleFactor
            x: 4 * scaleFactor

            Button {
                id: btn
                anchors.fill: parent

                background: Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: index === listView.currentIndex
                           ? theme.item.themeOneColor5
                           : (btn.hovered || moreBtn.hovered)
                             ? theme.item.themeOneColor6
                             : "transparent"
                }

                onClicked: {
                    // 已开启新对话且回答未生成时不响应
                    if(dialog_manager.dialogModel.rowCount()) {
                        if(!dialog_manager.responseGenerated) {
                            return
                        }
                    }

                    dialog_manager.currentIndex = index

                    dialog_manager.dialogModel.clearAll()
                    dialog_manager.parseJSONToCurrentChat()
                }

                ToolTip {
                    visible: btn.hovered
                    x: 0
                    y: btn.height + 4
                    width: promptText.width + 6 * 2 * scaleFactor
                    height: promptText.contentHeight
                    delay: 500

                    background: Rectangle {
                        id: promptRect
                        anchors.fill: parent
                        radius: 5
                        color: "black"

                        Text {
                            id: promptText
                            text: title
                            color: "white"
                            anchors.centerIn: parent
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            font.bold: true
                            font.pixelSize: 12 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name

                            width: contentWidth > (300 - 6 * 2) * scaleFactor
                                   ? (300 - 6 * 2) * scaleFactor
                                   : contentWidth
                          //  wrapMode: Text.WordWrap
                        }
                    }
                }
            } // Button

            Button {
                id: moreBtn
                anchors.right: btn.right
                anchors.verticalCenter: btn.verticalCenter
                width: 16 * scaleFactor
                height: 16 * scaleFactor
                visible: btn.hovered || moreBtn.hovered

                background: Image {
                    anchors.fill: parent
                    source: moreBtn.pressed
                            ? "qrc:/Image/Icon/more_pressed.svg"
                            : moreBtn.hovered
                              ? "qrc:/Image/Icon/more_hovered.svg"
                              : "qrc:/Image/Icon/more_normal.svg"
                }

                onClicked: {
                    morePopup.index = index

                    morePopup.x = moreBtn.x - (morePopup.width - moreBtn.width)
                    morePopup.y = (delegateItem.y - listView.contentY) + delegateItem.height + 4
                    morePopup.open()
                }
            }

            TextField {
                id: titleTextField
                visible: false
                anchors.fill: parent
                width: 106 * scaleFactor
                height: 30 * scaleFactor

                background: Rectangle {
                    anchors.fill: parent
                    color: theme.item.colorW1
                    border.color: theme.item.themeOneColor2
                    radius: 8
                }

                color: theme.item.colorB1
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14 * scaleFactor
                padding: 4 * scaleFactor
                horizontalAlignment: Text.AlignLeft

                selectionColor: "#3367D1"
                selectedTextColor: "white"

                text: ""
                onEditingFinished: {
                    dialog_manager.modifyTitleAtIndex(titleTextField.text, index)
                    titleText.text = titleTextField.text

                    titleText.visible = true
                    titleTextField.visible = false
                }

                onTextChanged: {
                    if(titleTextField.text.length > 33) {
                        titleTextField.text = titleTextField.text.slice(0, 33)
                    }
                }
            }

            Text {
                id: titleText
                anchors.left: parent.left
                anchors.leftMargin: 4 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                width: 82 * scaleFactor
                text: title
                color: index === listView.currentIndex
                       ? theme.item.colorB1
                       : theme.item.colorW5
                font.pixelSize: 14 * scaleFactor
                font.family: fontManager.item.oppoSansM.name
                horizontalAlignment: Text.AlignLeft

                elide: Text.ElideRight
                maximumLineCount: 1 // 否则elide属性无法生效

                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: {
                        titleTextField.text = titleText.text

                        titleText.visible = false
                        titleTextField.visible = true
                        titleTextField.forceActiveFocus()
                        titleTextField.selectAll()
                    }

                    onClicked: {
                        // 已开启新对话且回答未生成时不响应
                        if(dialog_manager.dialogModel.rowCount()) {
                            if(!dialog_manager.responseGenerated) {
                                return
                            }
                        }

                        dialog_manager.currentIndex = index

                        dialog_manager.dialogModel.clearAll()
                        dialog_manager.parseJSONToCurrentChat()
                    }
                }
            }

            Connections {
                target: morePopup

                function onRenameBtnClicked() {
                    if(index === morePopup.index) {
                        morePopup.close()
                        titleTextField.text = titleText.text

                        titleText.visible = false
                        titleTextField.visible = true
                        titleTextField.forceActiveFocus()
                        titleTextField.selectAll()
                    }
                }
            } // Connections
        } // Item

        MorePopup {
            id: morePopup
            visible: false

            onDeleteBtnClicked: {
                morePopup.close()

                dialog_manager.deleteChatAtIndex(morePopup.index)
            }
        }

        ScrollBar.vertical: ScrollBar {

            contentItem: Rectangle {
                visible: listView.contentHeight > listView.height
                implicitWidth: 4 * scaleFactor // 仅指定width该控件无法显示
                color: "#D7E0F3"
                radius: 18
            }
        }
    }
}
