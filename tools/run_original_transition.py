#!/usr/bin/env python3
"""B5标题手工LOAD→城镇/酒店四槽→stage1入场的原版／SDL对照。

依据field_town_prepare_execute @0x26152（原sub_26152）、field_hotel_menu_execute
@0x29300（原field_manual_slot_dispatch）、field_manual_slot_save @0x2968d（原sub_2968D）、
field_manual_slot_load、field_manual_slot_picker及field_stage1_entry_play @0x32d18（原sub_32D18）。
完整记录来源及字段写集由纯oracle声明；controlled五人不能冒称自然通关或完整B5通过。
"""
import argparse
import ast
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time
import wave

from PIL import Image

from fd2_transition_oracle import (derive_prepare_fixture, resources, write_manual_slot,
    picker_image, picker_match, town_images, hotel_menu_match, notice_resources,
    notice_match, entry_pages, entry_geometry, entry_browse, check_entry_measurement,
    entry_initialized_records)
from fd2_opening_oracle import page_match
from run_original_comparison import Capture, hashes, running, title_templates, compare
from run_original_battle_menu import wait_state, wait_title
from run_original_end_turn import RoundOracle, round_assets
from run_original_player import exact_tile
from run_original_load import require_source_unchanged, wait_measured_save
from run_graphics_tests import decode_save
from run_original_item_list import paired_checkpoint_records, rgb_difference
from run_original_reward import endpoint_guard


def prepare(original):
    encoded, plain, state, manifest = derive_prepare_fixture(original)
    assets = resources(original)
    field_assets = round_assets(original)
    # 全部资源和所有可能显示的slot页面在Popen前验证。
    for selected in range(4): picker_image(assets, plain, selected)
    towns = [town_images(original, assets, state, selected) for selected in range(5)]
    notices = {fragment: notice_resources(original, assets, fragment) for fragment in (585, 586, 660, 478)}
    notices[513] = notice_resources(original, assets, 513, portrait=75, origin=(95, 119))
    geometry = entry_geometry(original, state)
    from run_original_comparison import archive_entry
    metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 4)
    return {"encoded": encoded, "plain": plain, "state": state, "manifest": manifest,
        "assets": assets, "field_assets": field_assets, "towns": towns, "notices": notices,
        "titles": title_templates(original / "FDOTHER.DAT"),
        "pages": entry_pages(original, field_assets, state), "geometry": geometry,
        "metadata": metadata, "initialized": entry_initialized_records(original, state, geometry)}


def town_match(frame, prepared, selected):
    for phase, image in enumerate(prepared["towns"][selected]):
        found = exact_tile(frame, image, (4, 4))
        if found:
            return dict(found, kind="town_selector", selected=selected, marker_phase=phase,
                position=[4, 4], size=[312, 192], opaque_black_included=True)
    return None


def audio_toggle(capture, report, *, cleanup=False):
    recording = report.get("native_audio")
    if not recording or not recording["enabled"]: return
    if recording.get("stopped"): return
    if cleanup and not recording.get("started"): return
    action = "stop" if recording.get("started") else "start"
    capture.key("HOST_W")  # DOSBox-X官方MMODHOST+W；driver严格验证owned前台。
    recording[action + "_time"] = time.monotonic()
    recording["started" if action == "start" else "stopped"] = True
    if cleanup: recording["stopped_during_cleanup"] = True


def collect_native_audio(case, folder):
    recording = case.get("native_audio")
    if not recording or not recording["enabled"]: return
    files = []
    for path in sorted((folder / "captures").glob("*.wav")):
        with wave.open(str(path), "rb") as stream:
            details = {"channels": stream.getnchannels(), "sample_width": stream.getsampwidth(),
                "sample_rate": stream.getframerate(), "frames": stream.getnframes(), "compression": stream.getcomptype()}
        if details["compression"] != "NONE" or details["sample_width"] != 2 or details["sample_rate"] != 44100 or details["frames"] <= 0:
            raise AssertionError("原版WAV未按本轮PCM44100配置完整生成")
        files.append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), **details})
    recording.update(files=files, recording_only=True, a5_equivalence_validated=False)
    if case.get("business_sequence_completed") and (not files or not recording.get("stopped")):
        raise AssertionError("本轮要求的原版原生录音未完整停止/落盘")


