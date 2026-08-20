#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Classify recovered resources and perform reproducible static QML analysis."""

from __future__ import annotations

import argparse
import bisect
import csv
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterable, List, Optional, Sequence, Set, Tuple

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_INVALID


ASSET_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".svg", ".ico", ".icns", ".ttf", ".otf", ".qml", ".gif", ".bmp"
}
BUILTIN_QML_TYPES = {
    "ApplicationWindow", "Behavior", "Button", "ButtonGroup", "Canvas", "CheckBox", "Column",
    "ColumnLayout", "ComboBox", "Component", "Connections", "Desaturate", "DropShadow", "FastBlur",
    "Flickable", "Flow", "FocusScope", "FontLoader", "Glow", "Gradient", "GradientStop", "Grid",
    "GridLayout", "Image", "Item", "Keys", "ListElement", "ListModel", "ListView", "Loader", "MaskShape",
    "MouseArea", "NumberAnimation", "OpacityMask", "ParallelAnimation", "PauseAnimation", "Popup",
    "OpacityAnimator", "PropertyAnimation", "QtObject", "RadioButton", "Rectangle", "Repeater", "RotationAnimation", "Row", "RowLayout",
    "ScaleAnimator", "ScrollBar", "ScrollView", "SequentialAnimation", "ShaderEffect", "ShaderEffectSource",
    "Slider", "StackLayout", "State", "Text", "TextArea", "TextEdit", "TextField", "Timer", "ToolTip",
    "Transition", "Window"
}
BACKEND_OBJECTS = {
    "manager": "tray/menu controller",
    "dialog_manager": "AI chat/dialog controller",
    "device_list_HL": "connected Rapoo device list controller",
    "window": "C++ window/ViewHandler-facing object",
    "setting": "settings/update controller; likely SettingsPageManager instance",
    "themeManager": "theme state controller",
    "configure_Manager": "device configuration controller",
    "config_set_HL": "configuration/update controller",
    "lang_HL": "language controller",
    "pairmanager": "device pairing controller",
    "kb_travelDistance_manager": "keyboard travel/calibration controller",
    "recordDataModel": "requested legacy/candidate model name",
    "chatHistoryModel": "requested legacy/candidate model name",
    "SettingsPageManager": "requested C++ type/concept name",
    "ViewHandler": "requested C++ type/concept name",
}
TOP_LEVEL_NAMES = [
    "MainView.qml", "firstDiaLog.qml", "updateWindow.qml", "GuideWindow.qml", "SettingsPage.qml", "TrayIconMenu.qml"
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze the authoritative stage2_v3 resource tree.")
    parser.add_argument("--resource-root", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--inventory", required=True)
    parser.add_argument("--input-exe", required=True)
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def json_write(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def line_offsets(text: str) -> List[int]:
    result = [0]
    result.extend(index + 1 for index, char in enumerate(text) if char == "\n")
    return result


def line_number(offsets: Sequence[int], offset: int) -> int:
    return bisect.bisect_right(offsets, offset)


def detect_and_decode(raw: bytes) -> Tuple[str, str, Optional[str]]:
    if raw.startswith(b"\xef\xbb\xbf"):
        encoding = "utf-8-sig"
    elif raw.startswith(b"\xff\xfe"):
        encoding = "utf-16-le"
    elif raw.startswith(b"\xfe\xff"):
        encoding = "utf-16-be"
    else:
        encoding = "utf-8"
    try:
        return raw.decode(encoding), encoding, None
    except UnicodeDecodeError as exc:
        return "", encoding, f"{type(exc).__name__}: {exc}"


def lexical_scan(text: str) -> Dict[str, Any]:
    masked = list(text)
    comment_mask = [False] * len(text)
    offsets = line_offsets(text)
    stack: List[Tuple[str, int]] = []
    errors: List[Dict[str, Any]] = []
    strings: List[Dict[str, Any]] = []
    pairs = {"}": "{", "]": "[", ")": "("}
    state = "normal"
    quote = ""
    start = 0
    i = 0
    escaped = False
    while i < len(text):
        char = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "normal":
            if char == "/" and nxt == "/":
                state = "line_comment"
                comment_mask[i] = comment_mask[i + 1] = True
                masked[i] = masked[i + 1] = " "
                i += 2
                continue
            if char == "/" and nxt == "*":
                state = "block_comment"
                start = i
                comment_mask[i] = comment_mask[i + 1] = True
                masked[i] = masked[i + 1] = " "
                i += 2
                continue
            if char in {'"', "'", "`"}:
                state = "string"
                quote = char
                start = i
                escaped = False
                masked[i] = " "
                i += 1
                continue
            if char in "{[(":
                stack.append((char, i))
            elif char in "}])":
                if not stack or stack[-1][0] != pairs[char]:
                    errors.append({"kind": "unexpected_closer", "character": char, "line": line_number(offsets, i)})
                else:
                    stack.pop()
            i += 1
            continue
        if state == "line_comment":
            comment_mask[i] = True
            if char == "\n":
                state = "normal"
            else:
                masked[i] = " "
            i += 1
            continue
        if state == "block_comment":
            comment_mask[i] = True
            if char == "*" and nxt == "/":
                comment_mask[i + 1] = True
                masked[i] = masked[i + 1] = " "
                state = "normal"
                i += 2
            else:
                if char != "\n":
                    masked[i] = " "
                i += 1
            continue
        if state == "string":
            if char != "\n":
                masked[i] = " "
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                strings.append(
                    {"value": text[start + 1 : i], "start": start, "end": i + 1, "line": line_number(offsets, start)}
                )
                state = "normal"
            elif char == "\n" and quote != "`":
                errors.append({"kind": "unclosed_string_before_newline", "line": line_number(offsets, start)})
                state = "normal"
            i += 1
    if state == "block_comment":
        errors.append({"kind": "unclosed_block_comment", "line": line_number(offsets, start)})
    elif state == "string":
        errors.append({"kind": "unclosed_string_at_eof", "line": line_number(offsets, start)})
    for opener, position in stack:
        errors.append({"kind": "unclosed_delimiter", "character": opener, "line": line_number(offsets, position)})
    return {
        "masked": "".join(masked),
        "comment_mask": comment_mask,
        "strings": strings,
        "errors": errors,
        "balanced": not errors,
    }


IMPORT_RE = re.compile(
    r'^\s*import\s+(?:"(?P<quoted>[^"]+)"|(?P<module>[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))'
    r'(?:\s+(?P<version>\d+(?:\.\d+)?))?(?:\s+as\s+(?P<alias>[A-Za-z_]\w*))?', re.M
)


def normalize_qrc(value: str) -> str:
    if value.startswith("qrc:/"):
        return ":/" + value[5:]
    if value.startswith(":/"):
        return value
    return value


def qrc_join(base_dir: str, value: str) -> str:
    if value.startswith("qrc:/") or value.startswith(":/"):
        return normalize_qrc(value).rstrip("/")
    base = PurePosixPath(base_dir[2:] if base_dir.startswith(":/") else base_dir)
    parts: List[str] = []
    for part in (base / value).parts:
        if part in {"", "."}:
            continue
        if part == "..":
            if parts:
                parts.pop()
        else:
            parts.append(part)
    return ":/" + "/".join(parts).rstrip("/")


def parent_qrc(path: str) -> str:
    parent = str(PurePosixPath(path[2:]).parent)
    return ":/" if parent == "." else ":/" + parent


def directory_set(resource_paths: Iterable[str]) -> Set[str]:
    result = {":/"}
    for resource_path in resource_paths:
        parts = PurePosixPath(resource_path[2:]).parts
        for count in range(1, len(parts)):
            result.add(":/" + "/".join(parts[:count]))
    return result


def runtime_module_status(module: str, runtime_modules: Set[str]) -> Tuple[str, Optional[str]]:
    mapping = {
        "QtQuick": "QtQuick.2",
        "QtQuick.Controls": "QtQuick/Controls.2",
        "QtQuick.Window": "QtQuick/Window.2",
        "QtQuick.Layouts": "QtQuick/Layouts",
        "QtGraphicalEffects": "QtGraphicalEffects",
    }
    expected = mapping.get(module)
    if expected and expected in runtime_modules:
        return "runtime_module_present_engine_unchecked", expected
    return "runtime_module_not_confirmed", expected


def classify_resource(record: Dict[str, Any]) -> Tuple[str, str, List[str]]:
    path = str(record["resource_path"])
    extension = PurePosixPath(path).suffix.lower()
    bundle = record.get("bundle_id")
    if bundle == "bundle_000" or path.startswith(":/GraphicalEffects/"):
        return (
            "Qt standard GraphicalEffects",
            "bundle_and_path",
            ["RCC bundle_000 contains the deployed GraphicalEffects wrappers", f"resource_path={path}"],
        )
    if extension != ".qml":
        return (
            "font/image asset",
            "file_type_and_path",
            [f"extension={extension or '(none)'}", "non-QML payload under recovered Font/Image asset bundle"],
        )
    device_qml = {
        ":/GuideWindow.qml", ":/TrayIconMenu.qml", ":/Component/SwitchView.qml",
        ":/Component/WindowTitleBar.qml", ":/Controls/CustomSensitivitySlider1.qml",
        ":/Controls/CustomSensitivitySlider1_New.qml",
    }
    if path in device_qml:
        return (
            "Rapoo device/shared",
            "known_device_context_usage",
            ["references device_list_HL/configure_Manager/kb_travelDistance_manager or device UI flow", f"resource_path={path}"],
        )
    if bundle == "bundle_001":
        return (
            "SmartKey AI core",
            "authoritative_qml_bundle_and_role",
            ["QML belongs to RCC bundle_001", "used by AI chat/settings/dialog/theme/control flow"],
        )
    return "unknown", "fallback", ["no stronger classification rule matched"]


def extract_imports(
    text: str,
    resource_path: str,
    resources: Set[str],
    directories: Set[str],
    runtime_modules: Set[str],
) -> List[Dict[str, Any]]:
    offsets = line_offsets(text)
    result: List[Dict[str, Any]] = []
    base = parent_qrc(resource_path)
    for match in IMPORT_RE.finditer(text):
        quoted = match.group("quoted")
        if quoted is not None:
            target = qrc_join(base, quoted)
            status = "exists_directory" if target in directories else "missing_directory"
            result.append(
                {
                    "kind": "local_directory",
                    "raw": quoted,
                    "target": target,
                    "line": line_number(offsets, match.start()),
                    "alias": match.group("alias"),
                    "status": status,
                }
            )
        else:
            module = match.group("module")
            status, deployed = runtime_module_status(module, runtime_modules)
            result.append(
                {
                    "kind": "qt_module",
                    "module": module,
                    "version": match.group("version"),
                    "alias": match.group("alias"),
                    "line": line_number(offsets, match.start()),
                    "deployed_module_directory": deployed,
                    "status": status,
                }
            )
    return result


def reference_status(value: str, resource_path: str, resources: Set[str], directories: Set[str], dynamic: bool) -> Dict[str, Any]:
    target = qrc_join(parent_qrc(resource_path), value)
    if dynamic:
        exists = target in directories or any(path.startswith(target + "/") for path in resources)
        return {
            "resolved_target": target,
            "status": "dynamic_prefix_exists_unresolved" if exists else "dynamic_prefix_missing_unresolved",
            "exists": exists,
            "resolvable": False,
        }
    exists = target in resources or target in directories
    return {
        "resolved_target": target,
        "status": "exists" if target in resources else "exists_directory" if target in directories else "missing",
        "exists": exists,
        "resolvable": True,
    }


def extract_resource_references(
    text: str,
    scan: Dict[str, Any],
    resource_path: str,
    resources: Set[str],
    directories: Set[str],
) -> List[Dict[str, Any]]:
    refs: List[Dict[str, Any]] = []
    lines = text.splitlines()
    offsets = line_offsets(text)
    active_positions: Set[int] = set()
    for item in scan["strings"]:
        value = item["value"]
        line_text = lines[item["line"] - 1] if item["line"] <= len(lines) else ""
        is_qrc = value.startswith(("qrc:/", ":/"))
        suffix = PurePosixPath(value).suffix.lower()
        column = max(0, item["start"] - offsets[item["line"] - 1])
        before_literal = line_text[:column]
        source_assignment = bool(
            re.search(r"\bsource\s*:", before_literal)
            and not re.search(r"(?:===|!==|==|!=)\s*$", before_literal)
        )
        relative_context = bool(
            suffix in ASSET_EXTENSIONS
            and not re.match(r"^[A-Za-z][A-Za-z0-9+.-]*://", value)
            and (source_assignment or "property url" in line_text or line_text.lstrip().startswith("import "))
        )
        if not is_qrc and not relative_context:
            continue
        dynamic = is_qrc and (value.endswith("/") or "+" in line_text[item["end"] - sum(len(x) + 1 for x in lines[: item["line"] - 1]) :])
        status = reference_status(value, resource_path, resources, directories, dynamic)
        refs.append(
            {
                "source": resource_path,
                "line": item["line"],
                "raw_reference": value,
                "kind": "qrc_url" if is_qrc else "relative_resource",
                "active": True,
                "dynamic": dynamic,
                **status,
            }
        )
        active_positions.add(item["start"])

    quoted = re.compile(r'(["\'])(?P<value>[^"\'\r\n]+)\1')
    for match in quoted.finditer(text):
        if match.start() in active_positions or not scan["comment_mask"][match.start()]:
            continue
        value = match.group("value")
        if not (value.startswith(("qrc:/", ":/")) or PurePosixPath(value).suffix.lower() in ASSET_EXTENSIONS):
            continue
        dynamic = value.startswith(("qrc:/", ":/")) and value.endswith("/")
        status = reference_status(value, resource_path, resources, directories, dynamic)
        refs.append(
            {
                "source": resource_path,
                "line": line_number(offsets, match.start()),
                "raw_reference": value,
                "kind": "commented_reference",
                "active": False,
                "dynamic": dynamic,
                **status,
            }
        )
    return sorted(refs, key=lambda item: (item["line"], item["raw_reference"], not item["active"]))


TYPE_RE = re.compile(r'(?m)(?:^|[:=])\s*([A-Z][A-Za-z0-9_]*)\s*\{')


def local_component_usages(
    masked: str,
    resource_path: str,
    imports: List[Dict[str, Any]],
    qml_by_path: Dict[str, str],
    component_names: Set[str],
) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    offsets = line_offsets(masked)
    search_dirs = [parent_qrc(resource_path)]
    search_dirs.extend(i["target"] for i in imports if i["kind"] == "local_directory" and i["status"] == "exists_directory")
    usages: List[Dict[str, Any]] = []
    unknown: List[Dict[str, Any]] = []
    for match in TYPE_RE.finditer(masked):
        type_name = match.group(1)
        line = line_number(offsets, match.start())
        if type_name in component_names:
            candidates = [d.rstrip("/") + "/" + type_name + ".qml" for d in search_dirs]
            existing = [candidate for candidate in candidates if candidate in qml_by_path]
            usages.append(
                {
                    "source": resource_path,
                    "line": line,
                    "type": type_name,
                    "candidate_targets": candidates,
                    "resolved_targets": existing,
                    "status": "exists" if existing else "missing_local_component",
                }
            )
        elif type_name not in BUILTIN_QML_TYPES:
            unknown.append({"source": resource_path, "line": line, "type": type_name, "status": "external_or_unresolved_type"})
    return usages, unknown


def connections_signals(masked: str, resource_path: str) -> List[Dict[str, Any]]:
    offsets = line_offsets(masked)
    result: List[Dict[str, Any]] = []
    for start_match in re.finditer(r'\bConnections\s*\{', masked):
        open_pos = masked.find("{", start_match.start())
        depth = 0
        end = len(masked)
        for index in range(open_pos, len(masked)):
            if masked[index] == "{":
                depth += 1
            elif masked[index] == "}":
                depth -= 1
                if depth == 0:
                    end = index + 1
                    break
        block = masked[open_pos:end]
        target_match = re.search(r'\btarget\s*:\s*([A-Za-z_]\w*)', block)
        if not target_match:
            continue
        target = target_match.group(1)
        if target not in BACKEND_OBJECTS:
            continue
        for handler in re.finditer(r'\b(?:function\s+)?(on[A-Z][A-Za-z0-9_]*)\s*(?:\([^)]*\))?\s*[:{]', block):
            name = handler.group(1)
            result.append(
                {
                    "object": target,
                    "member_path": name[2:3].lower() + name[3:],
                    "access_kind": "signal_subscription",
                    "handler": name,
                    "file": resource_path,
                    "line": line_number(offsets, open_pos + handler.start()),
                }
            )
    return result


def backend_accesses(masked: str, resource_path: str) -> List[Dict[str, Any]]:
    result: List[Dict[str, Any]] = []
    object_pattern = "|".join(re.escape(name) for name in BACKEND_OBJECTS)
    pattern = re.compile(rf'\b(?P<object>{object_pattern})(?P<chain>(?:\s*\.\s*[A-Za-z_]\w*)+)')
    for number, line in enumerate(masked.splitlines(), 1):
        for match in pattern.finditer(line):
            obj = match.group("object")
            parts = re.findall(r'[A-Za-z_]\w*', match.group("chain"))
            if not parts:
                continue
            member_path = ".".join(parts)
            tail = line[match.end() :]
            if re.match(r'\s*\(', tail):
                kind = "method_call"
            elif re.match(r'\s*=(?!=)', tail):
                kind = "property_write"
            else:
                kind = "property_read"
            result.append(
                {
                    "object": obj,
                    "member_path": member_path,
                    "access_kind": kind,
                    "file": resource_path,
                    "line": number,
                }
            )
    result.extend(connections_signals(masked, resource_path))
    return result


def build_backend_map(accesses: List[Dict[str, Any]]) -> Dict[str, Any]:
    objects: List[Dict[str, Any]] = []
    for name, role in BACKEND_OBJECTS.items():
        occurrences = [entry for entry in accesses if entry["object"] == name]
        grouped: Dict[Tuple[str, str], List[Dict[str, Any]]] = defaultdict(list)
        for entry in occurrences:
            grouped[(entry["member_path"], entry["access_kind"])].append(
                {
                    "file": entry["file"],
                    "line": entry["line"],
                    "source_reachability": entry.get("source_reachability"),
                }
            )
        members = [
            {
                "member_path": key[0],
                "access_kind": key[1],
                "occurrence_count": len(locations),
                "locations": locations,
            }
            for key, locations in sorted(grouped.items())
        ]
        objects.append(
            {
                "object": name,
                "role": role,
                "status": "referenced" if occurrences else "not_referenced_in_recovered_qml",
                "occurrence_count": len(occurrences),
                "required_for_confirmed_entries": any(
                    entry.get("source_reachability") == "confirmed_entry_reachable"
                    for entry in occurrences
                ),
                "required_for_potential_dynamic_entries": any(
                    entry.get("source_reachability") == "potentially_entry_reachable_dynamic"
                    for entry in occurrences
                ),
                "members": members,
            }
        )
    return {
        "status": "ok",
        "method": "static member-chain and Connections handler extraction; no runtime metaobject introspection",
        "objects": objects,
        "requested_name_analysis": {
            "SettingsPageManager": {
                "literal_reference": False,
                "inferred_instance": "setting",
                "basis": "SettingsPage.qml and TrayIconMenu.qml use setting.* for update/startup/account properties",
            },
            "ViewHandler": {
                "literal_reference": False,
                "inferred_instance": "window",
                "basis": "top-level QML calls window.setXandY/setDialog/showGuideWindow/updateSoft and reads window state",
            },
            "recordDataModel": {"literal_reference": False, "replacement_observed": None},
            "chatHistoryModel": {
                "literal_reference": False,
                "replacement_observed": ["dialog_manager.dialogModel", "dialog_manager.dialogInfoModel"],
            },
        },
    }


def compute_reachability(
    qml_paths: Set[str],
    component_usages: List[Dict[str, Any]],
    references: List[Dict[str, Any]],
    binary: Dict[str, Any],
) -> Dict[str, Any]:
    exact_adjacency: Dict[str, Set[str]] = defaultdict(set)
    for usage in component_usages:
        for target in usage.get("resolved_targets", []):
            if target in qml_paths:
                exact_adjacency[usage["source"]].add(target)
    for reference in references:
        target = reference.get("resolved_target")
        if (
            reference.get("active")
            and not reference.get("dynamic")
            and reference.get("exists")
            and target in qml_paths
        ):
            exact_adjacency[reference["source"]].add(target)

    entries = sorted(
        item["resource"]
        for item in binary["resources"]
        if item["resource"] in qml_paths and item.get("confirmed_load_calls")
    )
    confirmed_origins: Dict[str, Set[str]] = defaultdict(set)
    queue: List[Tuple[str, str]] = [(entry, entry) for entry in entries]
    while queue:
        node, origin = queue.pop(0)
        if origin in confirmed_origins[node]:
            continue
        confirmed_origins[node].add(origin)
        for target in exact_adjacency.get(node, set()):
            queue.append((target, origin))

    potential_seeds: Dict[str, Set[str]] = defaultdict(set)
    dynamic_expansions: List[Dict[str, Any]] = []
    for reference in references:
        if not (reference.get("active") and reference.get("dynamic")):
            continue
        source = reference["source"]
        if source not in confirmed_origins:
            continue
        prefix = str(reference["resolved_target"]).rstrip("/") + "/"
        matches = sorted(path for path in qml_paths if path.startswith(prefix))
        dynamic_expansions.append(
            {
                "source": source,
                "line": reference["line"],
                "prefix": reference["resolved_target"],
                "candidate_qml_targets": matches,
            }
        )
        for target in matches:
            for origin in confirmed_origins[source]:
                potential_seeds[target].add(origin)

    potential_origins: Dict[str, Set[str]] = defaultdict(set)
    queue = [(node, origin) for node, origins in potential_seeds.items() for origin in origins]
    while queue:
        node, origin = queue.pop(0)
        if node in confirmed_origins or origin in potential_origins[node]:
            continue
        potential_origins[node].add(origin)
        for target in exact_adjacency.get(node, set()):
            queue.append((target, origin))

    nodes: Dict[str, Dict[str, Any]] = {}
    for path in sorted(qml_paths):
        if path in confirmed_origins:
            state = "confirmed_entry_reachable"
            origins = sorted(confirmed_origins[path])
        elif path in potential_origins:
            state = "potentially_entry_reachable_dynamic"
            origins = sorted(potential_origins[path])
        else:
            state = "not_entry_reachable"
            origins = []
        nodes[path] = {
            "reachability": state,
            "reachable_from_entry": state == "confirmed_entry_reachable",
            "potentially_reachable_from_dynamic_entry": state == "potentially_entry_reachable_dynamic",
            "entry_origins": origins,
        }
    return {
        "entry_roots": entries,
        "nodes": nodes,
        "dynamic_expansions": dynamic_expansions,
        "exact_adjacency": {key: sorted(value) for key, value in sorted(exact_adjacency.items())},
    }


def dependency_impact(item: Dict[str, Any]) -> str:
    state = item.get("source_reachability")
    target = str(item.get("resolved_target") or item.get("target") or "")
    is_qml = target.lower().endswith(".qml")
    if state == "confirmed_entry_reachable":
        return "entry_load_blocking_if_executed" if is_qml else "entry_reachable_runtime_missing_asset"
    if state == "potentially_entry_reachable_dynamic":
        return "potential_dynamic_runtime_dependency"
    return "non_blocking_for_confirmed_entries"


def binary_entry_evidence(exe_path: Path) -> Dict[str, Any]:
    pe = pefile.PE(str(exe_path), fast_load=False)
    data = bytes(pe.__data__)
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    imports: Dict[int, str] = {}
    for descriptor in pe.DIRECTORY_ENTRY_IMPORT:
        for imported in descriptor.imports:
            name = imported.name.decode(errors="replace") if imported.name else ""
            if "QQmlApplicationEngine4load" in name or "QQuickView9setSource" in name:
                va = int(imported.address)
                if va < image_base:
                    va += image_base
                imports[va] = name
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    instructions = []
    for section in pe.sections:
        if not (int(section.Characteristics) & 0x20000000):
            continue
        raw = data[int(section.PointerToRawData) : int(section.PointerToRawData) + int(section.SizeOfRawData)]
        instructions.extend(md.disasm(raw, image_base + int(section.VirtualAddress)))
    candidates = TOP_LEVEL_NAMES + ["QAndAPage.qml", "Views/AboutusPage.qml"]
    results: List[Dict[str, Any]] = []
    for filename in candidates:
        literal = "qrc:/" + filename
        pointer_vas: Set[int] = set()
        occurrences: List[Dict[str, Any]] = []
        for encoding in ("ascii", "utf-16-le"):
            needle = literal.encode(encoding)
            start = 0
            while True:
                raw_offset = data.find(needle, start)
                if raw_offset < 0:
                    break
                va = image_base + pe.get_rva_from_offset(raw_offset)
                pointer_va = va if encoding == "ascii" else va - 16
                pointer_vas.add(pointer_va)
                occurrences.append(
                    {"encoding": encoding, "raw_offset": raw_offset, "string_va": hex(va), "reference_pointer_va": hex(pointer_va)}
                )
                start = raw_offset + 1
        references: List[Dict[str, Any]] = []
        for index, ins in enumerate(instructions):
            hit = False
            for operand in ins.operands:
                if operand.type == X86_OP_IMM and int(operand.imm) in pointer_vas:
                    hit = True
                elif (
                    operand.type == X86_OP_MEM
                    and operand.mem.base == X86_REG_INVALID
                    and operand.mem.index == X86_REG_INVALID
                    and int(operand.mem.disp) in pointer_vas
                ):
                    hit = True
            if not hit:
                continue
            engine_calls: List[Dict[str, Any]] = []
            for following in instructions[index + 1 : index + 81]:
                if following.mnemonic.startswith("ret"):
                    break
                if following.mnemonic != "call" or not following.operands:
                    continue
                operand = following.operands[0]
                if (
                    operand.type == X86_OP_MEM
                    and operand.mem.base == X86_REG_INVALID
                    and operand.mem.index == X86_REG_INVALID
                    and int(operand.mem.disp) in imports
                ):
                    symbol = imports[int(operand.mem.disp)]
                    engine_calls.append(
                        {
                            "call_va": hex(following.address),
                            "api": "QQuickView::setSource" if "QQuickView" in symbol else "QQmlApplicationEngine::load",
                            "import_symbol": symbol,
                        }
                    )
                    break
            references.append(
                {
                    "instruction_va": hex(ins.address),
                    "instruction": f"{ins.mnemonic} {ins.op_str}",
                    "nearby_engine_calls": engine_calls,
                }
            )
        results.append(
            {
                "resource": ":/" + filename,
                "literal": literal,
                "literal_occurrences": occurrences,
                "code_references": references,
                "confirmed_load_calls": [call for reference in references for call in reference["nearby_engine_calls"]],
            }
        )
    return {
        "status": "ok",
        "method": "exact qrc literal xrefs followed by imported QQmlApplicationEngine::load/QQuickView::setSource calls",
        "target_imports": [{"iat_va": hex(va), "symbol": symbol} for va, symbol in sorted(imports.items())],
        "resources": results,
    }


def backend_markdown(mapping: Dict[str, Any]) -> str:
    lines = [
        "# Backend interface map", "",
        "> Static extraction only. Member names and line locations are authoritative QML uses; C++ signatures/types remain to be reconstructed or mocked.", "",
        "| object | role | status | occurrences |", "|---|---|---|---:|",
    ]
    for obj in mapping["objects"]:
        lines.append(f"| `{obj['object']}` | {obj['role']} | {obj['status']} | {obj['occurrence_count']} |")
    for obj in mapping["objects"]:
        if not obj["members"]:
            continue
        lines.extend(["", f"## `{obj['object']}`", "", "| member | access | count | locations |", "|---|---|---:|---|"])
        for member in obj["members"]:
            locations = ", ".join(
                f"`{x['file']}:{x['line']}` ({x.get('source_reachability')})"
                for x in member["locations"]
            )
            lines.append(f"| `{member['member_path']}` | {member['access_kind']} | {member['occurrence_count']} | {locations} |")
    lines.extend(
        [
            "", "## Requested names not present literally", "",
            "- `SettingsPageManager`: not referenced by type name; `setting` is the inferred instance/API surface.",
            "- `ViewHandler`: not referenced by type name; `window` is the inferred instance/API surface.",
            "- `recordDataModel`: absent.",
            "- `chatHistoryModel`: absent; recovered QML instead uses `dialog_manager.dialogModel` and `dialog_manager.dialogInfoModel`.",
        ]
    )
    return "\n".join(lines) + "\n"


def validation_markdown(validation: Dict[str, Any]) -> str:
    summary = validation["summary"]
    lines = [
        "# QML static validation", "",
        f"Validation mode: **{validation['validation_mode']}**. No Qt engine/parser result is claimed.", "",
        f"- QML files checked: {summary['qml_file_count']}",
        f"- Static checks passed: {summary['static_pass_count']}",
        f"- Static checks failed: {summary['static_fail_count']}",
        f"- Engine validations run: {summary['engine_validation_run_count']}",
        f"- Active resource references: {summary['active_reference_count']}",
        f"- Missing active references: {summary['missing_active_reference_count']}",
        f"- Dynamic unresolved references: {summary['dynamic_unresolved_reference_count']}",
        "", "| file | encoding | root object | reachability | static status | engine status |", "|---|---|---|---|---|---|",
    ]
    for record in validation["files"]:
        lines.append(
            f"| `{record['resource_path']}` | {record['encoding']} | `{record.get('root_object') or ''}` | "
            f"{record.get('reachability')} | {record['static_status']} | {record['engine_validation']['status']} |"
        )
    return "\n".join(lines) + "\n"


def application_flow_markdown(
    binary: Dict[str, Any], validations: List[Dict[str, Any]], graph: Dict[str, Any]
) -> str:
    by_name = {PurePosixPath(item["resource_path"]).name: item for item in validations}
    evidence_by_name = {PurePosixPath(item["resource"]).name: item for item in binary["resources"]}
    lines = [
        "# Application flow", "",
        "## Entry/loading evidence", "",
        "The RCC tree proves the source paths. Targeted PE xrefs then show which Qt loading API receives each exact qrc literal:", "",
        "| resource | root object | confirmed loading calls |", "|---|---|---|",
    ]
    for name in TOP_LEVEL_NAMES:
        validation = by_name.get(name, {})
        evidence = evidence_by_name.get(name, {})
        calls = ", ".join(
            f"`{call['api']} @ {call['call_va']}`" for call in evidence.get("confirmed_load_calls", [])
        ) or "no direct load xref confirmed"
        lines.append(f"| `:/{name}` | `{validation.get('root_object', '')}` | {calls} |")
    lines.extend(
        [
            "", "## Main and auxiliary windows", "",
            "- `:/MainView.qml` is the real AI chat main page. Two exact literal xrefs feed `QQuickView::setSource`; it depends on `window`, `dialog_manager`, `DialogModule`, `Controls`, `Component`, theme/font loaders and chat/history components.",
            "- `:/firstDiaLog.qml` is a separate consent/first-run `Window`; `window.agreePolicy`, `openWebsite` and `setFirstStart` must exist before it can function.",
            "- `:/updateWindow.qml` is a separate update `Window`; it reads version state and calls `window.updateSoft()` or `window.showGuideWindow()`.",
            "- `:/GuideWindow.qml` is a separate guide/reminder `Window` with shared theme/font loaders and image dependencies.",
            "- `:/SettingsPage.qml` is loaded by `QQmlApplicationEngine::load`; it uses the inferred SettingsPageManager instance `setting` for version/update/quota/startup behavior.",
            "- `:/TrayIconMenu.qml` is loaded with `QQuickView::setSource`; it needs `manager`, `setting`, `device_list_HL`, dynamic theme loading and the device model roles.",
            "", "## Corrected legacy assumptions", "",
            "- `:/MainView.qml` is authoritative and must be the chat main-page source in the next phase.",
            "- `QAndAPage.qml` does not exist in the RCC tree, but `:/Component/SwitchView.qml:100` contains one active unmet legacy loader reference. `SwitchView.qml` is not reachable from any PE-confirmed entry through recovered QML dependencies, so this is not a blocker for the confirmed SmartKey AI entry flow. If a later phase intentionally activates that shared device component, resolve its route deliberately (likely to authoritative `:/MainView.qml`) rather than fabricating a second page implementation or adding a global alias now.",
            "- `Views/AboutusPage.qml` neither exists in the RCC tree nor appears in recovered QML dependencies. Binary string presence alone is insufficient evidence and does not make it required source.",
            "", "## Required initialization order for a harness", "",
            "1. Register/mock context objects (`window`, `dialog_manager`, `setting`, device/configuration managers).",
            "2. Make the recovered RCC paths and deployed Qt 5.15.2 QML import modules visible.",
            "3. Load theme and font resources used by each top-level file.",
            "4. Load the selected top-level `Window`/`Item`; only then exercise signals and navigation.",
        ]
    )
    return "\n".join(lines) + "\n"


def stage3_report_markdown(
    classification: Dict[str, Any], validation: Dict[str, Any], missing: Dict[str, Any], backend: Dict[str, Any]
) -> str:
    counts = classification["summary"]["category_counts"]
    confirmed_backends = [
        obj["object"] for obj in backend["objects"] if obj.get("required_for_confirmed_entries")
    ]
    potential_backends = [
        obj["object"] for obj in backend["objects"]
        if obj.get("required_for_potential_dynamic_entries") and not obj.get("required_for_confirmed_entries")
    ]
    unreachable_backends = [
        obj["object"] for obj in backend["objects"]
        if obj["status"] == "referenced"
        and not obj.get("required_for_confirmed_entries")
        and not obj.get("required_for_potential_dynamic_entries")
    ]
    lines = [
        "# Stage 3 report", "", "## Outcome", "",
        f"All {classification['summary']['record_count']} authoritative RCC files were classified and all {validation['summary']['qml_file_count']} QML files received a validation record.",
        "Validation is **static_only** because no Qt 5.15.2 qmlscene/qmlcachegen/qmake development kit is installed. No engine-valid claim is made.",
        "", "## Resource classes", "", "| category | count |", "|---|---:|",
    ]
    for category, count in sorted(counts.items()):
        lines.append(f"| {category} | {count} |")
    lines.extend(
        [
            "", "## Ready for the next reconstruction phase", "",
            "- The 29 recovered QML files and all assets whose references resolve in `qml_dependency_graph.json` can be copied from the read-only stage2_v3 tree.",
            "- The real chat page is `:/MainView.qml`. `SwitchView.qml:100` has one stale unmet `qrc:/QAndAPage.qml` route, but SwitchView is not reachable from a confirmed entry, so no global redirect/alias is recommended now. `Views/AboutusPage.qml` is absent and unreferenced.",
            "- Qt standard GraphicalEffects wrappers are separated from SmartKey AI and Rapoo device/shared QML.",
            "", "## Missing/unresolved dependencies", "",
            f"- Missing active exact references: {missing['summary']['missing_exact_count']}",
            f"- Dynamic unresolved references: {missing['summary']['dynamic_unresolved_count']}",
            f"- Missing local component/import dependencies: {missing['summary']['missing_local_count']}",
            f"- Missing exact references by source reachability: `{json.dumps(missing['summary']['missing_exact_by_source_reachability'], ensure_ascii=False)}`",
            "- Treat only confirmed-entry missing QML as an entry-load blocker. Missing assets are runtime/visual gaps; potential-dynamic and unreachable shared-device references are tracked separately.",
            "- See `missing_dependencies.json` for every file/line/target; many missing image paths come from the shared Rapoo theme/device surface, not from fabricated QML source files.",
            "", "## Backend mocks required", "",
            "Confirmed-entry mocks: " + ", ".join(f"`{name}`" for name in confirmed_backends) + ".",
            "Potential dynamic-theme mocks: " + (", ".join(f"`{name}`" for name in potential_backends) or "none") + ".",
            "Referenced only by unreachable shared/device QML (defer unless activated): " + (", ".join(f"`{name}`" for name in unreachable_backends) or "none") + ".",
            "Exact property/method/signal paths and locations are in `backend_interface_map.json` and `.md`.",
            "", "## Toolchain gap", "",
            "Install a project-local Qt 5.15.2 MinGW 8.1 32-bit kit under `reconstruction_analysis/_toolchain`, invoke tools by absolute path, and set QML2_IMPORT_PATH to the deployed application QML module directory. Then run qmlcachegen/per-file parser checks before an isolated engine harness with mocks.",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    resource_root = Path(args.resource_root).resolve()
    manifest_path = Path(args.manifest).resolve()
    inventory_path = Path(args.inventory).resolve()
    exe_path = Path(args.input_exe).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))

    source_before = {
        str(path.relative_to(resource_root)).replace("\\", "/"): sha256_file(path)
        for path in resource_root.rglob("*") if path.is_file()
    }
    manifest_records = manifest.get("records", [])
    resources = {str(record["resource_path"]) for record in manifest_records}
    directories = directory_set(resources)
    runtime_modules = set(inventory["application_runtime"]["qml_modules"])

    classifications: List[Dict[str, Any]] = []
    for record in manifest_records:
        resource_path = str(record["resource_path"])
        relative = resource_path[2:]
        actual_path = resource_root.joinpath(*PurePosixPath(relative).parts)
        category, rule, basis = classify_resource(record)
        classifications.append(
            {
                "resource_path": resource_path,
                "relative_path": relative,
                "bundle_id": record.get("bundle_id"),
                "node_index": record.get("node_index"),
                "extension": actual_path.suffix.lower(),
                "size": actual_path.stat().st_size,
                "sha256": sha256_file(actual_path),
                "manifest_sha256": record.get("decompressed_sha256"),
                "manifest_hash_match": sha256_file(actual_path) == record.get("decompressed_sha256"),
                "classification": category,
                "classification_rule": rule,
                "classification_basis": basis,
                "stage2_validation_status": record.get("validation_status"),
            }
        )
    class_counts = Counter(item["classification"] for item in classifications)
    classification_document = {
        "status": "ok",
        "source_manifest": str(manifest_path),
        "source_tree": str(resource_root),
        "summary": {"record_count": len(classifications), "category_counts": dict(sorted(class_counts.items()))},
        "records": classifications,
    }
    json_write(output_dir / "resource_classification.json", classification_document)
    with (output_dir / "resource_classification.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        fields = [
            "resource_path", "relative_path", "bundle_id", "node_index", "extension", "size", "sha256",
            "manifest_sha256", "manifest_hash_match", "classification", "classification_rule",
            "classification_basis", "stage2_validation_status",
        ]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for item in classifications:
            row = dict(item)
            row["classification_basis"] = json.dumps(row["classification_basis"], ensure_ascii=False)
            writer.writerow(row)

    qml_paths = sorted((path for path in resource_root.rglob("*.qml") if path.is_file()), key=lambda p: str(p).casefold())
    qml_by_path = {":/" + str(path.relative_to(resource_root)).replace("\\", "/"): str(path) for path in qml_paths}
    component_names = {PurePosixPath(path).stem for path in qml_by_path}
    validations: List[Dict[str, Any]] = []
    graph_nodes: List[Dict[str, Any]] = []
    graph_edges: List[Dict[str, Any]] = []
    all_imports: List[Dict[str, Any]] = []
    all_refs: List[Dict[str, Any]] = []
    all_component_usages: List[Dict[str, Any]] = []
    all_unknown_types: List[Dict[str, Any]] = []
    all_backend_accesses: List[Dict[str, Any]] = []
    engine_mode = inventory["qml_engine_validation"]["mode"]
    for path in qml_paths:
        resource_path = ":/" + str(path.relative_to(resource_root)).replace("\\", "/")
        raw = path.read_bytes()
        text, encoding, decode_error = detect_and_decode(raw)
        if decode_error:
            scan = {"masked": "", "comment_mask": [], "strings": [], "errors": [{"kind": "decode_error", "detail": decode_error}], "balanced": False}
            imports: List[Dict[str, Any]] = []
            refs: List[Dict[str, Any]] = []
            usages: List[Dict[str, Any]] = []
            unknown_types: List[Dict[str, Any]] = []
            root_object = None
        else:
            scan = lexical_scan(text)
            imports = extract_imports(text, resource_path, resources, directories, runtime_modules)
            refs = extract_resource_references(text, scan, resource_path, resources, directories)
            usages, unknown_types = local_component_usages(
                scan["masked"], resource_path, imports, qml_by_path, component_names
            )
            root_matches = list(TYPE_RE.finditer(scan["masked"]))
            root_object = root_matches[0].group(1) if root_matches else None
            all_backend_accesses.extend(backend_accesses(scan["masked"], resource_path))
        structural_errors = list(scan["errors"])
        if not root_object:
            structural_errors.append({"kind": "missing_root_object"})
        static_status = "static_checks_passed" if not structural_errors else "static_checks_failed"
        engine_validation = {
            "status": "not_run" if engine_mode == "static_only" else "not_implemented",
            "mode": engine_mode,
            "reason": inventory["qml_engine_validation"].get("reason"),
            "engine_valid": None,
        }
        validations.append(
            {
                "resource_path": resource_path,
                "file": str(path),
                "size": len(raw),
                "sha256": hashlib.sha256(raw).hexdigest(),
                "encoding": encoding,
                "decode_error": decode_error,
                "root_object": root_object,
                "imports": imports,
                "delimiter_string_comment_checks": {
                    "passed": not scan["errors"], "errors": scan["errors"]
                },
                "structural_errors": structural_errors,
                "static_status": static_status,
                "engine_validation": engine_validation,
                "resource_reference_count": len(refs),
                "local_component_usage_count": len(usages),
            }
        )
        graph_nodes.append({"id": resource_path, "kind": "qml", "root_object": root_object, "static_status": static_status})
        for item in imports:
            entry = {"source": resource_path, **item}
            all_imports.append(entry)
            graph_edges.append(
                {
                    "source": resource_path,
                    "target": item.get("target") or item.get("module"),
                    "kind": "local_import" if item["kind"] == "local_directory" else "qt_module_import",
                    "line": item["line"],
                    "status": item["status"],
                }
            )
        for item in refs:
            all_refs.append(item)
            graph_edges.append(
                {
                    "source": resource_path,
                    "target": item["resolved_target"],
                    "kind": item["kind"],
                    "line": item["line"],
                    "status": item["status"],
                    "active": item["active"],
                    "dynamic": item["dynamic"],
                }
            )
        for item in usages:
            all_component_usages.append(item)
            for target in item["resolved_targets"] or item["candidate_targets"]:
                graph_edges.append(
                    {
                        "source": resource_path,
                        "target": target,
                        "kind": "local_component_type",
                        "line": item["line"],
                        "status": item["status"],
                        "type": item["type"],
                    }
                )
        all_unknown_types.extend(unknown_types)

    binary = binary_entry_evidence(exe_path)
    reachability = compute_reachability(set(qml_by_path), all_component_usages, all_refs, binary)
    reachability_nodes = reachability["nodes"]
    for collection, source_key in (
        (validations, "resource_path"),
        (graph_nodes, "id"),
        (all_imports, "source"),
        (all_refs, "source"),
        (all_component_usages, "source"),
        (all_unknown_types, "source"),
        (all_backend_accesses, "file"),
    ):
        for item in collection:
            source = item[source_key]
            item.update(reachability_nodes.get(source, {"reachability": "not_entry_reachable"}))
            item["source_reachability"] = item.get("reachability")
    for edge in graph_edges:
        edge["source_reachability"] = reachability_nodes.get(
            edge["source"], {"reachability": "not_entry_reachable"}
        )["reachability"]

    active_refs = [item for item in all_refs if item["active"]]
    missing_refs = [item for item in active_refs if item["status"] == "missing"]
    dynamic_refs = [item for item in active_refs if "dynamic" in item["status"]]
    missing_local_imports = [item for item in all_imports if item["kind"] == "local_directory" and item["status"] == "missing_directory"]
    missing_components = [item for item in all_component_usages if item["status"] == "missing_local_component"]
    for item in missing_refs + missing_local_imports + missing_components:
        item["impact"] = dependency_impact(item)
    validation_document = {
        "status": "ok",
        "validation_mode": engine_mode,
        "engine_validation_claim": False,
        "summary": {
            "qml_file_count": len(validations),
            "static_pass_count": sum(item["static_status"] == "static_checks_passed" for item in validations),
            "static_fail_count": sum(item["static_status"] != "static_checks_passed" for item in validations),
            "engine_validation_run_count": 0,
            "active_reference_count": len(active_refs),
            "missing_active_reference_count": len(missing_refs),
            "dynamic_unresolved_reference_count": len(dynamic_refs),
            "confirmed_entry_reachable_qml_count": sum(
                item["reachability"] == "confirmed_entry_reachable" for item in validations
            ),
            "potential_dynamic_qml_count": sum(
                item["reachability"] == "potentially_entry_reachable_dynamic" for item in validations
            ),
            "not_entry_reachable_qml_count": sum(
                item["reachability"] == "not_entry_reachable" for item in validations
            ),
        },
        "entry_roots": reachability["entry_roots"],
        "files": validations,
    }
    json_write(output_dir / "qml_static_validation.json", validation_document)
    (output_dir / "qml_static_validation.md").write_text(validation_markdown(validation_document), encoding="utf-8")
    dependency_graph = {
        "status": "ok",
        "validation_mode": engine_mode,
        "summary": {
            "node_count": len(graph_nodes), "edge_count": len(graph_edges), "import_count": len(all_imports),
            "resource_reference_count": len(all_refs), "local_component_usage_count": len(all_component_usages),
            "confirmed_entry_reachable_qml_count": sum(
                node["reachability"] == "confirmed_entry_reachable" for node in graph_nodes
            ),
            "potential_dynamic_qml_count": sum(
                node["reachability"] == "potentially_entry_reachable_dynamic" for node in graph_nodes
            ),
            "not_entry_reachable_qml_count": sum(
                node["reachability"] == "not_entry_reachable" for node in graph_nodes
            ),
        },
        "entry_roots": reachability["entry_roots"],
        "reachability": reachability,
        "nodes": graph_nodes,
        "edges": graph_edges,
        "imports": all_imports,
        "resource_references": all_refs,
        "local_component_usages": all_component_usages,
        "external_or_unresolved_types": all_unknown_types,
    }
    json_write(output_dir / "qml_dependency_graph.json", dependency_graph)
    missing_document = {
        "status": "ok",
        "summary": {
            "missing_exact_count": len(missing_refs),
            "dynamic_unresolved_count": len(dynamic_refs),
            "missing_local_count": len(missing_local_imports) + len(missing_components),
            "external_or_unresolved_type_count": len(all_unknown_types),
            "missing_exact_by_source_reachability": dict(
                sorted(Counter(item["source_reachability"] for item in missing_refs).items())
            ),
            "missing_exact_by_impact": dict(sorted(Counter(item["impact"] for item in missing_refs).items())),
        },
        "missing_exact_resource_references": missing_refs,
        "dynamic_unresolved_references": dynamic_refs,
        "missing_local_imports": missing_local_imports,
        "missing_local_components": missing_components,
        "external_or_unresolved_types": all_unknown_types,
    }
    json_write(output_dir / "missing_dependencies.json", missing_document)

    backend = build_backend_map(all_backend_accesses)
    json_write(output_dir / "backend_interface_map.json", backend)
    (output_dir / "backend_interface_map.md").write_text(backend_markdown(backend), encoding="utf-8")
    json_write(output_dir / "binary_entry_evidence.json", binary)
    (output_dir / "application_flow.md").write_text(
        application_flow_markdown(binary, validations, dependency_graph), encoding="utf-8"
    )
    (output_dir / "stage3_report.md").write_text(
        stage3_report_markdown(classification_document, validation_document, missing_document, backend), encoding="utf-8"
    )

    source_after = {
        str(path.relative_to(resource_root)).replace("\\", "/"): sha256_file(path)
        for path in resource_root.rglob("*") if path.is_file()
    }
    integrity = {
        "status": "PASS" if source_before == source_after else "FAIL",
        "source_tree": str(resource_root),
        "file_count_before": len(source_before),
        "file_count_after": len(source_after),
        "added": sorted(set(source_after) - set(source_before)),
        "removed": sorted(set(source_before) - set(source_after)),
        "changed": sorted(path for path in set(source_before) & set(source_after) if source_before[path] != source_after[path]),
        "manifest_hash_mismatch_count": sum(not item["manifest_hash_match"] for item in classifications),
    }
    json_write(output_dir / "source_tree_integrity.json", integrity)
    print(
        f"[analysis] resources={len(classifications)} qml={len(validations)} "
        f"static_pass={validation_document['summary']['static_pass_count']} "
        f"missing_refs={len(missing_refs)} dynamic={len(dynamic_refs)}"
    )
    print(f"[analysis] source_tree_integrity={integrity['status']} output={output_dir}")
    return 0 if integrity["status"] == "PASS" and len(validations) == len(qml_paths) else 3


if __name__ == "__main__":
    raise SystemExit(main())
