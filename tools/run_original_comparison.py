#!/usr/bin/env python3
"""Windows 原版 DOSBox-X／SDL 标题菜单图形对照。

依据 boot_intro_title_menu @0x1f894（原登记 title_action_menu）、
title_action_dispatch @0x25ebb：标题 Up/Down 回绕，Esc 不改变选择。
原版与 SDL 均使用宿主驱动的单窗口 WGC 截图，不读取宿主桌面。
只在独立副本中运行原版；不确认新游戏、读档或退出菜单项。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import struct
import subprocess
import sys
import time
from contextlib import contextmanager
from pathlib import Path

from PIL import Image, ImageChops

SIZE = (320, 200)
TOLERANCE = 3  # DOS VGA 6-bit <<2 与 SDL (v<<2)|(v>>4) 的最大差异。
SEQUENCE = ("DOWN", "UP", "UP", "DOWN", "ESC")


def normalize(image: Image.Image) -> Image.Image:
    """只接受无边框、同整数倍率的 320×200 内容，不裁切或改纵横比。"""
    width, height = image.size
    if width % SIZE[0] or height % SIZE[1] or width // SIZE[0] != height // SIZE[1]:
        raise ValueError(f"截图尺寸不是 320x200 的整数倍率：{image.size}")
    if width < SIZE[0]:
        raise ValueError(f"截图尺寸过小：{image.size}")
    return image.convert("RGB").resize(SIZE, Image.Resampling.NEAREST)


def compare(expected: Image.Image, actual: Image.Image) -> dict:
    if expected.size != SIZE or actual.size != SIZE:
        raise ValueError("比较输入必须已规范化为 320x200")
    difference = ImageChops.difference(expected.convert("RGB"), actual.convert("RGB"))
    extrema = difference.getextrema()
    bad_pixels = sum(max(pixel) > TOLERANCE for pixel in difference.getdata())
    return {"ok": bad_pixels == 0, "bad_pixels": bad_pixels,
            "max_channel_delta": max(high for _, high in extrema),
            "channel_tolerance": TOLERANCE}


def require_content(image: Image.Image) -> None:
    colors = image.convert("RGB").getcolors(SIZE[0] * SIZE[1])
    if not colors or len(colors) < 32 or max(image.convert("RGB").getextrema()[c][1]
                                            for c in range(3)) < 128:
        raise ValueError("截图为空白或颜色不足，不能作为原版标题证据")


def archive_entry(data: bytes, index: int) -> bytes:
    """只读取标题 fixture；边界按归档首偏移约束，不扫描 payload。"""
    if len(data) < 10 or data[:6] != b"LLLLLL":
        raise ValueError("标题归档头无效")
    end = struct.unpack_from("<I", data, 6)[0]
    if end < 10 or end >= len(data):
        raise ValueError("标题归档目录边界无效")
    offsets = []
    for position in range(6, end - 3, 4):
        offset = struct.unpack_from("<I", data, position)[0]
        if offset >= len(data) or (offsets and offset <= offsets[-1]):
            break
        offsets.append(offset)
    if not 0 <= index < len(offsets):
        raise ValueError(f"标题归档条目不存在：{index}")
    return data[offsets[index]:offsets[index + 1] if index + 1 < len(offsets) else len(data)]


def decode_image(data: bytes) -> Image.Image:
    if len(data) < 4:
        raise ValueError("标题 RLE 图像截断")
    width, height = struct.unpack_from("<HH", data)
    if not 0 < width <= 320 or not 0 < height <= 200:
        raise ValueError("标题 RLE 图像尺寸无效")
    pixels = bytearray(width * height)
    source = 4
    for row in range(height):
        column = 0
        while column < width:
            if source >= len(data):
                raise ValueError("标题 RLE 控制字节截断")
            control = data[source]
            source += 1
            count = (control & 63) + 1
            mode = control & 192
            span = count * 2 if mode == 64 else count
            if column + span > width:
                raise ValueError("标题 RLE 超行")
            target = row * width + column
            needed = count if mode == 128 else 1 if mode in (0, 64) else 0
            if source + needed > len(data):
                raise ValueError("标题 RLE 数据截断")
            if mode == 128:
                pixels[target:target + count] = data[source:source + count]
            elif mode == 0:
                pixels[target:target + count] = bytes([data[source]]) * count
            elif mode == 64:
                pixels[target + 1:target + span:2] = bytes([data[source]]) * count
            source += needed
            column += span
    if source != len(data):
        raise ValueError("标题 RLE 存在未消费数据")
    return Image.frombytes("P", (width, height), bytes(pixels))


def title_templates(resource: Path) -> dict[int, list[Image.Image]]:
    """资源模板只用于确认到达标题；最终 SDL oracle 来自原版实际捕获。"""
    data = resource.read_bytes()
    nested = archive_entry(data, 7)
    palette = archive_entry(data, 8)
    if len(palette) < 768 or max(palette[:768]) > 63:
        raise ValueError("标题调色板无效")
    base = decode_image(archive_entry(nested, 0))
    if base.size != SIZE:
        raise ValueError("标题底图必须为 320x200")
    entries = [decode_image(archive_entry(nested, index)) for index in range(1, 7)]
    templates = {}
    for count in (2, 3):
        templates[count] = []
        for selection in range(count):
            frame = base.copy()
            for item in range(count):
                frame.paste(entries[item * 2 + (item == selection)], (129, 164 + item * 9))
            frame.putpalette([value << 2 for value in palette[:768]])
            templates[count].append(frame.convert("RGB"))
    return templates


def hashes(directory: Path) -> dict[str, str]:
    return {str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(directory.rglob("*")) if path.is_file()}


def driver_call(driver: Path, command: str, process: subprocess.Popen,
                argument: str | Path | None = None) -> None:
    if process.poll() is not None:
        raise RuntimeError(f"目标进程提前结束：PID {process.pid}，exit={process.returncode}")
    args = [str(driver), command, str(process.pid)]
    if argument is not None:
        args.append(str(argument))
    result = subprocess.run(args, capture_output=True, text=True, timeout=15)
    if result.returncode:
        raise RuntimeError(f"窗口驱动 {command} 失败：{result.stdout}{result.stderr}")


@contextmanager
def running(command: list[str], cwd: Path, env: dict, log: Path, driver: Path,
            receipts: list[dict]):
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream, stderr=stream)
        receipt = {"pid": process.pid, "command": command, "log": str(log),
                   "forced_cleanup": False, "cleaned": False}
        receipts.append(receipt)
        body_failed = False
        try:
            yield process
        except BaseException:
            body_failed = True
            raise
        finally:
            if process.poll() is None:
                try:
                    driver_call(driver, "close", process)
                    process.wait(timeout=5)
                except (OSError, RuntimeError, subprocess.SubprocessError):
                    receipt["forced_cleanup"] = True
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=3)
            receipt["exit_code"] = process.poll()
            receipt["cleaned"] = process.poll() is not None
            if not body_failed and (receipt["forced_cleanup"] or receipt["exit_code"] != 0):
                raise RuntimeError(f"窗口进程未正常退出：{receipt}")


def verify_occlusion(capture: "Capture", output: Path) -> dict:
    """用自有无激活白窗遮挡 SDL；WGC 捕获不前置游戏窗口。"""
    import ctypes
    from ctypes import wintypes as wt

    user = ctypes.WinDLL("user32", use_last_error=True)
    user.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
    user.IsWindowVisible.argtypes = [wt.HWND]
    user.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
    user.UpdateWindow.argtypes = [wt.HWND]
    callback_type = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    target = []

    @callback_type
    def find(window, _):
        pid = wt.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(pid))
        if pid.value == capture.process.pid and user.IsWindowVisible(window):
            target.append(window)
            return False
        return True

    user.EnumWindows.argtypes = [callback_type, wt.LPARAM]
    user.EnumWindows(find, 0)
    if not target:
        raise RuntimeError("遮挡验证找不到 SDL 目标窗口")
    rectangle = wt.RECT()
    if not user.GetWindowRect(wt.HWND(target[0]), ctypes.byref(rectangle)):
        raise ctypes.WinError(ctypes.get_last_error())
    user.CreateWindowExW.argtypes = [wt.DWORD, wt.LPCWSTR, wt.LPCWSTR, wt.DWORD,
                                    ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                    wt.HWND, wt.HMENU, wt.HINSTANCE, wt.LPVOID]
    user.CreateWindowExW.restype = wt.HWND
    user.DestroyWindow.argtypes = [wt.HWND]
    baseline = capture.frame().copy()
    overlay = user.CreateWindowExW(0x08000008, "STATIC", "FD2 capture occlusion check",
                                  0x90000006, rectangle.left + 60, rectangle.top + 60,
                                  240, 180, None, None, None, None)
    if not overlay:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        user.UpdateWindow(wt.HWND(overlay))
        if not user.IsWindowVisible(wt.HWND(overlay)):
            raise RuntimeError("遮挡验证白窗未显示")
        time.sleep(0.25)
        obscured = capture.frame().copy()
        obscured.save(output / "sdl-occluded.png")
        result = compare(baseline, obscured)
        if not result["ok"]:
            ImageChops.difference(baseline, obscured).save(output / "sdl-occluded-diff.png")
            raise RuntimeError(f"单窗口截图受到宿主遮挡影响：{result}")
        return {"ok": True, "overlay_visible": True, "overlay_size": [240, 180], **result}
    finally:
        if not user.DestroyWindow(wt.HWND(overlay)):
            raise ctypes.WinError(ctypes.get_last_error())


class Capture:
    def __init__(self, driver: Path, process: subprocess.Popen, artifacts: Path, trace=None):
        self.driver, self.process, self.artifacts = driver, process, artifacts
        self.last: Image.Image | None = None
        self.trace = trace if trace is not None else []

    def key(self, name: str) -> None:
        event = {"time": time.monotonic(), "pid": self.process.pid, "key": name}
        self.trace.append(event)
        with (self.artifacts / "input-trace.jsonl").open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(event) + "\n")
        driver_call(self.driver, "focus", self.process)
        driver_call(self.driver, "key", self.process, name)

    def frame(self) -> Image.Image:
        path = self.artifacts / f"window-{self.process.pid}-current.bmp"
        driver_call(self.driver, "capture", self.process, path)
        with Image.open(path) as loaded:
            self.last = normalize(loaded)
        return self.last

    def wait(self, expected: Image.Image, *, startup: bool = False,
             blank_transition: bool = False, timeout: float = 15) -> Image.Image:
        deadline = time.monotonic() + timeout
        next_escape = time.monotonic() + 1
        while time.monotonic() < deadline:
            try:
                frame = self.frame()
            except RuntimeError as error:
                transient = any(message in str(error) for message in
                                ("no visible window", "blank or nearly uniform", "geometry changed"))
                allowed_blank = blank_transition and "blank or nearly uniform" in str(error)
                if not (startup and transient) and not allowed_blank:
                    raise
                if allowed_blank:
                    # 已知页面淡出／淡入：同一WGC、同一deadline，不发送跳过键。
                    # 保留错误画面，下一次成功捕获不得覆盖唯一现场。
                    event = {"time": time.monotonic(), "pid": self.process.pid,
                             "capture_error": str(error)}
                    path = self.artifacts / f"window-{self.process.pid}-current.bmp"
                    if path.is_file():
                        saved = self.artifacts / f"transition-blank-{time.monotonic_ns()}.bmp"
                        shutil.copyfile(path, saved)
                        event["frame"] = str(saved)
                    self.trace.append(event)
                    with (self.artifacts / "capture-errors.jsonl").open("a", encoding="utf-8") as stream:
                        stream.write(json.dumps(event) + "\n")
                time.sleep(0.15)
                continue
            if compare(expected, frame)["ok"]:
                require_content(frame)
                return frame.copy()
            if startup and time.monotonic() >= next_escape:
                self.key("ESC")  # 仅启动阶段跳过动画；已确认标题忽略 Esc。
                next_escape = time.monotonic() + 2
            time.sleep(0.15)
        expected.save(self.artifacts / "timeout-expected.png")
        if self.last is None:
            raise RuntimeError("等待图形检查点超时：目标窗口始终没有可用画面")
        self.last.save(self.artifacts / "timeout-actual.png")
        ImageChops.difference(expected, self.last).save(self.artifacts / "timeout-diff.png")
        raise RuntimeError(f"等待图形检查点超时：{compare(expected, self.last)}")


def run(args) -> dict:
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = root / "original_game"
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    # 每次独立运行保留现场，不覆盖上次捕获或可写游戏副本。
    output.mkdir(parents=True, exist_ok=False)
    before = hashes(original)
    result = {"ok": False, "scope": "title menu: DOWN UP UP DOWN ESC",
              "platform": platform.platform(),
              "inputs": {name: {"path": str(path.resolve()),
                                 "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                         for name, path in (("game", args.game), ("driver", args.driver),
                                            ("dosbox", args.dosbox))},
              "capture": {"original": "Windows.Graphics.Capture DOSBox-X target window",
                          "sdl": "Windows.Graphics.Capture target window"},
              "checkpoints": [], "original_checkpoints": [], "input_events": [], "processes": [],
              "original_before": before}
    try:
        game_copy = output / "game"
        shutil.copytree(original, game_copy)
        fixture = args.save_fixture.resolve() if args.save_fixture else original / "FD2.SAV"
        from run_graphics_tests import decode_save
        plain = decode_save(fixture)
        result["save_fixture"] = {
            "path": str(fixture), "sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
            "override": args.save_fixture is not None, "active_turn": plain[0x30c3],
            "active_unit_count": plain[0x30c4], "active_stage": plain[0x30c5]}
        if args.save_fixture:
            shutil.copy2(fixture, game_copy / "FD2.SAV")
        sdl_save = output / "SDL-FD2.SAV"
        shutil.copy2(game_copy / "FD2.SAV", sdl_save)
        templates = title_templates(original / "FDOTHER.DAT")
        # 先由资源和存档对应的实际原版画面识别菜单项数；不猜存档 checksum。
        conf = output / "dosbox.conf"
        conf.write_text(
            "[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\n"
            "autolock=false\nwaitonerror=false\n"
            "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n"
            "[render]\naspect=false\nscaler=normal2x\n"
            "[cpu]\ncore=normal\ncycles=fixed 20000\n"
            f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n',
            encoding="utf-8")
        oracle = []
        environment = os.environ.copy()
        for variable in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            environment.pop(variable, None)
        with running([str(args.dosbox.resolve()), "-conf", str(conf), "-noconsole"],
                     output, environment, output / "dosbox.log", args.driver,
                     result["processes"]) as process:
            capture = Capture(args.driver, process, output, result["input_events"])
            deadline = time.monotonic() + 45
            count = None
            while time.monotonic() < deadline:
                try:
                    frame = capture.frame()
                except RuntimeError as error:
                    if not any(message in str(error) for message in
                               ("no visible window", "blank or nearly uniform", "geometry changed")):
                        raise
                    time.sleep(0.15)
                    continue
                except ValueError as error:
                    if "截图尺寸" not in str(error):
                        raise
                    time.sleep(0.15)
                    continue
                for candidate, frames in templates.items():
                    if compare(frames[0], frame)["ok"]:
                        count = candidate
                        break
                if count:
                    break
                capture.key("ESC")
            if count is None:
                if capture.last:
                    capture.last.save(output / "original-title-timeout.png")
                raise RuntimeError("原版未到达与资源一致的标题菜单")
            result["menu_count"] = count
            oracle.append(frame.copy())
            oracle[0].save(output / "original-0-default.png")
            selection = 0
            for index, key in enumerate(SEQUENCE, 1):
                capture.key(key)
                selection = ((selection + (1 if key == "DOWN" else -1)) % count
                             if key != "ESC" else selection)
                frame = capture.wait(templates[count][selection])
                oracle.append(frame)
                frame.save(output / f"original-{index}-{key.lower()}.png")
                result["original_checkpoints"].append(
                    {"key": key, "selection": selection, **compare(templates[count][selection], frame)})
            if compare(oracle[0], oracle[1])["ok"] or not compare(oracle[0], oracle[2])["ok"]:
                raise RuntimeError("原版菜单没有响应上下键或未恢复初始选择")
        environment["FD2SDL_SAVE_PATH"] = str(sdl_save)
        with running([str(args.game.resolve())], root, environment, output / "sdl.log",
                     args.driver, result["processes"]) as process:
            capture = Capture(args.driver, process, output, result["input_events"])
            capture.wait(oracle[0], startup=True, timeout=45).save(output / "sdl-0-default.png")
            for index, key in enumerate(SEQUENCE, 1):
                capture.key(key)
                frame = capture.wait(oracle[index])
                frame.save(output / f"sdl-{index}-{key.lower()}.png")
                comparison = compare(oracle[index], frame)
                result["checkpoints"].append({"key": key, **comparison})
            result["occlusion"] = verify_occlusion(capture, output)
        result["ok"] = True
    except Exception as error:
        result["error"] = str(error)
    finally:
        result["original_after"] = hashes(original)
        result["original_unchanged"] = before == result["original_after"]
        result["ok"] = result["ok"] and result["original_unchanged"]
        (output / "comparison.json").write_text(json.dumps(result, ensure_ascii=False, indent=2),
                                                encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("game", "driver", "dosbox", "root", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path,
                        help="真实受支持存档；仅复制到本次独立副本，原版目录保持只读")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("原版比较 runner 当前使用 Windows 原生窗口驱动；Linux 输入回归另行运行")
    try:
        result = run(args)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result["ok"] else 1
    except Exception as error:
        print(f"原版图形比较无法启动：{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
