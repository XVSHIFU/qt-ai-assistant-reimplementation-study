#!/usr/bin/env python3
"""Copy authoritative RCC outputs byte-for-byte and generate the qrc file."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path
from xml.sax.saxutils import escape


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_manifest(workspace: Path) -> Path:
    candidates = list(workspace.rglob("authoritative_qrc_manifest_v3.json"))
    candidates = [p for p in candidates if "stage2_v3" in p.parts]
    if len(candidates) != 1:
        raise RuntimeError(f"expected exactly one Stage2 v3 manifest, found {candidates}")
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", default=str(Path(__file__).resolve().parents[1]))
    args = parser.parse_args()
    project = Path(args.project).resolve()
    workspace = project.parent
    manifest_path = find_manifest(workspace)
    source_root = manifest_path.parent / "recovered_qrc_tree"
    output_root = project / "qml" / "recovered"
    editable_root = project / "qml" / "app"
    report_dir = project / "reports"
    output_root.mkdir(parents=True, exist_ok=True)
    editable_root.mkdir(parents=True, exist_ok=True)
    report_dir.mkdir(parents=True, exist_ok=True)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    records_by_path = {}
    for record in manifest.get("records", []):
        resource_path = record["resource_path"]
        if resource_path in records_by_path:
            raise RuntimeError(f"duplicate resource path: {resource_path}")
        records_by_path[resource_path] = record

    copied = []
    expected_files = set()
    for resource_path, record in sorted(records_by_path.items()):
        relative = resource_path.removeprefix(":/")
        source = source_root / Path(relative)
        target = output_root / Path(relative)
        if not source.is_file():
            raise FileNotFoundError(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        source_bytes = source.read_bytes()
        target.write_bytes(source_bytes)
        editable_target = editable_root / Path(relative)
        if not editable_target.exists():
            editable_target.parent.mkdir(parents=True, exist_ok=True)
            editable_target.write_bytes(source_bytes)
        expected_files.add(target.resolve())
        source_hash = hashlib.sha256(source_bytes).hexdigest()
        target_hash = sha256(target)
        expected_hash = record["decompressed_sha256"]
        copied.append(
            {
                "resource_path": resource_path,
                "source": str(source.resolve()),
                "copy": str(target.resolve()),
                "size": target.stat().st_size,
                "manifest_sha256": expected_hash,
                "source_sha256": source_hash,
                "copy_sha256": target_hash,
                "verified": source_hash == target_hash == expected_hash,
            }
        )

    extras = sorted(
        str(p.resolve())
        for p in output_root.rglob("*")
        if p.is_file() and p.resolve() not in expected_files
    )
    status = "PASS" if len(copied) == 260 and all(x["verified"] for x in copied) and not extras else "FAIL"
    hashes_document = {
        "status": status,
        "method": "binary copy followed by SHA-256 comparison with authoritative_qrc_manifest_v3.json",
        "manifest": str(manifest_path.resolve()),
        "source_tree": str(source_root.resolve()),
        "copy_root": str(output_root.resolve()),
        "editable_root": str(editable_root.resolve()),
        "editable_policy": "seed missing files only; never overwrite second-development changes",
        "expected_count": 260,
        "copied_count": len(copied),
        "hash_match_count": sum(x["verified"] for x in copied),
        "unexpected_file_count": len(extras),
        "unexpected_files": extras,
        "records": copied,
    }
    (project / "recovered_file_hashes.json").write_text(
        json.dumps(hashes_document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    (report_dir / "recovered_copy_verification.json").write_text(
        json.dumps(hashes_document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    qrc_lines = ["<RCC>", '  <qresource prefix="/">']
    authoritative_aliases = set()
    for resource_path in sorted(records_by_path):
        relative = resource_path.removeprefix(":/")
        authoritative_aliases.add(relative)
        alias = escape(relative, {'"': "&quot;"})
        disk_path = escape(f"qml/app/{relative}")
        qrc_lines.append(f'    <file alias="{alias}">{disk_path}</file>')
    # Second-development QML/components live only in the editable tree. Keep
    # the authoritative 260-file verification strict while also compiling
    # additive application files into the resource bundle.
    editable_extras = []
    for path in sorted(p for p in editable_root.rglob("*") if p.is_file()):
        relative = path.relative_to(editable_root).as_posix()
        if relative in authoritative_aliases:
            continue
        editable_extras.append(relative)
        alias = escape(relative, {'"': "&quot;"})
        disk_path = escape(f"qml/app/{relative}")
        qrc_lines.append(f'    <file alias="{alias}">{disk_path}</file>')
    qrc_lines.extend(["  </qresource>", "</RCC>", ""])
    (project / "resources.qrc").write_text("\n".join(qrc_lines), encoding="utf-8")

    print(f"[sync] status={status} files={len(copied)} hash_matches={sum(x['verified'] for x in copied)} editable_extras={len(editable_extras)}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
