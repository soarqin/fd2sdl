#!/usr/bin/env python3
"""G1：在 ida-domain-scripting/run.py 提供的只读 db 上核实战斗读集。

依据 canonical LE relbase：51d01 的 28 项地图法术分派、1cff0/15311
玩家／AI caller、2ff01/2cf30/2d80d 演出与效果链、2ac7d 转职写集。
输出含原字节，仅写入本轮隔离目录，不作为可分发资源提交。
"""
import hashlib
import json
import os
from pathlib import Path
import struct
import sys


FUNCTIONS = (
    0x10010, 0x1088D, 0x10B4E, 0x1366A, 0x1A30B, 0x1A813, 0x1AA1D,
    0x1B653, 0x1B6B7, 0x34A1E, 0x4EB48, 0x205DA, 0x3231B,
    0x10C50, 0x112A5, 0x127E0, 0x14818, 0x149F8, 0x15311,
    0x18890, 0x18D8C, 0x1CA89, 0x1C269, 0x1C75E, 0x1C81F,
    0x1C916, 0x1C9DD, 0x1CFF0, 0x1D51D, 0x1D6C8,
    0x21227, 0x213B7, 0x214AD, 0x21548, 0x21B18,
    0x2218A, 0x22721, 0x22866, 0x22997, 0x22AA8, 0x22AF6,
    0x22CDA, 0x22D1B, 0x2AA00, 0x2AC7D, 0x2AE0E,
    0x2AEDB, 0x2CF30, 0x2D80D, 0x2FF01, 0x30E9D, 0x4E7DD,
    0x4E838, 0x4E821, 0x1E529,
)
TABLES = (
    ("promotion_profile_movement", 0x615FE, 36 * 2),
    ("promotion_item_by_original_id", 0x523D5, 18),
    ("revive_price_read_window", 0x52397, 29 * 2),
    ("magic_records", 0x619FD, 36 * 7),
    ("growth_records", 0x620A1, 68 * 11),
    ("magic_map_dispatch", 0x51D01, 28 * 4),
    ("movement_pointers", 0x627D8, 106 * 4),
    ("movement_script24_counted_bytes", 0x62BD0, 13),
    ("callback_index255_read_window", 0x51F8D, 4),
    ("constructor_id48_base_read_window", 0x62221, 24),
    ("constructor_id66_base_read_window", 0x623D1, 24),
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def collect(db, project: Path, executable: Path):
    sys.path.insert(0, str(project / "tools"))
    sys.path.insert(0, str(project / "tools" / "static_data"))
    from dump_fd2_fixup_table import collect_internal_offset_fixups
    from verify_unit_tables import EXE_SHA256, source_range

    original = executable.read_bytes()
    if sha(original) != EXE_SHA256:
        raise ValueError("FD2.EXE identity mismatch")
    fixups = collect_internal_offset_fixups(executable)
    result = {"schema": "fd2-g1-battle-readset-v1", "executable_sha256": sha(original),
              "project": str(project), "tables": {}, "functions": {}}
    magic_targets = []
    for name, va, length in TABLES:
        raw = source_range(original, va, length)
        expected = bytearray(raw)
        for source, (_, _, target) in fixups.items():
            if va <= source and source + 4 <= va + length:
                struct.pack_into("<I", expected, source - va, target)
        actual = bytes(db.bytes.get_bytes_at(va, size=length))
        if actual != expected:
            raise ValueError(f"{name}: strict LE/fixup and Domain bytes differ")
        if not all(db.bytes.is_value_initialized_at(va + i) for i in range(length)):
            raise ValueError(f"{name}: uninitialized bytes")
        row = {"canonical_va": hex(va), "length": length, "raw_sha256": sha(raw),
               "raw_hex": raw.hex(), "domain_sha256": sha(actual), "domain_hex": actual.hex(),
               "independent_source_match": True}
        row["internal_offset_fixups"] = [
            {"source_va": hex(source), "target_object": owner, "target_va": hex(target)}
            for source, (owner, _, target) in sorted(fixups.items())
            if va <= source and source + 4 <= va + length]
        if name == "magic_map_dispatch":
            magic_targets = list(struct.unpack("<28I", actual))
            row["targets"] = [hex(target) for target in magic_targets]
        result["tables"][name] = row
    for va in sorted(set(FUNCTIONS) | set(magic_targets)):
        function = db.functions.get_at(va)
        if function is None or function.start_ea != va:
            raise ValueError(f"{va:#x}: not a complete canonical function entry")
        chunks = []
        for chunk in db.functions.get_chunks(function):
            raw = bytes(db.bytes.get_bytes_at(chunk.start_ea, size=chunk.end_ea - chunk.start_ea))
            independent = source_range(original, chunk.start_ea, len(raw))
            expected = bytearray(independent)
            for source, (_, _, target) in fixups.items():
                if chunk.start_ea <= source and source + 4 <= chunk.end_ea:
                    struct.pack_into("<I", expected, source - chunk.start_ea, target)
            if raw != expected:
                raise ValueError(f"{chunk.start_ea:#x}: strict LE/fixup and Domain code differ")
            chunks.append({"va": hex(chunk.start_ea), "length": len(raw),
                           "sha256": sha(raw), "raw_sha256": sha(independent),
                           "independent_source_match": True, "bytes": raw.hex()})
        instructions = [{"va": hex(i.ea), "text": db.instructions.get_disassembly(i)}
                        for i in db.functions.get_instructions(function)]
        blocks = [{"start": hex(b.start_ea), "end": hex(b.end_ea),
                   "successors": [hex(s.start_ea) for s in b.get_successors()]}
                  for b in db.functions.get_flowchart(function)]
        if not chunks or not instructions or not blocks:
            raise ValueError(f"{va:#x}: incomplete function evidence")
        result["functions"][hex(va)] = {
            "name": db.functions.get_name(function), "end": hex(function.end_ea),
            "chunks": chunks,
            "disassembly": list(db.functions.get_disassembly(function, remove_tags=True)),
            "instructions": instructions, "blocks": blocks,
            "callers": [hex(f.start_ea) for f in db.functions.get_callers(function)],
            "callees": [hex(f.start_ea) for f in db.functions.get_callees(function)],
            "xref_to": [{"from": hex(x.from_ea), "type": x.type.name} for x in db.xrefs.to_ea(va)],
        }
    if executable.read_bytes() != original:
        raise ValueError("FD2.EXE changed during read-only evidence collection")
    return result


if __name__ == "__main__":
    project = Path(os.environ["FD2_G1_PROJECT"]).resolve()
    output = Path(os.environ["FD2_G1_BATTLE_OUTPUT"]).resolve()
    if output.exists():
        raise ValueError("refusing to overwrite existing evidence")
    readset = collect(db, project, Path(os.environ["FD2_G1_EXE"]))  # noqa: F821
    output.write_text(json.dumps(readset, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"tables": len(readset["tables"]), "functions": len(readset["functions"]),
                      "output": str(output), "sha256": sha(output.read_bytes())}))
