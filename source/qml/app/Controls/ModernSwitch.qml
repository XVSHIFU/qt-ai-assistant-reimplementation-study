import QtQuick 2.15
import QtQuick.Controls 2.15

Switch {
    id: control
    implicitHeight: uiPreferences.touchTarget
    spacing: uiPreferences.spacingSm
    activeFocusOnTab: true
    property string accessibleDescription: ""
    readonly property bool focusRingVisible: control.activeFocus
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.description: accessibleDescription
    Accessible.checked: checked

    background: Rectangle {
        color: "transparent"
        radius: uiPreferences.radiusSm
        border.width: control.activeFocus ? 2 : 0
        border.color: uiPreferences.focusRingColor
    }

    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        x: 0
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? uiPreferences.accentColor : uiPreferences.disabledTextColor
        Behavior on color { ColorAnimation { duration: 83 } }

        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 2
            x: control.checked ? parent.width - width - 2 : 2
            color: uiPreferences.surfaceColor
            Behavior on x { NumberAnimation { duration: 167; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? uiPreferences.textPrimaryColor : uiPreferences.disabledTextColor
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: uiPreferences.fontBody
    }
}
