import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    implicitHeight: uiPreferences.touchTarget
    leftPadding: uiPreferences.spacingLg
    rightPadding: uiPreferences.spacingLg
    font.pixelSize: uiPreferences.fontBody
    activeFocusOnTab: true
    property string accessibleDescription: ""
    readonly property bool focusRingVisible: control.activeFocus
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: accessibleDescription

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? uiPreferences.surfaceColor
                                   : (control.enabled ? uiPreferences.textPrimaryColor
                                                      : uiPreferences.disabledTextColor)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: uiPreferences.radiusSm
        color: control.flat ? (control.hovered ? uiPreferences.disabledSurfaceColor : "transparent")
                            : control.highlighted
                              ? (control.down ? uiPreferences.accentHoverColor : uiPreferences.accentColor)
                              : control.down ? uiPreferences.disabledSurfaceColor
                                             : control.hovered ? uiPreferences.backgroundColor
                                                               : uiPreferences.surfaceColor
        border.width: control.activeFocus ? 2 : (control.flat || control.highlighted ? 0 : 1)
        border.color: control.activeFocus ? uiPreferences.focusRingColor
                                          : uiPreferences.borderColor
    }
}
