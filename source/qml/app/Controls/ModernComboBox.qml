import QtQuick 2.15
import QtQuick.Controls 2.15

ComboBox {
    id: control
    implicitHeight: uiPreferences.touchTarget
    leftPadding: uiPreferences.spacingMd
    rightPadding: 34
    font.pixelSize: uiPreferences.fontBody
    activeFocusOnTab: true
    property string accessibleName: displayText
    property string accessibleDescription: ""
    readonly property bool focusRingVisible: control.activeFocus
    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription

    contentItem: Text {
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
        text: control.displayText
        font: control.font
        color: control.enabled ? uiPreferences.textPrimaryColor
                               : uiPreferences.disabledTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle
    }

    indicator: Text {
        x: control.width - width - 12
        y: (control.height - height) / 2
        text: control.popup.visible ? "^" : "v"
        color: uiPreferences.textSecondaryColor
        font.pixelSize: uiPreferences.fontBody
    }

    background: Rectangle {
        radius: uiPreferences.radiusSm
        color: control.enabled ? uiPreferences.surfaceColor : uiPreferences.disabledSurfaceColor
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? uiPreferences.focusRingColor
                                          : uiPreferences.borderColor
    }

    delegate: ItemDelegate {
        width: control.width
        implicitHeight: 38
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole ? (Array.isArray(control.model)
                                      ? modelData[control.textRole] : model[control.textRole])
                                   : modelData
            color: uiPreferences.textPrimaryColor
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
        }
        background: Rectangle {
            radius: uiPreferences.radiusSm
            color: parent.highlighted ? uiPreferences.disabledSurfaceColor : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            radius: uiPreferences.radiusMd
            color: uiPreferences.elevatedColor
            border.color: uiPreferences.borderColor
        }
    }
}
