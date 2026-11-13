#!/usr/bin/env python3
"""S2 victory2/233c6布局双fade代表：默认只读准备；--run才owned串行WGC。

原版真实CONTINUE/selection ESC到胜利2；SDL非shipping显式source Scene。
只签布局/原资源story7/静态完成标记，不开发或验收stage3，不读取其原活动owner。
"""
from __future__ import annotations

import argparse
from contextlib import nullcontext, contextmanager
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from PIL import Image
import fd2_natural_oracle as natural
import fd2_s0_native as s0
import fd2_s2_snapshot as s2_snapshot
import s2_layout_native_fixture as fixture
import s2_result_native_fixture as result_fixture
from fd2_display_requirement import windows_display_requirement
from fd2_opening_oracle import page_match
from field_play_input_test import wait_log
from run_g2_battle_check import freeze, unchanged, sha
from run_g4_controls_check import actor_templates
from run_original_battle_animation import owned_job_required, OwnedHelper, wait_ready, series_index
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import Capture, running, title_templates, normalize
from run_original_load import focused_actor, require_source_unchanged
from run_original_player import exact_tile
from run_s2_result_check import output_guard, write


@contextmanager
def fade_series(capture, record):
    directory = capture.artifacts / "action-series"; slot = {}; record["series_helper"] = slot
    # 仍只采owner HWND；16ms整段采集不要求逐2ms级或重采样补帧。
    helper = OwnedHelper(capture.driver, [capture.driver, "capture-series", capture.process.pid, directory, 45000, 16],
                         capture.artifacts, "s2-fade-series", slot)
    ready = None
    try:
        child = helper.start(); ready = wait_ready(directory / "ready.json", child, capture.process)
        if ready.get("pid") != capture.process.pid or not ready.get("hwnd"): raise AssertionError("fade WGC ready owner不符")
        yield
    finally:
        if directory.exists(): (directory / "stop").write_text("stop\n", encoding="utf8")
        helper.finish()
        if ready is not None:
            record["continuous_wgc"] = {"ready": ready, "summary": slot["summary"],
                "frames": series_index(directory, slot["summary"], ready, capture.process.pid),
                "complete_raw_collection": True, "full_RGB_equivalence_claimed": False}


def fade_gate(frames):
    maxima = []
    for row in frames:
        path = Path(row["path"])
        if sha(path) != row["sha256"]: raise AssertionError("fade原始WGC帧hash不同")
        with Image.open(path) as image: value = max(high for low, high in normalize(image).getextrema())
        maxima.append(value)
    # 16ms采样不承诺恰中6ms的全0黑段；预先声明<=12的完整近黑界，
    # 不补帧、不插值，真实全0帧仍单独报告。完整dark63/64源序由CPU/C门证明。
    dark = [i for i, value in enumerate(maxima) if value <= 12]
    if not any(any(value >= 64 for value in maxima[:i]) and any(value >= 64 for value in maxima[i + 1:]) for i in dark):
        raise AssertionError("连续WGC未证明正常亮度→全帧近黑→恢复；不得用最终文字页替代fade")
    return {"ok": True, "near_black_max_channel": 12, "frame_channel_maxima": maxima,
        "near_black_frames": [{key: frames[i][key] for key in ("index", "sha256", "wgc_timestamp_100ns")} for i in dark],
        "exact_black_frames": [frames[i]["index"] for i in dark if maxima[i] == 0],
        "scope": "visible fade boundary only; 64/65 source operations and 2ms order proved separately by CPU/C",
        "all_intermediate_grades_or_absolute_timing_claimed": False}


def completion_status(state):
    pair = state["stage"], state["roster_count"]
    if pair not in ((2, 4), (2, 5), (3, 5)):
        raise AssertionError("原胜利2静态stage/roster违反已证追加后stage写序")
    return pair == (3, 5)