def exercise(capture, prepared, save, guard, report):
    assets, field_assets = prepared["assets"], prepared["field_assets"]
    plain = prepared["plain"]
    def checkpoint(name, predicate, **options):
        result = wait_state(capture, name, predicate, report["checkpoints"], **options)
        require_source_unchanged(save, *guard)
        return result
    def picker(name, selected):
        image = picker_image(assets, plain, selected)
        return checkpoint(name, lambda frame: picker_match(frame, image), allow_low_color_resource=True)
    def town(name, selected):
        return checkpoint(name, lambda frame: town_match(frame, prepared, selected))
    def hotel(name, selected, fragment):
        def predicate(frame):
            menu = hotel_menu_match(frame, assets, selected)
            notice = notice_match(frame, prepared["notices"][fragment], field_assets)
            return dict(menu, notice=notice) if menu and notice else None
        return checkpoint(name, predicate)
    wait_title(capture, prepared["titles"][3][0], timeout=90)
    require_source_unchanged(save, *guard)
    audio_toggle(capture, report)
    capture.key("DOWN")
    checkpoint("title-load-selected", lambda frame: {"kind": "title_load", "selected": 1}
               if compare(prepared["titles"][3][1], frame)["ok"] else None)
    capture.key("ENTER")
    picker("title-picker-0", 0)
    # clamp两端和全部四个选择态；每一键均有完整26660像素ACK。
    for ordinal, (key, selected) in enumerate((("UP", 0), ("DOWN", 1), ("DOWN", 2),
            ("DOWN", 3), ("DOWN", 3), ("UP", 2), ("UP", 1), ("UP", 0))):
        capture.key(key); picker(f"title-picker-nav-{ordinal}", selected)
    capture.key("ENTER"); town("town-initial-0", 0)
    for ordinal, selected in enumerate((1, 2, 3, 4, 0)):
        capture.key("LEFT"); town(f"town-left-{ordinal}", selected)
    capture.key("RIGHT"); town("town-right-wrap-4", 4)
    capture.key("LEFT"); town("town-left-wrap-0", 0)
    capture.key("ENTER"); hotel("hotel-greeting", 0, 585)
    capture.key("RIGHT"); hotel("hotel-save-selected", 1, 585)
    capture.key("ENTER"); picker("hotel-save-picker-0", 0)
    report["manual_saves"] = []
    for slot in range(4):
        if slot:
            capture.key("DOWN"); picker(f"hotel-save-picker-{slot}", slot)
        expected = write_manual_slot(plain, prepared["state"], slot)
        capture.key("ENTER")
        actual = wait_measured_save(save, *guard)
        snapshot = capture.artifacts / f"manual-slot-{slot}.SAV"
        snapshot.write_bytes(save.read_bytes())
        if actual != expected:
            differences = [i for i, (a, b) in enumerate(zip(expected, actual)) if a != b]
            raise AssertionError(f"manual slot{slot}完整容器不符：{differences[:24]}")
        plain = actual
        guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
        report["manual_saves"].append({"slot": slot, "snapshot": str(snapshot),
            "sha256": hashlib.sha256(guard[0]).hexdigest(), "whole_plaintext_matches": True,
            "scope": "full0xa00 plus meta0..9/checksum; all other bytes preserved"})
        checkpoint(f"hotel-save-{slot}-notice660", lambda frame: notice_match(frame, prepared["notices"][660], field_assets))
        capture.key("ENTER"); picker(f"hotel-save-{slot}-reopened", slot)
    capture.key("ESC"); hotel("hotel-save-cancelled", 1, 586)
    capture.key("RIGHT"); hotel("hotel-load-selected", 2, 586)
    capture.key("ENTER"); picker("hotel-load-picker-0", 0)
    for slot in (1, 2, 3):
        capture.key("DOWN"); picker(f"hotel-load-picker-{slot}", slot)
    capture.key("ENTER")
    checkpoint("hotel-load-notice478", lambda frame: notice_match(frame, prepared["notices"][478], field_assets))
    capture.key("ENTER"); hotel("hotel-load-returned-menu", 2, 586)
    # Load/取消无文件写入：所有checkpoint沿同bytes+mtime守卫。
    capture.key("ESC"); town("hotel-escape-town", 0)
    for selected in (1, 2):
        capture.key("LEFT"); town(f"town-deploy-{selected}", selected)
    buttons = RoundOracle(field_assets, (0, 0))
    def deployment(frame, selected):
        notice = notice_match(frame, prepared["notices"][513], field_assets)
        choice = buttons.buttons(frame, selected)
        return dict(choice, notice=notice) if choice and notice else None
    capture.key("ENTER"); checkpoint("deploy-default-yes", lambda frame: deployment(frame, 0))
    capture.key("RIGHT"); checkpoint("deploy-no-selected", lambda frame: deployment(frame, 1))
    capture.key("ENTER"); town("deploy-no-town", 2)
    capture.key("ENTER"); checkpoint("deploy-second-yes", lambda frame: deployment(frame, 0))
    capture.key("ESC"); town("deploy-escape-town", 2)
    capture.key("ENTER"); checkpoint("deploy-final-yes", lambda frame: deployment(frame, 0))
    capture.key("ENTER")
    report["entry_acknowledgements"] = []
    for ordinal, page in enumerate(prepared["pages"]):
        name = f"entry-wait-{ordinal:02d}-fragment{page['fragment']}"
        checkpoint(name, lambda frame, p=page: page_match(frame, p, field_assets), timeout=90)
        report["entry_acknowledgements"].append({"name": name, "fragment": page["fragment"],
            "portrait_id": page["portrait_id"], "key": "ENTER"})
        capture.key("ENTER")
    _, first = checkpoint("entry-first-browse", lambda frame: entry_browse(frame, field_assets, prepared["geometry"]))
    report["entry_first_browse"] = first
    x, y = first["focus_screen"]
    if x < 52 or not 28 <= y <= 148:
        raise AssertionError("stage1首焦点没有已确认无滚屏LEFT测量格；未补键")
    capture.key("LEFT")
    menu = RoundOracle(field_assets, (x - 24, y))
    checkpoint("entry-measure-left", menu.closed_browse)
    capture.key("ENTER"); checkpoint("entry-parent", lambda frame: menu.menu(frame, 0))
    capture.key("ENTER"); checkpoint("entry-secondary", lambda frame: menu.secondary(frame, 0))
    capture.key("LEFT"); checkpoint("entry-save-selected", lambda frame: menu.secondary(frame, 1))
    capture.key("ENTER"); checkpoint("entry-save-yes", lambda frame: menu.buttons(frame, 0))
    capture.key("ENTER")
    measured = wait_measured_save(save, *guard)
    snapshot = capture.artifacts / "stage1-entry-measured.SAV"
    snapshot.write_bytes(save.read_bytes())
    report["entry_measurement"] = check_entry_measurement(measured, plain, prepared["state"], prepared["geometry"], first,
        prepared["metadata"], prepared["initialized"])
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    checkpoint("entry-save-returned-browse", menu.closed_browse)
    audio_toggle(capture, report)
    report["completed_save"] = {"snapshot": str(snapshot), "sha256": hashlib.sha256(guard[0]).hexdigest(),
                               "mtime_ns": guard[1]}


