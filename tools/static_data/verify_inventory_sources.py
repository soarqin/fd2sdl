#!/usr/bin/env python3
"""G2正常库存来源域：独立原资源，不读取SDL生产表或修改原文件。

仅确认初始记录密集性、合法ID和原物品分派域；事件/服务操作的保持性与
任意外部SAV兼容性必须另核，不由初始数据通过推导。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

from audit_stage_data import archive_entries, parse_metadata, SOURCE_HASHES
from verify_unit_tables import ROOT, source_range


def check_inventory(ids, occupied):
    if len(ids) != 8 or len(occupied) != 8:
        raise ValueError("inventory projection must have eight raw slots")
    count = sum(occupied)
    if occupied != [index < count for index in range(8)]:
        raise ValueError("source inventory is not a dense prefix")
    if any(used and not 0 <= item < 215 for item, used in zip(ids, occupied)):
        raise ValueError("occupied source item has no original record")


def verify(executable, field):
    if hashlib.sha256(field).hexdigest() != SOURCE_HASHES["FDFIELD.DAT"]:
        raise ValueError("FDFIELD identity mismatch")
    base = source_range(executable, 0x61da1, 32 * 24)
    items = source_range(executable, 0x602ad, 215 * 23)
    # sub_112A5: two equipment entries, four optional items, two hidden tail flags.
    for index in range(32):
        row = base[index * 24:(index + 1) * 24]
        ids = list(row[12:18]) + [255, 255]
        check_inventory(ids, [True, True] + [item != 255 for item in row[14:18]] + [False, False])
    entries = archive_entries(field)
    if len(entries) != 99:
        raise ValueError("expected all 33 stage groups")
    templates, tails = 0, 0
    for stage in range(33):
        raw = entries[stage * 3 + 1][1]
        metadata = parse_metadata(raw)
        effective = raw[2]
        if effective > len(metadata["templates"]):
            raise ValueError("effective template prefix exceeds raw metadata")
        tails += len(metadata["templates"]) - effective
        for row in metadata["templates"][:effective]:
            # 10c50's special first two equipment slots, then six optional raw items.
            ids = ([row[5], row[6]] if row[5] != 255 else [row[6], 255]) + list(row[7:13])
            occupied = [True, row[5] != 255] + [item != 255 for item in row[7:13]]
            check_inventory(ids, occupied)
            templates += 1
    codes = Counter(items[index * 23 + 13] for index in range(215))
    if set(codes) - {0, *range(5, 25)}:
        raise ValueError("new original Use dispatcher code requires semantic review")
    magic_refs = []
    for index in range(215):
        row = items[index * 23:(index + 1) * 23]
        if row[13] in (20, 21, 24):
            magic = row[14] | row[15] << 8
            if magic > 9:
                raise ValueError("new item-to-magic source requires effect review")
            magic_refs.append({"item_id": index, "code": row[13], "magic_id": magic})
    return {"ok": True, "scope": "original normal initial inventory and item dispatcher sources",
            "exe_sha256": SOURCE_HASHES["FD2.EXE"], "field_sha256": SOURCE_HASHES["FDFIELD.DAT"],
            "player_base_records": 32, "effective_templates": templates, "excluded_raw_tails": tails,
            "sparse_initial_inventories": 0, "occupied_unknown_items": 0,
            "item_records": 215, "use_code_counts": dict(sorted(codes.items())),
            "item_magic_references": magic_refs, "source_operations_preservation_proven": False,
            "arbitrary_external_save_domain_proven": False, "original_runtime_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = verify((args.original / "FD2.EXE").read_bytes(), (args.original / "FDFIELD.DAT").read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
