import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import "./Controls"
import "Component"
import "qrc:/GraphicalEffects"

ApplicationWindow  {
    id: root
    width: 600
    height: 210
    flags: Qt.Window| Qt.FramelessWindowHint// 无边框
    color: "transparent"

    signal closeWindow()

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
        id: background
        anchors.fill: parent

        radius: 20 // 圆角半径
        color:"white"
        border.width: 1
        border.color: "#11000000"
        //visible: false
    }

    Rectangle {
        id: content
        anchors.fill: parent
        color: "transparent" // 内容区域透明
        radius: background.radius // 圆角与背景一致

        //标题栏
        Rectangle {
            id:title
            width: root.width
            height: 40
            anchors.top: parent.top
            anchors.topMargin:0
            color: "transparent" // 标题栏透明
            Image {
                id: icon
                width: 99
                height: 20
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin:20
                source: "qrc:/Image/Settings/SmartAI.png"
            }

            Image {
                visible:false
                id: name
                anchors.left: icon.right
                anchors.leftMargin: 2
                anchors.top: icon.top
                anchors.topMargin:5
                source: "qrc:/Image/Settings/SmartKey_AI.svg"
            }
        }

        //发现新版本、close
        Rectangle {
            id:set_control
            width: root.width
            height: 30
            anchors.top: title.bottom
            anchors.topMargin:13
            color: "transparent" // 设置区域透明
            Text {
                id: set
                text: qsTr("发现新版本")
                anchors.left: set_control.left
                anchors.leftMargin: 20
                anchors.top: set_control.top
                anchors.topMargin:2
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 16
                color:theme.item.colorB1
                lineHeight: 19
            }

            Text {
                id: version
                text: "V" + window.newVersionNumber
                anchors.left: set_control.left
                anchors.leftMargin: 23
                anchors.top: set_control.top
                anchors.topMargin: 25
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 12
                color:theme.item.colorW6
                lineHeight: 9
            }

            Rectangle {
                id: close_btn
                width: 18 // 默认宽度
                height: 18 // 默认高度
                color: "transparent" // 背景透明
                anchors.right: set_control.right
                anchors.rightMargin: 20
                // 图片控件
                Image {
                    id: imageView
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit // 保持宽高比
                    smooth: true // 平滑渲染
                    sourceSize: Qt.size(width, height) // 图片大小
                    source: {
                        if (mouseArea.pressed) {
                            return "qrc:/Image/Settings/close_clicked.svg" // 点击状态
                        } else if (mouseArea.containsMouse) {
                            return "qrc:/Image/Settings/close_hot.svg" // 悬停状态
                        } else {
                            return "qrc:/Image/Settings/close_normal.svg" // 默认状态
                        }
                    }
                }

                // 鼠标区域
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true // 启用悬停检测
                    onClicked: {
                      window.showGuideWindow()
                      closeWindow()
                    }
                }
            }
        }

        Rectangle {
            id:title2
            width: root.width
            height: 80
            anchors.top: set_control.bottom
            anchors.topMargin:0
            color: "transparent"

            Text {
                id: tip
                text: qsTr("After canceling, you can also update the version in [Settings]")
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: title2.top
                anchors.topMargin: 20
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
                color:theme.item.colorB4
                lineHeight: 9
            }
        }

        Row {
            width: 208
            height: 38
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            spacing: 12

            Button {
                id:cancel
                width: 98
                height: 38
                background: Rectangle {
                    anchors.fill: parent
                    color: theme.item.themeOneColor6
                    radius: 12

                    Text {
                        text: qsTr("cancel")
                        anchors.centerIn: parent
                        font.family: fontManager.item.oppoSansM.name
                        font.pixelSize: 14
                        color: theme.item.colorB3
                    }
                }

                onClicked: {
                    window.showGuideWindow()
                    closeWindow()
                }
            }

            Button {
                id: confirm
                width: 98
                height: 38
                background: Rectangle {
                    anchors.fill: parent
                    color: theme.item.themeOneColor6
                    radius: 12

                    Text {
                        text: qsTr("去更新")
                        anchors.centerIn: parent
                        font.family: fontManager.item.oppoSansM.name
                        font.pixelSize: 14
                        color: theme.item.colorB3
                    }
                }

                onClicked: {
                    window.updateSoft()
                    closeWindow()
                }
            }
        }
    }
}
