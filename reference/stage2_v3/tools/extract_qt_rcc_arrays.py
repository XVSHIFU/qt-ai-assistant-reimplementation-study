#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Extract Qt 5 RCC v3 arrays referenced by qRegisterResourceData calls."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Set, Tuple

import pefile


FLAG_ZLIB = 0x01
FLAG_DIRECTORY = 0x02
FLAG_ZSTD = 0x04
KNOWN_FLAGS = FLAG_ZLIB | FLAG_DIRECTORY | FLAG_ZSTD
NODE_SIZE_BY_VERSION = {1: 14, 2: 22, 3: 22}
CORE_QML_QUERIES = [
    "MainView.qml",
    "QAndAPage.qml",
    "SettingsPage.qml",
    "TrayIconMenu.qml",
    "GuideWindow.qml",
    "updateWindow.qml",
    "firstDiaLog.qml",
    "Views/AboutusPage.qml",
]


class RCCFormatError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Parse Qt 5 RCC v3 arrays from a PE image.")
    parser.add_argument("--input-exe", required=True)
    parser.add_argument("--calls-json", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--v2-zlib-manifest", help="Optional stage2_v2 heuristic zlib manifest")
    parser.add_argument("--max-stored-size", type=int, default=512 * 1024 * 1024)
    parser.add_argument("--max-output-size", type=int, default=512 * 1024 * 1024)
    parser.add_argument("--max-nodes", type=int, default=1_000_000)
    return parser.parse_args()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def be16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def be64(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from(">Q", data, offset)[0]


def normalize_qrc_path(path: str) -> str:
    value = path.strip().replace("\\", "/")
    if value.startswith("qrc:/"):
        value = ":/" + value[5:]
    elif value.startswith("/"):
        value = ":" + value
    return value


def timestamp_iso(ms: int) -> Optional[str]:
    if not ms:
        return None
    try:
        return dt.datetime.fromtimestamp(ms / 1000, tz=dt.timezone.utc).isoformat().replace("+00:00", "Z")
    except (OverflowError, OSError, ValueError):
        return None


@dataclass(frozen=True)
class MappedVA:
    va: int
    rva: int
    section_name: str
    raw_offset: int
    section_raw_start: int
    section_raw_end: int

    def json(self) -> Dict[str, Any]:
        return {
            "virtual_address": hex(self.va),
            "rva": hex(self.rva),
            "section_name": self.section_name,
            "raw_offset": self.raw_offset,
            "section_raw_end": self.section_raw_end,
        }


class PEImage:
    def __init__(self, path: Path):
        self.path = path
        self.pe = pefile.PE(str(path), fast_load=False)
        self.data = bytes(self.pe.__data__)
        self.image_base = int(self.pe.OPTIONAL_HEADER.ImageBase)

    def map_va(self, va: int, size: int = 1) -> MappedVA:
        if size < 0:
            raise RCCFormatError(f"negative mapping size for VA {va:#x}")
        rva = va - self.image_base
        if rva < 0:
            raise RCCFormatError(f"VA {va:#x} is below image base {self.image_base:#x}")
        for section in self.pe.sections:
            section_rva = int(section.VirtualAddress)
            raw_size = int(section.SizeOfRawData)
            delta = rva - section_rva
            if 0 <= delta and delta + size <= raw_size:
                raw_start = int(section.PointerToRawData)
                raw_offset = raw_start + delta
                if raw_offset + size > len(self.data):
                    raise RCCFormatError(f"VA {va:#x} maps beyond the PE file")
                return MappedVA(
                    va=va,
                    rva=rva,
                    section_name=section.Name.decode("utf-8", errors="replace").rstrip("\x00"),
                    raw_offset=raw_offset,
                    section_raw_start=raw_start,
                    section_raw_end=raw_start + raw_size,
                )
        raise RCCFormatError(f"VA range {va:#x}+{size:#x} is not backed by a PE section")

    def read_at(self, mapped: MappedVA, relative_offset: int, size: int, label: str) -> bytes:
        if relative_offset < 0 or size < 0:
            raise RCCFormatError(f"{label}: negative offset/size")
        start = mapped.raw_offset + relative_offset
        end = start + size
        if start < mapped.raw_offset or end > mapped.section_raw_end or end > len(self.data):
            raise RCCFormatError(
                f"{label}: out of bounds (relative={relative_offset:#x}, size={size:#x}, "
                f"section_end={mapped.section_raw_end:#x})"
            )
        return self.data[start:end]


def flag_names(flags: int) -> List[str]:
    result: List[str] = []
    if flags & FLAG_ZLIB:
        result.append("Compressed")
    if flags & FLAG_DIRECTORY:
        result.append("Directory")
    if flags & FLAG_ZSTD:
        result.append("CompressedZstd")
    return result or ["NoFlags"]


def decompress_zlib(payload: bytes, maximum: int) -> Tuple[bytes, int, bytes]:
    if len(payload) < 4:
        raise RCCFormatError("zlib resource is missing qCompress's 4-byte size prefix")
    expected_size = be32(payload)
    if expected_size > maximum:
        raise RCCFormatError(f"zlib output size {expected_size} exceeds limit {maximum}")
    compressed = payload[4:]
    decoder = zlib.decompressobj(zlib.MAX_WBITS)
    try:
        output = decoder.decompress(compressed, maximum + 1)
        output += decoder.flush()
    except zlib.error as exc:
        raise RCCFormatError(f"zlib decompression failed: {exc}") from exc
    if len(output) > maximum:
        raise RCCFormatError(f"zlib output exceeds limit {maximum}")
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise RCCFormatError("zlib stream is truncated or has trailing/unconsumed bytes")
    if len(output) != expected_size:
        raise RCCFormatError(
            f"qCompress size prefix mismatch: expected {expected_size}, got {len(output)}"
        )
    return output, expected_size, compressed


def decompress_zstd(payload: bytes, maximum: int) -> bytes:
    try:
        import zstandard  # type: ignore

        output = zstandard.ZstdDecompressor().decompress(payload, max_output_size=maximum)
    except ImportError:
        try:
            from compression import zstd  # type: ignore

            output = zstd.decompress(payload)
        except ImportError as exc:
            raise RCCFormatError(
                "zstd resource found, but neither zstandard nor compression.zstd is installed"
            ) from exc
        except Exception as exc:
            raise RCCFormatError(f"zstd decompression failed: {exc}") from exc
    except Exception as exc:
        raise RCCFormatError(f"zstd decompression failed: {exc}") from exc
    if len(output) > maximum:
        raise RCCFormatError(f"zstd output exceeds limit {maximum}")
    return bytes(output)


def validate_component(name: str, node_index: int) -> None:
    if not name or name in {".", ".."}:
        raise RCCFormatError(f"node {node_index}: empty or traversal path component")
    if any(ch in name for ch in ("/", "\\", "\x00", ":")):
        raise RCCFormatError(f"node {node_index}: unsafe path component {name!r}")


class BundleParser:
    def __init__(
        self,
        image: PEImage,
        bundle: Dict[str, Any],
        version: int,
        max_nodes: int,
        max_stored_size: int,
        max_output_size: int,
    ):
        if version not in NODE_SIZE_BY_VERSION:
            raise RCCFormatError(f"unsupported Qt RCC format version {version}")
        self.image = image
        self.bundle = bundle
        self.version = version
        self.node_size = NODE_SIZE_BY_VERSION[version]
        self.max_nodes = max_nodes
        self.max_stored_size = max_stored_size
        self.max_output_size = max_output_size
        self.tree = image.map_va(int(bundle["tree_va"], 16))
        self.names = image.map_va(int(bundle["names_va"], 16))
        self.payloads = image.map_va(int(bundle["data_va"], 16))
        self.visited: Set[int] = set()
        self.directories: List[Dict[str, Any]] = []
        self.records: List[Dict[str, Any]] = []

    def read_node(self, index: int) -> Dict[str, Any]:
        if index < 0 or index >= self.max_nodes:
            raise RCCFormatError(f"node index {index} exceeds configured limit {self.max_nodes}")
        raw = self.image.read_at(
            self.tree, index * self.node_size, self.node_size, f"tree node {index}"
        )
        name_offset = be32(raw, 0)
        flags = be16(raw, 4)
        if flags & ~KNOWN_FLAGS:
            raise RCCFormatError(f"node {index}: unknown flag bits {flags & ~KNOWN_FLAGS:#x}")
        if (flags & FLAG_ZLIB) and (flags & FLAG_ZSTD):
            raise RCCFormatError(f"node {index}: both zlib and zstd flags are set")
        node: Dict[str, Any] = {
            "node_index": index,
            "node_raw_offset": self.tree.raw_offset + index * self.node_size,
            "name_offset": name_offset,
            "flags": flags,
            "flag_names": flag_names(flags),
            "last_modified": be64(raw, 14) if self.version >= 2 else None,
        }
        if flags & FLAG_DIRECTORY:
            if flags & (FLAG_ZLIB | FLAG_ZSTD):
                raise RCCFormatError(f"node {index}: directory has a compression flag")
            node["child_count"] = be32(raw, 6)
            node["first_child_index"] = be32(raw, 10)
        else:
            node["country"] = be16(raw, 6)
            node["language"] = be16(raw, 8)
            node["data_offset"] = be32(raw, 10)
        return node

    def read_name(self, node: Dict[str, Any]) -> Tuple[str, int, int]:
        offset = int(node["name_offset"])
        header = self.image.read_at(self.names, offset, 6, f"node {node['node_index']} name header")
        length = be16(header, 0)
        name_hash = be32(header, 2)
        encoded = self.image.read_at(
            self.names, offset + 6, length * 2, f"node {node['node_index']} UTF-16BE name"
        )
        try:
            name = encoded.decode("utf-16-be", errors="strict")
        except UnicodeDecodeError as exc:
            raise RCCFormatError(f"node {node['node_index']}: invalid UTF-16BE name") from exc
        validate_component(name, int(node["node_index"]))
        return name, length, name_hash

    def read_payload(self, node: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        relative = int(node["data_offset"])
        length_raw = self.image.read_at(
            self.payloads, relative, 4, f"node {node['node_index']} data length"
        )
        stored_size = be32(length_raw)
        if stored_size > self.max_stored_size:
            raise RCCFormatError(
                f"node {node['node_index']}: stored size {stored_size} exceeds limit {self.max_stored_size}"
            )
        stored = self.image.read_at(
            self.payloads, relative + 4, stored_size, f"node {node['node_index']} data payload"
        )
        flags = int(node["flags"])
        compression = "none"
        compressed_hash: Optional[str] = None
        compressed_stream_raw_offset: Optional[int] = None
        expected_output_size: Optional[int] = None
        if flags & FLAG_ZLIB:
            compression = "zlib"
            output, expected_output_size, compressed_bytes = decompress_zlib(
                stored, self.max_output_size
            )
            compressed_hash = sha256_bytes(compressed_bytes)
            compressed_stream_raw_offset = self.payloads.raw_offset + relative + 8
        elif flags & FLAG_ZSTD:
            compression = "zstd"
            output = decompress_zstd(stored, self.max_output_size)
            compressed_hash = sha256_bytes(stored)
            compressed_stream_raw_offset = self.payloads.raw_offset + relative + 4
        else:
            output = stored
            if len(output) > self.max_output_size:
                raise RCCFormatError(
                    f"node {node['node_index']}: output exceeds limit {self.max_output_size}"
                )
        metadata = {
            "data_offset": relative,
            "raw_data_offset": self.payloads.raw_offset + relative,
            "stored_payload_raw_offset": self.payloads.raw_offset + relative + 4,
            "compressed_stream_raw_offset": compressed_stream_raw_offset,
            "stored_size": stored_size,
            "decompressed_size": len(output),
            "qcompress_expected_size": expected_output_size,
            "compression": compression,
            "stored_sha256": sha256_bytes(stored),
            "compressed_sha256": compressed_hash,
            "decompressed_sha256": sha256_bytes(output),
        }
        return output, metadata

    def walk(self, index: int, parents: List[str], ancestors: Set[int]) -> None:
        if index in ancestors:
            raise RCCFormatError(f"tree cycle detected at node {index}")
        if index in self.visited:
            raise RCCFormatError(f"node {index} is referenced by more than one directory")
        self.visited.add(index)
        node = self.read_node(index)
        if index == 0:
            name = ""
            name_length = 0
            name_hash = 0
            parts = parents
        else:
            name, name_length, name_hash = self.read_name(node)
            parts = parents + [name]
        resource_path = ":/" + "/".join(parts)
        common = {
            "bundle_id": self.bundle["bundle_id"],
            "call_va": self.bundle["register_call_vas"][0],
            "register_call_vas": self.bundle["register_call_vas"],
            "unregister_call_vas": self.bundle["unregister_call_vas"],
            "tree_va": self.bundle["tree_va"],
            "names_va": self.bundle["names_va"],
            "data_va": self.bundle["data_va"],
            "tree_raw_offset": self.tree.raw_offset,
            "names_raw_offset": self.names.raw_offset,
            "data_base_raw_offset": self.payloads.raw_offset,
            "node_index": index,
            "node_raw_offset": node["node_raw_offset"],
            "name_offset": node["name_offset"],
            "name_record_raw_offset": self.names.raw_offset + int(node["name_offset"]),
            "name_length_utf16": name_length,
            "name_hash": name_hash,
            "resource_path": resource_path,
            "flags": node["flags"],
            "flag_names": node["flag_names"],
            "last_modified": {
                "msecs_since_epoch": node["last_modified"],
                "utc": timestamp_iso(int(node["last_modified"] or 0)),
            },
        }
        if int(node["flags"]) & FLAG_DIRECTORY:
            count = int(node["child_count"])
            first = int(node["first_child_index"])
            if count > self.max_nodes or first > self.max_nodes or first + count > self.max_nodes:
                raise RCCFormatError(
                    f"directory node {index}: invalid child range [{first}, {first + count})"
                )
            self.directories.append(
                {
                    **common,
                    "child_count": count,
                    "first_child_index": first,
                    "validation_status": "rcc_directory_valid",
                }
            )
            next_ancestors = ancestors | {index}
            for child_index in range(first, first + count):
                self.walk(child_index, parts, next_ancestors)
            return

        output, payload_metadata = self.read_payload(node)
        self.records.append(
            {
                **common,
                "locale": {"country": node["country"], "language": node["language"]},
                **payload_metadata,
                "validation_status": "rcc_valid_no_v2_match",
                "_payload": output,
            }
        )

    def parse(self) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
        root = self.read_node(0)
        if not (int(root["flags"]) & FLAG_DIRECTORY):
            raise RCCFormatError("root node 0 is not a directory")
        self.walk(0, [], set())
        if not self.visited:
            raise RCCFormatError("empty traversal")
        highest = max(self.visited)
        expected = set(range(highest + 1))
        if self.visited != expected:
            missing = sorted(expected - self.visited)[:20]
            raise RCCFormatError(f"tree node index gap/orphan before {highest}: {missing}")
        return self.records, self.directories


def load_v2_records(path: Optional[Path]) -> List[Dict[str, Any]]:
    if path is None:
        return []
    document = json.loads(path.read_text(encoding="utf-8"))
    records = document.get("records", [])
    if not isinstance(records, list):
        raise RCCFormatError("v2 zlib manifest has no records list")
    return records


def add_v2_alignment(record: Dict[str, Any], v2_records: List[Dict[str, Any]]) -> None:
    matches: List[Dict[str, Any]] = []
    for old in v2_records:
        compressed_match = bool(record.get("compressed_sha256")) and (
            record.get("compressed_sha256") == old.get("compressed_sha256")
        )
        decompressed_match = record.get("decompressed_sha256") == old.get("decompressed_sha256")
        if compressed_match and decompressed_match:
            claimed_path = normalize_qrc_path(str(old.get("claimed_path", "")))
            path_match = bool(claimed_path) and claimed_path == record["resource_path"]
            matches.append(
                {
                    "v2_raw_offset": old.get("raw_offset"),
                    "v2_output_file": old.get("output_file"),
                    "v2_claimed_path": old.get("claimed_path", ""),
                    "compressed_hash_match": True,
                    "decompressed_hash_match": True,
                    "resource_path_match": path_match,
                }
            )
    record["v2_alignment"] = matches
    if any(match["resource_path_match"] for match in matches):
        record["validation_status"] = "verified"
    elif matches:
        record["validation_status"] = "rcc_valid_v2_hash_match_path_mismatch"


def safe_output_path(output_root: Path, resource_path: str) -> Path:
    if not resource_path.startswith(":/"):
        raise RCCFormatError(f"invalid resource path {resource_path!r}")
    components = resource_path[2:].split("/")
    for index, component in enumerate(components):
        validate_component(component, index)
    target = output_root.joinpath(*components)
    root_resolved = output_root.resolve()
    target_parent = target.parent.resolve()
    if root_resolved != target_parent and root_resolved not in target_parent.parents:
        raise RCCFormatError(f"resource path escapes output root: {resource_path}")
    return target


def write_records(output_root: Path, records: List[Dict[str, Any]]) -> None:
    seen_casefold: Dict[str, Tuple[str, str]] = {}
    for record in records:
        payload = record.pop("_payload")
        target = safe_output_path(output_root, record["resource_path"])
        key = str(target).casefold()
        digest = record["decompressed_sha256"]
        if key in seen_casefold:
            old_path, old_digest = seen_casefold[key]
            if old_digest != digest:
                raise RCCFormatError(
                    f"different resources collide on output path: {old_path} / {record['resource_path']}"
                )
            record["output_collision"] = "same_content_deduplicated"
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(payload)
            seen_casefold[key] = (record["resource_path"], digest)
            record["output_collision"] = None
        record["output_file"] = str(target.resolve())


def version_from_calls(calls: Dict[str, Any], bundle_id: str) -> int:
    values: Set[int] = set()
    for candidate in calls.get("candidates", []):
        if candidate.get("bundle_id") != bundle_id or candidate.get("action") != "register":
            continue
        version = candidate.get("args", {}).get("version", {})
        if version.get("kind") == "immediate":
            values.add(int(version["integer_value"]))
    if len(values) != 1:
        raise RCCFormatError(f"{bundle_id}: expected one static registration version, got {values}")
    return values.pop()


def core_qml_results(records: Iterable[Dict[str, Any]]) -> List[Dict[str, Any]]:
    paths = sorted(str(record["resource_path"]) for record in records)
    results: List[Dict[str, Any]] = []
    for query in CORE_QML_QUERIES:
        exact = ":/" + query
        matches = [path for path in paths if path == exact or path.endswith("/" + query)]
        results.append({"query": query, "status": "found" if matches else "missing", "matches": matches})
    return results


def main() -> int:
    args = parse_args()
    input_exe = Path(args.input_exe).resolve()
    calls_path = Path(args.calls_json).resolve()
    output_root = Path(args.output_dir).resolve()
    manifest_path = Path(args.manifest).resolve()
    v2_path = Path(args.v2_zlib_manifest).resolve() if args.v2_zlib_manifest else None
    output_root.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)

    calls = json.loads(calls_path.read_text(encoding="utf-8"))
    image = PEImage(input_exe)
    v2_records = load_v2_records(v2_path)
    records: List[Dict[str, Any]] = []
    directories: List[Dict[str, Any]] = []
    bundle_results: List[Dict[str, Any]] = []

    for bundle in calls.get("bundles", []):
        result: Dict[str, Any] = {
            **bundle,
            "status": "failed",
            "error": None,
        }
        try:
            version = version_from_calls(calls, bundle["bundle_id"])
            parser = BundleParser(
                image=image,
                bundle=bundle,
                version=version,
                max_nodes=args.max_nodes,
                max_stored_size=args.max_stored_size,
                max_output_size=args.max_output_size,
            )
            bundle_records, bundle_directories = parser.parse()
            records.extend(bundle_records)
            directories.extend(bundle_directories)
            result.update(
                {
                    "status": "success",
                    "format_version": version,
                    "node_size": parser.node_size,
                    "tree_location": parser.tree.json(),
                    "names_location": parser.names.json(),
                    "data_location": parser.payloads.json(),
                    "node_count": len(bundle_records) + len(bundle_directories),
                    "directory_count": len(bundle_directories),
                    "file_count": len(bundle_records),
                }
            )
        except (RCCFormatError, KeyError, TypeError, ValueError) as exc:
            result["error"] = str(exc)
        bundle_results.append(result)

    for record in records:
        add_v2_alignment(record, v2_records)

    try:
        write_records(output_root, records)
    except RCCFormatError as exc:
        print(f"[rcc] output failure: {exc}", file=sys.stderr)
        return 3

    core_results = core_qml_results(records)
    successful = sum(bundle["status"] == "success" for bundle in bundle_results)
    failed = len(bundle_results) - successful
    manifest = {
        "status": "ok" if bundle_results and not failed else "partial" if successful else "failed",
        "manifest_version": "qt_rcc_v3",
        "format_basis": {
            "qt_version": "5.15",
            "byte_order": "big-endian",
            "node_size": "14 bytes plus 8-byte last_modified for version >= 2 (22 bytes in v3)",
            "flags": {"zlib": FLAG_ZLIB, "directory": FLAG_DIRECTORY, "zstd": FLAG_ZSTD},
        },
        "source_exe": str(input_exe),
        "source_exe_sha256": sha256_file(input_exe),
        "calls_json": str(calls_path),
        "calls_json_sha256": sha256_file(calls_path),
        "v2_zlib_manifest": str(v2_path) if v2_path else None,
        "output_root": str(output_root),
        "bundle_count": len(bundle_results),
        "successful_bundle_count": successful,
        "failed_bundle_count": failed,
        "record_count": len(records),
        "directory_count": len(directories),
        "verified_v2_path_and_hash_count": sum(
            record["validation_status"] == "verified" for record in records
        ),
        "bundles": bundle_results,
        "directories": directories,
        "core_qml_results": core_results,
        "records": records,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    print(
        f"[rcc] bundles={len(bundle_results)} success={successful} failed={failed} "
        f"files={len(records)} directories={len(directories)}"
    )
    print(
        f"[rcc] v2_path_and_hash_verified={manifest['verified_v2_path_and_hash_count']} "
        f"manifest={manifest_path}"
    )
    for item in core_results:
        print(f"[rcc] core {item['query']}: {item['status']} {item['matches']}")
    return 0 if bundle_results and not failed else 4


if __name__ == "__main__":
    raise SystemExit(main())