def compare_endpoints(cases, output, prepared):
    if [case["kind"] for case in cases] != ["original", "sdl"] or any(not case["probe_cases_passed"] for case in cases):
        raise AssertionError("B5配对必须原版先、正常清理后SDL，两端全部完成")
    records = [paired_checkpoint_records(case, output / case["kind"])[0] for case in cases]
    if records[0].keys() != records[1].keys(): raise AssertionError("B5业务检查点集合不同")
    saves = [case["manual_saves"] for case in cases]
    if [[s["slot"] for s in rows] for rows in saves] != [[0,1,2,3], [0,1,2,3]]:
        raise AssertionError("B5缺少四次真实manual Save")
    for first, second in zip(*saves):
        if Path(first["snapshot"]).read_bytes() != Path(second["snapshot"]).read_bytes():
            raise AssertionError("manual Save完整编码容器不等")
    measurements = [case["entry_measurement"] for case in cases]
    for field in ("stage", "turn", "active_count", "actors", "camera", "first_focus", "measuring_focus", "metadata_prefix_sha256"):
        if measurements[0][field] != measurements[1][field]: raise AssertionError("B5入场字段不同：" + field)
    actual = [decode_save(Path(case["completed_save"]["snapshot"])) for case in cases]
    for index, (_, mask) in enumerate(prepared["initialized"]):
        start = 0x12a3 + index * 80
        if any(flag and a != b for a, b, flag in zip(actual[0][start:start+80], actual[1][start:start+80], mask)):
            raise AssertionError("B5两端已初始化active byte不等")
    rgb = []
    for name, first in records[0].items():
        second = records[1][name]
        if first["kind"] != second["kind"]: raise AssertionError("B5检查点资源类别不同：" + name)
        frames = [Image.open(output / case["kind"] / (name + ".png")).convert("RGB") for case in cases]
        kind, areas = first["kind"], []
        if kind == "manual_slot_picker": areas = [((5,112),(310,86),None)]
        elif kind == "town_selector":
            mask = Image.new("L", (312,192), 255)
            x,y = prepared["assets"]["town_xy"][first["selected"]]; mask.paste(0,(x,y,x+24,y+24))
            areas = [((4,4),(312,192),mask)]
        elif kind == "hotel_menu": areas = [((5,112),(310,86),None),((220,8),(96,80),None)]
        elif kind == "resource_proven_opening_wait":
            page = next(p for p in prepared["pages"] if p["fragment"] == first["fragment"] and p["token_offset"] == first["token_offset"])
            areas = [((0,0),(320,200),page["text"])]
        elif kind == "prepare_notice" or first.get("notice"):
            notice = first if kind == "prepare_notice" else first["notice"]
            areas = [((0,0),(320,200),prepared["notices"][notice["fragment"]]["text"])]
        for position,size,mask in areas:
            diff = rgb_difference(*frames, position, size, mask)
            if not diff["within_tolerance"]: raise AssertionError("B5实际静态资源RGB不同：" + name)
            rgb.append({"name":name,"position":list(position),"size":list(size),**diff})
    return {"common_checkpoints":len(records[0]),"manual_four_saves_full_encoded_equal":True,
        "entry_initialized_bytes_equal":True,"entry_geometry_camera_and_focus_equal":True,
        "static_resource_rgb":rgb,"full_entry_save_bytes_equal_claimed":False,"shared_rng_aligned":False,
        "excluded":"metadata1171..2210 native heap; cache_index/NPC constructor holes; independent legal marker/blink phases; whole-scene/timing/audio"}


