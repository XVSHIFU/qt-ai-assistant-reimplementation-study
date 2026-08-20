import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Popup {
    id: root
    width: 148 * scaleFactor
    height: 124 * scaleFactor
    padding: 6 * scaleFactor
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

    property real scaleFactor: window.scale
    property string conversationId: ""
    property string conversationTitle: ""
    property bool pinned: false

    signal pinBtnClicked()
    signal renameBtnClicked()
    signal deleteBtnClicked()

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 5 * root.scaleFactor
            anchors.leftMargin: 2 * root.scaleFactor
            anchors.rightMargin: -2 * root.scaleFactor
            anchors.bottomMargin: -3 * root.scaleFactor
            color: "#24000000"
            radius: 12 * root.scaleFactor
        }
        Rectangle {
            anchors.fill: parent
            color: theme.item.colorW1
            radius: 12 * root.scaleFactor
            border.color: theme.item.colorW3
            border.width: Math.max(1, root.scaleFactor)
        }
    }

    contentItem: Column {
        spacing: 3 * root.scaleFactor

        ActionButton {
            text: root.pinned ? qsTr("取消置顶") : qsTr("置顶")
            iconSource: root.pinned
                        ? "qrc:/Image/Icon/topScreen_checked.svg"
                        : "qrc:/Image/Icon/topScreen_normal.svg"
            onClicked: root.pinBtnClicked()
        }
        ActionButton {
            text: qsTr("重命名")
            iconSource: "qrc:/Image/Icon/rename_normal.svg"
            onClicked: root.renameBtnClicked()
        }
        ActionButton {
            text: qsTr("删除")
            iconSource: "qrc:/Image/Icon/delete_normal.svg"
            destructive: true
            onClicked: root.deleteBtnClicked()
        }
    }

    component ActionButton: Button {
        id: action
        property string iconSource: ""
        property bool destructive: false
        width: 136 * root.scaleFactor
        height: 34 * root.scaleFactor
        hoverEnabled: true

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: action.hovered ? 2 * root.scaleFactor : 0
                anchors.leftMargin: action.hovered ? 1 * root.scaleFactor : 0
                color: action.hovered ? "#16000000" : "transparent"
                radius: 9 * root.scaleFactor
            }
            Rectangle {
                anchors.fill: parent
                anchors.bottomMargin: action.hovered ? 2 * root.scaleFactor : 0
                color: action.down
                       ? (action.destructive ? "#FEE2E2" : theme.item.themeOneColor5)
                       : action.hovered
                         ? (action.destructive ? "#FEF2F2" : theme.item.themeOneColor6)
                         : "transparent"
                radius: 9 * root.scaleFactor
            }
        }

        contentItem: RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 11 * root.scaleFactor
                anchors.rightMargin: 9 * root.scaleFactor
                spacing: 9 * root.scaleFactor
                Image {
                    Layout.preferredWidth: 17 * root.scaleFactor
                    Layout.preferredHeight: 17 * root.scaleFactor
                    Layout.alignment: Qt.AlignVCenter
                    source: action.iconSource
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: action.text
                    color: action.destructive ? "#B91C1C" : theme.item.colorB1
                    font.pixelSize: 13 * root.scaleFactor
                    font.family: fontManager.item.oppoSansM.name
                    verticalAlignment: Text.AlignVCenter
                }
        }
    }
}
