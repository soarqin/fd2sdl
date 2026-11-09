#!/usr/bin/env python3
"""G2地图法术合同：独立原fixup/指令锚与已审完整wrapper投影。

不导入生成器或SDL数组；不能把本表用于玩家24专用FULL分支。
完整wrapper的顺序语义由G1 Domain读集及G2复核提供，本工具不声称
自动反编译证明所有控制流。原版运行与FIGANI marker另行验收。
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import struct
import sys

from verify_unit_tables import ROOT, EXE_SHA256, source_range
sys.path.insert(0, str(ROOT / "tools"))
from dump_fd2_fixup_table import collect_internal_offset_fixups

NAMES = ("MAGIC_MAP_EFFECT_IDS", "MAGIC_MAP_COST_IDS", "MAGIC_MAP_TARGET_LIMITS", "MAGIC_MAP_COST_BEFORE_CAST")


def check(document, executable):
    data = executable.read_bytes()
    source_range(data, 0x51d01, 28 * 4)  # strict identity/page validation before fixups
    fixups = collect_internal_offset_fixups(executable)
    targets = []
    for index in range(28):
        target = fixups.get(0x51d01 + index * 4)
        if not target or target[0] != 1:
            raise ValueError("magic dispatch has no confirmed code-object fixup")
        targets.append(target[2])
    if targets[24] != targets[16] or targets[24] != 0x22153:
        raise ValueError("24/16 original map wrapper alias differs")
    # Exact scalar/call anchors from the complete independently reviewed Domain functions.
    if source_range(data, 0x22183, 2) != bytes((0x6a, 0x10)):
        raise ValueError("24 wrapper does not push record16")
    def branch_target(va, opcode):
        raw = source_range(data, va, 5)
        if raw[0] != opcode: raise ValueError("original relative call/jump anchor differs")
        return va + 5 + struct.unpack_from("<i", raw, 1)[0]
    if branch_target(0x22185, 0xe9) != 0x21b0b or branch_target(0x21b0f, 0xe8) != 0x21b18:
        raise ValueError("24 shared healing wrapper path differs")
    if source_range(data, 0x226fe, 2) != bytes((0x6a, 0x12)) or branch_target(0x22704, 0xe8) != 0x1ca89:
        raise ValueError("17 original cost is not record18")
    if source_range(data, 0x21b5c, 1) != bytes((0x57,)) or branch_target(0x21b61, 0xe8) != 0x1ca89:
        raise ValueError("shared healing cost does not consume the selected record ID")
    if (source_range(data, 0x214e9, 3) != bytes((0x0f, 0xb6, 0x03)) or
            branch_target(0x214ed, 0xe8) != 0x1c75e or
            source_range(data, 0x22199, 3) != bytes((0x0f, 0xb6, 0x03))):
        raise ValueError("9/23 first raw target anchors differ")
    if executable.read_bytes() != data:
        raise ValueError("EXE changed during independent projection verification")
    effects, costs, limits = list(range(28)), list(range(28)), [0] * 28
    effects[24] = costs[24] = 16; costs[17] = 18; limits[9] = limits[23] = 1
    before = [int(index in range(9, 13) or index in range(17, 24) or index in range(25, 28)) for index in range(28)]
    by_name = {t["name"]: t for t in document["tables"]}
    for name, values in zip(NAMES, (effects, costs, limits, before)):
        table = by_name[name]
        if (table.get("c_type"), table.get("dimensions"), table.get("values")) != ("uint8_t", [28], values):
            raise ValueError(f"{name}: reviewed original map projection differs")
        source = table.get("source", {})
        if (source.get("kind") != "decoded_dispatch_projection" or source.get("file") != "FD2.EXE" or
                source.get("file_sha256") != EXE_SHA256 or source.get("canonical_dispatch_va") != "0x51d01" or
                not source.get("evidence")):
            raise ValueError("map projection source identity/basis differs")
    return {"ok": True, "tables": 4, "dispatch_fixups": 28, "executable_sha256": EXE_SHA256,
            "scope": "map aliases, scalar/call/jump anchors and independently reviewed wrapper-order projections",
            "original_runtime_executed": False, "all_wrapper_control_flow_automatically_proven": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--tables", type=Path, default=ROOT / "data/static/tables/magic.json")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    document = json.loads(args.tables.read_text(encoding="utf-8"))
    report = check(document, args.executable)
    if args.self_test:
        rejected = 0
        for name in NAMES:
            bad = copy.deepcopy(document)
            next(t for t in bad["tables"] if t["name"] == name)["values"][24] ^= 1
            try: check(bad, args.executable)
            except ValueError: rejected += 1
            else: raise ValueError("changed map contract was accepted")
        report["negative_cases_rejected"] = rejected
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
