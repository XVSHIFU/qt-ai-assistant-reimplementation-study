# Backend interface map

> Static extraction only. Member names and line locations are authoritative QML uses; C++ signatures/types remain to be reconstructed or mocked.

| object | role | status | occurrences |
|---|---|---|---:|
| `manager` | tray/menu controller | referenced | 2 |
| `dialog_manager` | AI chat/dialog controller | referenced | 44 |
| `device_list_HL` | connected Rapoo device list controller | referenced | 17 |
| `window` | C++ window/ViewHandler-facing object | referenced | 75 |
| `setting` | settings/update controller; likely SettingsPageManager instance | referenced | 13 |
| `themeManager` | theme state controller | referenced | 5 |
| `configure_Manager` | device configuration controller | referenced | 4 |
| `config_set_HL` | configuration/update controller | referenced | 2 |
| `lang_HL` | language controller | referenced | 1 |
| `pairmanager` | device pairing controller | referenced | 1 |
| `kb_travelDistance_manager` | keyboard travel/calibration controller | referenced | 7 |
| `recordDataModel` | requested legacy/candidate model name | not_referenced_in_recovered_qml | 0 |
| `chatHistoryModel` | requested legacy/candidate model name | not_referenced_in_recovered_qml | 0 |
| `SettingsPageManager` | requested C++ type/concept name | not_referenced_in_recovered_qml | 0 |
| `ViewHandler` | requested C++ type/concept name | not_referenced_in_recovered_qml | 0 |

## `manager`

| member | access | count | locations |
|---|---|---:|---|
| `currentTheme` | property_read | 1 | `:/TrayIconMenu.qml:16` (confirmed_entry_reachable) |
| `itemClicked` | method_call | 1 | `:/TrayIconMenu.qml:104` (confirmed_entry_reachable) |

## `dialog_manager`

| member | access | count | locations |
|---|---|---:|---|
| `addNewChat` | method_call | 1 | `:/MainView.qml:802` (confirmed_entry_reachable) |
| `appended` | signal_subscription | 1 | `:/DialogModule/DialogPanel.qml:54` (confirmed_entry_reachable) |
| `copy` | method_call | 2 | `:/DialogModule/DialogPanel.qml:120` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:320` (confirmed_entry_reachable) |
| `currentIndex` | property_read | 1 | `:/DialogModule/ChatHistory.qml:47` (confirmed_entry_reachable) |
| `currentIndex` | property_write | 2 | `:/DialogModule/ChatHistory.qml:78` (confirmed_entry_reachable), `:/DialogModule/ChatHistory.qml:219` (confirmed_entry_reachable) |
| `deleteChatAtIndex` | method_call | 1 | `:/DialogModule/ChatHistory.qml:251` (confirmed_entry_reachable) |
| `dialogInfoModel` | property_read | 1 | `:/DialogModule/ChatHistory.qml:48` (confirmed_entry_reachable) |
| `dialogModel` | property_read | 2 | `:/DialogModule/DialogPanel.qml:32` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:52` (confirmed_entry_reachable) |
| `dialogModel.clearAll` | method_call | 2 | `:/DialogModule/ChatHistory.qml:80` (confirmed_entry_reachable), `:/DialogModule/ChatHistory.qml:221` (confirmed_entry_reachable) |
| `dialogModel.rowCount` | method_call | 4 | `:/DialogModule/ChatHistory.qml:72` (confirmed_entry_reachable), `:/DialogModule/ChatHistory.qml:213` (confirmed_entry_reachable), `:/MainView.qml:384` (confirmed_entry_reachable), `:/MainView.qml:797` (confirmed_entry_reachable) |
| `isChatEmpty` | property_read | 1 | `:/DialogModule/DialogPanel.qml:19` (confirmed_entry_reachable) |
| `isHistoryEmpty` | property_read | 1 | `:/MainView.qml:465` (confirmed_entry_reachable) |
| `modifyTitleAtIndex` | method_call | 1 | `:/DialogModule/ChatHistory.qml:169` (confirmed_entry_reachable) |
| `parseJSONToCurrentChat` | method_call | 2 | `:/DialogModule/ChatHistory.qml:81` (confirmed_entry_reachable), `:/DialogModule/ChatHistory.qml:222` (confirmed_entry_reachable) |
| `reasonModeActive` | property_read | 4 | `:/MainView.qml:128` (confirmed_entry_reachable), `:/MainView.qml:179` (confirmed_entry_reachable), `:/MainView.qml:647` (confirmed_entry_reachable), `:/MainView.qml:691` (confirmed_entry_reachable) |
| `reasonModeActive` | property_write | 2 | `:/MainView.qml:179` (confirmed_entry_reachable), `:/MainView.qml:691` (confirmed_entry_reachable) |
| `regenerate` | method_call | 1 | `:/DialogModule/DialogPanel.qml:371` (confirmed_entry_reachable) |
| `responseGenerated` | property_read | 8 | `:/DialogModule/ChatHistory.qml:73` (confirmed_entry_reachable), `:/DialogModule/ChatHistory.qml:214` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:232` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:283` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:305` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:357` (confirmed_entry_reachable), `:/MainView.qml:385` (confirmed_entry_reachable), `:/MainView.qml:798` (confirmed_entry_reachable) |
| `searchModeActive` | property_read | 4 | `:/MainView.qml:192` (confirmed_entry_reachable), `:/MainView.qml:242` (confirmed_entry_reachable), `:/MainView.qml:702` (confirmed_entry_reachable), `:/MainView.qml:746` (confirmed_entry_reachable) |
| `searchModeActive` | property_write | 2 | `:/MainView.qml:242` (confirmed_entry_reachable), `:/MainView.qml:746` (confirmed_entry_reachable) |
| `sendMessage` | method_call | 1 | `:/MainView.qml:390` (confirmed_entry_reachable) |

