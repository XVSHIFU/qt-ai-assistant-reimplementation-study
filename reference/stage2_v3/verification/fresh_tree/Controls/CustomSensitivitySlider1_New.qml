import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.12

// 自定义“触发灵敏度”、“重置灵敏度”纵向滑动条
Rectangle {
    id: control
    width: 466
    height: 50
    color: "transparent"
    property alias sliderValue: slider.value // 仅可读
    property double sliderFrom: slider.from
    property double sliderTo: slider.to
    property alias handleX: handle.x
    signal m_valueChanged()
    property bool isDragging: false  // 新增拖拽标志位
    property bool isLimiting: false  // 是否限制标志位
    property int limitingValue: 0  //滑动条限制值
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
    property real maxRef:100

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


    function updateSensitivity() {
        ref = Math.round((sensitivity - 0.00) / (kb_travelDistance_manager.physicalTravel - 0.00) * (255 - 0) + 0);
        //console.log("ref.x的值",ref);
        // 更新关联的显示
        m_valueChanged();
    }

    function updateRef() {
        // 根据 ref 计算 handle.y 的位置
        if (!chudi && ref < 1) {
                ref = 1;
            } else {
                if(isLimiting)
                {
                    var heightRatio = (ref) / maxRef;
                    handle.x = ((slider.width) * heightRatio) - handle.width / 2;
                    value_text.x= handle.x+value_text.width/2-15;
                    //console.log("开启限制handle.x的值",handle.x);
                    m_valueChanged();
                }
                else
                {
                    var heightRatiolimiting = (ref-limitingValue) / (maxRef-limitingValue);
                    handle.x = ((slider.width) * heightRatiolimiting) - handle.width / 2;
                    value_text.x= handle.x+value_text.width/2-15;
                    //console.log("关闭限制handle.x的值",handle.x);
                    m_valueChanged();
                }
            }
    }


    onRefChanged: {
        // 根据 ref 计算 handle.y 的位置
        updateRef();
    }

    onM_travelChanged: {
        slider.value = m_travel
    }

    onSensitivityChanged: {
            updateSensitivity();
    }




    Rectangle {
        id: slider
        width: 363
        height: 22
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 45
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
                orientation: Gradient.Horizontal // 水平渐变
                GradientStop { position: 0.0; color: "#E5EEFF" }  // Start color
                GradientStop { position: 0.7; color: "#E5EEFF" }  // middle color
                GradientStop { position: 1.0; color: "#FFD6D6" }  // End color
            }

            Gradient {
                id: gradient2
                orientation: Gradient.Horizontal // 水平渐变
                GradientStop { position: 0.0; color: theme.item.themeOneColor}  // Start color
                GradientStop { position: 1.0; color: theme.item.themeTwoColor }  // End color
            }

            Text {
                text: isLimiting ? "1" : "7"
                anchors.right: background_rect.left
                anchors.rightMargin: 8
                anchors.top: background_rect.top
                anchors.topMargin: 4
                //修改格式屏蔽
                /*color: chudi ? theme.item.funcColor2 : theme.item.themeTwoColor*/
                color: chudi ? theme.item.themeOneColor : theme.item.themeOneColor10
                font.family: fontManager.item.oppoSansM.name
                font.pixelSize: 14
            }

            Text {
                text: "100"
                anchors.left: background_rect.right
                anchors.leftMargin: 6
                anchors.top: background_rect.top
                anchors.topMargin: 4
                //修改格式屏蔽
                /*color: theme.item.themeOneColor10*/
                color: theme.item.themeOneColor10
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
            y: slider.height / 2 - height / 2
            width: 14
            height: 30
            color: "transparent"
            radius:8

            Image {
                anchors.fill: parent
                source: sliderMouseArea.pressed
                        ? "qrc:/Image/Function/Performance/vertial_travel_distance_slider_handle_pressed.svg"
                        : (handleMouseArea.containsMouse
                           ? "qrc:/Image/Function/Performance/vertial_travel_distance_slider_handle_hover.svg"
                           : "qrc:/Image/Function/Performance/vertial_travel_distance_slider_handle_normal.svg")
            }

            MouseArea {
                id: handleMouseArea
                hoverEnabled: true
                anchors.fill: parent
              //  drag.target: handle // 否则Slider范围外handle无法被拖动
            }

            onYChanged: {
                y = slider.height / 2 - height / 2
            }

            onXChanged: {
                // 反推计算 ref 的值
               //  ref = Math.round(((handle.y- handle.height / 2) / (slider.height)) * maxRef);
                if(isLimiting)
                {
                    ref =Math.round((maxRef* (handle.x + (handle.width / 2))) / slider.width);
                    //console.log("handle.x的值",handle.x,slider.value);
                }
                else
                {

                    ref =Math.round(limitingValue+(((maxRef-limitingValue)* (handle.x + (handle.width / 2))) / slider.width));
                     //console.log("limitingValue的值",limitingValue);
                    //console.log("maxRef的值",maxRef);
                     //console.log("ref的值",ref);
                    // ref =Math.round((maxRef * (handle.x + (handle.width / 2))) / slider.width);
                    // if(ref<limitingValue)
                    // {
                    //     var heightRatio = limitingValue / maxRef;
                    //     handle.x = ((slider.width) * heightRatio) - handle.width / 2;
                    //     value_text.x= handle.x+value_text.width/2;//加6是为了保持和UI一致
                    //     m_valueChanged();
                    // }
                }


                //console.log("isDragging的值",isDragging);
                //console.log("slider.to的值",slider.to);
                //console.log("slider.from的值",slider.from);
                //console.log("ref的值",ref);
                // // 确保 ref 在有效范围内
                 ref = Math.max(0, Math.min(maxRef, ref));

                if (isLimiting && ref < 1)
                {
                    ref = 1;
                }
                else if (!isLimiting && ref < limitingValue)
                {
                    ref = limitingValue;
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
                handle.x = mouse.x - handle.width / 2
            }

            onPressed: {
                control.dragStarted();
                handle.x = mouse.x - handle.width / 2
            }

            onReleased: {
                control.dragEnded();
            }
        }
    } // Slider

    Rectangle {
        id: value_text
        width: 63
        height: 11
        anchors.top: slider.bottom
        anchors.topMargin: 12
        /*anchors.verticalCenter: parent.verticalCenter*/
        color: "transparent"

        Text {
            id:value_text_first
            text: control.ref.toString()
            anchors.left: parent.left
            anchors.top: parent.top
            color: theme.item.color1
            font.family: fontManager.item.oppoSansM.name
            font.pixelSize: 14
        }

        Text {
            text: "(≈" + slider.value.toFixed(2).toString() + "mm)"
            anchors.left: value_text_first.right
            anchors.top: parent.top
            anchors.topMargin: 3
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
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.leftMargin: 6
        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
            Image {
                //source: theme.item.minus_vertical
                anchors.fill: parent
                source: minus_btn.pressed
                        ? theme.item.minus_horizontal_clicked
                        : (minus_btn.hovered
                           ? theme.item.minus_horizontal_hot
                           : theme.item.minus_horizontal_normal)
            }
        }

        onClicked: {
            if (control.ref > (chudi ? 0 : 1))
            {
                control.ref = Math.max(chudi ? 0 : 1, control.ref - 1);
            }
            control.minused()
        }
    }

    Button {
        id: plus_btn
        width: 20
        height: 20
        anchors.top: parent.top
        anchors.topMargin: 2
        anchors.right: parent.right
        anchors.rightMargin: 6
        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
            Image {
                //source: theme.item.plus_vertical
                anchors.fill: parent
                source: plus_btn.pressed
                        ? theme.item.plus_horizontal_clicked
                        : (plus_btn.hovered
                           ? theme.item.plus_horizontal_hot
                           : theme.item.plus_horizontal_normal)
            }
        }

        onClicked: {
            if (control.ref < control.maxRef)
            { // 确保 ref 不超过 maxRef
                control.ref = Math.min(control.maxRef, control.ref + 1); // 从 ref 中加上 1
            }
            control.plus()
        }
    }
} // Rectangle: control
