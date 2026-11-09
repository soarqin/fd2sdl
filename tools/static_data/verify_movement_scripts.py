#!/usr/bin/env python3
"""只读核对已支持移动脚本，不以JSON自报地址／长度作为提取依据。

field_movement_script_ptr @0x4eb48 的 object3 指针表 canonical0x627d8，
field_movement_script_play @0x1366a 的 group_count/mode/actor-direction 格式。
复用严格LE源页与fixup解析；不应用relocation、不写镜像或原资源。
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from dump_fd2_fixup_table import collect_internal_offset_fixups
from rebuild_fd2_analysis import parse_le, parse_objects
from verify_unit_tables import EXE_SHA256, source_range

SCRIPT_IDS = tuple(range(106))
POINTER_TABLE_VA = 0x627D8
ACTOR_LIMIT = 96  # 原10010/1088d分配7680B，不能以当前某一关人数缩小全局域。
EXPECTED_SOURCE_BYTES = 2896
GETTER_EVIDENCE = "field_movement_script_ptr @0x4eb48"
PLAYER_EVIDENCE = "field_movement_script_play @0x1366a"


@dataclass(frozen=True)
class ScriptSource:
    canonical_va: int
    data: bytes


def parse_stream(data: bytes, *, complete: bool = True) -> bytes:
    """按原计数解析一个完整脚本；源对象尾允许有其它数据，JSON流不允许尾随。"""
    if not isinstance(data, bytes) or not data:
        raise ValueError("移动脚本缺少group_count")
    at = 1
    for _ in range(data[0]):
        if at + 2 > len(data):
            raise ValueError("移动脚本group/mode截断")
        # mode所有位均有含义：bit7为等待／朝向模式，低7位为数量。
        mode, count = data[at:at + 2]
        if count > 32:
            raise ValueError("移动脚本pair数量越出原32B栈数组")
        at += 2
        end = at + count * 2
        if end > len(data):
            raise ValueError("移动脚本actor/direction对截断")
        for offset in range(at, end, 2):
            actor, direction = data[offset:offset + 2]
            if actor >= ACTOR_LIMIT or (direction > 3 and not (mode == 0x80 and direction == 5)):
                raise ValueError("移动脚本actor或direction越界")
        at = end
    if complete and at != len(data):
        raise ValueError("移动脚本未完整消费，存在尾随字节")
    return data[:at]


def source_scripts(executable: Path) -> dict[int, ScriptSource]:
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("FD2.EXE 身份不符，拒绝套用脚本指针表")
    objects = parse_objects(data, parse_le(data))
    owners = [obj for obj in objects if obj.index == 3 and obj.relbase == 0x60000]
    if len(owners) != 1:
        raise ValueError("缺少已确认的object3 canonical owner")
    owner = owners[0]
    # 从LE对象自身的范围取已初始化原字节，长度独立于JSON和脚本记录。
    payload = source_range(data, owner.relbase, owner.vsize)
    fixups = collect_internal_offset_fixups(executable)
    if executable.read_bytes() != data:
        raise ValueError("原EXE在读取过程中发生变化")
    result = {}
    for identity in SCRIPT_IDS:
        source_va = POINTER_TABLE_VA + identity * 4
        if not owner.relbase <= source_va <= owner.relbase + len(payload) - 4:
            raise ValueError("脚本指针表越出object3")
        target = fixups.get(source_va)
        if not target or target[0] != 3 or target[2] != owner.relbase + target[1]:
            raise ValueError(f"script{identity}: 缺少有效object3 fixup目标")
        at = target[2] - owner.relbase
        if not 0 <= at < len(payload):
            raise ValueError(f"script{identity}: fixup目标越出初始化源页")
        result[identity] = ScriptSource(target[2], parse_stream(payload[at:], complete=False))
    if sum(len(script.data) for script in result.values()) != EXPECTED_SOURCE_BYTES:
        raise ValueError("106条已确认移动脚本的独立读集长度不符")
    return result


def check_table(table: dict, identity: int, expected: ScriptSource) -> None:
    if type(identity) is not int or identity not in SCRIPT_IDS:
        raise ValueError("未支持的移动脚本ID")
    if not isinstance(table, dict) or set(table) != {"name", "c_type", "dimensions", "values", "source"}:
        raise ValueError(f"script{identity}: 表字段不符")
    if (table["name"], table["c_type"], table["dimensions"]) != (
            f"k_movement_script_{identity:02x}", "uint8_t", [len(expected.data)]):
        raise ValueError(f"script{identity}: 名称／类型／尺寸不符")
    values = table["values"]
    if not isinstance(values, list) or any(type(value) is not int or not 0 <= value <= 255 for value in values):
        raise ValueError(f"script{identity}: 脚本必须为完整byte数组")
    actual = parse_stream(bytes(values))
    if actual != expected.data:
        raise ValueError(f"script{identity}: 字节与独立fixup源不符")
    source = table["source"]
    provenance = {
        "file": "FD2.EXE", "file_sha256": EXE_SHA256, "object": 3,
        "canonical_va": hex(expected.canonical_va), "pointer_table_va": hex(POINTER_TABLE_VA),
        "script_id": identity, "length": len(expected.data),
        "sha256": hashlib.sha256(expected.data).hexdigest(),
    }
    if (not isinstance(source, dict) or
            type(source.get("script_id")) is not int or
            any(source.get(key) != value for key, value in provenance.items()) or
            not isinstance(source.get("evidence"), str) or
            GETTER_EVIDENCE not in source["evidence"] or PLAYER_EVIDENCE not in source["evidence"]):
        raise ValueError(f"script{identity}: 来源身份／指针目标／规范getter依据不符")


def check_document(document: dict, expected: dict[int, ScriptSource]) -> None:
    if (not isinstance(document, dict) or set(document) != {"schema_version", "tables"} or
            type(document["schema_version"]) is not int or document["schema_version"] != 1 or
            not isinstance(document["tables"], list)):
        raise ValueError("移动脚本JSON格式不符")
    if set(expected) != set(SCRIPT_IDS):
        raise ValueError("独立源读集必须覆盖固定106个已确认ID")
    records = {}
    for table in document["tables"]:
        identity = table.get("source", {}).get("script_id") if isinstance(table, dict) and isinstance(table.get("source"), dict) else None
        if type(identity) is not int or identity not in SCRIPT_IDS or identity in records:
            raise ValueError("移动脚本ID未知／缺失／重复")
        records[identity] = table
    if set(records) != set(SCRIPT_IDS):
        raise ValueError("移动脚本集合未覆盖固定106个已确认ID")
    for identity in SCRIPT_IDS:
        check_table(records[identity], identity, expected[identity])


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"重复JSON键：{key}")
        result[key] = value
    return result


def verify(executable: Path, definitions: Path = ROOT / "data/static/tables/movement_scripts.json") -> dict:
    expected = source_scripts(executable)
    document = json.loads(definitions.read_text(encoding="utf-8"), object_pairs_hook=unique_keys)
    check_document(document, expected)
    return {"ok": True, "scripts": len(expected), "source_bytes": sum(len(s.data) for s in expected.values()),
            "pointer_table_va": hex(POINTER_TABLE_VA), "executable_sha256": EXE_SHA256}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--definitions", type=Path, default=ROOT / "data/static/tables/movement_scripts.json")
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.executable, args.definitions), ensure_ascii=False))
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        parser.exit(1, f"移动脚本原字节对照失败：{error}\n")
