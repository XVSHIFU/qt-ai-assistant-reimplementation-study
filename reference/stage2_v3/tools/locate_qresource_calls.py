#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Locate Qt resource registration calls and recover MinGW stack-slot arguments."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_ESP, X86_REG_INVALID


SYMBOLS = {
    "_Z21qRegisterResourceDataiPKhS0_S0_": "register",
    "_Z23qUnregisterResourceDataiPKhS0_S0_": "unregister",
}
ARG_BY_STACK_OFFSET = {0: "version", 4: "tree", 8: "names", 12: "data"}
CONTROL_FLOW_PREFIXES = ("j", "loop")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Locate x86 Qt RCC register/unregister calls and decode arguments."
    )
    parser.add_argument("--input-exe", required=True, help="Input 32-bit PE executable")
    parser.add_argument("--output-json", required=True, help="Output candidate JSON")
    return parser.parse_args()


def _section_map(pe: pefile.PE) -> List[Dict[str, int | str]]:
    result: List[Dict[str, int | str]] = []
    for sec in pe.sections:
        result.append(
            {
                "name": sec.Name.decode("utf-8", errors="ignore").rstrip("\x00"),
                "rva": int(sec.VirtualAddress),
                "virtual_size": int(sec.Misc_VirtualSize),
                "raw_size": int(sec.SizeOfRawData),
                "raw_offset": int(sec.PointerToRawData),
            }
        )
    return result


def _classify_va(pe: pefile.PE, va: int) -> Dict[str, Any]:
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    rva = va - image_base
    for sec in _section_map(pe):
        start = int(sec["rva"])
        raw_size = int(sec["raw_size"])
        virtual_size = int(sec["virtual_size"])
        if start <= rva < start + max(raw_size, virtual_size):
            delta = rva - start
            raw_offset = int(sec["raw_offset"]) + delta if delta < raw_size else None
            return {
                "virtual_address": hex(va),
                "rva": hex(rva),
                "section_name": sec["name"],
                "raw_offset": raw_offset,
                "raw_backed": raw_offset is not None,
            }
    return {
        "virtual_address": hex(va),
        "rva": hex(rva) if rva >= 0 else None,
        "section_name": None,
        "raw_offset": None,
        "raw_backed": False,
    }


def _normalize_import_va(pe: pefile.PE, address: int) -> int:
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    image_end = image_base + int(pe.OPTIONAL_HEADER.SizeOfImage)
    if image_base <= address < image_end:
        return address
    return image_base + address


def _disassemble(pe: pefile.PE) -> List[Dict[str, Any]]:
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    decoded: List[Dict[str, Any]] = []
    for sec in pe.sections:
        if not (int(sec.Characteristics) & 0x20000000) or not int(sec.SizeOfRawData):
            continue
        sec_name = sec.Name.decode("utf-8", errors="ignore").rstrip("\x00")
        start = int(sec.PointerToRawData)
        raw = pe.__data__[start : start + int(sec.SizeOfRawData)]
        for ins in md.disasm(raw, image_base + int(sec.VirtualAddress)):
            operands: List[Dict[str, Any]] = []
            for op in ins.operands:
                item: Dict[str, Any] = {"type": int(op.type)}
                if op.type == X86_OP_IMM:
                    item["imm"] = int(op.imm)
                elif op.type == X86_OP_REG:
                    item["reg"] = int(op.reg)
                    item["reg_name"] = ins.reg_name(op.reg)
                elif op.type == X86_OP_MEM:
                    item.update(
                        {
                            "base": int(op.mem.base),
                            "index": int(op.mem.index),
                            "scale": int(op.mem.scale),
                            "disp": int(op.mem.disp),
                            "segment": int(op.mem.segment),
                        }
                    )
                operands.append(item)
            decoded.append(
                {
                    "va": int(ins.address),
                    "size": int(ins.size),
                    "mnemonic": ins.mnemonic,
                    "op_str": ins.op_str,
                    "bytes": ins.bytes.hex(),
                    "section": sec_name,
                    "operands": operands,
                }
            )
    return decoded


