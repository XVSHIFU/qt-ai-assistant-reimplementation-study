#!/usr/bin/env python3
"""Versioned release manifest, checksum, SBOM, and atomic publish helpers."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


VERSION_PATTERN = re.compile(r"^SMARTKEY_APP_VERSION\s*=\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$", re.MULTILINE)


def read_version(project: Path) -> str:
    source = (project / "version.pri").read_text(encoding="utf-8")
    match = VERSION_PATTERN.search(source)
    if not match:
        raise ValueError("version.pri must define one numeric SMARTKEY_APP_VERSION")
    return match.group(1)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def generate_metadata(
    project: Path,
    package_root: Path,
    run_manifest_path: Path,
    release_status: str,
    authenticode_status: str,
) -> dict[str, Any]:
    project = project.resolve()
    package_root = package_root.resolve()
    version = read_version(project)
    run = json.loads(run_manifest_path.read_text(encoding="utf-8"))
    executable = package_root / "SmartKeyAI.exe"
    if not executable.is_file():
        raise ValueError(f"missing packaged executable: {executable}")
    exe_hash = sha256(executable)
    if exe_hash != run.get("exe_sha256"):
        raise ValueError("packaged executable does not match build manifest")

    generated_at = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    release_manifest = {
        "schema_version": 1,
        "product": "SmartKey AI",
        "version": version,
        "platform": "win",
        "architecture": "x86",
        "directory_name": f"SmartKeyAI-{version}-win-x86",
        "release_status": release_status,
        "authenticode_status": authenticode_status,
        "run_id": run.get("run_id"),
        "source_hash": run.get("source_hash"),
        "built_at": run.get("built_at"),
        "generated_at": generated_at,
        "exe_sha256": exe_hash,
        "toolchain": run.get("toolchain", {}),
    }
    _write_json(package_root / "release-manifest.json", release_manifest)

    files_before_sbom = sorted(
        path for path in package_root.rglob("*")
        if path.is_file() and path.name not in {"SHA256SUMS.json", "sbom.spdx.json"}
    )
    sbom = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"SmartKeyAI-{version}-win-x86",
        "documentNamespace": f"https://smartkey.local/spdx/{run.get('run_id')}",
        "creationInfo": {
            "created": generated_at,
            "creators": ["Tool: SmartKeyAI release_artifacts.py"],
        },
        "packages": [
            {
                "name": "SmartKey AI",
                "SPDXID": "SPDXRef-Package-SmartKeyAI",
                "versionInfo": version,
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": True,
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
                "supplier": "Organization: SmartKeyAI",
            },
            {
                "name": "Qt",
                "SPDXID": "SPDXRef-Package-Qt",
                "versionInfo": str(run.get("toolchain", {}).get("qt_version", "unknown")),
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": False,
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
            },
        ],
        "files": [
            {
                "fileName": path.relative_to(package_root).as_posix(),
                "SPDXID": "SPDXRef-File-" + hashlib.sha256(
                    path.relative_to(package_root).as_posix().encode("utf-8")
                ).hexdigest()[:20],
                "checksums": [{"algorithm": "SHA256", "checksumValue": sha256(path)}],
                "licenseConcluded": "NOASSERTION",
            }
            for path in files_before_sbom
        ],
    }
    _write_json(package_root / "sbom.spdx.json", sbom)

    checksum_entries = []
    for path in sorted(package_root.rglob("*")):
        if not path.is_file() or path.name == "SHA256SUMS.json":
            continue
        checksum_entries.append({
            "path": path.relative_to(package_root).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256(path),
        })
    checksum_manifest = {
        "schema_version": 1,
        "algorithm": "SHA-256",
        "run_id": run.get("run_id"),
        "source_hash": run.get("source_hash"),
        "version": version,
        "files": checksum_entries,
    }
    _write_json(package_root / "SHA256SUMS.json", checksum_manifest)
    return release_manifest


def verify_checksums(package_root: Path) -> list[str]:
    document = json.loads((package_root / "SHA256SUMS.json").read_text(encoding="utf-8"))
    errors: list[str] = []
    for entry in document.get("files", []):
        path = package_root / entry["path"]
        if not path.is_file():
            errors.append(f"missing: {entry['path']}")
        elif path.stat().st_size != entry["size"]:
            errors.append(f"size mismatch: {entry['path']}")
        elif sha256(path) != entry["sha256"]:
            errors.append(f"hash mismatch: {entry['path']}")
    return errors


def publish_staging(staging: Path, final: Path) -> None:
    staging = staging.resolve()
    final = final.resolve()
    if not staging.is_dir():
        raise ValueError(f"missing staging directory: {staging}")
    if final.exists():
        raise FileExistsError(f"release already exists and is immutable: {final}")
    final.parent.mkdir(parents=True, exist_ok=True)
    os.replace(staging, final)


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    version_parser = subparsers.add_parser("version")
    version_parser.add_argument("--project", type=Path, required=True)
    generate_parser = subparsers.add_parser("generate")
    generate_parser.add_argument("--project", type=Path, required=True)
    generate_parser.add_argument("--package-root", type=Path, required=True)
    generate_parser.add_argument("--run-manifest", type=Path, required=True)
    generate_parser.add_argument("--release-status", required=True)
    generate_parser.add_argument("--authenticode-status", required=True)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--package-root", type=Path, required=True)
    publish_parser = subparsers.add_parser("publish")
    publish_parser.add_argument("--staging", type=Path, required=True)
    publish_parser.add_argument("--final", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "version":
        print(read_version(args.project))
        return 0
    if args.command == "generate":
        result = generate_metadata(
            args.project, args.package_root, args.run_manifest,
            args.release_status, args.authenticode_status,
        )
        print(json.dumps(result, ensure_ascii=False))
        return 0
    if args.command == "verify":
        errors = verify_checksums(args.package_root)
        if errors:
            print(json.dumps({"status": "FAIL", "errors": errors}, ensure_ascii=False))
            return 1
        print('{"status":"PASS"}')
        return 0
    publish_staging(args.staging, args.final)
    print(f"[publish] {args.final}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
