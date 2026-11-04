#!/usr/bin/env python3
"""原版-only ITEM子菜单／Use列表校准；没有真实使用、装备、丢弃或交接。

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
import sys

from fd2_item_list_oracle import prepare as prepare_templates
from run_original_comparison import Capture, SIZE, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, save_checkpoint, wait_state, wait_title
from run_original_end_turn import RoundOracle
from run_original_load import focus_cycle_assets, focused_actor, require_source_unchanged
from run_original_player import PlayerOracle, exact_tile, screen, upper_command_composite
from run_original_result import selection_probe, selection_state

def prepare(original):
    source, focus, assets, templates, fields = prepare_templates(original, original / "START.SAV")
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
    return source, focus, assets, actor, probe, templates, fields


def list_page(frame, templates, fields, selected=0):
    if selected not in (0, 1, 2):
        raise ValueError("物品资源门只支持真实三槽索引")
    if frame.size != SIZE:
        return None
    names = (f"selected_{selected}", "actor_name", "current_max_hp", "current_max_mp")
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
    return not any(list_page(frame, templates, fields, selected) for selected in range(3))


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


def exercise(capture, source, focus, assets, actor, probe, templates, fields, titles, save, data, stamp, report):
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
    output.mkdir(parents=True, exist_ok=False)
    fixture = original / "START.SAV"
    before, data, stamp = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "probe_only": True, "probe_cases_passed": False, "status": "failed",
              "scope": "real START raw0 ITEM submenu, Use navigation, code0 refusal and layered cancel",
              "excluded": "SDL pairing; actual Use/Equip/Drop/Give, effects/targets, memory-owner snapshot, UI RNG/timing/audio",
              "capture": "Windows.Graphics.Capture HWND only", "platform": platform.platform(),
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(data).hexdigest(), "mtime_ns": stamp},
              "original_before": before, "processes": [], "inputs": [], "checkpoints": []}
    save, save_stamp = None, None
    try:
        source, focus, assets, actor, probe, templates, fields = prepare(original)
        report["resource_fields"] = fields
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name, path in (("driver", args.driver), ("dosbox", args.dosbox))}
        for name, (at, image) in templates.items():
            image.save(output / ("expected-" + name + ".png"))
        game_copy = output / "game"
        shutil.copytree(original, game_copy)
        save = game_copy / "FD2.SAV"
        save.write_bytes(data)
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
            exercise(capture, source, focus, assets, actor, probe, templates, fields,
                     title_templates(original / "FDOTHER.DAT"), save, data, save_stamp, report)
            require_source_unchanged(save, data, save_stamp)
        require_source_unchanged(save, data, save_stamp)
        report["probe_cases_passed"], report["status"] = True, "passed"
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_unchanged"] = hashes(original) == before
        report["fixture_unchanged"] = (fixture.is_file() and fixture.read_bytes() == data and fixture.stat().st_mtime_ns == stamp)
        report["copy_save_unchanged_after_cleanup"] = (save is not None and save.is_file() and
            save.read_bytes() == data and save.stat().st_mtime_ns == save_stamp)
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
