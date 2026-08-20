import QtQuick 2.15
import QtQuick.Controls 2.15

// “新建宏”页多处复用单选按钮（用于切换宏播放方式）
RadioButton {
    id: root
    width: 24
    height: 24
    focusPolicy: Qt.NoFocus
    checkable: true

    property int size: 14
    property string noEnabled: "qrc:/Image/Icon/rBtn_enabled.svg"

    background: Image {
        anchors.fill: parent
        width: !parent.enabled
               ? size
               : (parent.checked
                 ? 24
                 : 24)
        height: !parent.enabled
                ? size
                : (parent.checked
                  ? 24
                  : 24)
        source: !parent.enabled
                ? noEnabled
                : (parent.checked
                  ? "qrc:/Image/Icon/rBtn_checked.svg"
                  : "qrc:/Image/Icon/rBtn_unchecked_day.svg")
    }

    indicator: Rectangle {
        visible: false
    }
}
