#!/usr/bin/env python3
"""原版-only ITEM列表控制／单次草药；不执行装备、丢弃或交接。

依据field_player_item_command_execute @0x1bbdc（原登记同名）、
field_unit_inventory_select @0x1b932（原sub_1B932）、
field_unit_inventory_select_step @0x1b9de（原sub_1B9DE）及
field_unit_detail_equipment_draw @0x184c0（原登记同名）。
取消路径必须结合18d8c @19055..19065与18890零步controller：
Use列表→ITEM子菜单→COMMAND→选择范围→browse。原版先校准，非SDL验收。
由宿主driver run-job串行约束进程树；只用WGC及本轮owned PID清理。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import sys

from PIL import Image

from audit_fdicon_opaque_zero import audit_fdicon
from fd2_item_list_oracle import AFTER_USE_INVENTORY, derive_injured, prepare as prepare_templates
from fd2_movement_oracle import movement_profile
from run_graphics_tests import decode_save
from run_original_comparison import Capture, SIZE, archive_entry, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, save_checkpoint, wait_state, wait_title
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle
from run_original_load import focus_cycle_assets, focused_actor, require_source_unchanged
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
    assets["herb"] = {"target_actor": target_actor, "acted_composites": acted,
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


def list_page(frame, templates, fields, selected=0):
    if selected not in range(len(fields["slots"])):
        raise ValueError("物品资源门只支持真实三槽索引")
    if frame.size != SIZE:
        return None
    names = (f"selected_{selected}", "actor_name", "current_max_hp", "current_max_mp")
    if "herb_contract" in fields:
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


def submenu_page(frame, assets, position, templates, fields, selected=0):
    if selected not in (0, 2, 3):
        raise ValueError("物品子菜单Give当前禁用，不能作为选中项")
    matches = []
    for item, command in enumerate((8, 9, 10, 11)):
        index = command * 3 + (2 if item == 1 else int(item == selected))
        at = tuple(a + b for a, b in zip(position, MENU_OFFSETS[item]))
        images = assets["upper_commands"][index] if item == 0 else [assets["commands"][index]]
        check = next((dict(frame=index, position=list(at), **result) for image in images
                      if (result := match_indexed(frame, image, at))["ok"]), None)
        if not check:
            return None
        matches.append(check)
    if not list_absent(frame, templates, fields):
        return None
    return {"kind": "item_submenu", "selected": selected, "disabled": [0, 1, 0, 0],
            "matches": matches, "inventory_list_absent": True, "operation_confirmed": False}


def command_page(frame, command, position, templates, fields):
    detail = command.command(frame, position, 2)
    if not detail or [match["frame"] for match in detail["matches"]] != [2, 5, 7, 9]:
        return None
    return {**detail, "native_first_enabled_item": True, "inventory_list_absent": True} if list_absent(frame, templates, fields) else None


def cancelled_browse(frame, assets, actor, probe, templates, fields):
    focused = focused_actor(frame, assets, actor)
    terrain = exact_tile(frame, probe["base"], probe["position"])
    if (focused and tuple(focused["focus_screen"]) == probe["actor_position"] and terrain and
            list_absent(frame, templates, fields) and
            not submenu_page(frame, assets, probe["actor_position"], templates, fields)):
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


def herb_browse(frame, source, assets, probe, templates, fields):
    plan = assets["herb"]
    acted = next((phase for phase, image in enumerate(plan["acted_composites"])
                  if match_indexed(frame, image, (52, 70))["ok"]), None)
    base = exact_tile(frame, probe["base"], probe["position"])
    terrain = plan["detail_close_terrain"]
    restored = exact_tile(frame, terrain["image"], terrain["position"])
    if acted is None or base is None or restored is None or not list_absent(frame, templates, fields):
        return None
    if PlayerOracle(source, assets, [], {}).command(frame, (52, 76)) or any(
            submenu_page(frame, assets, (52, 76), templates, fields, selected) for selected in (0, 2, 3)):
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
        lambda frame: herb_browse(frame, source, assets, probe, templates, fields), timeout=120)
    capture.key("F2")
    _, report["herb_measurement"] = checkpoint("herb-two-slot-f2-detail", lambda frame: herb_detail(frame, assets))
    capture.key("ESC")  # raw0无魔法，17aed一页后直接关闭。
    _, report["herb_detail_closed"] = checkpoint("herb-f2-closed-browse",
        lambda frame: herb_browse(frame, source, assets, probe, templates, fields))
    report["input_scope"] = {"valid_use_confirmations": 1, "target_open_count": 2, "target_cancel_count": 1,
        "post_use_skip_keys": [], "equip_drop_give_confirmations": 0, "save_or_end_sent": False,
        "runtime_raw_inventory_measured": False,
        "deferred_post_end_measurement": fields["herb_contract"]["deferred_post_end_measurement"]}


def exercise(capture, source, focus, assets, actor, probe, templates, fields, titles, save, data, stamp, report, *, scenario="controls"):
    def checkpoint(name, predicate, **kwargs):
        frame, detail = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
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
    checkpoint("item-submenu-default-use", lambda frame: submenu_page(frame, assets, position, templates, fields))
    if scenario != "controls":
        exercise_herb(capture, source, assets, probe, templates, fields, report, checkpoint)
        return
    for key, selected in (("RIGHT", 2), ("DOWN", 3), ("UP", 0), ("LEFT", 0)):
        capture.key(key)
        checkpoint(f"submenu-{key.lower()}", lambda frame, index=selected:
                   submenu_page(frame, assets, position, templates, fields, index))
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
    checkpoint("list-cancel-item-submenu", lambda frame: submenu_page(frame, assets, position, templates, fields))
    capture.key("ENTER")
    checkpoint("use-list-reopened-zero", lambda frame: list_page(frame, templates, fields))
    capture.key("ESC")
    checkpoint("list-cancel-item-submenu-again", lambda frame: submenu_page(frame, assets, position, templates, fields))
    capture.key("ESC")
    checkpoint("submenu-cancel-command", lambda frame: command_page(frame, command, position, templates, fields))
    capture.key("ESC")
    checkpoint("command-cancel-selection", lambda frame: selection_state(frame, assets, actor, probe))
    capture.key("ESC")
    checkpoint("selection-cancel-browse", lambda frame: cancelled_browse(frame, assets, actor, probe, templates, fields))
    report["input_scope"] = {"valid_use_confirmations": 0, "code0_confirmations": 4,
                             "equip_drop_give_confirmations": 0, "save_or_end_sent": False,
                             "cancel_route": ["Use list", "ITEM submenu", "COMMAND", "selection", "browse"]}


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入original_game")
    scenario = getattr(args, "scenario", "controls")
    if scenario not in ("controls", "herb-full", "herb-injured"):
        raise ValueError("未知物品校准场景")
    output.mkdir(parents=True, exist_ok=False)
    fixture = original / "START.SAV"
    before, data, stamp = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "probe_only": True, "probe_cases_passed": False, "status": "failed",
              "scenario": scenario, "scope": "real START raw0 ITEM submenu, Use navigation, code0 refusal and layered cancel",
              "excluded": "SDL pairing; actual Use/Equip/Drop/Give, effects/targets, memory-owner snapshot, UI RNG/timing/audio",
              "capture": "Windows.Graphics.Capture HWND only", "platform": platform.platform(),
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(data).hexdigest(), "mtime_ns": stamp},
              "original_before": before, "processes": [], "inputs": [], "checkpoints": []}
    save, save_stamp, save_data = None, None, data
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
        source, focus, assets, actor, probe, templates, fields = prepare(original) if scenario == "controls" else prepare(
            original, source=source_override, herb=True)
        if scenario != "controls":
            report["scope"] = "Continue from same START geometry -> herb192 target cancel -> one Use -> acted, F2 HP/MP/LV/EX/two visible slots -> close"
            report["excluded"] = "SDL pairing; raw16B inventory/stale-tail runtime measurement, post-END Save, exact RNG/floating heal/animation/audio and whole-owner equivalence"
        report["runtime_fixture_sha256"] = hashlib.sha256(save_data).hexdigest()
        report["resource_fields"] = fields
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name, path in (("driver", args.driver), ("dosbox", args.dosbox))}
        for name, (at, image) in templates.items():
            image.save(output / ("expected-" + name + ".png"))
        game_copy = output / "game"
        shutil.copytree(original, game_copy)
        save = game_copy / "FD2.SAV"
        save.write_bytes(save_data)
        save_stamp = save.stat().st_mtime_ns
        config = output / "dosbox.conf"
        config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
            "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
            f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
        env = os.environ.copy()
        for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            env.pop(name, None)
        driver = args.driver.resolve()
        with running([str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"],
                     output, env, output / "dosbox.log", driver, report["processes"]) as process:
            capture = Capture(driver, process, output, report["inputs"])
            arguments = (capture, source, focus, assets, actor, probe, templates, fields,
                         title_templates(original / "FDOTHER.DAT"), save, save_data, save_stamp, report)
            if scenario == "controls":
                exercise(*arguments)
            else:
                exercise(*arguments, scenario=scenario)
                if any(key not in report for key in ("herb_outcome", "herb_measurement", "herb_detail_closed")):
                    raise AssertionError("草药单次使用/详情/关闭检查点未全部完成")
            require_source_unchanged(save, save_data, save_stamp)
        require_source_unchanged(save, save_data, save_stamp)
        report["probe_cases_passed"], report["status"] = True, "passed"
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_unchanged"] = hashes(original) == before
        report["fixture_unchanged"] = (fixture.is_file() and fixture.read_bytes() == data and fixture.stat().st_mtime_ns == stamp)
        report["copy_save_unchanged_after_cleanup"] = (save is not None and save.is_file() and
            save.read_bytes() == save_data and save.stat().st_mtime_ns == save_stamp)
        guards = all(report[key] for key in ("original_unchanged", "fixture_unchanged", "copy_save_unchanged_after_cleanup"))
        report["probe_cases_passed"] = report["probe_cases_passed"] and guards
        if not report["probe_cases_passed"]:
            report["status"] = "failed"
            report.setdefault("error", "物品校准最终来源/存档bytes或mtime守卫失败")
        report["capture_hashes"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in output.iterdir() if p.is_file() and p.suffix.lower() in (".png", ".bmp")}
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--scenario", choices=("controls", "herb-full", "herb-injured"), default="controls")
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