## `device_list_HL`

| member | access | count | locations |
|---|---|---:|---|
| `deviceDisconnection` | signal_subscription | 1 | `:/Component/SwitchView.qml:109` (not_entry_reachable) |
| `deviceListChanged` | signal_subscription | 1 | `:/TrayIconMenu.qml:248` (confirmed_entry_reachable) |
| `deviceModel.get` | method_call | 9 | `:/Component/WindowTitleBar.qml:44` (not_entry_reachable), `:/TrayIconMenu.qml:238` (confirmed_entry_reachable), `:/TrayIconMenu.qml:239` (confirmed_entry_reachable), `:/TrayIconMenu.qml:240` (confirmed_entry_reachable), `:/TrayIconMenu.qml:241` (confirmed_entry_reachable), `:/TrayIconMenu.qml:255` (confirmed_entry_reachable), `:/TrayIconMenu.qml:256` (confirmed_entry_reachable), `:/TrayIconMenu.qml:257` (confirmed_entry_reachable), `:/TrayIconMenu.qml:258` (confirmed_entry_reachable) |
| `deviceModel.rowCount` | method_call | 5 | `:/Component/WindowTitleBar.qml:37` (not_entry_reachable), `:/Component/WindowTitleBar.qml:42` (not_entry_reachable), `:/TrayIconMenu.qml:11` (confirmed_entry_reachable), `:/TrayIconMenu.qml:234` (confirmed_entry_reachable), `:/TrayIconMenu.qml:251` (confirmed_entry_reachable) |
| `reCurConfigure` | method_call | 1 | `:/TrayIconMenu.qml:200` (confirmed_entry_reachable) |

## `window`

