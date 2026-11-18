#!/usr/bin/env python3
"""SD1 单位／物品／法术／移动冷表的独立原字节对照，不写原资源。

依据现有 getter 的 canonical VA 和严格 LE 页映射；不导入生成器，
不把 JSON 自报的地址、摘要或尺寸用作预期值，不应用 fixup。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from rebuild_fd2_analysis import parse_le, parse_objects, page_number_from_map_entry

EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
# 名称、类型、原始维度及VA独立固定；不能由待验JSON反向决定读集。
TABLES = {
    "unit_base": (
        ("k_player_base_records", "uint8_t", (32, 24), 0x61DA1),
        ("k_enemy_records", "uint8_t", (68, 10), 0x61AF9),
        ("k_growth_records", "uint8_t", (68, 11), 0x620A1),
        ("k_learning_records", "uint8_t", (20, 12), 0x626B3),
    ),
    "items": (
        ("ITEM_RECORDS", "uint8_t", (215, 23), 0x602AD),
        ("profiles", "uint8_t", (29, 7), 0x6188A),
    ),
    "magic": (
        ("MAGIC_RECORDS", "uint8_t", (36, 7), 0x619FD),
        ("MAGIC_DAMAGE_PROFILE_SCALES", "uint32_t", (28,), 0x51F96),
    ),
    "movement": (("k_profiles", "uint8_t", (29, 20), 0x61646),),
}
MAGIC_CONTRACT_TABLES = ("MAGIC_MAP_EFFECT_IDS", "MAGIC_MAP_COST_IDS", "MAGIC_MAP_TARGET_LIMITS", "MAGIC_MAP_COST_BEFORE_CAST")


def source_range(data: bytes, va: int, size: int) -> bytes:
    """只从完整、已初始化的LE文件页取连续canonical读集。"""
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("FD2.EXE 身份不符；拒绝套用当前已确认读集")
    le = parse_le(data)
    objects = parse_objects(data, le)
    owners = [o for o in objects if o.relbase <= va and va + size <= o.relbase + o.vsize]
    if len(owners) != 1 or size <= 0 or le.page_size <= 0:
        raise ValueError("读集不在唯一LE对象的完整范围内")
    obj = owners[0]
    result = bytearray()
    offset = va - obj.relbase
    while len(result) < size:
        page, within = divmod(offset, le.page_size)
        if not 0 <= page < obj.page_count:
            raise ValueError("读集越出文件映射页，不能把BSS当静态表")
        index = obj.page_map_index - 1 + page
        if not 0 <= index < le.num_pages:
            raise ValueError("LE页目录越界")
        at = le.le_off + le.objmap_off + index * 4
        entry = data[at:at + 4]
        if len(entry) != 4 or entry[3] != 0:
            raise ValueError("无效或未支持的LE文件页")
        number = page_number_from_map_entry(entry)
        if not 1 <= number <= le.num_pages:
            raise ValueError("读集必须来自已初始化文件页")
        amount = min(size - len(result), le.page_size - within)
        initialized = le.last_page_size if number == le.num_pages and le.last_page_size else le.page_size
        start = le.module_file_off + le.data_pages_off + (number - 1) * le.page_size + within
        if within + amount > initialized or start < 0 or start + amount > len(data):
            raise ValueError("LE源页截断")
        result.extend(data[start:start + amount])
        offset += amount
    return bytes(result)


def check_table(table: dict, spec: tuple, expected: bytes) -> None:
    name, c_type, dimensions, _ = spec
    if (table.get("name"), table.get("c_type"), table.get("dimensions")) != (name, c_type, list(dimensions)):
        raise ValueError(f"{name}: 名称／类型／维度不符")

    def flatten(value, dims):
        if not dims:
            limit = 255 if c_type == "uint8_t" else 0xFFFFFFFF
            if type(value) is not int or not 0 <= value <= limit:
                raise ValueError(f"{name}: 非法整数字段")
            return [value]
        if not isinstance(value, list) or len(value) != dims[0]:
            raise ValueError(f"{name}: 记录尺寸不符")
        return [number for child in value for number in flatten(child, dims[1:])]

    numbers = flatten(table.get("values"), dimensions)
    actual = bytes(numbers) if c_type == "uint8_t" else struct.pack(f"<{len(numbers)}I", *numbers)
    if actual != expected:
        raise ValueError(f"{name}: 原始字段／未知字节与原版不符")


def verify(executable: Path, tables_dir: Path = ROOT / "data" / "static" / "tables") -> dict:
    data = executable.read_bytes()
    checked = total = 0
    for stem, specs in TABLES.items():
        document = json.loads((tables_dir / f"{stem}.json").read_text(encoding="utf-8"))
        tables = document.get("tables")
        expected_count = len(specs) + (1 if stem == "unit_base" else len(MAGIC_CONTRACT_TABLES) if stem == "magic" else 0)
        if (type(document.get("schema_version")) is not int or document["schema_version"] != 2 or
                not isinstance(tables, list) or len(tables) != expected_count):
            raise ValueError(f"{stem}: 格式版本或表集合不符")
        for table, spec in zip(tables, specs):
            count = 1
            for dimension in spec[2]:
                count *= dimension
            size = count * (4 if spec[1] == "uint32_t" else 1)
            check_table(table, spec, source_range(data, spec[3], size))
            checked += 1
            total += size
        if stem == "magic":
            for table,name in zip(tables[len(specs):],MAGIC_CONTRACT_TABLES):
                if (table.get("name"),table.get("c_type"),table.get("dimensions")) != (name,"uint8_t",[28]):
                    raise ValueError("magic contract: 新增有限投影的名称／类型／维度不符")
                values=table.get("values")
                if (not isinstance(values,list) or len(values)!=28 or
                        any(type(value) is not int or not 0<=value<=255 for value in values)):
                    raise ValueError("magic contract: 投影整数不符")
    unit = json.loads((tables_dir / "unit_base.json").read_text(encoding="utf-8"))["tables"]
    stage = json.loads((tables_dir / "stage.json").read_text(encoding="utf-8"))["tables"]
    windows = {identity: source_range(data, 0x61DA1 + 24 * identity, 24) for identity in (48, 66)}
    check_constructor_alias_views(unit, stage, windows)
    return {"ok": True, "tables": checked, "source_bytes": total,
            "constructor_alias_views": len(windows), "constructor_alias_bytes": 48,
            "extra_magic_contracts":len(MAGIC_CONTRACT_TABLES),
            "executable_sha256": EXE_SHA256}


def check_constructor_alias_views(unit_tables, stage_tables, expected):
    """独立固定getter地址和实际调用ID，不导入生成器的投影实现。"""
    unit = {table["name"]: table for table in unit_tables}
    stage = {table["name"]: table for table in stage_tables}
    alias = unit.get("k_player_base_alias_views", {})
    if (alias.get("c_type"), alias.get("dimensions"), alias.get("values")) != (
            "uint16_t", [2, 3], [[48, 0, 384], [66, 1, 68]]):
        raise ValueError("constructor alias: 未确认的构造ID／来源／偏移")
    # 48读growth+384，66读stage_prepare+68；两个getter窗均非新基础表。
    growth = bytes(number for row in unit["k_growth_records"]["values"] for number in row)
    preparation = bytes(number for row in stage["k_stage_prepare"]["values"] for number in row)
    for identity, payload, offset in ((48, growth, 384), (66, preparation, 68)):
        if set(expected) != {48, 66} or payload[offset:offset + 24] != expected[identity]:
            raise ValueError("constructor alias: 投影字节与独立getter原窗不符")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--tables-dir", type=Path, default=ROOT / "data" / "static" / "tables")
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.executable, args.tables_dir), ensure_ascii=False))
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        parser.exit(1, f"冷表原字节对照失败：{error}\n")
