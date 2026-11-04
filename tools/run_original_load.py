#!/usr/bin/env python3
"""B1.3b：真实START经END到第二回合，再Load回第一回合并由UI Save测量。

依据 field_secondary_menu_execute @0x19df7、sub_10010 @0x10010。
Load本身不得改磁盘；测量Save逐字对照来源，仅排除active +2 cache class、
明确记录的测量焦点导航和派生checksum。不比较未对齐的第二回合伤害。
Windows启动由宿主driver run-job约束整树；原版、SDL串行。
在第二回合逐键实测Z轮换0→1→2→3→0，再验证Load后的首次Z重回raw0。
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
import time
from types import SimpleNamespace

from run_graphics_tests import check_roster_count, decode_save
from run_original_comparison import Capture, archive_entry, hashes, running, title_templates
from run_original_battle_menu import match_indexed, wait_state, wait_title
from run_original_end_turn import (META, STRIDE, UNITS, RoundOracle, browse_anywhere,
                                   check_round_phase_log, empty_focus_navigation,
                                   round_assets, round_two_template, validate_initial)


def require_source_unchanged(save, source, source_mtime):
    if save.read_bytes() != source:
        raise AssertionError(f"END/cancel/Load unexpectedly changed source save before measurement: {save}")
    if save.stat().st_mtime_ns != source_mtime:
        raise AssertionError(f"END/cancel/Load rewrote source save mtime before measurement: {save}")


def measuring_focus(source, steps):
    if not steps:
        return source[META + 5:META + 9]
    sx, sy = steps[-1][1]
    if (sx - 4) % 24 or (sy - 4) % 24:
        raise ValueError("measurement navigation is not aligned to the viewport cells")
    rx, ry = (sx - 4) // 24, (sy - 4) // 24
    return bytes((source[META + 3] + rx, source[META + 4] + ry, rx, ry))


def check_load_measurement(source, measured, steps):
    """整容器严格对照；只有已确认cache字段和显式测量导航可与A不同。"""
    for data in (source, measured):
        if len(data) != 0x59cb or sum(data[:-4]) != int.from_bytes(data[-4:], "little"):
            raise AssertionError("Load measurement save size/checksum invalid")
    count = source[META + 1]
    roster_count = check_roster_count(source)
    expected = bytearray(source)
    expected[META + 5:META + 9] = measuring_focus(source, steps)
    cache_changes = []
    for index in range(count):
        position = UNITS + index * STRIDE + 2
        expected[position] = measured[position]
        if source[position] != measured[position]:
            cache_changes.append({"actor": index, "source": source[position], "measured": measured[position]})
    mismatches = [position for position in range(len(source) - 4) if expected[position] != measured[position]]
    if mismatches:
        first = ", ".join(f"{position:#x}" for position in mismatches[:12])
        raise AssertionError(f"Load did not restore source A outside the declared cache/navigation fields: {first}")
    return {"turn": measured[META], "stage": measured[META + 2],
            "active_count": count, "roster_count": roster_count,
            "resource_and_options": list(measured[META + 10:META + 18]),
            "source_camera_focus": list(source[META + 3:META + 9]),
            "measurement_camera_focus": list(measured[META + 3:META + 9]),
            "cell_state": list(measured[0x30a3:META]),
            "raw_roster_sha256": hashlib.sha256(measured[0x8a3:UNITS]).hexdigest(),
            "cache_class_changes_excluded": cache_changes,
            "source_equal_except_declared_cache_and_measuring_focus": True}


def restored_source(frame, oracle):
    # START下格actor会遮住初始cursor底缘，仍使用已确认上21行判据；
    # 复用对话固定上边沿消失的资源门，不以动画中的战场背景作oracle。
    initial = SimpleNamespace(browse=oracle.initial_browse, assets=oracle.assets)
    return RoundOracle.closed_browse(initial, frame)


def focus_cycle_assets(original, source, assets):
    # 延后import：player复用本模块的source guard，不能在模块顶层互相import。
    from run_original_player import cursor_assets, navigation, screen
    players = [i for i in range(source[META + 1])
               if source[UNITS + i * STRIDE + 6] == 2]
    if players != [0, 1, 2, 3]:
        raise ValueError("Load focus-cycle comparison requires raw player actors0..3")
    actors = []
    for raw in players:
        record = source[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]
        if record[5] & 0x85:
            raise ValueError("Load focus-cycle source actor is not eligible")
        isolated = bytearray(source)
        isolated[META + 1] = 1
        isolated[UNITS:UNITS + STRIDE] = record
        _, composites = cursor_assets(original / "FDICON.B24", isolated, assets)
        # 原版先cursor后actor；当前actor上移6px。只取其完整24行，
        # 下格actor从cursor+18开始遮挡，不能把遮住的最后6行纳入比对。
        templates = [image.crop((0, 0, 24, 24)) for image, offset in composites if offset == -6]
        actors.append({"raw_actor": raw, "unit_id": record[7],
                       "world": list(record[:2]), "templates": templates})
    if len({actor["unit_id"] for actor in actors}) != 4:
        raise ValueError("Load focus-cycle actors need distinct resource identities")
    leader = tuple(actors[0]["world"])
    cx, cy, fx, fy = source[META + 3:META + 7]
    # Load后Z到raw0及回源焦点都限定在内区，不改变测量Save的camera allowlist。
    if any(not (2 <= x - cx <= 10 and 2 <= y - cy <= 5)
           for x, y in (leader, (fx, fy))):
        raise ValueError("Load focus-cycle restoration would need unverified scrolling")
    positioned = bytearray(source)
    positioned[META + 5:META + 7] = bytes(leader)
    returning = [(key, screen(source, cell))
                 for key, cell in navigation(positioned, (fx, fy))]
    if len(returning) > 20:
        raise ValueError("Load focus return exceeds bounded input budget")
    return {"actors": actors, "returning": returning}


def focused_actor(frame, assets, actor):
    matches = []
    for row in range(1, 7):
        for column in range(1, 12):
            position = (4 + column * 24, 4 + row * 24)
            found = next((index for index, template in enumerate(actor["templates"])
                          if match_indexed(frame, template, (position[0], position[1] - 6))["ok"]), None)
            if found is None:
                continue
            oracle = RoundOracle(assets, position)
            if (not oracle.menu(frame) and not oracle.buttons(frame) and
                    not match_indexed(frame, assets["dialog_top"], (5, 112))["ok"]):
                matches.append({"kind": "resource_proven_focus_actor", "raw_actor": actor["raw_actor"],
                                "unit_id": actor["unit_id"], "focus_screen": list(position),
                                "sprite_cursor_template": found, "sprite_cursor_rows": 24})
    return matches[0] if len(matches) == 1 else None


def wait_measured_save(save, previous, previous_mtime, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if save.read_bytes() != previous or save.stat().st_mtime_ns != previous_mtime:
            try:
                return decode_save(save)
            except AssertionError:
                pass  # 原版writer关闭文件前可能短暂不完整。
        time.sleep(0.05)
    raise TimeoutError("explicit measurement Save did not finish with a valid checksum")


def exercise(capture, kind, assets, focus, steps, titles, banner, save, source_bytes, log,
             checkpoints, observation, *, source_mtime, focus_cycle):
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    source_oracle = RoundOracle(assets, focus)
    oracle = source_oracle

    def checkpoint(name, predicate, **kwargs):
        return wait_state(capture, kind + "-" + name, predicate, checkpoints, **kwargs)

    checkpoint("source-initial-browse", oracle.initial_browse, timeout=30)
    for index, (key, position) in enumerate(steps, 1):
        capture.key(key)
        oracle = RoundOracle(assets, position)
        checkpoint(f"end-empty-step-{index}", oracle.browse)
    capture.key("ENTER")
    checkpoint("end-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("DOWN")
    checkpoint("end-selected", lambda frame: oracle.menu(frame, 3))
    capture.key("ENTER")
    checkpoint("end-default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")
    if kind == "original":
        def round_two(frame):
            matched = match_indexed(frame, banner, (120, 84))
            return {"kind": "round_banner", "turn": 2, **matched} if matched["ok"] else None
        checkpoint("round-002", round_two, timeout=90)
    else:
        from field_play_input_test import wait_log
        check_round_phase_log(wait_log(capture.process, log, "field phase: turn=2 phase=2 ", timeout=90))
    checkpoint("round2-leader", lambda frame: browse_anywhere(frame, assets, leader=True), timeout=30)
    require_source_unchanged(save, source_bytes, source_mtime)
    observation["round2_reached_without_save"] = True
    observation["focus_cycle_before_load"] = []
    for index, actor in enumerate(focus_cycle["actors"] + focus_cycle["actors"][:1], 1):
        capture.key("Z")
        _, detail = checkpoint(f"round2-focus-cycle-{index}-raw-{actor['raw_actor']}",
                               lambda frame, actor=actor: focused_actor(frame, assets, actor))
        require_source_unchanged(save, source_bytes, source_mtime)
        observation["focus_cycle_before_load"].append(detail)
    capture.key("LEFT")
    _, detail = checkpoint("round2-empty-cell", lambda frame: browse_anywhere(frame, assets))
    oracle = RoundOracle(assets, tuple(detail["focus_screen"]))

    def open_secondary_action(label, direction, selected):
        capture.key("ENTER")
        checkpoint(label + "-parent", lambda frame: oracle.menu(frame, 0))
        capture.key("UP")
        capture.key("ENTER")
        checkpoint(label + "-secondary", lambda frame: oracle.secondary(
            frame, 0, overview_available=True))
        capture.key(direction)
        checkpoint(label + "-selected", lambda frame: oracle.secondary(
            frame, selected, overview_available=True))
        capture.key("ENTER")
        checkpoint(label + "-default-yes", lambda frame: oracle.buttons(frame, 0))

    observation["cancellations"] = []
    for branch in ("escape", "no"):
        open_secondary_action("load-" + branch, "RIGHT", 2)
        if branch == "no":
            capture.key("RIGHT")
            checkpoint("load-no-selected", lambda frame: oracle.buttons(frame, 1))
        capture.key("ESC" if branch == "escape" else "ENTER")
        checkpoint("load-" + branch + "-browse", oracle.closed_browse,
                   transition=oracle.cancellation_notice)
        require_source_unchanged(save, source_bytes, source_mtime)
        if capture.process.poll() is not None:
            raise AssertionError(f"{kind} Load {branch} exited the game")
        observation["cancellations"].append({"branch": branch, "browse_restored": True,
                                             "disk_unchanged": True, "mtime_ns_unchanged": True,
                                             "full_runtime_fields_measured": False})
    open_secondary_action("load-affirmative", "RIGHT", 2)
    capture.key("ENTER")  # 一次Load肯定后只观察，不补字幕键或startup Esc。
    checkpoint("loaded-source-initial-browse",
               lambda frame: restored_source(frame, source_oracle), timeout=45)
    require_source_unchanged(save, source_bytes, source_mtime)
    observation["load_disk_unchanged_before_measurement"] = True
    observation["source_mtime_ns_before_measurement"] = source_mtime
    capture.key("Z")
    _, detail = checkpoint("loaded-focus-cycle-raw-0", lambda frame: focused_actor(
        frame, assets, focus_cycle["actors"][0]))
    observation["focus_cycle_after_load"] = detail
    require_source_unchanged(save, source_bytes, source_mtime)
    for index, (key, position) in enumerate(focus_cycle["returning"], 1):
        capture.key(key)
        at = RoundOracle(assets, position)
        checkpoint(f"loaded-source-return-step-{index}", at.initial_browse)
        require_source_unchanged(save, source_bytes, source_mtime)
    checkpoint("loaded-source-focus-restored", lambda frame: restored_source(frame, source_oracle))
    oracle = source_oracle
    for index, (key, position) in enumerate(steps, 1):
        capture.key(key)
        oracle = RoundOracle(assets, position)
        checkpoint(f"measurement-empty-step-{index}", oracle.browse)
    open_secondary_action("measurement-save", "LEFT", 1)
    require_source_unchanged(save, source_bytes, source_mtime)
    previous, previous_mtime = save.read_bytes(), save.stat().st_mtime_ns
    capture.key("ENTER")
    measured = wait_measured_save(save, previous, previous_mtime)
    checkpoint("measurement-saved-browse", oracle.closed_browse)
    observation["explicit_measurement_save_completed"] = True
    observation["measurement_save_mtime_ns"] = save.stat().st_mtime_ns
    return measured


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    output.mkdir(parents=True, exist_ok=False)
    before_hashes, source_bytes = hashes(original), fixture.read_bytes()
    report = {"ok": False, "scope": "round2 Z raw0/1/2/3/0 -> active Load A -> first Z raw0 -> source focus -> explicit UI Save",
              "cancel_scope": "browse/focus restored, disk unchanged, menu reusable; complete runtime HP/MP/turn/RNG not measured for Esc/NO",
              "excluded": "pre-Load round2 damage/AI/RNG equality, active +2 cache class, documented measuring-focus navigation, full-frame/audio equivalence",
              "capture": "Windows.Graphics.Capture HWND only", "platform": platform.platform(),
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(source_bytes).hexdigest()},
              "original_before": before_hashes, "processes": [], "inputs": [], "checkpoints": [], "cases": []}
    try:
        report["binaries"] = {name: {"path": str(path.resolve()),
                                     "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                               for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
        source, focus = validate_initial(fixture, metadata[0])
        steps = empty_focus_navigation(source)
        report["fixture"].update(active_count=source[META + 1], roster_count=check_roster_count(source),
                                 source_focus_screen=list(focus), measuring_navigation=steps)
        assets = round_assets(original)
        focus_cycle = focus_cycle_assets(original, source, assets)
        report["focus_cycle_plan"] = {
            "actors": [{key: actor[key] for key in ("raw_actor", "unit_id", "world")}
                       for actor in focus_cycle["actors"]],
            "return_to_source_focus": focus_cycle["returning"],
            "evidence": "each Z independently ACKed by expected actor FDICON+cursor, not only logs"}
        banner = round_two_template(original / "FDOTHER.DAT", assets["palette"])
        titles = title_templates(original / "FDOTHER.DAT")
        environment = os.environ.copy()
        for variable in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            environment.pop(variable, None)
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
                command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"
                shutil.copy2(fixture, save)
                environment.update(FD2SDL_SAVE_PATH=str(save), FD2SDL_FIELD_RNG_SEED="31256")
                command, cwd = [str(args.game.resolve())], root
            source_mtime = save.stat().st_mtime_ns
            case = {"name": kind, "status": "failed", "source_mtime_ns_before_start": source_mtime,
                    "observation": {}}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, work / "game.log", args.driver,
                             report["processes"]) as process:
                    case["pid"] = process.pid
                    capture = Capture(args.driver, process, work, report["inputs"])
                    measured = exercise(capture, kind, assets, focus, steps, titles, banner, save,
                                        source_bytes, work / "game.log", report["checkpoints"], case["observation"],
                                        source_mtime=source_mtime, focus_cycle=focus_cycle)
                    shutil.copy2(save, work / "loaded-source-measured.SAV")
                    case["measurement"] = check_load_measurement(source, measured, steps)
                case["status"] = "passed"
            except Exception as error:
                case["error"] = str(error)
                raise
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before_hashes
        report["fixture_unchanged"] = fixture.read_bytes() == source_bytes
        report["ok"] = report["ok"] and report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path,
                        help="只读来源A；默认original_game/START.SAV，仅复制到隔离目录")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("原版Load对照要求 Windows WGC；未启动 GUI")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"原版Load对照无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