def observe(capture, p, args, rpc, work, case, label, *, layout=False, complete=False):
    work = work / label; work.mkdir(); record = {"label": label, "pid": capture.process.pid}
    if case["kind"] == "original":
        def decode(memory, snapshot):
            if complete:
                state, proof = result_fixture.original_owner(memory, p["original"], snapshot["DS_descriptor_before"], actors=False)
                import struct
                state["roster_count"] = struct.unpack_from("<I", memory, proof["object2_DS_offset"] + 0x3bfb)[0]
                completion_status(state)
                proof.update(scope="static stage/roster globals only after final ACK; no active-pointer dereference")
            else:
                state, proof = fixture.original_owner(memory, p["original"], snapshot["DS_descriptor_before"])
                if layout: fixture.validate_layout_prefix(state, p["manifest"])
            return state, proof
        _, snapshot = s2_snapshot.original_snapshot(rpc, work, "layout", record, owner_decoder=decode, explicit_selectors=True)
        state, proof = record.pop("validated_owner"); record.update(snapshot=snapshot, mapping=proof)
    elif complete:
        symbols = s0.cdb_api.symbols(args.sdl, args.pdb, args.cdb.parent / "dbghelp.dll")
        cdb_record = {"symbols": symbols}
        with s0.cdb_api.CDB(args.cdb, capture.process, work, cdb_record) as debugger:
            debugger.request(".sympath " + str(args.pdb.resolve().parent)); debugger.request(".reload /f " + args.sdl.name)
            def get(expression): return s0.cdb_api.cpp_integer(debugger.request("?? " + expression))
            state = {"stage": get("s2_layout_game.stage"), "roster_count": get("s2_layout_game.roster_count"), "fixture_phase": get("s2_layout_phase")}
        if state != {"stage": 3, "roster_count": 5, "fixture_phase": 2}: raise AssertionError("SDL source2完成标记不同")
        record.update(cdb=cdb_record, scope="named fixture scalar completion only; no stage3 controller")
    else:
        state, cdb_record = fixture.sdl_owner(args.cdb, capture.process, args.sdl, args.pdb, work)
        record["cdb"] = cdb_record
        if layout: fixture.validate_layout_prefix(state, p["manifest"])
    if not layout and not complete and state["actors"] != s0.active_fields(p["plain"][fixture.UNITS:fixture.UNITS + 15 * fixture.STRIDE], 15):
        raise AssertionError("初次source入口前受控活动记录改变")
    record["actual"] = state; case.setdefault("runtime_observations", []).append(record); write(work / "receipt.json", record)
    return state


def wait_original_completion(capture, p, args, rpc, work, case):
    # 仅等本已证caller的静态写点，不把owner/桥错误改成可重试，也不读active。
    started = time.monotonic()
    for attempt in range(1, 13):
        state = observe(capture, p, args, rpc, work, case, f"source-static-completion-{attempt:02d}", complete=True)
        if completion_status(state):
            case["static_completion_wait"] = {"attempts": attempt, "maximum_attempts": 12,
                "elapsed_seconds": time.monotonic() - started, "owner_dereferenced": False,
                "allowed_prefixes": [[2, 4], [2, 5], [3, 5]]}
            return
        if time.monotonic() - started >= 15: break
    raise AssertionError("原胜利2有限静态完成等待耗尽；不复用未完成dump作成功")


