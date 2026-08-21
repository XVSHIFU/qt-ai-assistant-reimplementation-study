# Application flow

## Entry/loading evidence

The RCC tree proves the source paths. Targeted PE xrefs then show which Qt loading API receives each exact qrc literal:

| resource | root object | confirmed loading calls |
|---|---|---|
| `:/MainView.qml` | `Item` | `QQuickView::setSource @ 0x41ded4`, `QQuickView::setSource @ 0x4420f3` |
| `:/firstDiaLog.qml` | `ApplicationWindow` | `QQmlApplicationEngine::load @ 0x422739` |
| `:/updateWindow.qml` | `ApplicationWindow` | `QQmlApplicationEngine::load @ 0x4218bd` |
| `:/GuideWindow.qml` | `ApplicationWindow` | `QQmlApplicationEngine::load @ 0x42165a`, `QQmlApplicationEngine::load @ 0x42253d` |
| `:/SettingsPage.qml` | `ApplicationWindow` | `QQmlApplicationEngine::load @ 0x41c995` |
| `:/TrayIconMenu.qml` | `Item` | `QQuickView::setSource @ 0x41c637` |

## Main and auxiliary windows

- `:/MainView.qml` is the real AI chat main page. Two exact literal xrefs feed `QQuickView::setSource`; it depends on `window`, `dialog_manager`, `DialogModule`, `Controls`, `Component`, theme/font loaders and chat/history components.
- `:/firstDiaLog.qml` is a separate consent/first-run `Window`; `window.agreePolicy`, `openWebsite` and `setFirstStart` must exist before it can function.
- `:/updateWindow.qml` is a separate update `Window`; it reads version state and calls `window.updateSoft()` or `window.showGuideWindow()`.
- `:/GuideWindow.qml` is a separate guide/reminder `Window` with shared theme/font loaders and image dependencies.
- `:/SettingsPage.qml` is loaded by `QQmlApplicationEngine::load`; it uses the inferred SettingsPageManager instance `setting` for version/update/quota/startup behavior.
- `:/TrayIconMenu.qml` is loaded with `QQuickView::setSource`; it needs `manager`, `setting`, `device_list_HL`, dynamic theme loading and the device model roles.

## Corrected legacy assumptions

- `:/MainView.qml` is authoritative and must be the chat main-page source in the next phase.
- `QAndAPage.qml` does not exist in the RCC tree, but `:/Component/SwitchView.qml:100` contains one active unmet legacy loader reference. `SwitchView.qml` is not reachable from any PE-confirmed entry through recovered QML dependencies, so this is not a blocker for the confirmed SmartKey AI entry flow. If a later phase intentionally activates that shared device component, resolve its route deliberately (likely to authoritative `:/MainView.qml`) rather than fabricating a second page implementation or adding a global alias now.
- `Views/AboutusPage.qml` neither exists in the RCC tree nor appears in recovered QML dependencies. Binary string presence alone is insufficient evidence and does not make it required source.

## Required initialization order for a harness

1. Register/mock context objects (`window`, `dialog_manager`, `setting`, device/configuration managers).
2. Make the recovered RCC paths and deployed Qt 5.15.2 QML import modules visible.
3. Load theme and font resources used by each top-level file.
4. Load the selected top-level `Window`/`Item`; only then exercise signals and navigation.
