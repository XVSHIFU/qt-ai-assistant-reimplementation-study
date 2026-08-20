import QtQuick 2.15
import QtQuick.Controls 2.15

ToolTip {
    property alias tipBackground: bk_rec
    property alias tipTextView: tx_rec
    property bool displayTop: false
    id: control
    y: displayTop ? -bk_rec.height - 2 : parent.height + 2
    contentItem: Text {
        id: core_rx
        visible: false
        font.pixelSize: 12
        text: control.text
        wrapMode: tx_rec.wrapMode
    }
    background: Rectangle{
        id: bk_rec
        radius: 6
        color: window.sysSkinType ? "#4A556E" : "#D7E0F3"
        border.color: "transparent"
        width: core_rx.width + 12
        height: core_rx.height + 16
        Text {
            id: tx_rec
            anchors.centerIn: parent
            font.pixelSize: 12
            text: control.text
            color: "#000000"
            font.family: fontManager.item.oppoSansR.name
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

    }
}
