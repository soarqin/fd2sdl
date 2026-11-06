#!/usr/bin/env python3
"""B7 stage1实战／分支→stage2准备、真实入场与存取，原版先行且只读来源。

覆盖自然整关、三种受控胜利、三箱、村民败北和stage2存取共九种场景。
依据22f37/1c220/11506及32e8c，不把controlled胜利当自然stage1实战。
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fd2_stage1_complete_oracle as oracle
import fd2_natural_oracle as natural
from fd2_opening_oracle import page_match
from fd2_transition_oracle import (picker_image, picker_match,
    notice_resources, notice_match, SLOT_BASE, ROSTER_SIZE)
from run_graphics_tests import decode_save
from run_original_battle_menu import wait_state, wait_title, match_indexed, MENU_OFFSETS, PARENT_IDS
from run_original_comparison import Capture, archive_entry, running, title_templates
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets, browse_anywhere
from run_original_load import require_source_unchanged, wait_measured_save
from run_original_player import screen
from run_original_player import exact_tile
from run_original_transition import town_match

SUPPORTED = ("victory-all-villagers", "victory-villager-loss", "victory-full-inventory")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fact(path):
    path = Path(path)
    return {"path": str(path.resolve()), "sha256": sha(path), "size": path.stat().st_size,
        "mtime_ns": path.stat().st_mtime_ns}


def same(row):
    try:
        path = Path(row["path"])
        return path.is_file() and fact(path) == row
    except OSError:
        return False


def fingerprint(folder):
    return {str(p.relative_to(folder)): fact(p) for p in sorted(Path(folder).rglob("*")) if p.is_file()}


def folder_unchanged(folder, before):
    try: return fingerprint(folder) == before
    except OSError: return False


def snapshots_unchanged(case):
    try:
        required = all(key in case and Path(case[key]["snapshot"]).is_file() and
            sha(case[key]["snapshot"]) == case[key]["sha256"] for key in
            case.get("required_snapshots", ("manual_save", "completed_save"))) and (
            not case.get("result_capture_required") or bool(case.get("result_capture")) and
            same(case["result_capture"]))
        snapshots = case.get("manual_saves", []) + case.get("active_saves", []) + case.get("actual_turn_saves", [])
        snapshots += [{"snapshot": row["plan"], "sha256": row["sha256"]} for row in case.get("natural_actions", [])]
        # capture_hashes在本端正常关闭后封存；此处只补PNG的后继/最终守卫，
        # 不声称在各检查点创建时已封存，也不把可覆写的current.bmp当过程真值。
        snapshots += [{"snapshot": str(Path(case["artifacts"]) / name), "sha256": digest}
            for name, digest in case.get("capture_hashes", {}).items() if Path(name).suffix.lower() == ".png"]
        return required and all(Path(row["snapshot"]).is_file() and sha(row["snapshot"]) == row["sha256"] for row in snapshots)
    except (OSError, KeyError, TypeError, ValueError): return False


def owned_job_required():
    if sys.platform != "win32": raise RuntimeError("B7实际对照需Windows HWND WGC与owned Job")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.IsProcessInJob.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    present = ctypes.c_int()
    if not kernel.IsProcessInJob(kernel.GetCurrentProcess(), None, ctypes.byref(present)) or not present.value:
        raise RuntimeError("请用driver run-job管理本轮整树；尚未创建输出或窗口")


def source_origin(report_path, fixture):
    report_path, fixture = Path(report_path), Path(fixture)
    record = json.loads(report_path.read_text(encoding="utf-8"))
    completed = record.get("completed_save", {})
    if (record.get("kind") != "original" or record.get("status") != "passed" or
            record.get("probe_cases_passed") is not True or
            completed.get("sha256") != sha(fixture) or
            Path(completed.get("snapshot", "")).resolve() != fixture.resolve()):
        raise ValueError("受控起点需已有原版端真实turn4 completed Save，不接受模拟/模板")
    if not record.get("processes") or any(p.get("exit_code") != 0 or p.get("cleaned") is not True or p.get("forced_cleanup") for p in record["processes"]):
        raise ValueError("来源原版端未正常清理")
    if any(record.get(key) is not True for key in ("original_unchanged", "fixture_unchanged", "copy_save_matches_completed_state", "cleanup_passed", "original_all_mtimes_unchanged")):
        raise ValueError("来源原版端缺必需守卫")
    if len(record.get("story_acknowledgements", [])) != 9:
        raise ValueError("受控结算起点缺原script13完整九页实际ACK")
    return {"fixture": fact(fixture), "report": fact(report_path),
        "original_runtime_source": True, "inherited_runtime_as_B7_pass": False}


def prepare(original, fixture, scenario, report_path):
    if not Path(fixture).is_file() or not Path(report_path).is_file():
        raise ValueError("B7缺真实只读SAV/来源报告，未创建产物或窗口")
    if scenario == "natural-stage1":
        from fd2_stage1_natural import prepare as natural_prepare
        return natural_prepare(original, fixture, scenario, report_path)
    if scenario == "stage2-storage":
        from fd2_stage2_storage import prepare as storage_prepare
        return storage_prepare(original, fixture, scenario, report_path)
    if scenario in ("villager-defeat", "chest-129", "chest-201", "chest-193"):
        from fd2_stage1_branches import prepare as branch_prepare
        return branch_prepare(original, fixture, scenario, report_path)
    return prepare_controlled(original, fixture, scenario, report_path)


def prepare_controlled(original, fixture, scenario, report_path):
    if scenario not in SUPPORTED:
        raise ValueError("B7此入口当前仅可执行三种controlled胜利；其它场景尚未实装，不创建输出")
    origin = source_origin(report_path, fixture)
    encoded, source, manifest = oracle.derive_fixture(original, fixture, scenario)
    data = oracle.tables(original)
    state, masks, reward = oracle.settlement_projection(source, data)
    assets = round_assets(original)
    town = oracle.stage2_town(original, data, state)
    pages = oracle.settlement_pages(original, assets, source)
    entry = oracle.stage2_projection(state, data)
    entry_pages = oracle.stage2_pages(original, assets, state, data)
    selected = natural.navigation(source, oracle.actor(source, 0)[:2])
    selected_view = selected[-1][1] if selected else source
    summary = natural.selection_summary(original, selected_view, assets, 0)
    probes = natural.range_probe(original, selected_view, assets, 0)
    if len(pages) != 30 or len(entry_pages) != 15:
        raise ValueError("B7结算30页/stage2入场15页原tokens合同改变，不能猜ACK数")
    notices = {fragment: notice_resources(original, town["assets"], fragment) for fragment in (585, 586, 660)}
    notices[513] = notice_resources(original, town["assets"], 513, portrait=75, origin=(95, 119))
    for selected in range(4): picker_image(town["assets"], source, selected)
    return {"original": original, "scenario": scenario, "source": source, "encoded": encoded,
        "manifest": manifest, "origin": origin, "data": data, "expected_preparation": state,
        "roster_masks": masks, "reward": reward, "assets": assets, "town": town, "pages": pages,
        "entry_pages": entry_pages, "entry": entry, "selected_view": selected_view,
        "selection_summary": summary, "selection_probes": probes, "notices": notices,
        "titles": title_templates(original / "FDOTHER.DAT")}


def entry_browse(frame, prepared):
    found = browse_anywhere(frame, prepared["assets"], leader=True)
    if not found: return None
    at = found["focus_screen"]; world = prepared["entry"]["first_focus_world"]
    camera = [world[i] - (at[i] - 4) // 24 for i in range(2)]
    if camera != prepared["entry"]["camera"] or not natural.dialogs_absent(frame, prepared["assets"]): return None
    return dict(found, kind="stage2_initial_browse", inferred_camera=camera, world=world)


def stage2_parent(frame, menu, role):
    """仅stage2存储入口：原版March/END仍正常，SDL明确禁用；四图均核。"""
    matches = []
    for index, command in enumerate(PARENT_IDS):
        position = tuple(a + b for a, b in zip(menu.focus, MENU_OFFSETS[index]))
        states = (0, 1) if index == 0 else (2,) if role == "sdl" and index in (1, 3) else (0,)
        found = menu.choices(frame, [command * 3 + state for state in states], position)
        if found is None: return None
        matches.append(found)
    selected = 0 if matches[0]["frame"] == 22 else None
    return {"kind": "stage2_parent_capability_resources", "matches": matches, "selected": selected,
        "default_selection_observed": selected == 0, "March_END_unavailable_on_SDL": role == "sdl"}


def hotel_menu_state(frame, assets, selected):
    """26eda/27079的phase/2可熄灭选中亮帧；不从全normal推断selection。"""
    if not 0 <= selected < 4: raise ValueError("B7酒店菜单索引无效")
    matches = []
    for index in range(4):
        states = (0, 1) if index == selected else (0,)
        at = (201 + 26 * index, 169)
        # prepare assets的commands为13[3..10]，不是战场command*3目录。
        found = None
        for state in states:
            hit = match_indexed(frame, assets["commands"][2 * index + state], at)
            if hit["ok"]:
                found = {"frame": 3 + 2 * index + state, "position": list(at), **hit}
                break
        if found is None: return None
        matches.append(found)
    background = exact_tile(frame, assets["hotel_background"].crop((220, 8, 316, 88)), (220, 8))
    if not background: return None
    observed = matches[selected]["frame"] == 4 + 2 * selected
    return {"kind": "hotel_menu", "expected_selection": selected,
        "selected": selected if observed else None, "selection_observed": observed,
        "blink_off": not observed, "matches": matches, "background_roi": [220, 8, 96, 80],
        "next_action_resource_required": True}


def exercise(capture, prepared, save, guard, record):
    if prepared["scenario"] == "natural-stage1":
        from fd2_stage1_natural import exercise as natural_exercise
        return natural_exercise(capture, prepared, save, guard, record)
    if prepared["scenario"] == "stage2-storage":
        from fd2_stage2_storage import exercise as storage_exercise
        return storage_exercise(capture, prepared, save, guard, record)
    if prepared["scenario"] not in SUPPORTED:
        from fd2_stage1_branches import exercise as branch_exercise
        return branch_exercise(capture, prepared, save, guard, record)
    return exercise_controlled(capture, prepared, save, guard, record)


def exercise_controlled(capture, prepared, save, guard, record):
    original, assets = prepared["original"], prepared["assets"]
    source = prepared["source"]
    def cp(name, predicate, **kwargs):
        result = wait_state(capture, name, predicate, record["checkpoints"], **kwargs)
        require_source_unchanged(save, *guard)
        return result
    def first_ending(frame):
        found = page_match(frame, prepared["pages"][0], assets)
        return dict(found, kind="b7_first_ending") if found else None
    wait_title(capture, prepared["titles"][3][0], timeout=90)
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); capture.key("ENTER")
    _, seen = cp("b7-continue-or-ending", lambda frame: first_ending(frame) or natural.focus_match(frame, original, source, assets), timeout=90)
    if seen["kind"] != "b7_first_ending":
        selected = prepared["selected_view"]
        def selection(frame):
            focus = natural.focus_match(frame, original, selected, assets, summary=prepared["selection_summary"])
            probe = natural.range_match(frame, prepared["selection_probes"])
            return dict(focus, range=probe) if focus and probe else None
        for key, name, predicate in (("Z", "b7-hero-focus", lambda frame: natural.focus_match(frame, original, selected, assets)),
                ("ENTER", "b7-selection", selection), ("ESC", "b7-result-trigger", first_ending)):
            capture.key(key)
            _, seen = cp(name, lambda frame, p=predicate: first_ending(frame) or p(frame), timeout=90)
            if seen["kind"] == "b7_first_ending": break
    record["settlement_acknowledgements"] = []
    for index, page in enumerate(prepared["pages"]):
        cp(f"b7-settlement-{index:02d}", lambda frame, p=page: page_match(frame, p, assets), timeout=90)
        record["settlement_acknowledgements"].append({"fragment": page["fragment"], "offset": page["token_offset"], "portrait": page["portrait_id"]})
        capture.key("ENTER")
    finish_stage2(capture, prepared, save, guard, record)


def finish_stage2(capture, prepared, save, guard, record):
    """已完整结算剧情后复用酒店/部署/活动保存；不重进标题或重放剧情。"""
    original, assets = prepared["original"], prepared["assets"]
    def cp(name, predicate, **kwargs):
        result = wait_state(capture, name, predicate, record["checkpoints"], **kwargs)
        require_source_unchanged(save, *guard)
        return result
    town = prepared["town"]; ui = town["assets"]
    cp("b7-stage2-town0", lambda frame: town_match(frame, town, 0), timeout=90)
    def hotel(selected, fragment):
        def match(frame):
            menu = hotel_menu_state(frame, ui, selected)
            notice = notice_match(frame, prepared["notices"][fragment], assets)
            return dict(menu, notice=notice) if menu and notice else None
        return match
    capture.key("ENTER"); cp("b7-hotel", hotel(0, 585))
    capture.key("RIGHT"); cp("b7-hotel-save", hotel(1, 585)); capture.key("ENTER")
    before = decode_save(save)
    cp("b7-manual-picker0", lambda frame: picker_match(frame, picker_image(ui, before, 0)), allow_low_color_resource=True)
    record["manual_saves"] = []
    for slot in (range(4) if prepared.get("exercise_storage") else (0,)):
        if slot:
            capture.key("DOWN")
            cp(f"b7-manual-picker{slot}", lambda frame, i=slot: picker_match(frame, picker_image(ui, before, i)), allow_low_color_resource=True)
        capture.key("ENTER"); measured = wait_measured_save(save, *guard)
        if slot:
            state, check = oracle.check_stage2_manual_save(before, measured, state, [bytes([1] * 80)] * 6, slot)
        elif "expected_preparation" in prepared:
            state, check = oracle.check_stage2_manual_save(before, measured, prepared["expected_preparation"], prepared["roster_masks"])
        else:
            state, check = oracle.check_natural_stage2_manual_save(before, measured, prepared["source"], prepared["data"])
        guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
        snapshot = capture.artifacts / f"b7-stage2-manual{slot}.SAV"; snapshot.write_bytes(guard[0])
        row = {"slot": slot, "snapshot": str(snapshot), "sha256": sha(snapshot), "measurement": check}
        record["manual_saves"].append(row)
        if slot == 0: record["manual_save"] = row
        cp("b7-save660" if slot == 0 else f"b7-save{slot}-660", lambda frame: notice_match(frame, prepared["notices"][660], assets))
        capture.key("ENTER")
        cp(f"b7-saved-picker{slot}", lambda frame, i=slot: picker_match(frame, picker_image(ui, measured, i)), allow_low_color_resource=True)
        before = measured
    capture.key("ESC"); cp("b7-save-cancelled", hotel(1, 586))
    if prepared.get("exercise_storage"):
        capture.key("RIGHT"); cp("b7-hotel-load2", hotel(2, 586)); capture.key("ENTER")
        cp("b7-load-picker0", lambda frame: picker_match(frame, picker_image(ui, measured, 0)), allow_low_color_resource=True)
        for slot in (1, 2, 3):
            capture.key("DOWN")
            cp(f"b7-load-picker{slot}", lambda frame, i=slot: picker_match(frame, picker_image(ui, measured, i)), allow_low_color_resource=True)
        capture.key("ENTER"); cp("b7-load478", lambda frame: notice_match(frame, prepared["notices"][478], assets))
        capture.key("ENTER"); cp("b7-load-returned-hotel", hotel(2, 586))
        record["manual_load"] = {"slot": 3, "file_bytes_mtime_unchanged": True, "success478_then586_resource_checked": True}
    capture.key("ESC")
    cp("b7-hotel-to-town", lambda frame: town_match(frame, town, 0))
    for selected in (1, 2):
        capture.key("LEFT"); cp(f"b7-town-{selected}", lambda frame, i=selected: town_match(frame, town, i))
    buttons = RoundOracle(assets, (0, 0))
    def depart(frame):
        notice = notice_match(frame, prepared["notices"][513], assets)
        choice = buttons.buttons(frame, 0)
        return dict(choice, notice=notice) if notice and choice else None
    capture.key("ENTER"); cp("b7-depart-yes", depart); capture.key("ENTER")
    record["stage2_entry_acknowledgements"] = []
    for index, page in enumerate(prepared["entry_pages"]):
        cp(f"b7-entry-{index:02d}", lambda frame, p=page: page_match(frame, p, assets), timeout=90)
        record["stage2_entry_acknowledgements"].append({"fragment": page["fragment"], "offset": page["token_offset"], "portrait": page["portrait_id"]})
        capture.key("ENTER")
    cp("b7-stage2-first-browse", lambda frame: entry_browse(frame, prepared), timeout=90)
    projection = oracle.stage2_projection(state, prepared["data"])
    view = bytearray(measured)
    camera, hero = projection["camera"], projection["first_focus_world"]
    view[:1171] = prepared["data"]["stages"][2]["metadata"]
    view[0x30a3:0x30c3] = bytes(32)
    view[META:META + 10] = bytes((1, 15, 2, *camera, *hero, hero[0] - camera[0], hero[1] - camera[1], 6))
    import struct
    struct.pack_into("<I", view, META + 10, state["resource"])
    for raw, unit in enumerate(projection["units"]): view[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE] = unit
    view[0x8a3:0x12a3] = state["roster"]
    if prepared.get("exercise_storage"):
        from fd2_stage2_storage import details
        details(capture, prepared, view, cp)
    capture.key("LEFT")
    left = natural.view(view, (6, 22), camera)
    cp("b7-stage2-save-empty", lambda frame: natural.focus_match(frame, original, left, assets))
    menu = RoundOracle(assets, screen(left, (6, 22)))
    if prepared.get("exercise_storage"):
        from fd2_stage2_storage import overview
        overview(capture, prepared, left, menu, record, cp)
    capture.key("ENTER"); cp("b7-stage2-parent", lambda frame: stage2_parent(frame, menu, record["kind"]))
    capture.key("ENTER"); cp("b7-stage2-secondary", lambda frame: menu.secondary(frame, 0))
    capture.key("LEFT"); cp("b7-stage2-save", lambda frame: menu.secondary(frame, 1))
    capture.key("ENTER"); cp("b7-stage2-save-yes", lambda frame: menu.buttons(frame, 0))
    capture.key("ENTER"); active = wait_measured_save(save, *guard)
    check = oracle.check_stage2_active_save(measured, active, state, projection, left, prepared["data"])
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    snapshot = capture.artifacts / "b7-stage2-entry.SAV"; snapshot.write_bytes(guard[0])
    record["completed_save"] = {"snapshot": str(snapshot), "sha256": sha(snapshot), "mtime_ns": guard[1], "measurement": check}
    cp("b7-stage2-saved-browse", lambda frame: natural.focus_match(frame, original, left, assets))
    if prepared.get("exercise_storage"):
        from fd2_stage2_storage import active_load
        active_load(capture, prepared, save, guard, state, projection, active, record, cp)
    record["business_sequence_completed"] = True


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve(); original = (root / "original_game").resolve()
    fixture, source_report = args.save_fixture.resolve(), args.source_report.resolve()
    if output == original or original in output.parents or any(output == p.parent or p.parent in output.parents for p in (fixture, source_report)):
        raise ValueError("B7输出不得写入原版或只读来源证据目录")
    public_app = getattr(args, "public_app", None)
    planner = getattr(args, "planner", None)
    max_turns = getattr(args, "max_turns", 60)
    if args.scenario == "natural-stage1" and (planner is None or not planner.is_file() or not 1 <= max_turns <= 100):
        raise ValueError("B7 natural-stage1需实际planner与有界max-turns1..100；未创建产物")
    binary_paths = ((args.driver, args.dosbox) + ((args.sdl,) if args.sdl else ()) +
        ((public_app,) if public_app else ()) + ((planner,) if args.scenario == "natural-stage1" else ()))
    for path in (fixture, source_report) + binary_paths:
        if not path.is_file(): raise ValueError("B7必需文件不存在，未创建输出或启动窗口")
    prepared = prepare(original, fixture, args.scenario, source_report)
    if args.scenario == "natural-stage1": prepared.update(planner=planner.resolve(), root=root, max_turns=max_turns)
    owned_job_required()
    before = fingerprint(original)
    binaries = [fact(path) for path in binary_paths]
    tools = [fact(Path(m.__file__)) for m in list(sys.modules.values()) if getattr(m, "__file__", None)
        and Path(m.__file__).suffix == ".py" and any(folder in Path(m.__file__).resolve().parents for folder in (root / "tools", root / "src/tests"))]
    origins = [fact(fixture), fact(source_report)]
    origin = prepared["origin"]
    origin_pages = (origin.get("entry_captures", []) + origin.get("stage2_entry_captures", []) +
        origin.get("previous_native_entry_provenance", {}).get("entry_captures", []))
    for page in origin_pages:
        sealed = fact(page["path"])
        if sealed["sha256"] != page["sha256"]:
            raise ValueError("B7已登记来源剧情PNG在前置校验后漂移；不创建产物或窗口")
        origins.append(sealed)
    output.mkdir(parents=True, exist_ok=False)
    (output / "fixture-manifest.json").write_text(json.dumps(prepared["manifest"], ensure_ascii=False, indent=2), encoding="utf-8")
    report = {"ok": False, "status": "failed", "probe_only": args.sdl is None, "scenario": args.scenario,
        "controlled_fixture": args.scenario != "natural-stage1", "natural_stage1_battle": args.scenario == "natural-stage1", "cases": [], "original_before": before,
        "source_origin": prepared["origin"], "source_evidence_files": origins, "tool_sources": tools, "binaries": binaries,
        "pending": ["stage2 Load/extra storage and auxiliary protocols", "complete audio/FX/RNG alignment", "stage2 whole battle"]}
    try:
        for kind in (("original",) if args.sdl is None else ("original", "sdl")):
            if not all(snapshots_unchanged(case) for case in report["cases"]):
                raise AssertionError("B7已完成端的实存/计划/封存PNG发生漂移，不启动后继端")
            work = output / kind; work.mkdir()
            env = os.environ.copy()
            for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER", "FD2SDL_FIELD_RNG_SEED"): env.pop(name, None)
            if kind == "original":
                game = work / "game"; shutil.copytree(original, game); save = game / "FD2.SAV"
                config = work / "dosbox.conf"
                config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "' + str(game) + '"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"; env["FD2SDL_SAVE_PATH"] = str(save)
                command, cwd = [str(args.sdl.resolve())], root
            save.write_bytes(prepared["encoded"]); guard = [save.read_bytes(), save.stat().st_mtime_ns]
            case = {"kind": kind, "artifacts": str(work), "status": "failed", "checkpoints": [], "inputs": [], "processes": [], "business_sequence_completed": False}
            case["required_snapshots"] = prepared.get("required_snapshots", ("manual_save", "completed_save"))
            case["result_capture_required"] = prepared.get("result_capture_required", False)
            report["cases"].append(case)
            isolated = fingerprint(game) if kind == "original" else None
            try:
                with running(command, cwd, env, work / "game.log", args.driver.resolve(), case["processes"]) as process:
                    exercise(Capture(args.driver.resolve(), process, work, case["inputs"]), prepared, save, guard, case)
                    require_source_unchanged(save, *guard)
                case["status"] = "passed"
            except Exception as error: case["error"] = str(error)
            finally:
                try:
                    checks = {"source_fixture_and_report_unchanged": all(same(row) for row in origins),
                        "original_bytes_size_mtime_unchanged": folder_unchanged(original, before),
                        "tools_and_binaries_unchanged": all(same(row) for row in tools + binaries),
                        "save_bytes_mtime_expected": save.read_bytes() == guard[0] and save.stat().st_mtime_ns == guard[1],
                        "owned_cleanup": len(case["processes"]) == 1 and all(p.get("cleaned") and not p.get("forced_cleanup") and p.get("exit_code") == 0 for p in case["processes"]),
                        "business_complete": case.get("business_sequence_completed") is True and all(
                            len(case.get(name, [])) == count for name, count in prepared.get("expected_acknowledgements",
                                {"settlement_acknowledgements": 30, "stage2_entry_acknowledgements": 15}).items()),
                        "snapshot_files_unchanged": snapshots_unchanged(case)}
                    if isolated is not None:
                        current = fingerprint(game); mutable = {"FD2.SAV", "FD2.TMP"}
                        checks["isolated_static_resources_unchanged"] = all(current.get(name) == row for name, row in isolated.items() if name not in mutable) and not set(current) - set(isolated) - mutable
                except OSError as error:
                    checks = {"guard_read_error": False}; case.setdefault("error", str(error))
                case["guards"] = checks
                if not all(checks.values()): case["status"] = "failed"; case.setdefault("error", "B7晚守卫或owned清理失败")
                case["capture_hashes"] = {str(p.relative_to(work)): sha(p) for p in work.rglob("*") if p.is_file() and p.suffix.lower() in (".png", ".bmp", ".sav")}
                case["capture_hash_guard_scope"] = "PNG sealed after endpoint close, checked before successor and at final; not creation-time immutable or BMP late validation"
                (work / "comparison.json").write_text(json.dumps(case, ensure_ascii=False, indent=2), encoding="utf-8")
            if case["status"] != "passed": raise AssertionError(case["error"])
        report["probe_cases_passed"] = True
        report["ok"] = args.sdl is not None
        report["status"] = "passed" if report["ok"] else "original_calibrated"
    except Exception as error: report["error"] = str(error)
    finally:
        report["final_guards"] = {"original_unchanged": folder_unchanged(original, before),
            "origins_unchanged": all(same(row) for row in origins), "tools_and_binaries_unchanged": all(same(row) for row in tools + binaries),
            "snapshot_files_unchanged": all(snapshots_unchanged(case) for case in report["cases"])}
        if not all(report["final_guards"].values()): report["ok"] = False; report["status"] = "failed"; report.setdefault("error", "B7最终来源漂移")
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "driver", "dosbox", "save-fixture", "source-report", "artifacts"): parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--sdl", type=Path)
    parser.add_argument("--public-app", type=Path, help="可选公开入口只读快照守卫；不固定某次构建SHA")
    parser.add_argument("--planner", type=Path, help="仅natural-stage1：只读当前实际Save选择下一动作，不作验收oracle")
    parser.add_argument("--max-turns", type=int, default=60, help="natural-stage1有界实际回合数1..100；不重开或调seed")
    parser.add_argument("--scenario", choices=oracle.SCENARIOS, required=True)
    args = parser.parse_args(); report = run(args)
    print(json.dumps({"ok": report["ok"], "status": report["status"], "probe_only": report["probe_only"]}))
    return 0 if report["status"] in ("passed", "original_calibrated") else 1


if __name__ == "__main__": raise SystemExit(main())
