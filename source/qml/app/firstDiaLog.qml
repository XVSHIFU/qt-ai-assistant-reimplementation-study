import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "Controls"

ApplicationWindow {
    id: root
    width: 640
    height: 580
    minimumWidth: 520
    minimumHeight: 480
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    color: "transparent"
    title: qsTr("智键 AI 隐私说明")

    signal accepted()

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: "#F7F7F4"
        border.color: "#22000000"

        Rectangle {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 58
            color: "transparent"

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: qsTr("隐私说明与数据使用")
                    color: "#111827"
                    font.pixelSize: 19
                    font.bold: true
                }
                Text {
                    text: qsTr("政策版本 %1").arg(privacyConsent.policyVersion)
                    color: "#64748B"
                    font.pixelSize: 11
                }
            }

            ModernButton {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                height: 36
                text: "×"
                flat: true
                onClicked: root.hide()
            }

            MouseArea {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.rightMargin: 64
                onPressed: root.startSystemMove()
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: actionBar.top
            anchors.margins: 20
            radius: 12
            color: "white"
            border.color: "#E4E4E7"

            ScrollView {
                anchors.fill: parent
                anchors.margins: 16
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    width: parent.width
                    text: privacyConsent.policyText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: "#374151"
                    font.pixelSize: 13
                    background: null
                    Accessible.name: qsTr("隐私说明正文")
                }
            }
        }

        ColumnLayout {
            id: actionBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.bottomMargin: 18
            spacing: 9

            CheckBox {
                id: acknowledgement
                Layout.fillWidth: true
                text: qsTr("我已阅读并理解：聊天内容会发送给我选择的第三方 Provider，聊天记录以明文保存在本机。")
            }

            Label {
                Layout.fillWidth: true
                visible: privacyConsent.lastError !== ""
                text: privacyConsent.lastError
                color: "#B91C1C"
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ModernButton {
                    text: qsTr("暂不同意")
                    onClicked: root.hide()
                }
                ModernButton {
                    text: qsTr("同意并继续")
                    highlighted: true
                    enabled: acknowledgement.checked
                    onClicked: {
                        if (privacyConsent.accept(Qt.locale().name.replace("_", "-"))) {
                            root.accepted()
                            root.hide()
                        }
                    }
                }
            }
        }
    }
}
