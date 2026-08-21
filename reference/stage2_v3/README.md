# Stage 2 v3：Qt 5 RCC v3 真实资源树恢复

本阶段不做 ZIP 或全局字符串扫描。它从 `qRegisterResourceData` / `qUnregisterResourceData` 的 x86 调用点开始，解析 MinGW 生成的 `mov dword ptr [esp+offset], immediate` 参数，再依据精确的 tree/names/data VA 恢复 Qt RCC 目录树。

## 一键复现

在 PowerShell 中从本目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\commands.ps1
```

`commands.ps1` 会仅清空并重建 `stage2_v3/recovered_qrc_tree` 和 `stage2_v3/verification`，然后按顺序运行：

1. `tools/locate_qresource_calls.py`
2. `tools/extract_qt_rcc_arrays.py`
3. `tools/verify_qrc_tree.py`

三步均成功时总退出码为 `0`。Python 需要 v2 阶段已使用的 `pefile` 和 `capstone`。本样本没有 zstd 节点；提取器仍识别 `CompressedZstd`，若实际遇到该标志，需要 Python `zstandard` 包或带 `compression.zstd` 的 Python。

## 输出

- `qresource_call_candidates_v3.json`：注册/注销调用、基本块栈槽写入、参数 VA/section/raw offset，bundle 去重和重复注册标记。
- `authoritative_qrc_manifest_v3.json`：三个 bundle 的解析结果、目录节点、260 个文件节点、精确偏移、flags/locale/last_modified、压缩与解压 SHA-256，以及 v2 对齐状态。
- `recovered_qrc_tree/`：按真实 `:/...` 路径写出的资源。Qt 的根节点 0 本身没有路径名，因此不会伪造额外的顶层目录。
- `verification/qrc_tree_verification.json`：从空的 fresh tree 复跑后，对 bundle、节点、偏移、manifest 字段和所有输出文件哈希的比较。

## Qt 5.15 格式依据

实现直接依据 Qt 5.15 官方源码：

- [qresource.cpp（Qt 5.15）](https://github.com/qt/qtbase/blob/5.15/src/corelib/io/qresource.cpp)：`QResourceRoot::findOffset/name/hash/findNode` 以及资源 data/children/lastModified 访问规则。其中 node 0 的 `name()` 显式返回空字符串。
- [rcc.cpp（Qt 5.15）](https://github.com/qt/qtbase/blob/5.15/src/tools/rcc/rcc.cpp)：`RCCFileInfo::writeDataInfo` 和 `writeDataBlob`。

v3 节点固定为 22 bytes，数值均为大端：

- `+0` name offset（u32），`+4` flags（u16）。
- 目录（flag `0x02`）：`+6` child count（u32），`+10` first child index（u32）。
- 文件：`+6` country（u16），`+8` language（u16），`+10` data offset（u32）。
- `+14` last modified（u64，毫秒 Unix epoch）。
- names 条目：u540d称长度 u16、Qt hash u32、UTF-16BE code units。
- data 条目：u5b58储长度 u32，紧随u5177体 payload。zlib flag `0x01` 的 payload 是 qCompress 格式（解压长度 u32 + 标准 zlib stream）；zstd flag 为 `0x04`。

解析器会拒绝 PE raw 越界、非法 name/data offset、未知或互斥 flags、目录子节点越界、树环/重复引用/节点空洞、非法 UTF-16BE、路径穿越、压缩尺寸不符和截断/带尾数据的 zlib 流。

## v2 对齐规则

v2 zlib 候选的“压缩哈希 + 解压哈希”均一致，且 v2 `claimed_path` 与 RCC 恢复的真实路径一致时，记录才会标为 `verified`。仅哈希一致但旧路径猜测不同的记录为 `rcc_valid_v2_hash_match_path_mismatch`，不算 verified。
