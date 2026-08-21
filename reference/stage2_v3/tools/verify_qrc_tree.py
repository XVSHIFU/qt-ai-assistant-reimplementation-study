#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Re-extract Qt RCC arrays into a fresh tree and compare all authoritative hashes."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


RECORD_FIELDS = [
    "call_va",
    "register_call_vas",
    "unregister_call_vas",
    "tree_va",
    "names_va",
    "data_va",
    "tree_raw_offset",
    "names_raw_offset",
    "data_base_raw_offset",
    "node_raw_offset",
    "name_offset",
    "name_record_raw_offset",
    "name_length_utf16",
    "name_hash",
    "flags",
    "flag_names",
    "locale",
    "last_modified",
    "data_offset",
    "raw_data_offset",
    "stored_payload_raw_offset",
    "compressed_stream_raw_offset",
    "stored_size",
    "decompressed_size",
    "qcompress_expected_size",
    "compression",
    "stored_sha256",
    "compressed_sha256",
    "decompressed_sha256",
    "validation_status",
    "v2_alignment",
]
BUNDLE_FIELDS = [
    "status",
    "error",
    "tree_va",
    "names_va",
    "data_va",
    "register_call_vas",
    "unregister_call_vas",
    "format_version",
    "node_size",
    "tree_location",
    "names_location",
    "data_location",
    "node_count",
    "directory_count",
    "file_count",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify a stage2_v3 Qt RCC extraction.")
    parser.add_argument("--input-exe", required=True)
    parser.add_argument("--calls-json", required=True)
    parser.add_argument("--manifest", required=True, help="Baseline authoritative manifest")
    parser.add_argument("--output-dir", required=True, help="Verification working directory")
    parser.add_argument("--v2-zlib-manifest")
    return parser.parse_args()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_reset_child(parent: Path, child_name: str) -> Path:
    parent = parent.resolve()
    child = (parent / child_name).resolve()
    if child.parent != parent or child == parent:
        raise RuntimeError(f"refusing unsafe verification directory reset: {child}")
    if child.exists():
        shutil.rmtree(child)
    child.mkdir(parents=True)
    return child


def record_key(record: Dict[str, Any]) -> Tuple[Any, ...]:
    locale = record.get("locale", {})
    return (
        record.get("bundle_id"),
        int(record.get("node_index", -1)),
        record.get("resource_path"),
        locale.get("country"),
        locale.get("language"),
    )


def map_by(records: Iterable[Dict[str, Any]], key_fn) -> Dict[Any, Dict[str, Any]]:
    result: Dict[Any, Dict[str, Any]] = {}
    for record in records:
        key = key_fn(record)
        if key in result:
            raise RuntimeError(f"duplicate manifest key: {key}")
        result[key] = record
    return result


def compare_maps(
    baseline: Dict[Any, Dict[str, Any]],
    fresh: Dict[Any, Dict[str, Any]],
    fields: List[str],
) -> Tuple[List[Any], List[Any], List[Dict[str, Any]]]:
    missing = sorted(set(baseline) - set(fresh), key=str)
    extra = sorted(set(fresh) - set(baseline), key=str)
    mismatches: List[Dict[str, Any]] = []
    for key in sorted(set(baseline) & set(fresh), key=str):
        differences = {
            field: {"baseline": baseline[key].get(field), "fresh": fresh[key].get(field)}
            for field in fields
            if baseline[key].get(field) != fresh[key].get(field)
        }
        if differences:
            mismatches.append({"key": key, "differences": differences})
    return missing, extra, mismatches


def check_output_files(records: Iterable[Dict[str, Any]], label: str) -> List[Dict[str, Any]]:
    failures: List[Dict[str, Any]] = []
    for record in records:
        path = Path(str(record.get("output_file", "")))
        if not path.is_file():
            failures.append(
                {"set": label, "resource_path": record.get("resource_path"), "reason": "missing_file"}
            )
            continue
        actual = sha256_file(path)
        expected = record.get("decompressed_sha256")
        if actual != expected:
            failures.append(
                {
                    "set": label,
                    "resource_path": record.get("resource_path"),
                    "reason": "sha256_mismatch",
                    "expected": expected,
                    "actual": actual,
                }
            )
        if path.stat().st_size != int(record.get("decompressed_size", -1)):
            failures.append(
                {
                    "set": label,
                    "resource_path": record.get("resource_path"),
                    "reason": "size_mismatch",
                    "expected": record.get("decompressed_size"),
                    "actual": path.stat().st_size,
                }
            )
    return failures


def main() -> int:
    args = parse_args()
    input_exe = Path(args.input_exe).resolve()
    calls_json = Path(args.calls_json).resolve()
    baseline_path = Path(args.manifest).resolve()
    verify_root = Path(args.output_dir).resolve()
    v2_manifest = Path(args.v2_zlib_manifest).resolve() if args.v2_zlib_manifest else None
    verify_root.mkdir(parents=True, exist_ok=True)
    fresh_root = safe_reset_child(verify_root, "fresh_tree")
    fresh_manifest = verify_root / "fresh_manifest.json"
    report_path = verify_root / "qrc_tree_verification.json"

    extractor = Path(__file__).resolve().parent / "extract_qt_rcc_arrays.py"
    command = [
        sys.executable,
        str(extractor),
        "--input-exe",
        str(input_exe),
        "--calls-json",
        str(calls_json),
        "--output-dir",
        str(fresh_root),
        "--manifest",
        str(fresh_manifest),
    ]
    if v2_manifest:
        command += ["--v2-zlib-manifest", str(v2_manifest)]
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    print(completed.stdout, end="")
    print(completed.stderr, end="", file=sys.stderr)
    if completed.returncode != 0:
        report = {
            "status": "FAIL",
            "extract_exit_code": completed.returncode,
            "reason": "fresh extraction failed",
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"[verify] status=FAIL report={report_path}")
        return completed.returncode

    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    fresh = json.loads(fresh_manifest.read_text(encoding="utf-8"))
    baseline_records = baseline.get("records", [])
    fresh_records = fresh.get("records", [])

    baseline_map = map_by(baseline_records, record_key)
    fresh_map = map_by(fresh_records, record_key)
    missing_records, extra_records, record_mismatches = compare_maps(
        baseline_map, fresh_map, RECORD_FIELDS
    )

    baseline_bundles = map_by(baseline.get("bundles", []), lambda item: item.get("bundle_id"))
    fresh_bundles = map_by(fresh.get("bundles", []), lambda item: item.get("bundle_id"))
    missing_bundles, extra_bundles, bundle_mismatches = compare_maps(
        baseline_bundles, fresh_bundles, BUNDLE_FIELDS
    )

    baseline_dirs = {
        (d.get("bundle_id"), d.get("node_index"), d.get("resource_path"))
        for d in baseline.get("directories", [])
    }
    fresh_dirs = {
        (d.get("bundle_id"), d.get("node_index"), d.get("resource_path"))
        for d in fresh.get("directories", [])
    }
    directory_mismatch = {
        "missing": sorted(baseline_dirs - fresh_dirs, key=str),
        "extra": sorted(fresh_dirs - baseline_dirs, key=str),
    }

    source_checks = {
        "exe_sha256_matches_baseline": sha256_file(input_exe) == baseline.get("source_exe_sha256"),
        "calls_json_sha256_matches_baseline": sha256_file(calls_json)
        == baseline.get("calls_json_sha256"),
        "fresh_exe_sha256_matches_baseline": fresh.get("source_exe_sha256")
        == baseline.get("source_exe_sha256"),
        "fresh_calls_sha256_matches_baseline": fresh.get("calls_json_sha256")
        == baseline.get("calls_json_sha256"),
        "core_qml_results_match": fresh.get("core_qml_results") == baseline.get("core_qml_results"),
    }
    count_fields = [
        "bundle_count",
        "successful_bundle_count",
        "failed_bundle_count",
        "record_count",
        "directory_count",
        "verified_v2_path_and_hash_count",
    ]
    count_mismatches = {
        key: {"baseline": baseline.get(key), "fresh": fresh.get(key)}
        for key in count_fields
        if baseline.get(key) != fresh.get(key)
    }
    file_failures = check_output_files(baseline_records, "baseline") + check_output_files(
        fresh_records, "fresh"
    )

    passed = not any(
        [
            missing_records,
            extra_records,
            record_mismatches,
            missing_bundles,
            extra_bundles,
            bundle_mismatches,
            directory_mismatch["missing"],
            directory_mismatch["extra"],
            count_mismatches,
            file_failures,
            [key for key, value in source_checks.items() if not value],
        ]
    )
    report = {
        "status": "PASS" if passed else "FAIL",
        "verify_exit_code": 0 if passed else 3,
        "extract_exit_code": completed.returncode,
        "input_exe": str(input_exe),
        "calls_json": str(calls_json),
        "baseline_manifest": str(baseline_path),
        "fresh_manifest": str(fresh_manifest),
        "source_checks": source_checks,
        "baseline_record_count": len(baseline_records),
        "fresh_record_count": len(fresh_records),
        "missing_records": missing_records,
        "extra_records": extra_records,
        "record_mismatches": record_mismatches,
        "missing_bundles": missing_bundles,
        "extra_bundles": extra_bundles,
        "bundle_mismatches": bundle_mismatches,
        "directory_mismatch": directory_mismatch,
        "count_mismatches": count_mismatches,
        "file_failures": file_failures,
    }
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(
        f"[verify] status={report['status']} records={len(baseline_records)} "
        f"record_mismatches={len(record_mismatches)} file_failures={len(file_failures)}"
    )
    print(f"[verify] report={report_path}")
    return 0 if passed else 3


if __name__ == "__main__":
    raise SystemExit(main())
