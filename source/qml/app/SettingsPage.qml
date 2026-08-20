import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "Controls"

ApplicationWindow {
    id: root
    width: Math.max(minimumWidth, Math.min(700, Screen.desktopAvailableWidth - 32))
    height: Math.max(minimumHeight, Math.min(700, Screen.desktopAvailableHeight - 32))
    minimumWidth: 440
    minimumHeight: 500
    // Settings is deliberately a normal app window: when focus moves to
    // another application the user can recover it from taskbar/Alt+Tab.
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    color: "transparent"
    title: qsTr("智键 AI 设置")

    property string editingProfileId: ""
    property string editingCapabilitySchema: ""
    property string localStatusMessage: ""
    property bool localStatusOk: false
    property bool dirty: false
    property bool loadingProfile: false
    property bool advancedExpanded: false
    property int pendingProfileIndex: -2
    property string pendingAction: ""
    property string destructiveAction: ""
    property string destructiveTitle: ""
    property string destructiveMessage: ""
    property string profileNameError: ""
    property string baseUrlError: ""
    property string chatPathError: ""
    property string modelNameError: ""
    property string authHeaderError: ""
    readonly property bool testingConnection: providerSettings.lastTestStatus === "testing"
    readonly property bool ctaBarVisibleAtMinimumHeight: settingsCtaBar.visible
            && settingsCtaBar.y >= 0 && settingsCtaBar.y + settingsCtaBar.height <= settingsCtaBar.parent.height
    readonly property bool dirtyConfirmationVisible: unsavedConfirmation.visible
    readonly property bool destructiveConfirmationVisible: destructiveConfirmation.visible
    readonly property bool hotkeyRecording: hotkeyRecorder.recording

    Loader {
        id: theme
        source: "qrc:/Theme/LightTheme.qml"
        onLoaded: Qt.application.themeManager = item
    }

    function markDirty() {
        if (!loadingProfile)
            dirty = true
    }

    function editingProfileIndex() {
        for (var i = 0; i < providerSettings.profiles.length; ++i) {
            if (providerSettings.profiles[i].id === editingProfileId)
                return i
        }
        return -1
    }

    function validateFields() {
        profileNameError = profileName.text.trim() === "" ? qsTr("请输入配置名称") : ""
        modelNameError = modelName.text.trim() === "" ? qsTr("请输入模型名称") : ""
        baseUrlError = /^https:\/\/[^\s/]+/i.test(baseUrl.text.trim())
                       ? "" : qsTr("请输入有效的 HTTPS Base URL")
        chatPathError = chatPath.text.trim().indexOf("/") === 0
                        ? "" : qsTr("Chat Path 必须以 / 开头")
        authHeaderError = authScheme.currentText === "api-key"
                          && authHeaderName.text.trim() === ""
                          ? qsTr("API-Key 鉴权需要 Header 名称") : ""
        if (baseUrlError !== "" || chatPathError !== "" || authHeaderError !== "")
            advancedExpanded = true
        return profileNameError === "" && modelNameError === ""
                && baseUrlError === "" && chatPathError === ""
                && authHeaderError === ""
    }

    function shortcutText(key, modifiers) {
        var keyName = ""
        if (key >= Qt.Key_A && key <= Qt.Key_Z)
            keyName = String.fromCharCode(key)
        else if (key >= Qt.Key_0 && key <= Qt.Key_9)
            keyName = String.fromCharCode(key)
        else if (key === Qt.Key_Space)
            keyName = "Space"
        else if (key >= Qt.Key_F1 && key <= Qt.Key_F24)
            keyName = "F" + (key - Qt.Key_F1 + 1)
        if (keyName === "") return ""

        var parts = []
        if (modifiers & Qt.ControlModifier) parts.push("Ctrl")
        if (modifiers & Qt.AltModifier) parts.push("Alt")
        if (modifiers & Qt.ShiftModifier) parts.push("Shift")
        if (modifiers & Qt.MetaModifier) parts.push("Meta")
        parts.push(keyName)
        return parts.join("+")
    }

    function setDeepSeekDefaults() {
        profileName.text = "DeepSeek"
        baseUrl.text = "https://api.deepseek.com"
        chatPath.text = "/chat/completions"
        modelName.text = "deepseek-v4-flash"
        authScheme.currentIndex = 0
        authHeaderName.text = "Authorization"
        thinkingEnabled.checked = true
        reasoningEffort.currentIndex = 1
        timeoutMs.value = 60000
        contextLimit.value = 32768
        outputLimit.value = 2048
    }

    function selectProfile(index) {
        loadingProfile = true
        apiKey.text = ""
        localStatusMessage = ""
        if (index < 0 || index >= providerSettings.profiles.length) {
            editingProfileId = ""
            editingCapabilitySchema = ""
            providerPreset.currentIndex = 0
            setDeepSeekDefaults()
            loadingProfile = false
            dirty = false
            return
        }
        var profile = providerSettings.profiles[index]
        editingProfileId = profile.id || ""
        editingCapabilitySchema = profile.capabilitySchema || ""
        var deepSeek = (profile.providerType || "") === "deepseek"
                       || (profile.baseUrl || "").indexOf("api.deepseek.com") >= 0
        providerPreset.currentIndex = deepSeek ? 0 : 1
        profileName.text = profile.name || ""
        baseUrl.text = profile.baseUrl || ""
        chatPath.text = profile.chatPath || (deepSeek ? "/chat/completions" : "/v1/chat/completions")
        modelName.text = profile.model || (deepSeek ? "deepseek-v4-flash" : "")
        var scheme = (profile.authScheme || "bearer").toLowerCase()
        authScheme.currentIndex = scheme === "none" ? 2 : (scheme === "api-key" ? 1 : 0)
        authHeaderName.text = profile.authHeaderName || "Authorization"
        thinkingEnabled.checked = profile.thinkingEnabled === undefined ? deepSeek : profile.thinkingEnabled
        var effort = (profile.reasoningEffort || "high").toLowerCase()
        reasoningEffort.currentIndex = effort === "low" ? 0
                                       : (effort === "xhigh" ? 2 : (effort === "max" ? 3 : 1))
        timeoutMs.value = profile.timeoutMs || 60000
        contextLimit.value = profile.contextLimit || 32768
        outputLimit.value = profile.outputLimit || 2048
        loadingProfile = false
        dirty = false
    }

    function applyProfileSelection(index) {
        profilePicker.currentIndex = index
        selectProfile(index)
        if (index >= 0)
            providerSettings.setActiveProfile(providerSettings.profiles[index].id)
    }

    function requestProfileSelection(index) {
        if (!dirty) {
            applyProfileSelection(index)
            return
        }
        pendingProfileIndex = index
        pendingAction = "switch"
        profilePicker.currentIndex = editingProfileIndex()
        unsavedConfirmation.open()
    }

    function requestClose() {
        if (!dirty) {
            hide()
            return
        }
        pendingAction = "close"
        unsavedConfirmation.open()
    }

    function requestOpenChat() {
        if (!dirty) {
            window.revealAndActivate()
            hide()
            return
        }
        pendingAction = "openChat"
        unsavedConfirmation.open()
    }

    function reloadProfiles() {
        var selected = -1
        for (var i = 0; i < providerSettings.profiles.length; ++i) {
            if (providerSettings.profiles[i].id === providerSettings.activeProfileId) {
                selected = i
                break
            }
        }
        profilePicker.currentIndex = selected
        selectProfile(selected)
    }

    function saveCurrentProfile(openChat) {
        if (!validateFields()) {
            localStatusOk = false
            localStatusMessage = qsTr("请修正标红字段后再保存。")
            return false
        }
        // Profile metadata and its credential are committed as one operation.
        // Editing an endpoint/authentication binding requires a freshly entered key;
        // the backend rejects the save before an old key can be reused for a new host.
        var pendingApiKey = apiKey.text
        var id = providerSettings.saveProfileWithCredential({
            "id": editingProfileId,
            "name": profileName.text,
            "baseUrl": baseUrl.text,
            "chatPath": chatPath.text,
            "model": modelName.text,
            "providerType": providerPreset.currentIndex === 0 ? "deepseek" : "custom",
            "authScheme": authScheme.currentText,
            "authHeaderName": authHeaderName.text,
            "timeoutMs": timeoutMs.value,
            "contextLimit": contextLimit.value,
            "outputLimit": outputLimit.value,
            "capabilitySchema": editingCapabilitySchema,
            "thinkingEnabled": thinkingEnabled.checked,
            "reasoningEffort": reasoningEffort.currentText
        }, pendingApiKey)
        pendingApiKey = ""
        if (id === "") {
            localStatusOk = false
            localStatusMessage = qsTr("保存失败：请检查 URL、鉴权配置和模型名称；更换接口地址或鉴权方式时必须重新输入 API Key。")
            return false
        }
        apiKey.text = ""
        providerSettings.setActiveProfile(id)
        editingProfileId = id
        if (!providerSettings.configured) {
            localStatusOk = false
            localStatusMessage = qsTr("Profile 已保存，但 Bearer/API-Key 鉴权仍需填写密钥。")
            return false
        }
        providerSettings.startupCompleted = true
        localStatusOk = true
        localStatusMessage = qsTr("保存成功。最终请求地址：%1").arg(finalUrl.text)
        dirty = false
        if (openChat) {
            window.revealAndActivate()
            Qt.callLater(function() { root.hide() })
        }
        return true
    }

    function saveAndTest() {
        if (!testingConnection && saveCurrentProfile(false))
            providerSettings.testConnection(editingProfileId)
    }

    Connections {
        target: providerSettings
        function onProfilesChanged() { root.reloadProfiles() }
        function onActiveProfileIdChanged() { root.reloadProfiles() }
        function onOperationFailed(operation, message) {
            root.localStatusOk = false
            root.localStatusMessage = message || qsTr("操作失败，请稍后重试。")
        }
    }

    Component.onCompleted: reloadProfiles()
    onClosing: function(close) {
        if (dirty) {
            close.accepted = false
            pendingAction = "close"
            unsavedConfirmation.open()
        }
    }

    Dialog {
        id: unsavedConfirmation
        objectName: "unsavedChangesConfirmation"
        anchors.centerIn: parent
        width: Math.min(400, root.width - 40)
        modal: true
        title: qsTr("有未保存的更改")
        closePolicy: Popup.CloseOnEscape
        contentItem: Label {
            width: unsavedConfirmation.availableWidth
            padding: 14
            wrapMode: Text.WordWrap
            text: qsTr("当前 Provider 配置尚未保存。要放弃这些更改吗？")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("继续编辑"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button { text: qsTr("放弃更改"); DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            onRejected: unsavedConfirmation.close()
            onDiscarded: {
                root.dirty = false
                unsavedConfirmation.close()
                if (root.pendingAction === "switch")
                    root.applyProfileSelection(root.pendingProfileIndex)
                else if (root.pendingAction === "close")
                    root.hide()
                else if (root.pendingAction === "openChat") {
                    window.revealAndActivate()
                    root.hide()
                }
                root.pendingAction = ""
                root.pendingProfileIndex = -2
            }
        }
    }

    Dialog {
        id: destructiveConfirmation
        objectName: "providerDestructiveConfirmation"
        anchors.centerIn: parent
        width: Math.min(400, root.width - 40)
        modal: true
        title: root.destructiveTitle
        closePolicy: Popup.CloseOnEscape
        contentItem: ColumnLayout {
            width: destructiveConfirmation.availableWidth
            spacing: 8
            Label {
                Layout.fillWidth: true
                padding: 14
                wrapMode: Text.WordWrap
                text: root.destructiveMessage
            }
            Label {
                Layout.fillWidth: true
                leftPadding: 14
                rightPadding: 14
                bottomPadding: 8
                wrapMode: Text.WordWrap
                color: uiPreferences.dangerColor
                text: root.localStatusMessage
                visible: text !== "" && !root.localStatusOk
            }
        }
        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            ModernButton {
                text: qsTr("取消")
                onClicked: destructiveConfirmation.close()
            }
            ModernButton {
                text: root.destructiveAction === "delete" ? qsTr("删除") : qsTr("清除密钥")
                highlighted: true
                onClicked: {
                    var succeeded = root.destructiveAction === "delete"
                            ? providerSettings.removeProfile(root.editingProfileId)
                            : providerSettings.clearCredential(root.editingProfileId)
                    if (!succeeded) {
                        root.localStatusOk = false
                        root.localStatusMessage = qsTr("操作失败，未更改当前配置。请检查错误后重试。")
                        return
                    }
                    root.localStatusOk = true
                    root.localStatusMessage = root.destructiveAction === "delete"
                            ? qsTr("Profile 及其保存的密钥已删除。")
                            : qsTr("保存的密钥已清除。")
                    apiKey.text = ""
                    if (root.destructiveAction === "credential") {
                        var retainedId = root.editingProfileId
                        root.editingProfileId = ""
                        root.editingProfileId = retainedId
                    }
                    root.dirty = false
                    destructiveConfirmation.close()
                }
            }
        }
    }

    Dialog {
        id: deleteLocalDataConfirmation
        anchors.centerIn: parent
        width: Math.min(400, root.width - 40)
        modal: true
        title: qsTr("请求清除本地数据")
        standardButtons: Dialog.Yes | Dialog.No
        closePolicy: Popup.CloseOnEscape
        onAccepted: privacyConsent.requestDataDeletion()
        contentItem: Label {
            width: deleteLocalDataConfirmation.availableWidth
            padding: 14
            wrapMode: Text.WordWrap
            text: qsTr("确认发出清除本地数据请求？此入口当前只通知后端，不会在本步骤自动删除聊天、日志或配置。后续执行删除前仍应显示具体范围。")
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: uiPreferences.radiusLg
        color: uiPreferences.backgroundColor
        border.color: uiPreferences.borderColor

        Rectangle {
            id: titleBar
            width: parent.width
            height: 54
            color: "transparent"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("智键 AI 设置")
                font.pixelSize: uiPreferences.fontTitle
                font.bold: true
                color: uiPreferences.textPrimaryColor
            }

            ModernButton {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 40
                height: 40
                text: "×"
                flat: true
                font.pixelSize: 22
                Accessible.name: qsTr("关闭设置")
                accessibleDescription: qsTr("关闭设置窗口；有未保存更改时会先确认。")
                onClicked: root.requestClose()
            }

            MouseArea {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.rightMargin: 58
                onPressed: root.startSystemMove()
            }
        }

        TabBar {
            id: tabs
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            TabButton {
                text: qsTr("AI 配置")
                activeFocusOnTab: true
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                Accessible.checked: checked
                background: Rectangle {
                    color: parent.checked ? uiPreferences.accentColor : uiPreferences.disabledSurfaceColor
                    radius: uiPreferences.radiusSm
                    border.width: parent.activeFocus ? 2 : 0
                    border.color: uiPreferences.focusRingColor
                }
                contentItem: Text { text: parent.text; color: parent.checked ? uiPreferences.surfaceColor : uiPreferences.textPrimaryColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            TabButton {
                text: qsTr("快捷键与后台")
                activeFocusOnTab: true
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                Accessible.checked: checked
                background: Rectangle {
                    color: parent.checked ? uiPreferences.accentColor : uiPreferences.disabledSurfaceColor
                    radius: uiPreferences.radiusSm
                    border.width: parent.activeFocus ? 2 : 0
                    border.color: uiPreferences.focusRingColor
                }
                contentItem: Text { text: parent.text; color: parent.checked ? uiPreferences.surfaceColor : uiPreferences.textPrimaryColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            TabButton {
                objectName: "dataPrivacyTab"
                text: qsTr("数据与隐私")
                activeFocusOnTab: true
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                Accessible.checked: checked
                background: Rectangle {
                    color: parent.checked ? uiPreferences.accentColor : uiPreferences.disabledSurfaceColor
                    radius: uiPreferences.radiusSm
                    border.width: parent.activeFocus ? 2 : 0
                    border.color: uiPreferences.focusRingColor
                }
                contentItem: Text { text: parent.text; color: parent.checked ? uiPreferences.surfaceColor : uiPreferences.textPrimaryColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        StackLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabs.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 18
            currentIndex: tabs.currentIndex

            Item {
                ScrollView {
                    id: apiScroll
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: settingsCtaBar.top
                    anchors.bottomMargin: 10
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: Math.max(0, apiScroll.availableWidth - 4)
                        spacing: 10

                        GroupBox {
                            title: qsTr("基础配置")
                            Layout.fillWidth: true
                            label: Label {
                                text: qsTr("基础配置")
                                color: uiPreferences.textPrimaryColor
                                font.pixelSize: uiPreferences.fontBody
                                font.bold: true
                                leftPadding: 8
                            }
                            background: Rectangle {
                                color: uiPreferences.surfaceColor
                                radius: uiPreferences.radiusMd
                                border.color: uiPreferences.borderColor
                            }

                            GridLayout {
                                anchors.fill: parent
                                columns: 3
                                columnSpacing: 10
                                rowSpacing: 9

                                Label { text: qsTr("配置") }
                                ModernComboBox {
                                    id: profilePicker
                                    Layout.fillWidth: true
                                    model: providerSettings.profiles
                                    textRole: "name"
                                    onActivated: root.requestProfileSelection(index)
                                }
                                ModernButton {
                                    text: qsTr("新建")
                                    onClicked: root.requestProfileSelection(-1)
                                }

                                Label { text: qsTr("接口预设") }
                                ModernComboBox {
                                    id: providerPreset
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    model: ["DeepSeek", qsTr("自定义 OpenAI-compatible")]
                                    onActivated: {
                                        root.editingCapabilitySchema = ""
                                        if (index === 0) root.setDeepSeekDefaults()
                                        root.markDirty()
                                    }
                                }

                                Label { text: qsTr("名称") }
                                ModernTextField {
                                    id: profileName
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    accessibleName: qsTr("配置名称")
                                    onTextEdited: { root.profileNameError = ""; root.markDirty() }
                                }
                                Item { visible: root.profileNameError !== "" }
                                Label {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.profileNameError !== ""
                                    text: root.profileNameError
                                    color: uiPreferences.dangerColor
                                    font.pixelSize: uiPreferences.fontSmall
                                }

                                Label { text: qsTr("模型") }
                                ModernTextField {
                                    id: modelName
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    placeholderText: "deepseek-v4-flash"
                                    accessibleName: qsTr("模型名称")
                                    onTextEdited: { root.modelNameError = ""; root.markDirty() }
                                }
                                Item { visible: root.modelNameError !== "" }
                                Label {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.modelNameError !== ""
                                    text: root.modelNameError
                                    color: uiPreferences.dangerColor
                                    font.pixelSize: uiPreferences.fontSmall
                                }

                                Label { text: qsTr("API Key") }
                                RowLayout {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    ModernTextField {
                                        id: apiKey
                                        Layout.fillWidth: true
                                        echoMode: showKey.checked ? TextInput.Normal : TextInput.Password
                                        placeholderText: editingProfileId !== "" && providerSettings.hasCredential(editingProfileId)
                                                         ? qsTr("已加密保存；留空不修改") : qsTr("输入 DeepSeek API Key")
                                        accessibleName: qsTr("API Key")
                                        accessibleDescription: qsTr("密钥输入框，内容默认隐藏。")
                                        onTextEdited: root.markDirty()
                                    }
                                    CheckBox { id: showKey; text: qsTr("显示") }
                                }

                                Label { text: qsTr("凭据状态") }
                                Label {
                                    Layout.fillWidth: true
                                    color: editingProfileId !== "" && providerSettings.hasCredential(editingProfileId)
                                           ? uiPreferences.successColor : uiPreferences.warningColor
                                    text: editingProfileId !== "" && providerSettings.hasCredential(editingProfileId)
                                          ? qsTr("已由 DPAPI 加密保存") : qsTr("尚未保存")
                                }
                                ModernButton {
                                    text: qsTr("清除密钥…")
                                    enabled: editingProfileId !== "" && providerSettings.hasCredential(editingProfileId)
                                    onClicked: {
                                        root.localStatusMessage = ""
                                        root.destructiveAction = "credential"
                                        root.destructiveTitle = qsTr("清除已保存的密钥？")
                                        root.destructiveMessage = qsTr("将删除此 Profile 由 DPAPI 加密保存的 API Key。其他配置字段会保留。")
                                        destructiveConfirmation.open()
                                    }
                                }

                                Label { text: qsTr("Thinking") }
                                ModernSwitch {
                                    id: thinkingEnabled
                                    text: checked ? qsTr("启用") : qsTr("关闭")
                                    onToggled: root.markDirty()
                                }
                                ModernComboBox {
                                    id: reasoningEffort
                                    Layout.fillWidth: true
                                    enabled: thinkingEnabled.checked
                                    model: ["low", "high", "xhigh", "max"]
                                    onActivated: root.markDirty()
                                }

                                ModernButton {
                                    objectName: "advancedConnectionToggle"
                                    Layout.columnSpan: 3
                                    Layout.fillWidth: true
                                    text: root.advancedExpanded
                                          ? qsTr("收起高级连接设置 ▲") : qsTr("高级连接设置 ▼")
                                    onClicked: root.advancedExpanded = !root.advancedExpanded
                                }

                                Label { text: qsTr("Base URL"); visible: root.advancedExpanded }
                                ModernTextField {
                                    id: baseUrl
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded
                                    placeholderText: "https://api.deepseek.com"
                                    accessibleName: qsTr("Base URL")
                                    onTextEdited: { root.baseUrlError = ""; root.markDirty() }
                                }
                                Item { visible: root.advancedExpanded && root.baseUrlError !== "" }
                                Label {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded && root.baseUrlError !== ""
                                    text: root.baseUrlError
                                    color: uiPreferences.dangerColor
                                    font.pixelSize: uiPreferences.fontSmall
                                }

                                Label { text: qsTr("Chat Path"); visible: root.advancedExpanded }
                                ModernTextField {
                                    id: chatPath
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded
                                    placeholderText: "/chat/completions"
                                    accessibleName: qsTr("Chat Path")
                                    onTextEdited: { root.chatPathError = ""; root.markDirty() }
                                }
                                Item { visible: root.advancedExpanded && root.chatPathError !== "" }
                                Label {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded && root.chatPathError !== ""
                                    text: root.chatPathError
                                    color: uiPreferences.dangerColor
                                    font.pixelSize: uiPreferences.fontSmall
                                }

                                Label { text: qsTr("鉴权"); visible: root.advancedExpanded }
                                ModernComboBox {
                                    id: authScheme
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded
                                    model: ["bearer", "api-key", "none"]
                                    onCurrentTextChanged: if (currentText === "bearer") authHeaderName.text = "Authorization"
                                    onActivated: root.markDirty()
                                }
                                ModernTextField {
                                    id: authHeaderName
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded
                                    enabled: authScheme.currentText === "api-key"
                                    placeholderText: "api-key"
                                    accessibleName: qsTr("鉴权 Header 名称")
                                    onTextEdited: { root.authHeaderError = ""; root.markDirty() }
                                }
                                Item { visible: root.advancedExpanded && root.authHeaderError !== "" }
                                Label {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded && root.authHeaderError !== ""
                                    text: root.authHeaderError
                                    color: uiPreferences.dangerColor
                                    font.pixelSize: uiPreferences.fontSmall
                                }

                                Label { text: qsTr("超时"); visible: root.advancedExpanded }
                                SpinBox {
                                    id: timeoutMs
                                    visible: root.advancedExpanded
                                    from: 5000; to: 300000; stepSize: 5000; value: 60000
                                    onValueModified: root.markDirty()
                                }
                                Label { text: qsTr("毫秒"); color: uiPreferences.textSecondaryColor; visible: root.advancedExpanded }

                                Label { text: qsTr("上下文预算"); visible: root.advancedExpanded }
                                SpinBox {
                                    id: contextLimit
                                    visible: root.advancedExpanded
                                    from: 256; to: 2097152; stepSize: 1024; value: 32768
                                    onValueModified: root.markDirty()
                                }
                                Label { text: qsTr("Token（估算）"); color: uiPreferences.textSecondaryColor; visible: root.advancedExpanded }

                                Label { text: qsTr("最大输出"); visible: root.advancedExpanded }
                                SpinBox {
                                    id: outputLimit
                                    visible: root.advancedExpanded
                                    from: 1; to: 65536; stepSize: 256; value: 2048
                                    onValueModified: root.markDirty()
                                }
                                Label { text: qsTr("Token"); color: uiPreferences.textSecondaryColor; visible: root.advancedExpanded }

                                Label { text: qsTr("最终 URL"); visible: root.advancedExpanded }
                                Label {
                                    id: finalUrl
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    visible: root.advancedExpanded
                                    elide: Text.ElideMiddle
                                    color: uiPreferences.textSecondaryColor
                                    text: baseUrl.text.replace(/\/$/, "") + "/" + chatPath.text.replace(/^\//, "")
                                }

                                RowLayout {
                                    Layout.columnSpan: 3
                                    Layout.fillWidth: true
                                    Item { Layout.fillWidth: true }
                                    ModernButton {
                                        objectName: "deleteProfileButton"
                                        text: qsTr("删除 Profile…")
                                        enabled: editingProfileId !== ""
                                        onClicked: {
                                            root.localStatusMessage = ""
                                            root.destructiveAction = "delete"
                                            root.destructiveTitle = qsTr("删除此 Profile？")
                                            root.destructiveMessage = qsTr("将删除此 Profile 及其由 DPAPI 加密保存的 API Key。该操作不可撤销。")
                                            destructiveConfirmation.open()
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: uiPreferences.textSecondaryColor
                            text: qsTr("测试连接会向第三方 Provider 发出最小 API 调用，可能产生少量费用。DeepSeek 正常对话会按设置发送 thinking；密钥只写入当前 Windows 用户的 DPAPI 加密存储。")
                        }
                    }
                }

                Rectangle {
                    id: settingsCtaBar
                    objectName: "settingsCtaBar"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 104
                    radius: uiPreferences.radiusMd
                    color: uiPreferences.surfaceColor
                    border.color: uiPreferences.borderColor
                    z: 10

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            BusyIndicator {
                                objectName: "settingsTestBusy"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                running: root.testingConnection
                                visible: running
                            }
                            Label {
                                objectName: "settingsActionStatus"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: root.testingConnection ? uiPreferences.warningColor
                                      : (root.localStatusOk || providerSettings.lastTestStatus === "succeeded")
                                        ? uiPreferences.successColor : uiPreferences.dangerColor
                                text: root.testingConnection
                                      ? qsTr("正在发起最小 API 调用（最多输出 4 token，可能产生少量费用）…")
                                      : (root.localStatusMessage !== "" ? root.localStatusMessage
                                         : providerSettings.lastTestMessage)
                            }
                            Label {
                                text: root.dirty ? qsTr("未保存") : qsTr("已同步")
                                color: root.dirty ? uiPreferences.warningColor : uiPreferences.textSecondaryColor
                                font.pixelSize: uiPreferences.fontSmall
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            ModernButton {
                                objectName: "testConnectionButton"
                                Layout.fillWidth: true
                                text: root.testingConnection ? qsTr("测试中…") : qsTr("测试连接")
                                enabled: !root.testingConnection
                                onClicked: root.saveAndTest()
                            }
                            ModernButton {
                                objectName: "saveProfileButton"
                                Layout.fillWidth: true
                                text: qsTr("保存")
                                enabled: !root.testingConnection
                                onClicked: root.saveCurrentProfile(false)
                            }
                            ModernButton {
                                objectName: "saveAndChatButton"
                                Layout.fillWidth: true
                                text: qsTr("保存并开始对话")
                                enabled: !root.testingConnection
                                highlighted: true
                                onClicked: root.saveCurrentProfile(true)
                            }
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 12

                    GroupBox {
                        title: qsTr("语言")
                        Layout.fillWidth: true
                        label: Label {
                            text: qsTr("语言")
                            color: uiPreferences.textPrimaryColor
                            font.pixelSize: uiPreferences.fontBody
                            font.bold: true
                            leftPadding: uiPreferences.spacingSm
                        }
                        background: Rectangle {
                            color: uiPreferences.surfaceColor
                            radius: uiPreferences.radiusMd
                            border.color: uiPreferences.borderColor
                        }
                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            rowSpacing: uiPreferences.spacingSm
                            columnSpacing: uiPreferences.spacingMd

                            Label { text: qsTr("语言"); color: uiPreferences.textPrimaryColor }
                            ModernComboBox {
                                id: languagePicker
                                objectName: "languagePicker"
                                Layout.fillWidth: true
                                accessibleName: qsTr("界面语言")
                                model: [qsTr("跟随系统"), "简体中文", "English (US)"]
                                currentIndex: uiPreferences.language === "zh_CN" ? 1
                                              : uiPreferences.language === "en_US" ? 2 : 0
                                onActivated: uiPreferences.setLanguage(
                                                 ["system", "zh_CN", "en_US"][index])
                            }

                            Label {
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: uiPreferences.textSecondaryColor
                                font.pixelSize: uiPreferences.fontSmall
                                text: qsTr("语言选择会立即应用，并在下次启动时保留。界面固定使用浅色主题。")
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("全局快捷键")
                        Layout.fillWidth: true
                        label: Label {
                            text: qsTr("全局快捷键")
                            color: uiPreferences.textPrimaryColor
                            font.pixelSize: uiPreferences.fontBody
                            font.bold: true
                            leftPadding: 8
                        }
                        background: Rectangle {
                            color: uiPreferences.surfaceColor
                            radius: uiPreferences.radiusMd
                            border.color: uiPreferences.borderColor
                        }
                        GridLayout {
                            anchors.fill: parent
                            columns: 3
                            rowSpacing: 10
                            columnSpacing: 10

                            Label { text: qsTr("唤起组合键") }
                            HotkeyRecorder {
                                id: hotkeyRecorder
                                objectName: "globalHotkeyRecorder"
                                Layout.fillWidth: true
                                text: hotkeyService.sequence
                                placeholderText: qsTr("点击“录制”后按组合键")
                            }
                            ModernButton {
                                text: qsTr("绑定")
                                accessibleDescription: qsTr("保存当前录制的全局快捷键")
                                onClicked: hotkeyService.registerSequence(hotkeyRecorder.text)
                            }

                            Item { Layout.fillWidth: true }
                            Label {
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: hotkeyService.registered ? uiPreferences.successColor
                                                               : uiPreferences.dangerColor
                                text: hotkeyService.registered
                                      ? qsTr("已绑定 %1。按下后显示窗口并聚焦输入框。").arg(hotkeyService.sequence)
                                      : hotkeyService.errorMessage
                            }

                            Label { text: qsTr("开机后台启动") }
                            ModernSwitch { checked: setting.startUpAuto; onToggled: setting.setStartUpAuto(checked) }
                            ModernButton {
                                text: qsTr("恢复默认")
                                onClicked: {
                                    hotkeyRecorder.cancelRecording()
                                    hotkeyService.resetDefault()
                                    hotkeyRecorder.text = hotkeyService.sequence
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("隐私与本地数据")
                        Layout.fillWidth: true
                        label: Label {
                            text: qsTr("隐私与本地数据")
                            color: uiPreferences.textPrimaryColor
                            font.pixelSize: uiPreferences.fontBody
                            font.bold: true
                            leftPadding: 8
                        }
                        background: Rectangle {
                            color: uiPreferences.surfaceColor
                            radius: uiPreferences.radiusMd
                            border.color: uiPreferences.borderColor
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: privacyConsent.consentGranted ? uiPreferences.successColor
                                                                    : uiPreferences.warningColor
                                text: privacyConsent.consentGranted
                                      ? qsTr("已同意政策 %1（%2）").arg(privacyConsent.policyVersion).arg(privacyConsent.acceptedAt)
                                      : qsTr("尚未同意当前隐私说明；所有第三方网络请求均被阻止。")
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ModernButton {
                                    text: qsTr("查看隐私说明")
                                    onClicked: privacyConsent.requestPolicyDisplay()
                                }
                                ModernButton {
                                    text: qsTr("撤回同意")
                                    enabled: privacyConsent.consentGranted
                                    onClicked: privacyConsent.revoke()
                                }
                                Item { Layout.fillWidth: true }
                                ModernButton {
                                    text: qsTr("清除本地数据…")
                                    onClicked: deleteLocalDataConfirmation.open()
                                }
                            }
                        }
                    }

                    ModernButton {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("打开对话窗口")
                        enabled: providerSettings.configured
                        highlighted: true
                        onClicked: root.requestOpenChat()
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: uiPreferences.textSecondaryColor
                        text: qsTr("无需雷柏硬件 AI 键。若组合键被其他程序占用，绑定会失败且原快捷键继续有效。")
                    }
                }
            }

            Item {
                Loader {
                    objectName: "dataManagementLoader"
                    anchors.fill: parent
                    source: "qrc:/DataManagementPanel.qml"
                }
            }
        }

        MouseArea {
            width: 26
            height: 26
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor
            z: 20
            onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
    }
}
