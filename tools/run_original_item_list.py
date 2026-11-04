#!/usr/bin/env python3
"""ITEM列表控制／单次草药；另含原版-only草药Drop/Save校准，不执行装备或交接。

依据field_player_item_command_execute @0x1bbdc（原登记同名）、
field_unit_inventory_select @0x1b932（原sub_1B932）、
field_unit_inventory_select_step @0x1b9de（原sub_1B9DE）及
field_unit_detail_equipment_draw @0x184c0（原登记同名）。
Drop分支复用field_unit_inventory_slot_remove @0x1b8e7（原sub_1B8E7）与
field_unit_combat_stats_recompute @0x1b750（原登记同名）。
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

from PIL import Image, ImageChops

from audit_fdicon_opaque_zero import audit_fdicon
from fd2_item_list_oracle import AFTER_USE_INVENTORY, PLAIN_SHA256, derive_injured, item_table, prepare as prepare_templates
from fd2_movement_oracle import movement_profile
from run_graphics_tests import decode_save
from run_original_comparison import Capture, SIZE, archive_entry, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, save_checkpoint, wait_state, wait_title
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle
from run_original_load import (check_load_measurement, focus_cycle_assets, focused_actor,
                               require_source_unchanged, wait_measured_save)
from run_original_focus import native_focus_reference
from run_original_player import PlayerOracle, acted_sprite, exact_tile, screen, sheet_frame, upper_command_composite
from run_original_result import selection_probe, selection_state
from run_original_magic_list import actor_focus_resources, detail_close_terrain


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


def drop_expected_source(source, table):
    """1bbdc selector3→1b8e7→1b750；只建exact START静态预期，不模拟运行。"""
    if hashlib.sha256(source).hexdigest() != PLAIN_SHA256 or len(table) != 215 * 23:
        raise ValueError("Drop模型只支持完整已确认START及原版215项物品表")
    expected = bytearray(source)
    actor = bytearray(source[UNITS:UNITS + STRIDE])
    before = bytes(actor[0x0a:0x1a])
    if before.hex() != "4000408400c0800080008000800080c9" or actor[0x22:0x25] != b"\0\0\0":
        raise ValueError("Drop草药不是已确认的未装备slot2或属性状态前置不符")
    actor[0x0e:0x18] = actor[0x10:0x1a]
    actor[0x18] = 0x80
    attack, defense, accuracy = (struct.unpack_from("<h", actor, offset)[0] for offset in (0x37, 0x39, 0x3e))
    evasion = accuracy
    for slot in range(8):
        flags, item_id = actor[0x0a + 2 * slot:0x0c + 2 * slot]
        if not flags & 0x40:
            continue
        if item_id >= 215:
            raise ValueError("Drop重算包含未知装备ID")
        ap, ac, dp, ev = struct.unpack_from("<4h", table, item_id * 23 + 1)
        attack += ap; defense += dp; accuracy += ac; evasion += ev
    derived = [value & 0xffff for value in (attack, defense, accuracy, evasion)]
    if derived != [16, 12, 97, 2] or list(struct.unpack_from("<4H", actor, 0x48)) != derived:
        raise ValueError("原版表及1b750公式不满足本场四派生不变前置")
    struct.pack_into("<4H", actor, 0x48, *derived)
    if actor[0x0a:0x1a].hex() != AFTER_USE_INVENTORY:
        raise ValueError("原始移槽与已确认16字节模型不符")
    expected[UNITS:UNITS + STRIDE] = actor
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    return bytes(expected), {"native_branch": "0x1bfcd..0x1bff4", "remove_va": "0x1b8e7",
        "recompute_va": "0x1b750", "derived_offsets": [0x48, 0x4a, 0x4c, 0x4e],
        "derived_values": derived, "before_inventory_hex": before.hex(),
        "after_inventory_hex": actor[0x0a:0x1a].hex(), "runtime_raw_inventory_measured": False,
        "raw_unit_changed_offsets": [i for i, (a, b) in enumerate(zip(source[UNITS:UNITS + STRIDE], actor)) if a != b]}


def prepare_drop(original, source, assets, actor, fields):
    table, evidence = item_table(original / "FD2.EXE")
    expected, contract = drop_expected_source(source, table)
    _, _, _, after_templates, after_fields = prepare_templates(original, original / "START.SAV", source=source, after_use=True)
    contract.update(item_id=192, raw_slot=2, selected_command=3, list_mode=0, item_table=evidence,
                    after_model_scope="slot removal/recompute only; no HP heal, acted or runtime measurement",
                    cancel_recompute_evidence="static 0x1bfda -> 0x1bfeb -> 0x1b750; unchanged four values do not prove runtime writes")
    fields["drop_contract"] = contract
    after_fields = dict(after_fields, after_use_model=False, after_drop_model=True, drop_contract=contract)
    camera = tuple(source[META + 3:META + 5])
    world = tuple(actor["world"])
    target = (world[0] - 1, world[1])
    reference = native_focus_reference(camera, world, target)
    occupied = {(source[UNITS + i * STRIDE], source[UNITS + i * STRIDE + 1])
                for i in range(source[META + 1]) if not source[UNITS + i * STRIDE + 5] & 1}
    if reference["camera"] != list(camera) or target in occupied or (target[0], target[1] + 1) in occupied:
        raise ValueError("Drop测量LEFT必须不卷屏且无actor/下格头部遮挡")
    assets["drop"] = {"expected_source": expected, "after_templates": after_templates,
        "after_fields": after_fields, "steps": [("LEFT", tuple(reference["screen"]))]}
    contract["measurement_navigation"] = {"from_world": list(world), "to_world": list(target), **reference}


def list_page(frame, templates, fields, selected=0):
    if selected not in range(len(fields["slots"])):
        raise ValueError("物品资源门只支持真实三槽索引")
    if frame.size != SIZE:
        return None
    names = (f"selected_{selected}", "actor_name", "current_max_hp", "current_max_mp")
    if "herb_contract" in fields or "drop_contract" in fields:
        names += ("level", "experience")
    checks = [exact_tile(frame, templates[name][1], templates[name][0]) for name in names]
    if any(check is None for check in checks):
        return None
    slot = fields["slots"][selected]
    return {"kind": "use_inventory_list", "selected_raw_slot": selected,
            "item_id": slot["item_id"], "use_code": slot["use_code"],
            "checked_pixels": sum(check["checked_pixels"] for check in checks),
            "max_channel_delta": max(check["max_channel_delta"] for check in checks),
            "rectangles": [{"name": name, "position": list(templates[name][0]),
                            "size": list(templates[name][1].size), **check} for name, check in zip(names, checks)],
            "complete_bottom_checked": True, "hp": fields["hp"], "max_hp": fields["max_hp"],
            "mp": fields["mp"], "max_mp": fields["max_mp"]}


def list_absent(frame, templates, fields):
    return not any(list_page(frame, templates, fields, selected) for selected in range(len(fields["slots"])))


def submenu_page(frame, assets, position, templates, fields, selected=0, *, role="original"):
    if role not in ("original", "sdl"):
        raise ValueError("未知物品端点角色")
    disabled = [0, 1, 0, 0] if role == "original" else [0, 1, 1, 1]
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


def command_page(frame, command, position, templates, fields):
    detail = command.command(frame, position, 2)
    if not detail or [match["frame"] for match in detail["matches"]] != [2, 5, 7, 9]:
        return None
    return {**detail, "native_first_enabled_item": True, "inventory_list_absent": True} if list_absent(frame, templates, fields) else None


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
            for selected in ((0, 2, 3) if role == "original" else (0,))):
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


def drop_browse(frame, source, assets, actor, probe, templates, fields):
    detail = cancelled_browse(frame, assets, actor, probe, templates, fields)
    position = probe["actor_position"]
    if (not detail or PlayerOracle(source, assets, [], {}).command(frame, position) or
            any(submenu_page(frame, assets, position, templates, fields, selected) for selected in (0, 2, 3))):
        return None
    return {**detail, "kind": "drop_unacted_browse", "raw_acted_measured": False}


def drop_list_page(frame, templates, fields, selected=0):
    detail = list_page(frame, templates, fields, selected)
    return {**detail, "kind": "drop_inventory_list", "operation": "drop", "list_mode": 0,
            "mode_evidence": "1bbdc selector3 calls 1b932(actor,0); shared resource pixels"} if detail else None


def exercise_drop(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint):
    plan = assets["drop"]
    after_templates, after_fields = plan["after_templates"], plan["after_fields"]
    position = screen(source, tuple(actor["world"]))
    for attempt in ("cancel", "commit"):
        capture.key("DOWN")
        checkpoint(f"drop-{attempt}-selected", lambda frame: submenu_page(frame, assets, position, templates, fields, 3))
        capture.key("ENTER")
        checkpoint(f"drop-{attempt}-list-zero", lambda frame: drop_list_page(frame, templates, fields))
        for selected in (1, 2):
            capture.key("DOWN")
            checkpoint(f"drop-{attempt}-slot{selected}", lambda frame, index=selected: drop_list_page(frame, templates, fields, index))
        capture.key("ESC" if attempt == "cancel" else "ENTER")
        active_templates, active_fields = (templates, fields) if attempt == "cancel" else (after_templates, after_fields)
        checkpoint(f"drop-{attempt}-back-item-zero", lambda frame, t=active_templates, f=active_fields:
                   submenu_page(frame, assets, position, t, f))
    capture.key("ENTER")  # Use只读看剩余两槽；不确认任何物品。
    _, report["drop_two_slot_page"] = checkpoint("drop-after-use-list-two-slots",
        lambda frame: list_page(frame, after_templates, after_fields))
    capture.key("ESC")
    checkpoint("drop-readonly-list-cancel-item", lambda frame: submenu_page(frame, assets, position, after_templates, after_fields))
    capture.key("ESC")
    command = PlayerOracle(source, assets, [], {})
    checkpoint("drop-item-cancel-command", lambda frame: command_page(frame, command, position, after_templates, after_fields))
    capture.key("ESC")
    checkpoint("drop-command-cancel-selection", lambda frame: selection_state(frame, assets, actor, probe))
    capture.key("ESC")
    checkpoint("drop-selection-cancel-unacted-browse",
        lambda frame: drop_browse(frame, source, assets, actor, probe, after_templates, after_fields))
    for index, (key, at) in enumerate(plan["steps"], 1):
        capture.key(key)
        checkpoint(f"drop-save-empty-step-{index}", RoundOracle(assets, at).closed_browse)
    oracle = RoundOracle(assets, plan["steps"][-1][1])
    capture.key("ENTER")
    checkpoint("drop-save-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("ENTER")
    checkpoint("drop-save-secondary", lambda frame: oracle.secondary(frame, 0, overview_available=True))
    capture.key("LEFT")
    checkpoint("drop-save-selected", lambda frame: oracle.secondary(frame, 1, overview_available=True))
    capture.key("ENTER")
    checkpoint("drop-save-default-yes", lambda frame: oracle.buttons(frame, 0))
    require_source_unchanged(save, data, stamp)
    report["drop_save_requested"] = True
    capture.key("ENTER")  # 唯一正式UI Save肯定；之后不再要求副本等于源。
    measured = wait_measured_save(save, data, stamp)
    encoded = save.read_bytes()
    saved = capture.artifacts / "drop-measured.SAV"
    saved.write_bytes(encoded)  # 即使严格比较随后失败，也先保留真实测量。
    if decode_save(saved) != measured:
        raise AssertionError("Drop测量SAV在writer完成后仍变化")
    comparison = check_load_measurement(plan["expected_source"], measured, plan["steps"])
    completed_stamp = save.stat().st_mtime_ns
    checkpoint("drop-saved-browse", oracle.closed_browse, source_guard=False)
    report["drop_measurement"] = {**comparison, "runtime_raw_inventory_measured": True,
        "measured_inventory_hex": measured[UNITS + 0x0a:UNITS + 0x1a].hex(),
        "measured_derived_values": list(struct.unpack_from("<4H", measured, UNITS + 0x48)),
        "raw_flags": measured[UNITS + 5], "unit_experience": measured[UNITS + 0x3c],
        "snapshot": str(saved), "snapshot_sha256": hashlib.sha256(encoded).hexdigest()}
    report["completed_save"] = {"snapshot": str(saved), "sha256": hashlib.sha256(encoded).hexdigest(),
                                "mtime_ns": completed_stamp}
    require_source_unchanged(save, encoded, report["completed_save"]["mtime_ns"])
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
    capture.key("ENTER")
    command = PlayerOracle(source, assets, [], {})
    checkpoint("raw0-zero-move-command", lambda frame: command_page(frame, command, position, templates, fields))
    capture.key("ENTER")
    checkpoint("item-submenu-default-use", lambda frame: submenu_page(frame, assets, position, templates, fields, role=role))
    if scenario == "drop-herb":
        exercise_drop(capture, source, assets, actor, probe, templates, fields, save, data, stamp, report, checkpoint)
        return
    if scenario != "controls":
        exercise_herb(capture, source, assets, probe, templates, fields, report, checkpoint)
        return
    for key, selected in (("RIGHT", 2), ("DOWN", 3), ("UP", 0), ("LEFT", 0)):
        capture.key(key)
        selected = 0 if role == "sdl" else selected
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
              "excluded": "whole frame/water/actor idle phase/RNG/audio/raw16B; native EQT/Drop capability differs"}
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
        if detail["kind"] in ("use_inventory_list", "herb192_two_slot_detail"):
            a, b = detail["rectangles"], second[name]["rectangles"]
            if [(r["name"], r["position"], r["size"]) for r in a] != [(r["name"], r["position"], r["size"]) for r in b]:
                raise AssertionError("两端静态列表/详情ROI定义不一致")
            regions = [(r["name"], tuple(r["position"]), tuple(r["size"]), None) for r in a]
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
        elif detail["kind"] == "cancelled_browse":
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


def endpoint_guards(record, original, before, fixture, data, stamp, save_data):
    current = {"original_unchanged": hashes(original) == before,
        "fixture_unchanged": fixture.is_file() and fixture.read_bytes() == data and fixture.stat().st_mtime_ns == stamp}
    save = record.get("save")
    path = Path(save["path"]) if save else None
    if record.get("scenario") == "drop-herb":
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
                if record["scenario"] == "drop-herb":
                    if not record.get("drop_measurement", {}).get("runtime_raw_inventory_measured") or "completed_save" not in record:
                        raise AssertionError("Drop/取消/正式Save量测未全部完成")
                    final_data = Path(record["completed_save"]["snapshot"]).read_bytes()
                    final_stamp = record["completed_save"]["mtime_ns"]
                    if hashlib.sha256(final_data).hexdigest() != record["completed_save"]["sha256"]:
                        raise AssertionError("Drop测量快照在完成后被改变")
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
    if scenario not in ("controls", "herb-full", "herb-injured", "drop-herb"):
        raise ValueError("未知物品校准场景")
    sdl = getattr(args, "sdl", None)
    if scenario == "drop-herb" and sdl is not None:
        raise ValueError("drop-herb当前仅用于原版校准，不接受--sdl；未创建产物或启动窗口")
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
              "original_before": before, "processes": [], "inputs": [], "checkpoints": []}
    cases = []
    if sdl is not None:
        report["cases"] = cases
        report["known_capability_differences"] = {"item_submenu_disabled": {"original": [0, 1, 0, 0], "sdl": [0, 1, 1, 1]},
            "same_direction_keys": "native RIGHT/DOWN select EQT/Drop; SDL stays Use0",
            "full_resources_checked_per_endpoint": True, "cross_end_capability_equivalence_claimed": False}
    save_data = data
    try:
        source_override = None
        if scenario == "herb-injured":
            save_data, source_override, manifest = derive_injured(decode_save(fixture), data)
            derived = output / "herb-injured.SAV"
            derived.write_bytes(save_data)
            if decode_save(derived) != source_override:
                raise AssertionError("草药受控SAV的XOR/checksum往返不符")
            report["derived_fixture"] = manifest
            (output / "fixture-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        prepared = prepare(original) if scenario in ("controls", "drop-herb") else prepare(
            original, source=source_override, herb=True)
        source, focus, assets, actor, probe, templates, fields = prepared
        if scenario == "drop-herb":
            prepare_drop(original, source, assets, actor, fields)
            report["scope"] = "original-only exact START: Drop192 cancel -> one Drop -> two-slot Use view -> layered cancel -> unacted -> explicit UI Save/full-container measurement"
            report["excluded"] = "SDL pairing, Equip/Give/Use effects, other Drop items/empty inventory, exact UI RNG/timing/audio and whole-owner equivalence"
        elif scenario != "controls":
            report["scope"] = "Continue from same START geometry -> herb192 target cancel -> one Use -> acted, F2 HP/MP/LV/EX/two visible slots -> close"
            report["excluded"] = "SDL pairing; raw16B inventory/stale-tail runtime measurement, post-END Save, exact RNG/floating heal/animation/audio and whole-owner equivalence"
        report["runtime_fixture_sha256"] = hashlib.sha256(save_data).hexdigest()
        report["resource_fields"] = fields
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name, path in (("driver", args.driver), ("dosbox", args.dosbox))}
        if sdl is not None:
            report["binaries"]["sdl"] = {"path": str(sdl.resolve()), "sha256": hashlib.sha256(sdl.read_bytes()).hexdigest()}
            report["excluded"] = report["excluded"].replace("SDL pairing; ", "")
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
                "status": "failed", "scenario": scenario, "scope": report["scope"], "excluded": report["excluded"],
                "fixture": report["fixture"], "runtime_fixture_sha256": report["runtime_fixture_sha256"],
                "resource_fields": fields, "binaries": report["binaries"], "tool_sources": report["tool_sources"],
                "processes": [], "inputs": [], "checkpoints": []}
            record["kind"] = kind
            record["role_expected_submenu_disabled"] = [0, 1, 0, 0] if kind == "original" else [0, 1, 1, 1]
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
            for key in ("original_unchanged", "fixture_unchanged", "copy_save_unchanged_after_cleanup", "cleanup_passed"):
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
    parser.add_argument("--scenario", choices=("controls", "herb-full", "herb-injured", "drop-herb"), default="controls")
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