def _absolute_iat_operand(ins: Dict[str, Any], iat_vas: Set[int]) -> Optional[int]:
    ops = ins.get("operands", [])
    if len(ops) != 1 or ops[0].get("type") != X86_OP_MEM:
        return None
    op = ops[0]
    if op.get("base") == X86_REG_INVALID and op.get("index") == X86_REG_INVALID:
        disp = int(op.get("disp", 0))
        if disp in iat_vas:
            return disp
    return None


def _is_block_boundary(ins: Dict[str, Any]) -> bool:
    mnemonic = str(ins["mnemonic"]).lower()
    return mnemonic in {"call", "ret", "retf", "iret", "int", "int3"} or mnemonic.startswith(
        CONTROL_FLOW_PREFIXES
    )


def _decode_stack_args(
    pe: pefile.PE, instructions: List[Dict[str, Any]], call_index: int
) -> Tuple[Dict[str, Any], str, List[Dict[str, Any]]]:
    found: Dict[int, Dict[str, Any]] = {}
    sources: List[Dict[str, Any]] = []
    call = instructions[call_index]
    for index in range(call_index - 1, -1, -1):
        ins = instructions[index]
        if ins["section"] != call["section"] or _is_block_boundary(ins):
            break
        ops = ins.get("operands", [])
        if ins["mnemonic"] != "mov" or len(ops) != 2:
            continue
        dst, src = ops
        if (
            dst.get("type") != X86_OP_MEM
            or dst.get("base") != X86_REG_ESP
            or dst.get("index") != X86_REG_INVALID
            or int(dst.get("disp", -1)) not in ARG_BY_STACK_OFFSET
        ):
            continue
        slot = int(dst["disp"])
        if slot in found:
            continue
        source = {
            "instruction_va": hex(ins["va"]),
            "instruction": f'{ins["mnemonic"]} {ins["op_str"]}',
            "stack_offset": slot,
        }
        if src.get("type") == X86_OP_IMM:
            value = int(src["imm"])
            source["kind"] = "immediate"
            source["integer_value"] = value
            if slot:
                source["value"] = _classify_va(pe, value)
        elif src.get("type") == X86_OP_REG:
            source["kind"] = "register"
            source["register"] = src.get("reg_name")
        else:
            source["kind"] = "unsupported"
        found[slot] = source
        sources.append(source)
        if len(found) == 4:
            break

    args = {ARG_BY_STACK_OFFSET[o]: found.get(o, {"kind": "missing", "stack_offset": o}) for o in (0, 4, 8, 12)}
    if all(args[name].get("kind") == "immediate" for name in ("version", "tree", "names", "data")):
        status = "complete"
    elif all(args[name].get("kind") == "immediate" for name in ("tree", "names", "data")):
        status = "arrays_complete_version_dynamic"
    else:
        status = "incomplete"
    return args, status, list(reversed(sources))


def _arg_va(args: Dict[str, Any], name: str) -> Optional[str]:
    arg = args.get(name, {})
    value = arg.get("value")
    return value.get("virtual_address") if isinstance(value, dict) else None


