import QtQuick 2.15

// Text文本不可选中，用TextEdit替代Text作为聊天框文本
TextEdit {
    id: root
    property real scaleFactor: window.scale

    property int m_contentWidth: text.implicitWidth

    color: theme.item.colorB1
    font.pixelSize: 14 * scaleFactor
    font.family: fontManager.item.oppoSansM.name
    readOnly: true

    verticalAlignment: TextEdit.AlignVCenter
    horizontalAlignment: TextEdit.AlignLeft

    selectByMouse: true
    selectionColor: "#3367D1"
    selectedTextColor: "white"
    wrapMode: Text.WordWrap

    Text {
        id: text
        visible: false // 用于计算文本宽度
        font: root.font
        text: root.text
    }

    onLinkActivated: (link) => {
                Qt.openUrlExternally(link) // 在外部浏览器打开链接
            }
}
