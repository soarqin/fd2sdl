#!/usr/bin/env python3
"""S0定向CONTINUE/活动Load→首次controller结果门；默认仅准备，不启动窗口。

复用既有WGC/后台输入/owned/RPC/CDB。HP0-visible是受控导入域，不能称
自然战斗来源；只观察恢复、Z和selection取消，不运行整关或G历史套件。
"""
from __future__ import annotations

import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import shutil

import fd2_natural_oracle as natural
import fd2_s0_native as oracle
import fd2_stage1_complete_oracle as stage1
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze, unchanged, sha
from run_g4_controls_check import actor_templates
from run_graphics_tests import decode_save
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import Capture, running, title_templates
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets, empty_focus_navigation
from run_original_load import focused_actor, require_source_unchanged
from run_original_player import exact_tile, screen
from run_original_result import result_templates, result_branch


def write(path, data):
    Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def output_guard(root, original, output):
    root, original, output = (Path(p).resolve() for p in (root, original, output))
    if not output.is_relative_to(root / "build/s0") or output == original or original in output.parents or output.exists():
        raise ValueError("S0产物必须是build/s0内、原资源实际目标外的全新目录")


def prepare(args):
    root = args.root.resolve()
    original = (args.original or root / "original_game").resolve(strict=True)
    scenario = args.scenario
    fixture = args.stage1_source if scenario == "stage1-villager-priority" else original / "START.SAV"
    if fixture is None or not fixture.is_file():
        raise ValueError("S0缺只读原始SAV来源")
    source = decode_save(fixture)
    origin = {"path": str(fixture.resolve()), "sha256": sha(fixture), "size": fixture.stat().st_size,
              "mtime_ns": fixture.stat().st_mtime_ns}
    if scenario == "stage1-villager-priority":
        if args.stage1_report is None:
            raise ValueError("stage1代表必须给已通过原版端来源--stage1-report")
        origin["runtime_provenance"] = stage1.validate_origin_report(args.stage1_report, fixture)
        origin["resource_validation"] = stage1.validate_entry_source(original, source)
    elif sha(fixture) != "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310":
        raise ValueError("原START.SAV身份不同")
    plain, encoded, manifest = oracle.derive(source, scenario)
    assets = round_assets(original)
    return {"root": root, "original": original, "source": source, "plain": plain, "encoded": encoded,
            "manifest": manifest, "origin": origin, "fixture": fixture, "assets": assets,
            "titles": title_templates(original / "FDOTHER.DAT"), "result": result_templates(original, assets)}


def observe(capture, prepared, state, rpc, args, work, case, label, *, expected_cached_result=0):
    directory = work / label
    directory.mkdir()
    record = {"label": label, "after_input_count": len(case["inputs"]), "pid": capture.process.pid}
    case.setdefault("runtime_observations", []).append(record)
    if case["kind"] == "original":
        def decode_owner(memory, snapshot):
            actual, mapping = oracle.original_active(memory, prepared["original"], snapshot["DS_descriptor_before"])
            if oracle.check_active(actual, state, expected_cached_result=expected_cached_result)["ok"] is not True:
                raise AssertionError("恢复/非controller输入改变HP、flags、身份或提前设置结果：" + label)
            return actual, mapping

        _, snapshot = oracle.original_snapshot(rpc, directory, "active", record, owner_decoder=decode_owner)
        actual, mapping = record.pop("validated_owner")
        record.update(snapshot=snapshot, mapping=mapping)
    else:
        actual, receipt = oracle.sdl_active(args.cdb.resolve(), capture.process, args.sdl.resolve(), args.pdb.resolve(), directory, {})
        record["cdb"] = receipt
    record["check"] = oracle.check_active(actual, state, expected_cached_result=expected_cached_result)
    write(directory / "receipt.json", record)
    if record["check"]["ok"] is not True:
        raise AssertionError("恢复/非controller输入改变HP、flags、身份或提前设置结果：" + label)
    return actual


