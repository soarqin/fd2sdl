#!/usr/bin/env python3
"""串行运行 SDL 图形回归与 DOSBox 原版对照，证据只写构建目录。"""

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src/tests"))
from field_play_input_test import check_bitmap, wait_log

B3_SCENARIOS = ("controls", "herb-full", "herb-injured", "drop-herb", "drop-weapon", "drop-armor", "drop-last",
                "eqt-incompatible", "eqt-weapon", "give-cancel", "give-empty", "give-swap", "give-moved",
                *(f"use-{code:02d}" for code in range(5, 25)), "use-area2", "use-23-mp0")
B6_SCENARIOS = ("physical-full-hit", "player-magic0", "player-heal13")
B4_SCENARIOS = ("physical-cap", "physical-overwrite", "heal", "learn", "counter-multi", "status")
B5_LOOT_SCENARIOS = ("chest-empty", "chest-swap", "chest-list-cancel", "chest-no", "chest-full-no",
                     "treasure-money", "loot-item", "loot-money", "loot-callback", "loot-story")
B5_STAGE1_SCENARIOS = ("stage1-turn3", "stage1-villager-defeat")
# CLI登记不代表实际执行或整阶段已验收；自然场景的随机战斗结果不要求逐byte相同。
B5_SCENARIOS = ("prepare-controlled", "defeat", *B5_LOOT_SCENARIOS, *B5_STAGE1_SCENARIOS, "natural-stage0")


def hashes(directory):
    return {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob("*")) if p.is_file()}


def decode_save(path):
    """仅检查已登记的 XOR/checksum 与活动快照；不构造原版状态。"""
    data = bytearray(path.read_bytes())
    if len(data) != 0x59cb:
        raise AssertionError("save size is not 0x59cb")
    state = 0xa5
    for index in range(len(data)):
        state = (state + 0x9014) & 0xffff
        state = ((state << 3) | (state >> 13)) & 0xffff
        data[index] ^= state & 0xff
    if sum(data[:-4]) != int.from_bytes(data[-4:], "little"):
        raise AssertionError("save checksum mismatch")
    return bytes(data)


def check_roster_count(plain):
    """meta[9]是独立持续队伍表数量，不是活动actor数量的副本。"""
    count = plain[0x30cc]
    if count > 32:
        raise AssertionError("persistent roster count exceeds 32")
    return count


def check_storage_preservation(before, after, metadata_size):
    """新session拥有完整0xa00持续队伍表；目标metadata未用尾和手工槽保留。"""
    roster_count = check_roster_count(after)
    for start, end in ((metadata_size, 0x8a3), (0x312b, 0x59c7)):
        if after[start:end] != before[start:end]:
            raise AssertionError(f"Save overwrote unrelated container bytes or manual slots in {start:#x}:{end:#x}")
    return roster_count


def check_end_turn_log(text):
    """只接受一次肯定提交和相应完整phase顺序，防止重复按键跨过下一回合。"""
    checkpoints = (
        "field end turn: turn=1 phase=2 remaining=4",
        "field phase: turn=1 phase=1 remaining=0",
        "field phase: turn=1 phase=0 remaining=8",
        "field phase: turn=2 phase=2 remaining=4",
    )
    lines = text.splitlines()
    if any(lines.count(checkpoint) != 1 for checkpoint in checkpoints):
        raise AssertionError("end-turn confirmation or phase checkpoint missing/repeated")
    positions = [lines.index(checkpoint) for checkpoint in checkpoints]
    if positions != sorted(positions):
        raise AssertionError("end-turn phase checkpoints out of order")
    if any(line.startswith("field phase:") and "turn=3 " in line for line in lines):
        raise AssertionError("one end-turn confirmation advanced a second round")


def terminal_save_fixture(plain, outcome):
    """编码受控终态：只改已确认隐藏位和checksum；运行证据须由对应场景另验。"""
    if len(plain) != 0x59cb or sum(plain[:-4]) != int.from_bytes(plain[-4:], "little"):
        raise ValueError("terminal fixture requires a valid plaintext save")
    count = plain[0x30c4]
    if plain[0x30c5] != 0 or not 0 < count <= 64:
        raise ValueError("terminal fixture requires a supported stage 0 snapshot")
    if outcome not in ("victory", "defeat"):
        raise ValueError("unknown terminal outcome")
    data = bytearray(plain)
    targets = [index for index in range(count)
               if outcome == "victory" and data[0x12a3 + index * 0x50 + 6] == 0]
    if outcome == "defeat":
        if data[0x12a3 + 6] != 2:
            raise ValueError("terminal fixture leader is not a player actor")
        targets = [0]
    if not targets:
        raise ValueError("terminal fixture has no actors for the requested outcome")
    for index in targets:
        data[0x12a3 + index * 0x50 + 5] |= 1
    data[-4:] = sum(data[:-4]).to_bytes(4, "little")
    state = 0xa5
    for index in range(len(data)):
        state = (state + 0x9014) & 0xffff
        state = ((state << 3) | (state >> 13)) & 0xffff
        data[index] ^= state & 0xff
    return bytes(data)


