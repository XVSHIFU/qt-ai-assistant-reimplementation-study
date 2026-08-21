# QML static validation

Validation mode: **static_only**. No Qt engine/parser result is claimed.

- QML files checked: 29
- Static checks passed: 29
- Static checks failed: 0
- Engine validations run: 0
- Active resource references: 234
- Missing active references: 59
- Dynamic unresolved references: 1

| file | encoding | root object | reachability | static status | engine status |
|---|---|---|---|---|---|
| `:/Component/SwitchView.qml` | utf-8-sig | `Item` | not_entry_reachable | static_checks_passed | not_run |
| `:/Component/WindowTitleBar.qml` | utf-8 | `Item` | not_entry_reachable | static_checks_passed | not_run |
| `:/Controls/CustomRadioButton2.qml` | utf-8 | `RadioButton` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/Controls/CustomSensitivitySlider1.qml` | utf-8 | `Rectangle` | not_entry_reachable | static_checks_passed | not_run |
| `:/Controls/CustomSensitivitySlider1_New.qml` | utf-8 | `Rectangle` | not_entry_reachable | static_checks_passed | not_run |
| `:/Controls/CustomToggleButton.qml` | utf-8 | `Button` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/Controls/CustomToolTip.qml` | utf-8 | `ToolTip` | not_entry_reachable | static_checks_passed | not_run |
| `:/Controls/ImageButton.qml` | utf-8 | `Rectangle` | not_entry_reachable | static_checks_passed | not_run |
| `:/Controls/PushButton.qml` | utf-8 | `Rectangle` | not_entry_reachable | static_checks_passed | not_run |
| `:/Controls/SelectableText.qml` | utf-8 | `TextEdit` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/Controls/ShadowRectangle.qml` | utf-8 | `Item` | not_entry_reachable | static_checks_passed | not_run |
| `:/DialogModule/ChatHistory.qml` | utf-8 | `Item` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/DialogModule/DialogPanel.qml` | utf-8 | `Rectangle` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/DialogModule/MorePopup.qml` | utf-8 | `Popup` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/firstDiaLog.qml` | utf-8 | `ApplicationWindow` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/FontManager.qml` | utf-8 | `QtObject` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/GraphicalEffects/DesaturateEffect.qml` | utf-8 | `Desaturate` | not_entry_reachable | static_checks_passed | not_run |
| `:/GraphicalEffects/DropShadowEffect.qml` | utf-8 | `DropShadow` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/GraphicalEffects/FastBlurEffect.qml` | utf-8 | `FastBlur` | not_entry_reachable | static_checks_passed | not_run |
| `:/GraphicalEffects/GlowEffect.qml` | utf-8 | `Glow` | not_entry_reachable | static_checks_passed | not_run |
| `:/GraphicalEffects/OpacityMaskEffect.qml` | utf-8 | `OpacityMask` | not_entry_reachable | static_checks_passed | not_run |
| `:/GuideWindow.qml` | utf-8 | `ApplicationWindow` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/MainView.qml` | utf-8-sig | `Item` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/SettingsPage.qml` | utf-8 | `ApplicationWindow` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/Theme/DarkTheme.qml` | utf-8 | `QtObject` | potentially_entry_reachable_dynamic | static_checks_passed | not_run |
| `:/Theme/LightTheme.qml` | utf-8 | `QtObject` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/Theme/NeutralTheme.qml` | utf-8 | `QtObject` | potentially_entry_reachable_dynamic | static_checks_passed | not_run |
| `:/TrayIconMenu.qml` | utf-8 | `Item` | confirmed_entry_reachable | static_checks_passed | not_run |
| `:/updateWindow.qml` | utf-8 | `ApplicationWindow` | confirmed_entry_reachable | static_checks_passed | not_run |
