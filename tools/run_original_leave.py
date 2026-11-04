#!/usr/bin/env python3
"""B1.3b：原版／SDL leave取消与肯定后自然退出的WGC配对。

依据 field_secondary_menu_execute @0x19df7、field_controller_input @0x117e7、
main @0x25bf4：leave返回-1，最终退出DOS；返回标题不作为通过结果。
默认只读复制START.SAV。由宿主driver run-job约束本轮树，串行运行。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

from run_original_comparison import Capture, archive_entry, hashes, running, title_templates
from run_original_battle_menu import save_checkpoint, wait_state, wait_title
from run_original_end_turn import META, RoundOracle, empty_focus_navigation, round_assets, validate_initial


def require_unchanged(save, source):
    if save.read_bytes() != source:
        raise AssertionError(f"leave changed its isolated save: {save}")


def wait_natural_exit(capture, kind, receipt, checkpoints, timeout=15):
    """在running的finally之前确认自然退出；超时先留WGC，再由owner清理。"""
    receipt.update(natural_exit=False, timed_out=False, observed_before_cleanup=True,
                   expected_exit=0, extra_inputs_after_yes=False)
    try:
        exit_code = capture.process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        receipt["timed_out"] = True
        frame = None
        detail = {"kind": "natural_exit_timeout", "last_frame_may_precede_error": False}
        try:
            frame = capture.frame()
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
            detail.update(capture_error=str(error), last_frame_may_precede_error=True)
            frame = capture.last
        if frame is not None:
            name = kind + "-leave-not-exited"
            save_checkpoint(capture, name, frame, detail, checkpoints)
            receipt["timeout_frame"] = str(capture.artifacts / f"{name}.png")
        raise TimeoutError(f"{kind} leave did not naturally exit within {timeout}s; last WGC retained before cleanup")
    receipt.update(natural_exit=True, exit_code=exit_code, observed_time=time.monotonic())
    if exit_code != 0:
        raise AssertionError(f"{kind} leave naturally exited {exit_code}, expected 0")


def exercise(capture, kind, assets, initial_focus, initial_steps, titles, save, source,
             checkpoints, exit_receipt):
    wait_title(capture, titles[3][0])
    save_checkpoint(capture, kind + "-title", capture.last,
                    {"kind": "title", "selected": 0}, checkpoints)
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    oracle = RoundOracle(assets, initial_focus)

    def checkpoint(name, predicate, **kwargs):
        return wait_state(capture, kind + "-" + name, predicate, checkpoints, **kwargs)

    checkpoint("initial-browse", oracle.initial_browse, timeout=30)
    for index, (key, position) in enumerate(initial_steps, 1):
        capture.key(key)
        oracle = RoundOracle(assets, position)
        checkpoint(f"empty-step-{index}", oracle.browse)
    require_unchanged(save, source)

    def open_leave(label):
        capture.key("ENTER")
        checkpoint(label + "-parent", lambda frame: oracle.menu(frame, 0))
        capture.key("UP")
        checkpoint(label + "-secondary-selected", lambda frame: oracle.menu(frame, 0))
        capture.key("ENTER")
        checkpoint(label + "-secondary", lambda frame: oracle.secondary(
            frame, 0, overview_available=True))
        capture.key("DOWN")
        checkpoint(label + "-leave-selected", lambda frame: oracle.secondary(
            frame, 3, overview_available=True))
        capture.key("ENTER")
        checkpoint(label + "-default-yes", lambda frame: oracle.buttons(frame, 0))

    for branch in ("escape", "no"):
        open_leave(branch)
        if branch == "no":
            capture.key("RIGHT")
            checkpoint("no-selected", lambda frame: oracle.buttons(frame, 1))
        capture.key("ESC" if branch == "escape" else "ENTER")
        checkpoint(branch + "-cancelled-browse", oracle.closed_browse,
                   transition=oracle.cancellation_notice)
        require_unchanged(save, source)
        if capture.process.poll() is not None:
            raise AssertionError(f"{kind} leave {branch} unexpectedly ended the game")
    open_leave("affirmative")
    require_unchanged(save, source)
    exit_receipt["last_yes_wgc"] = str(capture.artifacts / f"{kind}-affirmative-default-yes.png")
    exit_receipt["yes_sent_time"] = time.monotonic()
    capture.key("ENTER")
    # 此后不调用key、startup wait或close；自然退出与finally清理分别登记。
    wait_natural_exit(capture, kind, exit_receipt, checkpoints)
    require_unchanged(save, source)


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    output.mkdir(parents=True, exist_ok=False)
    before, source = hashes(original), fixture.read_bytes()
    report = {"ok": False, "scope": "leave ESC/NO restore browse; affirmative leave naturally exits 0",
              "excluded": "RNG invariance, full-dialog/full-map pixels, audio, Load and victory/defeat continuation",
              "platform": platform.platform(), "capture": "Windows.Graphics.Capture HWND only",
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(source).hexdigest()},
              "original_before": before, "inputs": [], "checkpoints": [], "processes": [], "cases": []}
    try:
        report["binaries"] = {name: {"path": str(path.resolve()),
                                     "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                               for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
        plain, focus = validate_initial(fixture, metadata[0])
        steps = empty_focus_navigation(plain)
        report["fixture"].update(active_count=plain[META + 1], roster_count=plain[META + 9],
                                 initial_focus_screen=list(focus), empty_navigation=steps)
        assets = round_assets(original)
        titles = title_templates(original / "FDOTHER.DAT")
        environment = os.environ.copy()
        for variable in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            environment.pop(variable, None)
        # 原版先完成并关闭，原版证据不因后续SDL停在标题而丢失。
        for kind in ("original", "sdl"):
            work = output / kind
            work.mkdir()
            if kind == "original":
                game_copy = work / "game"
                shutil.copytree(original, game_copy)
                save = game_copy / "FD2.SAV"
                shutil.copy2(fixture, save)
                config = work / "dosbox.conf"
                config.write_text(
                    "[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n"
                    "[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n"
                    f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                command = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"]
                cwd = work
            else:
                save = work / "FD2.SAV"
                shutil.copy2(fixture, save)
                environment["FD2SDL_SAVE_PATH"] = str(save)
                command, cwd = [str(args.game.resolve())], root
            case = {"name": kind, "status": "failed", "exit_observation": {}}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, work / "game.log", args.driver,
                             report["processes"]) as process:
                    case["pid"] = process.pid
                    capture = Capture(args.driver, process, work, report["inputs"])
                    exercise(capture, kind, assets, focus, steps, titles, save, source,
                             report["checkpoints"], case["exit_observation"])
                require_unchanged(save, source)
                case.update(status="passed", save_unchanged=True,
                            save_sha256=hashlib.sha256(save.read_bytes()).hexdigest())
            except Exception as error:
                case["error"] = str(error)
                case["save_unchanged"] = save.exists() and save.read_bytes() == source
                raise
        if any(not receipt["cleaned"] or receipt["forced_cleanup"] or receipt["exit_code"] != 0
               for receipt in report["processes"]):
            raise AssertionError("leave process was not normally cleaned")
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before
        report["fixture_unchanged"] = fixture.read_bytes() == source
        report["ok"] = report["ok"] and report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path,
                        help="只读真实来源；默认original_game/START.SAV，仅复制到隔离目录")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("原版leave对照要求 Windows WGC；未启动 GUI")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"原版leave对照无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
