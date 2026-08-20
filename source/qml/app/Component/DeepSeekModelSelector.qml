import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "../Controls"

Item {
    id: selector

    property real scaleFactor: 1.0
    property color accentColor: "#4F46E5"
    property color normalColor: "#F4F4F5"
    property color hoverColor: "#EDEDF2"
    property color textColor: "#18181B"
    property color mutedColor: "#71717A"
    property string fontFamily: ""
    property real maximumWidth: 340 * scaleFactor
    signal popupOpening()
    signal popupClosed()

    readonly property bool pickerVisible: modelPopup.visible
    readonly property string selectedModel: providerSettings.activeModel || qsTr("选择模型")
    readonly property string reasoningMode: providerSettings.activeReasoningMode || "off"
    readonly property string reasoningLabel: reasoningMode === "off" ? qsTr("关闭")
                                            : reasoningMode === "low" ? qsTr("低")
                                            : reasoningMode === "high" ? qsTr("标准")
                                            : reasoningMode === "xhigh" ? qsTr("深度")
                                            : qsTr("极致")
    readonly property var effortOptions: [
        { "key": "off",   "title": qsTr("关闭"), "detail": qsTr("直接回答") },
        { "key": "low",   "title": qsTr("低"),   "detail": qsTr("更快响应") },
        { "key": "high",  "title": qsTr("标准"), "detail": qsTr("质量与速度平衡") },
        { "key": "xhigh", "title": qsTr("深度"), "detail": qsTr("更充分的推理") },
        { "key": "max",   "title": qsTr("极致"), "detail": qsTr("最高思考预算") }
    ]
    readonly property var filteredModels: {
        var needle = searchField.text.trim().toLowerCase()
        var result = []
        var models = providerSettings.availableModels || []
        for (var i = 0; i < models.length; ++i) {
            var name = String(models[i])
            if (needle === "" || name.toLowerCase().indexOf(needle) >= 0)
                result.push(name)
        }
        return result
    }
    readonly property int filteredModelCount: filteredModels.length
    readonly property bool modelListScrollable: modelList.contentHeight > modelList.height + 1
    readonly property bool reasoningOptionsVisible: modelPopup.visible && effortStrip.visible
    readonly property bool emptyStateVisible: emptyStateLabel.visible
    readonly property bool popupFitsAvailableGeometry: !modelPopup.visible
            || (modelPopup.x >= modelPopup.availableRect.x
                && modelPopup.y >= modelPopup.availableRect.y
                && modelPopup.x + modelPopup.width <= modelPopup.availableRect.x + modelPopup.availableRect.width
                && modelPopup.y + modelPopup.height <= modelPopup.availableRect.y + modelPopup.availableRect.height)

    function activeProfile() {
        return providerSettings.profile(providerSettings.activeProfileId) || {}
    }

    function capabilityLabels(modelName) {
        var profile = activeProfile()
        var labels = [qsTr("对话")]
        if (profile.supportsReasoning)
            labels.push(qsTr("思考"))
        if (profile.supportsSearch)
            labels.push(qsTr("搜索"))
        var lower = String(modelName).toLowerCase()
        if (lower.indexOf("vision") >= 0 || lower.indexOf("-vl") >= 0)
            labels.push(qsTr("视觉"))
        return labels
    }

    function togglePicker() {
        if (modelPopup.visible) {
            modelPopup.hidePicker()
            return
        }
        if (!providerSettings.modelDiscoveryInProgress
                && providerSettings.modelDiscoveryMessage === "")
            providerSettings.refreshModels()
        popupOpening()
        modelPopup.showPicker()
    }

    function openPickerForSmoke() {
        if (!modelPopup.visible)
            togglePicker()
    }

    function closePickerForSmoke() {
        modelPopup.hidePicker()
    }

    function setSearchForSmoke(text) {
        searchField.text = text
    }

    TextMetrics {
        id: modelTextMetrics
        font.family: selector.fontFamily
        font.pixelSize: 13 * selector.scaleFactor
        text: selector.selectedModel
    }
    TextMetrics {
        id: effortTextMetrics
        font.family: selector.fontFamily
        font.pixelSize: 12 * selector.scaleFactor
        text: "· " + selector.reasoningLabel
    }

    readonly property real preferredWidth: modelTextMetrics.advanceWidth
                                               + effortTextMetrics.advanceWidth
                                               + 82 * scaleFactor
    height: 32 * scaleFactor
    width: Math.max(210 * scaleFactor, Math.min(maximumWidth, preferredWidth))

    Button {
        id: trigger
        anchors.fill: parent
        hoverEnabled: true
        background: Item {
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: trigger.down ? "#E4E4E7"
                      : trigger.hovered ? selector.hoverColor : selector.normalColor
                border.width: modelPopup.visible ? Math.max(1, selector.scaleFactor) : 0
                border.color: selector.accentColor
            }
        }
        contentItem: RowLayout {
            spacing: 7 * selector.scaleFactor
            Rectangle {
                Layout.preferredWidth: 20 * selector.scaleFactor
                Layout.preferredHeight: 20 * selector.scaleFactor
                radius: width / 2
                color: selector.reasoningMode === "off" ? "#E4E4E7" : "#EEF2FF"
                Image {
                    anchors.centerIn: parent
                    width: 17 * selector.scaleFactor
                    height: 17 * selector.scaleFactor
                    source: selector.reasoningMode === "off"
                            ? "qrc:/Image/Icon/thinking_off.svg"
                            : "qrc:/Image/Icon/thinking_on.svg"
                }
            }
            Text {
                id: triggerLabel
                Layout.fillWidth: true
                text: selector.selectedModel
                color: selector.textColor
                elide: Text.ElideMiddle
                font.pixelSize: 13 * selector.scaleFactor
                font.family: selector.fontFamily
            }
            Text {
                text: "· " + selector.reasoningLabel
                color: selector.mutedColor
                font.pixelSize: 12 * selector.scaleFactor
                font.family: selector.fontFamily
            }
            Image {
                Layout.preferredWidth: 14 * selector.scaleFactor
                Layout.preferredHeight: 14 * selector.scaleFactor
                source: "qrc:/Image/Icon/pullUp.svg"
                fillMode: Image.PreserveAspectFit
                rotation: modelPopup.visible ? 0 : 180
            }
        }
        ModernToolTip {
            anchorItem: trigger
            visible: trigger.hovered && triggerLabel.truncated
            text: selector.selectedModel
            displayAbove: true
        }
        onClicked: selector.togglePicker()
    }

    Window {
        id: modelPopup
        objectName: "modelPickerPopup"
        transientParent: selector.Window.window
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
        modality: Qt.NonModal
        color: "transparent"
        visible: false
        title: qsTr("选择模型与思考强度")
        property rect availableRect: Qt.rect(0, 0, 1280, 720)
        property bool mayCloseOnDeactivate: false

        function updatePlacement() {
            var host = selector.Window.window
            var screen = host ? host.screen : null
            var available = window && typeof window.availableGeometry === "function"
                    ? window.availableGeometry() : Qt.rect(
                          screen ? screen.virtualX : 0,
                          screen ? screen.virtualY : 0,
                          screen ? screen.desktopAvailableWidth : 1280,
                          screen ? screen.desktopAvailableHeight : 720)
            if (!available || available.width <= 0 || available.height <= 0)
                available = Qt.rect(0, 0, 1280, 720)
            availableRect = Qt.rect(available.x, available.y,
                                    available.width, available.height)
            var margin = 8
            var anchor = selector.mapToGlobal(Qt.point(0, 0))
            var desiredWidth = Math.max(340 * selector.scaleFactor, selector.width)
            width = Math.max(260, Math.min(desiredWidth, available.width - margin * 2))
            var visibleRows = Math.max(2, Math.min(4, selector.filteredModelCount))
            var desiredHeight = (294 + (visibleRows - 2) * 40) * selector.scaleFactor
            height = Math.max(300, Math.min(desiredHeight, available.height - margin * 2))
            x = Math.round(Math.max(available.x + margin,
                                   Math.min(available.x + available.width - width - margin,
                                            anchor.x)))
            var above = anchor.y - height - 8 * selector.scaleFactor
            var below = anchor.y + selector.height + 8 * selector.scaleFactor
            if (above >= available.y + margin)
                y = Math.round(above)
            else if (below + height <= available.y + available.height - margin)
                y = Math.round(below)
            else
                y = Math.round(Math.max(available.y + margin,
                                       Math.min(available.y + available.height - height - margin,
                                                above)))
        }

        function showPicker() {
            searchField.text = ""
            updatePlacement()
            mayCloseOnDeactivate = false
            show()
            raise()
            requestActivate()
            closeArm.restart()
        }

        function hidePicker() {
            if (visible)
                hide()
        }

        onVisibleChanged: if (!visible) selector.popupClosed()
        onActiveChanged: {
            if (visible && !active && mayCloseOnDeactivate)
                Qt.callLater(hidePicker)
        }

        Timer {
            id: closeArm
            interval: 120
            onTriggered: modelPopup.mayCloseOnDeactivate = true
        }
        Shortcut {
            sequence: "Escape"
            enabled: modelPopup.visible
            onActivated: modelPopup.hidePicker()
        }

        Rectangle {
            anchors.fill: parent
            radius: 12 * selector.scaleFactor
            color: "#FFFFFF"
            border.color: "#D4D4D8"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12 * selector.scaleFactor
                spacing: 7 * selector.scaleFactor

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30 * selector.scaleFactor
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("模型")
                        color: selector.textColor
                        font.bold: true
                        font.pixelSize: 13 * selector.scaleFactor
                        font.family: selector.fontFamily
                    }
                    BusyIndicator {
                        Layout.preferredWidth: 20 * selector.scaleFactor
                        Layout.preferredHeight: 20 * selector.scaleFactor
                        running: providerSettings.modelDiscoveryInProgress
                        visible: running
                    }
                    Button {
                        id: refreshButton
                        Layout.preferredWidth: 64 * selector.scaleFactor
                        Layout.preferredHeight: 28 * selector.scaleFactor
                        text: providerSettings.modelDiscoveryMessage !== ""
                              && providerSettings.availableModels.length === 0
                              ? qsTr("重试") : qsTr("刷新")
                        enabled: !providerSettings.modelDiscoveryInProgress
                        hoverEnabled: true
                        background: Rectangle {
                            radius: 8 * selector.scaleFactor
                            color: refreshButton.down ? "#E4E4E7"
                                  : refreshButton.hovered ? "#F4F4F5" : "#FAFAFA"
                            border.width: 1
                            border.color: refreshButton.activeFocus ? selector.accentColor : "#D4D4D8"
                        }
                        contentItem: Text {
                            text: refreshButton.text
                            color: selector.textColor
                            font.pixelSize: 12 * selector.scaleFactor
                            font.family: selector.fontFamily
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: providerSettings.refreshModels()
                    }
                }

                TextField {
                    id: searchField
                    objectName: "modelSearchField"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34 * selector.scaleFactor
                    placeholderText: qsTr("搜索模型")
                    selectByMouse: true
                    leftPadding: 10 * selector.scaleFactor
                    rightPadding: 10 * selector.scaleFactor
                }

                ListView {
                    id: modelList
                    objectName: "modelList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 64 * selector.scaleFactor
                    Layout.maximumHeight: 150 * selector.scaleFactor
                    clip: true
                    spacing: 3 * selector.scaleFactor
                    model: selector.filteredModels
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: ItemDelegate {
                        id: modelRow
                        width: modelList.width
                        height: 40 * selector.scaleFactor
                        highlighted: modelData === selector.selectedModel
                        hoverEnabled: true
                        background: Rectangle {
                            radius: 8 * selector.scaleFactor
                            color: modelRow.highlighted ? "#EEF2FF"
                                  : modelRow.hovered ? "#F4F4F5" : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: 5 * selector.scaleFactor
                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                color: selector.textColor
                                font.pixelSize: 12 * selector.scaleFactor
                                font.family: selector.fontFamily
                                elide: Text.ElideMiddle
                            }
                            Repeater {
                                model: selector.capabilityLabels(modelData)
                                delegate: Rectangle {
                                    Layout.preferredWidth: capabilityText.implicitWidth + 10 * selector.scaleFactor
                                    Layout.preferredHeight: 20 * selector.scaleFactor
                                    radius: 6 * selector.scaleFactor
                                    color: "#F1F5F9"
                                    Text {
                                        id: capabilityText
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: "#475569"
                                        font.pixelSize: 9 * selector.scaleFactor
                                        font.family: selector.fontFamily
                                    }
                                }
                            }
                        }
                        onClicked: {
                            if (providerSettings.setActiveModel(modelData))
                                modelPopup.hidePicker()
                        }
                    }
                }

                Label {
                    id: emptyStateLabel
                    objectName: "modelEmptyState"
                    Layout.fillWidth: true
                    visible: selector.filteredModelCount === 0
                    text: searchField.text.trim() !== "" ? qsTr("没有匹配的模型")
                          : providerSettings.modelDiscoveryMessage !== ""
                            ? providerSettings.modelDiscoveryMessage : qsTr("暂无模型，请刷新重试")
                    color: providerSettings.modelDiscoveryMessage !== "" ? "#B45309" : selector.mutedColor
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11 * selector.scaleFactor
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#E4E4E7"
                }
                Text {
                    text: qsTr("思考强度")
                    color: selector.textColor
                    font.bold: true
                    font.pixelSize: 13 * selector.scaleFactor
                    font.family: selector.fontFamily
                }
                RowLayout {
                    id: effortStrip
                    objectName: "reasoningEffortStrip"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 38 * selector.scaleFactor
                    Layout.preferredHeight: 38 * selector.scaleFactor
                    Layout.maximumHeight: 38 * selector.scaleFactor
                    spacing: 4 * selector.scaleFactor
                    Repeater {
                        model: selector.effortOptions
                        delegate: Button {
                            id: effortButton
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            hoverEnabled: true
                            background: Rectangle {
                                radius: 8 * selector.scaleFactor
                                color: modelData.key === selector.reasoningMode ? "#EEF2FF"
                                      : effortButton.hovered ? "#F4F4F5" : "transparent"
                                border.width: modelData.key === selector.reasoningMode ? 1 : 0
                                border.color: selector.accentColor
                            }
                            contentItem: Text {
                                text: modelData.title
                                color: selector.textColor
                                font.pixelSize: 11 * selector.scaleFactor
                                font.family: selector.fontFamily
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.detail
                            onClicked: {
                                if (providerSettings.setActiveReasoningMode(modelData.key))
                                    modelPopup.hidePicker()
                            }
                        }
                    }
                }
            }
        }
    }
}
