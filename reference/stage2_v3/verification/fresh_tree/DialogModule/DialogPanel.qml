import QtQuick 2.15
import QtQuick.Controls 2.15
import "../Controls"

Rectangle {
    id: root

    // 自适应窗口的 DPI 缩放
    property real scaleFactor: window.scale

    // 以便调试listView布局
    // Rectangle {
    //     anchors.fill: listView
    //     color: "#F4F6F7"
    // }

    Image {
        id: backgroundImage
        visible: dialog_manager.isChatEmpty
        width: 180 * scaleFactor
        height: 180 * scaleFactor
        anchors.centerIn: parent
        source: "qrc:/Image/Icon/empty.svg"
    }

    ListView {
        id: listView
        clip: true
        width: (526 - 6 * 2)  * scaleFactor// 以便定义滑动条位置
        height: 476 * scaleFactor
        anchors.centerIn: parent
        model: dialog_manager.dialogModel
        orientation: ListView.Vertical
        spacing: 26

        property bool autoScroll: true
        property int m_contentHeight: -1

        onContentHeightChanged: {
            if(listView.autoScroll) {
                listView.positionViewAtEnd()

                listView.m_contentHeight = listView.contentY + listView.height
            }
        }

        onMovementStarted: {
            listView.autoScroll = false // 用户滑动列表时，该标志位为false
        }

        Connections {
            target: dialog_manager.dialogModel

            function onAppended() {
                listView.positionViewAtEnd()
                listView.autoScroll = true
            }
        }

        delegate: Component {

                Item {
                    id: cellItem
                    width: listView.width
                    height: sender === 0
                            ? (userBox.height)
                            : (asstBox.height)

                    Button {
                        id: userBtn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: userBox.height
                        visible: sender === 0

                        background: Rectangle {
                            id: userRect
                            anchors.fill: parent

                            Rectangle {
                                id: userBox
                                anchors.right: parent.right
                                anchors.rightMargin: 15 * scaleFactor
                                width: userContent.m_contentWidth + (12 * 2) * scaleFactor > (486 - 36) * scaleFactor
                                       ? (486 - 36) * scaleFactor
                                       : (userContent.m_contentWidth + (12 * 2) * scaleFactor)
                                height: userContent.contentHeight + (8 * 2) * scaleFactor
                                color: theme.item.themeOneColor6
                                radius: 4 * scaleFactor

                                SelectableText {
                                    id: userContent
                                    width: (userBox.width - (12 * 2) * scaleFactor)
                                    anchors.centerIn: parent
                                    text: model.content
                                }
                            }

                            Button {
                                id: copyBtn_user
                                visible: userBtn.hovered
                                anchors.right: userBox.left
                                anchors.rightMargin: 8 * scaleFactor
                                anchors.verticalCenter: userBox.verticalCenter
                                width: 20 * scaleFactor
                                height: 20 * scaleFactor

                                background: Image {
                                    anchors.fill: parent
                                    source: copyBtn_user.enabled
                                            ? copyBtn_user.pressed
                                              ? "qrc:/Image/Icon/copy_pressed.svg"
                                              : copyBtn_user.hovered
                                                ? "qrc:/Image/Icon/copy_hovered.svg"
                                                : "qrc:/Image/Icon/copy_normal.svg"
                                            : "qrc:/Image/Icon/copy_disabled.svg"
                                }

                                onClicked: {
                                    dialog_manager.copy(index)
                                }

                                ToolTip {
                                    visible: copyBtn_user.hovered
                                    x: - (width - copyBtn_user.width)/2
                                    y: copyBtn_user.height + 4
                                    width: (copyText_user.contentWidth + 6 * 2) * scaleFactor
                                    height: 25 * scaleFactor
                                    delay: 500

                                    background: Rectangle {
                                        id: copyRect_user
                                        anchors.fill: parent
                                        radius: 5
                                        color: "black"

                                        Text {
                                            id: copyText_user
                                            text: qsTr("Copy")
                                            color: "white"
                                            anchors.centerIn: parent
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            font.bold: true
                                            font.pixelSize: 12 * scaleFactor
                                            font.family: fontManager.item.oppoSansM.name
                                        }
                                    }
                                }
                            } // Button: copyBtn_user
                        } // background: Rectangle
                    } // Button: userBtn

                    Button {
                        id: asstBtn
                        visible: sender === 1
                        anchors.left: parent.left
                        anchors.leftMargin: 14 * scaleFactor
                        width: 486
                        height: {
                            if(reasoningRect.visible) {
                                if(reasoningContent.visible) {
                                    if(reference.visible) {
                                        return (reasoningRect.height
                                                + reasoningContent.contentHeight
                                                + content.contentHeight
                                                + reference.contentHeight
                                                + asstColumn.spacing * 3
                                                + 28 * scaleFactor)
                                    } else {
                                        return (reasoningRect.height
                                                + reasoningContent.contentHeight
                                                + content.contentHeight
                                                + asstColumn.spacing * 2
                                                + 28 * scaleFactor)
                                    }
                                } else {
                                    if(reference.visible) {
                                        return (reasoningRect.height
                                                + content.contentHeight
                                                + reference.contentHeight
                                                + asstColumn.spacing * 2
                                                + 28 * scaleFactor)
                                    } else {
                                        return (reasoningRect.height
                                                + content.contentHeight
                                                + asstColumn.spacing
                                                + 28 * scaleFactor)
                                    }
                                }
                            } else {
                                if(reference.visible) {
                                    return (reference.contentHeight
                                            + content.contentHeight
                                            + asstColumn.spacing
                                            + 28 * scaleFactor)
                                } else {
                                    return content.contentHeight + 28 * scaleFactor
                                }
                            }
                        }

                        background: Rectangle {
                            id: asstBox
                            anchors.fill: parent

                            Column {
                                id: asstColumn
                                spacing: 8 * scaleFactor

                                Rectangle {
                                    id: reasoningRect
                                    visible: model.reasonModeActive
                                    width: reasoningImage.width + reasoningText.contentWidth + 8 * 2 + 4
                                    height: 34
                                    color: theme.item.colorW2
                                    radius: 8
                                    anchors.left: parent.left

                                    Image {
                                        id: reasoningImage
                                        width: 16
                                        height: 16
                                        anchors.left: parent.left
                                        anchors.leftMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        source: "qrc:/Image/Icon/reason_normal.svg"
                                    }

                                    Text {
                                        id: reasoningText
                                        text: index === listView.count - 1 && !dialog_manager.responseGenerated
                                              ? qsTr("Reasoning...")
                                              : qsTr("Reasoned")
                                        anchors.left: reasoningImage.right
                                        anchors.leftMargin: 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: theme.item.colorB1
                                        font.pixelSize: 14
                                        font.family: fontManager.item.oppoSansM.name
                                    }
                                }

                                Rectangle {
                                    id: reasoningContentRect
                                    visible: model.reasoningContent !== ""
                                    width: asstBox.width
                                    height: reasoningContent.contentHeight

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        width: 1 * scaleFactor
                                        height: parent.height
                                        color: theme.item.colorW5
                                    }

                                    SelectableText {
                                        id: reasoningContent
                                        anchors.left: parent.left
                                        anchors.leftMargin: 10 * scaleFactor
                                        width: (parent.width - 10) * scaleFactor
                                        text: model.reasoningContent
                                        color: theme.item.colorW5

                                        textFormat: Text.RichText // 富文本显示content
                                    }
                                }

                                SelectableText {
                                    id: content
                                    width: asstBox.width * scaleFactor
                                    text: model.content

                                    textFormat: Text.RichText
                                }

                                SelectableText {
                                    id: reference
                                    // 回答包含引用且所在回答生成完毕后显示
                                    visible: model.reference !== "" &&
                                             (index === listView.count - 1
                                              ? dialog_manager.responseGenerated
                                              : true)
                                    width: asstBox.width
                                    text: model.reference

                                    textFormat: Text.RichText

                                    color: theme.item.themeZeroColor
                                }

                                Row {
                                    id: moreRow
                                    width: parent.width
                                    height: 20 * scaleFactor
                                    spacing: 8 * scaleFactor
                                    visible: asstBtn.hovered

                                    Button {
                                        id: copyBtn_asst
                                        width: 20 * scaleFactor
                                        height: 20 * scaleFactor
                                        visible: index === listView.count - 1
                                                 ? dialog_manager.responseGenerated
                                                 : true

                                        background: Image {
                                            anchors.fill: parent
                                            source: copyBtn_asst.enabled
                                                    ? copyBtn_asst.pressed
                                                      ? "qrc:/Image/Icon/copy_pressed.svg"
                                                      : copyBtn_asst.hovered
                                                        ? "qrc:/Image/Icon/copy_hovered.svg"
                                                        : "qrc:/Image/Icon/copy_normal.svg"
                                                    : "qrc:/Image/Icon/copy_disabled.svg"
                                        }

                                        onClicked: {
                                            dialog_manager.copy(index)
                                        }

                                        ToolTip {
                                            visible: copyBtn_asst.hovered
                                            x: -(width - copyBtn_asst.width)/2
                                            y: copyBtn_asst.height + 4
                                            width: (copyText_asst.contentWidth + 6 * 2) * scaleFactor
                                            height: 25 * scaleFactor
                                            delay: 500

                                            background: Rectangle {
                                                id: copyRect_asst
                                                anchors.fill: parent
                                                radius: 5
                                                color: "black"

                                                Text {
                                                    id: copyText_asst
                                                    text: qsTr("Copy")
                                                    color: "white"
                                                    anchors.centerIn: parent
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                    font.bold: true
                                                    font.pixelSize: 12 * scaleFactor
                                                    font.family: fontManager.item.oppoSansM.name
                                                }
                                            }
                                        }
                                    } // Button: copyBtn_asst

                                    Button {
                                        id: regenerateBtn
                                        width: 20 * scaleFactor
                                        height: 20 * scaleFactor
                                        enabled: window.allowQuestion
                                        visible: index === listView.count - 1 && dialog_manager.responseGenerated

                                        background: Image {
                                            anchors.fill: parent
                                            source: regenerateBtn.enabled
                                                    ? regenerateBtn.pressed
                                                      ? "qrc:/Image/Icon/regenerate_pressed.svg"
                                                      : regenerateBtn.hovered
                                                        ? "qrc:/Image/Icon/regenerate_hovered.svg"
                                                        : "qrc:/Image/Icon/regenerate_normal.svg"
                                                    : "qrc:/Image/Icon/regenerate_disabled.svg"
                                        }

                                        onClicked: {
                                            dialog_manager.regenerate()
                                        }

                                        ToolTip {
                                            visible: regenerateBtn.hovered
                                            x: - (width - regenerateBtn.width)/2
                                            y: regenerateBtn.height + 4
                                            width: (regenerateText.contentWidth + 6 * 2)
                                            height: 25
                                            delay: 500

                                            background: Rectangle {
                                                id: regenerateRect
                                                anchors.fill: parent
                                                radius: 5
                                                color: "black"

                                                Text {
                                                    id: regenerateText
                                                    text: qsTr("Regenerate")
                                                    color: "white"
                                                    anchors.centerIn: parent
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                    font.bold: true
                                                    font.pixelSize: 12 * scaleFactor
                                                    font.family: fontManager.item.oppoSansM.name
                                                }
                                            }
                                        }
                                    } // Button: regenerateBtn
                                } // Row: moreRow
                            } // Column: asstColumn
                        } // background: Rectangle
                    } // Button: asstBtn

                } // Item
            } // delegate: Component

        ScrollBar.vertical: ScrollBar {
            // policy: ScrollBar.AsNeeded未实际生效
            contentItem: Rectangle {
                visible: listView.m_contentHeight > listView.height && !backgroundImage.visible
                implicitWidth: 4 * scaleFactor // 仅指定width该控件无法显示
                color: "#D7E0F3"
                radius: 18
            }
        }
    }
}
