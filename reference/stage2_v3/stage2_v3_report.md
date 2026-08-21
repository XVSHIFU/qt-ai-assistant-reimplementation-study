# SmartKey AI 逆向阶段 2 v3 报告

## 结论

Qt RCC v3 真实树恢复成功。从 10 个调用点识别出 6 次注册、4 次注销；6 次注册是 3 个唯一数组 bundle 各两次。三个 bundle 全部通过边界、树结构、压缩和哈希验证，共恢复 17 个目录节点和 260 个文件节点。

## 调用与 bundle

| bundle | tree / names / data VA | register calls | unregister calls | 节点（目录+文件） | 结果 |
|---|---|---|---|---:|---|
| `bundle_000` | `0x470060 / 0x470140 / 0x470280` | `0x401612`, `0x460702` | `0x4015e2`, `0x401642` | 9（3+6） | success |
| `bundle_001` | `0x470820 / 0x470aa0 / 0x470ea0` | `0x401672`, `0x460742` | `0x4016a9` | 28（5+23） | success |
| `bundle_002` | `0x47c260 / 0x47d700 / 0x47ff80` | `0x4016f2`, `0x460782` | `0x401729` | 240（9+231） | success |

`0x460702 / 0x460742 / 0x460782` 是各 bundle 的重复注册包装器，不是新数组。两个注销点的 version 由 `eax` 运行时形成，但 tree/names/data 三个数组 VA 仍完整且与对应 bundle 一致；提取仅使用 version=3 的静态 register 调用。

## 核心 QML 查找

| 查询 | RCC 真实路径 | 解压 SHA-256 | v2 结果 |
|---|---|---|---|
| `MainView.qml` | `:/MainView.qml` | `794ee9881ce5dd352381ec2b26c7cb6f6b91990ddf98fa00a6810d209292f1a8` | 哈希对应 v2 的 `qrc:/QAndAPage.qml` 猜测，路径不同，未 verified |
| `QAndAPage.qml` | 未找到 | — | v2 猜测被 RCC 树否定 |
| `SettingsPage.qml` | `:/SettingsPage.qml` | `e8979d4de135b908f82f2ff151ff6f39f343a4659a03ab24c8b6778e33eed822` | verified |
| `TrayIconMenu.qml` | `:/TrayIconMenu.qml` | `dc32c1aa2a5974e66c742130db7911475ee87c661ad4286638e03190e9071bce` | verified |
| `GuideWindow.qml` | `:/GuideWindow.qml` | `9d349090cb767e43e1ae818fa40175bc1d36ddcabd4e360659ed16408255d94f` | verified |
| `updateWindow.qml` | `:/updateWindow.qml` | `4edafa49f1155cf00eb762d1e6f1768fc719f4f413b4433c81aa04170b42798f` | verified |
| `firstDiaLog.qml` | `:/firstDiaLog.qml` | `2e4844ac87e21dc7e1091e30bb5a61491fdc54eb70814ce4f46eecfb349a6c73` | verified |
| `Views/AboutusPage.qml` | 未找到 | — | 未 verified |

其他重要纠正：v2 的 `qrc:/Styles/FontManager.qml` 哈希实际属于 `:/FontManager.qml`；其 `qrc:/Theme/LightTheme.qml` 哈希实际属于 `:/Theme/DarkTheme.qml`。这些都只记为 hash-match/path-mismatch，未标 verified。

## 复跑验证

`commands.ps1` 从空的 `recovered_qrc_tree` 开始生成 baseline，验证器再从空的 `verification/fresh_tree` 生成 fresh 副本。实际结果：

```text
[qresource] calls=10 register=6 unregister=4 bundles=3
[rcc] bundles=3 success=3 failed=0 files=260 directories=17
[rcc] v2_path_and_hash_verified=5
[verify] status=PASS records=260 record_mismatches=0 file_failures=0
exit code: 0
```

验证范围包括 EXE SHA-256、calls JSON SHA-256、bundle 参数/偏移/计数、全部文件节点字段、目录集合、核心 QML 结果，以及 baseline/fresh 共 520 个输出文件的实际 SHA-256 和尺寸。
