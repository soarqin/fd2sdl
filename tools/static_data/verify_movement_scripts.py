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
from verify_unit_tables import EXE_SHA256, HISTORICAL_EXE_SHA256, source_range
from table_schema import TABLE_SCHEMA_VERSION, load_json

SCRIPT_IDS = tuple(range(106))
POINTER_TABLE_VA = 0x627D8
ACTOR_LIMIT = 96  # 原10010/1088d分配7680B，不能以当前某一关人数缩小全局域。
EXPECTED_SOURCE_BYTES = 2896


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


def source_scripts(executable: Path, *, data: bytes | None = None) -> dict[int, ScriptSource]:
    if data is None:
        data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() not in (EXE_SHA256, HISTORICAL_EXE_SHA256):
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


def script_definition(data: bytes) -> dict:
    groups = []
    at = 1
    for _ in range(data[0]):
        mode, count = data[at:at + 2]
        at += 2
        end = at + 2 * count
        actors = [{"slot": data[i], "direction": data[i + 1]} for i in range(at, end, 2)]
        groups.append({"wait_frames" if mode & 0x80 else "move_cells": mode & 0x7f, "actors": actors})
        at = end
    return {"groups": groups}


def check_script(definition: dict, identity: int, expected: ScriptSource) -> None:
    if type(identity) is not int or identity not in SCRIPT_IDS:
        raise ValueError("未支持的移动脚本ID")
    if not isinstance(definition, dict):
        raise ValueError(f"script{identity}: 需要具名脚本对象")
    if json.dumps(definition, sort_keys=True) != json.dumps(script_definition(expected.data), sort_keys=True):
        raise ValueError(f"script{identity}: 具名字段与独立fixup源不符")


def check_document(document: dict, expected: dict[int, ScriptSource]) -> None:
    if (not isinstance(document, dict) or set(document) != {"schema_version", "scripts"} or
            type(document["schema_version"]) is not int or document["schema_version"] != TABLE_SCHEMA_VERSION or
            not isinstance(document["scripts"], dict)):
        raise ValueError("移动脚本JSON格式不符")
    if set(expected) != set(SCRIPT_IDS):
        raise ValueError("独立源读集必须覆盖固定106个已确认ID")
    scripts = document["scripts"]
    if set(scripts) != {str(identity) for identity in SCRIPT_IDS}:
        raise ValueError("移动脚本集合未覆盖固定106个已确认ID")
    for identity in SCRIPT_IDS:
        check_script(scripts[str(identity)], identity, expected[identity])


def verify(executable: Path, definitions: Path = ROOT / "data/static/tables/movement_scripts.json") -> dict:
    data = executable.read_bytes()
    expected = source_scripts(executable, data=data)
    document = load_json(definitions)
    check_document(document, expected)
    return {"ok": True, "scripts": len(expected), "source_bytes": sum(len(s.data) for s in expected.values()),
            "pointer_table_va": hex(POINTER_TABLE_VA), "executable_sha256": hashlib.sha256(data).hexdigest()}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--definitions", type=Path, default=ROOT / "data/static/tables/movement_scripts.json")
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.executable, args.definitions), ensure_ascii=False))
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        parser.exit(1, f"移动脚本原字节对照失败：{error}\n")
