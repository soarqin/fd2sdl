#!/usr/bin/env python3
"""G2 动作呈现冷表的独立原字节／调用常量核对；不导入生产生成器。"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct

from verify_unit_tables import EXE_SHA256, ROOT, check_table, source_range
from table_schema import TABLE_SCHEMA_VERSION, load_table_document
from dump_fd2_fixup_table import collect_internal_offset_fixups

# Domain 完整1c4cc/1cd17/1e1dc/2d80d读集确定地址与消费索引，
# 不能从待验JSON自报VA、hash或行数反向决定原始读集。
SPECS = (
    ("k_battle_map_fx_start", "uint8_t", (33,), 0x51F33),
    ("k_battle_map_fx_count", "uint8_t", (33,), 0x51F54),
    ("k_battle_map_fx_cue", "uint8_t", (33,), 0x51F75),
    ("k_battle_map_damage_tint", "uint8_t", (30,), 0x52006),
    ("k_battle_popup_miss", "uint8_t", (4,), 0x5204A),
    ("k_battle_default_scene", "uint8_t", (30,), 0x52470),
    ("k_battle_composite_red", "uint8_t", (4,), 0x5265C),
    ("k_battle_composite_green", "uint8_t", (4,), 0x52660),
    ("k_battle_composite_blue", "uint8_t", (4,), 0x52664),
    ("k_battle_composite_banks", "uint8_t", (4,), 0x52668),
    ("k_battle_short_fx_sequences", "uint8_t", (84,), 0x619A9),
)
EXTRA_CUES = [[22, 7, 3], [25, 3, 5], [25, 6, 5], [18, 4, 7],
              [19, 3, 8], [19, 6, 8], [8, 3, 10], [8, 6, 10],
              [9, 15, 15], [9, 19, 15]]
ITEM_PRESENTATIONS = [
    [1,2,1], [1,2,1], [1,2,1], [1,1,1], [1,1,1],
    [1,1,1], [1,2,1], [1,2,1], [1,2,1], [1,1,1],
    [1,2,1], [1,2,1], [1,1,1], [1,1,1], [1,1,1],
    [2,1,0], [3,1,0], [1,1,1], [0,0,0], [2,1,0],
]


def source_short_fx_offsets(executable, fixups):
    # 21项指针从真实LE fixup的object3目标投影，不信JSON自报地址或原raw offset。
    offsets=[]
    for i in range(21):
        va=0x61955+i*4
        target=fixups.get(va)
        if (not target or target[0]!=3 or target[2]!=0x60000+target[1] or
                struct.unpack("<I",source_range(executable,va,4))[0]!=target[1] or
                not 0x619a9<=target[2]<0x619fd):
            raise ValueError("SHORT武器FX缺少有效object3 fixup目标")
        offsets.append(target[2]-0x619a9)
    raw=source_range(executable,0x619a9,84)
    at=0;starts=set()
    while at<len(raw):
        starts.add(at);count=raw[at]
        if not count or at+1+2*count>len(raw):
            raise ValueError("SHORT原FX序列截断")
        for index in range(count):
            if raw[at+1+index*2]>=39:
                raise ValueError("SHORT原FX frame越调用域")
        at+=1+2*count
    if at!=84 or set(offsets)!=starts or len(starts)!=6:
        raise ValueError("SHORT原指针未恰覆盖六个完整序列")
    return offsets


def check(document, executable, fixups):
    if hashlib.sha256(executable).hexdigest() != EXE_SHA256:
        raise ValueError("FD2.EXE 身份不符")
    if type(document.get("schema_version")) is not int or document["schema_version"] != TABLE_SCHEMA_VERSION:
        raise ValueError("呈现冷表版本不符")
    tables = {row["name"]: row for row in document["tables"]}
    if len(tables) != len(document["tables"]):
        raise ValueError("重复呈现冷表")
    for spec in SPECS:
        check_table(tables[spec[0]], spec, source_range(executable, spec[3], spec[2][0]))
    offsets = source_short_fx_offsets(executable, fixups)
    row=tables["k_battle_short_fx_offsets"]
    if ((row.get("c_type"),row.get("dimensions"),row.get("values"))!=
            ("uint8_t",[21],offsets)):
        raise ValueError("SHORT FX offsets不是原fixup目标投影")
    # 完整Domain读集对应的原指令；数值来自push/lea/cmp实际立即数，不以SDL观察拟合。
    anchors=((0x1a7da,"6a40",1),(0x25c63,"6a1f",1),(0x1ea4f,"6840010000",1),
        (0x1eb64,"8d7327",2),(0x1eb5b,"83fb0a",2),(0x1ebbe,"6a19",1),
        (0x1ea8b,"6a50",1),(0x1e96b,"6a08",1),(0x1e67a,"6a37",1))
    values=[]
    for va,hexcode,start in anchors:
        code=bytes.fromhex(hexcode)
        if source_range(executable,va,len(code))!=code:
            raise ValueError("SHORT/恢复参数原指令不同")
        values.append(int.from_bytes(code[start:],"little"))
    row=tables["k_battle_map_action_parameters"]
    if ((row.get("c_type"),row.get("dimensions"),row.get("values"))!=
        ("uint16_t",[9],values)):
        raise ValueError("SHORT/恢复参数不是原caller常量")
    # 完整4e127读集证实arg_C未用，arg_8同时作stride与AH常色；不是51f15原表。
    stride_call = source_range(executable, 0x1c350, 5)
    if (stride_call != bytes.fromhex("68c8010000") or source_range(executable,0x4e133,13) !=
            bytes.fromhex("8b551083ea1833c98b45108ae0")):
        raise ValueError("1c2da/4e127实际stride及常色原指令不符")
    solid = tables["k_battle_map_solid_color"]
    if ((solid.get("c_type"), solid.get("dimensions"), solid.get("values")) !=
            ("uint8_t", [1], [int.from_bytes(stride_call[1:],"little") & 255])):
        raise ValueError("4e127实际常色不是原stride456低byte200")
    colors = tables["k_battle_cast_colors"]
    raw = source_range(executable, 0x51AAD, 108)
    projected = bytes(value for i in range(36) for value in (raw[i], raw[36+i], raw[72+i]))
    check_table(colors, ("k_battle_cast_colors", "uint8_t", (36, 3), 0x51AAD), projected)
    cue = tables["k_battle_map_extra_cues"]
    if ((cue.get("c_type"), cue.get("dimensions"), cue.get("values")) !=
            ("uint8_t", [10, 3], EXTRA_CUES)):
        raise ValueError("1c4cc 附加cue分支投影不符")
    masks = tables["k_battle_heal_masks"]
    if ((masks.get("c_type"), masks.get("dimensions"), masks.get("values")) !=
            ("uint8_t", [4, 2], [[1, 2], [2, 4], [8, 4], [6, 6]])):
        raise ValueError("21eb1 恢复术caller半径／步长不符")
    item = tables["k_battle_item_presentations"]
    if ((item.get("c_type"), item.get("dimensions"), item.get("values")) !=
            ("uint8_t", [20, 3], ITEM_PRESENTATIONS)):
        raise ValueError("20c6f 物品呈现caller／零值规则投影不符")
    # 2cf30六步抖动与已有物理shake_x相同，保持唯一冷定义。
    if tables["shake_x"]["values"] != list(source_range(executable, 0x52656, 6)):
        raise ValueError("2cf30 与既有shake_x消费值不符")
    return {"ok": True, "raw_tables": len(SPECS), "extra_cues": len(EXTRA_CUES),
            "reused_shake": True, "executable_sha256": EXE_SHA256,
            "scope": "static bytes and reviewed call constants, not runtime animation acceptance"}


def negative_checks(document, executable, fixups):
    count = 0
    for name in [spec[0] for spec in SPECS] + ["k_battle_map_solid_color", "k_battle_cast_colors", "k_battle_map_extra_cues", "k_battle_heal_masks", "k_battle_item_presentations", "shake_x", "k_battle_short_fx_offsets", "k_battle_map_action_parameters"]:
        bad = copy.deepcopy(document)
        row = next(row for row in bad["tables"] if row["name"] == name)
        values = row["values"]
        while isinstance(values[0], list):
            values = values[0]
        values[0] ^= 1
        try:
            check(bad, executable, fixups)
        except ValueError:
            count += 1
            continue
        raise ValueError(f"错误呈现表被接受：{name}")
    for mutation in ("missing", "wrong_owner", "wrong_target"):
        bad=dict(fixups)
        owner,offset,va=bad[0x61955]
        if mutation=="missing": del bad[0x61955]
        elif mutation=="wrong_owner": bad[0x61955]=(2,offset,va)
        else: bad[0x61955]=(owner,offset+1,va+1)
        try: check(document,executable,bad)
        except ValueError: count+=1;continue
        raise ValueError(f"错误SHORT fixup被接受：{mutation}")
    return count


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--tables", type=Path,
                        default=ROOT / "data/static/tables/battle_animation.json")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        document = load_table_document(args.tables, "battle_animation")
        executable = args.executable.read_bytes()
        fixups = collect_internal_offset_fixups(args.executable)
        result = check(document, executable, fixups)
        if args.self_test:
            result["negative_cases_rejected"] = negative_checks(document, executable, fixups)
        if args.executable.read_bytes()!=executable:
            raise ValueError("原EXE在独立验证过程中变化")
        print(json.dumps(result, ensure_ascii=False))
    except (ValueError, KeyError, TypeError, OSError) as error:
        parser.exit(1, f"动作呈现冷表核对失败：{error}\n")
