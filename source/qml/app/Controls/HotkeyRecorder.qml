import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

FocusScope {
    id: control
    implicitWidth: 300
    implicitHeight: 40

    property alias text: captureField.text
    property string placeholderText: qsTr("尚未绑定")
    property bool recording: false
    property string originalText: ""
    readonly property bool escapeCancelsRecording: true
    readonly property bool tabPreservesNavigation: true
    readonly property bool focusRingVisible: captureField.focusRingVisible
    signal sequenceCaptured(string sequence)

    function shortcutText(key, modifiers) {
        var keyName = ""
        if (key >= Qt.Key_A && key <= Qt.Key_Z)
            keyName = String.fromCharCode(key)
        else if (key >= Qt.Key_0 && key <= Qt.Key_9)
            keyName = String.fromCharCode(key)
        else if (key === Qt.Key_Space)
            keyName = "Space"
        else if (key >= Qt.Key_F1 && key <= Qt.Key_F24)
            keyName = "F" + (key - Qt.Key_F1 + 1)
        if (keyName === "")
            return ""

        var parts = []
        if (modifiers & Qt.ControlModifier) parts.push("Ctrl")
        if (modifiers & Qt.AltModifier) parts.push("Alt")
        if (modifiers & Qt.ShiftModifier) parts.push("Shift")
        if (modifiers & Qt.MetaModifier) parts.push("Meta")
        parts.push(keyName)
        return parts.join("+")
    }

    function beginRecording() {
        if (recording)
            return
        originalText = text
        recording = true
        captureField.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    function cancelRecording() {
        if (!recording)
            return
        text = originalText
        recording = false
    }

    // Returns true when the recorder consumes a key. Tab/Shift+Tab always
    // return false so Qt's normal focus traversal remains authoritative.
    function handleKey(key, modifiers) {
        if (!recording)
            return false
        if (key === Qt.Key_Escape) {
            cancelRecording()
            return true
        }
        if (key === Qt.Key_Tab || key === Qt.Key_Backtab) {
            cancelRecording()
            return false
        }
        var value = shortcutText(key, modifiers)
        if (value !== "") {
            text = value
            recording = false
            sequenceCaptured(value)
        }
        return true
    }

    RowLayout {
        anchors.fill: parent
        spacing: 8

        ModernTextField {
            id: captureField
            objectName: "hotkeyCaptureField"
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            placeholderText: control.recording
                             ? qsTr("请按组合键；Esc 取消") : control.placeholderText
            accessibleName: qsTr("全局快捷键组合")
            accessibleDescription: control.recording
                                   ? qsTr("正在录制。按 Escape 取消，Tab 移到下一项。")
                                   : qsTr("使用后方的录制按钮开始录入组合键。")
            Keys.onPressed: function(event) {
                event.accepted = control.handleKey(event.key, event.modifiers)
            }
            onActiveFocusChanged: {
                if (!activeFocus && control.recording)
                    control.cancelRecording()
            }
        }

        ModernButton {
            id: recordButton
            objectName: "hotkeyRecordButton"
            Layout.preferredWidth: 72
            Layout.fillHeight: true
            text: control.recording ? qsTr("取消") : qsTr("录制")
            accessibleDescription: control.recording
                                   ? qsTr("取消快捷键录制") : qsTr("开始录制快捷键")
            onClicked: control.recording
                       ? control.cancelRecording() : control.beginRecording()
        }
    }
}
