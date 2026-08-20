#!/usr/bin/env python3
"""Final machine-readable Stage4 acceptance checks."""

from __future__ import annotations

import hashlib
import json
import struct
import argparse
from datetime import datetime, timezone
from pathlib import Path

from artifact_provenance import validate_provenance
from release_artifacts import read_version, verify_checksums


def pe_metadata(path: Path) -> tuple[int, str, int, str]:
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("missing MZ header")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("missing PE signature")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    subsystem = struct.unpack_from("<H", data, pe_offset + 24 + 68)[0]
    return (
        machine,
        {0x14C: "IMAGE_FILE_MACHINE_I386"}.get(machine, f"0x{machine:04x}"),
        subsystem,
        {2: "IMAGE_SUBSYSTEM_WINDOWS_GUI"}.get(subsystem, f"0x{subsystem:04x}"),
    )


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--max-age-hours", type=float, default=24.0)
    parser.add_argument("--require-live", action="store_true",
                        help="include the explicitly-run, artifact-bound vendor smoke in the gate")
    parser.add_argument("--development-package", action="store_true",
                        help="allow an explicitly marked unsigned development package")
    parser.add_argument("--provenance-only", action="store_true")
    parser.add_argument("--now", help="UTC ISO timestamp override for deterministic self-tests")
    args = parser.parse_args()
    project = args.project.resolve()
    now = datetime.fromisoformat(args.now.replace("Z", "+00:00")) if args.now else datetime.now(timezone.utc)
    provenance = validate_provenance(
        project, now=now, max_age_hours=args.max_age_hours, require_live=args.require_live
    )
    if args.provenance_only:
        print(json.dumps(provenance, indent=2, ensure_ascii=False))
        return 0 if provenance["status"] == "PASS" else 1
    reports = project / "reports"
    json_files = sorted(project.glob("*.json")) + sorted(reports.glob("*.json"))
    parsed = []
    for path in json_files:
        raw = path.read_bytes()
        document = json.loads(raw.decode("utf-8"))
        parsed.append({"path": str(path.resolve()), "utf8_bom": raw.startswith(b"\xef\xbb\xbf"), "status": document.get("status")})

    hashes = json.loads((project / "recovered_file_hashes.json").read_text(encoding="utf-8"))
    build = json.loads((reports / "build_verification.json").read_text(encoding="utf-8"))
    engine = json.loads((reports / "qml_engine_validation.json").read_text(encoding="utf-8"))
    mocks = json.loads((reports / "mock_contract_coverage.json").read_text(encoding="utf-8"))
    module_tests_path = reports / "module_test_results.json"
    module_tests = json.loads(module_tests_path.read_text(encoding="utf-8")) if module_tests_path.is_file() else {}
    package_path = reports / "package_verification.json"
    package = json.loads(package_path.read_text(encoding="utf-8")) if package_path.is_file() else {}
    app_version = read_version(project)
    package_root = Path(str(package.get("package", "")))
    release_manifest_path = package_root / "release-manifest.json"
    release_manifest = json.loads(release_manifest_path.read_text(encoding="utf-8")) if release_manifest_path.is_file() else {}
    live_deepseek_path = reports / "live_deepseek_validation.json"
    live_deepseek = json.loads(live_deepseek_path.read_text(encoding="utf-8")) if live_deepseek_path.is_file() else {}
    exe = Path(build["executable"])
    screenshot = Path(engine["screenshot"])
    machine, machine_name, subsystem, subsystem_name = pe_metadata(exe)

    checks = {
        "artifact_provenance_current_and_consistent": provenance["status"] == "PASS",
        "all_json_utf8_parseable_without_bom": all(not item["utf8_bom"] for item in parsed),
        "recovered_260_hashes_match": hashes["status"] == "PASS" and hashes["copied_count"] == 260 and hashes["hash_match_count"] == 260,
        "build_pass": build["status"] == "PASS" and all(code == 0 for code in build["exit_codes"].values()),
        "pe_is_x86": machine == 0x14C,
        "release_uses_windows_gui_subsystem": subsystem == 2
            and build.get("release_has_no_console_subsystem") is True,
        "qml_engine_pass": engine["status"] == "PASS" and engine["exit_code"] == 0 and engine["root_object_non_null"],
        "qml_engine_has_no_warnings": not engine.get("qml_warning_or_error_lines"),
        "keyboard_reveal_focuses_chat_input": engine.get("input_focus_is_chat_input") is True,
        "settings_window_adaptive_smoke": engine.get("settings_adaptive_minimum_met") is True,
        "settings_window_taskbar_contract": engine.get("settings_taskbar_window_contract") is True,
        "settings_window_visual_smoke": engine.get("settings_screenshot_exists") is True
            and engine.get("settings_screenshot_size", 0) > 0,
        "model_picker_visual_smoke": engine.get("model_picker_screenshot_exists") is True
            and engine.get("model_picker_screenshot_size", 0) > 0,
        "model_picker_reopens_after_close": engine.get("model_picker_reopens_after_close") is True,
        "history_panel_visual_smoke": engine.get("history_screenshot_exists") is True
            and engine.get("history_screenshot_size", 0) > 0,
        "delete_confirm_visual_smoke": engine.get("delete_confirm_screenshot_exists") is True
            and engine.get("delete_confirm_screenshot_size", 0) > 0,
        "tooltip_visual_smoke": engine.get("tooltip_screenshot_exists") is True
            and engine.get("tooltip_screenshot_size", 0) > 0,
        "screenshot_nonempty": screenshot.is_file() and screenshot.stat().st_size > 0,
        "mock_contract_covered": mocks["status"] == "PASS" and mocks["missing_member_count"] == 0,
        "production_module_tests_pass": module_tests.get("status") == "PASS"
            and len(module_tests.get("modules", [])) == 6
            and any(module.get("name") == "startup" and module.get("status") == "PASS"
                    for module in module_tests.get("modules", [])),
        "standalone_package_pass": package.get("status") == "PASS"
            and package.get("smoke_exit_code") == 0
            and package.get("root_object_non_null") is True,
        "application_pe_package_version_consistent": build.get("application_version") == app_version
            and build.get("pe_version_resource_valid") is True
            and package.get("version") == app_version
            and release_manifest.get("version") == app_version
            and release_manifest.get("directory_name") == f"SmartKeyAI-{app_version}-win-x86",
        "release_manifest_hashes_valid": package.get("manifest_hashes_valid") is True
            and package_root.is_dir() and not verify_checksums(package_root),
        "package_published_atomically": package.get("published_atomically") is True
            and package_root.name == f"SmartKeyAI-{app_version}-win-x86",
        "release_signature_gate": package.get("signature", {}).get("public_release_valid") is True
            or (args.development_package
                and package.get("release_status") == "unsigned-development"
                and package.get("development_package") is True),
        "package_runtime_complete": package.get("qwindows_present") is True
            and package.get("qsqlite_present") is True
            and package.get("openssl_library_count") == 0
            and package.get("toolchain_on_smoke_path") is False,
        "package_uses_windows_gui_subsystem": package.get("release_has_no_console_subsystem") is True
            and package.get("pe_subsystem") == 2,
        "package_settings_visual_smoke": package.get("settings_screenshot_size", 0) > 0,
        "package_model_picker_visual_smoke": package.get("model_picker_screenshot_size", 0) > 0,
        "package_model_picker_reopens_after_close": package.get("model_picker_reopens_after_close") is True,
        "package_history_visual_smoke": package.get("history_screenshot_size", 0) > 0,
        "package_delete_confirm_visual_smoke": package.get("delete_confirm_screenshot_size", 0) > 0,
        "package_tooltip_visual_smoke": package.get("tooltip_screenshot_size", 0) > 0,
        "package_settings_taskbar_contract": package.get("settings_taskbar_window_contract") is True,
        "editable_qml_tree_preserves_260_resources": len([p for p in (project / "qml" / "app").rglob("*") if p.is_file()]) >= 260,
        "qrc_uses_editable_tree": "qml/recovered/" not in (project / "resources.qrc").read_text(encoding="utf-8")
            and (project / "resources.qrc").read_text(encoding="utf-8").count("qml/app/") >= 260,
        "provider_settings_ui_enabled": "providerSettings" in (project / "qml" / "app" / "SettingsPage.qml").read_text(encoding="utf-8")
            and "enabled: false" not in (project / "qml" / "app" / "SettingsPage.qml").read_text(encoding="utf-8")
            and "保存并开始对话" in (project / "qml" / "app" / "SettingsPage.qml").read_text(encoding="utf-8"),
        "credential_save_survives_profile_reload": "var pendingApiKey = apiKey.text" in (project / "qml" / "app" / "SettingsPage.qml").read_text(encoding="utf-8")
            and "saveProfileWithCredential" in (project / "qml" / "app" / "SettingsPage.qml").read_text(encoding="utf-8")
            and "pendingApiKey = \"\"" in (project / "qml" / "app" / "SettingsPage.qml").read_text(encoding="utf-8")
            and "saveProfileWithCredential" in (project / "src" / "settings" / "provider_settings.cpp").read_text(encoding="utf-8"),
        "deepseek_thinking_profile_supported": "thinkingEnabled" in (project / "src" / "settings" / "provider_profile.cpp").read_text(encoding="utf-8")
            and "reasoning_effort" in (project / "src" / "app" / "dialogmanager.cpp").read_text(encoding="utf-8"),
        "deepseek_dynamic_model_picker": "ModelDiscoveryService" in (project / "src" / "ai" / "modeldiscoveryservice.cpp").read_text(encoding="utf-8")
            and "availableModels" in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8"),
        "hotkey_is_visibility_toggle": "toggleFromHotkey" in (project / "src" / "main.cpp").read_text(encoding="utf-8")
            and "toggleFromTray();" in (project / "src" / "platform" / "windowcontroller.cpp").read_text(encoding="utf-8"),
        "first_reveal_is_above_taskbar": "positionInitialWindow" in (project / "src" / "platform" / "windowcontroller.cpp").read_text(encoding="utf-8")
            and "availableGeometry" in (project / "src" / "platform" / "windowcontroller.cpp").read_text(encoding="utf-8"),
        "thinking_icon_has_state_contrast": "thinking_off.svg" in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8")
            and "thinking_on.svg" in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8")
            and "#3F3F46" in (project / "qml" / "app" / "Image" / "Icon" / "thinking_off.svg").read_text(encoding="utf-8")
            and "#4F46E5" in (project / "qml" / "app" / "Image" / "Icon" / "thinking_on.svg").read_text(encoding="utf-8"),
        "model_selector_adapts_to_name": "TextMetrics" in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8")
            and "preferredWidth" in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8")
            and "triggerLabel.truncated" in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8"),
        "main_window_has_system_drag_entry": "beginSystemMove" in (project / "qml" / "app" / "MainView.qml").read_text(encoding="utf-8")
            and "beginSystemMove" in (project / "src" / "platform" / "windowcontroller.cpp").read_text(encoding="utf-8"),
        "history_actions_use_stable_ids": all(token in (project / "qml" / "app" / "DialogModule" / "ChatHistory.qml").read_text(encoding="utf-8")
            for token in ("selectChatById", "setChatPinned", "renameChatById", "deleteChatById")),
        "history_pin_is_persistent": "CurrentSchemaVersion = 4" in (project / "src" / "storage" / "chat_storage.h").read_text(encoding="utf-8")
            and "ORDER BY c.pinned DESC" in (project / "src" / "storage" / "chat_storage.cpp").read_text(encoding="utf-8"),
        "markdown_message_rendering": all(token in (project / "qml" / "app" / "Controls" / "MarkdownText.qml").read_text(encoding="utf-8")
            for token in ("renderCodeBlock", "isTableDivider", "quoteAccentColor", "TextEdit.RichText"))
            and "MarkdownText" in (project / "qml" / "app" / "DialogModule" / "DialogPanel.qml").read_text(encoding="utf-8"),
        "markdown_remote_content_is_safe": "return hold(\"<img" not in (project / "qml" / "app" / "Controls" / "MarkdownText.qml").read_text(encoding="utf-8").lower()
            and "safeHref" in (project / "qml" / "app" / "Controls" / "MarkdownText.qml").read_text(encoding="utf-8")
            and "escapeHtml" in (project / "qml" / "app" / "Controls" / "MarkdownText.qml").read_text(encoding="utf-8"),
        "message_list_performance_guards": all(token in (project / "qml" / "app" / "DialogModule" / "DialogPanel.qml").read_text(encoding="utf-8")
            for token in ("reuseItems: true", "Flickable.StopAtBounds", "clip: true", "Loader", "pixelDelta.y", "wheelScrollAnimation"))
            and "asstBox.width * scaleFactor" not in (project / "qml" / "app" / "DialogModule" / "DialogPanel.qml").read_text(encoding="utf-8"),
        "production_renderer_is_not_forced_software": "SMARTKEY_SOFTWARE_RENDERER" in (project / "src" / "main.cpp").read_text(encoding="utf-8")
            and "if (qEnvironmentVariableIntValue" in (project / "src" / "main.cpp").read_text(encoding="utf-8"),
        "system_ui_and_code_fonts_selected": "uiFontFamily" in (project / "src" / "main.cpp").read_text(encoding="utf-8")
            and "codeFontFamily" in (project / "qml" / "app" / "Controls" / "MarkdownText.qml").read_text(encoding="utf-8")
            and (project / "qml" / "app" / "FontManager.qml").read_text(encoding="utf-8").count("FontLoader") == 2,
        "copy_action_has_success_feedback": all(token in (project / "qml" / "app" / "DialogModule" / "DialogPanel.qml").read_text(encoding="utf-8")
            for token in ("property bool copied", "已复制", "copiedResetTimer", "✓")),
        "modern_tooltips_are_shared": "ModernToolTip" in (project / "qml" / "app" / "MainView.qml").read_text(encoding="utf-8")
            and "ModernToolTip" in (project / "qml" / "app" / "DialogModule" / "DialogPanel.qml").read_text(encoding="utf-8")
            and "#18181B" in (project / "qml" / "app" / "Controls" / "ModernToolTip.qml").read_text(encoding="utf-8"),
        "markdown_code_highlighting_and_tables": all(token in (project / "qml" / "app" / "Controls" / "MarkdownText.qml").read_text(encoding="utf-8")
            for token in ("highlightCode", "codeKeywordColor", "codeStringColor", "border=\\\"1\\\"", "cellpadding=\\\"7\\\"")),
        "model_picker_dismissal_consumes_click": all(token in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8")
            for token in ("property bool mayCloseOnDeactivate", "onActiveChanged", "Qt.callLater(hidePicker)", "sequence: \"Escape\"")),
        "conversation_expand_control_is_stable": "conversationExpanded" in (project / "qml" / "app" / "MainView.qml").read_text(encoding="utf-8")
            and "toggleConversationExpanded" in (project / "qml" / "app" / "MainView.qml").read_text(encoding="utf-8")
            and "text: modelPopup.visible ? \"^\" : \"v\"" not in (project / "qml" / "app" / "Component" / "DeepSeekModelSelector.qml").read_text(encoding="utf-8"),
        "legacy_broker_not_in_ai_transport": "driverapi.rapoo.cn" not in "".join(
            p.read_text(encoding="utf-8", errors="ignore") for p in (project / "src" / "ai").rglob("*") if p.is_file()
        ),
        "no_fake_qandapage": not (project / "qml" / "recovered" / "QAndAPage.qml").exists(),
        "no_fake_aboutuspage": not (project / "qml" / "recovered" / "Views" / "AboutusPage.qml").exists(),
    }
    if args.require_live:
        checks["live_deepseek_winhttp_pass"] = live_deepseek.get("status") == "PASS" \
            and live_deepseek.get("transport") == "WinHTTP/Schannel" \
            and live_deepseek.get("content_nonempty") is True \
            and live_deepseek.get("reasoning_nonempty") is True \
            and live_deepseek.get("configured_model_discovered") is True
    static_contract_names = {
        "all_json_utf8_parseable_without_bom",
        "recovered_260_hashes_match",
        "mock_contract_covered",
        "editable_qml_tree_preserves_260_resources",
        "qrc_uses_editable_tree",
        "provider_settings_ui_enabled",
        "credential_save_survives_profile_reload",
        "deepseek_thinking_profile_supported",
        "deepseek_dynamic_model_picker",
        "hotkey_is_visibility_toggle",
        "first_reveal_is_above_taskbar",
        "thinking_icon_has_state_contrast",
        "model_selector_adapts_to_name",
        "main_window_has_system_drag_entry",
        "history_actions_use_stable_ids",
        "history_pin_is_persistent",
        "markdown_message_rendering",
        "markdown_remote_content_is_safe",
        "message_list_performance_guards",
        "production_renderer_is_not_forced_software",
        "system_ui_and_code_fonts_selected",
        "copy_action_has_success_feedback",
        "modern_tooltips_are_shared",
        "markdown_code_highlighting_and_tables",
        "model_picker_dismissal_consumes_click",
        "conversation_expand_control_is_stable",
        "legacy_broker_not_in_ai_transport",
        "no_fake_qandapage",
        "no_fake_aboutuspage",
    }
    behavioral_checks = {name: value for name, value in checks.items()
                         if name not in static_contract_names}
    static_contract_checks = {name: checks[name] for name in static_contract_names if name in checks}
    status = "PASS" if all(behavioral_checks.values()) and all(static_contract_checks.values()) else "FAIL"
    result = {
        "status": status,
        "run_id": provenance.get("run_id"),
        "source_hash": provenance.get("source_hash"),
        "verified_at": now.astimezone(timezone.utc).isoformat().replace("+00:00", "Z"),
        "release_status": package.get("release_status"),
        "development_package_override": args.development_package,
        "behavioral": {
            "status": "PASS" if all(behavioral_checks.values()) else "FAIL",
            "passed": sum(behavioral_checks.values()),
            "total": len(behavioral_checks),
            "checks": behavioral_checks,
        },
        "static_contract": {
            "status": "PASS" if all(static_contract_checks.values()) else "FAIL",
            "passed": sum(static_contract_checks.values()),
            "total": len(static_contract_checks),
            "checks": static_contract_checks,
            "note": "source-token and structural checks; excluded from behavioral totals",
        },
        "provenance": provenance,
        "json_files": parsed,
        "executable": {
            "path": str(exe.resolve()),
            "size": exe.stat().st_size,
            "sha256": sha256(exe),
            "pe_machine": f"0x{machine:04x}",
            "pe_machine_name": machine_name,
            "pe_subsystem": f"0x{subsystem:04x}",
            "pe_subsystem_name": subsystem_name,
        },
        "screenshot": {
            "path": str(screenshot.resolve()),
            "size": screenshot.stat().st_size,
            "sha256": sha256(screenshot),
            "capture_method": engine["capture_method"],
        },
        "settings_screenshot": {
            "path": engine.get("settings_screenshot"),
            "size": engine.get("settings_screenshot_size", 0),
        },
        "model_picker_screenshot": {
            "path": engine.get("model_picker_screenshot"),
            "size": engine.get("model_picker_screenshot_size", 0),
        },
        "delete_confirm_screenshot": {
            "path": engine.get("delete_confirm_screenshot"),
            "size": engine.get("delete_confirm_screenshot_size", 0),
        },
        "package": package,
        "live_deepseek": {
            "included_in_gate": args.require_live,
            "report": live_deepseek if args.require_live else None,
            "note": "opt-in and excluded from the default gate",
        },
    }
    output = reports / "final_acceptance.json"
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"[acceptance] {status} behavioral={sum(behavioral_checks.values())}/{len(behavioral_checks)} "
          f"static_contract={sum(static_contract_checks.values())}/{len(static_contract_checks)} "
          f"machine={machine_name}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
