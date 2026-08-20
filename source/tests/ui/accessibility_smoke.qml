import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/Controls"
import "qrc:/Theme"

Window {
    id: root
    width: 480
    height: 300
    visible: true
    title: "Accessibility smoke"

    property bool tabOrderPassed: false
    property bool escapeContractPassed: false
    property bool tabContractPassed: false
    property bool namesContractPassed: false
    property bool focusContractPassed: false
    property bool contrastContractPassed: false
    readonly property bool preliminaryPassed: escapeContractPassed
            && tabContractPassed && namesContractPassed
            && focusContractPassed && contrastContractPassed
    readonly property bool passed: preliminaryPassed && tabOrderPassed
    property string failure: "not completed"

    function linearChannel(value) {
        return value <= 0.03928 ? value / 12.92
                                : Math.pow((value + 0.055) / 1.055, 2.4)
    }

    function luminance(color) {
        return 0.2126 * linearChannel(color.r)
                + 0.7152 * linearChannel(color.g)
                + 0.0722 * linearChannel(color.b)
    }

    function contrastRatio(foreground, background) {
        var first = luminance(foreground)
        var second = luminance(background)
        var lighter = Math.max(first, second)
        var darker = Math.min(first, second)
        return (lighter + 0.05) / (darker + 0.05)
    }

    LightTheme { id: lightTheme }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        HotkeyRecorder {
            id: recorder
            objectName: "hotkeyRecorderUnderTest"
            Layout.fillWidth: true
        }
        ModernTextField {
            id: firstField
            objectName: "firstTabItem"
            Layout.fillWidth: true
            accessibleName: "消息输入"
            KeyNavigation.tab: providerCombo
        }
        ModernComboBox {
            id: providerCombo
            objectName: "secondTabItem"
            Layout.fillWidth: true
            model: ["DeepSeek", "Compatible"]
            accessibleName: "Provider"
            KeyNavigation.backtab: firstField
            KeyNavigation.tab: enabledSwitch
        }
        ModernSwitch {
            id: enabledSwitch
            text: "开机后台启动"
            KeyNavigation.tab: saveButton
        }
        ModernButton {
            id: saveButton
            text: "保存"
            KeyNavigation.tab: firstField
        }
    }

    Component.onCompleted: {
        recorder.text = "Ctrl+Space"
        recorder.beginRecording()
        recorder.text = "Alt+K"
        var escapeConsumed = recorder.handleKey(Qt.Key_Escape, Qt.NoModifier)
        escapeContractPassed = escapeConsumed && !recorder.recording
                && recorder.text === "Ctrl+Space"

        recorder.beginRecording()
        var tabConsumed = recorder.handleKey(Qt.Key_Tab, Qt.NoModifier)
        tabContractPassed = !tabConsumed && !recorder.recording
                && recorder.text === "Ctrl+Space"

        firstField.forceActiveFocus(Qt.TabFocusReason)
        namesContractPassed = firstField.Accessible.name !== ""
                && providerCombo.Accessible.name !== ""
                && enabledSwitch.Accessible.name !== ""
                && saveButton.Accessible.name !== ""
        focusContractPassed = firstField.focusRingVisible
        contrastContractPassed = contrastRatio(lightTheme.placeholderTextColor,
                                           lightTheme.colorW1) >= 4.5
                && contrastRatio(lightTheme.mutedTextColor,
                                 lightTheme.colorW1) >= 4.5
        failure = "waiting for focus and Tab traversal"
    }

    onTabOrderPassedChanged: {
        if (tabOrderPassed && preliminaryPassed)
            failure = ""
    }
    onPassedChanged: if (passed) failure = ""
}
