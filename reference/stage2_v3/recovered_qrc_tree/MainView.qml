import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.12
import "Component"
import "Controls"
import "DialogModule"
import "qrc:/GraphicalEffects"
Item {
    id:root
    width: window.width
    height: window.height

    // 自适应窗口的 DPI 缩放
    property real scaleFactor: window.scale

    onScaleFactorChanged: {
            //console.log("缩放比:", scaleFactor)
            //console.log(root.width,root.height,window.width,window.height,window.x,window.y)
            //console.log(textEdit.contentHeight,root.lineContentHeight)
        }

    property bool openHistory: false
    property int lineContentHeight: 27 * scaleFactor //单行内容高度
    property int lineHeight: 42 * scaleFactor  //单行高度
    property int textLineHeight: 22 * scaleFactor//输入框单行高度

    property int contentBoxHeight: 168 * scaleFactor//内容框高度 (仅包含内容)
    property int textAreaHeight: 212 * scaleFactor//整个文本框高度

    property int curwidth:576
    property int curHeight:82
    property int tmpValue:0
    // onActiveFocusChanged: {
    //         if (activeFocus) {
    //             root.z = 1 // 将 Item 置于上层
    //         } else {
    //             root.z = 0 // 恢复默认 Z 值
    //         }
    //     }

    //加载主题
    Loader {
        id: theme
        source: "qrc:/Theme/LightTheme.qml"
        onLoaded: {
            // 将主题管理器设置为全局属性
            Qt.application.themeManager = item
        }
    }

    //加载字体
    Loader {
        id: fontManager
        source: "qrc:/FontManager.qml"
        onLoaded: {
            // 将字体管理器设置为全局属性
            Qt.application.fontManager = item
        }
    }

    Rectangle {
        id: icon
        anchors.left: parent.left
        anchors.leftMargin: (openHistory ? 84: 0) * scaleFactor
        anchors.top: textRect.top
        radius: 125
        width: 42 * scaleFactor
        height: 42 * scaleFactor
        border.color: theme.item.colorW3
        border.width: 1 * scaleFactor

        Image {
           width: 40 * scaleFactor
           height: 40 * scaleFactor
           source: "qrc:/Image/Icon/DeepSeek.png"
           anchors.centerIn: parent
        }
    }

    Rectangle {
        id:pullup
        width: 16 * scaleFactor
        height: 16 * scaleFactor
        anchors.bottom: textRect.top
        anchors.bottomMargin: 8 * scaleFactor
        anchors.horizontalCenter: textRect.horizontalCenter
        color: theme.item.colorW3

        border.color: theme.item.colorW3
        border.width: 1 * scaleFactor
        radius: 36

        Image {
            anchors.fill: parent
            source: "qrc:/Image/Icon/pullUp.svg"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if(!dialog.visible)     
                {
                    //为上方创造空间
                    tmpValue=(textEdit.contentHeight > root.lineContentHeight) ? 212 : 42
                    curHeight=tmpValue + 594 + 5

                    window.setXandY(window.x,window.y + reasonNoPullBtn.height + 8 * scaleFactor - dialog.height - 5 * scaleFactor, curwidth * scaleFactor,curHeight * scaleFactor)
                    dialog.visible = true
                    window.setDialog(true)  //改变拖拽初始坐标
                  //  icon.height = 42
                }
            }
        }
    }

    ButtonGroup { id: radioGroup4 }

    RadioButton {
        id: reasonNoPullBtn
        height: 32 * scaleFactor
        width: (6 * 3 + 20) * scaleFactor + reasonText.contentWidth
        anchors.left: textRect.left
        anchors.bottom: textRect.top
        anchors.bottomMargin: 8 * scaleFactor
        checked: dialog_manager.reasonModeActive
        ButtonGroup.group: radioGroup4

        background: Rectangle {
            anchors.fill: parent
            color: reasonNoPullBtn.checked
                   ? theme.item.themeZeroColor
                   : reasonNoPullBtn.hovered
                     ? theme.item.colorW2
                     : theme.item.colorW2
            opacity: reasonNoPullBtn.checked ? 1 : 0.7
            radius: 38
            border.width: 1 * scaleFactor
            border.color: reasonNoPullBtn.checked
                          ? theme.item.themeZeroColor
                          : reasonNoPullBtn.hovered
                            ? theme.item.colorW6
                            : theme.item.colorW6

            Row {
                spacing: 6 * scaleFactor
                anchors.centerIn: parent
                Image {
                    width: 20 * scaleFactor
                    height: 20 * scaleFactor
                    source: reasonBtn.checked
                            ? "qrc:/Image/Icon/reason_pressed.svg"
                            : reasonBtn.hovered
                              ? "qrc:/Image/Icon/reason_normal.svg"
                              : "qrc:/Image/Icon/reason_normal.svg"
                }

                Text {
                    id: reasonNoPullText
                    text: qsTr("Reason(R1)")
                    color: reasonNoPullBtn.checked
                           ? theme.item.colorW1
                           : reasonNoPullBtn.hovered
                             ? theme.item.colorB1
                             : theme.item.colorW6
                    font.pixelSize: 14 * scaleFactor
                    font.family: fontManager.item.oppoSansM.name
                }
            }
        }

        indicator: Rectangle {
            visible: false
        }

        onClicked: {
            dialog_manager.reasonModeActive = !dialog_manager.reasonModeActive
        }
    }

    ButtonGroup { id: radioGroup3 }
    RadioButton {
        id: searchNoPullBtn
        height: 32 * scaleFactor
        width: (6 * 3 + 20) * scaleFactor + searchText.contentWidth
        anchors.left: reasonNoPullBtn.right
        anchors.leftMargin: 8 * scaleFactor
        anchors.bottom: textRect.top
        anchors.bottomMargin: 8 * scaleFactor
        checked: dialog_manager.searchModeActive
        ButtonGroup.group: radioGroup3

        background: Rectangle {
            anchors.fill: parent
            color: searchNoPullBtn.checked
                   ? theme.item.themeZeroColor
                   : searchNoPullBtn.hovered
                     ? theme.item.colorW2
                     : theme.item.colorW2
            opacity: searchNoPullBtn.checked ? 1 : 0.7
            border.color: searchNoPullBtn.checked
                          ? theme.item.themeZeroColor
                          : searchNoPullBtn.hovered
                            ? theme.item.colorW6
                            : theme.item.colorW6
            radius: 38

            Row {
                spacing: 6 * scaleFactor
                anchors.centerIn: parent
                Image {
                    width: 20 * scaleFactor
                    height: 20 * scaleFactor
                    source: searchNoPullBtn.checked
                            ? "qrc:/Image/Icon/search_pressed.svg"
                            : searchNoPullBtn.hovered
                              ? "qrc:/Image/Icon/search_normal.svg"
                              : "qrc:/Image/Icon/search_normal.svg"
                }

                Text {
                    id: searchNoPullText
                    text: qsTr("Search")
                    color: searchNoPullBtn.checked
                           ? theme.item.colorW1
                           : searchNoPullBtn.hovered
                             ? theme.item.colorB1
                             : theme.item.colorW6
                    font.pixelSize: 14 * scaleFactor
                    font.family: fontManager.item.oppoSansM.name
                }
            }
        }

        indicator: Rectangle {
            visible: false
        }

        onClicked: {
            dialog_manager.searchModeActive = !dialog_manager.searchModeActive
        }
    }

    Rectangle {
        id :textRect
        color: "white"
        radius: 20
        width: 526 * scaleFactor
        border.color: theme.item.colorW3
        border.width: 1 * scaleFactor
        height: (textEdit.contentHeight > 27 * scaleFactor) ? root.textAreaHeight : root.lineHeight
        anchors.left: icon.right
        anchors.leftMargin: 8 * scaleFactor
        anchors.bottom: parent.bottom

        ScrollView {
            id: scrollView
            width: 486 * scaleFactor
            height: root.contentBoxHeight
            anchors.left: parent.left
            anchors.leftMargin: 8 * scaleFactor
            anchors.top: parent.top
            anchors.topMargin: 10 * scaleFactor

            TextArea {
                id: textEdit
                width: 486 * scaleFactor
                height: (textEdit.contentHeight > 27 * scaleFactor) ? root.contentBoxHeight : root.textLineHeight
                wrapMode: TextArea.Wrap
                placeholderText: (!window.allowQuestion ? qsTr("Please connect the device first and activate the AI ​​function")
                                    : (window.notEnough ? qsTr("The balance is insufficient, please top up as soon as possible") : qsTr("Please enter your content,Enter Send,Shift+Enter Line Break")))
                placeholderTextColor: window.allowQuestion ? theme.item.colorW4 : "red"
                font.pixelSize: 14 * scaleFactor
                padding: 2 * scaleFactor  // 内边距2
                font.family: fontManager.item.oppoSansM.name
                selectionColor: theme.item.themeOneColor6
                selectByMouse: true
                color: theme.item.colorB1

                onContentHeightChanged: {
                    //console.log(contentHeight,lineContentHeight,lineHeight)

                    if(contentHeight > 27 * scaleFactor)
                    {
                        if(dialog.visible )
                        {
                            curHeight=212 + 594 + 5
                            window.setXandY(window.x,window.y ,curwidth * scaleFactor,curHeight * scaleFactor) //文本框+聊天框+间距
                        }
                        else
                        {
                            curHeight=212 + 32 + 8
                            window.setXandY(window.x,window.y,curwidth * scaleFactor,curHeight * scaleFactor)
                        }
                    }
                    else if(contentHeight <= root.lineContentHeight)
                    {
                        if(dialog.visible)
                        {
                           curHeight=42 + 594 + 5
                           window.setXandY(window.x,window.y ,curwidth * scaleFactor,curHeight * scaleFactor) //单行文本框+聊天框+间距
                        }
                        else
                        {
                           curHeight=42 + 32 + 8
                           window.setXandY(window.x,window.y,curwidth * scaleFactor,curHeight * scaleFactor) //恢复单行高度 + 上拉框 + 间距高度
                        }
                    }
                }
                Keys.onPressed:  (event) => {
                        if (event.key  === Qt.Key_Return || event.key  === Qt.Key_Enter) {
                            if (event.modifiers  & Qt.ShiftModifier) {
                                // Shift+Enter：手动插入换行
                                textEdit.insert(textEdit.cursorPosition,  "\n");
                            } else {
                                // 单独按下Enter：触发发送信号
                                send.clicked();
                                event.accepted  = true;  // 阻止默认换行行为
                            }
                        } else {
                            event.accepted  = false;  // 其他按键按默认处理
                        }
                    }
            }
        }

        Button {
            id: send
            width: 34 * scaleFactor
            height: 34 * scaleFactor
            anchors.right: parent.right
            anchors.rightMargin: 6 * scaleFactor
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4 * scaleFactor
            z: 2
            enabled: window.allowQuestion

            background: Image {
                anchors.fill: parent
                source: send.enabled
                        ? send.pressed
                          ? "qrc:/Image/Icon/send_pressed.svg"
                          : send.hovered
                            ? "qrc:/Image/Icon/send_hovered.svg"
                            : "qrc:/Image/Icon/send_normal.svg"
                        : "qrc:/Image/Icon/send_disabled.svg"
            }

            ToolTip {
                visible: send.hovered
                width: 40 * scaleFactor
                height: 24 * scaleFactor
                delay: 500

                background: Rectangle {
                    id: top_popup_rect
                    width: parent.width
                    height: parent.height
                    anchors.right: send.left
                    anchors.leftMargin: 20 * scaleFactor
                    anchors.top: send.top
                    color: "black"

                    Text {
                        id: top_popup_text
                        text: qsTr("发送")
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

            onClicked: {
                if(textEdit.text === "" || !window.allowQuestion)
                    return
                // 已开启新对话且回答未生成时无法点击发送按钮
                if(dialog_manager.dialogModel.rowCount()) {
                    if(!dialog_manager.responseGenerated) {
                        return
                    }
                }

                dialog_manager.sendMessage(textEdit.text);
                textEdit.text = ""

                window.setDialog(true)  //改变拖拽初始坐标
                //console.log(dialog.visible)

                if(!dialog.visible)
                {
                    window.height = root.lineHeight + 32 + 8  //恢复初始搜索框高度 + 上拉框 + 间距
                    //为上方创造空间

                    window.setXandY(window.x,window.y + 32 + 8 - dialog.height - 5, curwidth* scaleFactor,window.height - 32 - 8 + dialog.height + 5)
                    dialog.visible = true
                    icon.height = 42
                }
            }
        }
    }


    //对话框
    Rectangle {
        id: dialog
        width: 526 * scaleFactor
        height: 594 * scaleFactor
        visible: false

        anchors.bottom: icon.top
        anchors.bottomMargin: 5 * scaleFactor
        color: theme.item.colorW1
        anchors.right: parent.right
        radius: 20

        border.color: theme.item.colorW3
        border.width: 1 * scaleFactor

        //上分割线
        Rectangle {
            id: topLine
            height: 1 * scaleFactor
            width: 484 * scaleFactor
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 38 * scaleFactor
            color: theme.item.colorW3
        }

        DialogPanel {
            id: dialogPanel
            anchors.left: parent.left
            anchors.leftMargin: 2 * scaleFactor
            anchors.right: parent.right
            anchors.rightMargin: 2 * scaleFactor
            anchors.top: topLine.bottom
            anchors.topMargin: 1 * scaleFactor
            anchors.bottom: bottomLine.top
            anchors.bottomMargin: 1 * scaleFactor
        }

        //历史对话,下拉
        Row {
            width: 490 * scaleFactor
            height: 20 * scaleFactor
            spacing: 215 * scaleFactor
            anchors.left: parent.left
            anchors.leftMargin: 20 * scaleFactor
            anchors.top: parent.top
            anchors.topMargin: 10 * scaleFactor

            //历史对话
            Button {
                id: history
                width: 20 * scaleFactor
                height: 20 * scaleFactor
                hoverEnabled: true
                enabled: !dialog_manager.isHistoryEmpty
                checked: historyLog.visible

                background: Image {
                        anchors.fill: parent
                        source: history.enabled
                                ? history.hovered
                                  ? "qrc:/Image/Icon/history_hovered.svg"
                                  : history.checked
                                    ? "qrc:/Image/Icon/history_pressed.svg"
                                    : "qrc:/Image/Icon/history_normal.svg"
                                : "qrc:/Image/Icon/history_disabled.svg"
                }

                ToolTip {
                    visible: history.hovered
                    x: history.x - (historyRect.width - history.width)/2
                    y: history.y + history.height + 4
                    width: (historyText.contentWidth + 6 * 2) * scaleFactor
                    height: 25 * scaleFactor
                    delay: 500

                    background: Rectangle {
                        id: historyRect
                        anchors.fill: parent
                        radius: 5
                        color: "black"

                        Text {
                            id: historyText
                            text: qsTr("History")
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

                onClicked: {
                    if(!historyLog.visible)
                    {
                       openHistory = true
                       window.setHistory(true)
                       window.setDialog(false)
                       curwidth=576+84
                       //window.setXandY(window.x  - 84 * scaleFactor , window.y , window.width + 84 * scaleFactor , window.height) //icon到历史对话最左侧距离84
                       window.setXandY(window.x  - 84 * scaleFactor , window.y , curwidth * scaleFactor , window.height) //icon到历史对话最左侧距离84
                        historyLog.visible = true
                    }
                    else if(historyLog.visible)
                    {
                       openHistory = false
                       window.setHistory(false)
                       window.setDialog(true)
                       curwidth=576
                       //window.setXandY(window.x  + 84 * scaleFactor , window.y , window.width - 84 * scaleFactor , window.height)
                       window.setXandY(window.x  + 84 * scaleFactor , window.y , curwidth * scaleFactor , window.height)
                       historyLog.visible = false
                    }
                }
            }

            //下拉
            Rectangle {
                id: dropDown
                width: 20 * scaleFactor
                height: 20 * scaleFactor

                Image {
                    anchors.fill: parent
                    source: "qrc:/Image/Icon/dropDown_normal.svg"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if(dialog.visible)
                        {
                            //历史记录存在先隐藏历史记录
                            if(historyLog.visible)
                            {
                                openHistory = false
                                historyLog.visible = false
                                window.setHistory(false)
                                curwidth=576
                                //window.setXandY(window.x  + 84 * scaleFactor , window.y , window.width - 84 * scaleFactor , window.height) //icon到历史对话最左侧距离84
                                window.setXandY(window.x  + 84 * scaleFactor , window.y , curwidth * scaleFactor , window.height) //icon到历史对话最左侧距离84
                            }

                            dialog.visible = false
                            curwidth=576
                            window.setDialog(false)  //改变拖拽初始坐标
                            //window.setXandY(window.x , window.y  + dialog.height + 5 * scaleFactor - reasonNoPullBtn.height - 8 * scaleFactor, window.width,window.height - dialog.height - 5 * scaleFactor + reasonNoPullBtn.height + 8 * scaleFactor)
                            window.setXandY(window.x , window.y  + dialog.height + 5 * scaleFactor - reasonNoPullBtn.height - 8 * scaleFactor, curwidth* scaleFactor,window.height - dialog.height - 5 * scaleFactor + reasonNoPullBtn.height + 8 * scaleFactor)
                        }
                    }
                }
            }

            //置顶
            Button {
                id: topScreen
                width: 20 * scaleFactor
                height: 20 * scaleFactor
                checkable: true
                hoverEnabled: true

                background: Image {
                        anchors.fill: parent
                        source:
                            topScreen.pressed ? "qrc:/Image/Icon/topScreed_pressed.svg" :
                            (topScreen.checked ?
                                (topScreen.hovered ? "qrc:/Image/Icon/topScreen_checked_hover.svg" : "qrc:/Image/Icon/topScreen_checked.svg") :
                                (topScreen.hovered ? "qrc:/Image/Icon/topScreen_hover.svg" : "qrc:/Image/Icon/topScreen_normal.svg"))
                }

                ToolTip {
                    visible: topScreen.hovered
                    x: screenRect.x - (screenRect.width - topScreen.width)/2
                    y: topScreen.y + topScreen.height + 4
                    width: 40 * scaleFactor
                    height: 24 * scaleFactor
                    delay: 500

                    background: Rectangle {
                        id: screenRect
                        width: parent.width
                        height: parent.height
                        radius: 5
                        color: "black"

                        Text {
                            text: qsTr("置顶")
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

                onClicked: {
                    window.topScreen = !window.topScreen
                }
            }
        }

        //下分割线
        Rectangle {
            id: bottomLine
            height: 1 * scaleFactor
            width: 484 * scaleFactor
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 52 * scaleFactor
            color: theme.item.colorW3
        }

        //复制粘贴
        Row {
            width: (20 + 20 + 8) * scaleFactor
            height: 20 * scaleFactor
            spacing: 8 * scaleFactor
            anchors.left: parent.left
            anchors.leftMargin: 20 * scaleFactor
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16 * scaleFactor

            ButtonGroup { id: radioGroup1 }

            RadioButton {
                id: reasonBtn
                height: 32 * scaleFactor
                width: (6 * 3 + 20) * scaleFactor + reasonText.contentWidth
                anchors.verticalCenter: parent.verticalCenter
                checked: dialog_manager.reasonModeActive
                ButtonGroup.group: radioGroup1

                background: Rectangle {
                    anchors.fill: parent
                    color: reasonBtn.checked
                           ? theme.item.themeZeroColor
                           : reasonBtn.hovered
                             ? theme.item.themeOneColor6
                             : theme.item.colorW2
                    radius: 38

                    Row {
                        spacing: 6 * scaleFactor
                        anchors.centerIn: parent
                        Image {
                            width: 20 * scaleFactor
                            height: 20 * scaleFactor
                            source: reasonBtn.checked
                                    ? "qrc:/Image/Icon/reason_pressed.svg"
                                    : reasonBtn.hovered
                                      ? "qrc:/Image/Icon/reason_normal.svg"
                                      : "qrc:/Image/Icon/reason_normal.svg"
                        }

                        Text {
                            id: reasonText
                            text: qsTr("Reason(R1)")
                            color: reasonBtn.checked
                                   ? theme.item.colorW1
                                   : reasonBtn.hovered
                                     ? theme.item.colorB1
                                     : theme.item.colorB1
                            font.pixelSize: 14 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name
                        }
                    }
                }

                indicator: Rectangle {
                    visible: false
                }

                onClicked: {
                    dialog_manager.reasonModeActive = !dialog_manager.reasonModeActive
                }
            }

            ButtonGroup { id: radioGroup2 }

            RadioButton {
                id: searchBtn
                height: 32 * scaleFactor
                width: (6 * 3 + 20) * scaleFactor + searchText.contentWidth
                anchors.verticalCenter: parent.verticalCenter
                checked: dialog_manager.searchModeActive
                ButtonGroup.group: radioGroup2

                background: Rectangle {
                    anchors.fill: parent
                    color: searchBtn.checked
                           ? theme.item.themeZeroColor
                           : searchBtn.hovered
                             ? theme.item.themeOneColor6
                             : theme.item.colorW2
                    radius: 38

                    Row {
                        spacing: 6 * scaleFactor
                        anchors.centerIn: parent
                        Image {
                            width: 20 * scaleFactor
                            height: 20 * scaleFactor
                            source: searchBtn.checked
                                    ? "qrc:/Image/Icon/search_pressed.svg"
                                    : searchBtn.hovered
                                      ? "qrc:/Image/Icon/search_normal.svg"
                                      : "qrc:/Image/Icon/search_normal.svg"
                        }

                        Text {
                            id: searchText
                            text: qsTr("Search")
                            color: searchBtn.checked
                                   ? theme.item.colorW1
                                   : searchBtn.hovered
                                     ? theme.item.colorB1
                                     : theme.item.colorB1
                            font.pixelSize: 14 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name
                        }
                    }
                }

                indicator: Rectangle {
                    visible: false
                }

                onClicked: {
                    dialog_manager.searchModeActive = !dialog_manager.searchModeActive
                }
            }
        } // Row

        Button {
            id: newChatBtn
            anchors.right: bottomLine.right
            anchors.top: bottomLine.top
            anchors.topMargin: 12 * scaleFactor
            height: 32 * scaleFactor
            width: (6 * 3 + 20) * scaleFactor + newChatText.contentWidth

            background: Rectangle {
                anchors.fill: parent
                color: newChatBtn.pressed
                       ? theme.item.themeZeroColor
                       : newChatBtn.hovered
                         ? theme.item.themeOneColor6
                         : theme.item.colorW2
                radius: 38

                Row {
                    spacing: 6 * scaleFactor
                    anchors.centerIn: parent
                    Image {
                        width: 20 * scaleFactor
                        height: 20 * scaleFactor
                        source: newChatBtn.pressed
                                ? "qrc:/Image/Icon/newChat_pressed.svg"
                                : newChatBtn.hovered
                                  ? "qrc:/Image/Icon/newChat_hovered.svg"
                                  : "qrc:/Image/Icon/newChat_normal.svg"
                    }

                    Text {
                        id: newChatText
                        text: qsTr("New chat")
                        color: newChatBtn.pressed
                               ? theme.item.colorW1
                               : newChatBtn.hovered
                                 ? theme.item.colorB1
                                 : theme.item.colorB1
                        font.pixelSize: 14 * scaleFactor
                        font.family: fontManager.item.oppoSansM.name
                    }
                }
            }

            onClicked: {
                // 已开启新对话且回答未生成时不响应
                if(dialog_manager.dialogModel.rowCount()) {
                    if(!dialog_manager.responseGenerated) {
                        return
                    }
                }
                dialog_manager.addNewChat()
            }
        }
    }

    //历史对话框
    Rectangle {
       id: historyLog
       width: 126 * scaleFactor
       height: 590 * scaleFactor
       anchors.right: dialog.left
       anchors.rightMargin: 8 * scaleFactor
       anchors.top: dialog.top
       visible: false

       border.color: theme.item.colorW3
       border.width: 1 * scaleFactor

       radius: 20

       ChatHistory {
           anchors.fill: parent
       }
    }
}
