#!/usr/bin/env python3
"""只读审计sub_4E31C水DAC表：strict LE canonical读取与C常量逐字节比对。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image, page_number_from_map_entry
from dump_fd2_fixup_table import collect_internal_offset_fixups

EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
WATER_VA = 0x60003
READ_SIZE = 93
READ_SHA256 = "ff85b2a737b4e838d24fba73331de7d52815f1f87f6d940f339e5d1fd6e74198"
PERIOD_SHA256 = "6f54f27804a3897b8d558f0693d8d8268299f458f42a8fb0666fa2bbfa2dc381"
STATE_REFERENCES = {
    0x60000: {0x4e326, 0x4e37a},
    0x60002: {0x4e332, 0x4e338, 0x4e341, 0x4e349},
    0x60003: {0x4e353},
}


def water_table(executable):
    path = Path(executable)
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXE_SHA256:
        raise ValueError("FD2.EXE不是已确认版本；未解析未知LE或分配其声明大小")
    le = parse_le(data)
    objects = parse_objects(data, le)
    owners = [obj for obj in objects if obj.relbase <= WATER_VA and WATER_VA + READ_SIZE <= obj.relbase + obj.vsize]
    if len(owners) != 1 or owners[0].index != 3:
        raise ValueError("水色表不在唯一已确认object3 virtual范围")
    owner = owners[0]
    first = (WATER_VA - owner.relbase) // le.page_size
    last = (WATER_VA + READ_SIZE - 1 - owner.relbase) // le.page_size
    pages = []
    for page in range(first, last + 1):
        index = owner.page_map_index - 1 + page
        at = le.le_off + le.objmap_off + index * 4
        entry = data[at:at + 4]
        if not 0 <= page < owner.page_count or not 0 <= index < le.num_pages or len(entry) != 4 or entry[3] != 0:
            raise ValueError("水色表不是已初始化LE文件页")
        number = page_number_from_map_entry(entry)
        if not 1 <= number <= le.num_pages:
            raise ValueError("水色表不能从zero/BSS或无效物理页读取")
        offset = (WATER_VA - owner.relbase) % le.page_size if page == first else 0
        file_offset = le.module_file_off + le.data_pages_off + (number - 1) * le.page_size + offset
        pages.append({"object_page": page, "page_number": number, "page_flags": entry[3], "file_offset": file_offset})
    raw = build_relbase_image(data, le, objects)
    table = bytes(raw[WATER_VA:WATER_VA + READ_SIZE])
    if len(table) != READ_SIZE or hashlib.sha256(table).hexdigest() != READ_SHA256:
        raise ValueError("canonical水色有效读集93字节SHA256不符")
    period = table[:48]
    if hashlib.sha256(period).hexdigest() != PERIOD_SHA256 or table[48:] != period[:45] or max(table) > 63:
        raise ValueError("水色表不是已确认16 RGB6重复周期")
    if raw[0x60000:0x60003] != b"\0\0\0":
        raise ValueError("水色全局初始last_tick/phase不是0")
    fixups = collect_internal_offset_fixups(path)
    references = {target: {source for source, value in fixups.items() if value[2] == target}
                  for target in STATE_REFERENCES}
    if references != STATE_REFERENCES:
        raise ValueError("水色state/table的引用集变化；必须重新审Load生命周期")
    return period, {"source": "parse_le/parse_objects/build_relbase_image raw-unrelocated",
        "executable_sha256": digest, "canonical_va": WATER_VA, "read_size": READ_SIZE,
        "read_sha256": READ_SHA256, "period_size": 48, "period_sha256": PERIOD_SHA256,
        "repeat_tail_matches_first_15_rgb": True, "initial_state_hex": "000000",
        "object": owner.index, "object_offset": WATER_VA - owner.relbase, "pages": pages,
        "state_fixup_sources": {hex(target): [hex(source) for source in sorted(sources)]
                                for target, sources in references.items()},
        "rgb6": [list(period[i:i + 3]) for i in range(0, 48, 3)]}


def audit_c_table(source, period):
    text = Path(source).read_text(encoding="utf-8")
    match = re.search(r"field_water_rgb6\[16\]\[3\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not match:
        raise ValueError("C源码缺少精确16×RGB6水色常量")
    rows = re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}", match.group(1))
    if len(rows) != 16:
        raise ValueError("C水色常量行数无效")
    values = [int(value) for row in rows for value in row]
    if any(value > 63 for value in values) or bytes(values) != period:
        raise ValueError("C水色常量与canonical LE表逐字节不符")
    # 所有16个窗口也逐字节检查，不只比单个stage0颜色或表大小。
    for phase in range(16):
        actual = bytes(values[((phase + i) & 15) * 3 + channel] for i in range(16) for channel in range(3))
        expected = (period + period)[phase * 3:phase * 3 + 48]
        if actual != expected:
            raise ValueError("水色窗口循环等价检查失败")
    return {"c_source": str(Path(source).resolve()), "source_sha256": hashlib.sha256(Path(source).read_bytes()).hexdigest(),
            "table_bytes_compared": 48, "all_16_windows_compared": True, "ok": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "original_game/FD2.EXE")
    parser.add_argument("--c-source", type=Path, default=ROOT / "src/field_visual.c")
    parser.add_argument("--emit-c", action="store_true", help="只输出可再生产16×RGB6 initializer")
    args = parser.parse_args()
    try:
        period, evidence = water_table(args.executable)
        if args.emit_c:
            print("\n".join("    {" + ",".join(str(value) for value in period[i:i + 3]) + "}," for i in range(0, 48, 3)))
        else:
            print(json.dumps({**evidence, "c_audit": audit_c_table(args.c_source, period)}, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError) as error:
        print(f"水色LE审计未通过：{error}。检查来源版本与C表后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