def exercise(capture, prepared, save, rpc, args, work, case):
    assets, original = prepared["assets"], prepared["original"]
    scenario = args.scenario
    state = prepared["source"] if scenario == "active-load-hidden-defeat" else prepared["plain"]
    guard = [save.read_bytes(), save.stat().st_mtime_ns]

    def branch(frame):
        return result_branch(frame, "defeat", assets, prepared["result"])

    def any_result(frame):
        return branch(frame) or result_branch(frame, "victory", assets, prepared["result"])

    def cp(label, predicate, **kwargs):
        result = wait_state(capture, label, lambda frame: any_result(frame) or predicate(frame), case["checkpoints"], **kwargs)
        require_source_unchanged(save, *guard)
        if result[1].get("kind") in ("native_defeat_resource", "native_victory_first_page"):
            raise AssertionError("首次controller出口之前已出现结果：" + label)
        return result

    wait_title(capture, prepared["titles"][3][0], timeout=90)
    capture.key("UP")
    capture.wait(prepared["titles"][3][2])
    capture.key("ENTER")
    cp("continue-browse-no-query", lambda frame: natural.focus_match(frame, original, state, assets), timeout=90)
    observe(capture, prepared, state, rpc, args, work, case, "continue-restored")
    if scenario == "active-load-hidden-defeat":
        for index, (key, _) in enumerate(empty_focus_navigation(state)):
            # 同一有证navigation模型保留实际camera/相对focus，不靠按键次数猜状态。
            step = natural.navigation(state, (state[META + 5] + {"LEFT": -1, "RIGHT": 1}.get(key, 0),
                                              state[META + 6] + {"UP": -1, "DOWN": 1}.get(key, 0)))[-1][1]
            capture.key(key)
            cp("load-empty-" + str(index), lambda frame, step=step: natural.focus_match(frame, original, step, assets))
            state = step
        menu = RoundOracle(assets, screen(state, state[META + 5:META + 7]))
        save.write_bytes(prepared["encoded"])
        guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
        case["controlled_disk_replacement"] = {"after_live_continue": True, "path": str(save), "sha256": sha(save)}
        capture.key("ENTER")
        cp("load-parent", lambda frame: menu.menu(frame, 0))
        capture.key("UP")
        capture.key("ENTER")
        cp("load-secondary", lambda frame: menu.secondary(frame, 0))
        capture.key("RIGHT")
        cp("load-selected", lambda frame: menu.secondary(frame, 2))
        capture.key("ENTER")
        cp("load-default-yes", lambda frame: menu.buttons(frame, 0))
        capture.key("ENTER")
        state = prepared["plain"]
        cp("active-load-restored-no-query", lambda frame: natural.focus_match(frame, original, state, assets), timeout=90)
        observe(capture, prepared, state, rpc, args, work, case, "active-load-restored")

    # 原ESC/Z均按flags/side轮询，首次索引从0开始。leader场实际选择HP0的raw0，
    # 不执行死亡整理或动作，唯一业务出口是后面的selection ESC。
    target = prepared["manifest"]["controller_actor"]
    eligible = [raw for raw in range(state[META + 1]) if state[UNITS + raw * STRIDE + 6] == 2
                and not state[UNITS + raw * STRIDE + 5] & 0x85]
    if target not in eligible:
        raise ValueError("controller目标不在原Z可选域")
    for raw in eligible[:eligible.index(target) + 1]:
        actor = actor_templates(original, state, assets, raw)
        capture.key("Z")
        _, hit = cp("z-focus-raw-" + str(raw), lambda frame, actor=actor: focused_actor(frame, assets, actor))
        position = hit["focus_screen"]
        if any((coordinate - 4) % 24 for coordinate in position):
            raise ValueError("实际焦点资源不在地图格上")
        unit_at = UNITS + raw * STRIDE
        x, y = state[unit_at:unit_at + 2]
        rx, ry = ((coordinate - 4) // 24 for coordinate in position)
        updated = bytearray(state)
        updated[META + 3:META + 9] = bytes((x - rx, y - ry, x, y, rx, ry))
        state = bytes(updated)
    observe(capture, prepared, state, rpc, args, work, case, "z-before-first-controller")
    summary = natural.selection_summary(original, state, assets, target)
    probes = natural.range_probe(original, state, assets, target)
    capture.key("ENTER")
    cp("actor-selection", lambda frame: {"kind": "resource_proven_selection", "summary": found, "range": reached}
       if (found := exact_tile(frame, summary["image"], summary["position"]))
       and (reached := natural.range_match(frame, probes)) else None)
    case["first_controller_query_trigger"] = "selection ESC"
    case["before_first_controller_input_count"] = len(case["inputs"])
    capture.key("ESC")
    if prepared["manifest"]["expected_post_controller_result"] == 0:
        cp("controller-ongoing", lambda frame: natural.focus_match(frame, original, state, assets), timeout=45)
        observe(capture, prepared, state, rpc, args, work, case, "controller-returned-ongoing")
        case["controller_exit_result"] = 0
    elif prepared["manifest"]["expected_post_controller_result"] == 1:
        wait_state(capture, "controller-victory-first-page",
                   lambda frame: result_branch(frame, "victory", assets, prepared["result"]),
                   case["checkpoints"], timeout=60)
        case.update(controller_exit_result=1, victory_first_page_before_cleanup=True,
                    settlement_continuation_verified=False)
    else:
        _, first = wait_state(capture, "controller-defeat", branch, case["checkpoints"], timeout=60,
                              allow_low_color_resource=True)
        if first["frame"] != 1:
            wait_state(capture, "defeat-game-over", lambda frame: found if (found := branch(frame)) and found["frame"] == 1 else None,
                       case["checkpoints"], timeout=30, allow_low_color_resource=True)
        capture.wait(prepared["titles"][3][0], blank_transition=True, timeout=45)
        case.update(controller_exit_result=2, game_over_then_title_before_cleanup=True)
    require_source_unchanged(save, *guard)
    case["controlled_save_unchanged_after_replacement"] = True


def run(args):
    prepared = prepare(args)
    root, original, output = prepared["root"], prepared["original"], args.artifacts.resolve()
    output_guard(root, original, output)
    if args.run:
        owned_job_required()
    output.mkdir(parents=True)
    (output / "controlled.SAV").write_bytes(prepared["encoded"])
    write(output / "fixture.json", {**prepared["manifest"], "origin": prepared["origin"]})
    if decode_save(output / "controlled.SAV") != prepared["plain"]:
        raise AssertionError("S0夹具完整往返失败")
    report = {"ok": False, "status": "prepared_only", "scenario": args.scenario, "cases": [],
              "source": prepared["origin"], "manifest": prepared["manifest"], "display_requirement": {},
              "scope": "controlled CONTINUE/active Load: HP+flags preserved; Z no query; selection ESC first result",
              "excluded": ["natural state reachability", "all historical G regression", "all RNG/waveform/pixels", "Linux/macOS"]}
    if not args.run:
        write(output / "comparison.json", report)
        return report
    binaries = [args.sdl.resolve(), args.driver.resolve(), args.dosbox.resolve(), args.cdb.resolve(), args.pdb.resolve(),
                args.cdb.resolve().parent / "dbghelp.dll", prepared["fixture"].resolve()]
    if args.stage1_report is not None:
        binaries.append(args.stage1_report.resolve())
    user_save = original.parent / "FD2.SAV"
    if user_save.is_file():
        binaries.append(user_save)
    inputs, before, sources = freeze(root, original, binaries)
    report["input_sha256"] = inputs
    report["product_source_paths"] = sources
    try:
        with windows_display_requirement(report["display_requirement"]):
            for kind in ("original", "sdl"):
                work = output / kind
                work.mkdir()
                case = {"kind": kind, "ok": False, "inputs": [], "checkpoints": [], "processes": [], "rpc": {}}
                report["cases"].append(case)
                env = os.environ.copy()
                for name in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER", "FD2SDL_FIELD_RNG_SEED"):
                    env.pop(name, None)
                with (oracle.rpc_api.OwnedRPC(case["rpc"]) if kind == "original" else nullcontext()) as rpc:
                    if kind == "original":
                        game = work / "game"
                        shutil.copytree(original, game)
                        save = game / "FD2.SAV"
                        config = work / "dosbox.conf"
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n' + f'mcp_server={rpc.port}\n'
                            '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                            '[autoexec]\nmount c "' + str(game) + '"\nc:\nFD2.EXE\nexit\n', encoding='utf-8')
                        command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
                        mutable = {save.resolve(), (game / "FD2.TMP").resolve()}
                        for path in game.rglob("*"):
                            if path.is_file() and path.resolve() not in mutable:
                                inputs[str(path.resolve())] = sha(path)
                        inputs[str(config)] = sha(config)
                    else:
                        save = work / "FD2.SAV"
                        command, cwd = [str(args.sdl.resolve())], root
                        env["FD2SDL_SAVE_PATH"] = str(save)
                    save.write_bytes(oracle.encode(prepared["source"]) if args.scenario == "active-load-hidden-defeat" else prepared["encoded"])
                    try:
                        if not unchanged(inputs, original, before, root, sources):
                            raise ValueError("首窗口前来源改变")
                        with running(command, cwd, env, work / "game.log", args.driver.resolve(), case["processes"]) as process:
                            if rpc:
                                rpc.bind(process)
                            capture = Capture(args.driver.resolve(), process, work, case["inputs"])
                            exercise(capture, prepared, save, rpc, args, work, case)
                        case["ok"] = len(case["processes"]) == 1 and all(p["cleaned"] and not p["forced_cleanup"] and p["exit_code"] == 0 for p in case["processes"])
                    finally:
                        case["sources_unchanged"] = unchanged(inputs, original, before, root, sources)
                        case["ok"] &= case["sources_unchanged"]
                        write(work / "comparison.json", case)
                    if not case["ok"]:
                        raise AssertionError("S0端点未通过：" + kind)
        report["ok"] = len(report["cases"]) == 2 and all(case["ok"] for case in report["cases"])
        report["status"] = "passed" if report["ok"] else "failed"
    except BaseException as error:
        report["status"] = "failed"
        report["error"] = {"type": type(error).__name__, "message": str(error)}
    finally:
        report["sources_unchanged"] = unchanged(inputs, original, before, root, sources)
        report["ok"] &= report["sources_unchanged"] and report["display_requirement"].get("released") is True
        if not report["ok"]:
            report["status"] = "failed"
        write(output / "comparison.json", report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "artifacts"):
        parser.add_argument("--" + name, type=Path, required=True)
    for name in ("original", "sdl", "driver", "dosbox", "pdb", "stage1-source", "stage1-report"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--cdb", type=Path, default=Path("C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"))
    parser.add_argument("--scenario", choices=oracle.SCENARIOS, required=True)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args(argv)
    if args.sdl and args.pdb is None:
        args.pdb = args.sdl.with_suffix(".pdb")
    if args.run and any(getattr(args, name) is None or not getattr(args, name).is_file() for name in ("sdl", "driver", "dosbox", "cdb", "pdb")):
        parser.error("原生运行需要本轮实际--sdl/--driver/--dosbox/--cdb/--pdb")
    report = run(args)
    print(json.dumps({key: report[key] for key in ("ok", "status", "scenario")}, ensure_ascii=False))
    return 0 if not args.run or report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
