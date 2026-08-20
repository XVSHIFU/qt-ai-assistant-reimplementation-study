#!/usr/bin/env python3
"""Map recovered QML contracts to production facades and the remaining device placeholder."""

from __future__ import annotations

import json
import os
from datetime import datetime, timezone
from pathlib import Path


IMPLEMENTATION = {
    "window": {
        "x", "y", "width", "height", "scale", "agreePolicy", "allowQuestion", "notEnough",
        "newVersionNumber", "topScreen", "radius", "skinMode", "sysSkinType", "openWebsite",
        "setDialog", "setFirstStart", "setHistory", "setXandY", "showGuideWindow", "updateSoft",
    },
    "dialog_manager": {
        "reasonModeActive", "searchModeActive", "responseGenerated", "isChatEmpty", "isHistoryEmpty",
        "currentIndex", "dialogModel", "dialogInfoModel", "dialogModel.clearAll",
        "dialogModel.rowCount", "addNewChat", "sendMessage", "regenerate", "copy",
        "deleteChatAtIndex", "modifyTitleAtIndex", "parseJSONToCurrentChat", "appended",
    },
    "setting": {
        "updated", "currentVersion", "startUpAuto", "totalCount", "usedCount", "usedNumCount",
        "checkUpdate", "setStartUpAuto",
    },
    "manager": {"currentTheme", "itemClicked"},
    "device_list_HL": {
        "deviceModel", "deviceModel.get", "deviceModel.rowCount", "reCurConfigure",
        "deviceListChanged", "deviceDisconnection",
    },
}

IMPLEMENTATION_FILES = {
    "window": "src/app/windowfacade.* + src/platform/windowcontroller.*",
    "dialog_manager": "src/app/dialogmanager.* + src/app/chatmodels.*",
    "setting": "src/app/settingfacade.* + src/platform/autostartservice.*",
    "manager": "src/app/managerfacade.h + src/platform/systemtraycontroller.*",
    "device_list_HL": "src/mock/backendmocks.* (device-only placeholder)",
}


def main() -> int:
    project = Path(__file__).resolve().parents[1]
    workspace = project.parent
    candidates = list(workspace.rglob("backend_interface_map.json"))
    candidates = [p for p in candidates if p.parent.name == "reconstruction_analysis"]
    if len(candidates) != 1:
        raise RuntimeError(f"expected one Stage3 interface map, found {candidates}")
    source = candidates[0]
    backend = json.loads(source.read_text(encoding="utf-8"))
    records = []
    for obj in backend.get("objects", []):
        if not obj.get("required_for_confirmed_entries"):
            continue
        name = obj["object"]
        available = IMPLEMENTATION.get(name, set())
        for member in obj.get("members", []):
            path = member["member_path"]
            records.append({
                "object": name,
                "member_path": path,
                "access_kind": member["access_kind"],
                "occurrence_count": member["occurrence_count"],
                "covered": path in available,
                "implementation": IMPLEMENTATION_FILES.get(name) if path in available else None,
                "semantics": "production implementation" if name != "device_list_HL" else "non-fatal device placeholder",
            })
    missing = [x for x in records if not x["covered"]]
    document = {
        "status": "PASS" if not missing else "FAIL",
        "validation_kind": "static_contract",
        "run_id": os.environ.get("SMARTKEY_RUN_ID", ""),
        "source_hash": os.environ.get("SMARTKEY_SOURCE_HASH", ""),
        "checked_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "source_contract": str(source.resolve()),
        "scope": "Stage3 confirmed-entry-reachable context objects after second-development integration",
        "context_objects_registered": sorted(IMPLEMENTATION),
        "required_member_count": len(records),
        "covered_member_count": sum(x["covered"] for x in records),
        "missing_member_count": len(missing),
        "missing": missing,
        "records": records,
        "excluded_as_not_entry_reachable": [
            "configure_Manager", "config_set_HL", "lang_HL", "pairmanager",
            "kb_travelDistance_manager", "themeManager",
        ],
        "production_services": [
            "WinHTTP/Schannel OpenAI-compatible chat", "DPAPI provider credentials",
            "SQLite chat persistence", "clipboard integration", "startup registration",
            "global hotkey/system tray/single-instance window orchestration",
        ],
        "remaining_placeholders": ["device discovery/control", "software updater"],
    }
    output = project / "reports" / "mock_contract_coverage.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (project / "reports" / "backend_contract_coverage.json").write_text(
        json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"[mock-coverage] {document['status']} {document['covered_member_count']}/{document['required_member_count']}")
    return 0 if document["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
