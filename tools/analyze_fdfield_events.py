#!/usr/bin/env python3
"""Dump FDFIELD.DAT stage metadata/event tables.

当前用于逆向 stage 事件脚本，不修改原始文件。

已确认的 stage metadata(entry stage*3+1) 结构：
- 0x00: FDSHAP terrain shape_index
- 0x03..0x32: 16 条 3 字节阶段事件触发表，FUN_00017f5b @0x17f5b 使用
- 0x33..0x52: 16 条 2 字节 cell 事件查询表，FUN_0001118c @0x1118c 使用
- 0x53..0x82: 16 条 3 字节 cell 事件动作表，FUN_000111e7 @0x111e7 间接使用
- 0x83..end: 26 字节单位模板记录，字段仍在细分中

stage placement(entry stage*3+2) 结构：
- u16 count
- count 条 6 字节记录：u16 x, u16 y, u16 template_id
"""

from __future__ import annotations

import argparse
import struct
from collections import defaultdict
from pathlib import Path


def u16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def dat_entries(path: Path) -> list[bytes]:
    data = path.read_bytes()
    if data[:6] != b"LLLLLL":
        raise ValueError(f"{path} is not an LLLLLL DAT archive")
    offsets: list[int] = []
    pos = 6
    while pos + 4 <= len(data):
        off = struct.unpack_from("<I", data, pos)[0]
        if off >= len(data) or (offsets and off <= offsets[-1]):
            break
        offsets.append(off)
        pos += 4
    return [data[off:(offsets[i + 1] if i + 1 < len(offsets) else len(data))]
            for i, off in enumerate(offsets)]


def signed_byte(v: int) -> int:
    return v - 256 if v >= 128 else v


def parse_meta(meta: bytes) -> dict:
    if len(meta) < 0x83:
        raise ValueError(f"metadata too small: {len(meta)}")
    if (len(meta) - 0x83) % 26 != 0:
        raise ValueError(f"metadata unit template tail is not 26-byte aligned: {len(meta)}")
    return {
        "shape_index": meta[0],
        "unknown_01": meta[1],
        "unknown_02": meta[2],
        "turn_events": [tuple(meta[0x03 + i * 3:0x06 + i * 3]) for i in range(16)],
        "cell_lookup": [tuple(meta[0x33 + i * 2:0x35 + i * 2]) for i in range(16)],
        "cell_actions": [tuple(meta[0x53 + i * 3:0x56 + i * 3]) for i in range(16)],
        "unit_templates": [meta[0x83 + i * 26:0x83 + (i + 1) * 26]
                           for i in range((len(meta) - 0x83) // 26)],
    }


def parse_placements(blob: bytes) -> list[tuple[int, int, int]]:
    if len(blob) < 2:
        raise ValueError("placement entry too small")
    count = u16(blob, 0)
    if len(blob) != 2 + count * 6:
        raise ValueError(f"placement size mismatch: count={count} size={len(blob)}")
    return [(u16(blob, 2 + i * 6), u16(blob, 4 + i * 6), u16(blob, 6 + i * 6))
            for i in range(count)]


def match_templates(templates: list[bytes], placements: list[tuple[int, int, int]]) -> list[int | None]:
    by_id: dict[int, list[int]] = defaultdict(list)
    for i, rec in enumerate(templates):
        # 模板第 1 字节与 placement.template_id 在 stage 0/多数样本中对应。
        by_id[rec[1]].append(i)
    cursor: dict[int, int] = defaultdict(int)
    out: list[int | None] = []
    for _x, _y, tid in placements:
        ids = by_id.get(tid, [])
        cur = cursor[tid]
        if cur < len(ids):
            out.append(ids[cur])
            cursor[tid] += 1
        else:
            out.append(None)
    return out


def dump_stage(entries: list[bytes], stage: int) -> None:
    grid = entries[stage * 3]
    meta = entries[stage * 3 + 1]
    place_blob = entries[stage * 3 + 2]
    info = parse_meta(meta)
    placements = parse_placements(place_blob)
    matches = match_templates(info["unit_templates"], placements)

    w, h = u16(grid, 0), u16(grid, 2)
    print(f"stage {stage}")
    print(f"  grid: {w}x{h}, bytes={len(grid)}")
    print(f"  meta: bytes={len(meta)}, shape_index={info['shape_index']}, "
          f"unknown_01={info['unknown_01']}, unknown_02={info['unknown_02']}, "
          f"unit_templates={len(info['unit_templates'])}")
    print(f"  placements: bytes={len(place_blob)}, count={len(placements)}")

    print("\n  turn_events[16] @0x03: trigger, action, actor_or_side")
    for i, (trigger, action, actor) in enumerate(info["turn_events"]):
        if (trigger, action, actor) != (0xff, 0xff, 0):
            print(f"    {i:02d}: trigger={trigger:#04x} action={action:#04x} actor={actor:#04x}")

    print("\n  cell_lookup[16] @0x33: event_code, match_arg")
    for i, (code, arg) in enumerate(info["cell_lookup"]):
        if (code, arg) != (0xff, 0):
            print(f"    {i + 1:02d}: code={code:#04x} arg={arg:#04x}")

    print("\n  cell_actions[16] @0x53: mode, param_u16")
    for i, rec in enumerate(info["cell_actions"]):
        mode = rec[0]
        param = u16(bytes(rec), 1)
        if rec != (0, 0, 0):
            print(f"    {i:02d}: mode={mode:#04x} param={param:#06x}")

    print("\n  placements: index x y template_id -> template_index/template bytes")
    for i, ((x, y, tid), tix) in enumerate(zip(placements, matches)):
        if tix is None:
            print(f"    {i:02d}: ({x:02d},{y:02d}) id={tid:#04x} -> player/none")
        else:
            tpl = info["unit_templates"][tix]
            print(f"    {i:02d}: ({x:02d},{y:02d}) id={tid:#04x} -> tpl={tix:02d} "
                  f"bytes={tpl.hex(' ')}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--field", default="original_game/FDFIELD.DAT")
    ap.add_argument("--stage", type=int, default=0)
    args = ap.parse_args()
    entries = dat_entries(Path(args.field))
    if len(entries) % 3 != 0:
        raise ValueError(f"unexpected FDFIELD entry count: {len(entries)}")
    if args.stage < 0 or args.stage >= len(entries) // 3:
        raise ValueError(f"stage out of range: {args.stage}")
    dump_stage(entries, args.stage)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
