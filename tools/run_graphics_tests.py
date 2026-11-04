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
    """合成 SDL 终态夹具：只改已确认的隐藏位和checksum，不算原版行为证据。"""
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


class Suite:
    def __init__(self, args):
        self.args = args
        self.work = args.build_dir / "graphics-artifacts" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        original = (ROOT / "original_game").resolve()
        target = self.work.resolve()
        if target == original or original in target.parents:
            raise ValueError("测试产物不能写入 original_game/，请改用独立构建目录")
        self.work.mkdir(parents=True)
        self.report = {"status": "failed", "platform": sys.platform, "cases": [],
                       "capture": "Windows.Graphics.Capture HWND (SDL and DOSBox)",
                       "processes": [], "scope": "SDL host regression and listed original checkpoints"}
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

    def driver(self, process, command, *extra):
        if process.poll() is not None:
            raise RuntimeError(f"test process {process.pid} already exited {process.returncode}")
        self.record(pid=process.pid, command=command, args=list(map(str, extra)))
        subprocess.run([str(self.args.driver), command, str(process.pid), *map(str, extra)],
                       check=True, timeout=15)

    def key(self, process, *keys):
        self.driver(process, "focus")
        for key in keys:
            self.driver(process, "key", key)

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
                   FD2SDL_SAVE_PATH=str(save_path), FD2SDL_FIELD_RNG_SEED="31256")
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

    def first_save(self):
        with self.window("first-save", ["--field-play", "0"], save=False) as (process, work, log, save):
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            if save.exists():
                raise AssertionError("first-save must start without a save file")
            self.key(process, "ENTER", "UP", "ENTER", "LEFT", "ENTER")
            self.capture(process, work, "confirmation")
            if save.exists():
                raise AssertionError("opening Save confirmation created a file")
            self.key(process, "LEFT", "ENTER")
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
            self.capture(process, work, "created")
        self.report["cases"].append({"name": "first-save", "status": "passed",
                                     "started_without_file": True, "roster_count": roster_count})

    def storage_and_cancel(self):
        with self.window("storage-cancel", ["--field-play", "0"]) as (process, work, log, save):
            wait_log(process, log, "field play: stage=0")
            original = save.read_bytes()
            self.key(process, "Z", "ENTER", "DOWN", "ENTER")
            # 取消移动后菜单，再次选择同一actor：不能提交acted。
            time.sleep(0.6)
            self.capture(process, work, "command-before-cancel")
            self.key(process, "ESC", "ESC", "ENTER")
            time.sleep(0.4)
            text = log.read_text(encoding="utf-8", errors="replace")
            if "field action:" in text or save.read_bytes() != original:
                raise AssertionError("cancel committed action or changed save")
            if "selected=0, interaction=1" not in text.splitlines()[-1]:
                raise AssertionError("cancel did not allow selecting the same actor again")
            self.key(process, "ESC")
            # 焦点已回到unit 0 (7,20)，左邻格为空，进入parent/secondary。
            self.key(process, "LEFT", "ENTER", "UP", "ENTER", "LEFT", "ENTER")
            time.sleep(0.5)
            self.capture(process, work, "save-confirm")
            # 确认框默认左侧Yes；Esc取消必须逐字保留存档。
            self.key(process, "ESC")
            if save.read_bytes() != original:
                raise AssertionError("cancelled save changed the file")
            self.key(process, "ENTER", "UP", "ENTER", "LEFT", "ENTER")
            time.sleep(0.3)
            self.key(process, "LEFT", "ENTER")
            deadline = time.monotonic() + 15
            while save.read_bytes() == original:
                if time.monotonic() >= deadline:
                    raise TimeoutError(f"Save did not write the isolated file; see {log}")
                time.sleep(0.1)
            plain = decode_save(save)
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
            self.capture(process, work, "saved")
            # 确认成功/取消均回browse；结果文字只是短时覆盖层。
            # 配置立即提交：先证明music确实改变，再由Load恢复原快照的配置。
            self.key(process, "ENTER", "RIGHT", "ENTER", "UP", "ENTER")
            wait_log(process, log, f"field option: index=0 value={1 - meta[16]}")
            self.capture(process, work, "options-changed")
            self.key(process, "ESC", "ESC")
            self.key(process, "RIGHT", "RIGHT")
            self.key(process, "ENTER", "UP", "ENTER", "RIGHT", "ENTER")
            time.sleep(0.4)
            self.key(process, "LEFT", "ENTER")
            time.sleep(0.6)
            self.capture(process, work, "loaded")
            # 再次保存恢复后的快照，位置/单位/配置必须与加载源一致。
            self.key(process, "ENTER", "UP", "ENTER", "LEFT", "ENTER")
            time.sleep(0.4)
            self.key(process, "LEFT", "ENTER")
            deadline = time.monotonic() + 15
            while save.stat().st_mtime_ns == saved_mtime:
                if time.monotonic() >= deadline:
                    raise TimeoutError("restored snapshot was never saved again")
                time.sleep(0.1)
            if save.read_bytes() != saved:
                raise AssertionError("Load/Save did not restore identical snapshot bytes")
            text = log.read_text(encoding="utf-8", errors="replace")
            if "field save failed:" in text or "field load failed:" in text:
                raise AssertionError(f"storage reported failure; see {log}")
            self.capture(process, work, "restored")
            # 恢复后必须能继续真实行动；不能只验证codec往返或停在菜单中。
            self.key(process, "RIGHT", "ENTER", "ENTER", "DOWN", "ENTER")
            wait_log(process, log, "field action: unit=0 command=wait acted=1 remaining=3 phase=2")
            if save.read_bytes() != saved:
                raise AssertionError("continuing after Load unexpectedly rewrote the save")
            self.capture(process, work, "continued-wait")
        self.report["cases"].append({"name": "storage-cancel", "status": "passed",
                                     "configuration_restored": True, "continued_action": "unit 0 wait",
                                     "roster_count": roster_count})

    def end_turn(self):
        with self.window("end-turn", ["--field-play", "0"]) as (process, work, log, save):
            wait_log(process, log, "field play: stage=0")
            original = save.read_bytes()
            from PIL import Image
            browse = self.capture(process, work, "browse-before-confirmation")
            with Image.open(browse) as image:
                browse_image = image.copy()
            # 初始焦点为空格；parent下方项为结束回合，先验证Esc取消。
            self.key(process, "ENTER", "DOWN", "ENTER")
            confirmation = self.capture(process, work, "confirmation")
            with Image.open(confirmation) as image:
                buttons = check_confirmation_buttons(image, ROOT / "original_game/FDOTHER.DAT")
            self.key(process, "ESC")
            cancelled = log.read_text(encoding="utf-8", errors="replace")
            if any(marker in cancelled for marker in ("field end turn:", "field phase:", "field action:")):
                raise AssertionError("cancelled end-turn confirmation changed battle state")
            if save.read_bytes() != original:
                raise AssertionError("cancelled end-turn confirmation changed the save")
            cancelled_frame = self.capture(process, work, "cancelled")
            with Image.open(cancelled_frame) as image:
                check_browse_restored(browse_image, image)
            # Right选No；Up/Down被确认框忽略，不能把No切回Yes。
            for name, keys in (
                    ("no-selected", ("RIGHT", "ENTER")),
                    ("no-up-ignored", ("RIGHT", "UP", "ENTER")),
                    ("no-down-ignored", ("RIGHT", "DOWN", "ENTER"))):
                # 每个确认框返回browse，下一次必须从parent重新进入。
                self.key(process, "ENTER", "DOWN", "ENTER")
                self.key(process, *keys)
                rejected = log.read_text(encoding="utf-8", errors="replace")
                if any(marker in rejected for marker in ("field end turn:", "field phase:", "field action:")):
                    raise AssertionError(f"{name} end-turn confirmation changed battle state")
                if save.read_bytes() != original:
                    raise AssertionError(f"{name} end-turn confirmation changed the save")
                rejected_frame = self.capture(process, work, name)
                with Image.open(rejected_frame) as image:
                    check_browse_restored(browse_image, image)
            # 重新打开后默认Yes，直接Enter只能提交一次。
            self.key(process, "ENTER", "DOWN", "ENTER", "ENTER")
            text = wait_log(process, log, "field phase: turn=2 phase=2 remaining=4")
            check_end_turn_log(text)
            self.capture(process, work, "next-player-round")
            # 保持一个输入脉冲只提交一次，不能自动再结束刚进入的玩家回合。
            time.sleep(0.6)
            check_end_turn_log(log.read_text(encoding="utf-8", errors="replace"))
            if save.read_bytes() != original:
                raise AssertionError("end-turn unexpectedly wrote a save")
        self.report["cases"].append({"name": "end-turn", "status": "passed",
                                     "cancelled_before_commit": True, "no_before_commit": True,
                                     "default_yes": True, "browse_restored": True,
                                     "round": 2, "confirmation_buttons": buttons})

    def main_title_routes(self):
        from run_original_comparison import Capture, title_templates

        source = self.work / "storage-cancel/FD2.SAV"
        plain = decode_save(source)
        title = title_templates(ROOT / "original_game/FDOTHER.DAT")[3][0]
        fixtures = self.work / "terminal-fixtures"
        fixtures.mkdir()
        for route in ("leave", "victory", "defeat"):
            fixture = source
            if route != "leave":
                fixture = fixtures / f"{route}-synthetic.SAV"
                fixture.write_bytes(terminal_save_fixture(plain, route))
            expected_exit = 0 if route == "leave" else 2
            with self.window(f"main-{route}", [], expected_exit=expected_exit,
                             save_fixture=fixture) as (process, work, log, save):
                original = save.read_bytes()
                capture = Capture(self.args.driver, process, work)
                capture.key = lambda key: self.key(process, key)
                capture.wait(title, startup=True, timeout=45).save(work / "title-before-load.png")
                if route == "leave":
                    self.key(process, "DOWN", "ENTER")
                    wait_log(process, log, "title manual load not implemented; staying at title")
                    capture.wait(title, timeout=20).save(work / "title-after-unsupported-manual-load.png")
                    if save.read_bytes() != original:
                        raise AssertionError("unsupported manual Load changed the save")
                # 三项标题从START向上回绕到CONTINUE；LOAD是独立手工槽入口。
                self.key(process, "UP", "ENTER")
                if route == "leave":
                    wait_log(process, log, "field play: stage=0")
                    self.capture(process, work, "loaded-battle")
                    # 保存的焦点(6,20)为空，parent→secondary→leave→Yes。
                    self.key(process, "ENTER", "UP", "ENTER", "DOWN", "ENTER", "LEFT", "ENTER")
                    wait_log(process, log, "field leave: returning to title")
                    capture.wait(title, timeout=20).save(work / "title-after-leave.png")
                else:
                    if process.wait(timeout=20) != 2:
                        raise AssertionError(f"{route} did not stop with explicit unsupported-continuation exit 2")
                    text = log.read_text(encoding="utf-8", errors="replace")
                    continuation = ("stage 0 settlement not implemented" if route == "victory"
                                    else "stage 0 defeat continuation not implemented")
                    if text.count(f"field battle: {route}, stage=0") != 1 or continuation not in text:
                        raise AssertionError(f"{route} result was not distinctly dispatched to main")
                    if text.count("field play: stage=0") != 1 or "field leave: returning to title" in text:
                        raise AssertionError(f"{route} unexpectedly reentered a title/battle route")
                if save.read_bytes() != original:
                    raise AssertionError(f"main {route} route unexpectedly changed its save fixture")
            self.report["cases"].append({"name": f"main-{route}", "status": "passed",
                                         "synthetic_terminal_fixture": route != "leave",
                                         "expected_exit": expected_exit,
                                         "scope": "SDL main dispatch; not original gameplay acceptance"})

    def run(self):
        before = hashes(ROOT / "original_game")
        self.report["original_hashes"] = before
        try:
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
    parser.add_argument("--dosbox", default=shutil.which("dosbox-x"))
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
