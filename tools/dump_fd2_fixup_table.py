#!/usr/bin/env python3
"""Dump relocation-backed pointer tables from FD2.EXE.

用途：解释反编译中的 `(*(code **)(DS_OFFSET + idx*4))()` 这类表。
脚本只读取 LE fixup 记录，不把 fixup 写回分析镜像。

示例：
  tools/dump_fd2_fixup_table.py --ds 0x1d71 --count 40
  tools/dump_fd2_fixup_table.py --ds 0x1b91 --count 32
  tools/dump_fd2_fixup_table.py --object 3 --ds 0x27d8 --count 96
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from rebuild_fd2_analysis import decode_fixup_record_end, parse_le, parse_objects, u32

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXE = ROOT / "original_game" / "FD2.EXE"

def object_target_offset(target_offset: int) -> int:
    """LE internal target offsets are already offsets within their object."""
    if target_offset < 0:
        raise ValueError(f"negative target offset: {target_offset:#x}")
    return target_offset


# Backward-compatible names for callers; both now preserve the real object offset.
fd2_analysis_offset = object_target_offset
object1_code0_offset = object_target_offset


def collect_internal_offset_fixups(exe: Path) -> dict[int, tuple[int, int, int]]:
    """Return source relbase -> (target_object, target_offset, target_linear)."""
    buf = exe.read_bytes()
    le = parse_le(buf)
    objects = parse_objects(buf, le)

    page_to_obj: dict[int, tuple[object, int]] = {}
    for obj in objects:
        for page_i in range(obj.page_count):
            page_to_obj[obj.page_map_index - 1 + page_i] = (obj, page_i)

    page_table = le.le_off + le.fixup_page_table_off
    rec_base = le.le_off + le.fixup_record_table_off
    out: dict[int, tuple[int, int, int]] = {}

    for page_index in range(le.num_pages):
        start = u32(buf, page_table + page_index * 4)
        end = u32(buf, page_table + (page_index + 1) * 4)
        p = rec_base + start
        limit = rec_base + end
        while p < limit:
            rec_end = decode_fixup_record_end(buf, p, limit)
            if rec_end is None:
                break
            rec = buf[p:rec_end]
            # FD2 中函数/数据表常见两种：
            #   07 00 <src:u16> <obj:u8> <off:u16>
            #   07 10 <src:u16> <obj:u8> <off:u32>
            if len(rec) in (7, 9) and rec[0] == 0x07 and 1 <= rec[4] <= len(objects):
                source_offset = struct.unpack_from("<H", rec, 2)[0]
                source_obj, source_page_i = page_to_obj.get(page_index, (objects[0], page_index))
                source_relbase = source_obj.relbase + source_page_i * le.page_size + source_offset
                target_object = rec[4]
                if rec[1] & 0x10:
                    target_offset = struct.unpack_from("<I", rec, 5)[0]
                else:
                    target_offset = struct.unpack_from("<H", rec, 5)[0]
                target_linear = objects[target_object - 1].relbase + target_offset
                out[source_relbase] = (target_object, target_offset, target_linear)
            p = rec_end
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    ap.add_argument("--ds", required=True, type=lambda s: int(s, 0),
                    help="DS offset of table base, e.g. 0x1d71")
    ap.add_argument("--count", type=lambda s: int(s, 0), default=16)
    ap.add_argument("--entry-size", type=lambda s: int(s, 0), default=4)
    ap.add_argument("--object", type=int, default=2,
                    help="source object that owns the offset; default 2 for normal DS tables, use 3 for object3 BSS tables")
    args = ap.parse_args()

    buf = args.exe.read_bytes()
    le = parse_le(buf)
    objects = parse_objects(buf, le)
    if args.object < 1 or args.object > len(objects):
        raise SystemExit(f"invalid --object {args.object}; valid range is 1..{len(objects)}")
    data_obj = objects[args.object - 1]
    table_linear = data_obj.relbase + args.ds
    fixups = collect_internal_offset_fixups(args.exe)

    print(f"obj{args.object} table @ {args.ds:#06x} (linear {table_linear:#08x}), count={args.count}")
    for i in range(args.count):
        source = table_linear + i * args.entry_size
        hit = fixups.get(source)
        if hit is None:
            print(f"{i:02d}: source={source:#08x} -> <no simple internal offset fixup>")
            continue
        target_object, target_offset, target_linear = hit
        # target_offset 是目标 object 内偏移。code0 对 object1 从 0 开始，
        # dual_clean 对 object1 加 relbase 0x10000；object3 raw 同理。
        extra = ""
        if target_object == 1:
            try:
                code0 = target_offset
                dual = objects[0].relbase + target_offset
                extra = f" code0={code0:#07x} dual={dual:#08x}"
            except ValueError:
                extra = " code0=<below-analysis-bias>"
        elif target_object == 3:
            try:
                data_offset = target_offset
                raw = objects[2].relbase + target_offset
                extra = f" object_offset={data_offset:#07x} raw={raw:#08x}"
            except ValueError:
                extra = " object_offset=<invalid>"
        print(f"{i:02d}: source={source:#08x} -> obj{target_object}:off={target_offset:#07x} "
              f"le_target_linear={target_linear:#08x}{extra}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
