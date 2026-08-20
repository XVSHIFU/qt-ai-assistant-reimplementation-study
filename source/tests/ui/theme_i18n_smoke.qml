import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root
    width: 320; height: 160; visible: false
    property bool passed: false
    property string failure: "not completed"
    property int stage: 0
    property string localizedSend: qsTranslate("MainView", "发送")
    property string missingFallback: qsTranslate("MainView", "__missing_translation_fallback__")

    function linear(v) { return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4) }
    function luminance(c) { return 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b) }
    function contrast(a, b) {
        var x = luminance(a), y = luminance(b)
        return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05)
    }

    Timer {
        interval: 80; repeat: true; running: true
        onTriggered: {
            if (root.stage === 0) {
                uiPreferences.setThemeMode("light")
                if (uiPreferences.resolvedTheme !== "light" || root.contrast(uiPreferences.textPrimaryColor, uiPreferences.surfaceColor) < 4.5) {
                    root.failure = "light contrast"; running = false; return
                }
                uiPreferences.setThemeMode("dark"); root.stage = 1; return
            }
            if (root.stage === 1) {
                if (uiPreferences.themeMode !== "light" || uiPreferences.resolvedTheme !== "light") {
                    root.failure = "dark mode was not rejected"; running = false; return
                }
                uiPreferences.setThemeMode("high-contrast"); root.stage = 2; return
            }
            if (root.stage === 2) {
                if (uiPreferences.themeMode !== "light" || uiPreferences.resolvedTheme !== "light" || uiPreferences.highContrast) {
                    root.failure = "high contrast mode was not rejected"; running = false; return
                }
                uiPreferences.setLanguage("en_US"); root.stage = 3; return
            }
            if (root.stage === 3) {
                if (root.localizedSend !== "Send" || root.missingFallback !== "__missing_translation_fallback__") {
                    root.failure = "English/fallback"; running = false; return
                }
                uiPreferences.setLanguage("zh_CN"); root.stage = 4; return
            }
            root.passed = root.localizedSend === "发送"
            root.failure = root.passed ? "" : "Chinese translation"
            uiPreferences.setThemeMode("system"); uiPreferences.setLanguage("system")
            running = false
        }
    }
}
