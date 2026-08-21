import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.12
import "qrc:/GraphicalEffects"

Rectangle {
    id:root_item
    property alias mouseArea: mouse_area
    property alias imageView: image_view
    property bool hoverd: false
    property bool clicked: false
    property bool enableHoverState: false
    property ToolTip tooltip
    property int zoomin: 1
    property bool updated: false // 是否显示更新标志
    color: "#00000000"
    radius:30
    Image {
        id: image_view
        anchors.fill: parent
        anchors.margins: enableHoverState && hoverd ? -zoomin : 0
        fillMode: Image.PreserveAspectFit
        smooth: true
        //mipmap: true
        //antialiasing: true
        sourceSize: Qt.size(width, height)
        opacity: enabled ? 1 : 0.2
    }

    MouseArea {
        id: mouse_area
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            root_item.clicked = true
        }
        onReleased: {
            root_item.clicked = false
        }
        onCanceled: {
            root_item.clicked = false
        }
        onEntered: {
            hoverd = true
            if(!tooltip) return
            if(tooltip.text.length){
                tooltip.visible = true
            }
        }
        onExited: {
            hoverd = false
            if(!tooltip) return
            tooltip.visible = false
        }
    }
}
