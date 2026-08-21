# Stage 4 reconstruction report

## Outcome

The project uses Qt 5.15.2 with MinGW 8.1 targeting `i686-w64-mingw32`. A clean qmake release build embeds all 260 authoritative RCC files and produces a 32-bit PE. The real Qt Quick engine loads `qrc:/MainView.qml` with a non-null root object under the software/offscreen smoke environment.

## Resource fidelity

`qml/recovered/` is a mechanical byte copy of the Stage2 v3 authoritative tree. `recovered_file_hashes.json` and `reports/recovered_copy_verification.json` contain one record per file and require all 260 SHA-256 values to match. There are no adapted QML files and no invented `QAndAPage.qml` or `Views/AboutusPage.qml`.

## Engine validation

`scripts/smoke_test.ps1` runs the release executable with `QT_QPA_PLATFORM=offscreen` and `QT_QUICK_BACKEND=software`. The executable first calls `QQuickView::grabWindow()`. On this Windows offscreen plugin the call returns a null image, so the implemented fallback renders the actual root `QQuickItem` using `grabToImage`; it does not generate a placeholder. The capture method is explicit in `qml_engine_validation.json`.

## Mock boundary

Only the five Stage3 confirmed-entry context objects are registered: `window`, `dialog_manager`, `setting`, `manager`, and `device_list_HL`. They expose all 60 referenced members and basic local demonstration rows. Network/AI requests, device discovery and control, persisted chat history/accounts, updater behavior, startup registration, clipboard behavior, and auxiliary window orchestration remain mocks.

The next backend phase should replace those five context objects one contract at a time, starting with chat persistence/service (`dialog_manager`), then real window orchestration (`window`), settings/update storage (`setting`), tray actions (`manager`), and Rapoo device discovery/model (`device_list_HL`).

