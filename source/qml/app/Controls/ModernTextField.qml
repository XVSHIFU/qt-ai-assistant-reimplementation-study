import QtQuick 2.15
import QtQuick.Controls 2.15

TextField {
    id: control
    implicitHeight: uiPreferences.touchTarget
    leftPadding: uiPreferences.spacingMd
    rightPadding: uiPreferences.spacingMd
    font.pixelSize: uiPreferences.fontBody
    color: uiPreferences.textPrimaryColor
    placeholderTextColor: uiPreferences.placeholderColor
    selectionColor: uiPreferences.selectionColor
    selectedTextColor: uiPreferences.textPrimaryColor
    activeFocusOnTab: true
    property string accessibleName: placeholderText
    property string accessibleDescription: ""
    readonly property bool focusRingVisible: control.activeFocus
    Accessible.role: Accessible.EditableText
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription

    background: Rectangle {
        radius: uiPreferences.radiusSm
        color: control.enabled ? uiPreferences.surfaceColor : uiPreferences.disabledSurfaceColor
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? uiPreferences.focusRingColor
                                          : uiPreferences.borderColor
        Behavior on border.color { ColorAnimation { duration: 83 } }
    }
}
