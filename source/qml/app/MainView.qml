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

    Rectangle {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 16 * scaleFactor, 520 * scaleFactor)
        height: storageWarningText.implicitHeight + 12 * scaleFactor
        radius: uiPreferences.radiusSm * scaleFactor
        color: uiPreferences.errorSurfaceColor
        border.color: uiPreferences.dangerColor
        visible: dialog_manager.storageUnavailable
        z: 100

        Text {
            id: storageWarningText
            anchors.fill: parent
            anchors.margins: 6 * scaleFactor
            text: dialog_manager.storageErrorMessage
            color: uiPreferences.dangerColor
            font.pixelSize: uiPreferences.fontSmall * scaleFactor
            font.family: fontManager.item.oppoSansM.name
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    function focusInput() {
        textEdit.forceActiveFocus()
    }

    // Keep one authoritative expansion state.  Previously the upper and lower
    // buttons each changed dialog.visible directly, so a geometry update could
    // leave the visual state one click behind the window state.
    property bool conversationExpanded: false
    property bool conversationGeometryBusy: false
    property bool userResizeActive: false
    property real preferredMainWidth: 576
    property real preferredExpandedHeight: 641
    readonly property real minimumMainWidth: 480
    readonly property real minimumExpandedHeight: 440
    // The compact controls include a 40 px conversation toggle. Size the
    // native window from the tallest control so none of the toolbar is clipped.
    readonly property real compactToolbarHeight: Math.max(reasonNoPullBtn.height,
                                                           searchNoPullBtn.height,
                                                           conversationToggle.height)
                                                + 8 * scaleFactor
    readonly property real dialogSpacing: 5 * scaleFactor
    readonly property real effectiveInputHeight: textEdit.contentHeight > root.lineContentHeight
                                                 ? root.textAreaHeight : root.lineHeight
    readonly property var hostScreen: root.Window.window ? root.Window.window.screen : null
    readonly property real availableScreenHeight: hostScreen
                                                  ? hostScreen.availableGeometry.height
                                                  : 900 * scaleFactor
    readonly property real adaptiveDialogHeight: conversationExpanded
                                                  ? Math.max(220 * scaleFactor,
                                                             root.height
                                                             - effectiveInputHeight
                                                             - dialogSpacing)
                                                  : Math.max(220 * scaleFactor,
                                                    Math.min(594 * scaleFactor,
                                                             availableScreenHeight
                                                             - effectiveInputHeight
                                                             - 32 * scaleFactor))
    onAdaptiveDialogHeightChanged: {
        if (conversationExpanded && !userResizeActive)
            Qt.callLater(function() { root.syncConversationGeometry(true) })
    }

    function historyWidthOffset() {
        return openHistory ? historyExtraWidth * scaleFactor : 0
    }

    function updateResizeConstraints() {
        var minimumWidth = minimumMainWidth * scaleFactor + historyWidthOffset()
        var minimumHeight = conversationExpanded
                ? minimumExpandedHeight * scaleFactor
                : effectiveInputHeight + compactToolbarHeight
        window.setMinimumSize(minimumWidth, minimumHeight)
    }

    function initializeSavedWindowSize() {
        var saved = window.savedMainWindowSize()
        var available = window.availableGeometry()
        var usableWidth = available && available.width > 0
                ? available.width : 1200 * scaleFactor
        var maximumBaseWidth = Math.max(minimumMainWidth,
                                        (usableWidth - historyExtraWidth * scaleFactor
                                         - 24 * scaleFactor)
                                        / scaleFactor)
        var maximumHeight = Math.max(minimumExpandedHeight,
                                     (availableScreenHeight - 24 * scaleFactor)
                                     / scaleFactor)
        if (saved && saved.valid) {
            preferredMainWidth = Math.max(minimumMainWidth,
                                          Math.min(maximumBaseWidth, saved.width))
            preferredExpandedHeight = Math.max(minimumExpandedHeight,
                                                Math.min(maximumHeight, saved.height))
        } else {
            preferredMainWidth = 576
            preferredExpandedHeight = Math.max(minimumExpandedHeight,
                                                Math.min(maximumHeight, 641))
        }
        curwidth = Math.round(preferredMainWidth)
    }

    function targetConversationHeight() {
        return conversationExpanded
                ? Math.max(minimumExpandedHeight * scaleFactor,
                           preferredExpandedHeight * scaleFactor)
                : effectiveInputHeight + compactToolbarHeight
    }

    function syncConversationGeometry(keepBottom) {
        if (conversationGeometryBusy)
            return
        var targetHeight = targetConversationHeight()
        var targetWidth = preferredMainWidth * scaleFactor + historyWidthOffset()
        if (targetHeight <= 0
                || (Math.abs(window.height - targetHeight) < 0.5
                    && Math.abs(window.width - targetWidth) < 0.5))
            return
        conversationGeometryBusy = true
        var targetY = keepBottom ? window.y + window.height - targetHeight : window.y
        curHeight = Math.round(targetHeight / scaleFactor)
        window.setXandY(window.x, targetY,
                        targetWidth, targetHeight)
        conversationGeometryBusy = false
    }

    function setConversationExpanded(expand) {
        if (conversationExpanded === expand)
            return

        if (!expand && historyLog.visible)
            setHistoryVisible(false)

        userResizeActive = false
        conversationExpanded = expand
        window.setDialog(expand)
        updateResizeConstraints()
        syncConversationGeometry(true)
        Qt.callLater(function() {
            textEdit.forceActiveFocus()
        })
    }

    function toggleConversationExpanded() {
        setConversationExpanded(!conversationExpanded)
    }

    function setHistoryVisible(show) {
        if (historyLog.visible === show || (show && !conversationExpanded))
            return
        var delta = historyExtraWidth * scaleFactor
        userResizeActive = false
        if (show) {
            openHistory = true
            window.setHistory(true)
            window.setDialog(false)
            curwidth = Math.round(preferredMainWidth + historyExtraWidth)
            updateResizeConstraints()
            window.setXandY(window.x - delta, window.y,
                            window.width + delta, window.height)
            historyLog.visible = true
        } else {
            openHistory = false
            historyLog.visible = false
            window.setHistory(false)
            window.setDialog(true)
            curwidth = Math.round(preferredMainWidth)
            updateResizeConstraints()
            window.setXandY(window.x + delta, window.y,
                            window.width - delta, window.height)
        }
    }

    function beginUserResize(edges) {
        if (!conversationExpanded)
            return
        userResizeActive = true
        if (window.beginSystemResize(edges)) {
            var baseWidth = (window.width - historyWidthOffset()) / scaleFactor
            preferredMainWidth = Math.max(minimumMainWidth, baseWidth)
            preferredExpandedHeight = Math.max(minimumExpandedHeight,
                                                window.height / scaleFactor)
            curwidth = Math.round(preferredMainWidth + historyWidthOffset() / scaleFactor)
            curHeight = Math.round(preferredExpandedHeight)
            window.saveMainWindowSize(preferredMainWidth, preferredExpandedHeight)
        }
        userResizeActive = false
        updateResizeConstraints()
    }

    property real smokeOriginalX: 0
    property real smokeOriginalY: 0
    property real smokeOriginalWidth: 0
    property real smokeOriginalHeight: 0

    function openHistoryForSmoke() {
        smokeOriginalX = window.x
        smokeOriginalY = window.y
        smokeOriginalWidth = window.width
        smokeOriginalHeight = window.height
        setConversationExpanded(true)
        setHistoryVisible(true)
        Qt.callLater(function() { chatHistoryPanel.scrollForSmoke() })
    }

    function closeHistoryForSmoke() {
        historyLog.visible = false
        openHistory = false
        conversationExpanded = false
        curwidth = Math.round(preferredMainWidth)
        window.setHistory(false)
        window.setDialog(false)
        updateResizeConstraints()
        window.setXandY(smokeOriginalX, smokeOriginalY,
                        smokeOriginalWidth, smokeOriginalHeight)
    }

    function openDeleteConfirmForSmoke() {
        if (!conversationExpanded)
            setConversationExpanded(true)
        if (!historyLog.visible)
            setHistoryVisible(true)
        chatHistoryPanel.openDeleteConfirmForSmoke()
    }

    function closeDeleteConfirmForSmoke() {
        chatHistoryPanel.closeDeleteConfirmForSmoke()
        closeHistoryForSmoke()
    }

    function showTooltipForSmoke() {
        conversationTip.forceVisible = true
    }

    function hideTooltipForSmoke() {
        conversationTip.forceVisible = false
    }

    // 自适应窗口的 DPI 缩放
    property real scaleFactor: window.scale

    onScaleFactorChanged: {
            //console.log("缩放比:", scaleFactor)
            //console.log(root.width,root.height,window.width,window.height,window.x,window.y)
            //console.log(textEdit.contentHeight,root.lineContentHeight)
            Qt.callLater(function() {
                root.updateResizeConstraints()
                root.syncConversationGeometry(true)
            })
        }

    Component.onCompleted: {
        initializeSavedWindowSize()
        updateResizeConstraints()
        syncConversationGeometry(false)
    }

    property bool openHistory: false
    property int historyPanelWidth: 176
    property int historyExtraWidth: historyPanelWidth - 42
    property int lineContentHeight: 27 * scaleFactor //单行内容高度
    property int lineHeight: 42 * scaleFactor  //单行高度
    property int textLineHeight: 22 * scaleFactor//输入框单行高度

    property int contentBoxHeight: 168 * scaleFactor//内容框高度 (仅包含内容)
    property int textAreaHeight: 212 * scaleFactor//整个文本框高度

    property int curwidth: 576
    property int curHeight:90
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
        anchors.leftMargin: (openHistory ? historyExtraWidth : 0) * scaleFactor
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

        MouseArea {
            id: dragArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: window.beginSystemMove()
            hoverEnabled: true
            ModernToolTip {
                anchorItem: dragArea
                visible: dragArea.containsMouse
                text: qsTr("按住拖动窗口")
            }
        }
    }

    // One persistent toggle is used in both compact and expanded modes.  It
    // stays at the horizontal centre, while its y position follows the active
    // surface. A single control avoids the old alternating-click failure.
    Button {
        id: conversationToggle
        objectName: "conversationToggle"
        width: 40 * scaleFactor
        height: 40 * scaleFactor
        x: textRect.x + (textRect.width - width) / 2
        y: conversationExpanded
           ? dialog.y
           : textRect.y - height - 4 * scaleFactor
        z: 8
        hoverEnabled: true
        activeFocusOnTab: true
        checkable: true
        checked: root.conversationExpanded
        Accessible.role: Accessible.Button
        Accessible.name: conversationExpanded ? qsTr("收起会话") : qsTr("展开会话")
        Accessible.description: qsTr("切换聊天记录区域。")
        Accessible.checked: checked

        background: Item {
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: conversationToggle.down
                       ? theme.item.themeOneColor6
                       : conversationToggle.hovered
                         ? theme.item.colorW2 : "transparent"
                border.width: 0
            }
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: "transparent"
                border.width: conversationToggle.activeFocus ? 2 * scaleFactor : 0
                border.color: theme.item.focusRingColor
            }
        }

        contentItem: Image {
            source: "qrc:/Image/Icon/pullUp.svg"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 16 * scaleFactor
            sourceSize.height: 16 * scaleFactor
            rotation: conversationExpanded ? 180 : 0
            scale: conversationToggle.down ? 0.9 : 1.0
            smooth: true
        }

        ModernToolTip {
            id: conversationTip
            anchorItem: conversationToggle
            visible: conversationToggle.hovered || forceVisible
            text: conversationExpanded ? qsTr("收起会话") : qsTr("展开会话")
        }
        onClicked: root.toggleConversationExpanded()
    }

    DeepSeekModelSelector {
        id: reasonNoPullBtn
        objectName: "collapsedModelSelector"
        scaleFactor: root.scaleFactor
        accentColor: theme.item.themeZeroColor
        normalColor: theme.item.colorW2
        hoverColor: theme.item.themeOneColor6
        textColor: theme.item.colorB1
        mutedColor: theme.item.colorW6
        fontFamily: fontManager.item.uiFontFamily
        maximumWidth: 226 * scaleFactor
        enabled: dialog_manager.responseGenerated
        opacity: enabled ? 1 : 0.55
        anchors.left: textRect.left
        anchors.bottom: textRect.top
        anchors.bottomMargin: 8 * scaleFactor
    }

    ButtonGroup { id: radioGroup3 }
    RadioButton {
        id: searchNoPullBtn
        objectName: "collapsedSearchButton"
        height: 32 * scaleFactor
        width: dialog_manager.searchModeAvailable
               ? (6 * 3 + 18) * scaleFactor + searchNoPullText.contentWidth
               : 36 * scaleFactor
        anchors.right: textRect.right
        anchors.bottom: textRect.top
        anchors.bottomMargin: 8 * scaleFactor
        checked: dialog_manager.searchModeActive
        enabled: dialog_manager.searchModeAvailable && dialog_manager.responseGenerated
        hoverEnabled: true
        activeFocusOnTab: true
        Accessible.role: Accessible.CheckBox
        Accessible.name: qsTr("联网搜索")
        Accessible.description: !dialog_manager.responseGenerated
                                ? qsTr("回答生成期间不能更改搜索模式。")
                                : dialog_manager.searchModeAvailable
                                  ? qsTr("在发送消息时启用联网搜索。")
                                  : qsTr("当前 Provider 不支持联网搜索。")
        Accessible.checked: checked
        ButtonGroup.group: radioGroup3

        background: Rectangle {
            anchors.fill: parent
            color: searchNoPullBtn.checked
                   ? theme.item.themeZeroColor
                   : searchNoPullBtn.hovered
                     ? theme.item.colorW2
                     : theme.item.colorW2
            opacity: searchNoPullBtn.checked ? 1 : 0.7
            border.width: searchNoPullBtn.activeFocus ? 2 * scaleFactor : 1 * scaleFactor
            border.color: searchNoPullBtn.activeFocus ? theme.item.focusRingColor
                          : searchNoPullBtn.checked ? theme.item.themeZeroColor
                          : theme.item.colorW6
            radius: 38

            Row {
                spacing: 6 * scaleFactor
                anchors.centerIn: parent
                Image {
                    width: 18 * scaleFactor
                    height: 18 * scaleFactor
                    source: searchNoPullBtn.checked
                            ? "qrc:/Image/Icon/search_pressed.svg"
                            : searchNoPullBtn.hovered
                              ? "qrc:/Image/Icon/search_normal.svg"
                              : "qrc:/Image/Icon/search_normal.svg"
                }

                Text {
                    id: searchNoPullText
                    visible: dialog_manager.searchModeAvailable
                    text: qsTr("联网")
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

        ModernToolTip {
            anchorItem: searchNoPullBtn
            visible: searchNoPullBtn.hovered
            text: dialog_manager.searchModeAvailable
                  ? qsTr("联网搜索") : qsTr("当前 Provider 不支持联网搜索")
        }

        onClicked: {
            dialog_manager.searchModeActive = !dialog_manager.searchModeActive
        }
    }

    Rectangle {
        id :textRect
        color: theme.item.surfaceColor
        radius: uiPreferences.radiusLg
        width: Math.max(430 * scaleFactor,
                        root.width - root.historyWidthOffset()
                        - icon.width - 8 * scaleFactor)
        border.color: textEdit.activeFocus ? theme.item.focusRingColor : theme.item.borderColor
        border.width: textEdit.activeFocus ? 2 * scaleFactor : 1 * scaleFactor
        height: (textEdit.contentHeight > 27 * scaleFactor) ? root.textAreaHeight : root.lineHeight
        anchors.left: icon.right
        anchors.leftMargin: 8 * scaleFactor
        anchors.bottom: parent.bottom
        z: 1

        ScrollView {
            id: scrollView
            width: parent.width - 64 * scaleFactor
            height: Math.min(root.contentBoxHeight,
                             Math.max(root.textLineHeight,
                                      textRect.height - 20 * scaleFactor))
            anchors.left: parent.left
            anchors.leftMargin: 8 * scaleFactor
            anchors.top: parent.top
            anchors.topMargin: 10 * scaleFactor
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: textEdit.contentHeight > height
                                       ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

            TextArea {
                id: textEdit
                objectName: "chatInput"
                width: scrollView.width
                height: (textEdit.contentHeight > 27 * scaleFactor) ? root.contentBoxHeight : root.textLineHeight
                wrapMode: TextArea.Wrap
                placeholderText: (dialog_manager.storageUnavailable ? qsTr("本地存储不可用，输入内容会保留但无法发送")
                                    : !window.allowQuestion ? qsTr("请先在托盘设置中保存 API Provider 和密钥")
                                    : (window.notEnough ? qsTr("余额不足，请及时充值") : qsTr("输入消息，Enter 发送，Shift+Enter 换行")))
                placeholderTextColor: window.allowQuestion && dialog_manager.storageAvailable
                                      ? theme.item.colorW4 : "red"
                font.pixelSize: 14 * scaleFactor
                padding: 2 * scaleFactor  // 内边距2
                font.family: fontManager.item.oppoSansM.name
                selectionColor: theme.item.themeOneColor6
                selectByMouse: true
                color: theme.item.colorB1
                activeFocusOnTab: true
                Accessible.role: Accessible.EditableText
                Accessible.name: qsTr("聊天消息")
                Accessible.description: qsTr("输入消息。Enter 发送，Shift+Enter 换行；输入法组词期间 Enter 只确认候选。")

                onContentHeightChanged: root.syncConversationGeometry(true)
                Keys.onPressed:  (event) => {
                        if (event.key  === Qt.Key_Return || event.key  === Qt.Key_Enter) {
                            if (textEdit.inputMethodComposing) {
                                // Do not consume the IME commit key and never send a
                                // half-composed message.
                                event.accepted = false
                                return
                            } else if (event.modifiers  & Qt.ShiftModifier) {
                                // Shift+Enter：手动插入换行
                                textEdit.insert(textEdit.cursorPosition,  "\n");
                                event.accepted = true
                            } else {
                                // 生成中不让 Enter 误触取消；停止操作只接受鼠标明确点击。
                                if (dialog_manager.responseGenerated && send.enabled)
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
            objectName: "sendOrStopButton"
            width: 40 * scaleFactor
            height: 40 * scaleFactor
            anchors.right: parent.right
            anchors.rightMargin: 6 * scaleFactor
            anchors.verticalCenter: parent.verticalCenter
            z: 2
            activeFocusOnTab: true
            enabled: !dialog_manager.responseGenerated
                     || (window.allowQuestion && dialog_manager.storageAvailable
                         && textEdit.text.trim().length > 0)
            Accessible.role: Accessible.Button
            Accessible.name: dialog_manager.responseGenerated ? qsTr("发送消息") : qsTr("停止生成")
            Accessible.description: !dialog_manager.responseGenerated
                                    ? qsTr("停止当前回答生成。")
                                    : textEdit.text.trim().length === 0
                                      ? qsTr("请输入非空白消息后发送。")
                                      : dialog_manager.storageUnavailable
                                        ? qsTr("本地存储不可用，消息输入会保留。")
                                        : qsTr("发送当前聊天消息。")

            background: Item {
                Image {
                    id: sendIcon
                    objectName: "sendIcon"
                    anchors.centerIn: parent
                    width: 34 * scaleFactor
                    height: 34 * scaleFactor
                    visible: dialog_manager.responseGenerated
                    source: send.enabled
                            ? send.pressed
                              ? "qrc:/Image/Icon/send_pressed.svg"
                              : send.hovered
                                ? "qrc:/Image/Icon/send_hovered.svg"
                                : "qrc:/Image/Icon/send_normal.svg"
                            : "qrc:/Image/Icon/send_disabled.svg"
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 10 * scaleFactor
                    color: "transparent"
                    border.width: send.activeFocus ? 2 * scaleFactor : 0
                    border.color: theme.item.focusRingColor
                }
            }

            Rectangle {
                id: stopIndicator
                objectName: "stopIndicator"
                anchors.centerIn: parent
                width: 11 * scaleFactor
                height: 11 * scaleFactor
                radius: 2 * scaleFactor
                color: theme.item.accentColor
                visible: !dialog_manager.responseGenerated
            }

            ModernToolTip {
                anchorItem: send
                visible: send.hovered
                text: dialog_manager.responseGenerated ? qsTr("发送") : qsTr("停止")
                displayAbove: true
            }

            onClicked: {
                if(!dialog_manager.responseGenerated) {
                    dialog_manager.cancelResponse()
                    return
                }
                if(textEdit.text.trim().length === 0 || !window.allowQuestion)
                    return
                // 已开启新对话且回答未生成时无法点击发送按钮
                if(dialog_manager.dialogModel.rowCount()) {
                    if(!dialog_manager.responseGenerated) {
                        return
                    }
                }

                if (dialog_manager.sendMessage(textEdit.text)) {
                    textEdit.text = ""
                    root.setConversationExpanded(true)
                }
            }
        }
    }


    //对话框
    Rectangle {
        id: dialog
        width: textRect.width
        height: root.adaptiveDialogHeight
        visible: root.conversationExpanded

        anchors.bottom: icon.top
        anchors.bottomMargin: 5 * scaleFactor
        color: theme.item.colorW1
        anchors.right: parent.right
        radius: uiPreferences.radiusLg
        z: 2

        border.color: theme.item.colorW3
        border.width: 1 * scaleFactor

        //上分割线
        Rectangle {
            id: topLine
            height: 1 * scaleFactor
            width: Math.max(1, parent.width - 42 * scaleFactor)
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
        Item {
            id: dialogHeader
            width: Math.max(1, parent.width - 40 * scaleFactor)
            height: 20 * scaleFactor
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 10 * scaleFactor

            //历史对话
            Button {
                id: history
                width: 40 * scaleFactor
                height: 40 * scaleFactor
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                hoverEnabled: true
                enabled: !dialog_manager.isHistoryEmpty
                checked: historyLog.visible
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("历史会话")
                Accessible.description: enabled
                                        ? qsTr("显示或隐藏历史会话列表。")
                                        : qsTr("当前没有历史会话。")
                Accessible.checked: checked

                background: Item {
                    Image {
                        anchors.centerIn: parent
                        width: 20 * scaleFactor
                        height: 20 * scaleFactor
                        source: history.enabled
                                ? history.hovered
                                  ? "qrc:/Image/Icon/history_hovered.svg"
                                  : history.checked
                                    ? "qrc:/Image/Icon/history_pressed.svg"
                                    : "qrc:/Image/Icon/history_normal.svg"
                                : "qrc:/Image/Icon/history_disabled.svg"
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: 8 * scaleFactor
                        color: "transparent"
                        border.width: history.activeFocus ? 2 * scaleFactor : 0
                        border.color: theme.item.focusRingColor
                    }
                }

                ModernToolTip {
                    anchorItem: history
                    visible: history.hovered
                    text: qsTr("历史会话")
                }

                onClicked: {
                    root.setHistoryVisible(!historyLog.visible)
                }
            }

            //置顶
            Button {
                id: topScreen
                width: 40 * scaleFactor
                height: 40 * scaleFactor
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                checkable: true
                hoverEnabled: true
                activeFocusOnTab: true
                Accessible.role: Accessible.CheckBox
                Accessible.name: qsTr("窗口置顶")
                Accessible.description: checked ? qsTr("对话窗口当前保持置顶。")
                                                : qsTr("让对话窗口保持在其他窗口上方。")
                Accessible.checked: checked

                background: Item {
                    Image {
                        anchors.centerIn: parent
                        width: 20 * scaleFactor
                        height: 20 * scaleFactor
                        source:
                            topScreen.pressed ? "qrc:/Image/Icon/topScreed_pressed.svg" :
                            (topScreen.checked ?
                                (topScreen.hovered ? "qrc:/Image/Icon/topScreen_checked_hover.svg" : "qrc:/Image/Icon/topScreen_checked.svg") :
                                (topScreen.hovered ? "qrc:/Image/Icon/topScreen_hover.svg" : "qrc:/Image/Icon/topScreen_normal.svg"))
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: 8 * scaleFactor
                        color: "transparent"
                        border.width: topScreen.activeFocus ? 2 * scaleFactor : 0
                        border.color: theme.item.focusRingColor
                    }
                }

                ModernToolTip {
                    anchorItem: topScreen
                    visible: topScreen.hovered
                    text: topScreen.checked ? qsTr("取消置顶") : qsTr("置顶")
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
            width: Math.max(1, parent.width - 42 * scaleFactor)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 52 * scaleFactor
            color: theme.item.colorW3
        }

        Item {
            id: dialogToolbar
            anchors.left: parent.left
            anchors.leftMargin: 20 * scaleFactor
            anchors.right: parent.right
            anchors.rightMargin: 20 * scaleFactor
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10 * scaleFactor
            height: 32 * scaleFactor

            DeepSeekModelSelector {
                id: reasonBtn
                maximumWidth: 226 * scaleFactor
                scaleFactor: root.scaleFactor
                accentColor: theme.item.themeZeroColor
                normalColor: theme.item.colorW2
                hoverColor: theme.item.themeOneColor6
                textColor: theme.item.colorB1
                mutedColor: theme.item.colorW6
                fontFamily: fontManager.item.uiFontFamily
                enabled: dialog_manager.responseGenerated
                opacity: enabled ? 1 : 0.55
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            Button {
                id: searchBtn
                objectName: "expandedSearchButton"
                height: 32 * scaleFactor
                width: dialog_manager.searchModeAvailable
                       ? (6 * 3 + 18) * scaleFactor + searchText.contentWidth
                       : 36 * scaleFactor
                anchors.left: reasonBtn.right
                anchors.leftMargin: 8 * scaleFactor
                anchors.verticalCenter: parent.verticalCenter
                checkable: true
                checked: dialog_manager.searchModeActive
                enabled: dialog_manager.searchModeAvailable && dialog_manager.responseGenerated
                hoverEnabled: true
                activeFocusOnTab: true
                Accessible.role: Accessible.CheckBox
                Accessible.name: qsTr("联网搜索")
                Accessible.description: !dialog_manager.responseGenerated
                                        ? qsTr("回答生成期间不能更改搜索模式。")
                                        : dialog_manager.searchModeAvailable
                                          ? qsTr("在发送消息时启用联网搜索。")
                                          : qsTr("当前 Provider 不支持联网搜索。")
                Accessible.checked: checked

                background: Rectangle {
                    anchors.fill: parent
                    color: searchBtn.checked
                           ? theme.item.themeZeroColor
                           : searchBtn.hovered
                             ? theme.item.themeOneColor6
                             : theme.item.colorW2
                    radius: height / 2
                    border.width: searchBtn.activeFocus ? 2 * scaleFactor : 0
                    border.color: theme.item.focusRingColor

                    Row {
                        spacing: 6 * scaleFactor
                        anchors.centerIn: parent
                        Image {
                            width: 18 * scaleFactor
                            height: 18 * scaleFactor
                            source: searchBtn.checked
                                    ? "qrc:/Image/Icon/search_pressed.svg"
                                    : searchBtn.hovered
                                      ? "qrc:/Image/Icon/search_normal.svg"
                                      : "qrc:/Image/Icon/search_normal.svg"
                        }

                        Text {
                            id: searchText
                            visible: dialog_manager.searchModeAvailable
                            text: qsTr("联网")
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

                ModernToolTip {
                    anchorItem: searchBtn
                    visible: searchBtn.hovered
                    text: dialog_manager.searchModeAvailable
                          ? qsTr("联网搜索") : qsTr("当前 Provider 不支持联网搜索")
                }

                onClicked: {
                    dialog_manager.searchModeActive = !dialog_manager.searchModeActive
                }
            }

            Button {
                id: newChatBtn
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 32 * scaleFactor
                width: (6 * 3 + 20) * scaleFactor + newChatText.contentWidth
                enabled: dialog_manager.responseGenerated
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("新对话")
                Accessible.description: enabled
                                        ? qsTr("创建一个新的聊天会话。")
                                        : qsTr("回答生成期间不能创建新对话；可先停止生成。")

                background: Rectangle {
                    anchors.fill: parent
                    color: !newChatBtn.enabled ? theme.item.colorW3
                           : newChatBtn.pressed
                           ? theme.item.themeZeroColor
                           : newChatBtn.hovered
                             ? theme.item.themeOneColor6
                             : theme.item.colorW2
                    radius: height / 2
                    border.width: newChatBtn.activeFocus ? 2 * scaleFactor : 0
                    border.color: theme.item.focusRingColor

                    Row {
                        spacing: 6 * scaleFactor
                        anchors.centerIn: parent
                        Image {
                            width: 20 * scaleFactor
                            height: 20 * scaleFactor
                            source: !newChatBtn.enabled
                                    ? "qrc:/Image/Icon/newChat_disabled.svg"
                                    : newChatBtn.pressed
                                    ? "qrc:/Image/Icon/newChat_pressed.svg"
                                    : newChatBtn.hovered
                                      ? "qrc:/Image/Icon/newChat_hovered.svg"
                                      : "qrc:/Image/Icon/newChat_normal.svg"
                        }

                        Text {
                            id: newChatText
                            text: qsTr("新对话")
                            color: newChatBtn.pressed ? theme.item.colorW1 : theme.item.colorB1
                            font.pixelSize: 14 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name
                        }
                    }
                }

                ModernToolTip {
                    anchorItem: newChatBtn
                    visible: newChatBtn.hovered
                    text: newChatBtn.enabled ? qsTr("新对话")
                                             : qsTr("回答生成期间不能创建新对话")
                    displayAbove: true
                }

                onClicked: {
                    if(dialog_manager.dialogModel.rowCount()
                            && !dialog_manager.responseGenerated)
                        return
                    dialog_manager.addNewChat()
                }
            }
        }
    }

    //历史对话框
    Rectangle {
       id: historyLog
       width: historyPanelWidth * scaleFactor
       height: Math.max(1, dialog.height - 4 * scaleFactor)
       anchors.right: dialog.left
       anchors.rightMargin: 8 * scaleFactor
       anchors.top: dialog.top
       visible: false

       border.color: theme.item.colorW3
       border.width: 1 * scaleFactor

       radius: uiPreferences.radiusLg * scaleFactor

       ChatHistory {
           id: chatHistoryPanel
           anchors.fill: parent
       }
    }

    // The main assistant uses a frameless native window, so it does not receive
    // normal OS resize borders. These narrow hit areas restore the standard
    // eight-direction system resize operation without covering the content.
    Item {
        id: resizeLayer
        objectName: "mainWindowResizeLayer"
        anchors.fill: parent
        visible: root.conversationExpanded
        z: 1000
        readonly property real edgeSize: Math.max(5, 6 * root.scaleFactor)
        readonly property real cornerSize: Math.max(14, 16 * root.scaleFactor)

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: resizeLayer.cornerSize
            anchors.bottomMargin: resizeLayer.cornerSize
            width: resizeLayer.edgeSize
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.LeftEdge)
        }
        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: resizeLayer.cornerSize
            anchors.bottomMargin: resizeLayer.cornerSize
            width: resizeLayer.edgeSize
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.RightEdge)
        }
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: resizeLayer.cornerSize
            anchors.rightMargin: resizeLayer.cornerSize
            height: resizeLayer.edgeSize
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.TopEdge)
        }
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: resizeLayer.cornerSize
            anchors.rightMargin: resizeLayer.cornerSize
            height: resizeLayer.edgeSize
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.BottomEdge)
        }
        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            width: resizeLayer.cornerSize
            height: resizeLayer.cornerSize
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.LeftEdge | Qt.TopEdge)
        }
        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            width: resizeLayer.cornerSize
            height: resizeLayer.cornerSize
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.RightEdge | Qt.TopEdge)
        }
        MouseArea {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: resizeLayer.cornerSize
            height: resizeLayer.cornerSize
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.LeftEdge | Qt.BottomEdge)
        }
        MouseArea {
            id: bottomRightResizeHandle
            objectName: "mainWindowBottomRightResizeHandle"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: resizeLayer.cornerSize
            height: resizeLayer.cornerSize
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            onPressed: root.beginUserResize(Qt.RightEdge | Qt.BottomEdge)

            Rectangle {
                width: 9 * root.scaleFactor
                height: Math.max(1, root.scaleFactor)
                x: 5 * root.scaleFactor
                y: 9 * root.scaleFactor
                rotation: -45
                color: theme.item.colorW6
                opacity: 0.65
            }
            Rectangle {
                width: 5 * root.scaleFactor
                height: Math.max(1, root.scaleFactor)
                x: 9 * root.scaleFactor
                y: 11 * root.scaleFactor
                rotation: -45
                color: theme.item.colorW6
                opacity: 0.65
            }
        }
    }
}
