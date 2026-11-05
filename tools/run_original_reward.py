#!/usr/bin/env python3
"""B4奖励、成长、学习与状态提示的受控原版校准／可选SDL配对。

field_unit_experience_reward_execute @0x1e292（原sub_1E292）、
field_growth_stat_present_apply @0x1e529（原sub_1E529）、
field_unit_magic_bit_set @0x1d79c（原sub_1D79C）；物理归属field_physical_attack_resolve @0x1ecc7、
field_physical_exchange @0x1548e，状态field_phase_status_tick @0x1a866。
只用正式Continue、真实键和WGC；fixture逐byte声明，不修改原文件或运行内存。
原版先行且正常清理后才启动SDL。没有--sdl时ok始终false，校准另记通过。
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import sys
import time

from PIL import Image

from fd2_reward_oracle import (SCENARIOS, derive_fixture, expected_state, reward_tables,
    reward_pages, page_match, magic_page)
from fd2_item_list_oracle import inventory_templates
from run_original_comparison import Capture, archive_entry, hashes, running, title_templates
from run_original_battle_menu import wait_state, wait_title, match_indexed
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle, round_assets, round_two_template, check_round_phase_log
from run_original_focus import native_focus_reference
from run_original_load import focused_actor, require_source_unchanged, check_load_measurement
from run_original_player import PlayerOracle, screen, sheet_frame, upper_command_composite, exact_tile
from run_original_result import selection_probe, selection_state
from run_original_magic_list import actor_focus_resources, detail_close_terrain
from run_original_item_list import (focus_steps, inventory_detail, measure_inventory_save,
    post_use_navigation_resources, post_use_navigation_state, use_range_probe,
    paired_checkpoint_records, rgb_difference)
from run_graphics_tests import decode_save


def actor_view(original, source, assets, raw):
    actor = source[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]
    icons = (original / "FDICON.B24").read_bytes()
    sprites = [sheet_frame(icons, actor[7] * 12 + phase) for phase in range(12)]
    for image in sprites: image.putpalette(assets["palette"])
    return dict(assets, upper_commands={index: [upper_command_composite(assets["commands"][index], sprite)
        for sprite in sprites] for index in range(3)})


def prepare(original, scenario):
    encoded, source, manifest = derive_fixture(original, scenario)
    tables, table_evidence = reward_tables(original / "FD2.EXE")
    model = expected_state(original, source, scenario, tables)
    assets = round_assets(original)
    raw = model["reward_actor"]
    pages = reward_pages(original, assets, source[UNITS + raw * STRIDE + 7],
                         model["status_events"] if scenario == "status" else model["events"])
    resources = {"source": source, "encoded": encoded, "manifest": manifest, "model": model,
        "assets": assets, "pages": pages, "tables": table_evidence,
        "titles": title_templates(original / "FDOTHER.DAT"), "original": original, "scenario": scenario}
    resources["source_actors"] = [actor_focus_resources(original, source, assets, i, assets["cursor"]) for i in (0, 1)]
    if scenario in ("heal", "learn"):
        resources["movement"] = selection_probe(original, source, 1, assets)
        actor = source[UNITS + STRIDE:UNITS + 2 * STRIDE]
        resources["magic_lists"] = [magic_page(original, actor, assets, i)[0] for i in (0, 1)]
        _, detail, fields = inventory_templates(original, actor, assets)
        resources["list_upper"] = {key: detail[key] for key in ("actor_name", "current_max_mp")}
        cursor = sheet_frame(archive_entry((original / "FDOTHER.DAT").read_bytes(), 1), 1)
        cursor.putpalette(assets["palette"])
        resources["target"] = actor_focus_resources(original, source, assets, 0, cursor)
        resources["target_probe"] = resources["movement"]
    elif scenario in ("physical-cap", "physical-overwrite"):
        # 即使起点处于ZOC，原range首步可走出；只用一格已确认可达的range ROI。
        probe = use_range_probe(original, source, assets, 1, 0, (8, 14))
        probe["actor_position"] = screen(source, (7, 14))
        resources["movement"] = probe
        resources["target_probe"] = probe
        resources["target"] = actor_focus_resources(original, source, assets, 11, assets["cursor"])
    resources["initial_focus"] = tuple(source[META + 5:META + 7])
    resources["end_world"] = (6, 14)
    # 所有实际发送的导航预先验证；不以启动后的失败试探固定夹具几何。
    for state, start, end in ((source, resources["initial_focus"], (6, 14)),
            (source, (7, 14), (6, 14)), (source, (8, 14), (6, 14))):
        focus_steps(state, start, end)
    final = model["final"]
    final_actor = final[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]
    _, templates, fields = inventory_templates(original, final_actor, assets)
    fields["require_word_bars"] = True
    resources["final_detail"] = (templates, fields)
    resources["final_magic"] = magic_page(original, final_actor, assets)[0] if any(final_actor[0x1a:0x1f]) else None
    if scenario == "learn":
        resources["learned_lists"] = [magic_page(original, final_actor, assets, i)[0] for i in range(3)]
        resources["learned_upper"] = {key: templates[key] for key in ("actor_name", "current_max_mp")}
    resources["terrain"] = detail_close_terrain(original, final, assets)
    resources["wait_probe"] = resources["movement"] if scenario == "physical-overwrite" else selection_probe(original, final, raw, assets)
    resources["banner"] = round_two_template(original / "FDOTHER.DAT", assets["palette"])
    return resources


def command(frame, source, view, raw, selected, expected):
    detail = PlayerOracle(source, view, [], {}).command(frame, screen(source, tuple(source[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2])), selected)
    if detail is None or [item["frame"] for item in detail["matches"]] != expected:
        return None
    return dict(detail, expected_command_frames=expected, complete_resources_checked=True)


def list_state(frame, prepared, selected, *, learned=False):
    at, image = prepared["learned_lists" if learned else "magic_lists"][selected]
    checks = {"full_bottom": exact_tile(frame, image, at)}
    for key, (where, template) in prepared["learned_upper" if learned else "list_upper"].items():
        checks[key] = exact_tile(frame, template, where)
    return {"kind": "b4_magic_list", "selected": selected, "magic_ids": [0, 13, 17] if learned else [0, 13], "rectangles": checks} if all(checks.values()) else None


def target_state(frame, prepared):
    assets, probe = prepared["assets"], prepared["target_probe"]
    actor = focused_actor(frame, assets, prepared["target"])
    if actor is None: return None
    for phase, image in probe["ranges"]:
        found = exact_tile(frame, image, probe["position"])
        if found:
            return {"kind": "b4_action_target", "actor": actor, "range": found, "range_phase": phase,
                    "selection_frame": 1 if prepared["scenario"] in ("heal", "learn") else 0}
    return None


def navigate(capture, prepared, source, start, end, checkpoint, prefix):
    for index, (key, world, _) in enumerate(focus_steps(source, start, end), 1):
        resource = post_use_navigation_resources(prepared["original"], source, prepared["assets"], world)
        capture.key(key)
        checkpoint(f"{prefix}-{index}", lambda frame, r=resource: post_use_navigation_state(frame, source, prepared["assets"], r))
    return end


def resource_browse(prepared, source, world):
    asset = post_use_navigation_resources(prepared["original"], source, prepared["assets"], world)
    assets, at = prepared["assets"], screen(source, world)
    selected_actor = asset.get("actor")
    command_oracle = PlayerOracle(source, actor_view(prepared["original"], source, assets, selected_actor["raw_actor"]), [], {}) if selected_actor else None
    def match(frame):
        oracle = RoundOracle(assets, at)
        if (oracle.menu(frame) or oracle.buttons(frame) or match_indexed(frame, assets["dialog_top"], (5, 112))["ok"] or
                command_oracle is not None and command_oracle.command(frame, at)):
            return None
        return post_use_navigation_state(frame, source, assets, asset)
    return match


def quiet_browse(prepared, source, world, report, name):
    predicate = resource_browse(prepared, source, world)
    state = {"first": None, "samples": 0, "resets": 0, "wgc_retries": 0,
             "host_resource_stability_seconds": 2.0, "native_tick_timing_claimed": False}
    report[name + "_quiet"] = state
    def match(frame):
        retries = sum(row.get("kind") == "wgc_transient" and row["name"].startswith(name + "-wgc-retry-") for row in report["checkpoints"])
        result = predicate(frame)
        if not result or retries != state["wgc_retries"]:
            state.update(first=None, samples=0, resets=state["resets"] + 1, wgc_retries=retries)
            if result is None: return None
        now = time.monotonic()
        if state["first"] is None: state["first"] = now
        state["samples"] += 1
        return dict(result, resource_quiet=dict(state)) if now - state["first"] >= 2 and state["samples"] >= 2 else None
    return match


def observe_pages(capture, prepared, checkpoint, report):
    report["reward_acknowledgements"] = []
    for ordinal, page in enumerate(prepared["pages"]):
        name = f"b4-message-{ordinal:02d}-{page['fragment']}"
        _, detail = checkpoint(name, lambda frame, p=page: page_match(frame, p, prepared["assets"]), timeout=120)
        report["reward_acknowledgements"].append({"name": name, "fragment": page["fragment"],
            "requires_key": page["requires_key"], "key_sent": "ENTER" if page["requires_key"] else None})
        if page["requires_key"]:
            capture.key("ENTER")  # 只回应已完整证明的当前-3；没有skip/补键。


def end_turn(capture, prepared, checkpoint, report, observe):
    assets = prepared["assets"]
    oracle = RoundOracle(assets, screen(prepared["source"], prepared["end_world"]))
    capture.key("ENTER"); checkpoint("b4-end-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("DOWN"); checkpoint("b4-end-selected", lambda frame: oracle.menu(frame, 3))
    capture.key("ENTER"); checkpoint("b4-end-default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")
    if observe: observe_pages(capture, prepared, checkpoint, report)
    def banner(frame):
        found = match_indexed(frame, prepared["banner"], (120, 84))
        return dict(kind="round_banner", turn=2, **found) if found["ok"] else None
    if report["kind"] == "original" and prepared["scenario"] != "status":
        # status页发生在round002之后，不能回头要求已过去的banner。
        # 原版专属观察不进入双端共同检查点；不要求SDL补未实现的旧banner演出。
        _, report["native_round_banner"] = wait_state(capture, "b4-native-round002", banner, [], timeout=120)
    elif report["kind"] == "sdl":
        from field_play_input_test import wait_log
        text = wait_log(capture.process, capture.artifacts / "game.log", "field phase: turn=2 phase=2 ", timeout=120)
        check_round_phase_log(text)
        report["sdl_phase_log"] = {"ordered_single_end_1_to_0_to_new2": True,
                                   "banner_image_compared": False,
                                   "scope": "phase log plus subsequent real quiet browse and complete turn2 SAV"}
    checkpoint("b4-turn2-ready", quiet_browse(prepared, prepared["model"]["final"], (7, 14), report, "b4-turn2-ready"), timeout=90)


def show_detail(capture, prepared, checkpoint, report, *, prefix):
    source = prepared["model"]["final"]
    raw = prepared["model"]["reward_actor"]
    templates, fields = prepared["final_detail"]
    capture.key("F2")
    _, detail = checkpoint(prefix + "-detail", lambda frame: inventory_detail(frame, templates, fields))
    report[prefix + "_detail"] = detail
    if prepared["final_magic"]:
        at, image = prepared["final_magic"]
        capture.key("ESC")
        def magic(frame):
            found = exact_tile(frame, image, at)
            upper = exact_tile(frame, templates["level"][1], templates["level"][0])
            return {"kind": "reward_learned_magic_detail", "selected": -1,
                    "magic_ids": [8 * b + bit for b in range(5) for bit in range(8) if source[UNITS + raw * STRIDE + 0x1a + b] & (1 << bit)],
                    "bottom": found, "upper_level": upper} if found and upper else None
        checkpoint(prefix + "-magic", magic)
    capture.key("ESC")
    world = tuple(source[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2])
    predicate = resource_browse(prepared, source, world)
    def closed(frame):
        found = predicate(frame)
        terrain = prepared["terrain"]
        restored = exact_tile(frame, terrain["image"], terrain["position"])
        return dict(found, left_detail_fully_closed=restored) if found and restored else None
    checkpoint(prefix + "-closed", closed)
    return world


def save_measure(capture, prepared, save, guard, report, checkpoint, prefix):
    final, assets = prepared["model"]["final"], prepared["assets"]
    plan = {"expected_source": final, "steps": [("LEFT", screen(final, (6, 14)))]}
    measure_inventory_save(capture, assets, plan, save, guard[0], guard[1], report, checkpoint,
                           prefix=prefix, measurement_key=prefix + "_save")
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    saved = decode_save(save)
    raw = prepared["model"]["reward_actor"]
    at = UNITS + raw * STRIDE
    report[prefix + "_save"]["reward_actor_record_hex"] = saved[at:at + STRIDE].hex()
    return saved


def exercise(capture, prepared, save, guard, report):
    assets, source, scenario = prepared["assets"], prepared["source"], prepared["scenario"]
    def checkpoint(name, predicate, **kwargs):
        frame, detail = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        require_source_unchanged(save, guard[0], guard[1])
        return frame, detail
    # writer完成之后checkpoint不能仍拿写前bytes作守卫；measurement helper已作精确完成守卫。
    def save_checkpoint(name, predicate, *, source_guard=True, **kwargs):
        result = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        if source_guard: require_source_unchanged(save, guard[0], guard[1])
        return result
    wait_title(capture, prepared["titles"][3][0])
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); capture.key("ENTER")
    checkpoint("b4-continued", RoundOracle(assets, screen(source, prepared["initial_focus"])).initial_browse, timeout=45)
    if scenario in ("status", "counter-multi"):
        navigate(capture, prepared, source, prepared["initial_focus"], (6, 14), checkpoint, "b4-to-end")
        end_turn(capture, prepared, checkpoint, report, True)
    else:
        raw = prepared["model"]["reward_actor"]
        for actor in prepared["source_actors"][:raw + 1]:
            capture.key("Z"); checkpoint(f"b4-z-{actor['raw_actor']}", lambda frame, a=actor: focused_actor(frame, assets, a))
        capture.key("ENTER")
        checkpoint("b4-move-selection", lambda frame: selection_state(frame, assets, prepared["source_actors"][raw], prepared["movement"]))
        capture.key("ENTER")
        view = actor_view(prepared["original"], source, assets, raw)
        selected, expected = (1, [2, 4, 6, 9]) if raw == 1 else (0, [1, 5, 6, 9])
        checkpoint("b4-zero-move-command", lambda frame: command(frame, source, view, raw, selected, expected))
        capture.key("ENTER")
        if raw == 1:
            checkpoint("b4-magic-list0", lambda frame: list_state(frame, prepared, 0))
            capture.key("UP"); checkpoint("b4-magic13-selected", lambda frame: list_state(frame, prepared, 1))
            capture.key("ENTER")
        checkpoint("b4-target", lambda frame: target_state(frame, prepared))
        if scenario in ("physical-cap", "physical-overwrite"):
            for key in ("Z", "KP5"):
                capture.key(key)
                checkpoint("b4-target-cycle-" + key.lower(), lambda frame: target_state(frame, prepared))
            capture.key("ESC")
            checkpoint("b4-target-cancel-command", lambda frame: command(frame, source, view, raw, 0, [1, 5, 6, 9]))
            capture.key("ENTER")
            checkpoint("b4-target-reopened", lambda frame: target_state(frame, prepared))
            report["physical_target_controls"] = {"cycle_keys": ["Z", "KP5"],
                "target_cancel_count": 1, "command_reopen_count": 1,
                "native_and_sdl_same_key_sequence": True, "effect_confirmations": 1}
        capture.key("ENTER")  # 本场唯一效果提交；此后只逐已证-3 ACK。
        report["action_confirmations"] = 1
        observe_pages(capture, prepared, checkpoint, report)
        world = (7, 14) if raw == 1 else (8, 14)
        checkpoint("b4-reward-ready", quiet_browse(prepared, prepared["model"]["immediate"], world, report, "b4-reward-ready"), timeout=90)
        navigate(capture, prepared, prepared["model"]["immediate"], world, (6, 14), checkpoint, "b4-after-reward-to-end")
        end_turn(capture, prepared, checkpoint, report, False)
    final = prepared["model"]["final"]
    raw = prepared["model"]["reward_actor"]
    world = tuple(final[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2])
    navigate(capture, prepared, final, (7, 14), world, checkpoint, "b4-to-final-detail")
    show_detail(capture, prepared, checkpoint, report, prefix="b4-before-save")
    navigate(capture, prepared, final, world, (7, 14), checkpoint, "b4-return-leader")
    saved = save_measure(capture, prepared, save, guard, report, save_checkpoint, "b4-initial")
    # 真实WAIT改变acted（及有缺口的HP），然后Load回A；不能用只读同档冒充恢复。
    navigate(capture, prepared, final, (6, 14), world, checkpoint, "b4-to-wait")
    actor = actor_focus_resources(prepared["original"], final, assets, raw, assets["cursor"])
    capture.key("ENTER"); checkpoint("b4-wait-selection", lambda frame: selection_state(frame, assets, actor, prepared["wait_probe"]))
    capture.key("ENTER")
    view = actor_view(prepared["original"], final, assets, raw)
    checkpoint("b4-wait-command", lambda frame: PlayerOracle(final, view, [], {}).command(frame, screen(final, world)))
    if scenario == "learn":
        checkpoint("b4-learned-command-magic", lambda frame: command(frame, final, view, raw, 1, [2, 4, 6, 9]))
        capture.key("ENTER"); checkpoint("b4-learned-list0", lambda frame: list_state(frame, prepared, 0, learned=True))
        capture.key("UP"); checkpoint("b4-learned-list17", lambda frame: list_state(frame, prepared, 2, learned=True))
        if report["kind"] == "sdl":
            for key in ("ENTER", "SPACE"):
                capture.key(key)
                checkpoint("b4-unsupported17-refused-" + key.lower(), lambda frame: list_state(frame, prepared, 2, learned=True))
        report["learned_spell_support"] = {"shown_ids": [0, 13, 17], "known_0_13_remain_available": True,
            "native_spell17_confirmation_sent": False, "sdl_spell17_refused": report["kind"] == "sdl",
            "spell17_effect_verified": False}
        capture.key("DOWN"); checkpoint("b4-learned-wrap-list0", lambda frame: list_state(frame, prepared, 0, learned=True))
        capture.key("DOWN"); checkpoint("b4-learned-list13", lambda frame: list_state(frame, prepared, 1, learned=True))
        capture.key("ENTER"); checkpoint("b4-learned13-target", lambda frame: target_state(frame, prepared))
        capture.key("ESC"); checkpoint("b4-learned13-cancel-list0", lambda frame: list_state(frame, prepared, 0, learned=True))
        capture.key("ESC"); checkpoint("b4-learned-list-cancel-command", lambda frame: command(frame, final, view, raw, 1, [2, 4, 6, 9]))
    capture.key("DOWN")
    # 逐entry支持门：新学17不应剥夺现有0/13；未开放17自身仍不提交。
    magic_frame = 3 if raw == 1 else 5
    attack_frame = 0 if scenario == "physical-overwrite" else 2
    checkpoint("b4-wait-selected", lambda frame: command(frame, final, view, raw, 3, [attack_frame, magic_frame, 6, 10]))
    capture.key("ENTER")
    mutated = bytearray(final); mutated[UNITS + raw * STRIDE + 5] |= 0x80
    hp, maximum = struct.unpack_from("<HH", mutated, UNITS + raw * STRIDE + 0x40)
    struct.pack_into("<H", mutated, UNITS + raw * STRIDE + 0x40, min(maximum, hp + maximum // 5))
    checkpoint("b4-wait-mutated", quiet_browse(prepared, mutated, world, report, "b4-wait-mutated"), timeout=45)
    navigate(capture, prepared, mutated, world, (6, 14), checkpoint, "b4-to-load")
    oracle = RoundOracle(assets, screen(final, (6, 14)))
    capture.key("ENTER"); checkpoint("b4-load-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("ENTER"); checkpoint("b4-load-secondary", lambda frame: oracle.secondary(frame, 0, save_available=False))
    capture.key("RIGHT"); checkpoint("b4-load-selected", lambda frame: oracle.secondary(frame, 2, save_available=False))
    capture.key("ENTER"); checkpoint("b4-load-default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")
    checkpoint("b4-loaded-browse", resource_browse(prepared, final, (6, 14)), timeout=45)
    navigate(capture, prepared, final, (6, 14), world, checkpoint, "b4-to-loaded-detail")
    show_detail(capture, prepared, checkpoint, report, prefix="b4-after-load")
    navigate(capture, prepared, final, world, (7, 14), checkpoint, "b4-loaded-return-leader")
    second = save_measure(capture, prepared, save, guard, report, save_checkpoint, "b4-reloaded")
    if saved != second:
        raise AssertionError("B4 Save→真实WAIT→Load→Save全容器不一致")
    report["save_load"] = {"whole_plaintext_equal": True, "real_wait_before_load": True,
                           "both_snapshots_validated": True, "raw_global_experience_not_serialized_or_read": True}


def endpoint_guard(record, original, before, fixture, fixture_data, fixture_stamp, save, guard):
    completed = record.get("completed_save")
    snapshot_ok = not completed or (Path(completed["snapshot"]).is_file() and
        hashlib.sha256(Path(completed["snapshot"]).read_bytes()).hexdigest() == completed["sha256"])
    checks = {"original_unchanged": hashes(original) == before,
        "fixture_unchanged": fixture.read_bytes() == fixture_data and fixture.stat().st_mtime_ns == fixture_stamp,
        "copy_save_matches_completed_state": snapshot_ok and save.is_file() and save.read_bytes() == guard[0] and save.stat().st_mtime_ns == guard[1],
        "cleanup_passed": len(record["processes"]) == 1 and all(p["cleaned"] and not p["forced_cleanup"] and p["exit_code"] == 0 for p in record["processes"])}
    for key, value in checks.items(): record[key] = record.get(key, True) and value
    record["probe_cases_passed"] = record["probe_cases_passed"] and all(record[key] for key in checks)
    if not record["probe_cases_passed"]:
        record["status"] = "failed"
        record.setdefault("error", "B4端点最终来源/存档bytes+mtime/清理守卫失败")


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    if output == original or original in output.parents:
        raise ValueError("B4产物不能写入original_game或其解析后的子目录")
    if args.scenario not in SCENARIOS: raise ValueError("未知B4场景")
    if args.sdl is not None and not args.sdl.is_file(): raise ValueError("--sdl程序不存在；未创建产物或启动窗口")
    prepared = prepare(original, args.scenario)
    output.mkdir(parents=True, exist_ok=False)
    before = hashes(original)
    fixture = original / "START.SAV"
    fixture_data, fixture_stamp = fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "probe_only": args.sdl is None, "probe_cases_passed": False, "status": "failed",
        "scenario": args.scenario, "scope": "controlled native B4 experience/growth/learning/status; resource ACK, complete detail bars, legitimate Save/WAIT/Load/Save",
        "excluded": "natural progression, entire FIGANI/timing/audio, shared RNG alignment, raw unpersisted contribution memory",
        "capture_backend": "Windows.Graphics.Capture HWND", "derived_fixture": prepared["manifest"],
        "tables": prepared["tables"], "original_before": before, "cases": [],
        "binaries": {key: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                     for key, path in (("driver", args.driver), ("dosbox", args.dosbox)) + ((("sdl", args.sdl),) if args.sdl is not None else ())}}
    report["tool_sources"] = {name: {"path": str(Path(sys.modules[name].__file__).resolve()),
        "sha256": hashlib.sha256(Path(sys.modules[name].__file__).read_bytes()).hexdigest()}
        for name in ("fd2_reward_oracle", "fd2_item_list_oracle", "run_original_item_list", "run_original_magic_list", "run_original_comparison", "run_original_end_turn")}
    report["tool_sources"]["run_original_reward"] = {"path": str(Path(__file__).resolve()),
        "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (output / "fixture-manifest.json").write_text(json.dumps(prepared["manifest"], ensure_ascii=False, indent=2), encoding="utf-8")
    controlled_path = output / "controlled.SAV"
    controlled_path.write_bytes(prepared["encoded"])
    if decode_save(controlled_path) != prepared["source"]:
        raise AssertionError("B4 exact manifest XOR/checksum往返不符；未启动窗口")
    try:
        for kind in (("original",) if args.sdl is None else ("original", "sdl")):
            work = output / kind; work.mkdir()
            env = os.environ.copy()
            for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"): env.pop(name, None)
            if kind == "original":
                game_copy = work / "game"; shutil.copytree(original, game_copy)
                save = game_copy / "FD2.SAV"
                config = work / "dosbox.conf"
                config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
                    f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                command_line, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"; env["FD2SDL_SAVE_PATH"] = str(save)
                command_line, cwd = [str(args.sdl.resolve())], root
            save.write_bytes(prepared["encoded"])
            guard = [prepared["encoded"], save.stat().st_mtime_ns]
            record = {"kind": kind, "status": "failed", "probe_cases_passed": False,
                      "probe_only": True, "endpoint_only": True, "ok": False,
                      "checkpoints": [], "processes": [], "inputs": [], "initial_save_mtime_ns": guard[1],
                      "binaries": report["binaries"], "tool_sources": report["tool_sources"],
                      "runtime_fixture_sha256": hashlib.sha256(prepared["encoded"]).hexdigest(),
                      "scope": report["scope"], "excluded": report["excluded"]}
            report["cases"].append(record)
            try:
                with running(command_line, cwd, env, work / "game.log", args.driver.resolve(), record["processes"]) as process:
                    capture = Capture(args.driver.resolve(), process, work, record["inputs"])
                    exercise(capture, prepared, save, guard, record)
                    require_source_unchanged(save, *guard)
                record["status"], record["probe_cases_passed"] = "passed", True
            except Exception as error:
                record.setdefault("error", str(error))
            finally:
                endpoint_guard(record, original, before, fixture, fixture_data, fixture_stamp, save, guard)
                record["capture_hashes"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in work.iterdir()
                                            if p.is_file() and p.suffix.lower() in (".png", ".bmp", ".sav")}
                (work / "comparison.json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
            if not record["probe_cases_passed"]: raise AssertionError(record["error"])
        if args.sdl is not None:
            report["paired"] = compare_endpoints(report["cases"], output, prepared)
        report["status"], report["probe_cases_passed"] = "passed", True
    except Exception as error:
        report.setdefault("error", str(error))
    finally:
        report["original_unchanged"] = before == hashes(original)
        report["fixture_unchanged"] = fixture.read_bytes() == fixture_data and fixture.stat().st_mtime_ns == fixture_stamp
        # 每端最终守卫已在端结束时落盘；此后再核其实际文件，不能被末端成功遮蔽。
        for record in report["cases"]:
            work = output / record["kind"]
            save = work / "game" / "FD2.SAV" if record["kind"] == "original" else work / "FD2.SAV"
            completed = record.get("completed_save")
            expected = Path(completed["snapshot"]).read_bytes() if completed else prepared["encoded"]
            stamp = completed["mtime_ns"] if completed else record.get("initial_save_mtime_ns", -1)
            endpoint_guard(record, original, before, fixture, fixture_data, fixture_stamp, save, [expected, stamp])
            (work / "comparison.json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
        report["probe_cases_passed"] = report["probe_cases_passed"] and all(record["probe_cases_passed"] for record in report["cases"]) and report["original_unchanged"] and report["fixture_unchanged"]
        report["ok"] = args.sdl is not None and report["probe_cases_passed"] and len(report["cases"]) == 2
        if not report["probe_cases_passed"]:
            report["status"] = "failed"
            report.setdefault("error", next((case["error"] for case in report["cases"] if "error" in case), "B4端点未全部完成或最终守卫失败"))
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def compare_endpoints(cases, output, prepared):
    if [case["kind"] for case in cases] != ["original", "sdl"] or any(not case["probe_cases_passed"] for case in cases):
        raise AssertionError("B4配对必须两个真实端点均已通过")
    endpoints = [paired_checkpoint_records(case, output / case["kind"])[0] for case in cases]
    role_only = {"b4-unsupported17-refused-enter", "b4-unsupported17-refused-space"} if prepared["scenario"] == "learn" else set()
    if role_only:
        if any(name in endpoints[0] for name in role_only) or not role_only.issubset(endpoints[1]):
            raise AssertionError("B4未开放17的端内拒绝收据不完整或错误落入原版")
        for name in role_only:
            row = endpoints[1][name]
            if row.get("kind") != "b4_magic_list" or row.get("selected") != 2 or row.get("magic_ids") != [0, 13, 17]:
                raise AssertionError("B4未开放17的端内收据不是完整selected17列表")
        endpoints[1] = {name: row for name, row in endpoints[1].items() if name not in role_only}
    if endpoints[0].keys() != endpoints[1].keys(): raise AssertionError("B4业务检查点集合不一致")
    results = []
    for page in prepared["pages"]:
        name = f"b4-message-{page['ordinal']:02d}-{page['fragment']}"
        frames = [Image.open(output / case["kind"] / (name + ".png")).convert("RGB") for case in cases]
        difference = rgb_difference(*frames, (0, 0), (320, 200), page["text"])
        if not difference["within_tolerance"]: raise AssertionError("B4奖励文字实际RGB跨端差异超过3")
        results.append({"name": name, **difference})
    for name in ("b4-before-save-detail", "b4-after-load-detail"):
        rows = [endpoint[name] for endpoint in endpoints]
        geometry = [[(row["name"], row["position"], row["size"]) for row in result["rectangles"]] for result in rows]
        if geometry[0] != geometry[1]: raise AssertionError("B4详情配对矩形不一致")
        frames = [Image.open(output / case["kind"] / (name + ".png")).convert("RGB") for case in cases]
        for label, position, size in geometry[0]:
            difference = rgb_difference(*frames, tuple(position), tuple(size))
            if not difference["within_tolerance"]: raise AssertionError("B4详情实际RGB跨端差异超过3：" + label)
            results.append({"name": name + ":" + label, **difference})
    static_lists = [name for name, row in endpoints[0].items() if row["kind"] == "b4_magic_list"]
    if prepared["final_magic"]:
        static_lists += ["b4-before-save-magic", "b4-after-load-magic"]
    for name in static_lists:
        frames = [Image.open(output / case["kind"] / (name + ".png")).convert("RGB") for case in cases]
        difference = rgb_difference(*frames, (5, 94), (310, 99))
        if not difference["within_tolerance"]: raise AssertionError("B4列表完整下板实际RGB跨端差异超过3")
        results.append({"name": name, **difference})
    snapshots = [Path(case["completed_save"]["snapshot"]).read_bytes() for case in cases]
    if snapshots[0] != snapshots[1]: raise AssertionError("B4两端最终真实SAV全部byte不同")
    return {"resource_text_rgb": results, "whole_save_bytes_equal": True,
            "same_complete_resource_checkpoints": len(endpoints[0]), "shared_rng_aligned": False,
            "explicit_sdl_only_unsupported17_refusals": sorted(role_only),
            "portrait_timing_full_figani_and_audio_compared": False,
            "phase_anchor_policy": "native-only round002 resource when observed before expiry pages; SDL ordered phase log, no SDL banner image claim; both real quiet browse and full turn2 SAV",
            "save_sha256": hashlib.sha256(snapshots[0]).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "driver", "dosbox", "artifacts"): parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--sdl", type=Path)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    args = parser.parse_args()
    if sys.platform != "win32": parser.error("B4原版对照需要Windows WGC；未启动窗口")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"B4对照无法启动：{error}。检查前置条件并使用新产物目录。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