def exercise(capture, p, args, rpc, work, case, save):
    guard = save.read_bytes(), save.stat().st_mtime_ns
    if case["kind"] == "original":
        wait_title(capture, p["titles"][3][0], timeout=90)
        capture.key("UP"); capture.wait(p["titles"][3][2]); capture.key("ENTER")
        wait_state(capture, "continue-stage2", lambda frame: natural.focus_match(frame, p["original"], p["plain"], p["assets"]), case["checkpoints"], timeout=90)
        observe(capture, p, args, rpc, work, case, "before-controller")
        capture.key("Z")
        wait_state(capture, "focus-raw0", lambda frame: focused_actor(frame, p["assets"], p["actor_template"]), case["checkpoints"])
        capture.key("ENTER")
        summary = p["summary"]
        wait_state(capture, "selection-raw0", lambda frame: {"summary": found, "range": reached}
            if (found := exact_tile(frame, summary["image"], summary["position"])) and
               (reached := natural.range_match(frame, p["range_probes"])) else None, case["checkpoints"])
        trigger = "ESC"
        case["trigger"] = "real117e7 selection ESC ->205b4 victory ->stage2/230f2 ->233c6"
    else:
        wait_log(capture.process, work / "game.log", "S2 layout source ready:", timeout=60)
        wait_state(capture, "source-only-stage2-ready", lambda frame: natural.focus_match(frame, p["original"], p["plain"], p["assets"]), case["checkpoints"], timeout=60)
        observe(capture, p, args, rpc, work, case, "before-explicit-source")
        trigger = "ENTER"; case["trigger"] = "nonshipping explicit stage.source.victory.002 Scene; no stage/controller binding"
    with fade_series(capture, case):
        capture.key(trigger)
        wait_state(capture, "story7-wait-00", lambda frame: page_match(frame, p["pages"][0], p["assets"]), case["checkpoints"], timeout=90)
    case["fade_gate"] = fade_gate(case["continuous_wgc"]["frames"])
    observe(capture, p, args, rpc, work, case, "layout-first-story-wait", layout=True)
    for index, page in enumerate(p["pages"]):
        if index:
            wait_state(capture, f"story7-wait-{index:02d}", lambda frame, page=page: page_match(frame, page, p["assets"]), case["checkpoints"], timeout=90)
        require_source_unchanged(save, *guard)
        capture.key("SPACE")
    if case["kind"] == "sdl":
        wait_log(capture.process, work / "game.log", "S2 layout source done: stage3/roster5", timeout=60)
    # 原版不再等/判断stage3活动画面，只观察已证静态全局；不读释放后的active表。
    if case["kind"] == "original": wait_original_completion(capture, p, args, rpc, work, case)
    else: observe(capture, p, args, rpc, work, case, "source-complete-static-markers", complete=True)
    require_source_unchanged(save, *guard); case["controlled_save_unchanged"] = True


