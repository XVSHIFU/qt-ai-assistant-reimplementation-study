import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "../Controls"
Item {
    id: root_item
    property string sysos: Qt.platform.os
    //property alias title: text_view.text
    //property alias iconVisible: icon_image_item.visible
    property var win
    property var titleType: Qt.Window
    property var flags: Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint | Qt.WindowTitleHint
    property int winType: 0
    property alias closeMouseArea: close_btn.mouseArea
    property bool isEnabledCloseEvent: true
    property int indexID: -1

    signal myConfigure()  //我的配置
    signal myUser()  //用户中心
    signal myConfigSet()  //我的设置页面
    signal calibrationPage()  //打开校准页面

    height: {
        switch(titleType){
        case Qt.Dialog:
            48
            break
        default:
            94
            break
        }
    }
    width: parent.width

    function deviceListConfig(){
    // 检查设备列表是否为空
        if (device_list_HL.deviceModel.rowCount() === 0) {
            configure_Manager.deleteDevice();
            return;
        }

        for (var i = 0; i < device_list_HL.deviceModel.rowCount(); ++i)
        {
            var item = device_list_HL.deviceModel.get(i);
            configure_Manager.initDevice(item)  //加载初始的配置并导入
        }
    }

    //主页
    PushButton {
        enableHoverState: true
        id: home_btn
        width: lang_HL.optLanguageName === "中文(简体)" ? 100 : 175
        height: 48
        color:clicked ? theme.item.auxiliaryColor :  (hoverd ? theme.item.themeOneColor9 : theme.item.color7)
        radius: height / 2
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin:  24
        imageView.source: home_btn.clicked ? theme.item.homePage_clicked :  (home_btn.hoverd ? theme.item.homePage_hover : theme.item.homePage_normal)
        textView.text: qsTr("Homepage")
        textView.anchors.leftMargin: 8
        textView.horizontalAlignment: Text.AlignLeft
        textView.font.pixelSize: 18
        textView.color: home_btn.clicked ? theme.item.color7 : theme.item.color1

        mouseArea {
        onClicked: {

            switch_view.currentIndexID=0

            //恢复其他图标状态
             setting_btn.clicked = false
        }
        }

        Connections {
            target:switch_view
            onConvertPage_windowTitle: {
                home_btn.clicked = false
            }
        }
    }


    //我的配置
    PushButton {
        enableHoverState: true
        id: profile_btn
        visible:true
      //  width: lang_HL.optLanguageName === "中文(简体)" ? 150 : 170
        width: textView.contentWidth + imageView.width + 40
        height: 48
        color:clicked ? theme.item.auxiliaryColor :  (hoverd ? theme.item.themeOneColor9 : theme.item.color7)
        radius: height / 2
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: home_btn.right
        anchors.leftMargin: 8
        imageView.source:  profile_btn.clicked ? theme.item.profile_clicked :  (profile_btn.hoverd ? theme.item.profile_hover : theme.item.profile_normal)
        textView.text: configure_Manager.keyboardName === ""
                       ? qsTr("My profile")
                       : qsTr("My profile: ") + qsTranslate("ConfigureManager", configure_Manager.keyboardName)
        textView.anchors.leftMargin: 12
        textView.horizontalAlignment: Text.AlignLeft
        textView.font.pixelSize: 18
        textView.color: profile_btn.clicked ? theme.item.color7 : theme.item.color1
        mouseArea {
            onClicked: {
            myConfigure()
            //恢复其他按钮状态
            home_btn.clicked = false
            deviceListConfig() //初始化配置
        }
        }

        Connections {
            target:switch_view
            onResetConfigure: {
                // 禁用悬停状态更新
                profile_btn.clicked  = false
                // 恢复设置图标状态
                setting_btn.clicked = false
            }
        }
    }

    // 新版本、主题、设置、最小化及关闭等按钮工具栏
    Item {
        id: toolBar_right
        width: newVersion_btn.visible
               ? newVersion_btn.width + theme_btn.width + setting_btn.width + minimize_btn.width + close_btn.width + 24 * 6
               : theme_btn.width + setting_btn.width + minimize_btn.width + close_btn.width + 24 * 5
        height: close_btn.height
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        Row {
            anchors.fill: parent
            spacing: 24
            Button {
                id: newVersion_btn
                visible: config_set_HL.updated
                width: newVersion_text.contentWidth + 48 + 16
                height: 42

                enabled: updateLog_popup.opened ? false : true

                background: Rectangle {
                    anchors.fill: newVersion_btn
                    radius: 30
                    border.color: newVersion_btn.hovered
                                  ? theme.item.themeOneColor9
                                  : updateLog_popup.opened
                                    ? theme.item.themeOneColor8
                                    : theme.item.themeOneColor9
                    color: newVersion_btn.hovered
                           ? theme.item.color7
                           : updateLog_popup.opened
                             ? theme.item.themeOneColor9
                             : theme.item.color7

                    Text {
                        id: newVersion_text
                        anchors.left: parent.left
                        anchors.leftMargin: 48
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("New version")
                        font.family: fontManager.item.oppoSansM.name
                        font.pixelSize: 16
                        color: newVersion_btn.hovered
                               ? theme.item.themeOneColor4
                               : updateLog_popup.opened
                                 ? theme.item.auxiliaryColor
                                 : theme.item.auxiliaryColor
                    }

                    Image {
                        id: icon
                        width: 24
                        height: 24
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        source: newVersion_btn.hovered
                                ? "qrc:/Image/TitleView/newVersion_hover.svg"
                                : "qrc:/Image/TitleView/newVersion_normal.svg"
                    }
                }

                onPressed: {
                    if(!updateLog_popup.opened) {
                        config_set_HL.getRemoteDriverUpdateLog()

                        updateLog_popup.open()
                    }
                }
            }

            //月光模式& 日光模式
            ImageButton {
                id: theme_btn
                width: visible ? close_btn.width : 0
                height: visible ? close_btn.height : 0
                visible: flags & Qt.WindowMinimizeButtonHint
                imageView.source: (themeManager.currentTheme === "LightTheme.qml") ? theme.item.light_hover : theme.item.moon_normal
                imageView.sourceSize: Qt.size(minimize_btn.width, minimize_btn.height)

                mouseArea {
                   onClicked: {
                       if (themeManager.currentTheme === "LightTheme.qml") {
                         themeManager.currentTheme = "DarkTheme.qml"; // 切换为月光模式
                         window.skinMode = 1
                       } else if (themeManager.currentTheme === "DarkTheme.qml") {
                         themeManager.currentTheme = "LightTheme.qml"; // 切换为日光模式
                         window.skinMode = 0
                       }
                   }
                   }
            }

            //设置
            ImageButton {
                id: setting_btn
                width: visible ? close_btn.width : 0
                height: visible ? close_btn.height : 0
                visible: flags & Qt.WindowMinimizeButtonHint
                imageView.sourceSize: Qt.size(minimize_btn.width, minimize_btn.height)
                imageView.source: setting_btn.clicked ? theme.item.setting_clicked :  (setting_btn.hoverd ? theme.item.setting_hover : theme.item.setting_normal)
                mouseArea.anchors.margins: 4

                MouseArea {
                    id: mouseArea_setting
                    anchors.fill: parent
                    onClicked: {
                        myConfigSet()
                        pairmanager.init()
                        setting_btn.clicked = true

                        //恢复其他图标状态
                      //  home_btn.clicked = false
                    }
                }
                tooltip: CustomToolTip{
                    anchorItem: setting_btn
                    delay: 600
                    text: qsTr("Setting")
                }
            }




            //分割线
            Rectangle{
                id:split1
                height: 13
                width:1
                visible: flags
                color:"#C3C5C9"
                anchors.verticalCenter: minimize_btn.verticalCenter
            }


            //最小化
            ImageButton {
                id: minimize_btn
                height: 42
                width: 42
                visible: flags & Qt.WindowMinimizeButtonHint
                imageView.sourceSize: Qt.size(minimize_btn.width, minimize_btn.height)
                imageView.source: minimize_btn.hoverd ? theme.item.minus_hover : theme.item.minus_normal
                mouseArea.anchors.margins: 4
                MouseArea {
                    id: mouseArea_minus
                    anchors.fill: parent
                    onClicked: {
                        win.showMinimized()
                    }
                }

                tooltip: CustomToolTip{
                    anchorItem: minimize_btn
                    delay: 600
                    text: qsTr("Minimize")
                }
            }

            //关闭
            ImageButton {//32*24  //42
                id: close_btn
                height: 42
                width: 42
                visible: flags & Qt.WindowCloseButtonHint
                imageView.sourceSize: Qt.size(close_btn.width, close_btn.height)
                imageView.source: close_btn.clicked ? theme.item.close_clicked :  (close_btn.hoverd ? theme.item.close_hover : theme.item.close_normal)
                color:theme.item.color7
                mouseArea.anchors.margins: 4

                MouseArea {
                    id: mouseArea_close
                    anchors.fill: parent
                    onClicked: {
                        if(isEnabledCloseEvent){
                            if(!winType) win.close()
                            else{
                                win.active = false
                                win.source = ""
                            }
                        }
                    }
                }
                tooltip: CustomToolTip{
                    anchorItem: close_btn
                    delay: 600
                    text: qsTr("Close")
                }
            }
        }
    }

    function exitDemoModeAndResetAdapCalibration() {
        //console.log("key_level_indication_panel visible: false");
        // 退出演示模式（按键级数示意模块不可用）
        kb_travelDistance_manager.exitCalibOrDemoMode(false, true)
        // 按用户原设置启动/关闭自适应校准功能
        var adapCalibration = kb_travelDistance_manager.adapCalibration;
        kb_travelDistance_manager.setWasdShake(adapCalibration);
    }
}
