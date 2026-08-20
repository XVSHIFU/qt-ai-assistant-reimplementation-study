import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import "Controls"

Item {
    id: root
    objectName: "dataManagementPanel"
    property string pendingConfirmation: ""

    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / (1024 * 1024)).toFixed(1) + " MB"
    }

    function retentionIndex(days) {
        return days === 7 ? 1 : days === 30 ? 2 : days === 90 ? 3 : 0
    }

    function requestConfirmation(action) {
        pendingConfirmation = action
        destructiveConfirmation.open()
    }

    Component.onCompleted: dataManagement.refreshStatistics()

    QQC2.Dialog {
        id: destructiveConfirmation
        objectName: "dataDestructiveConfirmation"
        anchors.centerIn: parent
        width: Math.min(420, root.width - 32)
        modal: true
        title: pendingConfirmation === "logs" ? qsTr("清除全部日志？")
               : pendingConfirmation === "chats" ? qsTr("清除全部聊天？")
               : pendingConfirmation === "credentials" ? qsTr("清除全部凭据？")
               : qsTr("应用聊天保留期限？")
        standardButtons: QQC2.Dialog.Yes | QQC2.Dialog.No
        closePolicy: QQC2.Popup.CloseOnEscape
        contentItem: Label {
            width: destructiveConfirmation.availableWidth
            padding: 14
            wrapMode: Text.WordWrap
            text: root.pendingConfirmation === "logs"
                  ? qsTr("将删除本地诊断日志及轮转备份。此操作不可撤销。")
                  : root.pendingConfirmation === "chats"
                    ? qsTr("将永久删除全部聊天、最近删除内容和请求元数据。Provider 配置与凭据不受影响。")
                    : root.pendingConfirmation === "credentials"
                      ? qsTr("将清除所有 Provider 的 DPAPI 加密凭据。聊天和 Provider 配置保留；之后需重新输入密钥。")
                      : qsTr("超过当前保留期限的聊天会移到“最近删除”，不会立即永久清除。")
        }
        onAccepted: {
            if (root.pendingConfirmation === "logs")
                dataManagement.clearLogs()
            else if (root.pendingConfirmation === "chats")
                dataManagement.clearAllChats()
            else if (root.pendingConfirmation === "credentials")
                dataManagement.clearAllCredentials()
            else if (root.pendingConfirmation === "retention")
                dataManagement.applyRetentionPolicy()
            root.pendingConfirmation = ""
        }
        onRejected: root.pendingConfirmation = ""
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(0, parent.width - 4)
            spacing: 12

            GroupBox {
                title: qsTr("本地数据概览")
                Layout.fillWidth: true
                background: Rectangle { color: "#FFFFFF"; radius: 12; border.color: "#E4E4E7" }
                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 8
                    Label { text: qsTr("聊天") }
                    Label { text: dataManagement.activeChatCount + qsTr(" 个") }
                    Label { text: qsTr("最近删除") }
                    Label { text: dataManagement.deletedChatCount + qsTr(" 个") }
                    Label { text: qsTr("数据库大小") }
                    Label { text: root.formatBytes(dataManagement.databaseBytes) }
                    Label { text: qsTr("日志大小") }
                    Label { text: root.formatBytes(dataManagement.logBytes) }
                    ModernButton {
                        text: qsTr("刷新")
                        onClicked: dataManagement.refreshStatistics()
                    }
                    ModernButton {
                        text: qsTr("打开数据目录")
                        onClicked: dataManagement.openDataDirectory()
                    }
                }
            }

            GroupBox {
                title: qsTr("历史策略")
                Layout.fillWidth: true
                background: Rectangle { color: "#FFFFFF"; radius: 12; border.color: "#E4E4E7" }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("保存本地聊天历史"); Layout.fillWidth: true }
                        ModernSwitch {
                            objectName: "historyPersistenceSwitch"
                            checked: dataManagement.historyPersistenceEnabled
                            onToggled: {
                                if (checked !== dataManagement.historyPersistenceEnabled)
                                    dataManagement.setHistoryPersistenceEnabled(checked)
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: dataManagement.historyPersistenceEnabled ? "#64748B" : "#B45309"
                        text: dataManagement.historyPersistenceEnabled
                              ? qsTr("聊天正文以明文保存在本机数据库中。")
                              : qsTr("“不保存历史”已开启：发送会被后端阻止并明确提示，不会静默丢弃你的输入。")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("保留期限") }
                        ComboBox {
                            id: retention
                            objectName: "historyRetentionSelector"
                            Layout.fillWidth: true
                            model: [qsTr("永久"), qsTr("7 天"), qsTr("30 天"), qsTr("90 天")]
                            currentIndex: root.retentionIndex(dataManagement.retentionDays)
                            onActivated: dataManagement.setRetentionDays(
                                             index === 1 ? 7 : index === 2 ? 30 : index === 3 ? 90 : 0)
                        }
                        ModernButton {
                            text: qsTr("应用…")
                            enabled: dataManagement.retentionDays > 0
                            onClicked: root.requestConfirmation("retention")
                        }
                    }
                }
            }

            GroupBox {
                title: qsTr("清除数据")
                Layout.fillWidth: true
                background: Rectangle { color: "#FFF7F7"; radius: 12; border.color: "#FECACA" }
                RowLayout {
                    anchors.fill: parent
                    spacing: 8
                    ModernButton { text: qsTr("清除日志…"); onClicked: root.requestConfirmation("logs") }
                    ModernButton { text: qsTr("清除全部聊天…"); onClicked: root.requestConfirmation("chats") }
                    ModernButton { text: qsTr("清除全部凭据…"); onClicked: root.requestConfirmation("credentials") }
                }
            }

            Label {
                objectName: "dataManagementResult"
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.WordWrap
                color: dataManagement.operationSucceeded ? "#047857" : "#B91C1C"
                text: dataManagement.operationMessage
            }
        }
    }
}
