#!/usr/bin/env python3
"""B3物品原版/SDL比较：Use5..24、Give、EQT、Drop及各层取消。

依据field_player_item_command_execute @0x1bbdc（原登记同名）、
field_unit_inventory_select @0x1b932（原sub_1B932）、
field_unit_inventory_select_step @0x1b9de（原sub_1B9DE）及
field_unit_detail_equipment_draw @0x184c0（原登记同名）。
Drop分支复用field_unit_inventory_slot_remove @0x1b8e7（原sub_1B8E7）与
field_unit_combat_stats_recompute @0x1b750（原登记同名）。
EQT复用field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）、
field_unit_can_equip_item @0x1c1c3（原sub_1C1C3）、
field_equipment_profile_get @0x4e88e（原sub_4E88E）及
field_unit_equipment_slot_set @0x1c142（原sub_1C142）。Give复用
field_unit_inventory_item_append @0x1bb8c（原sub_1BB8C）；Use复用
field_item_use_effect_execute @0x20c6f（原sub_20C6F）。依据本轮隔离IDA库
完整发布及独立strict LE；静态语义、SDL自动检查与原版WGC实际收据分开登记。
取消路径必须结合18d8c @19055..19065与18890零步controller：
Use列表→ITEM子菜单→COMMAND→选择范围→browse。不带--sdl保持原版-only；
配对仅验命名资源、共同静态RGB及已量测终值，不冒称整帧/完整RNG/音画。
由宿主driver run-job串行约束进程树；只用WGC及本轮owned PID清理。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import struct
import sys
import time

from PIL import Image, ImageChops

from audit_fdicon_opaque_zero import audit_fdicon
from fd2_item_list_oracle import (AFTER_USE_INVENTORY, PLAIN_SHA256, START_SHA256,
    derive_injured, equipment_rows, inventory_templates, item_table, use_effect_tables, prepare as prepare_templates)
from fd2_movement_oracle import movement_profile
from run_graphics_tests import decode_save
from run_original_comparison import Capture, SIZE, archive_entry, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, save_checkpoint, wait_state, wait_title
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle
from run_original_load import (check_load_measurement, focus_cycle_assets, focused_actor,
                               require_source_unchanged, wait_measured_save)
from run_original_focus import native_focus_reference
from run_original_player import PlayerOracle, acted_sprite, cursor_assets, exact_tile, screen, sheet_frame, upper_command_composite
from run_original_result import selection_probe, selection_state
from run_original_magic_list import actor_focus_resources, detail_close_terrain
from fd2_magic_list_oracle import prepare as prepare_magic_templates
from run_original_end_turn import round_two_template, check_round_phase_log

DROP_SCENARIOS = {"drop-herb": 2, "drop-weapon": 0, "drop-armor": 1, "drop-last": 0}
DROP_LAST_PLAIN_SHA256 = "f5103f3d69983c6eda7d963dc26dfb720627a5331996c5fa1b8f1b79b528b41c"
SUBMENU_DISABLED = {"original": [0, 1, 0, 0], "sdl": [0, 1, 0, 0]}
EQUIPMENT_SCENARIOS = ("eqt-incompatible", "eqt-weapon")
SECOND_WEAPON_PLAIN_SHA256 = "e559d7bb1c719a1b0631775d64e748555fe6e0839347eb3a819a2722b963d21f"
GIVE_SCENARIOS = ("give-cancel", "give-empty", "give-swap", "give-moved")
USE_ITEMS = {5: 192, 6: 196, 7: 197, 8: 198, 9: 199, 10: 200, 11: 206, 12: 210,
             13: 58, 14: 212, 15: 213, 16: 214, 17: 94, 18: 95, 19: 96,
             20: 11, 21: 38, 22: 57, 23: 101, 24: 79}
USE_SCENARIOS = (*tuple(f"use-{code:02d}" for code in USE_ITEMS), "use-area2", "use-23-mp0")
SAVED_SCENARIOS = (*DROP_SCENARIOS, *EQUIPMENT_SCENARIOS, *GIVE_SCENARIOS, *USE_SCENARIOS)


def measurement_name(scenario):
    return "use_measurement" if scenario in USE_SCENARIOS else "give_measurement" if scenario in GIVE_SCENARIOS else "equipment_measurement" if scenario in EQUIPMENT_SCENARIOS else "drop_measurement"


def derive_use(source, encrypted, scenario, table):
    """20c6f玩家字段+10/+12/+15；controlled04隔离第一END，不走AI效果规则。"""
    if scenario not in USE_SCENARIOS:
        raise ValueError("未知玩家Use场景")
    code = 5 if scenario == "use-area2" else 23 if scenario == "use-23-mp0" else int(scenario[4:])
    item = 102 if scenario == "use-area2" else USE_ITEMS[code]
    record = table[item * 23:(item + 1) * 23]
    if len(record) != 23 or record[13] != code:
        raise ValueError("玩家Use场景原始记录/code不符")
    actor = bytearray(source[UNITS:UNITS + STRIDE])
    actor[15], actor[0x3c] = item, 99
    target = 0
    modified = {0: actor}
    if code in (5, 6, 13):
        struct.pack_into("<H", actor, 0x40, 20)
    if code == 6:
        actor[0x25] = 2
    if scenario == "use-area2":
        recipient = bytearray(source[UNITS + STRIDE:UNITS + 2 * STRIDE])
        recipient[:2] = bytes((8, 14)); struct.pack_into("<H", recipient, 0x40, 20)
        modified[1] = recipient
    if code == 7:
        recipient = bytearray(source[UNITS + STRIDE:UNITS + 2 * STRIDE])
        recipient[:2] = bytes((8, 14)); recipient[0x26] = 2
        struct.pack_into("<HH", recipient, 0x40, 20, 42)  # 原raw1 max28会夹顶；明列受控max42以可见地锁+9。
        modified[1], target = recipient, 1
    if code == 11:
        struct.pack_into("<HH", actor, 0x44, 0, 20)
    if code in (14, 20, 21, 22, 24):
        enemy = bytearray(source[UNITS + 4 * STRIDE:UNITS + 5 * STRIDE])
        enemy[:2] = bytes((9, 14) if code == 24 else (8, 13))  # 24/cast31初始十字须同轴；均距离2、无起点ZOC。
        struct.pack_into("<HH", enemy, 0x40, *((999, 999) if code in (20, 21, 24) else (20, 28)))
        enemy[5] |= 4
        modified[4], target = enemy, 4
    if code == 23:
        actor[8] = 24
        struct.pack_into("<HH", actor, 0x44, 0 if scenario == "use-23-mp0" else 40, 40)
        recipient = bytearray(source[UNITS + STRIDE:UNITS + 2 * STRIDE])
        recipient[:2] = bytes((8, 14))
        modified[1], target = recipient, 1
    changes = [(UNITS + i * STRIDE, bytes(record)) for i, record in modified.items()]
    changes += [(UNITS + i * STRIDE + 5, bytes((source[UNITS + i * STRIDE + 5] | 4,)))
        for i in range(source[META + 1]) if source[UNITS + i * STRIDE + 6] != 2 and i not in modified]
    encoded, changed, manifest = derive_active_fixture(source, encrypted, scenario, changes)
    manifest.update(item_id=item, use_code=code, source_raw_slot=2, target_raw=target,
        final_target_raws=[0, 1] if scenario == "use-area2" else [target],
        record_hex=record.hex(), player_cast=record[16], player_area=record[18], player_filter=record[21],
        controlled_nonplayer_ai_ineligible_bit=4, unit_experience_preserved=99,
        destination=[9, 14] if code == 23 else None, natural_one_turn_route_claimed=False)
    return encoded, changed, manifest


def phase_end_model(source, table):
    """1a30b/1a866首END：04 skip AI；无turn2事件，保未知字节/方向。"""
    expected = bytearray(source)
    for i in range(source[META + 1]):
        at = UNITS + i * STRIDE
        actor = bytearray(expected[at:at + STRIDE])
        hp, maximum = struct.unpack_from("<HH", actor, 0x40)
        if actor[6] == 2 and not actor[5] & 0x81 and not actor[0x25] and not actor[0x26] and hp != maximum:
            struct.pack_into("<H", actor, 0x40, min(maximum, hp + maximum // 5))
            actor[5] |= 0x80
        # 每阵营在其phase恰一次tick；归零才重算，不把stale recipient统一重算。
        if not actor[5] & 1:
            expired = False
            for offset in range(0x22, 0x28):
                if actor[offset]:
                    actor[offset] -= 1
                    expired |= actor[offset] == 0
            if expired:
                # 本夹具duration>=2，cure立即清0；未覆盖poison finalize/归零重算。
                raise ValueError("首END模型出现未声明状态归零；不放宽容器差异")
        actor[5] &= 0x7f
        expected[at:at + STRIDE] = actor
    expected[META] = 2
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    return bytes(expected)


def use_outcomes(original, source, scenario):
    """20c6f玩家效果的有限记录集合；不读SDL表、不重抽RNG、不放宽其他字节。"""
    table, _ = item_table(original / "FD2.EXE")
    base = decode_save(original / "START.SAV")
    _, exact, manifest = derive_use(base, (original / "START.SAV").read_bytes(), scenario, table)
    if bytes(source) != exact:
        raise ValueError("Use来源不等于该场景完整manifest")
    code, raw = manifest["use_code"], manifest["target_raw"]
    record = table[manifest["item_id"] * 23:(manifest["item_id"] + 1) * 23]
    power = struct.unpack_from("<H", record, 14)[0]
    target = bytearray(source[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE])
    targets = []
    def add(actor):
        if bytes(actor) not in targets: targets.append(bytes(actor))
    def word(actor, offset): return struct.unpack_from("<H", actor, offset)[0]
    def store(actor, offset, value): struct.pack_into("<H", actor, offset, value & 0xffff)
    if code in (5, 13, 11):
        offset = 0x44 if code == 11 else 0x40
        current, maximum = word(target, offset), word(target, offset + 2)
        if code == 11 and maximum == 0:
            add(target)
        else:
            for spread in range(100):
                actor = bytearray(target)
                store(actor, offset, min(maximum, current + power * 9 // 10 + spread * power // 1000))
                add(actor)
    elif code in (6, 7):
        actor = bytearray(target); offset = 0x25 if code == 6 else 0x26
        if actor[offset]:
            store(actor, 0x40, min(word(actor, 0x42), word(actor, 0x40) + 9)); actor[offset] = 0
        add(actor)
    elif code in (8, 9, 10, 17, 18, 19):
        actor = bytearray(target)
        offset = {8: 0x37, 9: 0x39, 10: 0x3e, 17: 0x42, 18: 0x46, 19: 0x3b}[code]
        experience = actor[0x3c]; store(actor, offset, word(actor, offset) + power); actor[0x3c] = experience
        add(actor)
    elif code in (12, 15, 16):
        status = {12: 0x24, 15: 0x23, 16: 0x22}[code]
        if target[status]: add(target)
        else:
            for duration in range(2, 6):
                actor = bytearray(target); actor[status] = duration
                for offset in ((0x4c, 0x4e) if code == 12 else (0x4a,) if code == 15 else (0x48,)):
                    old = word(actor, offset)
                    store(actor, offset, old + (15 if code == 12 else (15 * old + 100) // 100))
                add(actor)
    elif code in (14, 22):
        status = 0x26 if code == 14 else 0x27
        add(target)  # immune or chance miss; valid outcome, never retry it.
        if not target[status] and target[0x20] not in (25, 26):
            for duration in range(2, 6):
                actor = bytearray(target); actor[status] = duration
                store(actor, 0x40, max(0, word(actor, 0x40) - 9)); add(actor)
    elif code in (20, 21, 24):
        tables, _ = use_effect_tables(original / "FD2.EXE")
        magic = tables["magic"][power * 7:(power + 1) * 7]
        if len(magic) != 7 or not 1 <= target[0x20] <= 28:
            raise ValueError("Use damage magic/profile不在已确认原表内")
        scale = struct.unpack_from("<i", tables["damage_profiles"], (target[0x20] - 1) * 4)[0]
        damage = struct.unpack_from("<h", magic)[0] * scale // 10
        if magic[2] < 100: add(target)
        if magic[2]:
            for spread in range(100):
                actor = bytearray(target)
                store(actor, 0x40, max(0, word(actor, 0x40) - damage * 9 // 10 - spread * damage // 1000)); add(actor)
    elif code == 23:
        actor = bytearray(target); actor[:2] = bytes(manifest["destination"]); add(actor)
    else:
        raise ValueError("没有已确认的玩家Use数学分支")
    outcomes = []
    for target_record in targets:
        changed = bytearray(source)
        changed[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE] = target_record
        if scenario == "use-area2":
            # 真实102/500恢复的第二目标；全部100种spread都夹到原max28。
            at = UNITS + STRIDE; current, maximum = struct.unpack_from("<HH", source, at + 0x40)
            if {min(maximum, current + power * 9 // 10 + roll * power // 1000) for roll in range(100)} != {maximum}:
                raise ValueError("area2第二目标并非全部native spread确定夹顶")
            struct.pack_into("<H", changed, at + 0x40, maximum)
        caster = bytearray(changed[UNITS:UNITS + STRIDE])
        if code in (5, 6, 7, 8, 9, 10, 11, 17, 18, 19): inventory_remove_raw(caster, 2)
        if code in (8, 9, 10, 17, 18, 19): struct.pack_into("<4H", caster, 0x48, *drop_derived(caster, table))
        if code == 23: store(caster, 0x44, word(caster, 0x44) - 20)
        caster[5] |= 0x80
        changed[UNITS:UNITS + STRIDE] = caster
        changed[-4:] = sum(changed[:-4]).to_bytes(4, "little")
        final = phase_end_model(changed, table)
        outcomes.append((bytes(changed), final))
    return outcomes, {**manifest, "outcome_count": len(outcomes),
        "random_fields": ["target.HP"] if code in (20, 21, 24) else ["target.HP", "target.status_duration"] if code in (14, 22) else ["target.status_duration"] if code in (12, 15, 16) else [],
        "full_container_outside_native_finite_records_exact": True, "rng_rerolled": False}


def derive_active_fixture(source, encrypted, scenario, changes):
    """固定START的隔离活动记录派生；不修改profile/roster/手工槽/原文件。"""
    if hashlib.sha256(source).hexdigest() != PLAIN_SHA256 or hashlib.sha256(encrypted).hexdigest() != START_SHA256:
        raise ValueError("B3受控夹具只从完整已确认START派生")
    changed = bytearray(source)
    written = set()
    count = source[META + 1]
    for offset, value in changes:
        value = bytes(value)
        indices = set(range(offset, offset + len(value)))
        if not value or written & indices or not all(UNITS <= i < UNITS + count * STRIDE or META + 5 <= i < META + 9 for i in indices):
            raise ValueError("B3夹具写集越界、重叠或触及未拥有容器区域")
        changed[offset:offset + len(value)] = value
        written |= indices
    changed[-4:] = sum(changed[:-4]).to_bytes(4, "little")
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, changed))
    def patches(before, after):
        return [{"offset": i, "before": a, "after": b} for i, (a, b) in enumerate(zip(before, after)) if a != b]
    return encoded, bytes(changed), {"scenario": scenario, "controlled_fixture": True,
        "source_sha256": START_SHA256, "source_plain_sha256": PLAIN_SHA256,
        "derived_sha256": hashlib.sha256(encoded).hexdigest(), "derived_plain_sha256": hashlib.sha256(changed).hexdigest(),
        "plaintext_patches": patches(source, changed), "encrypted_patches": patches(encrypted, encoded),
        "scope": "explicit active-unit/focus fields plus checksum only; not natural acquisition/action history"}


def derive_give(source, encrypted, scenario):
    if scenario not in GIVE_SCENARIOS:
        raise ValueError("未知Give场景")
    changes = [(UNITS + i * STRIDE + 5, bytes((source[UNITS + i * STRIDE + 5] | 4,)))
               for i in range(source[META + 1]) if source[UNITS + i * STRIDE + 6] != 2]
    # 非玩家04仅用于隔离END测量；不是hidden，不引发stage0全灭。
    changes += [(UNITS + STRIDE, bytes((6, 15) if scenario == "give-moved" else (8, 14))),
                (UNITS + 0x3c, b"\x63"), (UNITS + STRIDE + 0x3c, b"\x63")]
    if scenario in ("give-cancel", "give-swap"):
        changes.append((UNITS + STRIDE + 0x0e, bytes((0, 192)) * 6))
    encoded, changed, manifest = derive_active_fixture(source, encrypted, scenario, changes)
    manifest.update(sender=0, recipient=1, source_raw_slot=0, swap_recipient_raw_slot=0,
        recipient_initial_count=8 if scenario in ("give-cancel", "give-swap") else 2,
        nonplayer_ai_ineligible_bit=4, after_end_scope="first END 1->2 has no stage0 turn event; 04 survives flags&7f",
        moved_in_runtime=scenario == "give-moved", source_experience=99, recipient_experience=99)
    return encoded, changed, manifest


def inventory_remove_raw(actor, selected):
    actor[0x0a + selected * 2:0x18] = actor[0x0c + selected * 2:0x1a]
    actor[0x18] = 0x80


def inventory_append_raw(actor, item):
    for slot in range(8):
        if actor[0x0a + slot * 2] & 0x80:
            actor[0x0a + slot * 2:0x0c + slot * 2] = bytes((0, item))
            return
    raise ValueError("Give模型收方无空槽")


def give_expected_source(original, source, scenario):
    base = decode_save(original / "START.SAV")
    _, exact, manifest = derive_give(base, (original / "START.SAV").read_bytes(), scenario)
    if bytes(source) != exact:
        raise ValueError("Give来源不是完整场景manifest")
    expected = bytearray(source)
    sender = bytearray(source[UNITS:UNITS + STRIDE])
    receiver = bytearray(source[UNITS + STRIDE:UNITS + 2 * STRIDE])
    receiver_stats = bytes(receiver[0x48:0x50])
    if scenario != "give-cancel":
        item = sender[0x0b]
        if scenario == "give-swap":
            returned = receiver[0x0b]
            inventory_remove_raw(receiver, 0)
            inventory_append_raw(receiver, item)
            inventory_remove_raw(sender, 0)
            inventory_append_raw(sender, returned)
        else:
            inventory_append_raw(receiver, item)
            inventory_remove_raw(sender, 0)
        table, _ = item_table(original / "FD2.EXE")
        struct.pack_into("<4H", sender, 0x48, *drop_derived(sender, table))
        if scenario == "give-moved":
            sender[:2] = bytes((7, 15))
        # 53C53取消COMMAND会acted且不退坐标；量测须合法END后，flags&7f。
        expected[META] = 2
        for i in range(expected[META + 1]):
            expected[UNITS + i * STRIDE + 5] &= 0x7f
    expected[UNITS:UNITS + STRIDE] = sender
    expected[UNITS + STRIDE:UNITS + 2 * STRIDE] = receiver
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    if bytes(receiver[0x48:0x50]) != receiver_stats:
        raise AssertionError("Give不得重算收方派生；END无归零状态时也不重算")
    return bytes(expected), {"scenario": scenario, "sender": 0, "recipient": 1,
        "sender_inventory_hex": sender[10:26].hex(), "recipient_inventory_hex": receiver[10:26].hex(),
        "sender_derived_values": list(struct.unpack_from("<4H", sender, 0x48)),
        "recipient_derived_values": list(struct.unpack_from("<4H", receiver, 0x48)),
        "recipient_recomputed": False, "success_sticky_53c53": scenario != "give-cancel",
        "measured_turn": 1 if scenario == "give-cancel" else 2,
        "end_confirmations_required": 0 if scenario == "give-cancel" else 1,
        "sender_world_after_command_cancel": list(sender[:2]), "unit_experience_preserved": [sender[0x3c], receiver[0x3c]],
        "native_evidence": "1bbdc/1bb8c/1b8e7 sender-only1b750; 18890 sticky1->acted/finallookup;1a30b/13a9f04skip;stage0 turn rows start3"}


def derive_second_weapon(source, encrypted):
    """仅raw0 slot2的192改1及checksum；原装备／派生值原样保留。"""
    if hashlib.sha256(source).hexdigest() != PLAIN_SHA256 or hashlib.sha256(encrypted).hexdigest() != START_SHA256:
        raise ValueError("第二把剑夹具只从完整已确认START派生")
    changed = bytearray(source)
    changed[UNITS + 0x0f] = 1
    changed[-4:] = sum(changed[:-4]).to_bytes(4, "little")
    if hashlib.sha256(changed).hexdigest() != SECOND_WEAPON_PLAIN_SHA256:
        raise ValueError("第二把剑完整明文manifest不符")
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, changed))
    def patches(before, after):
        return [{"offset": i, "before": a, "after": b} for i, (a, b) in enumerate(zip(before, after)) if a != b]
    return encoded, bytes(changed), {"scenario": "eqt-weapon", "controlled_fixture": True,
        "source_sha256": START_SHA256, "source_plain_sha256": PLAIN_SHA256,
        "derived_sha256": hashlib.sha256(encoded).hexdigest(), "derived_plain_sha256": SECOND_WEAPON_PLAIN_SHA256,
        "plaintext_patches": patches(source, changed), "encrypted_patches": patches(encrypted, encoded),
        "scope": "only active raw0 slot2 item ID 192->1 plus checksum; no natural acquisition history"}


def equipment_expected_source(original, source, scenario):
    """field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）→
    field_unit_can_equip_item @0x1c1c3（原sub_1C1C3）→
    field_unit_equipment_slot_set @0x1c142（原sub_1C142）。

    复用本轮隔离IDA库发布及独立strict LE；六许可类别不取第七byte。
    """
    if scenario not in EQUIPMENT_SCENARIOS:
        raise ValueError("未知装备校准场景")
    wanted = SECOND_WEAPON_PLAIN_SHA256 if scenario == "eqt-weapon" else PLAIN_SHA256
    if hashlib.sha256(source).hexdigest() != wanted:
        raise ValueError("装备校准来源不是完整START或第二把剑manifest")
    table, evidence = item_table(original / "FD2.EXE")
    rows = equipment_rows(original / "FD2.EXE")
    actor = bytearray(source[UNITS:UNITS + STRIDE])
    if actor[0x20] != 1:
        raise ValueError("装备校准只覆盖START raw0 profile1")
    row = rows[7:14]
    if row != bytes.fromhex("011516ffffff01"):
        raise ValueError("4e88e profile1原始七字节不符")
    selected = actor[0x0f]
    category = table[selected * 23]
    compatible = category in row[:6]
    if compatible != (scenario == "eqt-weapon"):
        raise ValueError("装备许可与场景预期不符")
    if compatible:
        for slot in range(8):
            flags, item_id = actor[10 + slot * 2:12 + slot * 2]
            if flags & 0x40 and (item_id < 128) == (selected < 128):
                actor[10 + slot * 2] = 0
        actor[0x0e] = 0x40
        struct.pack_into("<4H", actor, 0x48, *drop_derived(actor, table))
    values = [26, 12, 97, 2] if compatible else [16, 12, 97, 2]
    if list(struct.unpack_from("<4H", actor, 0x48)) != values:
        raise ValueError("装备后EXE派生四数不符")
    expected = bytearray(source)
    expected[UNITS:UNITS + STRIDE] = actor
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    return bytes(expected), {"scenario": scenario, "item_id": selected, "raw_slot": 2,
        "profile": 1, "row_va": "0x61891", "row_hex": row.hex(), "allowed_categories": list(row[:6]),
        "selected_category": category, "compatible": compatible, "seventh_byte_used": False,
        "item_table": evidence, "derived_values": values, "index_after_confirmation": 2,
        "before_inventory_hex": source[UNITS + 10:UNITS + 26].hex(), "after_inventory_hex": actor[10:26].hex(),
        "native_functions": ["0x1bffe", "0x1c1c3", "0x4e88e", "0x1c142", "0x1b750"],
        "runtime_raw_inventory_measured": False}


def prepare_equipment(original, source, assets, actor, fields, scenario):
    expected, contract = equipment_expected_source(original, source, scenario)
    _, after_templates, after_fields = inventory_templates(original, expected[UNITS:UNITS + STRIDE], assets)
    fields["equipment_contract"] = contract
    after_fields = dict(after_fields, submenu=fields["submenu"], equipment_contract=contract)
    navigation, steps = inventory_save_navigation(source, actor)
    assets["equipment"] = {"expected_source": expected, "after_templates": after_templates,
        "after_fields": after_fields, "steps": steps}
    contract["measurement_navigation"] = navigation


def inventory_save_navigation(source, actor):
    """Drop/EQT共用已确认LEFT空格，不卷屏，不扩大Save对照的导航例外。"""
    camera = tuple(source[META + 3:META + 5])
    world = tuple(actor["world"])
    target = (world[0] - 1, world[1])
    reference = native_focus_reference(camera, world, target)
    occupied = {(source[UNITS + i * STRIDE], source[UNITS + i * STRIDE + 1])
                for i in range(source[META + 1]) if not source[UNITS + i * STRIDE + 5] & 1}
    if reference["camera"] != list(camera) or target in occupied or (target[0], target[1] + 1) in occupied:
        raise ValueError("库存测量LEFT必须不卷屏且无actor/下格头部遮挡")
    return {"from_world": list(world), "to_world": list(target), **reference}, [("LEFT", tuple(reference["screen"]))]


def drop_derived(actor, table):
    """1b750的本组无状态分支；仅从EXE表推导，不以SDL或捕获值作oracle。"""
    if len(table) != 215 * 23 or actor[0x22:0x25] != b"\0\0\0":
        raise ValueError("Drop派生模型只覆盖原版完整表及无AP/DP/AC状态的夹具")
    attack, defense, accuracy = (struct.unpack_from("<h", actor, offset)[0] for offset in (0x37, 0x39, 0x3e))
    evasion = accuracy
    for slot in range(8):
        flags, item_id = actor[0x0a + 2 * slot:0x0c + 2 * slot]
        if flags & 0x40:
            if item_id >= 215:
                raise ValueError("Drop重算包含未知装备ID")
            ap, ac, dp, ev = struct.unpack_from("<4h", table, item_id * 23 + 1)
            attack += ap; defense += dp; accuracy += ac; evasion += ev
    return [value & 0xffff for value in (attack, defense, accuracy, evasion)]


def derive_drop_last(source, encrypted, table):
    """只改active raw0库存和1b750已证派生值；其余来源逐字节保留。"""
    if hashlib.sha256(source).hexdigest() != PLAIN_SHA256 or hashlib.sha256(encrypted).hexdigest() != START_SHA256:
        raise ValueError("最后一项Drop必须从完整已确认START派生")
    changed = bytearray(source)
    actor = bytearray(source[UNITS:UNITS + STRIDE])
    for slot in range(8):
        actor[0x0a + 2 * slot] = 0x80  # 旧ID原样保留，只有slot0改为草药。
    actor[0x0a:0x0c] = bytes((0, 192))
    struct.pack_into("<4H", actor, 0x48, *drop_derived(actor, table))
    changed[UNITS:UNITS + STRIDE] = actor
    changed[-4:] = sum(changed[:-4]).to_bytes(4, "little")
    if hashlib.sha256(changed).hexdigest() != DROP_LAST_PLAIN_SHA256:
        raise ValueError("最后一项Drop派生完整明文不符")
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, changed))
    def patches(before, after):
        return [{"offset": i, "before": a, "after": b} for i, (a, b) in enumerate(zip(before, after)) if a != b]
    return encoded, bytes(changed), {"scenario": "drop-last", "controlled_fixture": True,
        "source_sha256": START_SHA256, "source_plain_sha256": PLAIN_SHA256,
        "derived_sha256": hashlib.sha256(encoded).hexdigest(), "derived_plain_sha256": DROP_LAST_PLAIN_SHA256,
        "plaintext_patches": patches(source, changed), "encrypted_patches": patches(encrypted, encoded),
        "checksum_before": source[-4:].hex(), "checksum_after": changed[-4:].hex(),
        "inventory_hex": actor[0x0a:0x1a].hex(), "derived_values": list(struct.unpack_from("<4H", actor, 0x48)),
        "scope": "active raw0 inventory plus EXE-derived AP/DP/AC/EV and checksum only; not natural inventory history",
        "cancel_recompute_dynamic_write_observed": False}


def prepare(original, *, source=None, herb=False):
    source, focus, assets, templates, fields = prepare_templates(original, original / "START.SAV", source=source)
    actor = focus_cycle_assets(original, source, assets)["actors"][0]
    # composite首6行在cursor开始之前，仍是原actor头部；复用其12帧。
    assets["upper_commands"] = {index: [upper_command_composite(assets["commands"][index], image)
                                        for image in actor["templates"]]
                                for index in (*range(3), 24, 25)}
    probe = selection_probe(original, source, 0, assets)
    if screen(source, tuple(actor["world"])) != (52, 76) or actor["world"] != [7, 14]:
        raise ValueError("物品校准仅覆盖真实START raw0已确认的焦点与相机")
    fields["submenu"] = {"command_ids": [8, 9, 10, 11], "disabled": [0, 1, 0, 0],
                         "default": 0, "disabled_give_reason": "no adjacent filter3 target",
                         "command_default": 2, "actor_screen": [52, 76]}
    if herb:
        prepare_herb(original, source, assets, probe, fields)
    return source, focus, assets, actor, probe, templates, fields


def prepare_controlled(original, source):
    """场景先通过完整derive manifest；重算其实际几何，不改夹具或运行态。"""
    _, _, assets, _, _, _, _ = prepare(original)
    actor = focus_cycle_assets(original, source, assets)["actors"][0]
    _, templates, fields = inventory_templates(original, source[UNITS:UNITS + STRIDE], assets)
    fields["raw_actor"] = 0
    adjacent = any(not source[UNITS + i * STRIDE + 5] & 1 and source[UNITS + i * STRIDE + 6] == 2 and
        abs(source[UNITS + i * STRIDE] - actor["world"][0]) + abs(source[UNITS + i * STRIDE + 1] - actor["world"][1]) == 1
        for i in range(source[META + 1]))
    fields["submenu"] = {"command_ids": [8, 9, 10, 11], "disabled": [0, int(not adjacent), 0, 0],
        "default": 0, "command_default": 2, "actor_screen": list(screen(source, tuple(actor["world"]))),
        "native_evidence": "1bbdc/14818 filter3: visible side2 at distance1, excludes entire origin cell"}
    probe = selection_probe(original, source, 0, assets)
    return source, screen(source, tuple(source[META + 5:META + 7])), assets, actor, probe, templates, fields


def inventory_detail(frame, templates, fields):
    """17fc0/184c0：完整未选中底板及本actor数值；不把别人的MP/姓名拼上。"""
    names = ("unselected", "actor_name", "current_max_hp", "current_max_mp", "level", "experience",
             "attack", "defense", "accuracy", "evasion", "dexterity", "movement", "status_25", "status_26", "status_27")
    if fields.get("require_word_bars"):
        names += ("hp_bar", "mp_bar")
    checks = [exact_tile(frame, templates[name][1], templates[name][0]) for name in names]
    if any(check is None for check in checks):
        return None
    detail = {"kind": "inventory_detail", "rectangles": [{"name": name, "position": list(templates[name][0]),
        "size": list(templates[name][1].size), **check} for name, check in zip(names, checks)],
        "checked_pixels": sum(check["checked_pixels"] for check in checks),
        **{key: fields[key] for key in ("hp", "max_hp", "mp", "max_mp", "level", "experience", "derived_values")},
        "visible_inventory_ids": [row["item_id"] for row in fields["slots"]]}
    if fields["experience"] > 99:
        detail["experience_display"] = {"width": 2, "overflow_frame": 93,
            "expected_raw_from_fixture": fields["experience"], "numeric_value_not_read_from_pixels": True,
            "raw_preservation_requires_final_save": True}
    detail["signed_word_display"] = fields["signed_word_display"]
    if fields.get("require_word_bars"):
        detail["hp_bar"], detail["mp_bar"] = fields["hp_bar"], fields["mp_bar"]
    return detail


def focused_browse(frame, source, assets, actor, terrain, templates, fields):
    focused = focused_actor(frame, assets, actor)
    restored = exact_tile(frame, terrain["image"], terrain["position"])
    position = screen(source, tuple(actor["world"]))
    if (not focused or focused["focus_screen"] != list(position) or not restored or
            not list_absent(frame, templates, fields) or PlayerOracle(source, assets, [], {}).command(frame, position)):
        return None
    return {"kind": "inventory_closed_browse", "actor": focused,
        "restored_detail_terrain": {**{key: value for key, value in terrain.items() if key != "image"}, **restored}}


def focus_steps(source, start, destination):
    reference = native_focus_reference(tuple(source[META + 3:META + 5]), tuple(start), tuple(destination))
    camera = list(source[META + 3:META + 5])
    if reference["camera"] != camera or any(step["camera"] != camera for step in reference["steps"]):
        raise ValueError("本B3场景导航需要尚未建模的卷屏；不发送输入")
    result, previous = [], tuple(start)
    for step in reference["steps"]:
        world = tuple(step["world"])
        key = "RIGHT" if world[0] > previous[0] else "LEFT" if world[0] < previous[0] else "DOWN" if world[1] > previous[1] else "UP"
        result.append((key, world, screen(source, world)))
        previous = world
    return result


def post_use_navigation_resources(original, source, assets, world):
    """只属于post-Use导航；完整保留gray actor及下格头部遮挡，不改初始assets。"""
    visible = [(i, tuple(source[UNITS + i * STRIDE:UNITS + i * STRIDE + 2]))
        for i in range(source[META + 1]) if not source[UNITS + i * STRIDE + 5] & 1]
    raw = next((i for i, at in visible if at == tuple(world)), None)
    if raw is not None:
        return {"world": tuple(world), "actor": actor_focus_resources(original, source, assets, raw, assets["cursor"])}
    lower = next((i for i, at in visible if at == (world[0], world[1] + 1)), None)
    composites = []
    if lower is not None:
        isolated = bytearray(source); isolated[META + 1] = 1
        isolated[UNITS:UNITS + STRIDE] = source[UNITS + lower * STRIDE:UNITS + (lower + 1) * STRIDE]
        _, candidates = cursor_assets(original / "FDICON.B24", isolated, assets)
        composites = [image for image, offset in candidates if offset == 0]
    return {"world": tuple(world), "lower_raw_actor": lower, "composites": composites}


def post_use_navigation_state(frame, source, assets, resource):
    at = screen(source, resource["world"])
    oracle = RoundOracle(assets, at)
    if "actor" in resource:
        found = focused_actor(frame, assets, resource["actor"])
        return dict(kind="player_use_navigation", focus_world=list(resource["world"]),
            post_use_actor=found) if found and found["focus_screen"] == list(at) else None
    if resource["composites"]:
        if oracle.menu(frame) or oracle.buttons(frame) or match_indexed(frame, assets["dialog_top"], (5, 112))["ok"]:
            return None
        found = next((result for image in resource["composites"] if (result := match_indexed(frame, image, at))["ok"]), None)
        return dict(kind="player_use_navigation", focus_world=list(resource["world"]),
            lower_raw_actor=resource["lower_raw_actor"], cursor_actor_composite=found) if found else None
    return oracle.closed_browse(frame)


def prepare_give(original, source, assets, actor, probe, fields, scenario):
    expected, contract = give_expected_source(original, source, scenario)
    runtime = bytearray(source)
    if scenario == "give-moved":
        runtime[UNITS:UNITS + 2] = b"\x07\x0f"
        if tuple(probe["world"]) == (7, 15):
            raise ValueError("moved Give初始range ROI不能与实际DOWN目的格重叠；未启动窗口")
    runtime_actor = actor_focus_resources(original, runtime, assets, 0, assets["cursor"])
    runtime_probe = selection_probe(original, runtime, 0, assets)
    if abs(runtime_probe["world"][0] - runtime_actor["world"][0]) + abs(runtime_probe["world"][1] - runtime_actor["world"][1]) != 1:
        raise ValueError("Give范围证据必须来自原版profile0可达邻格")
    fields["give_contract"] = contract
    fields["submenu"]["actor_screen"] = list(screen(runtime, tuple(runtime_actor["world"])))
    fields["submenu"]["disabled"][1] = 0
    target = actor_focus_resources(original, runtime, assets, 1, assets["cursor"])
    _, recipient_templates, recipient_fields = inventory_templates(original, runtime[UNITS + STRIDE:UNITS + 2 * STRIDE], assets)
    after = bytearray(expected)
    if scenario != "give-cancel":
        after[UNITS + 5] |= 0x80
        after[META] = 1
    _, after_templates, after_fields = inventory_templates(original, after[UNITS:UNITS + STRIDE], assets)
    after_fields = dict(after_fields, submenu=fields["submenu"], give_contract=contract)
    _, detail_templates, detail_fields = inventory_templates(original, after[UNITS + STRIDE:UNITS + 2 * STRIDE], assets)
    # recipient的magic位图/MP未变；仅复用真实START同actor未选中法术底板。
    magic_templates, _ = prepare_magic_templates(original, original / "START.SAV")
    terrain = detail_close_terrain(original, runtime, assets)
    endpoint = tuple(expected[UNITS:UNITS + 2])
    navigation = [("DOWN", (7, 16))] if scenario == "give-moved" else [("LEFT", (6, 14))]
    steps = [(key, screen(expected, world)) for key, world in navigation]
    occupied = {tuple(expected[UNITS + i * STRIDE:UNITS + i * STRIDE + 2]) for i in range(expected[META + 1]) if not expected[UNITS + i * STRIDE + 5] & 1}
    if navigation[-1][1] in occupied or (navigation[-1][1][0], navigation[-1][1][1] + 1) in occupied:
        raise ValueError("Give测量导航被actor遮挡")
    assets["give"] = {"expected_source": expected, "steps": steps, "runtime": bytes(runtime),
        "runtime_actor": runtime_actor, "probe": runtime_probe, "move_selection_probe": probe, "target": target,
        "recipient_templates": recipient_templates, "recipient_fields": recipient_fields,
        "after_source": bytes(after), "after_templates": after_templates, "after_fields": after_fields,
        "detail_templates": detail_templates, "detail_fields": detail_fields,
        "magic_bottom": magic_templates["unselected"], "terrain": terrain,
        "sender": actor_focus_resources(original, after, assets, 0, assets["cursor"]),
        "recipient": actor_focus_resources(original, after, assets, 1, assets["cursor"]),
        "end_banner": round_two_template(original / "FDOTHER.DAT", assets["palette"]), "endpoint": endpoint}


def use_range_probe(original, source, assets, cast, area, center):
    """仅这些stage0 profile0/all-one场景；避开actor、选择sprite、overlay与水色。"""
    if cast == 0:
        return None
    costs, _ = movement_profile(original / "FD2.EXE", 0)
    if costs != bytes([1] * 20):
        raise ValueError("Use范围资源oracle需要已确认profile0全1成本")
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata, data = archive_entry(field, 1), archive_entry(field, 0)
    width, height = struct.unpack_from("<HH", data)
    shape = (original / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shape, metadata[0] * 2), archive_entry(shape, metadata[0] * 2 + 1)
    lut_sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
    count = struct.unpack_from("<H", lut_sheet, 4)[0]
    occupied = {tuple(source[UNITS + i * STRIDE:UNITS + i * STRIDE + 2]) for i in range(source[META + 1]) if not source[UNITS + i * STRIDE + 5] & 1}
    caster = tuple(source[UNITS:UNITS + 2])
    cx, cy = source[META + 3:META + 5]
    for y in range(cy + 1, min(height, cy + 7)):
        for x in range(cx + 1, min(width, cx + 12)):
            distance = abs(x - caster[0]) + abs(y - caster[1])
            inside = (x == caster[0] or y == caster[1]) and distance <= cast - 16 if cast >= 16 else distance <= cast
            if not inside or (x, y) in occupied or (x, y + 1) in occupied:
                continue
            # area4不画sprite；其他area的十字/斜角都在Manhattan<=area内。
            if (x, y) == tuple(center) or area != 4 and abs(x - center[0]) + abs(y - center[1]) <= area:
                continue
            cell = struct.unpack_from("<I", data, 4 + (y * width + x) * 4)[0]
            tile = cell & 0x3ff
            if cell >> 10 or tile * 4 + 4 > len(attrs) or attrs[tile * 4]:
                continue
            base = sheet_frame(tiles, tile)
            pixels = bytes(base.getdata())
            if 0 in pixels or any(224 <= p < 240 for p in pixels):
                continue
            base.putpalette(assets["palette"])
            ranges = []
            for phase in range(count):
                start = struct.unpack_from("<I", lut_sheet, 6 + phase * 4)[0]
                end = struct.unpack_from("<I", lut_sheet, 10 + phase * 4)[0] if phase + 1 < count else len(lut_sheet)
                lut = lut_sheet[start:end]
                if len(lut) != 256:
                    raise ValueError("Use范围LUT长度不符")
                image = Image.frombytes("P", (24, 24), bytes(lut[p] for p in pixels)); image.putpalette(assets["palette"])
                if exact_tile(base.convert("RGB"), image, (0, 0)) is None:
                    ranges.append((phase, image))
            if ranges:
                return {"position": screen(source, (x, y)), "world": (x, y), "base": base, "ranges": ranges}
    raise ValueError("Use场景缺少不被sprite/actor/水色覆盖的正向range LUT资源门")


def prepare_use(original, source, assets, actor, fields, scenario):
    outcomes, contract = use_outcomes(original, source, scenario)
    raw, area, cast = contract["target_raw"], contract["player_area"], contract["player_cast"]
    center = tuple(source[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2])
    sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 1)
    cursor_frame = 1 if area in (0, 2) else 14 if area == 1 else None
    cursor = sheet_frame(sheet, cursor_frame) if cursor_frame is not None else Image.new("P", (24, 24), 0)
    cursor.putpalette(assets["palette"])
    target = actor_focus_resources(original, source, assets, raw, cursor)
    terrain = detail_close_terrain(original, source, assets)
    variants = []
    for immediate, final in outcomes:
        _, detail_templates, detail_fields = inventory_templates(original, immediate[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE], assets)
        detail_fields["require_word_bars"] = scenario == "use-area2" or contract["use_code"] == 23
        _, caster_templates, caster_fields = inventory_templates(original, immediate[UNITS:UNITS + STRIDE], assets)
        caster_fields = dict(caster_fields, submenu=fields["submenu"])
        variants.append({"source": immediate, "final": final, "detail_templates": detail_templates,
            "detail_fields": detail_fields, "caster_templates": caster_templates, "caster_fields": caster_fields})
    contract["expected_end_source_count"] = len({variant["final"] for variant in variants})
    fields["use_contract"] = contract
    final_center = tuple(contract["destination"] or center)
    first = variants[0]["source"]
    focus_raw = 0 if final_center == tuple(first[UNITS:UNITS + 2]) else raw
    icons = (original / "FDICON.B24").read_bytes()
    gray = []
    for frame_id in range(12):
        image = acted_sprite(sheet_frame(icons, frame_id)); image.putpalette(assets["palette"]); gray.append(image)
    plan = {"contract": contract, "variants": variants, "target": target, "cursor": cursor, "original": str(original),
        "cursor_frame": cursor_frame, "center": center, "range": use_range_probe(original, source, assets, cast, area, center),
        "terrain": terrain, "expected_sources": [variant["final"] for variant in variants],
        "steps": [("LEFT", screen(source, (6, 14)))], "end_banner": round_two_template(original / "FDOTHER.DAT", assets["palette"]),
        "closed_focus": actor_focus_resources(original, first, assets, focus_raw, assets["cursor"]), "gray_sprites": gray}
    plan["extra_details"] = []
    extra_raw = 1 if scenario == "use-area2" else 0 if contract["use_code"] == 23 else None
    endpoint = final_center
    if extra_raw is not None:
        raw_record = first[UNITS + extra_raw * STRIDE:UNITS + (extra_raw + 1) * STRIDE]
        _, extra_templates, extra_fields = inventory_templates(original, raw_record, assets)
        extra_fields["require_word_bars"] = True
        world = tuple(raw_record[:2])
        extra = {"raw_actor": extra_raw, "templates": extra_templates, "fields": extra_fields,
            "actor": actor_focus_resources(original, first, assets, extra_raw, assets["cursor"]),
            "navigation": [(key, post_use_navigation_resources(original, first, assets, point))
                for key, point, _ in focus_steps(source, endpoint, world)], "return_navigation": []}
        if any(raw_record[0x1a:0x1f]):
            if extra_raw != 1 or raw_record[0x1a:0x1f] != b"\x01\0\0\0\0":
                raise ValueError("额外F2法术页不在本B3已确认原START fire0资源内")
            magic_templates, _ = prepare_magic_templates(original, original / "START.SAV")
            extra["magic_bottom"] = magic_templates["unselected"]
        if scenario == "use-area2":
            extra["return_navigation"] = [(key, post_use_navigation_resources(original, first, assets, point))
                for key, point, _ in focus_steps(source, world, final_center)]
        else:
            endpoint = world
        plan["extra_details"].append(extra)
    plan["end_navigation"] = [(key, post_use_navigation_resources(original, first, assets, world))
        for key, world, _ in focus_steps(source, endpoint, (6, 14))]
    if scenario == "use-area2":
        # 122dc/mode4精确12个周边图；actor覆盖时比较原绘制顺序完整合成，不裁框。
        offsets = ((2, 0, -2), (3, -2, 0), (4, 2, 0), (5, 0, 2),
            (6, -1, -1), (7, 1, -1), (8, -1, 1), (9, 1, 1),
            (10, 0, -1), (11, -1, 0), (12, 1, 0), (13, 0, 1))
        visible = {tuple(source[UNITS + i * STRIDE:UNITS + i * STRIDE + 2]): i for i in reversed(range(source[META + 1]))
            if not source[UNITS + i * STRIDE + 5] & 1}
        plan["area2_markers"] = []
        for frame_id, dx, dy in offsets:
            world = (center[0] + dx, center[1] + dy)
            marker = sheet_frame(sheet, frame_id); marker.putpalette(assets["palette"])
            raw_actor, offset = visible.get(world), -6
            if raw_actor is None:
                raw_actor, offset = visible.get((world[0], world[1] + 1)), 0
            if raw_actor is None:
                candidates = [marker]
            else:
                isolated = bytearray(source); isolated[META + 1] = 1
                isolated[UNITS:UNITS + STRIDE] = source[UNITS + raw_actor * STRIDE:UNITS + (raw_actor + 1) * STRIDE]
                _, composites = cursor_assets(original / "FDICON.B24", isolated, dict(assets, cursor=marker))
                candidates = [image for image, at in composites if at == offset]
            at = screen(source, world); at = (at[0], at[1] + offset if raw_actor is not None else at[1])
            pixels = bytes(value if value and all(image.getdata()[i] == value for image in candidates[1:]) else 0
                for i, value in enumerate(candidates[0].getdata()))
            static = Image.frombytes("P", candidates[0].size, pixels); static.putpalette(assets["palette"])
            plan["area2_markers"].append({"frame": frame_id, "position": at,
                "images": candidates, "static": static, "actor_composited": raw_actor is not None})
        contract["area2_resource_frames"] = list(range(2, 14))
    if area == 4:
        # 122dc/display6不画sprite且写node+7=0；11eee的!=ff门仍画LUT，不是base色。
        field = archive_entry((original / "FDFIELD.DAT").read_bytes(), 0)
        width = struct.unpack_from("<H", field)[0]
        tile = struct.unpack_from("<I", field, 4 + (center[1] * width + center[0]) * 4)[0]
        if tile >> 10:
            raise ValueError("area4中心资源含overlay，不能声明完整未翻译地形门")
        tiles = archive_entry((original / "FDSHAP.DAT").read_bytes(), source[0] * 2)
        image = sheet_frame(tiles, tile & 0x3ff); image.putpalette(assets["palette"])
        if 0 in image.getdata() or any(224 <= value < 240 for value in image.getdata()):
            raise ValueError("area4中心包含透明或水色，不能使用该资源门")
        lut_sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
        count = struct.unpack_from("<H", lut_sheet, 4)[0]
        plan["center_ranges"] = []
        for phase in range(count):
            begin = struct.unpack_from("<I", lut_sheet, 6 + phase * 4)[0]
            end = struct.unpack_from("<I", lut_sheet, 10 + phase * 4)[0] if phase + 1 < count else len(lut_sheet)
            lut = lut_sheet[begin:end]
            if len(lut) != 256: raise ValueError("area4中心LUT长度不符")
            translated = Image.frombytes("P", (24, 24), bytes(lut[value] for value in image.getdata()))
            translated.putpalette(assets["palette"])
            plan["center_ranges"].append((phase, translated.crop((0, 18, 24, 24))))
    if raw == 1:
        magic_templates, _ = prepare_magic_templates(original, original / "START.SAV")
        plan["target_magic_bottom"] = magic_templates["unselected"]
    if contract["use_code"] == 23:
        plan["destination_initial_actor"] = actor_focus_resources(original, source, assets, 1, assets["cursor"])
    # execute_end只读这些小字段，不另造第二套END流程。
    assets["use"] = plan


def use_target(frame, source, assets, templates, fields):
    plan = assets["use"]
    at = screen(source, plan["center"])
    if not list_absent(frame, templates, fields): return None
    actor = next((match_indexed(frame, image, (at[0], at[1] - 6)) for image in plan["target"]["templates"]
        if match_indexed(frame, image, (at[0], at[1] - 6))["ok"]), None)
    if not actor: return None
    if plan["cursor_frame"] is None:
        marker = None  # 与独立range ROI必须是同一LUT相位，见下。
    else:
        marker = match_indexed(frame, plan["cursor"].crop((0, 18, 24, 24)), (at[0], at[1] + 18))
        if not marker["ok"]: return None
    if plan["cursor_frame"] is not None and marker is None: return None
    range_detail = None
    if plan["range"]:
        for phase, image in plan["range"]["ranges"]:
            found = exact_tile(frame, image, plan["range"]["position"])
            if found:
                if plan["cursor_frame"] is None:
                    center_image = dict(plan["center_ranges"])[phase]
                    marker = exact_tile(frame, center_image, (at[0], at[1] + 18))
                    if marker is None: continue
                range_detail = dict(phase=phase, **found); break
        if range_detail is None: return None
    marker_checks = []
    for item in plan.get("area2_markers", []):
        found = next((result for image in item["images"] if (result := match_indexed(frame, image, item["position"]))["ok"]), None)
        if found is None: return None
        marker_checks.append({"frame": item["frame"], "position": list(item["position"]),
            "size": list(item["images"][0].size), "actor_composited": item["actor_composited"], **found})
    return {"kind": "player_use_target", "use_code": plan["contract"]["use_code"],
        "target_raw": plan["contract"]["target_raw"], "center": list(plan["center"]),
        "selection_frame": plan["cursor_frame"], "display_mode": plan["contract"]["player_area"] + 2,
        "center_actor": actor, "center_marker": marker, "range_lut": range_detail,
        "range_not_independently_visible_when_cast0": plan["range"] is None,
        "selection_markers": marker_checks}


def use_completed_browse(frame, source, assets, templates, fields):
    plan = assets["use"]
    variant = plan["variants"][0]
    center = tuple(plan["contract"]["destination"] or plan["center"])
    # source target的status/HP不改变sprite；只复用资源身份/acted，不把随机raw值读作oracle。
    if not focused_browse(frame, variant["source"], assets, plan["closed_focus"], plan["terrain"], templates, fields): return None
    at = screen(source, tuple(source[UNITS:UNITS + 2]))
    if not any(match_indexed(frame, image, (at[0], at[1] - 6))["ok"] for image in plan["gray_sprites"]): return None
    return {"kind": "player_use_acted_browse", "use_code": plan["contract"]["use_code"],
        "focus_world": list(center), "caster_acted_resource": True,
        "one_confirmation_only": True, "whole_animation_rng_audio_verified": False}


def use_detail(frame, assets):
    matches, detail = [], None
    for index, variant in enumerate(assets["use"]["variants"]):
        result = inventory_detail(frame, variant["detail_templates"], variant["detail_fields"])
        if result:
            matches.append(index); detail = result
    return dict(detail, kind="player_use_detail", finite_native_outcome_indices=matches,
        random_regions=assets["use"]["contract"]["random_fields"]) if detail else None


def exercise_use_additional_details(capture, source, assets, report, checkpoint):
    plan = assets["use"]
    immediate = plan["variants"][0]["source"]
    report["use_additional_details"] = []
    for extra in plan["extra_details"]:
        raw = extra["raw_actor"]
        for index, (key, resource) in enumerate(extra["navigation"], 1):
            capture.key(key)
            checkpoint(f"use-extra-{raw}-focus-{index}", lambda frame, item=resource:
                post_use_navigation_state(frame, source, assets, item))
        capture.key("F2")
        _, detail = checkpoint(f"use-extra-{raw}-f2-detail",
            lambda frame: inventory_detail(frame, extra["templates"], extra["fields"]))
        report["use_additional_details"].append({"raw_actor": raw, **detail})
        capture.key("ESC")
        if "magic_bottom" in extra:
            at, image = extra["magic_bottom"]
            checkpoint(f"use-extra-{raw}-magic-unselected", lambda frame: dict(kind="magic_unselected_detail", **found)
                if (found := exact_tile(frame, image, at)) else None)
            capture.key("ESC")
        checkpoint(f"use-extra-{raw}-detail-closed", lambda frame: focused_browse(frame, immediate, assets,
            extra["actor"], plan["terrain"], extra["templates"], extra["fields"]))
        for index, (key, resource) in enumerate(extra["return_navigation"], 1):
            capture.key(key)
            checkpoint(f"use-extra-{raw}-return-{index}", lambda frame, item=resource:
                post_use_navigation_state(frame, source, assets, item))


def exercise_use(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint):
    plan = assets["use"]; code = plan["contract"]["use_code"]
    def target(frame): return use_target(frame, source, assets, templates, fields)
    def destination_start(frame):
        focus = focused_actor(frame, assets, plan["destination_initial_actor"])
        restored = exact_tile(frame, plan["range"]["base"], plan["range"]["position"])
        return dict(kind="use23_destination_start", actor=focus, cast_lut_cleared=restored) if focus and restored else None
    attempts = ("cancel", "destination-cancel", "commit") if code == 23 else ("cancel", "commit")
    for attempt in attempts:
        capture.key("ENTER"); checkpoint("use-" + attempt + "-list-zero", lambda frame: list_page(frame, templates, fields))
        capture.key("UP"); checkpoint("use-" + attempt + "-selected-slot2", lambda frame: list_page(frame, templates, fields, 2))
        capture.key("ENTER")
        if code == 7:
            original_target, center = plan["target"], plan["center"]
            plan["target"] = actor_focus_resources(Path(report["original_path"]), source, assets, 0, plan["cursor"])
            plan["center"] = tuple(source[UNITS:UNITS + 2])
            try: checkpoint("use-" + attempt + "-first-raw0", target)
            finally: plan["target"], plan["center"] = original_target, center
            capture.key("Z")
        checkpoint("use-" + attempt + "-target", target)
        if attempt == "cancel":
            capture.key("ESC"); checkpoint("use-target-cancel-list-zero", lambda frame: list_page(frame, templates, fields))
            capture.key("ESC"); checkpoint("use-list-cancel-item-zero", lambda frame: submenu_page(frame, assets,
                screen(source, tuple(actor["world"])), templates, fields, role=report.get("kind", "original")))
        elif attempt == "destination-cancel":
            capture.key("ENTER")
            checkpoint("use23-destination-cancel-frame0", destination_start)
            capture.key("ESC")
            checkpoint("use23-destination-cancel-list-zero", lambda frame: list_page(frame, templates, fields))
            capture.key("ESC")
            checkpoint("use23-destination-cancel-item-zero", lambda frame: submenu_page(frame, assets,
                screen(source, tuple(actor["world"])), templates, fields, role=report.get("kind", "original")))
    capture.key("ENTER")  # 唯一实际Use提交；23仍须目的地一次肯定才执行效果。
    if code == 23:
        checkpoint("use23-destination-initial-frame0", destination_start)
        capture.key("RIGHT")
        at = screen(source, tuple(plan["contract"]["destination"]))
        def destination_selected(frame):
            focus = RoundOracle(assets, at).closed_browse(frame)
            restored = exact_tile(frame, plan["range"]["base"], plan["range"]["position"])
            return dict(kind="use23_destination_selected", browse=focus, cast_lut_cleared=restored) if focus and restored else None
        checkpoint("use23-destination-right", destination_selected)
        capture.key("ENTER")  # 独立destination一次肯定，不是第二次施放。
    predicate = lambda frame: use_completed_browse(frame, source, assets, templates, fields)
    if report.get("kind", "original") == "original":
        state = {"host_stable_seconds": 2.0, "first": None, "samples": 0, "resets": 0, "wgc_retries": 0,
            "scope": "no-key host resource stability; controller119a6 BIOS clear not directly observed"}
        report["use_resource_quiet"] = state
        def quiet(frame):
            retries = sum(row.get("kind") == "wgc_transient" and row["name"].startswith("use-once-complete-wgc-retry-") for row in report["checkpoints"])
            result = predicate(frame)
            if not result or retries != state["wgc_retries"]:
                state.update(first=None, samples=0, resets=state["resets"] + 1, wgc_retries=retries)
                if not result: return None
            now = time.monotonic()
            if state["first"] is None: state["first"] = now
            state["samples"] += 1
            return dict(result, resource_quiet=dict(state)) if now - state["first"] >= 2.0 and state["samples"] >= 2 else None
        ready = quiet
    else: ready = predicate
    _, report["use_outcome"] = checkpoint("use-once-complete", ready, timeout=120)
    capture.key("F2")
    _, report["use_immediate_detail"] = checkpoint("use-immediate-f2-detail", lambda frame: use_detail(frame, assets))
    observed = report["use_immediate_detail"].get("finite_native_outcome_indices", [])
    if (not observed or len(observed) != len(set(observed)) or
            any(type(index) is not int or not 0 <= index < len(plan["variants"]) for index in observed)):
        raise AssertionError("Use即时F2未提供该端有效的native有限结果；不发送关闭/END/Save")
    endpoint_plan = dict(plan, expected_sources=[plan["variants"][index]["final"] for index in observed],
        expected_source_indices=list(observed))  # 仅此端的观测；绝不写共享plan或跨端保护RNG结果。
    capture.key("ESC")
    if "target_magic_bottom" in plan:
        at, image = plan["target_magic_bottom"]
        checkpoint("use-target-magic-unselected", lambda frame: dict(kind="magic_unselected_detail", **found) if (found := exact_tile(frame, image, at)) else None)
        capture.key("ESC")
    checkpoint("use-f2-closed-browse", predicate)
    exercise_use_additional_details(capture, source, assets, report, checkpoint)
    for index, (key, resource) in enumerate(plan["end_navigation"], 1):
        capture.key(key)
        checkpoint(f"use-end-empty-step-{index}", lambda frame, item=resource:
            post_use_navigation_state(frame, source, assets, item))
    # END共用小接口；仅传必要的页/背景资源，pending/RNG不另做保存。
    assets["give"] = {"end_banner": plan["end_banner"], "terrain": plan["terrain"],
        "after_templates": plan["variants"][0]["caster_templates"], "after_fields": plan["variants"][0]["caster_fields"]}
    execute_end(capture, assets, screen(source, (6, 14)), plan["variants"][0]["final"], report, checkpoint, "use")
    measure_inventory_save(capture, assets, endpoint_plan, save, data, stamp, report, checkpoint,
        prefix="use", measurement_key="use_measurement")
    report["input_scope"] = {"valid_use_confirmations": 1, "destination_confirmation_count": int(code == 23),
        "target_cancel_count": 1, "destination_cancel_count": int(code == 23),
        "first_target_confirmation_count": 2 if code == 23 else 1, "end_confirmations": 1, "explicit_save_count": 1,
        "post_use_skip_keys": [], "rng_rerolled": False, "natural_acquisition_or_single_turn_route_claimed": False}


def give_target(frame, assets, templates, fields):
    plan = assets["give"]
    focus = focused_actor(frame, assets, plan["target"])
    if not focus or focus["focus_screen"] != list(screen(plan["runtime"], tuple(plan["target"]["world"]))) or not list_absent(frame, templates, fields):
        return None
    probe = plan["probe"]
    for phase, image in probe["ranges"]:
        found = exact_tile(frame, image, probe["position"])
        if found:
            return {"kind": "give_target", "selector_filter": 3, "selection_frame": 0, "actor": focus,
                "range_lut": phase, "range": found, "range_roi": [*probe["position"], 24, 24]}
    return None


def execute_end(capture, assets, at, source, report, checkpoint, prefix):
    oracle = RoundOracle(assets, at)
    capture.key("ENTER"); checkpoint(prefix + "-end-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("DOWN"); checkpoint(prefix + "-end-selected", lambda frame: oracle.menu(frame, 3))
    capture.key("ENTER"); checkpoint(prefix + "-end-default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")
    if report.get("kind", "original") == "original":
        def banner(frame):
            found = match_indexed(frame, assets["give"]["end_banner"], (120, 84))
            return dict(kind="round_banner", turn=2, **found) if found["ok"] else None
        _, report["native_round_banner"] = wait_state(capture, prefix + "-round-002", banner, [], timeout=90)
    else:
        from field_play_input_test import wait_log
        text = wait_log(capture.process, capture.artifacts / "game.log", "field phase: turn=2 phase=2 ", timeout=90)
        check_round_phase_log(text)
        report["end_phase_log_passed"] = True
    leader = actor_focus_resources(Path(report["original_path"]), source, assets, 0, assets["cursor"])
    checkpoint(prefix + "-next-player-browse", lambda frame: focused_browse(frame, source, assets, leader,
        assets["give"]["terrain"], assets["give"]["after_templates"], assets["give"]["after_fields"]), timeout=45)


def exercise_give(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint):
    plan = assets["give"]
    role = report.get("kind", "original")
    position = screen(plan["runtime"], tuple(plan["runtime_actor"]["world"]))
    after_templates, after_fields = plan["after_templates"], plan["after_fields"]
    def submenu(frame, t=templates, f=fields, selected=0):
        return submenu_page(frame, assets, position, t, f, selected, role=role)
    # 先逐层取消，再重开一次执行；取消绝不触及库存/acted。
    attempts = ("source-cancel", "target-cancel", "recipient-cancel") if report["scenario"] == "give-cancel" else ("target-cancel", "commit")
    for attempt in attempts:
        capture.key("LEFT"); checkpoint("give-" + attempt + "-selected", lambda frame: submenu(frame, selected=1))
        capture.key("ENTER"); checkpoint("give-" + attempt + "-source-list-zero", lambda frame: list_page(frame, templates, fields))
        if attempt == "source-cancel":
            capture.key("ESC"); checkpoint("give-source-cancel-item-zero", submenu); continue
        capture.key("ENTER"); checkpoint("give-" + attempt + "-target", lambda frame: give_target(frame, assets, templates, fields))
        if attempt == "target-cancel":
            capture.key("ESC"); checkpoint("give-target-cancel-item-zero", submenu); continue
        capture.key("ENTER")
        if report["scenario"] in ("give-cancel", "give-swap"):
            checkpoint("give-recipient-full-list-zero", lambda frame: list_page(frame, plan["recipient_templates"], plan["recipient_fields"]))
            capture.key("ESC" if attempt == "recipient-cancel" else "ENTER")
        checkpoint("give-" + attempt + "-item-zero", lambda frame: submenu(frame, after_templates, after_fields))
    capture.key("ESC")
    checkpoint("give-item-cancel-command", lambda frame: command_page(frame, PlayerOracle(source, assets, [], {}), position, after_templates, after_fields))
    capture.key("ESC")
    if report["scenario"] == "give-cancel":
        checkpoint("give-command-cancel-selection", lambda frame: selection_state(frame, assets, actor, probe))
        capture.key("ESC")
    checkpoint("give-command-cancel-browse", lambda frame: focused_browse(frame, plan["after_source"], assets,
        plan["sender"], plan["terrain"], after_templates, after_fields))
    if report["scenario"] != "give-cancel":
        # 即时F2锁收方stale派生，不能让END后的量测掩盖过早重算。
        for index, (key, world, at) in enumerate(focus_steps(plan["after_source"], plan["endpoint"], plan["recipient"]["world"]), 1):
            capture.key(key)
            predicate = (lambda frame: focused_browse(frame, plan["after_source"], assets, plan["recipient"], plan["terrain"], after_templates, after_fields)) if world == tuple(plan["recipient"]["world"]) else RoundOracle(assets, at).closed_browse
            checkpoint(f"give-recipient-focus-{index}", predicate)
        capture.key("F2")
        _, report["give_recipient_detail"] = checkpoint("give-recipient-immediate-detail",
            lambda frame: inventory_detail(frame, plan["detail_templates"], plan["detail_fields"]))
        capture.key("ESC")
        at, image = plan["magic_bottom"]
        checkpoint("give-recipient-magic-unselected", lambda frame: dict(kind="magic_unselected_detail", **found)
            if (found := exact_tile(frame, image, at)) else None)
        capture.key("ESC")
        checkpoint("give-recipient-detail-closed", lambda frame: focused_browse(frame, plan["after_source"], assets,
            plan["recipient"], plan["terrain"], after_templates, after_fields))
        for index, (key, world, at) in enumerate(focus_steps(plan["after_source"], plan["recipient"]["world"], plan["endpoint"]), 1):
            capture.key(key)
            predicate = (lambda frame: focused_browse(frame, plan["after_source"], assets, plan["sender"], plan["terrain"], after_templates, after_fields)) if world == plan["endpoint"] else RoundOracle(assets, at).closed_browse
            checkpoint(f"give-sender-return-{index}", predicate)
        key, at = plan["steps"][0]
        capture.key(key); checkpoint("give-end-empty-cell", RoundOracle(assets, at).closed_browse)
        execute_end(capture, assets, at, plan["expected_source"], report, checkpoint, "give")
    measure_inventory_save(capture, assets, plan, save, data, stamp, report, checkpoint,
        prefix="give", measurement_key="give_measurement")
    report["input_scope"] = {"give_success_count": int(report["scenario"] != "give-cancel"),
        "cancel_counts": {"source": int(report["scenario"] == "give-cancel"), "target": 1, "recipient": int(report["scenario"] == "give-cancel")},
        "end_confirmations": int(report["scenario"] != "give-cancel"), "explicit_save_count": 1,
        "source_movement_committed": report["scenario"] == "give-moved", "post_use_skip_keys": []}


def prepare_herb(original, source, assets, probe, fields):
    """item192：20c6f→211a4→1c916一次HP恢复→1b8e7 slot2移除。

    玩家范围读record+10/+12/+15（十六进制），不复用AI的+11。
    只覆盖本exact START中心唯一raw0，不把Manhattan用于通用地图判定。
    """
    costs, movement = movement_profile(original / "FD2.EXE", 0)
    if costs != bytes([1] * 20) or abs(probe["world"][0] - 7) + abs(probe["world"][1] - 14) != 1:
        raise ValueError("草药独立范围ROI不满足已确认profile0/邻格可达门")
    record = bytes.fromhex(fields["slots"][2]["record_hex"])
    if (record[13], record[16], record[18], record[21]) != (5, 1, 0, 1):
        raise ValueError("草药code/cast/area/filter字段不符")
    candidates = [i for i in range(source[META + 1])
        if not source[UNITS + i * STRIDE + 5] & 1 and source[UNITS + i * STRIDE + 6] != 0 and
        abs(source[UNITS + i * STRIDE] - 7) + abs(source[UNITS + i * STRIDE + 1] - 14) <= 1]
    if candidates != [0] or any(source[UNITS + 0x1a:UNITS + 0x1f]):
        raise ValueError("草药目标或F2单页关闭前置偏离已确认START")
    other = (original / "FDOTHER.DAT").read_bytes()
    sheet = archive_entry(other, 1)
    cursors = []
    for frame in (0, 1):
        begin, end = struct.unpack_from("<II", sheet, 6 + frame * 4)
        audit = audit_fdicon(struct.pack("<HHHI", 24, 24, 1, 10) + sheet[begin:end], expected_frames=1)
        if audit["opaque_zero_pixels"]:
            raise ValueError("草药光标含opaque0，不能用非零mask合成")
        image = sheet_frame(sheet, frame)
        image.putpalette(assets["palette"])
        cursors.append(image)
    target_actor = actor_focus_resources(original, source, assets, 0, cursors[1])
    sprites = (original / "FDICON.B24").read_bytes()
    acted = []
    for frame in range(12):
        begin, end = struct.unpack_from("<II", sprites, 6 + frame * 4)
        audit = audit_fdicon(struct.pack("<HHHI", 24, 24, 1, 10) + sprites[begin:end], expected_frames=1)
        if audit["opaque_zero_pixels"]:
            raise ValueError("草药actor含opaque0，不能用非零mask合成")
        sprite = sheet_frame(sprites, frame)
        sprite.putpalette(assets["palette"])
        composite = Image.new("P", (24, 30), 0)
        composite.putpalette(assets["palette"])
        for part, at in ((cursors[0], (0, 6)), (acted_sprite(sprite), (0, 0))):
            composite.paste(part, at, part.point(lambda color: 255 if color else 0, mode="L"))
        acted.append(composite)
    _, _, _, after_templates, after_fields = prepare_templates(original, original / "START.SAV", source=source, after_use=True)
    assets["herb"] = {"target_actor": target_actor, "target_cursor": cursors[1], "acted_composites": acted,
                      "after_templates": after_templates, "after_fields": after_fields,
                      "detail_close_terrain": detail_close_terrain(original, source, assets)}
    fields["herb_contract"] = {"item_id": 192, "use_code": 5, "power": 40,
        "cast": 1, "area": 0, "filter": 1, "record_offsets_hex": ["10", "12", "15"],
        "target_candidates": candidates, "initial_target": 0, "target_frame": 1, "selection_mode": 2,
        "profile0_costs_all_one": True, "movement_table": movement,
        "target_world": [7, 14], "target_screen": [52, 76], "camera": [5, 11],
        "range_roi": [*probe["position"], 24, 24],
        "expected_after_inventory_hex": AFTER_USE_INVENTORY, "runtime_raw_inventory_measured": False,
        "raw_heal_static_range": [36, 39], "floating_raw_heal_and_rng_measured": False,
        "deferred_post_end_measurement": "separate original case: END -> next player phase -> legal Save; not executed here"}


def drop_expected_source(source, table, scenario="drop-herb"):
    """1bbdc selector3→1b8e7→1b750；只建锁定完整来源的静态预期。"""
    if scenario not in DROP_SCENARIOS:
        raise ValueError("未知Drop模型")
    source_sha = DROP_LAST_PLAIN_SHA256 if scenario == "drop-last" else PLAIN_SHA256
    if hashlib.sha256(source).hexdigest() != source_sha or len(table) != 215 * 23:
        raise ValueError("Drop模型只支持完整已确认START/最后项manifest及原版215项物品表")
    expected = bytearray(source)
    actor = bytearray(source[UNITS:UNITS + STRIDE])
    before = bytes(actor[0x0a:0x1a])
    selected = DROP_SCENARIOS[scenario]
    if drop_derived(actor, table) != list(struct.unpack_from("<4H", actor, 0x48)):
        raise ValueError("Drop来源的EXE派生值与记录不符")
    item_id = actor[0x0b + selected * 2]
    actor[0x0a + selected * 2:0x18] = actor[0x0c + selected * 2:0x1a]
    actor[0x18] = 0x80
    derived = drop_derived(actor, table)
    values = {"drop-herb": [16, 12, 97, 2], "drop-weapon": [6, 12, 2, 2],
              "drop-armor": [16, 4, 97, 2], "drop-last": [6, 4, 2, 2]}
    if derived != values[scenario]:
        raise ValueError("原版表及1b750公式不满足本场四派生预期")
    struct.pack_into("<4H", actor, 0x48, *derived)
    if scenario == "drop-herb" and actor[0x0a:0x1a].hex() != AFTER_USE_INVENTORY:
        raise ValueError("原始移槽与已确认16字节模型不符")
    expected[UNITS:UNITS + STRIDE] = actor
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    return bytes(expected), {"scenario": scenario, "item_id": item_id, "raw_slot": selected,
        "native_branch": "0x1bfcd..0x1bff4", "remove_va": "0x1b8e7",
        "recompute_va": "0x1b750", "derived_offsets": [0x48, 0x4a, 0x4c, 0x4e],
        "derived_values": derived, "before_inventory_hex": before.hex(),
        "after_inventory_hex": actor[0x0a:0x1a].hex(), "runtime_raw_inventory_measured": False,
        "raw_unit_changed_offsets": [i for i, (a, b) in enumerate(zip(source[UNITS:UNITS + STRIDE], actor)) if a != b]}


def prepare_drop(original, source, assets, actor, fields, scenario="drop-herb"):
    table, evidence = item_table(original / "FD2.EXE")
    expected, contract = drop_expected_source(source, table, scenario)
    _, after_templates, after_fields = inventory_templates(original, expected[UNITS:UNITS + STRIDE], assets)
    # 1bbdc除零库存返回外，唯一运行置禁为Give的相邻filter3门；
    # 本组只改库存/四派生，完整人物几何仍同START，EQT没有菜单类型门。
    fields["submenu"] = dict(fields["submenu"], disabled=[0, 1, 0, 0],
        native_evidence="1bbdc: nonempty; only missing adjacent filter3 disables Give; first enabled Use0")
    after_fields["submenu"] = dict(fields["submenu"], command_default=2 if after_fields["slots"] else 3)
    contract.update(selected_command=3, list_mode=0, item_table=evidence,
                    after_model_scope="slot removal/recompute only; no HP heal, acted or runtime measurement",
                    cancel_recompute_evidence="static 0x1bfda -> 0x1bfeb -> 0x1b750; unchanged four values do not prove runtime writes")
    fields["drop_contract"] = contract
    after_fields = dict(after_fields, after_use_model=False, after_drop_model=True, drop_contract=contract)
    navigation, steps = inventory_save_navigation(source, actor)
    assets["drop"] = {"expected_source": expected, "after_templates": after_templates,
        "after_fields": after_fields, "steps": steps}
    contract["measurement_navigation"] = navigation


def list_page(frame, templates, fields, selected=0):
    if selected not in range(len(fields["slots"])):
        raise ValueError("物品资源门索引超出本场已确认可见库存")
    if frame.size != SIZE:
        return None
    names = (f"selected_{selected}", "actor_name", "current_max_hp", "current_max_mp")
    if "herb_contract" in fields or "drop_contract" in fields or "equipment_contract" in fields:
        names += ("level", "experience")
    if "equipment_contract" in fields or fields.get("drop_contract", {}).get("scenario") in ("drop-weapon", "drop-armor", "drop-last"):
        names += ("attack", "defense", "accuracy", "evasion")
    checks = [exact_tile(frame, templates[name][1], templates[name][0]) for name in names]
    if any(check is None for check in checks):
        return None
    slot = fields["slots"][selected]
    detail = {"kind": "use_inventory_list", "selected_raw_slot": selected,
            "item_id": slot["item_id"], "use_code": slot["use_code"],
            "checked_pixels": sum(check["checked_pixels"] for check in checks),
            "max_channel_delta": max(check["max_channel_delta"] for check in checks),
            "rectangles": [{"name": name, "position": list(templates[name][0]),
                            "size": list(templates[name][1].size), **check} for name, check in zip(names, checks)],
            "complete_bottom_checked": True, "hp": fields["hp"], "max_hp": fields["max_hp"],
            "mp": fields["mp"], "max_mp": fields["max_mp"]}
    if "attack" in names:
        detail["visible_derived_values"] = fields["derived_values"]
    return detail


def list_absent(frame, templates, fields):
    return not any(list_page(frame, templates, fields, selected) for selected in range(len(fields["slots"])))


def submenu_page(frame, assets, position, templates, fields, selected=0, *, role="original"):
    if role not in ("original", "sdl"):
        raise ValueError("未知物品端点角色")
    disabled = fields["submenu"]["disabled"]
    if selected not in range(4) or disabled[selected]:
        raise ValueError("当前物品子菜单禁用项不能作为选中项")
    matches = []
    for item, command in enumerate((8, 9, 10, 11)):
        index = command * 3 + (2 if disabled[item] else int(item == selected))
        at = tuple(a + b for a, b in zip(position, MENU_OFFSETS[item]))
        images = assets["upper_commands"][index] if item == 0 else [assets["commands"][index]]
        check = next((dict(frame=index, position=list(at), **result) for image in images
                      if (result := match_indexed(frame, image, at))["ok"]), None)
        if not check:
            return None
        matches.append(check)
    if not list_absent(frame, templates, fields):
        return None
    return {"kind": "item_submenu", "selected": selected, "disabled": disabled, "role": role,
            "matches": matches, "inventory_list_absent": True, "operation_confirmed": False,
            "capability_equivalence_claimed": False}


def command_page(frame, command, position, templates, fields, selected=None):
    selected = fields["submenu"]["command_default"] if selected is None else selected
    disabled = fields["submenu"].get("command_disabled", [1, 1, int(not fields["slots"]), 0])
    expected = [i * 3 + (2 if value else int(i == selected)) for i, value in enumerate(disabled)]
    detail = command.command(frame, position, selected)
    if not detail or [match["frame"] for match in detail["matches"]] != expected:
        return None
    return {**detail, "native_first_enabled_item": selected == 2,
            "native_first_enabled_wait_after_empty_drop": selected == 3,
            "inventory_list_absent": True} if list_absent(frame, templates, fields) else None


def cancelled_browse(frame, assets, actor, probe, templates, fields, *, role="original"):
    focused = focused_actor(frame, assets, actor)
    terrain = exact_tile(frame, probe["base"], probe["position"])
    if (focused and tuple(focused["focus_screen"]) == probe["actor_position"] and terrain and
            list_absent(frame, templates, fields) and
            not submenu_page(frame, assets, probe["actor_position"], templates, fields, role=role)):
        return {"kind": "cancelled_browse", "actor": focused, "untranslated_terrain": terrain}
    return None


def herb_target(frame, assets, probe, templates, fields):
    focused = focused_actor(frame, assets, assets["herb"]["target_actor"])
    if not focused or focused["focus_screen"] != [52, 76] or not list_absent(frame, templates, fields):
        return None
    for phase, image in probe["ranges"]:
        found = exact_tile(frame, image, probe["position"])
        if found:
            return {"kind": "herb192_target", "selection_mode": 2, "selection_frame": 1,
                    "actor": focused, "range_lut": phase, "range_roi": [*probe["position"], 24, 24],
                    "range": found, "target_confirmation_sent": False}
    return None


def herb_browse(frame, source, assets, probe, templates, fields, *, role="original"):
    plan = assets["herb"]
    acted = next((phase for phase, image in enumerate(plan["acted_composites"])
                  if match_indexed(frame, image, (52, 70))["ok"]), None)
    base = exact_tile(frame, probe["base"], probe["position"])
    terrain = plan["detail_close_terrain"]
    restored = exact_tile(frame, terrain["image"], terrain["position"])
    if acted is None or base is None or restored is None or not list_absent(frame, templates, fields):
        return None
    if PlayerOracle(source, assets, [], {}).command(frame, (52, 76)) or any(
            submenu_page(frame, assets, (52, 76), templates, fields, selected, role=role)
            for selected in range(4) if not fields["submenu"]["disabled"][selected]):
        return None
    return {"kind": "herb192_acted_browse", "acted_sprite_cursor_frame_candidate": acted,
            "ordinary_cursor_frame": 0, "sprite_cursor_roi": [52, 70, 24, 30],
            "untranslated_terrain": base,
            "restored_detail_terrain": {**{key: value for key, value in terrain.items() if key != "image"}, **restored}}


def herb_detail(frame, assets):
    if frame.size != SIZE:
        return None
    plan = assets["herb"]
    templates, fields = plan["after_templates"], plan["after_fields"]
    names = ("unselected", "actor_name", "current_max_hp", "current_max_mp", "level", "experience")
    checks = [exact_tile(frame, templates[name][1], templates[name][0]) for name in names]
    if any(check is None for check in checks):
        return None
    return {"kind": "herb192_two_slot_detail", "checked_pixels": sum(check["checked_pixels"] for check in checks),
            "rectangles": [{"name": name, "position": list(templates[name][0]), "size": list(templates[name][1].size), **check}
                           for name, check in zip(names, checks)],
            **{key: fields[key] for key in ("hp", "max_hp", "mp", "max_mp", "level", "experience")},
            "visible_inventory_ids": [item["item_id"] for item in fields["slots"]],
            "expected_inventory_hex_static_only": AFTER_USE_INVENTORY, "runtime_raw_inventory_measured": False}


def exercise_herb(capture, source, assets, probe, templates, fields, report, checkpoint):
    role = report.get("kind", "original")
    capture.key("ENTER")
    checkpoint("herb-use-list-first", lambda frame: list_page(frame, templates, fields))
    capture.key("UP")
    checkpoint("herb-selected-slot2", lambda frame: list_page(frame, templates, fields, 2))
    capture.key("ENTER")  # 仅打开目标选择；不是实际Use提交。
    checkpoint("herb-target-first", lambda frame: herb_target(frame, assets, probe, templates, fields))
    capture.key("ESC")
    checkpoint("herb-target-cancel-list-zero", lambda frame: list_page(frame, templates, fields))
    capture.key("UP")
    checkpoint("herb-selected-slot2-again", lambda frame: list_page(frame, templates, fields, 2))
    capture.key("ENTER")
    checkpoint("herb-target-reopened", lambda frame: herb_target(frame, assets, probe, templates, fields))
    capture.key("ENTER")  # 本场唯一实际Use确认；之后只观察，不跳过演出/提示。
    _, report["herb_outcome"] = checkpoint("herb-once-used-acted-browse",
        lambda frame: herb_browse(frame, source, assets, probe, templates, fields, role=role), timeout=120)
    capture.key("F2")
    _, report["herb_measurement"] = checkpoint("herb-two-slot-f2-detail", lambda frame: herb_detail(frame, assets))
    capture.key("ESC")  # raw0无魔法，17aed一页后直接关闭。
    _, report["herb_detail_closed"] = checkpoint("herb-f2-closed-browse",
        lambda frame: herb_browse(frame, source, assets, probe, templates, fields, role=role))
    report["input_scope"] = {"valid_use_confirmations": 1, "target_open_count": 2, "target_cancel_count": 1,
        "post_use_skip_keys": [], "equip_drop_give_confirmations": 0, "save_or_end_sent": False,
        "runtime_raw_inventory_measured": False,
        "deferred_post_end_measurement": fields["herb_contract"]["deferred_post_end_measurement"]}


def drop_browse(frame, source, assets, actor, probe, templates, fields, *, role="original"):
    detail = cancelled_browse(frame, assets, actor, probe, templates, fields, role=role)
    position = probe["actor_position"]
    if (not detail or PlayerOracle(source, assets, [], {}).command(frame, position) or
            any(submenu_page(frame, assets, position, templates, fields, selected, role=role)
                for selected in range(4) if not fields["submenu"]["disabled"][selected])):
        return None
    return {**detail, "kind": "drop_unacted_browse", "raw_acted_measured": False}


def drop_list_page(frame, templates, fields, selected=0):
    detail = list_page(frame, templates, fields, selected)
    return {**detail, "kind": "drop_inventory_list", "operation": "drop", "list_mode": 0,
            "mode_evidence": "1bbdc selector3 calls 1b932(actor,0); shared resource pixels"} if detail else None


def equipment_page(frame, templates, fields, selected=0):
    detail = list_page(frame, templates, fields, selected)
    return {**detail, "kind": "equipment_inventory_list", "operation": "equip", "list_mode": 0,
        "visible_inventory_flags": [row["flags"] for row in fields["slots"]],
        "mode_evidence": "sub_1BFFE resets index0; repeated sub_1B9DE(actor,0), stays after each confirmation"} if detail else None


def exercise_equipment(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint):
    role = report.get("kind", "original")
    plan = assets["equipment"]
    after_templates, after_fields = plan["after_templates"], plan["after_fields"]
    position = screen(source, tuple(actor["world"]))
    capture.key("RIGHT")
    checkpoint("eqt-selected", lambda frame: submenu_page(frame, assets, position, templates, fields, 2, role=role))
    capture.key("ENTER")
    checkpoint("eqt-list-zero", lambda frame: equipment_page(frame, templates, fields))
    for selected in (1, 2):
        capture.key("DOWN")
        checkpoint(f"eqt-selected-slot{selected}", lambda frame, index=selected: equipment_page(frame, templates, fields, index))
    keys = ("ENTER",) if fields["equipment_contract"]["compatible"] else ("ENTER", "SPACE")
    for index, key in enumerate(keys, 1):
        capture.key(key)
        _, report["equipment_confirmation"] = checkpoint(f"eqt-confirm-{index}-stays-slot2",
            lambda frame: equipment_page(frame, after_templates, after_fields, 2))
    capture.key("ESC")
    checkpoint("eqt-cancel-back-item-zero", lambda frame: submenu_page(frame, assets, position, after_templates, after_fields, role=role))
    # 完整重新进入必须归0；不能把旧slot2实帧当作新打开的列表0。
    capture.key("RIGHT")
    checkpoint("eqt-reopen-selected", lambda frame: submenu_page(frame, assets, position, after_templates, after_fields, 2, role=role))
    capture.key("ENTER")
    checkpoint("eqt-reopened-list-zero", lambda frame: equipment_page(frame, after_templates, after_fields))
    capture.key("ESC")
    checkpoint("eqt-reopened-cancel-item-zero", lambda frame: submenu_page(frame, assets, position, after_templates, after_fields, role=role))
    capture.key("ESC")
    command = PlayerOracle(source, assets, [], {})
    checkpoint("eqt-item-cancel-command", lambda frame: command_page(frame, command, position, after_templates, after_fields))
    capture.key("ESC")
    checkpoint("eqt-command-cancel-selection", lambda frame: selection_state(frame, assets, actor, probe))
    capture.key("ESC")
    def browse(frame):
        detail = drop_browse(frame, source, assets, actor, probe, after_templates, after_fields, role=role)
        return {**detail, "kind": "equipment_unacted_browse"} if detail else None
    checkpoint("eqt-selection-cancel-unacted-browse", browse)
    measure_inventory_save(capture, assets, plan, save, data, stamp, report, checkpoint,
        prefix="eqt", measurement_key="equipment_measurement")
    report["input_scope"] = {"compatible_equipment_confirmations": int(fields["equipment_contract"]["compatible"]),
        "incompatible_confirmation_keys": [] if fields["equipment_contract"]["compatible"] else list(keys),
        "list_reopened_at_zero": True, "use_drop_give_confirmations": 0, "explicit_measurement_save_count": 1,
        "end_sent": False, "post_equipment_skip_keys": [], "runtime_ui_rng_alignment": "unverified"}


def measure_inventory_save(capture, assets, plan, save, data, stamp, report, checkpoint, *, prefix, measurement_key):
    """Drop/EQT共同的正式UI Save与整容器量测；不扩差异范围。"""
    for index, (key, at) in enumerate(plan["steps"], 1):
        capture.key(key)
        checkpoint(f"{prefix}-save-empty-step-{index}", RoundOracle(assets, at).closed_browse)
    oracle = RoundOracle(assets, plan["steps"][-1][1])
    capture.key("ENTER")
    checkpoint(f"{prefix}-save-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("ENTER")
    checkpoint(f"{prefix}-save-secondary", lambda frame: oracle.secondary(frame, 0, overview_available=True))
    capture.key("LEFT")
    checkpoint(f"{prefix}-save-selected", lambda frame: oracle.secondary(frame, 1, overview_available=True))
    capture.key("ENTER")
    checkpoint(f"{prefix}-save-default-yes", lambda frame: oracle.buttons(frame, 0))
    require_source_unchanged(save, data, stamp)
    report[prefix + "_save_requested"] = True
    capture.key("ENTER")  # 唯一正式UI Save肯定；之后不再要求副本等于源。
    measured = wait_measured_save(save, data, stamp)
    encoded = save.read_bytes()
    saved = capture.artifacts / f"{prefix}-measured.SAV"
    saved.write_bytes(encoded)  # 即使严格比较随后失败，也先保留真实测量。
    if decode_save(saved) != measured:
        raise AssertionError("库存测量SAV在writer完成后仍变化")
    if "expected_sources" in plan:
        matches = []
        first_error = None
        for index, expected in enumerate(plan["expected_sources"]):
            try:
                comparison = check_load_measurement(expected, measured, plan["steps"])
                matches.append(index)
            except AssertionError as error:
                if first_error is None: first_error = error
        if not matches:
            raise AssertionError("Use真实SAV不属于任何完整native有限结果；" + str(first_error))
        comparison = {**comparison, "finite_native_outcome_indices": matches,
            "random_field_policy": "complete container matched to native finite records; no generic byte allowlist"}
        if "expected_source_indices" in plan:
            comparison["finite_native_outcome_indices"] = [plan["expected_source_indices"][index] for index in matches]
            comparison["immediate_f2_native_outcome_indices"] = plan["expected_source_indices"]
    else:
        comparison = check_load_measurement(plan["expected_source"], measured, plan["steps"])
    completed_stamp = save.stat().st_mtime_ns
    checkpoint(f"{prefix}-saved-browse", oracle.closed_browse, source_guard=False)
    report[measurement_key] = {**comparison, "runtime_raw_inventory_measured": True,
        "measured_inventory_hex": measured[UNITS + 0x0a:UNITS + 0x1a].hex(),
        "measured_derived_values": list(struct.unpack_from("<4H", measured, UNITS + 0x48)),
        "raw_flags": measured[UNITS + 5], "unit_experience": measured[UNITS + 0x3c],
        "snapshot": str(saved), "snapshot_sha256": hashlib.sha256(encoded).hexdigest()}
    report["completed_save"] = {"snapshot": str(saved), "sha256": hashlib.sha256(encoded).hexdigest(),
                                "mtime_ns": completed_stamp}
    require_source_unchanged(save, encoded, report["completed_save"]["mtime_ns"])


def exercise_drop(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint):
    plan = assets["drop"]
    after_templates, after_fields = plan["after_templates"], plan["after_fields"]
    position = screen(source, tuple(actor["world"]))
    selected_slot = fields["drop_contract"]["raw_slot"]
    command = PlayerOracle(source, assets, [], {})
    role = report.get("kind", "original")
    for attempt in ("cancel", "commit"):
        capture.key("DOWN")
        checkpoint(f"drop-{attempt}-selected", lambda frame: submenu_page(frame, assets, position, templates, fields, 3, role=role))
        capture.key("ENTER")
        checkpoint(f"drop-{attempt}-list-zero", lambda frame: drop_list_page(frame, templates, fields))
        for selected in range(1, selected_slot + 1):
            capture.key("DOWN")
            checkpoint(f"drop-{attempt}-slot{selected}", lambda frame, index=selected: drop_list_page(frame, templates, fields, index))
        capture.key("ESC" if attempt == "cancel" else "ENTER")
        active_templates, active_fields = (templates, fields) if attempt == "cancel" else (after_templates, after_fields)
        if active_fields["slots"]:
            checkpoint(f"drop-{attempt}-back-item-zero", lambda frame, t=active_templates, f=active_fields:
                       submenu_page(frame, assets, position, t, f, role=role))
        else:
            _, report["drop_empty_command"] = checkpoint("drop-commit-back-empty-command",
                lambda frame: command_page(frame, command, position, active_templates, active_fields))
    if after_fields["slots"]:
        capture.key("ENTER")  # Use只读看剩余两槽；不确认任何物品。
        _, report["drop_two_slot_page"] = checkpoint("drop-after-use-list-two-slots",
            lambda frame: list_page(frame, after_templates, after_fields))
        capture.key("ESC")
        checkpoint("drop-readonly-list-cancel-item", lambda frame: submenu_page(frame, assets, position, after_templates, after_fields, role=role))
        capture.key("ESC")
        checkpoint("drop-item-cancel-command", lambda frame: command_page(frame, command, position, after_templates, after_fields))
    capture.key("ESC")
    checkpoint("drop-command-cancel-selection", lambda frame: selection_state(frame, assets, actor, probe))
    capture.key("ESC")
    checkpoint("drop-selection-cancel-unacted-browse",
        lambda frame: drop_browse(frame, source, assets, actor, probe, after_templates, after_fields, role=role))
    measure_inventory_save(capture, assets, plan, save, data, stamp, report, checkpoint,
        prefix="drop", measurement_key="drop_measurement")
    report["input_scope"] = {"drop_confirmation_count": 1, "drop_cancel_count": 1,
        "valid_use_confirmations": 0, "equip_give_confirmations": 0,
        "explicit_measurement_save_count": 1, "end_sent": False, "post_drop_skip_keys": [], "drop_retry_count": 0,
        "cancel_recompute_dynamic_write_observed": False,
        "use_acted_raw_inventory_measured": False}


def exercise(capture, source, focus, assets, actor, probe, templates, fields, titles, save, data, stamp, report, *, scenario="controls"):
    role = report.get("kind", "original")
    def checkpoint(name, predicate, *, source_guard=True, **kwargs):
        frame, detail = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        if source_guard:
            require_source_unchanged(save, data, stamp)
        return frame, detail

    wait_title(capture, titles[3][0])
    save_checkpoint(capture, "title-start", capture.last, {"kind": "title", "selected": 0}, report["checkpoints"])
    capture.key("UP")
    capture.wait(titles[3][2])
    save_checkpoint(capture, "title-continue", capture.last, {"kind": "title", "selected": 2}, report["checkpoints"])
    capture.key("ENTER")
    checkpoint("continued-source-browse", RoundOracle(assets, focus).initial_browse, timeout=45)
    capture.key("Z")
    _, focused = checkpoint("z-raw0", lambda frame: focused_actor(frame, assets, actor))
    position = screen(source, tuple(actor["world"]))
    if tuple(focused["focus_screen"]) != position:
        raise AssertionError("物品校准聚焦出现未确认的卷屏；未发送后续输入")
    capture.key("ENTER")
    checkpoint("raw0-movement-selection", lambda frame: selection_state(frame, assets, actor, probe))
    if scenario == "give-moved":
        at = screen(source, (7, 15))
        selection = assets["give"]["move_selection_probe"]
        capture.key("DOWN")
        def moved_selection(frame):
            focus = match_indexed(frame, assets["cursor"], at)
            for phase, image in selection["ranges"]:
                found = exact_tile(frame, image, selection["position"])
                if focus["ok"] and found:
                    return dict(kind="give_move_selection", destination=[7, 15], range_lut=phase, range=found, cursor=focus)
            return None
        checkpoint("give-one-step-focus-selected", moved_selection)
    capture.key("ENTER")
    command = PlayerOracle(source, assets, [], {})
    if scenario == "give-moved":
        position = screen(source, (7, 15))
    checkpoint("raw0-zero-move-command", lambda frame: command_page(frame, command, position, templates, fields))
    capture.key("ENTER")
    checkpoint("item-submenu-default-use", lambda frame: submenu_page(frame, assets, position, templates, fields, role=role))
    if scenario in GIVE_SCENARIOS:
        exercise_give(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint)
        return
    if scenario in USE_SCENARIOS:
        exercise_use(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint)
        return
    if scenario in DROP_SCENARIOS:
        exercise_drop(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint)
        return
    if scenario in EQUIPMENT_SCENARIOS:
        exercise_equipment(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint)
        return
    if scenario != "controls":
        exercise_herb(capture, source, assets, probe, templates, fields, report, checkpoint)
        return
    for key, selected in (("RIGHT", 2), ("DOWN", 3), ("UP", 0), ("LEFT", 0)):
        capture.key(key)
        checkpoint(f"submenu-{key.lower()}", lambda frame, index=selected:
                   submenu_page(frame, assets, position, templates, fields, index, role=role))
    capture.key("ENTER")
    checkpoint("use-list-first", lambda frame: list_page(frame, templates, fields))
    # 仅槽0/1的code0项发确认；药草code5永远只导航，不进入目标或效果。
    steps = (("ENTER", 0), ("SPACE", 0), ("DOWN", 1), ("ENTER", 1), ("SPACE", 1),
             ("RIGHT", 1), ("LEFT", 1), ("DOWN", 2), ("DOWN", 0), ("UP", 2))
    previous = 0
    for index, (key, selected) in enumerate(steps, 1):
        if key in ("ENTER", "SPACE") and fields["slots"][previous]["use_code"] != 0:
            raise AssertionError("拒绝对非0code物品发送确认")
        capture.key(key)
        checkpoint(f"use-list-{index}-{key.lower()}", lambda frame, slot=selected: list_page(frame, templates, fields, slot))
        previous = selected
    capture.key("ESC")
    checkpoint("list-cancel-item-submenu", lambda frame: submenu_page(frame, assets, position, templates, fields, role=role))
    capture.key("ENTER")
    checkpoint("use-list-reopened-zero", lambda frame: list_page(frame, templates, fields))
    capture.key("ESC")
    checkpoint("list-cancel-item-submenu-again", lambda frame: submenu_page(frame, assets, position, templates, fields, role=role))
    capture.key("ESC")
    checkpoint("submenu-cancel-command", lambda frame: command_page(frame, command, position, templates, fields))
    capture.key("ESC")
    checkpoint("command-cancel-selection", lambda frame: selection_state(frame, assets, actor, probe))
    capture.key("ESC")
    checkpoint("selection-cancel-browse", lambda frame: cancelled_browse(frame, assets, actor, probe, templates, fields, role=role))
    report["input_scope"] = {"valid_use_confirmations": 0, "code0_confirmations": 4,
                             "equip_drop_give_confirmations": 0, "save_or_end_sent": False,
                             "cancel_route": ["Use list", "ITEM submenu", "COMMAND", "selection", "browse"]}


def rgb_difference(first, second, position, size, mask=None):
    x, y = position
    width, height = size
    if first.size != SIZE or second.size != SIZE or not (0 <= x < x + width <= 320 and 0 <= y < y + height <= 200):
        raise ValueError("配对RGB图像或ROI越界")
    difference = ImageChops.difference(first.crop((x, y, x + width, y + height)).convert("RGB"),
                                      second.crop((x, y, x + width, y + height)).convert("RGB"))
    pixels = list(difference.getdata())
    if mask is not None:
        if mask.size != size:
            raise ValueError("配对资源mask尺寸不符")
        pixels = [value for index, value in zip(mask.getdata(), pixels) if index != 0]
    deltas = [max(value) for value in pixels]
    return {"checked_pixels": len(deltas), "bad_pixels": sum(value > 3 for value in deltas),
            "max_channel_delta": max(deltas, default=0), "channel_tolerance": 3,
            "within_tolerance": bool(deltas) and max(deltas) <= 3}


def paired_checkpoint_records(case, folder):
    """wait_state的重试仅是诊断，必须归属于已成功的业务检查点。"""
    records = case["checkpoints"]
    names = [item["name"] for item in records]
    if len(set(names)) != len(names):
        raise AssertionError("端点检查点名称重复")
    business = {item["name"]: item for item in records if item["kind"] != "wgc_transient"}
    positions = {name: index for index, name in enumerate(names)}
    diagnostics, ordinals = [], {}
    for index, event in enumerate(records):
        if event["kind"] != "wgc_transient":
            continue
        base, separator, suffix = event["name"].rpartition("-wgc-retry-")
        target = business.get(base)
        if not separator or not suffix.isascii() or not suffix.isdecimal() or target is None:
            raise AssertionError("WGC诊断名称或关联业务检查点无效")
        number = int(suffix)
        if number < 1 or str(number) != suffix or index >= positions[base]:
            raise AssertionError("WGC诊断序号或业务顺序无效")
        error = event.get("error", "")
        if (event.get("capture_backend") != "Windows.Graphics.Capture HWND" or
                event.get("last_frame_may_precede_error") is not True or not isinstance(error, str) or
                not any(message in error for message in ("blank or nearly uniform captured client area",
                    "window geometry changed during capture", "no visible window for PID ",
                    "截图尺寸不是 320x200 的整数倍率", "截图尺寸过小"))):
            raise AssertionError("WGC诊断类型、后端或瞬态原因无效")
        if (type(event.get("pid")) is not int or event["pid"] <= 0 or event["pid"] != target.get("pid") or
                any(type(row.get("time")) not in (int, float) or not math.isfinite(row["time"])
                    for row in (event, target)) or event["time"] > target["time"]):
            raise AssertionError("WGC诊断PID或时间不属于关联业务检查点")
        for key, ending in (("last_valid_frame", "-last-valid.png"), ("last_raw_capture", "-last-raw.bmp")):
            if key in event:
                filename = event["name"] + ending
                if event[key] != filename or hashlib.sha256((folder / filename).read_bytes()).hexdigest() != case["capture_hashes"].get(filename):
                    raise AssertionError("WGC诊断附件名称或capture hash不符")
        ordinals.setdefault(base, []).append(number)
        diagnostics.append({**event, "business_checkpoint": base, "retry": number})
    for name, item in business.items():
        count = item.get("wgc_transient_retries", 0)
        numbers = ordinals.get(name, [])
        if type(count) is not int or count < 0 or count != len(numbers) or numbers != list(range(1, len(numbers) + 1)):
            raise AssertionError("WGC诊断与业务声明的连续retry数量不符")
    return business, diagnostics


def compare_paired_checkpoints(cases, output, assets, probe):
    """只比较两端共同静态读集；动态actor/范围相位及菜单能力差异另列。"""
    original, sdl = cases
    endpoints = [paired_checkpoint_records(case, output / role)
                 for case, role in ((original, "original"), (sdl, "sdl"))]
    (first, original_diagnostics), (second, sdl_diagnostics) = endpoints
    if first.keys() != second.keys():
        raise AssertionError("两端检查点缺失或重复，不能只比较交集")
    result = {"ok": True, "checked_pixels": 0, "checkpoints": [],
              "business_checkpoint_count": len(first),
              "wgc_transient_diagnostics": {"original": original_diagnostics, "sdl": sdl_diagnostics},
              "excluded": "whole frame/water/actor idle phase/RNG/audio; menu capability comparison applies only to these exact fixtures; raw inventory only compared in explicit Save scenarios"}
    for name, detail in first.items():
        if detail["kind"] != second[name]["kind"]:
            raise AssertionError("两端检查点状态类型不一致")
        images = []
        for case, role in ((original, "original"), (sdl, "sdl")):
            filename = name + ".png"
            path = output / role / filename
            if hashlib.sha256(path.read_bytes()).hexdigest() != case["capture_hashes"].get(filename):
                raise AssertionError("配对检查点PNG不再匹配该端capture收据")
            with Image.open(path) as image:
                images.append(image.convert("RGB"))
        row = {"name": name, "per_endpoint_resource_gates_passed": True, "static_regions": []}
        regions = []
        if detail["kind"] in ("use_inventory_list", "drop_inventory_list", "equipment_inventory_list", "herb192_two_slot_detail", "inventory_detail", "player_use_detail"):
            a, b = detail["rectangles"], second[name]["rectangles"]
            if [(r["name"], r["position"], r["size"]) for r in a] != [(r["name"], r["position"], r["size"]) for r in b]:
                raise AssertionError("两端静态列表/详情ROI定义不一致")
            random_fields = original.get("resource_fields", {}).get("use_contract", {}).get("random_fields", []) if detail["kind"] == "player_use_detail" else []
            excluded = {"current_max_hp"} if "target.HP" in random_fields else set()
            if "target.status_duration" in random_fields:
                code = original["resource_fields"]["use_contract"]["use_code"]
                if code in (14, 22): excluded.add("status_26" if code == 14 else "status_27")
            regions = [(r["name"], tuple(r["position"]), tuple(r["size"]), None) for r in a if r["name"] not in excluded]
            if excluded:
                row["native_random_regions_not_cross_compared"] = sorted(excluded)
                row["native_random_regions_per_endpoint_exact_resource_checked"] = True
        elif detail["kind"] in ("item_submenu", "player_command_resource_overlay"):
            if len(detail["matches"]) != 4 or len(second[name]["matches"]) != 4:
                raise AssertionError("配对菜单缺少完整四向资源收据")
            for item, (a, b) in enumerate(zip(detail["matches"], second[name]["matches"])):
                if a["position"] != b["position"]:
                    raise AssertionError("两端菜单ROI位置不一致")
                if a["frame"] != b["frame"]:
                    row.setdefault("capability_or_selection_differences", []).append({"original_frame": a["frame"], "sdl_frame": b["frame"],
                        "scope": "each full resource independently verified; no cross-end equality claim"})
                    continue
                mask = assets["commands"][a["frame"]]
                # 179d5/127e0重画actor会覆盖上icon末6行；该动态部分逐端核完整资源。
                if item == 0:
                    mask = mask.crop((0, 0, 24, 14))
                regions.append((f"command-frame{a['frame']}", tuple(a["position"]), mask.size, mask))
        elif detail["kind"] == "player_use_target" and detail.get("selection_markers"):
            markers = assets["use"]["area2_markers"]
            if [item["frame"] for item in detail["selection_markers"]] != list(range(2, 14)) or [item["frame"] for item in second[name]["selection_markers"]] != list(range(2, 14)):
                raise AssertionError("area2两端缺少完整frame2..13资源门")
            regions = [("area2-frame" + str(item["frame"]), tuple(item["position"]), item["static"].size, item["static"])
                for item in markers if any(item["static"].getdata())]
            row["dynamic_actor_composites_per_endpoint_checked"] = True
        elif detail["kind"] == "herb192_target":
            # 本exact START目标下格没有actor；取frame1不受自身actor覆盖的末6行。
            mask = assets["herb"]["target_cursor"].crop((0, 18, 24, 24))
            for image in images:
                if not match_indexed(image, mask, (52, 94))["ok"]:
                    raise AssertionError("配对目标光标静态末6行未匹配frame1资源")
            regions.append(("target-frame1-bottom6", (52, 94), mask.size, mask))
            row["range_rgb_diagnostic"] = {**rgb_difference(*images, tuple(probe["position"]), (24, 24)),
                "original_lut": detail["range_lut"], "sdl_lut": second[name]["range_lut"],
                "used_for_pass": False, "reason": "dynamic LUT phase; each endpoint must match its own exact accepted resource"}
        elif detail["kind"] in ("cancelled_browse", "drop_unacted_browse", "equipment_unacted_browse"):
            regions.append(("plain-terrain", tuple(probe["position"]), (24, 24), None))
        elif detail["kind"] == "herb192_acted_browse":
            regions.append(("plain-terrain", tuple(probe["position"]), (24, 24), None))
            at = assets["herb"]["detail_close_terrain"]["position"]
            regions.append(("detail-close-static-terrain", tuple(at), (24, 24), None))
        for label, at, size, mask in regions:
            checked = rgb_difference(*images, at, size, mask)
            row["static_regions"].append({"name": label, "position": list(at), "size": list(size), **checked})
            result["checked_pixels"] += checked["checked_pixels"]
            result["ok"] = result["ok"] and checked["within_tolerance"]
        row["static_rgb_status"] = "compared" if regions else "not_compared; resource gates only, outside static RGB scope"
        result["checkpoints"].append(row)
    result["ok"] = bool(result["ok"] and result["checked_pixels"])
    return result


def compare_inventory_saves(cases, measurement_key):
    """每端先过独立模型；再要求两份真实SAV完整相同，不归一cache或焦点。"""
    encoded, plain, hashes_by_role = [], [], {}
    for case in cases:
        completed = case["completed_save"]
        path = Path(completed["snapshot"])
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != completed["sha256"] or digest != case[measurement_key]["snapshot_sha256"]:
            raise AssertionError("库存配对快照不再匹配已测量hash")
        encoded.append(data)
        plain.append(decode_save(path))
        hashes_by_role[case["kind"]] = digest
    if len(encoded) != 2 or encoded[0] != encoded[1] or plain[0] != plain[1]:
        raise AssertionError("两端库存真实SAV完整字节不一致；不归一cache/导航或扩大差异范围")
    return {"full_encoded_bytes_equal": True, "full_plaintext_bytes_equal": True,
            "size": len(encoded[0]), "sha256": hashes_by_role,
            "cache_or_navigation_normalized": False}


def compare_use_saves(cases, assets):
    """各端完整有限native记录已过；仅数学分支本身可变的字节不要求跨端相等。"""
    if len(cases) != 2 or [case["kind"] for case in cases] != ["original", "sdl"]:
        raise AssertionError("Use配对必须包含顺序正确的original和SDL两端")
    plan = assets["use"]
    expected = plan["expected_sources"]
    variable = {i for i in range(len(expected[0])) if any(row[i] != expected[0][i] for row in expected[1:])}
    measured, hashes_by_role = [], {}
    for case in cases:
        completed = case["completed_save"]; path = Path(completed["snapshot"])
        encoded = path.read_bytes(); digest = hashlib.sha256(encoded).hexdigest()
        if digest != completed["sha256"] or digest != case["use_measurement"]["snapshot_sha256"]:
            raise AssertionError("Use配对真实SAV hash改变")
        plain = decode_save(path)
        observed = case.get("use_immediate_detail", {}).get("finite_native_outcome_indices", [])
        if not observed or any(type(index) is not int or not 0 <= index < len(expected) for index in observed):
            raise AssertionError("Use配对端缺少即时F2的同端有限结果收据")
        if not any(_matches_complete_save(expected[index], plain, plan["steps"]) for index in observed):
            raise AssertionError("Use一端未匹配完整native数学有限结果")
        measured.append(plain); hashes_by_role[case["kind"]] = digest
    mismatches = [i for i, (a, b) in enumerate(zip(*measured)) if a != b and i not in variable]
    if mismatches:
        raise AssertionError("Use确定性容器字节跨端不一致：" + ",".join(hex(i) for i in mismatches[:12]))
    return {"deterministic_container_bytes_equal": True, "full_bytes_equal": measured[0] == measured[1],
        "native_finite_variable_offsets": sorted(variable), "allowed_native_outcomes": len(expected),
        "each_endpoint_complete_container_validated": True, "rng_aligned": False, "rerolled": False,
        "sha256": hashes_by_role}


def _matches_complete_save(expected, measured, steps):
    try:
        check_load_measurement(expected, measured, steps)
        return True
    except AssertionError:
        return False


def endpoint_guards(record, original, before, fixture, data, stamp, save_data):
    current = {"original_unchanged": hashes(original) == before,
        "fixture_unchanged": fixture.is_file() and fixture.read_bytes() == data and fixture.stat().st_mtime_ns == stamp}
    save = record.get("save")
    path = Path(save["path"]) if save else None
    if record.get("scenario") in SAVED_SCENARIOS:
        completed = record.get("completed_save")
        snapshot = Path(completed["snapshot"]) if completed else None
        matches = bool(path and path.is_file())
        if matches and completed:
            matches = bool(snapshot.is_file() and
                hashlib.sha256(snapshot.read_bytes()).hexdigest() == completed["sha256"] and
                path.read_bytes() == snapshot.read_bytes() and path.stat().st_mtime_ns == completed["mtime_ns"])
        elif matches:
            matches = path.read_bytes() == save_data and path.stat().st_mtime_ns == save["initial_mtime_ns"]
        current["copy_save_matches_completed_state_after_cleanup"] = matches
    else:
        current["copy_save_unchanged_after_cleanup"] = bool(path and path.is_file() and
            path.read_bytes() == save_data and path.stat().st_mtime_ns == save["initial_mtime_ns"])
    current["cleanup_passed"] = len(record["processes"]) == 1 and all(
        p["cleaned"] and not p["forced_cleanup"] and p["exit_code"] == 0 for p in record["processes"])
    for key, value in current.items():
        record[key] = record.get(key, True) and value  # 已观察到的失败不能被最终恢复救回。
    record["probe_cases_passed"] = record["probe_cases_passed"] and all(record[key] for key in current)
    if not record["probe_cases_passed"]:
        record["status"] = "failed"
        record.setdefault("error", "物品端点最终来源/存档bytes或mtime/清理守卫失败；不启动后续端")


def run_endpoint(kind, work, record, args, root, original, before, fixture, data, stamp, save_data, prepared, titles):
    save = None
    try:
        env = os.environ.copy()
        for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            env.pop(name, None)
        if kind == "original":
            game_copy = work / "game"
            shutil.copytree(original, game_copy)
            save = game_copy / "FD2.SAV"
            config = work / "dosbox.conf"
            config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
                f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
            application = args.dosbox.resolve()
            command, cwd = [str(application), "-conf", str(config), "-noconsole"], work
        else:
            save = work / "FD2.SAV"
            env["FD2SDL_SAVE_PATH"] = str(save)
            application = args.sdl.resolve()
            command, cwd = [str(application)], root
        record["application"] = {"path": str(application), "sha256": hashlib.sha256(application.read_bytes()).hexdigest()}
        save.write_bytes(save_data)
        save_stamp = save.stat().st_mtime_ns
        final_data, final_stamp = save_data, save_stamp
        record["save"] = {"path": str(save), "sha256": hashlib.sha256(save_data).hexdigest(), "initial_mtime_ns": save_stamp}
        driver = args.driver.resolve()
        with running(command, cwd, env, work / "dosbox.log" if kind == "original" else work / "game.log", driver, record["processes"]) as process:
            capture = Capture(driver, process, work, record["inputs"])
            arguments = (capture, *prepared, titles, save, save_data, save_stamp, record)
            if record["scenario"] == "controls":
                exercise(*arguments)
            else:
                exercise(*arguments, scenario=record["scenario"])
                if record["scenario"] in SAVED_SCENARIOS:
                    measurement_key = measurement_name(record["scenario"])
                    if not record.get(measurement_key, {}).get("runtime_raw_inventory_measured") or "completed_save" not in record:
                        raise AssertionError("库存操作/取消/正式Save量测未全部完成")
                    final_data = Path(record["completed_save"]["snapshot"]).read_bytes()
                    final_stamp = record["completed_save"]["mtime_ns"]
                    if hashlib.sha256(final_data).hexdigest() != record["completed_save"]["sha256"]:
                        raise AssertionError("库存测量快照在完成后被改变")
                elif any(key not in record for key in ("herb_outcome", "herb_measurement", "herb_detail_closed")):
                    raise AssertionError("草药单次使用/详情/关闭检查点未全部完成")
            require_source_unchanged(save, final_data, final_stamp)
        require_source_unchanged(save, final_data, final_stamp)
        record["probe_cases_passed"], record["status"] = True, "passed"
    except Exception as error:
        record["error"] = str(error)
    finally:
        endpoint_guards(record, original, before, fixture, data, stamp, save_data)
        record["capture_hashes"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in work.iterdir() if p.is_file() and p.suffix.lower() in (".png", ".bmp")}
        (work / "comparison.json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入original_game")
    scenario = getattr(args, "scenario", "controls")
    if scenario not in ("controls", "herb-full", "herb-injured", *SAVED_SCENARIOS):
        raise ValueError("未知物品校准场景")
    sdl = getattr(args, "sdl", None)
    if sdl is not None and not sdl.is_file():
        raise ValueError("--sdl 程序不存在；提供已构建的正式SDL程序，未创建产物或启动窗口")
    output.mkdir(parents=True, exist_ok=False)
    fixture = original / "START.SAV"
    before, data, stamp = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "probe_only": sdl is None, "probe_cases_passed": False, "status": "failed",
              "scenario": scenario, "scope": "real START raw0 ITEM submenu, Use navigation, code0 refusal and layered cancel",
              "excluded": "SDL pairing; actual Use/Equip/Drop/Give, effects/targets, memory-owner snapshot, UI RNG/timing/audio",
              "capture": "Windows.Graphics.Capture HWND only", "platform": platform.platform(),
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(data).hexdigest(), "mtime_ns": stamp},
              "original_path": str(original),
              "original_before": before, "processes": [], "inputs": [], "checkpoints": []}
    cases = []
    if sdl is not None:
        report["cases"] = cases
        report["known_capability_differences"] = {"item_submenu_disabled": SUBMENU_DISABLED,
            "same_direction_keys": "RIGHT selects EQT2 and DOWN selects Drop3 in both exact START-derived fixtures",
            "full_resources_checked_per_endpoint": True, "cross_end_capability_equivalence_claimed": True,
            "scope": "only exact fixtures and enabled B3 commands; not arbitrary profile/inventory support"}
    save_data = data
    try:
        source_override = None
        if scenario in USE_SCENARIOS:
            table, _ = item_table(original / "FD2.EXE")
            save_data, source_override, manifest = derive_use(decode_save(fixture), data, scenario, table)
            derived = output / (scenario + ".SAV"); derived.write_bytes(save_data)
            if decode_save(derived) != source_override: raise AssertionError("Use manifest XOR/checksum往返不符")
            report["derived_fixture"] = manifest
            (output / "fixture-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        elif scenario in GIVE_SCENARIOS:
            save_data, source_override, manifest = derive_give(decode_save(fixture), data, scenario)
            derived = output / (scenario + ".SAV")
            derived.write_bytes(save_data)
            if decode_save(derived) != source_override:
                raise AssertionError("Give exact manifest XOR/checksum往返不符")
            report["derived_fixture"] = manifest
            (output / "fixture-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        elif scenario == "herb-injured":
            save_data, source_override, manifest = derive_injured(decode_save(fixture), data)
            derived = output / "herb-injured.SAV"
            derived.write_bytes(save_data)
            if decode_save(derived) != source_override:
                raise AssertionError("草药受控SAV的XOR/checksum往返不符")
            report["derived_fixture"] = manifest
            (output / "fixture-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        elif scenario == "drop-last":
            table, _ = item_table(original / "FD2.EXE")
            save_data, source_override, manifest = derive_drop_last(decode_save(fixture), data, table)
            derived = output / "drop-last.SAV"
            derived.write_bytes(save_data)
            if decode_save(derived) != source_override:
                raise AssertionError("最后一项Drop的XOR/checksum往返不符")
            report["derived_fixture"] = manifest
            (output / "fixture-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        elif scenario == "eqt-weapon":
            save_data, source_override, manifest = derive_second_weapon(decode_save(fixture), data)
            derived = output / "second-weapon.SAV"
            derived.write_bytes(save_data)
            if decode_save(derived) != source_override:
                raise AssertionError("第二把剑XOR/checksum往返不符")
            report["derived_fixture"] = manifest
            (output / "fixture-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        prepared = prepare_controlled(original, source_override) if scenario in (*GIVE_SCENARIOS, *USE_SCENARIOS) else prepare(original) if scenario == "controls" or scenario in SAVED_SCENARIOS else prepare(
            original, source=source_override, herb=True)
        source, focus, assets, actor, probe, templates, fields = prepared
        if scenario in USE_SCENARIOS:
            prepare_use(original, source, assets, actor, fields, scenario)
            report["scope"] = "controlled player Use20c6f: target cancel, single confirmation, finite native F2 outcomes, acted, one legal isolated END and whole-container Save"
            report["excluded"] = "natural acquisition/history, full RNG alignment/animation/audio; only declared random fields differ across endpoints and each must match full native finite record"
        elif scenario in GIVE_SCENARIOS:
            prepare_give(original, source, assets, actor, probe, fields, scenario)
            report["scope"] = "controlled adjacent Give: layered cancel or one remove/append transfer, sender-only recompute, recipient immediate F2, sticky COMMAND cancel, legal END and whole-container UI Save"
            report["excluded"] = "natural acquisition/AI encounter, full RNG/animation/audio; fixture non-player04 explicitly freezes AI for first END"
        elif scenario in DROP_SCENARIOS:
            if source_override is not None:
                source = source_override
                _, templates, updated = inventory_templates(original, source[UNITS:UNITS + STRIDE], assets)
                fields = dict(updated, submenu=fields["submenu"])
            prepare_drop(original, source, assets, actor, fields, scenario)
            prepared = (source, focus, assets, actor, probe, templates, fields)
            report["scope"] = f"original-only {scenario}: mode0 cancel -> one Drop -> remaining inventory/empty COMMAND -> unacted -> explicit UI Save/full-container measurement"
            report["excluded"] = "SDL pairing, Equip/Give/Use effects, other Drop fixtures, exact UI RNG/timing/audio and whole-owner equivalence"
        elif scenario in EQUIPMENT_SCENARIOS:
            if source_override is not None:
                source = source_override
                _, templates, updated = inventory_templates(original, source[UNITS:UNITS + STRIDE], assets)
                fields = dict(updated, submenu=fields["submenu"])
            prepare_equipment(original, source, assets, actor, fields, scenario)
            prepared = (source, focus, assets, actor, probe, templates, fields)
            report["scope"] = f"original-only {scenario}: mode0 equipment list, compatibility/flags/stats, persistent list2, Esc/reopen0, layered cancel and full UI Save measurement"
            report["excluded"] = "SDL pairing, other EQT profiles/items/hidden states, Use/Drop/Give, exact UI RNG/animation/timing/audio and whole-owner equivalence"
        elif scenario != "controls":
            report["scope"] = "Continue from same START geometry -> herb192 target cancel -> one Use -> acted, F2 HP/MP/LV/EX/two visible slots -> close"
            report["excluded"] = "SDL pairing; raw16B inventory/stale-tail runtime measurement, post-END Save, exact RNG/floating heal/animation/audio and whole-owner equivalence"
        report["runtime_fixture_sha256"] = hashlib.sha256(save_data).hexdigest()
        report["resource_fields"] = fields
        if sdl is not None:
            report["known_capability_differences"]["item_submenu_disabled"] = {
                role: list(fields["submenu"]["disabled"]) for role in ("original", "sdl")}
            report["known_capability_differences"]["same_direction_keys"] = "each endpoint's exact source-derived capability row; Give may be enabled only for visible adjacent side2"
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name, path in (("driver", args.driver), ("dosbox", args.dosbox))}
        if sdl is not None:
            report["binaries"]["sdl"] = {"path": str(sdl.resolve()), "sha256": hashlib.sha256(sdl.read_bytes()).hexdigest()}
            report["excluded"] = report["excluded"].removeprefix("SDL pairing; ").removeprefix("SDL pairing, ")
            report["scope"] = report["scope"].replace("original-only ", "")
            report["paired_scope"] = "original first, cleanup+guards, then SDL; exact per-end resource gates and shared static RGB <=3 only"
        report["tool_sources"] = {name: {"path": str(Path(module.__file__).resolve()),
            "sha256": hashlib.sha256(Path(module.__file__).read_bytes()).hexdigest()}
            for name in ("fd2_item_list_oracle", "run_original_comparison", "run_original_player", "run_original_magic_list")
            for module in (sys.modules[name],)}
        report["tool_sources"]["run_original_item_list"] = {"path": str(Path(__file__).resolve()),
            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
        for name, (at, image) in templates.items():
            image.save(output / ("expected-" + name + ".png"))
        titles = title_templates(original / "FDOTHER.DAT")
        for kind in (("original",) if sdl is None else ("original", "sdl")):
            work = output if sdl is None else output / kind
            if sdl is not None:
                work.mkdir()
            record = report if sdl is None else {"ok": False, "probe_only": True, "endpoint_only": True, "probe_cases_passed": False,
                "status": "failed", "scenario": scenario, "scope": report["scope"], "excluded": report["excluded"], "original_path": str(original),
                "fixture": report["fixture"], "runtime_fixture_sha256": report["runtime_fixture_sha256"],
                "resource_fields": fields, "binaries": report["binaries"], "tool_sources": report["tool_sources"],
                "processes": [], "inputs": [], "checkpoints": []}
            record["kind"] = kind
            record["role_expected_submenu_disabled"] = fields["submenu"]["disabled"]
            cases.append(record)
            run_endpoint(kind, work, record, args, root, original, before, fixture, data, stamp, save_data, prepared, titles)
            if sdl is not None:
                report["processes"].extend(record["processes"])
                report["inputs"].extend(dict(case=kind, **item) for item in record["inputs"])
                report["checkpoints"].extend(dict(case=kind, **item) for item in record["checkpoints"])
            if not record["probe_cases_passed"]:
                raise AssertionError(record["error"])
        if sdl is not None:
            report["paired_rgb"] = compare_paired_checkpoints(cases, output, assets, probe)
            if not report["paired_rgb"]["ok"]:
                raise AssertionError("两端共同静态ROI的实际RGB差异超出容差3")
            if scenario == "controls":
                values = [{key: case["input_scope"][key] for key in
                    ("valid_use_confirmations", "code0_confirmations", "equip_drop_give_confirmations", "save_or_end_sent", "cancel_route")}
                    for case in cases]
            elif scenario in SAVED_SCENARIOS:
                measurement_key = measurement_name(scenario)
                report["paired_" + measurement_key.removesuffix("_measurement") + "_save"] = compare_use_saves(cases, assets) if scenario in USE_SCENARIOS else compare_inventory_saves(cases, measurement_key)
                values = [{key: case[measurement_key][key] for key in
                    ("measured_inventory_hex", "measured_derived_values", "raw_flags", "unit_experience")} for case in cases]
                if scenario in USE_SCENARIOS:
                    # AP/DP/AC buff数值确定；HP/duration独立保留在每端完整SAV收据，不反复重抽。
                    values = [{"item_id": assets["use"]["contract"]["item_id"], "use_confirmations": case["input_scope"]["valid_use_confirmations"],
                        "runtime_full_finite_container_validated": bool(case[measurement_key]["finite_native_outcome_indices"])} for case in cases]
            else:
                values = [{key: case["herb_measurement"][key] for key in
                    ("hp", "max_hp", "mp", "max_mp", "level", "experience", "visible_inventory_ids")} for case in cases]
            if values[0] != values[1]:
                raise AssertionError("两端已确认的物品流程终态不一致")
            report["paired_terminal_values"] = dict(zip(("original", "sdl"), values))
        report["probe_cases_passed"], report["status"] = True, "passed"
    except Exception as error:
        report.setdefault("error", str(error))
        report["probe_cases_passed"] = False
    finally:
        if sdl is not None:
            # 最后再次核实所有端，不能只检查最后SDL的SAV而漏掉原版副本。
            for record in cases:
                endpoint_guards(record, original, before, fixture, data, stamp, save_data)
                (output / record["kind"] / "comparison.json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
            save_guard = "copy_save_matches_completed_state_after_cleanup" if scenario in SAVED_SCENARIOS else "copy_save_unchanged_after_cleanup"
            for key in ("original_unchanged", "fixture_unchanged", save_guard, "cleanup_passed"):
                report[key] = bool(cases) and all(record[key] for record in cases)
            report["probe_cases_passed"] = report["probe_cases_passed"] and len(cases) == 2 and all(
                record["status"] == "passed" and record["probe_cases_passed"] for record in cases)
            if not report["probe_cases_passed"]:
                report.setdefault("error", next((record["error"] for record in cases if "error" in record), "物品配对端点未全部通过"))
        elif not cases:
            endpoint_guards(report, original, before, fixture, data, stamp, save_data)
        if not report["probe_cases_passed"]:
            report["status"] = "failed"
            report.setdefault("error", "物品校准最终来源/存档bytes或mtime守卫失败")
        report["ok"] = sdl is not None and report["probe_cases_passed"]
        report["capture_hashes"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in output.iterdir() if p.is_file() and p.suffix.lower() in (".png", ".bmp")}
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--scenario", choices=("controls", "herb-full", "herb-injured", *SAVED_SCENARIOS), default="controls")
    parser.add_argument("--sdl", type=Path, help="原版先通过并清理后，以同一fixture正式启动SDL配对")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("原版物品校准要求Windows WGC；未启动窗口")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"物品校准无法启动：{error}。检查参数并使用新产物目录重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
