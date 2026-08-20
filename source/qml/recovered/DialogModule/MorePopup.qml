import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Popup {
    id: root
    width: 90 * scaleFactor
    height: 80 * scaleFactor
    closePolicy: Popup.CloseOnPressOutside
    x: 0
    y: 0

    // 自适应窗口的 DPI 缩放
    property real scaleFactor: window.scale

    signal renameBtnClicked()
    signal deleteBtnClicked()

    property int index: -1

    background: Rectangle {
        id: backgroundRect
        anchors.fill: parent
        color: theme.item.colorW1
        radius: 12 * scaleFactor

        layer.enabled: true
        layer.effect: DropShadow {
            color: "#40000000"
            radius: 10 * scaleFactor
            samples: 20
            horizontalOffset: 0
            verticalOffset: 2
            transparentBorder: true
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {}
        }

        Column {
            anchors.top: parent.top
            anchors.topMargin: 6 * scaleFactor
            anchors.left: parent.left
            anchors.leftMargin: 6 * scaleFactor
            spacing: 4 * scaleFactor

            Button {
                id: renameBtn
                width: 78 * scaleFactor
                height: 32 * scaleFactor

                background: Rectangle {
                    anchors.fill: parent
                    radius: 8 * scaleFactor
                    color: renameBtn.pressed
                           ? theme.item.themeOneColor5
                           : renameBtn.hovered
                             ? theme.item.themeOneColor6
                             : theme.item.colorW1

                    Row {
                        anchors.centerIn: parent
                        spacing: 4 * scaleFactor
                        Image {
                            width: 16 * scaleFactor
                            height: 16 * scaleFactor
                            source: "qrc:/Image/Icon/rename_normal.svg"
                        }

                        Text {
                            id: renameText
                            text: qsTr("Rename")
                            color: theme.item.colorB1
                            font.pixelSize: 14 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name
                        }
                    }
                }

                onClicked: {
                    root.renameBtnClicked()
                }
            } // Button

            Button {
                id: deleteBtn
                width: 78 * scaleFactor
                height: 32 * scaleFactor

                background: Rectangle {
                    anchors.fill: parent
                    radius: 8 * scaleFactor
                    color: deleteBtn.pressed
                           ? theme.item.themeOneColor5
                           : deleteBtn.hovered
                             ? theme.item.themeOneColor6
                             : theme.item.colorW1

                    Row {
                        anchors.centerIn: parent
                        Image {
                            width: 16 * scaleFactor
                            height: 16 * scaleFactor
                            source: "qrc:/Image/Icon/delete_normal.svg"
                        }

                        Text {
                            id: deleteText
                            text: qsTr("Delete")
                            color: theme.item.colorB1
                            font.pixelSize: 14 * scaleFactor
                            font.family: fontManager.item.oppoSansM.name
                        }
                    }
                }

                onClicked: {
                    root.deleteBtnClicked()
                }
            } // Button
        } // Column
    } // background
}
