import QtQuick 2.15
import QtQuick.Controls 2.15
Item{
    id: root_item
    clip: true

    property int currentIndexID: -1

    signal configure()  //打开我的配置
    signal configset()  //打开我的设置页面
    signal userCerten()  //打开用户中心

    signal convertTheme()  //控制顶部栏主题按钮-由设置页发出
    signal convertPage() //主页设备点击后恢复主页按钮-由主页发出
    signal convertPage_windowTitle()  //本页发到顶部栏

    signal resetConfigure()  //恢复重置-配置按钮
    signal calibratinPage_windowTitle()  //由顶部栏发出打开校准

    onCurrentIndexIDChanged: {
        left_ani.start()
        animate_timer.start()
    }

    onConvertPage:{
        convertPage_windowTitle()
    }

    WindowTitleBar{
        id: title_view
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 0
        anchors.topMargin: 0
        anchors.rightMargin: 0
        win: window
        indexID:currentIndexID
        flags: Qt.WindowMinimizeButtonHint | Qt.WindowCloseButtonHint

        onMyConfigure: {
            configure()
        }

        onMyConfigSet:{
            configset()
        }

        onMyUser: {
            userCerten()
        }

        onCalibrationPage:{
            calibratinPage_windowTitle()
        }
    }

    Loader {
        id: loader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: title_view.bottom
        anchors.bottom: parent.bottom
    }

    Rectangle{
        id: enter_rec
        color: theme.item.backgroundColor
        radius: window.radius
        //anchors.fill: parent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: title_view.bottom
        anchors.bottom: parent.bottom
    }

    OpacityAnimator {
        id: enter_ani
        target: enter_rec
        from: 1
        to: 0
        duration: 350
        easing.type: Easing.InQuint
    }
    OpacityAnimator {
        id: left_ani
        target: enter_rec
        from: 0
        to: 1
        duration: 350
        easing.type: Easing.OutQuint
    }

    Timer {
        id: animate_timer
        interval: 350
        onTriggered: {
            switch(currentIndexID){
            case 0:
                loader.source = "qrc:/QAndAPage.qml"
                break
            }
            enter_ani.start()
        }
    }

    Connections {
        target: device_list_HL
        function onDeviceDisconnection() {
           currentIndexID=0
        }
    }

    Connections {
        target: root
        onResetConfigure:{
            resetConfigure()
        }
    }
}
