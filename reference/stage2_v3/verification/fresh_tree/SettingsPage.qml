import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
import "./Controls"
import "qrc:/GraphicalEffects"
ApplicationWindow  {
    id: root
    width: 524
    height: 284
    flags: Qt.Window| Qt.FramelessWindowHint // 无边框
    color: "transparent" // 窗口背景透明
    // onActiveChanged: {
    //         if (!active) {
    //             root.raise(); // 将窗口置于顶部
    //             root.requestActivate(); // 请求激活窗口
    //         }
    //     }
    // 定义一个信号，用于通知 C++ 关闭窗口
    signal closeWindow()
    signal startWindow()
    signal moveWindow()
    signal endDragWindow()
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

    // // 使用OpacityMask实现透明圆角
    // OpacityMask {
    //     id: mask
    //     anchors.fill: background
    //     source: background
    //     maskSource: Rectangle {
    //         width: background.width
    //         height: background.height
    //         radius: background.radius
    //         color: "white"
    //     }
    // }


    // // 添加阴影效果
    // DropShadow {
    //     anchors.fill: mask
    //     horizontalOffset: 0
    //     verticalOffset: 2
    //     radius: 10
    //     samples: 8
    //     color: "#33000000" // 阴影颜色和透明度
    //     source: mask
    // }

    Rectangle {
        id: content
        anchors.fill: parent
        color: "transparent" // 内容区域透明
        radius: background.radius // 圆角与背景一致

        Rectangle {
            width: 140
            height: 44
            visible: btn_area.pressed  && !setting.updated
            anchors.top: smartkey_area.bottom
            anchors.topMargin: 4
            anchors.right: smartkey_area.right
            anchors.rightMargin: 12
            color: theme.item.W1
            radius: 12
            z:2

            //多重采样抗锯齿
            layer.enabled: true
            layer.samples: Qt.platform.os === "osx" ? 0 : 6

            //layer.effect 属性用于为QML元素（如 Rectangle、Image、Text 等）添加图形效果。
            //DropShadowEffect用于在矩形周围添加阴影效果。
            //@disable-check M301
            layer.effect: DropShadowEffect {
                color: Qt.rgba(0, 0, 0, 0.25)
                horizontalOffset: 0
                verticalOffset: 1
                spread: 0.1
                radius: 4
            }

            Text {
                text: qsTr("当前已是最新版本")
                anchors.centerIn: parent
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
                color: "#000000"
            }
        }

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

        MouseArea
            {
                id: mouseArea_drag
                anchors.fill: parent
                onPressed: {
                    startWindow();
                }
                onPositionChanged: {
                    moveWindow();
                }
                onReleased: {
                   endDragWindow();
                }
            }
    }

    Rectangle {
        id:set_control
        width: root.width
        height: 30
        anchors.top: title.bottom
        anchors.topMargin:13
        color: "transparent" // 设置区域透明
        Text {
            id: set
            text: qsTr("设置")
            anchors.left: set_control.left
            anchors.leftMargin: 20
            anchors.top: set_control.top
            anchors.topMargin:2
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 16
            color:"#0A0A0A"
            lineHeight: 19
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
                    closeWindow()
                    //setting.test()
                }
            }
        }
    }


    Rectangle {
        id:smartkey_area
        width: root.width
        height: 50
        anchors.top: set_control.bottom
        anchors.topMargin:3
        color: "transparent" // 设置区域透明
        Rectangle {
         id:smartkeyversion
         anchors.top: parent.top
         anchors.topMargin:1
         anchors.left: parent.left
         anchors.leftMargin:20
         anchors.right: parent.right
         anchors.rightMargin:20
         height:48
         color:"#F4F6F7"
         radius: 14

         Image {
             id: refresh_icon
             width: 24
             height: 24
             anchors.left: smartkeyversion.left
             anchors.leftMargin: 12
             anchors.top: smartkeyversion.top
             anchors.topMargin:12
             source: "qrc:/Image/Settings/refresh_icon.svg"
         }
         Text {
             id: smartkey_text
             text: qsTr("智键AI")
             anchors.left: refresh_icon.right
             anchors.leftMargin: 8
             anchors.top: parent.top
             anchors.topMargin: 14
             font.family: fontManager.item.oppoSansM.name
             font.pixelSize: 14
             color:"#0A0A0A"
             lineHeight: 16
         }
         Text {
             id: smartkeyversion_text
             text: qsTr("当前版本V")+setting.currentVersion
             anchors.left: smartkey_text.right
             anchors.leftMargin: 10
             anchors.top: parent.top
             anchors.topMargin: 15
             font.family: fontManager.item.oppoSansM.name
             font.pixelSize: 12
             color:"#5D5D5D"
             lineHeight: 14
         }

         Rectangle {
             id: checkupdate_btn
             width: 80
             height: 28
             color:btn_area.pressed ? "#4737FF" :  (btn_area.containsMouse ? "#EBF1FF" : "#FFFFFF")
             radius: height / 2
             anchors.verticalCenter: parent.verticalCenter
             anchors.right: parent.right
             anchors.rightMargin: 10

             Image {
                 id: updated_icon
                 visible: setting.updated
                 fillMode: Image.PreserveAspectFit
                 anchors.right: parent.right
                 anchors.rightMargin: -4
                 anchors.top: parent.top
                 anchors.topMargin: -3
                 width: 14
                 height: 14
                 source: "qrc:/Image/Settings/updateTip.png"
                 z: 2
             }

             Text {
                 id: updated_text
                 anchors.centerIn: parent
                 text: qsTr("检查更新")
                 color: btn_area.pressed ? "#FFFFFF" :  (btn_area.containsMouse ? "#4737FF" : "#4737FF")
                 font.family: fontManager.item.oppoSansM.name
                 font.pixelSize: 14
             }

             MouseArea {
                 id:btn_area
                 anchors.fill: parent
                 hoverEnabled: true // 启用悬停检测
                 onClicked: {
                    setting.checkUpdate()
                 }
             }
         }
        }
    }

    Rectangle {
        id:usecount_area
        width: root.width
        height: 36
        anchors.top: smartkey_area.bottom
        anchors.topMargin:3
        anchors.left: parent.left
        anchors.leftMargin:20
        color: "transparent" // 设置区域透明
        Text {
            id: usercount_text
            text: qsTr("AI模型使用次数")
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 12
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 14
            color:"#0A0A0A"
            lineHeight: 27
        }

        Rectangle {
                id:count_slider
                width: 250
                height: 16
                color: "#DBE3FF"
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 40
                radius: 8
                property int percent: 2000




                Rectangle {
                    id: front
                    //宽度是 背景宽度 * 百分比
                    width: setting.usedCount / setting.totalCount * parent.width
                    height: parent.height
                    radius: parent.radius

                    gradient: Gradient {
                                // 水平渐变
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#4737FF" } // 渐变起始色
                                GradientStop { position: 0.3; color: "#B863FF" } // 渐变结束色
                                GradientStop { position: 0.6; color: "#14F1FF" } // 渐变结束色
                                GradientStop { position: 1.0; color: "#80FF61" } // 渐变结束色
                            }
                }
                // 显示计数值的文本
                Text {
                    id: totol_count
                    text: "/" + setting.totalCount // 显示滑动条的值
                    anchors.top: parent.top
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    font.pixelSize: 12
                    font.family: fontManager.item.oppoSansM.name
                    lineHeight: 27
                    color: "#0A0A0A"
                }
                // 标题文本
                Text {
                    id:use_count
                    text: setting.usedNumCount
                    anchors.top: parent.top
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: totol_count.left
                    font.pixelSize: 12
                    font.family: fontManager.item.oppoSansM.name
                    lineHeight: 27
                    color: "#4C4CFF"
                }
            }

    }

    Rectangle {
        id:bootup_area
        width: root.width
        height: 36
        anchors.top: usecount_area.bottom
        anchors.topMargin:3
        anchors.left: parent.left
        anchors.leftMargin:20
        color: "transparent" // 设置区域透明
        Text {
            id: bootup_text
            text: qsTr("开机自启动")
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 12
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 14
            color:"#0A0A0A"
            lineHeight: 27
        }

        CustomToggleButton {
            id:bootup_check
            anchors.right: parent.right
            anchors.rightMargin: 40
            anchors.top: parent.top
            anchors.topMargin: 12
            checked: setting.startUpAuto

            onCheckedChanged: {
                let newValue = bootup_check.checked;
                setting.startUpAuto=bootup_check.checked;
                setting.setStartUpAuto(newValue)
            }
        }
    }


    Rectangle {
        id:chooseAi_area
        width: root.width
        height: 36
        anchors.top: bootup_area.bottom
        anchors.topMargin:3
        anchors.left: parent.left
        anchors.leftMargin:20
        color: "transparent" // 设置区域透明
        Text {
            id: chooseAi_text
            text: qsTr("设置默认AI模型")
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.top: parent.top
            anchors.topMargin: 12
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 14
            color:"#0A0A0A"
            lineHeight: 27
        }

        // 自定义 ComboBox
        ComboBox {
                id: aimodel_combo
                width: 113
                height: 30
                anchors.right: parent.right
                anchors.rightMargin: 40
                anchors.top: parent.top
                anchors.topMargin: 2
                currentIndex: 0
                enabled: false
                // 设置背景为透明
                    background: Rectangle {
                        color: "transparent"
                    }

                // 下拉框的选项
                model: ListModel {
                    id: comboModel
                    ListElement { name: "deepseek"; icon: "qrc:/Image/Settings/deepseek_icon.png" }
                    ListElement { name: "ChatGPT"; icon: "qrc:/Image/Settings/chatgpt_icon.png" }
                    ListElement { name: "通义千问"; icon: "qrc:/Image/Settings/qianwen_icon.png" }
                    ListElement { name: "抖音豆包"; icon: "qrc:/Image/Settings/doubao_icon.png" }
                }

                // 自定义选项的显示内容
                delegate: ItemDelegate {
                    width: aimodel_combo.width
                    height: 30

                    contentItem: Row {
                        spacing: 5
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            source: model.icon
                            width: 20
                            height: 20
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: model.name
                            font.pixelSize: 12
                            font.family: fontManager.item.oppoSansM.name
                            color: "#0A0A0A"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    background: Rectangle {
                        radius:10
                        color: aimodel_combo.highlightedIndex === index ? "#DBE3FF" : "transparent"
                    }
                }

                // 自定义当前选中项的显示内容
                contentItem: Row {
                    spacing: 5
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        source: comboModel.get(aimodel_combo.currentIndex).icon
                        width: 20
                        height: 20
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: comboModel.get(aimodel_combo.currentIndex).name
                        font.pixelSize: 12
                        font.family: fontManager.item.oppoSansM.name
                        color: "#0A0A0A"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // 自定义下拉箭头
                indicator: Image {
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 1
                    source: aimodel_combo.down ? "qrc:/Image/Settings/arror_clicked.svg" :
                                    aimodel_combo.hovered ? "qrc:/Image/Settings/arror_hot.svg" :
                                    "qrc:/Image/Settings/arror_normal.svg"
                }

                // 自定义下拉菜单的样式
                popup: Popup {
                    y: aimodel_combo.height
                    width: aimodel_combo.width
                    implicitHeight: Math.min(contentItem.implicitHeight, 200) // 限制最大高度
                    padding: 1

                    // 下拉菜单的背景
                    background: Rectangle {
                        color: "#ffffff"
                        radius: 10
                        border.color: "transparent" // 可选：添加边框
                    }

                    contentItem: ListView {
                        clip: false // 设置为 false，避免内容被裁剪
                        implicitHeight: contentHeight
                        model: aimodel_combo.popup.visible ? aimodel_combo.delegateModel : null
                        currentIndex: aimodel_combo.highlightedIndex

                        // 自定义每个选项的样式
                        delegate: ItemDelegate {
                            width: aimodel_combo.width
                            height: 30

                            contentItem: Row {
                                spacing: 10
                                anchors.verticalCenter: parent.verticalCenter

                                Image {
                                    source: model.icon
                                    width: 20
                                    height: 20
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: model.name
                                    font.pixelSize: 12
                                    font.family: fontManager.item.oppoSansM.name
                                    color: aimodel_combo.highlightedIndex === index ? "#ffffff" : "#0A0A0A"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            background: Rectangle {
                                radius:10
                                color: aimodel_combo.highlightedIndex === index ? "#DBE3FF" : "transparent"
                            }
                        }
                    }
                }
            }
    }
}
}