def main() -> int:
    args_ns = parse_args()
    input_exe = Path(args_ns.input_exe).resolve()
    output_json = Path(args_ns.output_json).resolve()
    output_json.parent.mkdir(parents=True, exist_ok=True)

    pe = pefile.PE(str(input_exe), fast_load=False)
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    imports: Dict[int, Dict[str, str]] = {}
    for descriptor in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        dll = descriptor.dll.decode("utf-8", errors="replace")
        for imported in descriptor.imports:
            name = imported.name.decode("utf-8", errors="replace") if imported.name else ""
            if name in SYMBOLS:
                va = _normalize_import_va(pe, int(imported.address))
                imports[va] = {"symbol": name, "action": SYMBOLS[name], "dll": dll}

    instructions = _disassemble(pe)
    iat_vas = set(imports)
    thunks: Dict[int, int] = {}
    for ins in instructions:
        if ins["mnemonic"] == "jmp":
            iat_va = _absolute_iat_operand(ins, iat_vas)
            if iat_va is not None:
                thunks[int(ins["va"])] = iat_va

    candidates: List[Dict[str, Any]] = []
    for index, ins in enumerate(instructions):
        if ins["mnemonic"] != "call":
            continue
        target_iat: Optional[int] = _absolute_iat_operand(ins, iat_vas)
        target_va: Optional[int] = target_iat
        call_target_type = "call_mem_iat" if target_iat is not None else None
        ops = ins.get("operands", [])
        if target_iat is None and len(ops) == 1 and ops[0].get("type") == X86_OP_IMM:
            immediate = int(ops[0]["imm"])
            if immediate in thunks:
                target_va = immediate
                target_iat = thunks[immediate]
                call_target_type = "call_thunk"
        if target_iat is None or call_target_type is None:
            continue

        decoded_args, parse_status, stack_sources = _decode_stack_args(pe, instructions, index)
        import_info = imports[target_iat]
        context_start = max(0, index - 12)
        context_end = min(len(instructions), index + 9)
        context = [
            {
                "va": hex(item["va"]),
                "mnemonic": item["mnemonic"],
                "op_str": item["op_str"],
                "section": item["section"],
                "bytes": item["bytes"],
            }
            for item in instructions[context_start:context_end]
        ]
        candidates.append(
            {
                "candidate_id": len(candidates),
                "action": import_info["action"],
                "call_va": hex(ins["va"]),
                "call_location": _classify_va(pe, int(ins["va"])),
                "call_target_type": call_target_type,
                "call_target_va": hex(int(target_va)),
                "target_iat_va": hex(target_iat),
                "target_import_name": import_info["symbol"],
                "target_import_dll": import_info["dll"],
                "parse_status": parse_status,
                "args": decoded_args,
                "stack_write_sources": stack_sources,
                "context_12_before_8_after": context,
            }
        )

    triples = sorted(
        {
            (_arg_va(c["args"], "tree"), _arg_va(c["args"], "names"), _arg_va(c["args"], "data"))
            for c in candidates
            if all(_arg_va(c["args"], name) for name in ("tree", "names", "data"))
        },
        key=lambda t: int(t[0], 16),
    )
    bundle_by_triple = {triple: f"bundle_{idx:03d}" for idx, triple in enumerate(triples)}
    seen_register: Set[str] = set()
    for candidate in candidates:
        triple = tuple(_arg_va(candidate["args"], n) for n in ("tree", "names", "data"))
        bundle_id = bundle_by_triple.get(triple)  # type: ignore[arg-type]
        candidate["bundle_id"] = bundle_id
        duplicate = candidate["action"] == "register" and bundle_id in seen_register
        candidate["is_duplicate_registration"] = bool(duplicate)
        if candidate["action"] == "register" and bundle_id:
            seen_register.add(bundle_id)

    bundles: List[Dict[str, Any]] = []
    for triple in triples:
        bundle_id = bundle_by_triple[triple]
        related = [c for c in candidates if c.get("bundle_id") == bundle_id]
        bundles.append(
            {
                "bundle_id": bundle_id,
                "tree_va": triple[0],
                "names_va": triple[1],
                "data_va": triple[2],
                "register_call_vas": [c["call_va"] for c in related if c["action"] == "register"],
                "unregister_call_vas": [c["call_va"] for c in related if c["action"] == "unregister"],
            }
        )

    action_counts = {
        action: sum(1 for c in candidates if c["action"] == action)
        for action in ("register", "unregister")
    }
    report = {
        "status": "ok" if bundles else "failed",
        "format_version": 3,
        "input_exe": str(input_exe),
        "image_base": hex(image_base),
        "imports": [
            {"iat_va": hex(va), **info, "location": _classify_va(pe, va)}
            for va, info in sorted(imports.items())
        ],
        "analysis": {
            "sections_scanned": sorted({i["section"] for i in instructions}),
            "total_instructions_scanned": len(instructions),
            "argument_pattern": "mov dword ptr [esp+0/4/8/0xc], immediate in the same basic block",
            "calls_found": len(candidates),
            "action_counts": action_counts,
            "unique_bundles": len(bundles),
            "duplicate_registrations": sum(bool(c["is_duplicate_registration"]) for c in candidates),
        },
        "bundles": bundles,
        "candidates": candidates,
    }
    output_json.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(
        f"[qresource] calls={len(candidates)} register={action_counts['register']} "
        f"unregister={action_counts['unregister']} bundles={len(bundles)}"
    )
    print(f"[qresource] report={output_json}")
    return 0 if bundles else 2


if __name__ == "__main__":
    raise SystemExit(main())
