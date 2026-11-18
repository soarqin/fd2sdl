#!/usr/bin/env python3
"""SD1 准备／音乐／战斗／脚步冷表的独立来源核对，不写原资源。

原读集由既有 getter、数据格式和 IDA 证据固定，不导入生产生成器。
cue 是原调用常量的投影，另核对现有 11 项契约，不冒称连续原字节表。
--self-test 在内存中注入错误；原字节、EXE 身份或 cue 不符必须明确失败。
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct

from verify_unit_tables import EXE_SHA256, ROOT, source_range
from table_schema import TABLE_SCHEMA_VERSION, load_table_document

# 名称／类型／尺寸／VA独立登记，禁止按待验JSON的自报范围寻找匹配。
TABLES = {
    "stage": (
        ("k_stage_prepare", "uint8_t", (26, 31), 0x6238D, 806),
        ("k_stage_music", "fd2_field_stage_music", (30,), 0x51E63, 60),
    ),
    "attack": (
        ("k_terrain_attack_percent", "int32_t", (6,), 0x51A12, 24),
        ("k_terrain_defense_percent", "int32_t", (6,), 0x51A2A, 24),
        ("k_critical_by_profile", "uint8_t", (30,), 0x524A8, 30),
    ),
    "field_audio": (
        ("k_footstep_classes", "uint8_t", (29,), 0x52725, 29),
    ),
}

# field_audio既有映射；依据为当前IDA伪C的对应sfx_play调用。
# 顺序固定为公开fd2_field_sfx的0..10，不从待验values反向推导。
CUES = (
    ("FD2_FIELD_SFX_DETAIL_OPEN", 31, 5),
    ("FD2_FIELD_SFX_DETAIL_CLOSE", 31, 6),
    ("FD2_FIELD_SFX_COMMAND_MENU", 31, 8),
    ("FD2_FIELD_SFX_DIALOG_GLYPH", 31, 2),
    ("FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9", 31, 9),
    ("FD2_FIELD_SFX_FOOTSTEP_SAMPLE_10", 31, 10),
    ("FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11", 31, 11),
    ("FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL", 95, 0),
    ("FD2_FIELD_SFX_ACTOR_GROUP_FLASH", 80, 1),
    ("FD2_FIELD_SFX_STAGE_TRANSITION", 80, 11),
    ("FD2_FIELD_SFX_EARTHQUAKE", 80, 13),
)


def flatten(value, dimensions, c_type):
    if dimensions:
        if not isinstance(value, list) or len(value) != dimensions[0]:
            raise ValueError("原始记录尺寸不符")
        return [number for child in value
                for number in flatten(child, dimensions[1:], c_type)]
    if type(value) is not int or not (
            (-2147483648 <= value <= 2147483647) if c_type == "int32_t"
            else (0 <= value <= 255)):
        raise ValueError("非法冷表整数字段")
    return [value]


def table_bytes(table, spec):
    name, c_type, dimensions, _, _ = spec
    if (table.get("name"), table.get("c_type"), table.get("dimensions")) != (
            name, c_type, list(dimensions)):
        raise ValueError(f"{name}：名称／类型／尺寸不符")
    # 原音乐表是连续primary[30]和alternate[30]，JSON结构按stage配对。
    shape = dimensions + (2,) if c_type == "fd2_field_stage_music" else dimensions
    values = flatten(table.get("values"), shape, c_type)
    if c_type == "fd2_field_stage_music":
        return bytes(values[::2] + values[1::2])
    return struct.pack(f"<{len(values)}i", *values) if c_type == "int32_t" else bytes(values)


def check(documents, executable):
    if hashlib.sha256(executable).hexdigest() != EXE_SHA256:
        raise ValueError("FD2.EXE 身份不符；拒绝套用已确认来源")
    checked = source_bytes = 0
    for stem, specs in TABLES.items():
        document = documents[stem]
        tables = document.get("tables")
        count = len(specs) + (stem == "field_audio")
        if (type(document.get("schema_version")) is not int
                or document["schema_version"] != TABLE_SCHEMA_VERSION
                or not isinstance(tables, list) or len(tables) != count):
            raise ValueError(f"{stem}：格式版本或表集合不符")
        for table, spec in zip(tables, specs):
            name, _, _, va, size = spec
            expected = source_range(executable, va, size)
            if table_bytes(table, spec) != expected:
                raise ValueError(f"{name}：原始字段／未知字节与原版不符")
            checked += 1
            source_bytes += size
    cue = documents["field_audio"]["tables"][-1]
    spec = ("k_sfx_cues", "uint8_t", (11, 2), 0, 22)
    expected = bytes(number for _, bank, sample in CUES for number in (bank, sample))
    if table_bytes(cue, spec) != expected:
        raise ValueError("cue：11项bank／sample映射不符")
    return {"ok": True, "raw_tables": checked, "source_bytes": source_bytes,
            "cue_mappings": len(CUES), "executable_sha256": EXE_SHA256}


def negative_checks(documents, executable):
    bad_cases = []
    for stem, specs in TABLES.items():
        for index, _ in enumerate(specs):
            bad = copy.deepcopy(documents)
            values = bad[stem]["tables"][index]["values"]
            while isinstance(values[0], list):
                values = values[0]
            values[0] += 1
            bad_cases.append((bad, executable))
    bad = copy.deepcopy(documents)
    bad["stage"]["tables"][0]["dimensions"] = [1]
    bad_cases.append((bad, executable))
    bad = copy.deepcopy(documents)
    bad["field_audio"]["tables"][-1]["values"] = [[31, 5]] * 11
    bad_cases.append((bad, executable))
    bad_cases.append((documents, bytes((executable[0] ^ 1,)) + executable[1:]))
    for bad, data in bad_cases:
        try:
            check(bad, data)
        except ValueError:
            continue
        raise ValueError("负例错误地通过独立来源检查")
    return len(bad_cases)


def verify(executable: Path, tables_dir: Path, self_test=False):
    documents = {stem: load_table_document(tables_dir / f"{stem}.json") for stem in TABLES}
    data = executable.read_bytes()
    result = check(documents, data)
    if self_test:
        result["negative_cases_rejected"] = negative_checks(documents, data)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--tables-dir", type=Path, default=ROOT / "data/static/tables")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.executable, args.tables_dir, args.self_test), ensure_ascii=False))
    except (OSError, ValueError, TypeError, KeyError, IndexError, AttributeError, struct.error) as error:
        parser.exit(1, f"冷表来源对照失败：{error}\n")
