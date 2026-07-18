#!/usr/bin/env python3
"""生成 FD2 canonical Ghidra 的证据分层函数 seed manifest。

Capstone 对整个 object1 做线性解码得到的 direct-call targets 只进入 Tier D
候选账本；嵌入数据中的假指令不能作为入口证据。Tier A 的人工 anchor 只含已按
当前机器码和 xref 复核的少量入口。Tier C 记录
Watcom ``push imm32; call __chkstk`` wrapper/body 关系，body 默认只建 label。
Relocation-backed object1 target 只进入 deferred candidate ledger，不自动建函数。
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86_const import X86_INS_CALL, X86_OP_IMM

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CODE = ROOT / "tools" / "fd2_le_code0.bin"
DEFAULT_RELOC = ROOT / "tools" / "fd2_le_relocation_manifest.tsv"
DEFAULT_OUT = ROOT / "docs" / "generated" / "fd2-function-seeds.json"
SCHEMA = "fd2-function-seeds-v1"
OBJ1_BASE = 0x10000
OBJ1_SIZE = 0x3EF29
CHKSTK_OFF = 0x2702F

# 这些入口均已用当前 code0 字节、直接 call 或 relocation-backed dispatch 复核。
ANCHORS = (
    (0x2CCB4, "fd2_le_entry", "le-transfer-entry", None),
    (0x2702F, "__chkstk", "watcom-runtime-helper", None),
    (0x0F894, "title_action_menu", "confirmed-current-bytes-and-xrefs", "chkstk-wrapper"),
    (0x10421, "animation_play", "confirmed-current-bytes-and-xrefs", "chkstk-wrapper"),
    (0x15977, "music_track_play", "confirmed-current-bytes-and-xrefs", "chkstk-wrapper"),
    (0x15A96, "sfx_play", "confirmed-current-bytes-and-xrefs", "chkstk-wrapper"),
    (0x2231B, "new_game_opening_play", "relocation-backed-dispatch-and-current-bytes", "chkstk-wrapper"),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def prefix(data: bytes, offset: int, length: int = 16) -> str:
    return data[offset:offset + length].hex()


def linear_decode(data: bytes) -> tuple[list[dict[str, int]], set[int]]:
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    calls: list[dict[str, int]] = []
    decoded_starts: set[int] = set()
    for instruction in md.disasm(data, 0):
        decoded_starts.add(instruction.address)
        if (instruction.id == X86_INS_CALL and instruction.operands and
                instruction.operands[0].type == X86_OP_IMM):
            target = instruction.operands[0].imm & 0xFFFFFFFF
            if 0 <= target < len(data):
                calls.append({"callsite_offset": instruction.address,
                              "target_offset": target})
    return calls, decoded_starts


def chkstk_callers(data: bytes) -> list[dict[str, int]]:
    rows = []
    for callsite in range(5, len(data) - 5):
        if data[callsite] != 0xE8:
            continue
        relative = struct.unpack_from("<i", data, callsite + 1)[0]
        if callsite + 5 + relative != CHKSTK_OFF:
            continue
        if data[callsite - 5] != 0x68:
            raise ValueError(f"non-canonical __chkstk caller at code0 {callsite:#x}")
        frame_size = struct.unpack_from("<I", data, callsite - 4)[0]
        rows.append({
            "entry_offset": callsite - 5,
            "callsite_offset": callsite,
            "body_offset": callsite + 5,
            "frame_size": frame_size,
        })
    if len(rows) != 541:
        raise ValueError(f"expected 541 __chkstk callers, got {len(rows)}")
    return rows


def relocation_code_candidates(path: Path) -> list[dict[str, object]]:
    sources: dict[int, list[dict[str, object]]] = {}
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            if int(row["target_object"]) != 1:
                continue
            target_off = int(row["target_offset"], 16)
            source_va = int(row["source_relbase"], 16)
            sources.setdefault(target_off, []).append({
                "source_va": source_va,
                "record_file_offset": int(row["record_file_off"], 16),
                "source_object": (1 if 0x10000 <= source_va < 0x4EF29 else
                                  2 if 0x50000 <= source_va < 0x556B0 else 3),
            })
    return [
        {"object": 1, "offset": target, "va": OBJ1_BASE + target,
         "tier": "D", "kind": "relocation-code-candidate",
         "disposition": "deferred-needs-source-consumer-classification",
         "sources": sorted(rows, key=lambda r: (r["source_va"], r["record_file_offset"]))}
        for target, rows in sorted(sources.items())
        if 0 <= target < OBJ1_SIZE
    ]


def build(code_path: Path, relocation_path: Path) -> dict[str, object]:
    data = code_path.read_bytes()
    if len(data) != OBJ1_SIZE:
        raise ValueError(f"object1 size changed: {len(data):#x}")
    calls, decoded_starts = linear_decode(data)
    wrappers = chkstk_callers(data)

    direct_sources: dict[int, list[int]] = {}
    for row in calls:
        direct_sources.setdefault(row["target_offset"], []).append(row["callsite_offset"])
    seeds: dict[int, dict[str, object]] = {}
    for offset, name, evidence, entry_kind in ANCHORS:
        row = seeds.setdefault(offset, {
            "object": 1, "offset": offset, "va": OBJ1_BASE + offset,
            "tier": "A", "kind": "confirmed-entry", "source_vas": [],
            "disposition": "create-if-no-conflict",
        })
        row.update({
            "tier": "A", "kind": "confirmed-entry", "name": name,
            "evidence": evidence,
            "expected_prefix": prefix(data, offset),
            "expected_prefix_sha256": sha256(data[offset:offset + 16]),
        })
        if entry_kind:
            row["entry_kind"] = entry_kind
    wrapper_rows = []
    for row in wrappers:
        entry = row["entry_offset"]
        body = row["body_offset"]
        seed = seeds.setdefault(entry, {
            "object": 1, "offset": entry, "va": OBJ1_BASE + entry,
            "tier": "C", "kind": "chkstk-wrapper",
            "evidence": "push-imm32-direct-call-__chkstk",
            "source_vas": [], "disposition": "create-if-no-conflict",
            "expected_prefix": prefix(data, entry),
            "expected_prefix_sha256": sha256(data[entry:entry + 16]),
        })
        seed["entry_kind"] = "chkstk-wrapper"
        seed["body_va"] = OBJ1_BASE + body
        seed["frame_size"] = row["frame_size"]
        wrapper_rows.append({
            "entry_va": OBJ1_BASE + entry,
            "callsite_va": OBJ1_BASE + row["callsite_offset"],
            "body_va": OBJ1_BASE + body,
            "frame_size": row["frame_size"],
            "body_disposition": "label-only-unless-independent-entry-evidence",
        })

    return {
        "schema": SCHEMA,
        "object1": {"base_va": OBJ1_BASE, "size": OBJ1_SIZE,
                    "sha256": sha256(data)},
        "decoder": {"name": "capstone", "mode": "x86-32-linear",
                    "decoded_instruction_starts": len(decoded_starts),
                    "direct_call_count": len(calls),
                    "unique_direct_call_targets": len(direct_sources)},
        "seeds": [seeds[key] for key in sorted(seeds)],
        "direct_call_candidates": [
            {"object": 1, "offset": target, "va": OBJ1_BASE + target,
             "tier": "D", "kind": "linear-decode-direct-call-candidate",
             "evidence": "capstone-linear-decode-not-reachability-proof",
             "source_vas": [OBJ1_BASE + source for source in sorted(sources)],
             "expected_prefix": prefix(data, target),
             "expected_prefix_sha256": sha256(data[target:target + 16]),
             "disposition": "deferred-needs-reachable-callsite-proof"}
            for target, sources in sorted(direct_sources.items())
        ],
        "chkstk": {"helper_va": OBJ1_BASE + CHKSTK_OFF,
                   "direct_caller_count": len(wrapper_rows),
                   "wrappers": wrapper_rows},
        "relocation_code_candidates": relocation_code_candidates(relocation_path),
        "policy": {
            "body_labels_are_functions": False,
            "relocation_candidates_auto_create": False,
            "non_executable_objects_allow_functions": False,
            "legacy_markdown_is_seed_source": False,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--code", type=Path, default=DEFAULT_CODE)
    parser.add_argument("--relocations", type=Path, default=DEFAULT_RELOC)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    result = build(args.code.resolve(), args.relocations.resolve())
    payload = (json.dumps(result, ensure_ascii=False, sort_keys=True,
                          separators=(",", ":")) + "\n").encode("utf-8")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(payload)
    print(f"seeds={len(result['seeds'])}")
    print(f"chkstk_callers={result['chkstk']['direct_caller_count']}")
    print(f"relocation_code_candidates={len(result['relocation_code_candidates'])}")
    print(f"sha256={sha256(payload)}")
    print(f"wrote={args.out.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
