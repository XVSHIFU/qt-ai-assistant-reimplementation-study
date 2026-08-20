import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.12

// 自定义“触发灵敏度”、“重置灵敏度”纵向滑动条
Rectangle {
    id: control
    width: 249
    height: 146
    color: "transparent"
    property alias sliderValue: slider.value // 仅可读
    property double sliderFrom: slider.from
    property double sliderTo: slider.to
    property alias handleY: handle.y
    signal m_valueChanged()
    property bool isDragging: false  // 新增拖拽标志位
    property bool chudi:false  //是否为触底死区

    // 计算级数所映射的行程（单位为0.01mm）
    property real m_travel: (control.ref - m_level_from) / (m_level_to - m_level_from) * (m_to - m_from) + m_from
    /*property real m_from: (0 / 100).toFixed(2)
    property real m_to: (kb_travelDistance_manager.physicalTravel / 100).toFixed(2)*/

    property real m_from: (kb_travelDistance_manager.physicalTravel / 100).toFixed(2)
    property real m_to: (0 / 100).toFixed(2)

    /*property int m_level_from: 0
    property int m_level_to: 255*/

    property int m_level_from: 255
    property int m_level_to: 0

    //触底最大/小灵敏度  非触底最大/小灵敏度
    property double minSensitivity: chudi ? 0 : 0
    property double maxSensitivity: 3.5
    property real maxRef:chudi?50:100

    //提示文字
    property bool lingMinTextVisible: false
    property bool chudiTextVisible: false

    signal dragStarted()
    signal dragEnded()

    //加1 减1
    signal minused()
    signal plus()

    // 外部通过指定该值修改内部灵敏度
    property int sensitivity: -1

    //级数
    property int ref: -1


    onRefChanged: {
        // 根据 ref 计算 handle.y 的位置
        if (!chudi && ref < 1) {
                ref = 1;
            } else {
                var heightRatio = (maxRef-ref) / maxRef;
                handle.y = ((slider.height) * heightRatio) - handle.height / 2;
                value_text.y= handle.y- value_text.height / 2+6;//加6是为了保持和UI一致
                m_valueChanged();
            }
    }

    onM_travelChanged: {
        slider.value = m_travel
    }

    onSensitivityChanged: {
            ref = Math.round((sensitivity - 0.00) / (kb_travelDistance_manager.physicalTravel - 0.00) * (255 - 0) + 0);

            // 更新关联的显示
            m_valueChanged();
    }

    Rectangle {
        id: slider
        width: 111
        height: 146
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 30
        property double from: 0.00
        property double value: 0.00
        property double to: chudi ? 0.65 : 3.30
        radius: 8

        Rectangle {
            id: background_rect
            anchors.fill: parent
            radius: 8

            gradient: control.chudi ? gradient1 : gradient2

            Gradient {
                id: gradient1
                orientation: Gradient.Vertical // 竖直渐变
                GradientStop { position: 0.0; color: "#E5EEFF" }  // Start color
                GradientStop { position: 0.7; color: "#E5EEFF" }  // middle color
                GradientStop { position: 1.0; color: "#FFD6D6" }  // End color
            }

            Gradient {
                id: gradient2
                orientation: Gradient.Vertical // 竖直渐变
                GradientStop { position: 0.0; color: theme.item.themeOneColor}  // Start color
                GradientStop { position: 1.0; color: theme.item.themeTwoColor }  // End color
            }

            Text {
                text: chudi ? "50" : "1"
                anchors.left: background_rect.left
                anchors.leftMargin: 4
                anchors.top: background_rect.top
                anchors.topMargin: 4
                //修改格式屏蔽
                /*color: chudi ? theme.item.funcColor2 : theme.item.themeTwoColor*/
                color: chudi ? theme.item.themeOneColor : theme.item.themeTwoColor
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
            }

            Text {
                text: chudi ? "0" : "100"
                anchors.left: background_rect.left
                anchors.leftMargin: 4
                anchors.bottom: background_rect.bottom
                anchors.bottomMargin: 4
                //修改格式屏蔽
                /*color: theme.item.themeOneColor*/
                color: theme.item.funcColor2
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
            }

            Text {
                text: "0" + "mm"
                visible:chudi
                anchors.right : background_rect.right
                anchors.rightMargin: 4
                //修改格式屏蔽
                /*anchors.top: background_rect.top
                anchors.topMargin: 4*/
                anchors.bottom: background_rect.bottom
                anchors.bottomMargin: 4
                color: theme.item.funcColor2
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
            }

            Text {
                text: "0.69" + "mm"
                visible:chudi
                anchors.right : background_rect.right
                anchors.rightMargin: 4
                //修改格式屏蔽
                /*anchors.bottom: background_rect.bottom
                anchors.bottomMargin: 4*/
                anchors.top: background_rect.top
                anchors.topMargin: 4
                color: theme.item.themeOneColor
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
            }
        }

        Rectangle {
            id: handle
            x: slider.width / 2 - width / 2
            width: 128
            height: 14
            color: "transparent"
            radius:8

            Image {
                anchors.fill: parent
                source: sliderMouseArea.pressed
                        ? "qrc:/Image/Function/Performance/travel_distance_slider_handle_pressed.svg"
                        : (handleMouseArea.containsMouse
                           ? "qrc:/Image/Function/Performance/travel_distance_slider_handle_hover.svg"
                           : "qrc:/Image/Function/Performance/travel_distance_slider_handle_normal.svg")
            }

            MouseArea {
                id: handleMouseArea
                hoverEnabled: true
                anchors.fill: parent
              //  drag.target: handle // 否则Slider范围外handle无法被拖动
            }

            onXChanged: {
                x = slider.width / 2 - width / 2
            }

            onYChanged: {
                // 反推计算 ref 的值
               //  ref = Math.round(((handle.y- handle.height / 2) / (slider.height)) * maxRef);
                ref =maxRef- Math.round((maxRef * (handle.y + (handle.height / 2))) / slider.height);
                //console.log("maxRef的值",maxRef);
                //console.log("handle.y的值",handle.y);
                //console.log("handle.height的值",handle.height);
                //console.log("slider.height的值",slider.height);
                //console.log("ref的值",ref);
                // // 确保 ref 在有效范围内
                 ref = Math.max(0, Math.min(maxRef, ref));

                if (!chudi && ref < 1) {
                        ref = 1;
                    }
            }

            Component.onCompleted: {
            }
        }

        MouseArea {
            id: sliderMouseArea
            anchors.fill: parent

            // 鼠标按下且光标位置发生改变时触发
            onPositionChanged: {
                handle.y = mouse.y - handle.height / 2
            }

            onPressed: {
                control.dragStarted();
                handle.y = mouse.y - handle.height / 2
            }

            onReleased: {
                control.dragEnded();
            }
        }
    } // Slider

    Rectangle {
        id: value_text
        width: 66
        height: 33
        anchors.right: parent.right
        anchors.rightMargin: 26
        /*anchors.verticalCenter: parent.verticalCenter*/
        color: "transparent"

        Text {
            text: control.ref.toString()
            anchors.left: parent.left
            anchors.top: parent.top
            color: theme.item.color1
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 14
        }

        Text {
            text: "(≈" + slider.value.toFixed(2).toString() + "mm)"
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            color: "#57585A"
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 12
        }
    }

        Button {
            id: minus_btn
            width: 20
            height: 20
            anchors.top: parent.top
            anchors.left: parent.left
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
                Image {
                    //source: theme.item.minus_vertical
                    anchors.fill: parent
                    source: minus_btn.pressed
                            ? theme.item.minus_vertical_clicked
                            : (minus_btn.hovered
                               ? theme.item.minus_vertical_hot
                               : theme.item.minus_vertical_normal)
                }
            }

            onClicked: {
                /*if (control.ref > (chudi ? 0 : 1))
                {
                    control.ref = Math.max(chudi ? 0 : 1, control.ref - 1);
                }*/

                if (control.ref < control.maxRef)
                { // 确保 ref 不超过 maxRef
                    control.ref = Math.min(control.maxRef, control.ref + 1); // 从 ref 中加上 1
                }
                control.minused()
            }
        }

        Button {
            id: plus_btn
            width: 20
            height: 20
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
                Image {
                    //source: theme.item.plus_vertical
                    anchors.fill: parent
                    source: plus_btn.pressed
                            ? theme.item.plus_vertical_clicked
                            : (plus_btn.hovered
                               ? theme.item.plus_vertical_hot
                               : theme.item.plus_vertical_normal)
                }
            }

            onClicked: {
                /*if (control.ref < control.maxRef)
                { // 确保 ref 不超过 maxRef
                    control.ref = Math.min(control.maxRef, control.ref + 1); // 从 ref 中加上 1
                }*/
                if (control.ref > (chudi ? 0 : 1))
                {
                    control.ref = Math.max(chudi ? 0 : 1, control.ref - 1);
                }
                control.plus()
            }
        }
} // Rectangle: control
