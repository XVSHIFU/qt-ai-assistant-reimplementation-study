import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.12
import "qrc:/GraphicalEffects"

Rectangle {
    id: root_item
    property alias imageView: image_view
    property alias textView: text_view
    property alias mouseArea: mouse_area
    property bool enableHoverState: false
    property bool hoverd: false
    property bool clicked: false
    property ToolTip tooltip
    radius: 73

    Rectangle{
        id:rect
        anchors.fill: parent
        radius: parent.radius
        border.color: "#00000000"
        color:clicked ? theme.item.auxiliaryColor :  (hoverd ? theme.item.themeOneColor9 : theme.item.color7)

        opacity: 0.2
        visible: (enableHoverState && hoverd && enabled)
    }

    layer.enabled: false
    layer.samples: Qt.platform.os === "osx" ? 0 : 6
    //@disable-check M301
    layer.effect: DropShadowEffect {
        color: "#4023252B"
        //anchors.fill: root_item
        horizontalOffset: 0
        verticalOffset: 1
        spread: 0.1
        radius: 7.0
        //source: root_item
    }
    Image {
        id: image_view
        width: height
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.bottomMargin: 10
        anchors.topMargin: 10
        smooth: true
        mipmap: true
        antialiasing: true
        fillMode: Image.PreserveAspectFit
    }

    Text {
        id: text_view
        text: "Text"
        anchors.left: image_view.visible ? image_view.right : parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        color:theme.item.color1

      //  wrapMode: Text.WordWrap
        font.family: fontManager.item.oppoSansB.name
        font.pixelSize: 20
    }

    MouseArea{
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            root_item.clicked = true
        }

        onEntered: {
            //console.log("onEntered")
            hoverd = true
            if(!tooltip) return
            if(tooltip.text.length){
                tooltip.visible = true
            }
        }
        onExited: {
            //console.log("onExited")
            hoverd = false
            if(!tooltip) return
            tooltip.visible = false
        }
    }
}