| member | access | count | locations |
|---|---|---:|---|
| `agreePolicy` | property_read | 4 | `:/firstDiaLog.qml:160` (confirmed_entry_reachable), `:/firstDiaLog.qml:163` (confirmed_entry_reachable), `:/firstDiaLog.qml:264` (confirmed_entry_reachable), `:/firstDiaLog.qml:293` (confirmed_entry_reachable) |
| `agreePolicy` | property_write | 1 | `:/firstDiaLog.qml:163` (confirmed_entry_reachable) |
| `allowQuestion` | property_read | 5 | `:/DialogModule/DialogPanel.qml:356` (confirmed_entry_reachable), `:/MainView.qml:272` (confirmed_entry_reachable), `:/MainView.qml:274` (confirmed_entry_reachable), `:/MainView.qml:338` (confirmed_entry_reachable), `:/MainView.qml:381` (confirmed_entry_reachable) |
| `height` | property_read | 6 | `:/MainView.qml:12` (confirmed_entry_reachable), `:/MainView.qml:401` (confirmed_entry_reachable), `:/MainView.qml:515` (confirmed_entry_reachable), `:/MainView.qml:525` (confirmed_entry_reachable), `:/MainView.qml:555` (confirmed_entry_reachable), `:/MainView.qml:562` (confirmed_entry_reachable) |
| `height` | property_write | 1 | `:/MainView.qml:398` (confirmed_entry_reachable) |
| `newVersionNumber` | property_read | 1 | `:/updateWindow.qml:107` (confirmed_entry_reachable) |
| `notEnough` | property_read | 1 | `:/MainView.qml:273` (confirmed_entry_reachable) |
| `openWebsite` | method_call | 2 | `:/firstDiaLog.qml:191` (confirmed_entry_reachable), `:/firstDiaLog.qml:209` (confirmed_entry_reachable) |
| `radius` | property_read | 1 | `:/Component/SwitchView.qml:69` (not_entry_reachable) |
| `scale` | property_read | 5 | `:/Controls/SelectableText.qml:6` (confirmed_entry_reachable), `:/DialogModule/ChatHistory.qml:8` (confirmed_entry_reachable), `:/DialogModule/DialogPanel.qml:9` (confirmed_entry_reachable), `:/DialogModule/MorePopup.qml:14` (confirmed_entry_reachable), `:/MainView.qml:15` (confirmed_entry_reachable) |
| `setDialog` | method_call | 5 | `:/MainView.qml:112` (confirmed_entry_reachable), `:/MainView.qml:393` (confirmed_entry_reachable), `:/MainView.qml:512` (confirmed_entry_reachable), `:/MainView.qml:522` (confirmed_entry_reachable), `:/MainView.qml:560` (confirmed_entry_reachable) |
| `setFirstStart` | method_call | 1 | `:/firstDiaLog.qml:295` (confirmed_entry_reachable) |
| `setHistory` | method_call | 3 | `:/MainView.qml:511` (confirmed_entry_reachable), `:/MainView.qml:521` (confirmed_entry_reachable), `:/MainView.qml:552` (confirmed_entry_reachable) |
| `setXandY` | method_call | 10 | `:/MainView.qml:110` (confirmed_entry_reachable), `:/MainView.qml:290` (confirmed_entry_reachable), `:/MainView.qml:295` (confirmed_entry_reachable), `:/MainView.qml:303` (confirmed_entry_reachable), `:/MainView.qml:308` (confirmed_entry_reachable), `:/MainView.qml:401` (confirmed_entry_reachable), `:/MainView.qml:515` (confirmed_entry_reachable), `:/MainView.qml:525` (confirmed_entry_reachable), `:/MainView.qml:555` (confirmed_entry_reachable), `:/MainView.qml:562` (confirmed_entry_reachable) |
| `showGuideWindow` | method_call | 2 | `:/updateWindow.qml:149` (confirmed_entry_reachable), `:/updateWindow.qml:205` (confirmed_entry_reachable) |
| `skinMode` | property_write | 2 | `:/Component/WindowTitleBar.qml:211` (not_entry_reachable), `:/Component/WindowTitleBar.qml:214` (not_entry_reachable) |
| `sysSkinType` | property_read | 1 | `:/Controls/CustomToolTip.qml:20` (not_entry_reachable) |
| `topScreen` | property_read | 1 | `:/MainView.qml:614` (confirmed_entry_reachable) |
| `topScreen` | property_write | 1 | `:/MainView.qml:614` (confirmed_entry_reachable) |
| `updateSoft` | method_call | 1 | `:/updateWindow.qml:229` (confirmed_entry_reachable) |
| `width` | property_read | 1 | `:/MainView.qml:11` (confirmed_entry_reachable) |
| `x` | property_read | 10 | `:/MainView.qml:110` (confirmed_entry_reachable), `:/MainView.qml:290` (confirmed_entry_reachable), `:/MainView.qml:295` (confirmed_entry_reachable), `:/MainView.qml:303` (confirmed_entry_reachable), `:/MainView.qml:308` (confirmed_entry_reachable), `:/MainView.qml:401` (confirmed_entry_reachable), `:/MainView.qml:515` (confirmed_entry_reachable), `:/MainView.qml:525` (confirmed_entry_reachable), `:/MainView.qml:555` (confirmed_entry_reachable), `:/MainView.qml:562` (confirmed_entry_reachable) |
| `y` | property_read | 10 | `:/MainView.qml:110` (confirmed_entry_reachable), `:/MainView.qml:290` (confirmed_entry_reachable), `:/MainView.qml:295` (confirmed_entry_reachable), `:/MainView.qml:303` (confirmed_entry_reachable), `:/MainView.qml:308` (confirmed_entry_reachable), `:/MainView.qml:401` (confirmed_entry_reachable), `:/MainView.qml:515` (confirmed_entry_reachable), `:/MainView.qml:525` (confirmed_entry_reachable), `:/MainView.qml:555` (confirmed_entry_reachable), `:/MainView.qml:562` (confirmed_entry_reachable) |