def reference_protocol(prepared):
    """继承证据的独立有序清单；不能用旧report自行声称的kind/selector作判据。"""
    rows = []
    def add(name, kind, selected=None, **fields): rows.append(dict(name=name, kind=kind, selected=selected, **fields))
    add("title-load-selected", "title_load", 1)
    add("title-picker-0", "manual_slot_picker", 0)
    for i, selected in enumerate((0, 1, 2, 3, 3, 2, 1, 0)):
        add(f"title-picker-nav-{i}", "manual_slot_picker", selected)
    add("town-initial-0", "town_selector", 0)
    for i, selected in enumerate((1, 2, 3, 4, 0)): add(f"town-left-{i}", "town_selector", selected)
    add("town-right-wrap-4", "town_selector", 4); add("town-left-wrap-0", "town_selector", 0)
    add("hotel-greeting", "hotel_menu", 0, fragment=585)
    add("hotel-save-selected", "hotel_menu", 1, fragment=585)
    add("hotel-save-picker-0", "manual_slot_picker", 0)
    for slot in range(4):
        if slot: add(f"hotel-save-picker-{slot}", "manual_slot_picker", slot)
        add(f"hotel-save-{slot}-notice660", "prepare_notice", fragment=660, saved_slot=slot)
        add(f"hotel-save-{slot}-reopened", "manual_slot_picker", slot)
    add("hotel-save-cancelled", "hotel_menu", 1, fragment=586)
    add("hotel-load-selected", "hotel_menu", 2, fragment=586)
    for slot in range(4): add(f"hotel-load-picker-{slot}", "manual_slot_picker", slot)
    add("hotel-load-notice478", "prepare_notice", fragment=478)
    add("hotel-load-returned-menu", "hotel_menu", 2, fragment=586)
    add("hotel-escape-town", "town_selector", 0)
    for selected in (1, 2): add(f"town-deploy-{selected}", "town_selector", selected)
    for name, kind, selected in (("deploy-default-yes", "confirmation", 0),
            ("deploy-no-selected", "confirmation", 1), ("deploy-no-town", "town_selector", 2),
            ("deploy-second-yes", "confirmation", 0), ("deploy-escape-town", "town_selector", 2),
            ("deploy-final-yes", "confirmation", 0)):
        add(name, kind, selected, **({"fragment": 513} if kind == "confirmation" else {}))
    for i, page in enumerate(prepared["pages"]):
        add(f"entry-wait-{i:02d}-fragment{page['fragment']}", "resource_proven_opening_wait", page=page)
    add("entry-first-browse", "stage1_first_browse")
    for name, kind, selected in (("entry-measure-left", "browse", None), ("entry-parent", "parent_menu", 0),
            ("entry-secondary", "secondary_menu", 0), ("entry-save-selected", "secondary_menu", 1),
            ("entry-save-yes", "confirmation", 0), ("entry-save-returned-browse", "browse", None)):
        add(name, kind, selected)
    return rows


def reference_keys(page_count):
    keys = ["DOWN", "ENTER", "UP", "DOWN", "DOWN", "DOWN", "DOWN", "UP", "UP", "UP", "ENTER"]
    keys += ["LEFT"] * 5 + ["RIGHT", "LEFT", "ENTER", "RIGHT", "ENTER"]
    for slot in range(4): keys += (["DOWN"] if slot else []) + ["ENTER", "ENTER"]
    keys += ["ESC", "RIGHT", "ENTER", "DOWN", "DOWN", "DOWN", "ENTER", "ENTER", "ESC", "LEFT", "LEFT",
             "ENTER", "RIGHT", "ENTER", "ENTER", "ESC", "ENTER", "ENTER"]
    return keys + ["ENTER"] * page_count + ["LEFT", "ENTER", "ENTER", "LEFT", "ENTER", "ENTER"]


