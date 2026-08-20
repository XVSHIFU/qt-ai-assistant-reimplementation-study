#!/usr/bin/env python3
"""Shared, deterministic build/report provenance helpers."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


EXCLUDED_DIRECTORY_NAMES = {
    ".agents", ".git", ".pytest_cache", "__pycache__", "build", "dist", "reports", "_toolchain"
}


def _excluded(relative: Path) -> bool:
    for part in relative.parts[:-1]:
        lowered = part.lower()
        if lowered in EXCLUDED_DIRECTORY_NAMES:
            return True
        if (lowered.startswith("build_") or lowered.startswith("agent_")
                or lowered.startswith("agent-") or lowered.startswith("_agent")):
            return True
    return False


def source_tree_hash(project: Path) -> str:
    """Hash source paths and bytes, independent of timestamps and enumeration order."""
    project = project.resolve()
    digest = hashlib.sha256()
    files = sorted(
        (path for path in project.rglob("*") if path.is_file()
         and not _excluded(path.relative_to(project))),
        key=lambda path: path.relative_to(project).as_posix().encode("utf-8"),
    )
    for path in files:
        relative = path.relative_to(project).as_posix().encode("utf-8")
        content = path.read_bytes()
        digest.update(len(relative).to_bytes(8, "big"))
        digest.update(relative)
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
    return digest.hexdigest()


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def utc_iso(value: datetime | None = None) -> str:
    return (value or utc_now()).astimezone(timezone.utc).isoformat().replace("+00:00", "Z")


def parse_utc(value: Any, field: str) -> datetime:
    if not isinstance(value, str) or not value:
        raise ValueError(f"missing {field}")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise ValueError(f"{field} must include a timezone")
    return parsed.astimezone(timezone.utc)


def load_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise ValueError(f"missing report: {path}")
    document = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError(f"report is not an object: {path}")
    return document


def validate_provenance(
    project: Path,
    *,
    now: datetime | None = None,
    max_age_hours: float = 24.0,
    require_live: bool = False,
) -> dict[str, Any]:
    """Validate that every behavioral report belongs to one current artifact run."""
    project = project.resolve()
    reports = project / "reports"
    now = (now or utc_now()).astimezone(timezone.utc)
    errors: list[str] = []
    checks: dict[str, bool] = {}

    documents: dict[str, dict[str, Any]] = {}
    paths = {
        "manifest": reports / "run_manifest.json",
        "build": reports / "build_verification.json",
        "module": reports / "module_test_results.json",
        "smoke": reports / "qml_engine_validation.json",
        "package": reports / "package_verification.json",
    }
    for name, path in paths.items():
        try:
            documents[name] = load_json(path)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            errors.append(str(exc))

    if errors:
        return {"status": "FAIL", "checks": checks, "errors": errors, "live": {"included": False}}

    manifest = documents["manifest"]
    build = documents["build"]
    run_id = manifest.get("run_id")
    source_hash = manifest.get("source_hash")
    exe_hash = manifest.get("exe_sha256")
    built_at_raw = manifest.get("built_at")

    checks["manifest_complete"] = all(
        isinstance(value, str) and bool(value)
        for value in (run_id, source_hash, exe_hash, built_at_raw)
    ) and isinstance(manifest.get("toolchain"), dict)
    checks["build_matches_manifest"] = all(
        build.get(field) == manifest.get(field)
        for field in ("run_id", "source_hash", "built_at", "exe_sha256")
    )
    checks["source_tree_matches_build"] = source_tree_hash(project) == source_hash

    try:
        built_at = parse_utc(built_at_raw, "built_at")
    except (TypeError, ValueError) as exc:
        errors.append(str(exc))
        built_at = now
        checks["time_order_valid"] = False

    max_age_seconds = max_age_hours * 3600.0
    checks["build_report_fresh"] = -300.0 <= (now - built_at).total_seconds() <= max_age_seconds

    build_exe = Path(str(build.get("executable", "")))
    checks["build_executable_hash_matches"] = (
        build_exe.is_file() and file_sha256(build_exe) == exe_hash
    )

    time_order_valid = True
    inherited = True
    fresh = True
    for report_name in ("module", "smoke", "package"):
        document = documents[report_name]
        inherited = inherited and all(
            document.get(field) == manifest.get(field)
            for field in ("run_id", "source_hash", "built_at")
        )
        artifact_hash_field = "build_exe_sha256" if report_name == "package" else "exe_sha256"
        inherited = inherited and document.get(artifact_hash_field) == exe_hash
        try:
            tested_at = parse_utc(document.get("tested_at"), f"{report_name}.tested_at")
            time_order_valid = time_order_valid and built_at <= tested_at <= now.replace(microsecond=999999)
            fresh = fresh and -300.0 <= (now - tested_at).total_seconds() <= max_age_seconds
            if report_name == "package":
                packaged_at = parse_utc(document.get("packaged_at"), "package.packaged_at")
                time_order_valid = time_order_valid and built_at <= packaged_at <= tested_at
        except (TypeError, ValueError) as exc:
            errors.append(str(exc))
            time_order_valid = False
            fresh = False

    checks["reports_inherit_build_identity"] = inherited
    checks["time_order_valid"] = time_order_valid
    checks["behavioral_reports_fresh"] = fresh

    package = documents["package"]
    package_exe = Path(str(package.get("package_executable", "")))
    package_hash = package.get("package_sha256")
    checks["package_executable_hash_matches"] = (
        package_exe.is_file() and isinstance(package_hash, str)
        and file_sha256(package_exe) == package_hash
    )
    checks["build_and_package_executable_identical"] = package_hash == exe_hash

    live_path = reports / "live_deepseek_validation.json"
    live_info: dict[str, Any] = {"included": require_live, "path": str(live_path)}
    if require_live:
        try:
            live = load_json(live_path)
            live_tested_at = parse_utc(live.get("tested_at"), "live.tested_at")
            live_checks = {
                "current_run": all(live.get(field) == manifest.get(field)
                                   for field in ("run_id", "source_hash", "built_at")),
                "bound_artifact": live.get("exe_sha256") == exe_hash,
                "time_order": built_at <= live_tested_at <= now.replace(microsecond=999999),
                "fresh": -300.0 <= (now - live_tested_at).total_seconds() <= max_age_seconds,
                "explicit_opt_in": live.get("opt_in") is True,
                "passed": live.get("status") == "PASS",
            }
            live_info.update({"checks": live_checks, "status": "PASS" if all(live_checks.values()) else "FAIL"})
            checks["live_vendor_smoke_current_and_bound"] = all(live_checks.values())
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            errors.append(str(exc))
            checks["live_vendor_smoke_current_and_bound"] = False
            live_info["status"] = "FAIL"
    else:
        live_info.update({"status": "NOT_RUN_OR_NOT_REQUIRED", "reason": "opt-in; excluded from default gate"})

    for name, passed in checks.items():
        if not passed:
            errors.append(f"failed provenance check: {name}")
    return {
        "status": "PASS" if not errors else "FAIL",
        "run_id": run_id,
        "source_hash": source_hash,
        "exe_sha256": exe_hash,
        "checks": checks,
        "errors": errors,
        "live": live_info,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    hash_parser = subparsers.add_parser("source-hash")
    hash_parser.add_argument("--project", type=Path, required=True)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--project", type=Path, required=True)
    verify_parser.add_argument("--max-age-hours", type=float, default=24.0)
    verify_parser.add_argument("--require-live", action="store_true")
    args = parser.parse_args()
    if args.command == "source-hash":
        print(source_tree_hash(args.project))
        return 0
    result = validate_provenance(
        args.project, max_age_hours=args.max_age_hours, require_live=args.require_live
    )
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
