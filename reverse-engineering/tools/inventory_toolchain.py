#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Inventory local Qt/QML and C++ tools without modifying the machine."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

import pefile


TOOLS = [
    "qmake",
    "qmlscene",
    "qmlimportscanner",
    "qmlcachegen",
    "cmake",
    "g++",
    "mingw32-make",
    "ninja",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inventory the local Qt 5.15.2 toolchain.")
    parser.add_argument("--workspace", required=True)
    parser.add_argument("--app-dir", required=True)
    parser.add_argument("--output", required=True)
    return parser.parse_args()


def deduplicate(paths: Iterable[Path]) -> List[Path]:
    seen = set()
    result: List[Path] = []
    for path in paths:
        try:
            resolved = path.resolve()
        except OSError:
            continue
        key = str(resolved).casefold()
        if key not in seen and resolved.is_file():
            seen.add(key)
            result.append(resolved)
    return result


def candidates_for(name: str, workspace: Path) -> List[Path]:
    exe = name + ".exe"
    result: List[Path] = []
    on_path = shutil.which(name) or shutil.which(exe)
    if on_path:
        result.append(Path(on_path))
    local_app = Path(os.environ.get("LOCALAPPDATA", "C:/Users/worker/AppData/Local"))
    roots = [
        Path("C:/Qt"),
        Path("C:/Program Files/Qt"),
        Path("C:/Program Files (x86)/Qt"),
        Path("C:/msys64"),
        Path("C:/mingw32"),
        Path("C:/mingw64"),
        local_app / "Programs" / "Qt",
        workspace / "reconstruction_analysis" / "_toolchain",
    ]
    for root in roots:
        if root.is_dir():
            result.extend(root.rglob(exe))
    if name == "cmake":
        vs_root = Path("C:/Program Files/Microsoft Visual Studio")
        if vs_root.is_dir():
            result.extend(vs_root.glob("*/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"))
            result.extend(vs_root.glob("*/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"))
    return deduplicate(result)


def run_version(path: Path) -> Dict[str, Any]:
    attempts = [[str(path), "--version"], [str(path), "-version"], [str(path), "-v"]]
    for command in attempts:
        try:
            completed = subprocess.run(
                command, check=False, capture_output=True, text=True, timeout=10
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        output = (completed.stdout + "\n" + completed.stderr).strip()
        if output:
            return {
                "command": command[1:],
                "exit_code": completed.returncode,
                "output": output[:4000],
            }
    return {"command": None, "exit_code": None, "output": None}


def pe_versions(path: Path) -> Dict[str, Optional[str]]:
    result: Dict[str, Optional[str]] = {"file_version": None, "product_version": None}
    try:
        pe = pefile.PE(str(path), fast_load=False)
        for group in getattr(pe, "FileInfo", []) or []:
            for entry in group:
                if getattr(entry, "Key", b"") != b"StringFileInfo":
                    continue
                for table in entry.StringTable:
                    values = {
                        key.decode(errors="replace").lower(): value.decode(errors="replace")
                        for key, value in table.entries.items()
                    }
                    result["file_version"] = values.get("fileversion", result["file_version"])
                    result["product_version"] = values.get("productversion", result["product_version"])
    except Exception:
        pass
    return result


def find_development_artifacts(workspace: Path) -> Dict[str, List[str]]:
    roots = [
        Path("C:/Qt"),
        Path("C:/Program Files/Qt"),
        Path("C:/Program Files (x86)/Qt"),
        Path("C:/msys64"),
        workspace / "reconstruction_analysis" / "_toolchain",
    ]
    headers: List[Path] = []
    qt_import_libs: List[Path] = []
    mingw_runtime_import_libs: List[Path] = []
    for root in roots:
        if not root.is_dir():
            continue
        headers.extend(root.rglob("qglobal.h"))
        qt_import_libs.extend(root.rglob("libQt5Core.a"))
        qt_import_libs.extend(root.rglob("Qt5Core.lib"))
        mingw_runtime_import_libs.extend(root.rglob("libgcc.a"))
    return {
        "qt_headers_qglobal_h": [str(path) for path in deduplicate(headers)],
        "qt_core_import_libraries": [str(path) for path in deduplicate(qt_import_libs)],
        "mingw_import_libraries": [str(path) for path in deduplicate(mingw_runtime_import_libs)],
    }


def main() -> int:
    args = parse_args()
    workspace = Path(args.workspace).resolve()
    app_dir = Path(args.app_dir).resolve()
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    tools: Dict[str, Any] = {}
    for name in TOOLS:
        candidates = candidates_for(name, workspace)
        tools[name] = {
            "available": bool(candidates),
            "on_path": bool(shutil.which(name) or shutil.which(name + ".exe")),
            "candidates": [
                {"path": str(path), "version_probe": run_version(path)} for path in candidates
            ],
        }

    runtime_dlls: List[Dict[str, Any]] = []
    for path in sorted(app_dir.glob("Qt5*.dll"), key=lambda p: p.name.casefold()):
        runtime_dlls.append(
            {"name": path.name, "path": str(path), "size": path.stat().st_size, **pe_versions(path)}
        )
    qml_modules = sorted(
        str(path.parent.relative_to(app_dir)).replace("\\", "/")
        for path in app_dir.rglob("qmldir")
    )
    app_archives = [
        {"name": path.name, "path": str(path), "size": path.stat().st_size}
        for path in sorted(app_dir.glob("*.a"))
    ]
    dev = find_development_artifacts(workspace)
    qt_qml_tools = ["qmlscene", "qmlcachegen"]
    engine_validator = next(
        (name for name in qt_qml_tools if tools[name]["available"]), None
    )
    engine_available = engine_validator is not None and bool(dev["qt_headers_qglobal_h"] or tools["qmake"]["available"])

    inventory = {
        "status": "ok",
        "target": {"qt_version": "5.15.2", "architecture": "x86", "compiler": "MinGW"},
        "workspace": str(workspace),
        "application_runtime_directory": str(app_dir),
        "search_scope": [
            "PATH",
            "C:/Qt",
            "C:/Program Files/Qt",
            "C:/Program Files (x86)/Qt",
            "C:/msys64",
            "C:/mingw32",
            "C:/mingw64",
            "%LOCALAPPDATA%/Programs/Qt",
            "reconstruction_analysis/_toolchain",
            "Visual Studio bundled CMake path",
        ],
        "tools": tools,
        "development_artifacts": dev,
        "application_runtime": {
            "qt_dlls": runtime_dlls,
            "qml_module_count": len(qml_modules),
            "qml_modules": qml_modules,
            "static_or_import_archives": app_archives,
        },
        "qml_engine_validation": {
            "available": engine_available,
            "validator": engine_validator,
            "mode": "engine" if engine_available else "static_only",
            "reason": None
            if engine_available
            else (
                "Qt 5.15.2 runtime DLLs and QML modules are present, but no qmlscene/qmlcachegen, "
                "qmake, Qt headers, or Qt import libraries were found; the application executable "
                "is not a controlled per-file validation harness."
            ),
        },
        "project_local_toolchain_plan": {
            "install_root": str((output.parent / "_toolchain" / "Qt").resolve()),
            "required_kit": "Qt 5.15.2 MinGW 8.1.0 32-bit (mingw81_32)",
            "required_components": [
                "Qt 5.15.2 mingw81_32 bin/include/lib/qml",
                "Qt Tools mingw810_32 compiler",
            ],
            "usage": [
                "Do not add the kit globally to PATH; invoke its binaries by absolute path.",
                "Set QML2_IMPORT_PATH to the application runtime directory for its deployed QtQuick modules.",
                "Use qmlcachegen for parser/compile checks and an isolated qmlscene harness only after backend mocks exist.",
            ],
        },
    }
    output.write_text(json.dumps(inventory, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[inventory] qml_validation_mode={inventory['qml_engine_validation']['mode']}")
    print(f"[inventory] output={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
