import QtQuick 2.15
import QtQuick.Controls 2.15
import "../Controls"

Rectangle {
    id: root
    property real scaleFactor: window.scale
    color: theme.item.colorW1
    clip: true

    function isWorking(value) {
        return value === "in_flight" || value === "streaming"
    }

    Column {
        id: emptyState
        visible: dialog_manager.isChatEmpty
        anchors.centerIn: parent
        spacing: 9 * scaleFactor

        Image {
            width: 88 * scaleFactor
            height: 88 * scaleFactor
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/Image/Icon/empty.svg"
            opacity: 0.72
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("有什么可以帮你？")
            color: theme.item.colorB1
            font.pixelSize: 18 * scaleFactor
            font.family: fontManager.item.uiFontFamily
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("选择模型和思考强度，然后开始对话")
            color: theme.item.colorW6
            font.pixelSize: 12 * scaleFactor
            font.family: fontManager.item.uiFontFamily
        }
    }

    Component {
        id: userMessageComponent

        Item {
            id: userMessage
            width: parent.width
            implicitHeight: userBubble.height + 2 * root.scaleFactor

            TextMetrics {
                id: userMetrics
                text: userMessage.parent.messageContent
                font.pixelSize: 14 * root.scaleFactor
                font.family: fontManager.item.uiFontFamily
            }

            Rectangle {
                id: userBubble
                anchors.right: parent.right
                anchors.rightMargin: 8 * root.scaleFactor
                width: Math.min(parent.width * 0.78,
                                Math.max(64 * root.scaleFactor,
                                         userMetrics.advanceWidth + 28 * root.scaleFactor))
                height: Math.ceil(userText.contentHeight) + 18 * root.scaleFactor
                radius: 14 * root.scaleFactor
                color: theme.item.themeOneColor6

                MarkdownText {
                    id: userText
                    markdown: false
                    sourceText: parent.parent.parent.messageContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14 * root.scaleFactor
                    anchors.rightMargin: 14 * root.scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Component {
        id: assistantMessageComponent

        Item {
            id: assistantMessage
            width: parent.width
            implicitHeight: assistantColumn.implicitHeight + 4 * root.scaleFactor
            property bool thoughtExpanded: root.isWorking(parent.messageStatus)

            Column {
                id: assistantColumn
                x: 10 * root.scaleFactor
                width: parent.width - 20 * root.scaleFactor
                spacing: 8 * root.scaleFactor

                Button {
                    id: reasoningHeader
                    visible: parent.parent.parent.messageReasoning !== ""
                             || root.isWorking(parent.parent.parent.messageStatus)
                    width: reasoningHeaderRow.implicitWidth + 20 * root.scaleFactor
                    height: 30 * root.scaleFactor
                    hoverEnabled: true

                    background: Rectangle {
                        radius: 9 * root.scaleFactor
                        color: reasoningHeader.down ? uiPreferences.disabledSurfaceColor
                              : reasoningHeader.hovered ? uiPreferences.backgroundColor : theme.item.colorW2
                    }
                    contentItem: Row {
                        id: reasoningHeaderRow
                        anchors.centerIn: parent
                        spacing: 6 * root.scaleFactor
                        Image {
                            width: 16 * root.scaleFactor
                            height: 16 * root.scaleFactor
                            source: "qrc:/Image/Icon/thinking_on.svg"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: root.isWorking(assistantMessage.parent.messageStatus)
                                  ? qsTr("正在思考…")
                                  : assistantMessage.thoughtExpanded
                                    ? qsTr("收起思考过程") : qsTr("思考过程")
                            color: theme.item.colorB1
                            font.pixelSize: 12 * root.scaleFactor
                            font.family: fontManager.item.uiFontFamily
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    onClicked: {
                        if (assistantMessage.parent.messageReasoning !== "")
                            assistantMessage.thoughtExpanded = !assistantMessage.thoughtExpanded
                    }
                }

                Loader {
                    width: parent.width
                    active: assistantMessage.thoughtExpanded
                            && assistantMessage.parent.messageReasoning !== ""
                    visible: active
                    height: item ? item.implicitHeight : 0
                    sourceComponent: Component {
                        Item {
                            width: parent.width
                            implicitHeight: reasoningMarkdown.height

                            Rectangle {
                                width: 2 * root.scaleFactor
                                height: parent.height
                                radius: width / 2
                                color: theme.item.themeOneColor3
                            }
                            MarkdownText {
                                id: reasoningMarkdown
                                x: 12 * root.scaleFactor
                                width: parent.width - x
                                height: Math.ceil(contentHeight)
                                sourceText: assistantMessage.parent.messageReasoning
                                streaming: root.isWorking(assistantMessage.parent.messageStatus)
                                color: theme.item.colorW7
                            }
                        }
                    }
                }

                Text {
                    visible: root.isWorking(assistantMessage.parent.messageStatus)
                             && assistantMessage.parent.messageContent === ""
                    text: qsTr("正在连接并生成回答…")
                    color: theme.item.colorW6
                    font.pixelSize: 13 * root.scaleFactor
                    font.family: fontManager.item.uiFontFamily
                }

                MarkdownText {
                    id: answerMarkdown
                    visible: sourceText !== ""
                    width: parent.width
                    height: visible ? Math.ceil(contentHeight) : 0
                    sourceText: assistantMessage.parent.messageContent
                    markdown: true
                    streaming: root.isWorking(assistantMessage.parent.messageStatus)
                }

                MarkdownText {
                    id: referenceMarkdown
                    visible: sourceText !== "" && !root.isWorking(assistantMessage.parent.messageStatus)
                    width: parent.width
                    height: visible ? Math.ceil(contentHeight) : 0
                    sourceText: assistantMessage.parent.messageReference
                    markdown: true
                    color: theme.item.themeZeroColor
                    font.pixelSize: 12 * root.scaleFactor
                }

                Rectangle {
                    id: errorCard
                    objectName: "assistantErrorCard"
                    visible: assistantMessage.parent.messageStatus === "failed"
                             || assistantMessage.parent.messageStatus === "interrupted"
                             || assistantMessage.parent.messageStatus === "cancelled"
                    width: parent.width
                    height: visible ? errorColumn.implicitHeight + 20 * root.scaleFactor : 0
                    radius: 10 * root.scaleFactor
                    color: assistantMessage.parent.messageNeutralError
                           ? uiPreferences.disabledSurfaceColor : uiPreferences.errorSurfaceColor
                    border.width: 1
                    border.color: assistantMessage.parent.messageNeutralError
                                  ? uiPreferences.borderColor : uiPreferences.dangerColor

                    Column {
                        id: errorColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12 * root.scaleFactor
                        anchors.rightMargin: 12 * root.scaleFactor
                        spacing: 7 * root.scaleFactor

                        Text {
                            visible: assistantMessage.parent.messagePartial
                            text: qsTr("部分回复 · 后续内容未完成，重试不会自动拼接")
                            color: uiPreferences.warningColor
                            font.pixelSize: 11 * root.scaleFactor
                            font.bold: true
                            font.family: fontManager.item.uiFontFamily
                        }
                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: assistantMessage.parent.messageErrorMessage !== ""
                                  ? assistantMessage.parent.messageErrorMessage
                                  : assistantMessage.parent.messageStatus === "cancelled"
                                    ? qsTr("已取消生成。") : qsTr("上次请求未完成。")
                            color: assistantMessage.parent.messageNeutralError
                                   ? theme.item.colorW7 : uiPreferences.dangerColor
                            font.pixelSize: 12 * root.scaleFactor
                            font.family: fontManager.item.uiFontFamily
                        }
                        Row {
                            spacing: 8 * root.scaleFactor
                            visible: assistantMessage.parent.messageErrorAction !== ""
                                     && assistantMessage.parent.messageIndex === messageList.count - 1
                            Button {
                                visible: assistantMessage.parent.messageErrorAction === "retry"
                                text: assistantMessage.parent.messageRetryAfterSeconds >= 0
                                      ? qsTr("重试（建议等待 %1 秒）").arg(assistantMessage.parent.messageRetryAfterSeconds)
                                      : qsTr("原地重试")
                                enabled: dialog_manager.canRetry && window.allowQuestion
                                onClicked: dialog_manager.retryLastRequest()
                            }
                            Button {
                                visible: assistantMessage.parent.messageErrorAction === "settings"
                                text: qsTr("打开设置")
                                onClicked: window.showGuideWindow()
                            }
                        }
                    }
                }

                Row {
                    height: 30 * root.scaleFactor
                    spacing: 6 * root.scaleFactor
                    visible: !root.isWorking(assistantMessage.parent.messageStatus)

                    ToolButton {
                        id: copyAction
                        visible: assistantMessage.parent.messageContent !== ""
                        property bool copied: false
                        width: 30 * root.scaleFactor
                        height: width
                        hoverEnabled: true
                        background: Rectangle {
                            radius: 9 * root.scaleFactor
                            color: copyAction.copied ? uiPreferences.errorSurfaceColor
                                  : copyAction.down ? uiPreferences.disabledSurfaceColor
                                  : copyAction.hovered ? theme.item.colorW2 : "transparent"
                            border.width: copyAction.copied ? 1 : 0
                            border.color: "#A7D8B6"
                        }
                        contentItem: Item {
                            Image {
                                anchors.centerIn: parent
                                width: 18 * root.scaleFactor
                                height: width
                                visible: !copyAction.copied
                                source: "qrc:/Image/Icon/copy_normal.svg"
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: copyAction.copied
                                text: "✓"
                                color: uiPreferences.successColor
                                font.pixelSize: 17 * root.scaleFactor
                                font.bold: true
                                font.family: fontManager.item.uiFontFamily
                            }
                        }
                        Timer {
                            id: copiedResetTimer
                            interval: 1500
                            onTriggered: copyAction.copied = false
                        }
                        onClicked: {
                            dialog_manager.copy(assistantMessage.parent.messageIndex)
                            copied = true
                            copiedResetTimer.restart()
                        }
                        ModernToolTip {
                            anchorItem: copyAction
                            visible: copyAction.hovered || copyAction.copied
                            delay: copyAction.copied ? 0 : 420
                            text: copyAction.copied ? qsTr("已复制") : qsTr("复制")
                            displayAbove: true
                        }
                        Accessible.name: copied ? qsTr("已复制") : qsTr("复制回答")
                    }
                    ToolButton {
                        id: regenerateAction
                        width: 30 * root.scaleFactor
                        height: width
                        hoverEnabled: true
                        visible: assistantMessage.parent.messageIndex === messageList.count - 1
                                 && dialog_manager.responseGenerated
                        enabled: window.allowQuestion
                        background: Rectangle {
                            radius: 9 * root.scaleFactor
                            color: regenerateAction.down ? uiPreferences.disabledSurfaceColor
                                  : regenerateAction.hovered ? theme.item.colorW2 : "transparent"
                        }
                        contentItem: Image {
                            width: 18 * root.scaleFactor
                            height: width
                            source: "qrc:/Image/Icon/regenerate_normal.svg"
                            fillMode: Image.PreserveAspectFit
                        }
                        onClicked: dialog_manager.regenerate()
                        ModernToolTip {
                            anchorItem: regenerateAction
                            visible: regenerateAction.hovered
                            text: qsTr("重新生成")
                            displayAbove: true
                        }
                        Accessible.name: qsTr("重新生成回答")
                    }
                }
            }
        }
    }

    ListView {
        id: messageList
        anchors.fill: parent
        anchors.leftMargin: 6 * scaleFactor
        anchors.rightMargin: 6 * scaleFactor
        anchors.topMargin: 8 * scaleFactor
        anchors.bottomMargin: 4 * scaleFactor
        clip: true
        model: dialog_manager.dialogModel
        spacing: 16 * scaleFactor
        boundsBehavior: Flickable.StopAtBounds
        // Sub-pixel positioning preserves precision from Windows touchpads.
        pixelAligned: false
        reuseItems: true
        cacheBuffer: Math.max(0, Math.min(height, 520 * scaleFactor))
        maximumFlickVelocity: 1700 * scaleFactor
        flickDeceleration: 3000 * scaleFactor

        property bool followTail: true
        property bool customScrollActive: false
        property real wheelTargetY: 0

        function minimumContentY() {
            return originY
        }

        function maximumContentY() {
            return originY + Math.max(0, contentHeight - height)
        }

        function boundedContentY(value) {
            return Math.max(minimumContentY(), Math.min(maximumContentY(), value))
        }

        function nearTail(value) {
            return value >= maximumContentY() - Math.max(2, 3 * root.scaleFactor)
        }

        function handleWheel(pixelDeltaY, angleDeltaY, inverted) {
            var precise = pixelDeltaY !== 0
            var delta = precise ? pixelDeltaY
                                : (angleDeltaY / 120.0) * 72 * root.scaleFactor
            if (inverted)
                delta = -delta
            if (delta === 0)
                return

            cancelFlick()
            var base = wheelScrollAnimation.running ? wheelTargetY : contentY
            wheelTargetY = boundedContentY(base - delta)
            followTail = delta < 0 && nearTail(wheelTargetY)

            if (precise) {
                wheelScrollAnimation.stop()
                customScrollActive = true
                contentY = wheelTargetY
                customScrollActive = false
                if (followTail)
                    tailPositionTimer.restart()
            } else {
                wheelScrollAnimation.from = contentY
                wheelScrollAnimation.to = wheelTargetY
                wheelScrollAnimation.restart()
            }
        }

        onMovementStarted: {
            if (!customScrollActive && !wheelScrollAnimation.running)
                followTail = nearTail(contentY)
        }
        onContentYChanged: {
            if ((dragging || flicking) && !customScrollActive && !wheelScrollAnimation.running)
                followTail = nearTail(contentY)
        }
        onMovementEnded: {
            if (!customScrollActive && !wheelScrollAnimation.running)
                followTail = nearTail(contentY)
        }
        onContentHeightChanged: {
            if (followTail) {
                // Streaming can grow the last delegate while a mouse-wheel
                // animation is still settling. Stop the stale destination and
                // coalesce one exact tail placement for the new geometry.
                if (wheelScrollAnimation.running)
                    wheelScrollAnimation.stop()
                tailPositionTimer.restart()
            }
        }

        Timer {
            id: tailPositionTimer
            interval: 0
            onTriggered: {
                if (messageList.followTail) {
                    messageList.positionViewAtEnd()
                    messageList.returnToBounds()
                    messageList.wheelTargetY = messageList.contentY
                }
            }
        }

        NumberAnimation {
            id: wheelScrollAnimation
            target: messageList
            property: "contentY"
            duration: 145
            easing.type: Easing.OutCubic
            onStarted: messageList.customScrollActive = true
            onStopped: {
                messageList.customScrollActive = false
                messageList.returnToBounds()
                if (messageList.followTail)
                    tailPositionTimer.restart()
            }
        }

        Connections {
            target: dialog_manager.dialogModel
            function onAppended() {
                messageList.followTail = true
                tailPositionTimer.restart()
            }
        }

        delegate: Loader {
            id: messageHost
            width: messageList.width
            height: item ? item.implicitHeight : 0
            property int messageIndex: index
            property string messageContent: model.content || ""
            property string messageReasoning: model.reasoningContent || ""
            property string messageReference: model.reference || ""
            property string messageStatus: model.status || "complete"
            property string messageErrorCode: model.displayErrorCode || ""
            property string messageErrorMessage: model.displayErrorMessage || ""
            property string messageErrorAction: model.errorAction || ""
            property int messageRetryAfterSeconds: model.retryAfterSeconds === undefined
                                                   ? -1 : model.retryAfterSeconds
            property bool messagePartial: model.partial === true
            property bool messageNeutralError: model.neutralError === true
            sourceComponent: model.sender === 0
                             ? userMessageComponent : assistantMessageComponent
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: 6 * root.scaleFactor
            contentItem: Rectangle {
                implicitWidth: 4 * root.scaleFactor
                radius: width / 2
                color: "#C7CCE0"
                opacity: parent.active ? 0.8 : 0.35
            }
            background: Item {}
        }
    }

    // A sibling MouseArea receives each wheel packet once and explicitly
    // accepts it, so ListView's built-in wheel path cannot apply it again.
    // With Qt.NoButton it does not interfere with selection or action buttons.
    MouseArea {
        anchors.fill: messageList
        acceptedButtons: Qt.NoButton
        hoverEnabled: false
        onWheel: {
            messageList.handleWheel(wheel.pixelDelta.y, wheel.angleDelta.y, wheel.inverted)
            wheel.accepted = true
        }
    }
}