def check_confirmation_buttons(image, resource):
    """按FDOTHER[2]原图校验固定位置的YES/NO，允许normal/highlight闪动。"""
    from run_original_comparison import archive_entry, normalize

    actual = normalize(image)
    data = resource.read_bytes()
    palette = archive_entry(data, 0)
    commands = archive_entry(data, 2)
    if len(palette) < 768 or max(palette[:768]) > 63 or len(commands) < 4:
        raise ValueError("invalid confirmation resource palette or directory")
    first = struct.unpack_from("<I", commands)[0]
    if first % 4 or first < 54 * 4 or first > len(commands):
        raise ValueError("invalid confirmation frame directory")
    result = {}
    for choice, name in enumerate(("yes", "no")):
        x, y = 232 + choice * 32, 168
        for phase in (0, 1):
            frame = (16 + choice) * 3 + phase
            start, end = struct.unpack_from("<II", commands, frame * 4)
            if start < first or end > len(commands) or end - start != 4 + 24 * 16:
                raise ValueError("invalid confirmation frame bounds")
            if struct.unpack_from("<HH", commands, start) != (24, 16):
                raise ValueError("invalid confirmation frame dimensions")
            pixels = commands[start + 4:end]
            opaque = [(index, color) for index, color in enumerate(pixels) if color]
            matches = bool(opaque)
            for index, color in opaque:
                pixel = actual.getpixel((x + index % 24, y + index // 24))
                if any(abs(pixel[channel] - (palette[color * 3 + channel] << 2)) > 3
                       for channel in range(3)):
                    matches = False
                    break
            if matches:
                result[name] = {"position": [x, y], "frame": frame, "checked_pixels": len(opaque)}
                break
        else:
            raise AssertionError(f"{name.upper()} button missing or wrong at ({x},{y})")
    return result


def check_browse_restored(expected, actual):
    """stage 0空格焦点(6,19)：避开单位/光标动画，检查菜单与确认框均已移除。"""
    from run_original_comparison import normalize

    expected, actual = normalize(expected), normalize(actual)
    for region in ((148, 80, 172, 96), (264, 168, 288, 184)):
        if expected.crop(region).tobytes() != actual.crop(region).tobytes():
            raise AssertionError(f"confirmation did not restore browse pixels in {region}")


def check_b6_comparison(path, scenario):
    """exit0的采集器报告仍须三场核心帧/声音/cleanup门全部为真。"""
    raw = path.read_bytes()
    report = json.loads(raw)
    if not isinstance(report, dict):
        raise AssertionError("B6 comparison不是对象")
    core = report.get("core_verification", {})
    pair = report.get("independent_frame_pairing", {})
    sound = report.get("core_sfx_observations", {})
    if not all(isinstance(value, dict) for value in (core, pair, sound)):
        raise AssertionError("B6 comparison缺少核心校验对象")
    sfx = core.get("sfx_passed", {})
    if not isinstance(sfx, dict) or not all(isinstance(sound.get(role), dict) for role in ("original", "sdl")):
        raise AssertionError("B6 comparison缺少双端声音结果")
    gates = {"ok": report.get("ok") is True, "status": report.get("status") == "passed",
        "scenario": report.get("scenario") == scenario,
        "verified": report.get("verify_core_requested") is True,
        "not_collection_only": report.get("collected_only") is False,
        "core": core.get("ok") is True,
        "cleanup_and_guards": core.get("collection_guards_cleanup_passed") is True,
        "frame": core.get("frame_passed") is True and pair.get("ok") is True}
    for role in ("original", "sdl"):
        gates["sfx_" + role] = sfx.get(role) is True and sound[role].get("minimum_core_sfx_supported") is True
    if not all(gates.values()):
        raise AssertionError("B6 comparison未通过：" + ", ".join(name for name, passed in gates.items() if not passed))
    return hashlib.sha256(raw).hexdigest()


class Suite:
    def __init__(self, args):
        if sum(bool(getattr(args, name, False)) for name in ("b3_only", "b4_only", "b5_only", "b6_only")) > 1:
            raise ValueError("--b3-only、--b4-only、--b5-only与--b6-only不能同时指定；未创建产物")
        requested = getattr(args, "b3_scenarios", None)
        if requested is not None:
            if not getattr(args, "b3_only", False) or not requested or len(requested) != len(set(requested)) or any(name not in B3_SCENARIOS for name in requested):
                raise ValueError("--b3-scenarios要求--b3-only且场景非空、无重复、全部已登记；未创建产物")
        requested_b4 = getattr(args, "b4_scenarios", None)
        if requested_b4 is not None:
            if not getattr(args, "b4_only", False) or not requested_b4 or len(requested_b4) != len(set(requested_b4)) or any(name not in B4_SCENARIOS for name in requested_b4):
                raise ValueError("--b4-scenarios要求--b4-only且场景非空、无重复、全部已登记；未创建产物")
        requested_b5 = getattr(args, "b5_scenarios", None)
        if requested_b5 is not None:
            if not getattr(args, "b5_only", False) or not requested_b5 or len(requested_b5) != len(set(requested_b5)) or any(name not in B5_SCENARIOS for name in requested_b5):
                raise ValueError("--b5-scenarios要求--b5-only且场景非空、无重复、全部已登记；未创建产物")
        requested_b6 = getattr(args, "b6_scenarios", None)
        if requested_b6 is not None:
            if not getattr(args, "b6_only", False) or not requested_b6 or len(requested_b6) != len(set(requested_b6)) or any(name not in B6_SCENARIOS for name in requested_b6):
                raise ValueError("--b6-scenarios要求--b6-only且场景非空、无重复、全部已登记；未创建产物")
        self.args = args
        self.work = args.build_dir / "graphics-artifacts" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        original = (ROOT / "original_game").resolve()
        target = self.work.resolve()
        if target == original or original in target.parents:
            raise ValueError("测试产物不能写入 original_game/，请改用独立构建目录")
        self.stage1_fixture = None
        self.stage1_fixture_source = None
        self.planner = None
        if not any(getattr(args, name, False) for name in ("b3_only", "b4_only", "b6_only")):
            selected = requested_b5 or B5_SCENARIOS
            stage1_positions = [selected.index(name) for name in B5_STAGE1_SCENARIOS if name in selected]
            if stage1_positions:
                explicit = getattr(args, "b5_save_fixture", None)
                if explicit is not None:
                    fixture = Path(explicit).resolve()
                    if not fixture.is_file() or fixture == target or target in fixture.parents:
                        raise ValueError("--b5-save-fixture必须是本轮输出目录外已存在的独立只读SAV；未创建产物")
                    self.stage1_fixture, self.stage1_fixture_source = fixture, "explicit_readonly"
                elif "prepare-controlled" in selected and selected.index("prepare-controlled") < min(stage1_positions):
                    self.stage1_fixture = self.work / "original-prepare-controlled/original/stage1-entry-measured.SAV"
                    self.stage1_fixture_source = "fresh_prepare_controlled_original"
                else:
                    raise ValueError("stage1场景需要先运行prepare-controlled，或显式--b5-save-fixture；未创建产物")
            if "natural-stage0" in selected:
                planner = getattr(args, "planner", None)
                if planner is None:
                    multi = args.build_dir / getattr(args, "config", "Debug") / "fd2_field_natural_plan.exe"
                    planner = multi if multi.is_file() else args.build_dir / "fd2_field_natural_plan.exe"
                self.planner = Path(planner).resolve()
                if not self.planner.is_file():
                    raise ValueError("自然首关规划器不存在；先构建fd2_field_natural_plan或用--planner指定；未创建产物")
        self.audio_recorder = None
        if sys.platform == "win32" and not any(getattr(args, name, False) for name in ("b3_only", "b4_only", "b5_only")):
            recorder = getattr(args, "audio_recorder", None)
            self.audio_recorder = (Path(recorder) if recorder is not None else
                Path(args.driver).parent / "fd2_process_audio_capture.exe").resolve()
            if not self.audio_recorder.is_file():
                raise ValueError("B6进程录音工具不存在；先构建fd2_process_audio_capture或用--audio-recorder指定；未创建产物")
        self.work.mkdir(parents=True)
        self.report = {"status": "failed", "platform": sys.platform, "cases": [],
                       "capture": "Windows.Graphics.Capture HWND (SDL and DOSBox)",
                       "processes": [], "scope": "SDL host regression and listed original checkpoints"}
        if self.stage1_fixture is not None:
            self.report["b5_stage1_fixture"] = {"path": str(self.stage1_fixture), "source": self.stage1_fixture_source}
        if self.planner is not None:
            self.report["b5_planner"] = {"path": str(self.planner), "sha256": hashlib.sha256(self.planner.read_bytes()).hexdigest()}
        if self.audio_recorder is not None:
            self.report["b6_audio_recorder"] = {"path": str(self.audio_recorder),
                "sha256": hashlib.sha256(self.audio_recorder.read_bytes()).hexdigest()}
        self.trace = self.work / "input-trace.jsonl"

    def record(self, **event):
        with self.trace.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps({"time": time.monotonic(), **event}) + "\n")

    def command(self, name, command, timeout=240):
        log = self.work / f"{name}.log"
        command = list(map(str, command))
        outer_timeout = timeout
        receipt = {"name": name, "status": "failed", "log": str(log)}
        if sys.platform == "win32":
            command = [str(self.args.driver), "run-job", str(timeout * 1000), *command]
            outer_timeout += 15  # Job先正常关闭5秒、必要时终止并等待5秒。
            receipt["process_tree_owner"] = "Windows Job; suspended assign before resume; kill on close"
        self.report["cases"].append(receipt)
        with log.open("wb") as output:
            result = subprocess.run(command, cwd=ROOT, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=outer_timeout)
        if result.returncode:
            raise RuntimeError(f"{name} exited {result.returncode}; see {log}")
        receipt["status"] = "passed"
        return receipt

    def driver(self, process, command, *extra):
        if process.poll() is not None:
            raise RuntimeError(f"test process {process.pid} already exited {process.returncode}")
        self.record(pid=process.pid, command=command, args=list(map(str, extra)))
        subprocess.run([str(self.args.driver), command, str(process.pid), *map(str, extra)],
                       check=True, timeout=15)

    def key(self, process, *keys):
        from run_original_comparison import Capture
        capture = Capture(self.args.driver, process, self.work)
        for key in keys:
            capture.key(key)

    def capture(self, process, work, name):
        path = work / f"{name}.bmp"
        self.driver(process, "capture", path)
        check_bitmap(path)
        return path

    @contextmanager
    def window(self, name, arguments, cwd=ROOT, save=True, expected_exit=0, save_fixture=None):
        work = self.work / name
        work.mkdir()
        save_path = work / "FD2.SAV"
        if save:
            shutil.copyfile(save_fixture or ROOT / "original_game/FD2.SAV", save_path)
        env = os.environ.copy()
        env.update(SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="windows",
                   FD2SDL_SAVE_PATH=str(save_path), FD2SDL_FIELD_RNG_SEED="31256",
                   SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0", SDL_WINDOW_ACTIVATE_WHEN_RAISED="0",
                   SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS="0", SDL_WINDOWS_RAW_KEYBOARD="0")
        log = work / "game.log"
        with log.open("wb") as output:
            process = subprocess.Popen([str(self.args.game), *arguments], cwd=cwd,
                                       env=env, stdout=output, stderr=subprocess.STDOUT)
            receipt = {"name": name, "pid": process.pid, "cleaned": False,
                       "forced_cleanup": False, "expected_exit": expected_exit}
            self.report["processes"].append(receipt)
            body_failed = False
            try:
                yield process, work, log, save_path
            except BaseException as exc:
                body_failed = True
                receipt["body_error"] = str(exc)
                raise
            finally:
                cleanup_error = None
                try:
                    if process.poll() is None:
                        try:
                            self.driver(process, "close")
                            process.wait(timeout=5)
                        except (subprocess.SubprocessError, OSError, RuntimeError):
                            receipt["forced_cleanup"] = True
                            process.terminate()
                            try:
                                process.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                process.kill()
                                process.wait(timeout=5)
                except Exception as exc:
                    cleanup_error = exc
                    receipt["cleanup_error"] = str(exc)
                receipt.update(cleaned=process.poll() is not None, exit_code=process.returncode)
                if not body_failed:
                    if cleanup_error or not receipt["cleaned"]:
                        raise RuntimeError(f"test process not cleaned: {process.pid}") from cleanup_error
                    if receipt["forced_cleanup"]:
                        raise RuntimeError(f"test process required forced cleanup: {process.pid}")
                    if process.returncode != expected_exit:
                        raise RuntimeError(f"test process {process.pid} exited {process.returncode}, expected {expected_exit}")

    def host_windows(self):
        # 每个正式入口使用独立进程；预览不充当输入状态机证据。
        for name, arguments, marker in (
                ("map", ["--map-preview", "0"], None),
                ("field-preview", ["--field-preview", "0"], None),
                ("dialog-close", ["--prologue-preview"], None),
                ("handoff-close", ["--new-game-play"], None)):
            with self.window(name, arguments) as (process, work, log, _):
                if marker:
                    wait_log(process, log, marker)
                else:
                    # 用窗口帧等待资源初始化，不向剧情盲发确认键。
                    deadline = time.monotonic() + 20
                    while True:
                        if process.poll() is not None:
                            raise RuntimeError(f"{name} exited before capture; see {log}")
                        try:
                            self.capture(process, work, "active")
                            break
                        except (subprocess.SubprocessError, ValueError):
                            if time.monotonic() >= deadline:
                                raise
                            time.sleep(0.2)
                self.capture(process, work, "before-close")
                self.driver(process, "close")
                if process.wait(timeout=10) != 0:
                    raise RuntimeError(f"{name} did not close normally; see {log}")
            self.report["cases"].append({"name": name, "status": "passed"})

    def battle_probe(self, process, work):
        """CLI stage0的真实初位是unit0(7,20)，空格(6,20)屏坐标为(148,124)。"""
        from run_original_comparison import Capture
        from run_original_end_turn import RoundOracle, round_assets
        from run_original_battle_menu import wait_state

        capture = Capture(self.args.driver, process, work)
        capture.key = lambda key: self.key(process, key)
        assets = round_assets(ROOT / "original_game")
        oracle = RoundOracle(assets, (148, 124))
        checkpoints = self.report.setdefault("resource_checkpoints", {}).setdefault(work.name, [])

        def checkpoint(name, predicate, **kwargs):
            return wait_state(capture, name, predicate, checkpoints, **kwargs)
        return capture, oracle, checkpoint

    def confirmation(self, capture, oracle, checkpoint, name):
        from run_original_battle_menu import save_checkpoint
        checkpoints = self.report.setdefault("resource_checkpoints", {}).setdefault(capture.artifacts.name, [])
        first_seen = False

        def selected(frame):
            nonlocal first_seen
            detail = oracle.buttons(frame)
            if not detail:
                return None
            if not first_seen:
                save_checkpoint(capture, name + "-first-complete-buttons", frame, detail, checkpoints)
                first_seen = True
            no = oracle.buttons(frame, 1)
            if no:
                save_checkpoint(capture, name + "-unexpected-no-buttons", frame, no, checkpoints)
                raise AssertionError(f"{name}: explicit No observed; original frame retained")
            # 48/51是合法blink-off；在本次15秒等待内继续，不能推定YES或重开期限。
            return oracle.buttons(frame, 0)

        checkpoint(name + "-first-selected-buttons", selected)

    def storage_confirmation(self, capture, oracle, checkpoint, name, selected,
                             *, load_available):
        capture.key("ENTER")
        checkpoint(name + "-parent", lambda frame: oracle.menu(frame, 0))
        capture.key("ENTER")
        checkpoint(name + "-secondary", lambda frame: oracle.secondary(
            frame, 0, load_available=load_available))
        capture.key({1: "LEFT", 2: "RIGHT", 3: "DOWN"}[selected])
        checkpoint(name + "-selected", lambda frame: oracle.secondary(
            frame, selected, load_available=load_available))
        capture.key("ENTER")
        self.confirmation(capture, oracle, checkpoint, name)

    def first_save(self):
        with self.window("first-save", ["--field-play", "0"], save=False) as (process, work, log, save):
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            capture, oracle, checkpoint = self.battle_probe(process, work)
            if save.exists():
                raise AssertionError("first-save must start without a save file")
            capture.key("LEFT")
            checkpoint("first-save-empty-browse", oracle.closed_browse)
            self.storage_confirmation(capture, oracle, checkpoint, "first-save", 1,
                                      load_available=False)
            if save.exists():
                raise AssertionError("opening Save confirmation created a file")
            capture.key("ENTER")
            deadline = time.monotonic() + 15
            while not save.exists():
                if time.monotonic() >= deadline:
                    raise TimeoutError("first Save did not create its isolated file")
                time.sleep(0.1)
            plain = decode_save(save)
            from run_original_comparison import archive_entry
            metadata = archive_entry((ROOT / "original_game/FDFIELD.DAT").read_bytes(), 1)
            if plain[:len(metadata)] != metadata or plain[0x30c3:0x30c6] != bytes((1, 12, 0)):
                raise AssertionError("first Save did not contain the stage 0 session")
            roster_count = check_roster_count(plain)
            if plain[0x312b:-4] != b"\xff" * (len(plain) - 4 - 0x312b):
                raise AssertionError("first Save altered empty manual slots")
            checkpoint("first-save-created-browse", oracle.closed_browse)
        self.report["cases"].append({"name": "first-save", "status": "passed",
                                     "started_without_file": True, "roster_count": roster_count})

    def storage_and_cancel(self):
        from run_original_end_turn import META, UNITS, RoundOracle, browse_anywhere
        from run_original_load import wait_measured_save
        from run_original_player import PlayerOracle, exact_tile, sheet_frame, upper_command_composite
        from run_original_battle_menu import MENU_OFFSETS
        from run_original_result import selection_probe
        from fd2_natural_oracle import selection_summary

        with self.window("storage-cancel", ["--field-play", "0"]) as (process, work, log, save):
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            capture, oracle, checkpoint = self.battle_probe(process, work)
            original = save.read_bytes()
            original_mtime = save.stat().st_mtime_ns
            icons = (ROOT / "original_game/FDICON.B24").read_bytes()
            sprites = [sheet_frame(icons, phase) for phase in range(12)]
            for sprite in sprites:
                sprite.putpalette(oracle.assets["palette"])
            command_assets = dict(oracle.assets, upper_commands={index: [
                upper_command_composite(oracle.assets["commands"][index], sprite) for sprite in sprites]
                for index in range(3)})
            # CLI初始Hero与真实START的姓名/LV1/HP42/MP0相同；这里只映射已知初位，
            # 不把START敌军站位或完整80B当CLI会话预期。
            summary_source = bytearray(decode_save(ROOT / "original_game/START.SAV"))
            summary_source[META + 3:META + 5] = bytes((0, 15))
            summary_source[UNITS:UNITS + 2] = bytes((7, 20))
            summary = selection_summary(ROOT / "original_game", summary_source, oracle.assets, 0)
            probe = selection_probe(ROOT / "original_game", summary_source, 0, oracle.assets)
            if tuple(probe["world"]) != (6, 20):
                raise ValueError("storage-cancel需要已确认空格6,20的独立移动范围资源")
            command_assets["probe_base_tile"] = probe["base"]
            player = PlayerOracle(summary_source, command_assets, probe["ranges"], {
                "leader_screen": [172, 124], "leader_world": [7, 20],
                "destination_screen": list(probe["position"]), "destination_world": list(probe["world"])})

            def leader(frame):
                found = browse_anywhere(frame, oracle.assets, leader=True)
                return found if found and found["focus_screen"] == [172, 124] else None

            def selected_summary(frame):
                focused = player.selection(frame)
                panel = exact_tile(frame, summary["image"], summary["position"])
                return {"kind": "leader_selection_summary", "leader": focused, "panel": panel} if focused and panel else None

            capture.key("Z")
            checkpoint("cancel-leader-browse", leader)
            capture.key("ENTER")
            checkpoint("cancel-first-selection", selected_summary)
            capture.key("DOWN")
            destination = RoundOracle(oracle.assets, (172, 148))
            checkpoint("cancel-move-focus", destination.browse)
            capture.key("ENTER")
            checkpoint("command-before-cancel", lambda frame: player.command(frame, (172, 148)))
            # 取消移动后菜单，再次选择同一actor：不能提交acted。
            capture.key("ESC")
            checkpoint("cancel-return-selection", player.selection)
            capture.key("ESC")
            checkpoint("cancel-return-browse", player.browse)
            capture.key("ENTER")
            checkpoint("cancel-reselected-same-actor", selected_summary)
            text = log.read_text(encoding="utf-8", errors="replace")
            if "field action:" in text or save.read_bytes() != original or save.stat().st_mtime_ns != original_mtime:
                raise AssertionError("cancel committed action or changed save")
            capture.key("ESC")
            checkpoint("cancel-final-browse", player.browse)
            # 焦点已回到unit 0 (7,20)，左邻格为空，进入parent/secondary。
            capture.key("LEFT")
            checkpoint("storage-empty-browse", oracle.closed_browse)
            # 本套件原FD2.SAV活动stage29不在正式Load范围；Save后才可Load。
            self.storage_confirmation(capture, oracle, checkpoint, "cancel-save", 1, load_available=False)
            # 确认框默认左侧Yes；Esc取消必须逐字保留存档。
            capture.key("ESC")
            checkpoint("cancel-save-browse", oracle.closed_browse)
            if save.read_bytes() != original or save.stat().st_mtime_ns != original_mtime:
                raise AssertionError("cancelled save changed the file")
            self.storage_confirmation(capture, oracle, checkpoint, "write-save", 1, load_available=False)
            capture.key("ENTER")
            plain = wait_measured_save(save, original, original_mtime)
            checkpoint("saved-browse", oracle.closed_browse)
            original_plain_path = work / "before-save.SAV"
            original_plain_path.write_bytes(original)
            before_plain = decode_save(original_plain_path)
            from run_original_comparison import archive_entry
            stage_metadata = archive_entry((ROOT / "original_game/FDFIELD.DAT").read_bytes(), 1)
            if plain[:len(stage_metadata)] != stage_metadata:
                raise AssertionError("Save retained another stage's metadata/shape instead of stage 0")
            roster_count = check_storage_preservation(before_plain, plain, len(stage_metadata))
            meta = plain[0x30c3:0x312b]
            if meta[2] != 0 or meta[0] != 1 or not 0 < meta[1] <= 96:
                raise AssertionError("saved snapshot stage/turn/unit count mismatch")
            saved = save.read_bytes()
            saved_mtime = save.stat().st_mtime_ns
            # 确认成功/取消均回browse；结果文字只是短时覆盖层。
            # 配置立即提交：先证明music确实改变，再由Load恢复原快照的配置。
            capture.key("ENTER")
            checkpoint("options-parent", lambda frame: oracle.menu(frame, 0))
            capture.key("RIGHT")
            checkpoint("options-selected", lambda frame: oracle.menu(frame, 2))
            capture.key("ENTER")

            def options(frame, values):
                matches = []
                for index, command in enumerate((18 + (not values[0]), 20 + (not values[1]),
                                                  22 + values[2], 24 + (not values[3]))):
                    at = tuple(a + b for a, b in zip(oracle.focus, MENU_OFFSETS[index]))
                    if index >= 2:  # 原disabled帧全透明；两项仍不可选，不冒充已开放。
                        if oracle.choices(frame, (command * 3, command * 3 + 1), at):
                            return None
                        matches.append({"item": index, "disabled_transparent_resource": True})
                    else:
                        found = oracle.choices(frame, (command * 3 + (index == 0),), at)
                        if not found:
                            return None
                        matches.append(found)
                return {"kind": "options", "selected": 0, "matches": matches}

            values = list(meta[16:18]) + list(meta[14:16])
            checkpoint("options-before-toggle", lambda frame: options(frame, values))
            capture.key("ENTER")
            wait_log(process, log, f"field option: index=0 value={1 - meta[16]}")
            values[0] ^= 1
            checkpoint("options-changed", lambda frame: options(frame, values))
            capture.key("ESC")
            checkpoint("options-return-parent", lambda frame: oracle.menu(frame, 0))
            capture.key("ESC")
            checkpoint("options-return-browse", oracle.closed_browse)
            capture.key("RIGHT")
            checkpoint("load-navigate-leader", leader)
            capture.key("RIGHT")
            load_oracle = RoundOracle(oracle.assets, (196, 124))
            checkpoint("load-offset-browse", load_oracle.closed_browse)
            self.storage_confirmation(capture, load_oracle, checkpoint, "load", 2, load_available=True)
            capture.key("ENTER")
            checkpoint("loaded-source-browse", oracle.closed_browse)
            if save.read_bytes() != saved or save.stat().st_mtime_ns != saved_mtime:
                raise AssertionError("Load unexpectedly rewrote its source save")
            # 再次保存恢复后的快照，位置/单位/配置必须与加载源一致。
            self.storage_confirmation(capture, oracle, checkpoint, "restored-save", 1, load_available=True)
            capture.key("ENTER")
            wait_measured_save(save, saved, saved_mtime)
            checkpoint("restored-save-browse", oracle.closed_browse)
            if save.read_bytes() != saved:
                raise AssertionError("Load/Save did not restore identical snapshot bytes")
            text = log.read_text(encoding="utf-8", errors="replace")
            if "field save failed:" in text or "field load failed:" in text:
                raise AssertionError(f"storage reported failure; see {log}")
            # 恢复后必须能继续真实行动；不能只验证codec往返或停在菜单中。
            capture.key("RIGHT")
            checkpoint("continued-leader", leader)
            capture.key("ENTER")
            checkpoint("continued-selection", selected_summary)
            capture.key("ENTER")
            checkpoint("continued-command", lambda frame: player.command(frame, (172, 124)))
            capture.key("DOWN")
            checkpoint("continued-wait-selected", lambda frame: player.command(frame, (172, 124), 3))
            capture.key("ENTER")
            wait_log(process, log, "field action: unit=0 command=wait acted=1 remaining=3 phase=2")
            if save.read_bytes() != saved:
                raise AssertionError("continuing after Load unexpectedly rewrote the save")
            self.capture(process, work, "continued-wait")
        self.report["cases"].append({"name": "storage-cancel", "status": "passed",
                                     "configuration_restored": True, "continued_action": "unit 0 wait",
                                     "roster_count": roster_count})

    def end_turn(self):
        from run_original_battle_menu import restored_browse

        with self.window("end-turn", ["--field-play", "0"]) as (process, work, log, save):
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            capture, oracle, checkpoint = self.battle_probe(process, work)
            original = save.read_bytes()
            original_mtime = save.stat().st_mtime_ns
            capture.key("LEFT")
            browse_image, _ = checkpoint("end-empty-browse", oracle.closed_browse)

            def open_end(name):
                capture.key("ENTER")
                checkpoint(name + "-parent", lambda frame: oracle.menu(frame, 0))
                capture.key("DOWN")
                checkpoint(name + "-end-selected", lambda frame: oracle.menu(frame, 3))
                capture.key("ENTER")
                self.confirmation(capture, oracle, checkpoint, name)

            def restored(name):
                checkpoint(name, lambda frame: restored_browse(frame, browse_image, oracle),
                           transition=oracle.cancellation_notice)

            # 原初位在unit0；先空格资源ACK，再验证Esc/No都不提交。
            open_end("end-cancel")
            buttons = check_confirmation_buttons(capture.last, ROOT / "original_game/FDOTHER.DAT")
            capture.key("ESC")
            restored("end-cancelled-browse")
            cancelled = log.read_text(encoding="utf-8", errors="replace")
            if any(marker in cancelled for marker in ("field end turn:", "field phase:", "field action:")):
                raise AssertionError("cancelled end-turn confirmation changed battle state")
            if save.read_bytes() != original or save.stat().st_mtime_ns != original_mtime:
                raise AssertionError("cancelled end-turn confirmation changed the save")
            # Right选No；Up/Down被确认框忽略，不能把No切回Yes。
            for name, ignored in (("no-selected", None), ("no-up-ignored", "UP"),
                                  ("no-down-ignored", "DOWN")):
                # 每个确认框返回browse，下一次必须从parent重新进入。
                open_end(name)
                capture.key("RIGHT")
                checkpoint(name + "-no", lambda frame: oracle.buttons(frame, 1))
                if ignored:
                    capture.key(ignored)
                    checkpoint(name + "-still-no", lambda frame: oracle.buttons(frame, 1))
                capture.key("ENTER")
                restored(name + "-browse")
                rejected = log.read_text(encoding="utf-8", errors="replace")
                if any(marker in rejected for marker in ("field end turn:", "field phase:", "field action:")):
                    raise AssertionError(f"{name} end-turn confirmation changed battle state")
                if save.read_bytes() != original or save.stat().st_mtime_ns != original_mtime:
                    raise AssertionError(f"{name} end-turn confirmation changed the save")
            # 重新打开后默认Yes，直接Enter只能提交一次。
            open_end("end-submit")
            capture.key("ENTER")
            text = wait_log(process, log, "field phase: turn=2 phase=2 remaining=4")
            check_end_turn_log(text)
            self.capture(process, work, "next-player-round")
            # 保持一个输入脉冲只提交一次，不能自动再结束刚进入的玩家回合。
            time.sleep(0.6)
            check_end_turn_log(log.read_text(encoding="utf-8", errors="replace"))
            if save.read_bytes() != original or save.stat().st_mtime_ns != original_mtime:
                raise AssertionError("end-turn unexpectedly wrote a save")
        self.report["cases"].append({"name": "end-turn", "status": "passed",
                                     "cancelled_before_commit": True, "no_before_commit": True,
                                     "default_yes": True, "browse_restored": True,
                                     "round": 2, "confirmation_buttons": buttons})

    def main_title_routes(self):
        """标题手工槽取消与活动Continue/离场；胜败后续由B5配对流程验收。"""
        from run_original_comparison import Capture, title_templates
        from run_original_battle_menu import wait_state
        from fd2_transition_oracle import resources, picker_image, picker_match

        source = self.work / "storage-cancel/FD2.SAV"
        plain = decode_save(source)
        title = title_templates(ROOT / "original_game/FDOTHER.DAT")[3][0]
        picker = picker_image(resources(ROOT / "original_game"), plain, 0)
        checkpoints = []
        with self.window("main-leave", [], expected_exit=0,
                         save_fixture=source) as (process, work, log, save):
            original = save.read_bytes()
            capture = Capture(self.args.driver, process, work)
            capture.key = lambda key: self.key(process, key)
            capture.wait(title, startup=True, timeout=45).save(work / "title-before-load.png")
            self.key(process, "DOWN", "ENTER")
            wait_state(capture, "title-manual-picker", lambda frame: picker_match(frame, picker),
                       checkpoints, allow_low_color_resource=True)
            self.key(process, "ESC")
            capture.wait(title, blank_transition=True, timeout=20).save(work / "title-after-picker-cancel.png")
            if save.read_bytes() != original:
                raise AssertionError("cancelled manual Load changed the save")
            # 三项标题从START向上回绕到CONTINUE；LOAD是独立手工槽入口。
            self.key(process, "UP", "ENTER")
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            capture, oracle, checkpoint = self.battle_probe(process, work)
            if plain[0x30c6:0x30cc] != bytes((0, 15, 6, 20, 6, 5)):
                raise AssertionError("main-leave source must preserve the measured empty focus/camera")
            checkpoint("loaded-battle-empty-browse", oracle.closed_browse)
            # 保存的焦点(6,20)为空，parent→secondary→leave→Yes。
            self.storage_confirmation(capture, oracle, checkpoint, "leave", 3, load_available=True)
            capture.key("ENTER")
            wait_log(process, log, "field leave: exiting game")
            if process.wait(timeout=20) != 0:
                raise AssertionError("game leave did not naturally exit 0")
            text = log.read_text(encoding="utf-8", errors="replace")
            if text.count("field play: stage=0") != 1:
                raise AssertionError("game leave unexpectedly reentered battle")
            if save.read_bytes() != original:
                raise AssertionError("main leave route unexpectedly changed its save fixture")
        self.report["cases"].append({"name": "main-leave", "status": "passed",
                                     "checkpoints": checkpoints, "expected_exit": 0,
                                     "scope": "SDL title manual-picker cancel, Continue and leave; not original acceptance"})

    def item_cases(self):
        """B3本阶段功能对照；也供大阶段完整套件调用，不借用历史收据。"""
        for scenario in getattr(self.args, "b3_scenarios", None) or B3_SCENARIOS:
            name = "original-item-" + scenario
            self.command(name, [sys.executable, ROOT / "tools/run_original_item_list.py",
                         "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--scenario", scenario,
                         "--artifacts", self.work / name], timeout=480)

    def reward_cases(self):
        """B4本阶段奖励/成长/状态对照；登记不代表已执行或通过。"""
        for scenario in getattr(self.args, "b4_scenarios", None) or B4_SCENARIOS:
            name = "original-reward-" + scenario
            self.command(name, [sys.executable, ROOT / "tools/run_original_reward.py",
                         "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--scenario", scenario,
                         "--artifacts", self.work / name], timeout=900)

    def transition_cases(self, *, include_defeat=True):
        """B5已登记入口；完整套件的original-result已执行正式败北，不能重复。"""
        self.report["registered_b5_scenarios"] = list(B5_SCENARIOS)
        for scenario in getattr(self.args, "b5_scenarios", None) or B5_SCENARIOS:
            if scenario == "defeat":
                if include_defeat:
                    self.command("original-result", [sys.executable, ROOT / "tools/run_original_result.py",
                        "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                        "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-result",
                        "--b5-defeat"], timeout=360)
                continue
            if scenario == "prepare-controlled":
                name, script, timeout = "original-prepare-controlled", "run_original_transition.py", 1200
            elif scenario == "natural-stage0":
                name = "original-natural-stage0"
                self.command(name, [sys.executable, ROOT / "tools/run_original_natural.py",
                    "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                    "--dosbox", self.args.dosbox, "--planner", self.planner,
                    "--scenario", scenario, "--record-native-audio", "--artifacts", self.work / name], timeout=7200)
                continue
            elif scenario in B5_STAGE1_SCENARIOS:
                name = "original-" + scenario
                self.command(name, [sys.executable, ROOT / "tools/run_original_stage1.py",
                    "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                    "--dosbox", self.args.dosbox, "--save-fixture", self.stage1_fixture,
                    "--scenario", scenario.removeprefix("stage1-"), "--artifacts", self.work / name],
                    timeout=600 if scenario == "stage1-turn3" else 240)
                continue
            else:
                name, script, timeout = "original-loot-" + scenario.removeprefix("loot-"), "run_original_loot.py", 900
            self.command(name, [sys.executable, ROOT / "tools" / script,
                "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                "--dosbox", self.args.dosbox, "--scenario", scenario,
                "--artifacts", self.work / name], timeout=timeout)

    def battle_animation_cases(self):
        """B6最低三场正式配对；未匹配帧/声音或晚守卫失败不能由exit0冒充通过。"""
        self.report["registered_b6_scenarios"] = list(B6_SCENARIOS)
        for scenario in getattr(self.args, "b6_scenarios", None) or B6_SCENARIOS:
            name = "original-battle-animation-" + scenario
            artifacts = self.work / name
            receipt = self.command(name, [sys.executable, ROOT / "tools/run_original_battle_animation.py",
                "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                "--dosbox", self.args.dosbox, "--audio-recorder", self.audio_recorder,
                "--scenario", scenario, "--verify-core", "--artifacts", artifacts], timeout=600)
            try:
                digest = check_b6_comparison(artifacts / "comparison.json", scenario)
            except (OSError, ValueError, AssertionError) as error:
                receipt.update(status="failed", error=str(error))
                raise
            receipt.update(comparison=str(artifacts / "comparison.json"), comparison_sha256=digest,
                core_verification_passed=True)

    def run(self):
        before = hashes(ROOT / "original_game")
        self.report["original_hashes"] = before
        try:
            if getattr(self.args, "b6_only", False):
                if sys.platform != "win32":
                    raise RuntimeError("B6核心原版/SDL帧声音配对当前要求Windows WGC与进程录音；未运行不能记为通过")
                self.report["scope"] = "B6 three core physical/player0/heal13 full-frame and minimum SFX pairs only; no historical rerun; not full FX/A7 fidelity"
                self.battle_animation_cases()
                self.report["status"] = "passed"
                return
            if getattr(self.args, "b5_only", False):
                if sys.platform != "win32":
                    raise RuntimeError("B5原版/SDL对照当前要求Windows WGC；没有运行或跳过为通过")
                self.report["scope"] = "B5 preparation, defeat, loot, stage1 and natural stage0 only; no historical suite rerun or inheritance; native audio is A5 identity/switch evidence, not A7 fidelity"
                self.transition_cases()
                self.report["status"] = "passed"
                return
            if getattr(self.args, "b4_only", False):
                if sys.platform != "win32":
                    raise RuntimeError("B4原版/SDL对照当前要求Windows WGC；没有运行或跳过为通过")
                self.report["scope"] = "B4 changed/new reward, growth, learning, status and save/load only; no historical suite rerun or inheritance"
                self.reward_cases()
                self.report["status"] = "passed"
                return
            if getattr(self.args, "b3_only", False):
                if sys.platform != "win32":
                    raise RuntimeError("B3原版/SDL对照当前要求Windows WGC；没有运行或跳过为通过")
                self.report["scope"] = "B3 changed/new item features only; historical full suite not rerun or inherited"
                self.item_cases()
                self.report["status"] = "passed"
                return
            if sys.platform == "win32":
                # 自然站位与边缘镜头是后续菜单/动作截图的共同前置。
                self.command("original-opening", [sys.executable, ROOT / "tools/run_original_opening.py",
                             "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                             "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-opening"], timeout=1500)
                self.command("original-focus", [sys.executable, ROOT / "tools/run_original_focus.py",
                             "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                             "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-focus"], timeout=600)
                self.command("background-input", [sys.executable, ROOT / "tools/run_background_input_check.py",
                             "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                             "--dosbox", self.args.dosbox, "--artifacts", self.work / "background-input"], timeout=600)
            self.command("input", ["ctest", "--test-dir", self.args.build_dir, "-C", self.args.config,
                                   "--output-on-failure", "--no-tests=error", "-R", "^field_play_input$"])
            if sys.platform != "win32":
                raise RuntimeError("full window/original suite currently requires Windows; Linux XTest result is in input.log")
            self.host_windows()
            self.first_save()
            self.storage_and_cancel()
            self.end_turn()
            self.main_title_routes()
            self.command("original", [sys.executable, ROOT / "tools/run_original_comparison.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original",
                         "--save-fixture", self.work / "storage-cancel/FD2.SAV"], timeout=300)
            self.command("original-battle-menu", [sys.executable, ROOT / "tools/run_original_battle_menu.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-battle-menu",
                         "--save-fixture", self.work / "storage-cancel/FD2.SAV"], timeout=300)
            self.command("original-end-turn", [sys.executable, ROOT / "tools/run_original_end_turn.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-end-turn"], timeout=300)
            self.command("original-load", [sys.executable, ROOT / "tools/run_original_load.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-load"], timeout=360)
            self.command("original-leave", [sys.executable, ROOT / "tools/run_original_leave.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-leave"], timeout=300)
            self.command("original-overview", [sys.executable, ROOT / "tools/run_original_overview.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-overview"], timeout=360)
            self.command("original-player", [sys.executable, ROOT / "tools/run_original_player.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-player"], timeout=360)
            self.command("original-player-wait", [sys.executable, ROOT / "tools/run_original_player.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--scenario", "wait-full-hp",
                         "--artifacts", self.work / "original-player-wait"], timeout=360)
            self.command("original-march", [sys.executable, ROOT / "tools/run_original_march.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-march"], timeout=480)
            self.command("original-f1", [sys.executable, ROOT / "tools/run_original_f1.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-f1"], timeout=480)
            self.command("original-result", [sys.executable, ROOT / "tools/run_original_result.py",
                         "--root", ROOT, "--game", self.args.game, "--driver", self.args.driver,
                         "--dosbox", self.args.dosbox, "--artifacts", self.work / "original-result",
                         "--b5-defeat"], timeout=360)
            for name, scenario in (("original-magic-list", "single"),
                                   ("original-magic-damage", "damage"),
                                   ("original-magic-heal", "heal")):
                self.command(name, [sys.executable, ROOT / "tools/run_original_magic_list.py",
                             "--root", ROOT, "--sdl", self.args.game, "--driver", self.args.driver,
                             "--dosbox", self.args.dosbox, "--scenario", scenario,
                             "--artifacts", self.work / name], timeout=480)
            self.item_cases()
            self.reward_cases()
            self.transition_cases(include_defeat=False)
            self.battle_animation_cases()
            self.report["status"] = "passed"
        finally:
            self.report["original_unchanged"] = hashes(ROOT / "original_game") == before
            if not self.report["original_unchanged"]:
                self.report["status"] = "failed"
                raise AssertionError("original_game changed during tests")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--driver", type=Path)
    parser.add_argument("--planner", type=Path, help="自然首关只读规划器；默认查build-dir/CONFIG或单配置构建目录")
    parser.add_argument("--dosbox", default=shutil.which("dosbox-x"))
    parser.add_argument("--audio-recorder", type=Path, help="B6进程录音工具；默认查driver同目录fd2_process_audio_capture.exe")
    parser.add_argument("--b3-only", action="store_true", help="仅运行本轮B3物品功能；历史完整实机回归留大阶段审查")
    parser.add_argument("--b3-scenarios", nargs="+", help="仅在--b3-only下选择已登记场景；未知/重复在创建产物前拒绝")
    parser.add_argument("--b4-only", action="store_true", help="仅运行本轮B4经验/成长/学习/状态与存档功能")
    parser.add_argument("--b4-scenarios", nargs="+", help="仅在--b4-only下选择已登记场景；未知/重复在创建产物前拒绝")
    parser.add_argument("--b5-only", action="store_true", help="仅运行本轮B5准备/败北/掉落/第二关/自然首关；不调用历史套件")
    parser.add_argument("--b5-scenarios", nargs="+", help="仅在--b5-only下选择已登记场景；未知/重复在创建产物前拒绝")
    parser.add_argument("--b5-save-fixture", type=Path, help="stage1子集无前置prepare时的独立只读实机SAV；默认复用本次prepare原版量测")
    parser.add_argument("--b6-only", action="store_true", help="仅运行B6核心攻击/施法/治疗三场正式帧声音配对")
    parser.add_argument("--b6-scenarios", nargs="+", help="仅在--b6-only下选择三场核心子集；未知/重复/空选择在创建产物前拒绝")
    args = parser.parse_args()
    args.build_dir, args.game = args.build_dir.resolve(), args.game.resolve()
    if not args.driver:
        multi = args.build_dir / args.config / "fd2_field_play_input_driver.exe"
        args.driver = multi if multi.exists() else args.build_dir / "fd2_field_play_input_driver.exe"
    args.driver = args.driver.resolve()
    # Scoop 的 shim 自身没有窗口；使用其声明的真实可执行文件 PID。
    if args.dosbox:
        shim = Path(args.dosbox).with_suffix(".shim")
        if shim.is_file():
            for line in shim.read_text(encoding="utf-8-sig").splitlines():
                if line.startswith('path = "') and line.endswith('"'):
                    args.dosbox = line[8:-1]
                    break
    if sys.platform == "win32" and not args.dosbox:
        parser.error("DOSBox-X is required; pass --dosbox PATH")
    try:
        suite = Suite(args)
    except ValueError as exc:
        parser.error(str(exc))
    try:
        suite.run()
    except (OSError, RuntimeError, ValueError, AssertionError, subprocess.SubprocessError) as exc:
        suite.report.update(status="failed", error=str(exc))
        print(f"图形回归未通过：{exc}。查看 {suite.work / 'summary.json'} 后重跑。", file=sys.stderr)
    finally:
        (suite.work / "summary.json").write_text(
            json.dumps(suite.report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(suite.work)
    return 0 if suite.report["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
