# Stage 3 report

## Outcome

All 260 authoritative RCC files were classified and all 29 QML files received a validation record.
Validation is **static_only** because no Qt 5.15.2 qmlscene/qmlcachegen/qmake development kit is installed. No engine-valid claim is made.

## Resource classes

| category | count |
|---|---:|
| Qt standard GraphicalEffects | 6 |
| Rapoo device/shared | 6 |
| SmartKey AI core | 17 |
| font/image asset | 231 |

## Ready for the next reconstruction phase

- The 29 recovered QML files and all assets whose references resolve in `qml_dependency_graph.json` can be copied from the read-only stage2_v3 tree.
- The real chat page is `:/MainView.qml`. `SwitchView.qml:100` has one stale unmet `qrc:/QAndAPage.qml` route, but SwitchView is not reachable from a confirmed entry, so no global redirect/alias is recommended now. `Views/AboutusPage.qml` is absent and unreferenced.
- Qt standard GraphicalEffects wrappers are separated from SmartKey AI and Rapoo device/shared QML.

## Missing/unresolved dependencies

- Missing active exact references: 59
- Dynamic unresolved references: 1
- Missing local component/import dependencies: 0
- Missing exact references by source reachability: `{"confirmed_entry_reachable": 6, "not_entry_reachable": 7, "potentially_entry_reachable_dynamic": 46}`
- Treat only confirmed-entry missing QML as an entry-load blocker. Missing assets are runtime/visual gaps; potential-dynamic and unreachable shared-device references are tracked separately.
- See `missing_dependencies.json` for every file/line/target; many missing image paths come from the shared Rapoo theme/device surface, not from fabricated QML source files.

## Backend mocks required

Confirmed-entry mocks: `manager`, `dialog_manager`, `device_list_HL`, `window`, `setting`.
Potential dynamic-theme mocks: none.
Referenced only by unreachable shared/device QML (defer unless activated): `themeManager`, `configure_Manager`, `config_set_HL`, `lang_HL`, `pairmanager`, `kb_travelDistance_manager`.
Exact property/method/signal paths and locations are in `backend_interface_map.json` and `.md`.

## Toolchain gap

Install a project-local Qt 5.15.2 MinGW 8.1 32-bit kit under `reconstruction_analysis/_toolchain`, invoke tools by absolute path, and set QML2_IMPORT_PATH to the deployed application QML module directory. Then run qmlcachegen/per-file parser checks before an isolated engine harness with mocks.