def run(args):
    root = args.root.resolve(); original = (args.original or root / "original_game").resolve(strict=True); output = args.artifacts.resolve()
    output_guard(root, original, output)
    p = fixture.prepare(original); p["titles"] = title_templates(original / "FDOTHER.DAT")
    natural._stage_map(original, p["plain"])
    focused = natural.view(p["plain"], tuple(p["plain"][fixture.UNITS:fixture.UNITS + 2]))
    p["actor_template"] = actor_templates(original, focused, p["assets"], 0)
    p["summary"] = natural.selection_summary(original, focused, p["assets"], 0)
    p["range_probes"] = natural.range_probe(original, focused, p["assets"], 0)
    output.mkdir(parents=True); controlled = output / "controlled.SAV"; controlled.write_bytes(p["encoded"]); write(output / "fixture.json", p["manifest"])
    report = {"schema": "fd2-s2-layout-native-v1", "scenario": "stage2-victory-layout-double-fade", "ok": False, "status": "prepared",
        "manifest": p["manifest"], "cases": [], "display_requirement": {}, "original_runtime_executed": False, "SDL_runtime_executed": False,
        "new_gameplay_enabled": False, "headless_predicates": {"story7_waits": len(p["pages"]), "range_resources": len(p["range_probes"])},
        "scope": "controlled real victory2 vs explicit source Scene; doublefade/layout0..6/story7/static stage+roster completion",
        "excluded": ["natural stage2 history", "shipping victory binding", "stage3 controller/resources", "post-free original active owner",
                     "all intermediate fade RGB or absolute2ms timing", "RNG whole trace/audio equality", "Linux/macOS"]}
    if args.sdl:
        checked = subprocess.run([str(args.sdl.resolve()), "--check", str(controlled)], cwd=root, text=True, capture_output=True)
        report["SDL_headless_resource_check"] = {"exe_sha256": sha(args.sdl), "stdout": checked.stdout, "stderr": checked.stderr,
                                                 "returncode": checked.returncode, "window_started": False}
        if checked.returncode:
            report.update(status="failed", error={"type": "resource_precheck", "message": "SDL无窗口资源预检失败；保留stdout/stderr，未启动GUI"})
            write(output / "comparison.json", report); return report
    if not args.run: write(output / "comparison.json", report); return report
    owned_job_required()
    binaries = [args.sdl.resolve(), args.pdb.resolve(), args.driver.resolve(), args.dosbox.resolve(), args.cdb.resolve(), args.cdb.parent / "dbghelp.dll",
                root / "src/tests/s2_layout_native_fixture.c"]
    user_save = original.parent / "FD2.SAV"
    if user_save.is_file(): binaries.append(user_save)
    inputs, before, sources = freeze(root, original, binaries); report.update(input_sha256=inputs, product_source_paths=sources)
    try:
        with windows_display_requirement(report["display_requirement"]):
            for kind in ("original", "sdl"):
                work = output / kind; work.mkdir()
                case = {"kind": kind, "ok": False, "inputs": [], "checkpoints": [], "processes": [], "rpc": {}}; report["cases"].append(case)
                env = os.environ.copy()
                for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER", "FD2SDL_FIELD_RNG_SEED"): env.pop(name, None)
                with (s2_snapshot.OwnedRPC(case["rpc"]) if kind == "original" else nullcontext()) as rpc:
                    if kind == "original":
                        game = work / "game"; shutil.copytree(original, game); save = game / "FD2.SAV"; config = work / "dosbox.conf"
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n' + f'mcp_server={rpc.port}\n'
                            '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                            '[autoexec]\nmount c "' + str(game) + '"\nc:\nFD2.EXE\nexit\n', encoding="utf8")
                        command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
                        mutable = {save.resolve(), (game / "FD2.TMP").resolve()}
                        for path in game.rglob("*"):
                            if path.is_file() and path.resolve() not in mutable: inputs[str(path.resolve())] = sha(path)
                        inputs[str(config)] = sha(config)
                    else: save = work / "controlled.SAV"; command, cwd = [str(args.sdl.resolve()), "--save", str(save)], root
                    save.write_bytes(p["encoded"])
                    try:
                        if not unchanged(inputs, original, before, root, sources): raise ValueError("首窗口前来源改变")
                        with running(command, cwd, env, work / "game.log", args.driver.resolve(), case["processes"]) as process:
                            if rpc: rpc.bind(process)
                            capture = Capture(args.driver.resolve(), process, work, case["inputs"])
                            exercise(capture, p, args, rpc, work, case, save)
                        case["ok"] = len(case["processes"]) == 1 and all(r["cleaned"] and not r["forced_cleanup"] and r["exit_code"] == 0 for r in case["processes"])
                    finally:
                        case["sources_unchanged"] = unchanged(inputs, original, before, root, sources); case["ok"] &= case["sources_unchanged"]
                        write(work / "comparison.json", case)
                    if not case["ok"]: raise AssertionError("S2布局机制端点未通过：" + kind)
        report.update(ok=all(c["ok"] for c in report["cases"]), status="passed", original_runtime_executed=True, SDL_runtime_executed=True)
    except BaseException as error: report.update(status="failed", error={"type": type(error).__name__, "message": str(error)})
    finally:
        report["sources_unchanged"] = unchanged(inputs, original, before, root, sources)
        report["ok"] &= report["sources_unchanged"] and report["display_requirement"].get("released") is True
        if not report["ok"]: report["status"] = "failed"
        write(output / "comparison.json", report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "artifacts"): parser.add_argument("--" + name, type=Path, required=True)
    for name in ("original", "sdl", "pdb", "driver", "dosbox"): parser.add_argument("--" + name, type=Path)
    parser.add_argument("--cdb", type=Path, default=Path("C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"))
    parser.add_argument("--run", action="store_true"); args = parser.parse_args(argv)
    if args.sdl and args.pdb is None: args.pdb = args.sdl.with_suffix(".pdb")
    if args.run and any(getattr(args, name) is None or not getattr(args, name).is_file() for name in ("sdl", "pdb", "driver", "dosbox", "cdb")):
        parser.error("原生运行需实际--sdl/--pdb/--driver/--dosbox/--cdb")
    report = run(args); print(json.dumps({key: report[key] for key in ("ok", "status", "scenario")}, ensure_ascii=False))
    return 0 if report["status"] == "prepared" or report["ok"] else 1


if __name__ == "__main__": raise SystemExit(main())
