import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.12

// “按键设置”模块切换按钮，用于开启或关闭“交换WASD”
Button {
    id: btn
    focusPolicy: Qt.NoFocus
    checkable: true
    checked: false

    property string checkedSource: "qrc:/Image/Settings/checkbox_open.svg"
    property string uncheckedSource: "qrc:/Image/Settings/checkbox_close.svg"

    background: Image {
        source: btn.checked
                ? btn.checkedSource
                : btn.uncheckedSource

        Behavior on source {
            PropertyAnimation {
                property: "opacity"
                duration: 200
                from: 0.7
                to: 0.1
            }
        }
    }
}