def validate_original_reference(path, prepared, original):
    """只读复核已完成的original端；父任务失败不等于其已通过original端失败。

    必须保留正常endpoint退出和无残留Job收据。逐一重验原PNG资源、四次整档、
    entry已知写集、依赖源码及二进制；不把离线复核称为本次新实机执行。
    """
    path = path.resolve()
    document = json.loads(path.read_text(encoding="utf-8"))
    if (document.get("scenario") != "prepare-controlled" or document.get("capture_backend") != "Windows.Graphics.Capture HWND" or
            document.get("derived_fixture") != prepared["manifest"] or document.get("geometry") != prepared["geometry"] or
            document.get("original_before") != hashes(original) or not document.get("original_unchanged") or
            not document.get("original_all_mtimes_unchanged")):
        raise ValueError("继承原版来源/fixture/几何/只读守卫不匹配")
    cases = [case for case in document.get("cases", []) if case.get("kind") == "original"]
    if len(cases) != 1: raise ValueError("继承证据缺少唯一original端")
    case, folder = cases[0], path.parent / "original"
    if (case.get("status") != "passed" or not all(case.get(key) is True for key in
            ("probe_cases_passed", "business_sequence_completed", "original_unchanged", "fixture_unchanged",
             "copy_save_matches_completed_state", "cleanup_passed"))):
        raise ValueError("继承的original端未完成或清理/最终守卫失败")
    processes = case.get("processes", [])
    if len(processes) != 1 or not processes[0].get("cleaned") or processes[0].get("forced_cleanup") or processes[0].get("exit_code") != 0:
        raise ValueError("继承original端不是正常退出0且正常清理")
    actual_keys = [row["key"] for row in case.get("inputs", []) if "key" in row]
    expected_keys = reference_keys(len(prepared["pages"]))
    if case.get("native_audio", {}).get("enabled"): expected_keys = ["HOST_W"] + expected_keys + ["HOST_W"]
    if actual_keys != expected_keys or any(row.get("pid") != processes[0]["pid"] for row in case.get("inputs", [])):
        raise ValueError("继承原版逐键协议/owned PID不匹配，不能复用补键或缺ACK记录")
    receipt_path = path.parent.parent / "job-receipt.json"
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    job = receipt.get("job", {})
    if (any(job.get(key) != 0 for key in ("timed_out", "residual", "forced")) or job.get("cleaned") != 1 or
            not all(receipt.get(key) is True for key in ("frozen_tools_unchanged", "frozen_driver_unchanged", "actual_original_unchanged", "snapshot_original_unchanged"))):
        raise ValueError("继承Job缺少完整无超时/残留/强制清理及immutable守卫")
    # 核原版使用的真实依赖。只有本runner新增的继承run/main不要求全文件相同；
    # 其原有执行函数必须AST相同，不能把改过的协议当旧版本同一对照。
    current_tree = ast.parse(Path(__file__).read_text(encoding="utf-8"))
    current_functions = {node.name: ast.dump(node, include_attributes=False) for node in current_tree.body if isinstance(node, ast.FunctionDef)}
    dependency_deltas = []
    for name, source in case.get("tool_sources", {}).items():
        source_path = Path(source["path"])
        if hashlib.sha256(source_path.read_bytes()).hexdigest() != source["sha256"]:
            raise ValueError("继承依赖源码已改：" + name)
        module = sys.modules.get(name) if name != "run_original_transition" else sys.modules.get(__name__)
        if module is None: raise ValueError("继承依赖未在当前协议加载：" + name)
        current_path = Path(module.__file__)
        if name == "run_original_transition" or current_path.resolve() == Path(__file__).resolve():
            old_tree = ast.parse(source_path.read_text(encoding="utf-8"))
            for node in old_tree.body:
                if isinstance(node, ast.FunctionDef) and node.name not in ("run", "main"):
                    if current_functions.get(node.name) != ast.dump(node, include_attributes=False):
                        raise ValueError("继承原版执行合同已改变：" + node.name)
        elif hashlib.sha256(current_path.read_bytes()).hexdigest() != source["sha256"]:
            if name != "run_original_result":
                raise ValueError("当前资源/协议依赖不同于继承原版：" + name)
            # 同阶段新增的独立--b5-defeat不被本prepared协议调用；只允许其
            # entry/CLI变化，其余整个AST（含selection与资源helper）必须相同。
            unrelated = {"exercise_original", "exercise_sdl_defeat_continuation", "run", "main"}
            def used_ast(file):
                tree = ast.parse(file.read_text(encoding="utf-8"))
                tree.body = [node for node in tree.body if not isinstance(node, ast.FunctionDef) or node.name not in unrelated]
                return ast.dump(tree, include_attributes=False)
            if used_ast(source_path) != used_ast(current_path):
                raise ValueError("当前prepared使用的result资源/selection helper已改变")
            dependency_deltas.append({"module": name, "inherited_sha256": source["sha256"],
                "current_sha256": hashlib.sha256(current_path.read_bytes()).hexdigest(),
                "excluded_entry_functions": sorted(unrelated),
                "all_other_module_ast_equal": True, "scope": "independent B5 defeat entry, not used by prepared"})
    required_modules = {"run_original_transition", "fd2_transition_oracle", "fd2_opening_oracle", "run_original_comparison"}
    if not required_modules <= set(case.get("tool_sources", {})): raise ValueError("继承依赖清单缺少必要owner")
    for name in ("driver", "dosbox"):
        binary = case["binaries"][name]
        if hashlib.sha256(Path(binary["path"]).read_bytes()).hexdigest() != binary["sha256"]:
            raise ValueError("继承二进制已改：" + name)
    if receipt.get("driver_sha256") != case["binaries"]["driver"]["sha256"] or receipt.get("dosbox_sha256") != case["binaries"]["dosbox"]["sha256"]:
        raise ValueError("继承Job与endpoint二进制不一致")
    captured = case.get("capture_hashes", {})
    if not captured: raise ValueError("继承capture清单为空")
    for name, sha in captured.items():
        file = folder / name
        if Path(name).name != name or hashlib.sha256(file.read_bytes()).hexdigest() != sha:
            raise ValueError("继承capture/SAV已改或越界：" + name)
    plain = prepared["plain"]
    saves = case.get("manual_saves", [])
    if [row.get("slot") for row in saves] != [0, 1, 2, 3]: raise ValueError("继承证据缺少四次完整手工Save")
    for slot, save in enumerate(saves):
        file = Path(save["snapshot"])
        if file.resolve().parent != folder.resolve() or file.name not in captured:
            raise ValueError("继承manual附件不属于本轮original capture清单")
        plain = write_manual_slot(plain, prepared["state"], slot)
        if hashlib.sha256(file.read_bytes()).hexdigest() != save["sha256"] or decode_save(file) != plain:
            raise ValueError("继承manual整容器写集不符")
    completed = case["completed_save"]
    snapshot = Path(completed["snapshot"])
    if snapshot.resolve().parent != folder.resolve() or snapshot.name not in captured:
        raise ValueError("继承entry附件不属于本轮original capture清单")
    if hashlib.sha256(snapshot.read_bytes()).hexdigest() != completed["sha256"]:
        raise ValueError("继承entry最终Save已改")
    require_source_unchanged(folder / "game/FD2.SAV", snapshot.read_bytes(), completed["mtime_ns"])
    measurement = check_entry_measurement(decode_save(snapshot), plain, prepared["state"], prepared["geometry"],
        case["entry_first_browse"], prepared["metadata"], prepared["initialized"])
    if measurement != case["entry_measurement"]: raise ValueError("继承entry已知写集复核不同")
    rows = [row for row in case["checkpoints"] if row.get("kind") != "wgc_transient"]
    contract = reference_protocol(prepared)
    if [(r["name"], r["kind"]) for r in rows] != [(r["name"], r["kind"]) for r in contract]:
        raise ValueError("继承业务检查点缺失、乱序或类型错误")
    field_assets, assets = prepared["field_assets"], prepared["assets"]
    plain = prepared["plain"]
    first = case["entry_first_browse"]["focus_screen"]
    oracle = RoundOracle(field_assets, (first[0] - 24, first[1]))
    for expected, row in zip(contract, rows):
        name, kind, selected = expected["name"], expected["kind"], expected["selected"]
        if row.get("pid") != processes[0]["pid"] or name + ".png" not in captured:
            raise ValueError("继承检查点缺少owned PID或PNG清单")
        with Image.open(folder / (name + ".png")) as image: frame = image.convert("RGB")
        if "saved_slot" in expected: plain = decode_save(Path(saves[expected["saved_slot"]]["snapshot"]))
        if kind == "manual_slot_picker": found = picker_match(frame, picker_image(assets, plain, selected))
        elif kind == "town_selector": found = town_match(frame, prepared, selected)
        elif kind == "hotel_menu":
            found = hotel_menu_match(frame, assets, selected) and notice_match(frame, prepared["notices"][expected["fragment"]], field_assets)
        elif kind == "prepare_notice": found = notice_match(frame, prepared["notices"][expected["fragment"]], field_assets)
        elif kind == "resource_proven_opening_wait": found = page_match(frame, expected["page"], field_assets)
        elif kind == "stage1_first_browse": found = entry_browse(frame, field_assets, prepared["geometry"])
        elif kind == "parent_menu": found = oracle.menu(frame, selected)
        elif kind == "secondary_menu": found = oracle.secondary(frame, selected)
        elif kind == "browse": found = oracle.closed_browse(frame)
        elif kind == "confirmation":
            found = oracle.buttons(frame, selected)
            if "fragment" in expected: found = found and notice_match(frame, prepared["notices"][expected["fragment"]], field_assets)
        elif kind == "title_load": found = compare(prepared["titles"][3][1], frame)["ok"]
        else: raise ValueError("继承资源类型未确认：" + kind)
        if not found: raise ValueError("继承原版实际PNG不符合当前完整资源门：" + name)
    audio = case.get("native_audio", {})
    if audio.get("enabled") and (not audio.get("started") or not audio.get("stopped") or not audio.get("files")):
        raise ValueError("继承原版声称录音但未完整开始/停止/落盘")
    for record in audio.get("files", []):
        if hashlib.sha256(Path(record["path"]).read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("继承原生WAV已改")
    provenance = {"report": str(path), "report_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "job_receipt": str(receipt_path), "job_receipt_sha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
        "source_endpoint": str(folder), "actual_original_pid": processes[0]["pid"],
        "revalidated_resource_checkpoints": len(rows), "capture_files_rehashed": len(captured),
        "explicit_unrelated_tool_deltas": dependency_deltas,
        "parent_run_passed": bool(document.get("ok")), "endpoint_passed": True,
        "offline_revalidation_not_new_runtime": True}
    return case, provenance


def copy_original_reference(case, provenance, destination):
    """复制审阅附件，保留旧PID/Job/输入的实际路径；不伪造本次启动记录。"""
    source = Path(provenance["source_endpoint"])
    destination.mkdir()
    for name, sha in case["capture_hashes"].items():
        shutil.copy2(source / name, destination / name)
        if hashlib.sha256((destination / name).read_bytes()).hexdigest() != sha or hashlib.sha256((source / name).read_bytes()).hexdigest() != sha:
            raise ValueError("继承capture复制前后hash不一致：" + name)
    inherited = copy.deepcopy(case)
    inherited["execution_origin"] = "inherited_original"
    inherited["provenance"] = provenance
    for row in inherited["manual_saves"]:
        row["original_snapshot"] = row["snapshot"]; row["snapshot"] = str(destination / Path(row["snapshot"]).name)
    completed = inherited["completed_save"]
    completed["original_snapshot"] = completed["snapshot"]; completed["snapshot"] = str(destination / Path(completed["snapshot"]).name)
    (destination / "game").mkdir()
    shutil.copy2(source / "game/FD2.SAV", destination / "game/FD2.SAV")
    for row in inherited.get("native_audio", {}).get("files", []):
        old = Path(row["path"]); (destination / "captures").mkdir(exist_ok=True)
        target = destination / "captures" / old.name; shutil.copy2(old, target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != row["sha256"] or hashlib.sha256(old.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError("继承WAV复制前后hash不一致")
        row["original_path"] = row["path"]; row["path"] = str(target)
    return inherited


def reference_unchanged(case, provenance):
    for field in ("report", "job_receipt"):
        if hashlib.sha256(Path(provenance[field]).read_bytes()).hexdigest() != provenance[field + "_sha256"]:
            raise ValueError("继承报告/Job收据在本轮被修改")
    source = Path(provenance["source_endpoint"])
    for name, sha in case["capture_hashes"].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != sha:
            raise ValueError("继承原版附件在本轮被修改：" + name)
    for record in case.get("native_audio", {}).get("files", []):
        if hashlib.sha256(Path(record["path"]).read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("继承原生WAV在本轮被修改")
    for record in list(case["tool_sources"].values()) + [case["binaries"][name] for name in ("driver", "dosbox")]:
        if hashlib.sha256(Path(record["path"]).read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("继承原版源码/二进制在本轮被修改")
    completed = case["completed_save"]
    require_source_unchanged(source / "game/FD2.SAV", Path(completed["snapshot"]).read_bytes(), completed["mtime_ns"])


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    if output == original or original in output.parents:
        raise ValueError("B5输出不能位于original_game或其解析子目录")
    if args.sdl is not None and not args.sdl.is_file():
        raise ValueError("SDL程序不存在；未创建产物或窗口")
    reference_path = getattr(args, "original_evidence", None)
    if reference_path is not None and args.sdl is None:
        raise ValueError("继承原版证据只用于本次SDL续跑；未创建产物或窗口")
    if reference_path is not None and (output == reference_path.resolve().parent or reference_path.resolve().parent in output.parents):
        raise ValueError("新输出不能写入继承原版证据目录或其子目录")
    prepared = prepare(original)
    reference = validate_original_reference(reference_path, prepared, original) if reference_path is not None else None
    before = hashes(original)
    stamps = {name: (original / name).stat().st_mtime_ns for name in before}
    output.mkdir(parents=True, exist_ok=False)
    report = {"ok": False, "probe_only": args.sdl is None, "probe_cases_passed": False, "status": "failed",
        "scenario": "prepare-controlled", "scope": "controlled authentic5roster manualLoad/town/hotel4slots/stage1entry",
        "excluded": "natural first-stage completion, native metadata heap tail/cache/NPC malloc holes, whole-map/timing/audio/RNG equivalence",
        "capture_backend": "Windows.Graphics.Capture HWND", "original_before": before,
        "derived_fixture": prepared["manifest"], "geometry": prepared["geometry"],
        "cases": [], "binaries": {}, "tool_sources": {}}
    report["composition"] = {"inherited_original": int(reference is not None),
        "executed_original": 0, "executed_sdl": 0, "fresh_pair": reference is None and args.sdl is not None}
    for name, path in (("driver", args.driver), ("dosbox", args.dosbox)) + ((("sdl", args.sdl),) if args.sdl is not None else ()):
        report["binaries"][name] = {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    project = Path(__file__).resolve().parents[1]
    for name, module in tuple(sys.modules.items()):
        candidate = getattr(module, "__file__", None)
        if not candidate: continue
        path = Path(candidate).resolve()
        if path.suffix == ".py" and (path.is_relative_to(project / "tools") or path.is_relative_to(project / "src/tests")):
            report["tool_sources"][name] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    (output / "fixture-manifest.json").write_text(json.dumps(prepared["manifest"], ensure_ascii=False, indent=2), encoding="utf-8")
    guards = {}
    fixture = original / "START.SAV"
    fixture_data, fixture_stamp = fixture.read_bytes(), fixture.stat().st_mtime_ns
    try:
        if reference is not None:
            case = copy_original_reference(*reference, output / "original")
            report["cases"].append(case)
            save = output / "original/game/FD2.SAV"
            guards["original"] = save, [save.read_bytes(), save.stat().st_mtime_ns]
            if getattr(args, "record_native_audio", False) and not case.get("native_audio", {}).get("files"):
                raise ValueError("继承原版没有本轮要求的完整原生录音；不会重新发HOST_W")
        roles = ("sdl",) if reference is not None else (("original",) if args.sdl is None else ("original", "sdl"))
        for role in roles:
            work = output / role; work.mkdir()
            environment = os.environ.copy()
            for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"): environment.pop(name, None)
            if role == "original":
                game_copy = work / "game"; shutil.copytree(original, game_copy)
                save = game_copy / "FD2.SAV"
                config = work / "dosbox.conf"
                audio = bool(getattr(args, "record_native_audio", False))
                capture_config = ""
                if audio:
                    (work / "captures").mkdir()
                    capture_config = f"captures={(work / 'captures').as_posix()}\nshow recorded filename=false\n"
                config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n" + capture_config +
                    "[mixer]\nrate=44100\n[render]\naspect=false\nscaler=normal2x\n"
                    f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"; environment["FD2SDL_SAVE_PATH"] = str(save)
                command, cwd = [str(args.sdl.resolve())], root
            save.write_bytes(prepared["encoded"])
            guard = [prepared["encoded"], save.stat().st_mtime_ns]; guards[role] = save, guard
            if decode_save(save) != prepared["plain"]: raise AssertionError("受控manifest codec往返失败；未启动窗口")
            case = {"kind": role, "status": "failed", "probe_cases_passed": False, "probe_only": True, "ok": False,
                "checkpoints": [], "processes": [], "inputs": [], "initial_save_mtime_ns": guard[1],
                "binaries": report["binaries"], "tool_sources": report["tool_sources"]}
            case["native_audio"] = {"enabled": role == "original" and bool(getattr(args, "record_native_audio", False)),
                "started": False, "stopped": False, "scope": "native recorder only; not A5/A7 equivalence"}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, work / "game.log", args.driver.resolve(), case["processes"]) as process:
                    report["composition"]["executed_" + role] += 1
                    # B5不在窗口重建阶段发片头skip键；先证明完整标题。
                    capture = Capture(args.driver.resolve(), process, work, case["inputs"], startup_skip=True)
                    try:
                        exercise(capture, prepared, save, guard, case)
                        require_source_unchanged(save, *guard)
                        case["business_sequence_completed"] = True
                    finally:
                        failed = sys.exc_info()[0] is not None
                        try: audio_toggle(capture, case, cleanup=True)
                        except Exception as error:
                            case["audio_cleanup_error"] = str(error)
                            if not failed: raise
                case["probe_cases_passed"], case["status"] = True, "passed"
            except Exception as error:
                case["error"] = str(error)
            finally:
                try: collect_native_audio(case, work)
                except Exception as error:
                    case["native_audio"]["error"] = str(error)
                    case.setdefault("error", str(error))
                    case["probe_cases_passed"] = False
                endpoint_guard(case, original, before, fixture, fixture_data, fixture_stamp, save, guard)
                case["capture_hashes"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in work.iterdir()
                    if p.is_file() and p.suffix.lower() in (".png", ".bmp", ".sav")}
                (work / "comparison.json").write_text(json.dumps(case, ensure_ascii=False, indent=2), encoding="utf-8")
            if not case["probe_cases_passed"]: raise AssertionError(case["error"])
        if args.sdl is not None: report["paired"] = compare_endpoints(report["cases"], output, prepared)
        report["probe_cases_passed"], report["status"] = True, "passed"
    except Exception as error:
        report["error"] = str(error)
    finally:
        checks = {"original_unchanged": hashes(original) == before,
            "original_all_mtimes_unchanged": all((original / name).stat().st_mtime_ns == stamp for name, stamp in stamps.items())}
        report.update(checks)
        if reference is not None:
            try:
                reference_unchanged(*reference)
                report["inherited_original_unchanged_after_sdl"] = True
            except Exception as error:
                report["inherited_original_unchanged_after_sdl"] = False
                checks["inherited_original_unchanged_after_sdl"] = False
                report.setdefault("error", str(error))
        for case in report["cases"]:
            if case["kind"] not in guards: continue
            save, guard = guards[case["kind"]]
            endpoint_guard(case, original, before, fixture, fixture_data, fixture_stamp, save, guard)
            (output / case["kind"] / "comparison.json").write_text(json.dumps(case, ensure_ascii=False, indent=2), encoding="utf-8")
        report["probe_cases_passed"] &= all(checks.values()) and all(case["probe_cases_passed"] for case in report["cases"])
        report["ok"] = args.sdl is not None and report["probe_cases_passed"] and len(report["cases"]) == 2
        if not report["probe_cases_passed"]: report["status"] = "failed"
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "driver", "dosbox", "artifacts"): parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--sdl", type=Path)
    parser.add_argument("--record-native-audio", action="store_true", help="原版标题确认后至entry Save记录原生WAV；不等于音频对齐验收")
    parser.add_argument("--original-evidence", type=Path, help="继承已严格复核的original端report，本次只执行SDL；不称fresh pair")
    parser.add_argument("--scenario", choices=("prepare-controlled",), default="prepare-controlled")
    args = parser.parse_args()
    if sys.platform != "win32": parser.error("B5原版校准要求Windows WGC，未启动窗口")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"B5校准不能启动：{error}。检查来源与新输出目录。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