## `setting`

| member | access | count | locations |
|---|---|---:|---|
| `checkUpdate` | method_call | 2 | `:/SettingsPage.qml:306` (confirmed_entry_reachable), `:/TrayIconMenu.qml:103` (confirmed_entry_reachable) |
| `currentVersion` | property_read | 1 | `:/SettingsPage.qml:257` (confirmed_entry_reachable) |
| `setStartUpAuto` | method_call | 1 | `:/SettingsPage.qml:428` (confirmed_entry_reachable) |
| `startUpAuto` | property_read | 1 | `:/SettingsPage.qml:423` (confirmed_entry_reachable) |
| `startUpAuto` | property_write | 1 | `:/SettingsPage.qml:427` (confirmed_entry_reachable) |
| `totalCount` | property_read | 2 | `:/SettingsPage.qml:353` (confirmed_entry_reachable), `:/SettingsPage.qml:369` (confirmed_entry_reachable) |
| `updated` | property_read | 3 | `:/SettingsPage.qml:78` (confirmed_entry_reachable), `:/SettingsPage.qml:280` (confirmed_entry_reachable), `:/TrayIconMenu.qml:78` (confirmed_entry_reachable) |
| `usedCount` | property_read | 1 | `:/SettingsPage.qml:353` (confirmed_entry_reachable) |
| `usedNumCount` | property_read | 1 | `:/SettingsPage.qml:382` (confirmed_entry_reachable) |

## `themeManager`

| member | access | count | locations |
|---|---|---:|---|
| `currentTheme` | property_read | 3 | `:/Component/WindowTitleBar.qml:204` (not_entry_reachable), `:/Component/WindowTitleBar.qml:209` (not_entry_reachable), `:/Component/WindowTitleBar.qml:212` (not_entry_reachable) |
| `currentTheme` | property_write | 2 | `:/Component/WindowTitleBar.qml:210` (not_entry_reachable), `:/Component/WindowTitleBar.qml:213` (not_entry_reachable) |

## `configure_Manager`

| member | access | count | locations |
|---|---|---:|---|
| `deleteDevice` | method_call | 1 | `:/Component/WindowTitleBar.qml:38` (not_entry_reachable) |
| `initDevice` | method_call | 1 | `:/Component/WindowTitleBar.qml:45` (not_entry_reachable) |
| `keyboardName` | property_read | 2 | `:/Component/WindowTitleBar.qml:100` (not_entry_reachable), `:/Component/WindowTitleBar.qml:102` (not_entry_reachable) |

## `config_set_HL`

| member | access | count | locations |
|---|---|---:|---|
| `getRemoteDriverUpdateLog` | method_call | 1 | `:/Component/WindowTitleBar.qml:191` (not_entry_reachable) |
| `updated` | property_read | 1 | `:/Component/WindowTitleBar.qml:141` (not_entry_reachable) |

## `lang_HL`

| member | access | count | locations |
|---|---|---:|---|
| `optLanguageName` | property_read | 1 | `:/Component/WindowTitleBar.qml:53` (not_entry_reachable) |

## `pairmanager`

| member | access | count | locations |
|---|---|---:|---|
| `init` | method_call | 1 | `:/Component/WindowTitleBar.qml:235` (not_entry_reachable) |

## `kb_travelDistance_manager`

| member | access | count | locations |
|---|---|---:|---|
| `adapCalibration` | property_read | 1 | `:/Component/WindowTitleBar.qml:322` (not_entry_reachable) |
| `exitCalibOrDemoMode` | method_call | 1 | `:/Component/WindowTitleBar.qml:320` (not_entry_reachable) |
| `physicalTravel` | property_read | 4 | `:/Controls/CustomSensitivitySlider1.qml:24` (not_entry_reachable), `:/Controls/CustomSensitivitySlider1.qml:73` (not_entry_reachable), `:/Controls/CustomSensitivitySlider1_New.qml:26` (not_entry_reachable), `:/Controls/CustomSensitivitySlider1_New.qml:59` (not_entry_reachable) |
| `setWasdShake` | method_call | 1 | `:/Component/WindowTitleBar.qml:323` (not_entry_reachable) |

## Requested names not present literally

- `SettingsPageManager`: not referenced by type name; `setting` is the inferred instance/API surface.
- `ViewHandler`: not referenced by type name; `window` is the inferred instance/API surface.
- `recordDataModel`: absent.
- `chatHistoryModel`: absent; recovered QML instead uses `dialog_manager.dialogModel` and `dialog_manager.dialogInfoModel`.
