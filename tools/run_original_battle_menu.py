#!/usr/bin/env python3
"""B1 原版 DOSBox-X／SDL 战场 END 确认框与取消路径的 WGC 对照。

依据 field_empty_focus_menu_execute @0x16f55、sub_19953 @0x19953、
field_controller_input @0x117e7。仅使用真实标题 CONTINUE 与真实按键；
不肯定提交 END，不比较 phase、伤害或 RNG（原版确认框本身消费 RNG）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import struct
import sys
import time
from pathlib import Path

from PIL import Image

from run_original_comparison import (Capture, SIZE, TOLERANCE, archive_entry,
                                     compare, decode_image, hashes,
                                     require_content, running, title_templates)
from run_graphics_tests import decode_save

BUTTON_POSITIONS = ((232, 168), (264, 168))
PARENT_IDS = (7, 5, 6, 4)
MENU_OFFSETS = ((0, -20), (-24, 2), (24, 2), (0, 24))


def resource_templates(resource: Path, text_resource: Path) -> dict:
    data = resource.read_bytes()
    palette6 = archive_entry(data, 0)
    if len(palette6) != 768 or max(palette6) > 63:
        raise ValueError("战场调色板不是 256 项 VGA 六位 RGB")
    palette = [value << 2 for value in palette6]
    sheet = archive_entry(data, 2)
    if len(sheet) < 312 or struct.unpack_from("<I", sheet)[0] != 78 * 4:
        raise ValueError("FDOTHER[2] 不是已确认的 78 帧 command sheet")
    commands = []
    for index in range(78):
        start = struct.unpack_from("<I", sheet, index * 4)[0]
        end = struct.unpack_from("<I", sheet, (index + 1) * 4)[0] if index < 77 else len(sheet)
        if not 312 <= start < end <= len(sheet) or end - start < 4:
            raise ValueError("command 帧边界无效")
        width, height = struct.unpack_from("<HH", sheet, start)
        expected_height = 16 if index in (48, 49, 51, 52) else 20
        if (width, height) != (24, expected_height) or end - start != 4 + width * height:
            raise ValueError(f"command frame {index} 尺寸／长度不符")
        image = Image.frombytes("P", (width, height), sheet[start + 4:end])
        image.putpalette(palette)
        commands.append(image)
    cursor_sheet = archive_entry(data, 1)
    width, height, count = struct.unpack_from("<HHH", cursor_sheet)
    if (width, height, count) != (24, 24, 20):
        raise ValueError("FDOTHER[1] 选择框布局不符")
    start, end = struct.unpack_from("<II", cursor_sheet, 6)
    cursor = decode_image(struct.pack("<HH", width, height) + cursor_sheet[start:end])
    cursor.putpalette(palette)

    # fragment 412 只有十个普通字形和 -1，不等待键盘；只识别前两字的
    # 前景／阴影，允许在 200 ms 的完整提示停留期间未采到这一瞬间。
    text = archive_entry(text_resource.read_bytes(), 0)
    start, end = struct.unpack_from("<HH", text, 412 * 2)
    tokens = struct.unpack("<" + "h" * ((end - start) // 2), text[start:end])
    if not tokens or tokens[-1] != -1 or any(token < 0 for token in tokens[:-1]):
        raise ValueError("FDTXT fragment 412 不再符合无等待取消提示")
    font = archive_entry(data, 4)
    notice = Image.new("P", (33, 17), 0)
    notice.putpalette(palette)
    for item, token in enumerate(tokens[:2]):
        glyph = font[token * 32:(token + 1) * 32]
        if len(glyph) != 32:
            raise ValueError("取消提示字形越界")
        for y in range(16):
            bits = int.from_bytes(glyph[y * 2:y * 2 + 2], "big")
            for x in range(16):
                if bits & (0x8000 >> x):
                    notice.putpixel((item * 16 + x, y), 0xcd)
                    notice.putpixel((item * 16 + x + 1, y), 0x4c)
                    notice.putpixel((item * 16 + x, y + 1), 0x4c)
    return {"commands": commands, "cursor": cursor, "notice": notice,
            "palette6": palette6, "palette": palette}


def match_indexed(actual: Image.Image, template: Image.Image, position: tuple) -> dict:
    """只检查资源非零像素；0 是原版透明色，不取相邻地形作 oracle。"""
    x, y = position
    if actual.size != SIZE or x < 0 or y < 0 or x + template.width > 320 or y + template.height > 200:
        raise ValueError("检查点图像或资源 ROI 越界")
    target = actual.crop((x, y, x + template.width, y + template.height)).convert("RGB")
    expected = template.convert("RGB")
    deltas = [max(abs(a - b) for a, b in zip(want, got))
              for index, want, got in zip(template.getdata(), expected.getdata(), target.getdata())
              if index != 0]
    bad = sum(delta > TOLERANCE for delta in deltas)
    return {"ok": bool(deltas) and bad == 0, "bad_pixels": bad,
            "checked_pixels": len(deltas), "max_channel_delta": max(deltas, default=0)}


class Oracle:
    def __init__(self, assets: dict, focus: tuple):
        self.assets, self.focus = assets, focus

    def choices(self, frame, indices, position):
        for index in indices:
            result = match_indexed(frame, self.assets["commands"][index], position)
            if result["ok"]:
                return {"frame": index, "position": list(position), **result}
        return None

    def menu(self, frame, selected=None):
        matches = []
        for item, command in enumerate(PARENT_IDS):
            states = (0, 1) if selected is None else (1,) if selected == item else (0,)
            # SDL 的 march 禁用是能力门，原版正常图与 SDL 禁用图分开核实。
            if item == 1:
                states = (0, 2)
            position = tuple(a + b for a, b in zip(self.focus, MENU_OFFSETS[item]))
            found = self.choices(frame, [command * 3 + state for state in states], position)
            if not found:
                return None
            matches.append(found)
        return {"kind": "parent_menu", "selected": selected, "matches": matches}

    def buttons(self, frame, selected=None):
        matches = []
        for item, command in enumerate((16, 17)):
            states = (0, 1) if selected is None else (1,) if selected == item else (0,)
            found = self.choices(frame, [command * 3 + state for state in states], BUTTON_POSITIONS[item])
            if not found:
                return None
            matches.append(found)
        return {"kind": "confirmation", "selected": selected, "matches": matches}

    def browse(self, frame):
        cursor = match_indexed(frame, self.assets["cursor"], self.focus)
        if cursor["ok"] and not self.menu(frame) and not self.buttons(frame):
            return {"kind": "browse", "cursor": cursor, "focus_screen": list(self.focus)}
        return None

    def cancellation_notice(self, frame):
        return match_indexed(frame, self.assets["notice"], (99, 146))["ok"]


def save_checkpoint(capture, name, frame, detail, report):
    frame.save(capture.artifacts / f"{name}.png")
    raw = capture.artifacts / f"window-{capture.process.pid}-current.bmp"
    if raw.exists():
        shutil.copy2(raw, capture.artifacts / f"{name}.bmp")
    report.append({"name": name, "pid": capture.process.pid,
                   "time": time.monotonic(), **detail})


def wait_state(capture, name, predicate, report, *, timeout=15, transition=None):
    deadline = time.monotonic() + timeout
    notice_seen = False
    retries = 0
    while time.monotonic() < deadline:
        try:
            frame = capture.frame()
        except (RuntimeError, ValueError) as error:
            # 切场景可能暂时黑屏、变更尺寸或重建 HWND；只对白名单中的
            # WGC 瞬态等待下一帧。权限、API、进程退出等错误必须直接失败。
            message = str(error)
            transient = (isinstance(error, RuntimeError) and any(text in message for text in (
                "blank or nearly uniform captured client area",
                "window geometry changed during capture", "no visible window for PID ")))
            transient = transient or (isinstance(error, ValueError) and message.startswith(
                ("截图尺寸不是 320x200 的整数倍率", "截图尺寸过小")))
            if not transient:
                raise
            retries += 1
            label = f"{name}-wgc-retry-{retries}"
            event = {"name": label, "pid": capture.process.pid, "time": time.monotonic(),
                     "kind": "wgc_transient", "error": message,
                     "capture_backend": "Windows.Graphics.Capture HWND",
                     "last_frame_may_precede_error": True}
            # WGC 驱动在拒绝黑帧前不写 BMP；明确保存的是最后可用帧，
            # 不能把它登记为当前瞬态黑帧或已到达目标检查点。
            if capture.last is not None:
                capture.last.save(capture.artifacts / f"{label}-last-valid.png")
                event["last_valid_frame"] = f"{label}-last-valid.png"
            raw = capture.artifacts / f"window-{capture.process.pid}-current.bmp"
            if raw.exists():
                shutil.copy2(raw, capture.artifacts / f"{label}-last-raw.bmp")
                event["last_raw_capture"] = f"{label}-last-raw.bmp"
            report.append(event)
            time.sleep(0.08)
            continue
        detail = predicate(frame)
        if detail:
            require_content(frame)
            detail["cancellation_notice_observed"] = notice_seen
            detail["wgc_transient_retries"] = retries
            save_checkpoint(capture, name, frame, detail, report)
            return frame.copy(), detail
        if transition and not notice_seen and transition(frame):
            notice_seen = True
            save_checkpoint(capture, name + "-fragment412", frame,
                            {"kind": "automatic_cancellation_notice", "fragment": 412}, report)
        time.sleep(0.08)
    if capture.last is not None:
        save_checkpoint(capture, name + "-timeout", capture.last, {"kind": "timeout"}, report)
    raise TimeoutError(f"等待战场检查点超时：{name}；未发送额外按键，截图已保留")


def compare_resources(original, actual, detail, assets):
    """只对照已识别的资源像素；march 能力门与地形动画不在此像素范围。"""
    expected_canvas = Image.new("RGB", SIZE)
    actual_canvas = Image.new("RGB", SIZE)
    checked = 0
    for match in detail.get("matches", []):
        if match["frame"] // 3 == 5:
            continue
        template = assets["commands"][match["frame"]]
        px, py = match["position"]
        for y in range(template.height):
            for x in range(template.width):
                if template.getpixel((x, y)):
                    point = (px + x, py + y)
                    expected_canvas.putpixel(point, original.getpixel(point))
                    actual_canvas.putpixel(point, actual.getpixel(point))
                    checked += 1
    result = compare(expected_canvas, actual_canvas)
    result["ok"] = result["ok"] and checked > 0
    result["checked_pixels"] = checked
    result["scope"] = "identified opaque resource pixels; SDL march capability gate excluded"
    return result


def wait_title(capture, expected, timeout=45):
    # DOSBox 在切换 VGA 模式前可能短暂给出非 320×200 整数倍率客户区；
    # 只在标题启动边界等待该状态结束，不裁切或更换截图接口。
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            return capture.wait(expected, startup=True,
                                timeout=max(0.1, deadline - time.monotonic()))
        except ValueError as error:
            if "截图尺寸" not in str(error):
                raise
            time.sleep(0.15)
    raise TimeoutError("标题启动超时：窗口未形成有效的 320×200 整数倍率内容")


def restored_browse(frame, baseline, oracle):
    # 本夹具的左下角 ROI 经原版 WGC 确认不受 actor／地形动画影响；
    # 确认层在此显示头像。光标可能仍显示在 fragment 412 上方，
    # 必须同时证明此区域恢复为本进程的原始战场。
    box = (16, 184, 32, 196)
    expected = Image.new("RGB", SIZE)
    actual = Image.new("RGB", SIZE)
    expected.paste(baseline.crop(box), box[:2])
    actual.paste(frame.crop(box), box[:2])
    restored = compare(expected, actual)
    if restored["ok"]:
        detail = oracle.browse(frame)
        if detail:
            return {**detail, "restored_battle_roi": list(box), "restoration": restored}
    return None


def exercise(capture, prefix, oracle, titles, checkpoints, references=None):
    wait_title(capture, titles[3][0])
    save_checkpoint(capture, prefix + "-title", capture.last, {"kind": "title", "selected": 0}, checkpoints)
    capture.key("UP")
    capture.wait(titles[3][2])
    save_checkpoint(capture, prefix + "-continue", capture.last, {"kind": "title", "selected": 2}, checkpoints)
    capture.key("ENTER")
    observed = {}

    def checkpoint(label, predicate, **kwargs):
        frame, detail = wait_state(capture, prefix + "-" + label, predicate, checkpoints, **kwargs)
        if references is not None and detail.get("matches"):
            comparison = compare_resources(references[label], frame, detail, oracle.assets)
            checkpoints[-1]["original_comparison"] = comparison
            if not comparison["ok"]:
                raise AssertionError(f"SDL 与原版资源像素不符：{label} {comparison}")
        observed[label] = frame

    checkpoint("browse", oracle.browse, timeout=30)
    for cycle, cancel in (("escape", "ESC"), ("no", "ENTER")):
        capture.key("ENTER")
        checkpoint(cycle + "-menu", lambda frame: oracle.menu(frame, 0))
        capture.key("DOWN")
        checkpoint(cycle + "-end-selected", lambda frame: oracle.menu(frame, 3))
        capture.key("ENTER")
        checkpoint(cycle + "-default-yes", lambda frame: oracle.buttons(frame, 0))
        if cycle == "no":
            capture.key("RIGHT")
            checkpoint("no-selected", lambda frame: oracle.buttons(frame, 1))
        capture.key(cancel)
        checkpoint(cycle + "-cancelled-browse",
                   lambda frame: restored_browse(frame, observed["browse"], oracle),
                   transition=oracle.cancellation_notice)
    # 取消后重新打开 parent，证明仍可接受真实菜单输入，再回 browse 退出。
    capture.key("ENTER")
    checkpoint("reopened-menu", lambda frame: oracle.menu(frame, 0))
    capture.key("ESC")
    checkpoint("final-browse", oracle.browse)
    return observed


def validate_fixture(path, expected_shape):
    plain = decode_save(path)
    meta = plain[0x30c3:0x30d5]
    if meta[2] != 0 or not 0 < meta[1] <= 64:
        raise ValueError("B1 对照要求受支持的 stage 0 活动快照")
    if plain[0] != expected_shape:
        raise ValueError("快照 metadata 的 shape 与本关 FDFIELD 资源不一致")
    x, y = meta[5:7]
    if any(plain[0x12a3 + i * 80] == x and plain[0x12a3 + i * 80 + 1] == y and
           not plain[0x12a3 + i * 80 + 5] & 1 for i in range(meta[1])):
        raise ValueError("存档焦点存在可见 actor，不能打开空焦点菜单")
    focus = (4 + (x - meta[3]) * 24, 4 + (y - meta[4]) * 24)
    if not (28 <= focus[0] <= 268 and 24 <= focus[1] <= 152):
        raise ValueError("存档焦点不足以容纳已确认的四向菜单 ROI")
    return focus, list(meta)


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original, fixture = root / "original_game", args.save_fixture.resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    output.mkdir(parents=True, exist_ok=False)
    before = hashes(original)
    fixture_bytes = fixture.read_bytes()
    result = {"ok": False, "scope": "END buttons/default YES/Right NO/Esc and NO cancel to browse",
              "excluded": "END affirmative submission, phase, damage, RNG equivalence, map/background pixels, full-dialog portrait/text layout",
              "platform": platform.platform(), "capture": "Windows.Graphics.Capture HWND only",
              "original_before": before, "processes": [], "inputs": [], "checkpoints": []}
    try:
        stage_metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
        focus, meta = validate_fixture(fixture, stage_metadata[0])
        result["fixture"] = {"path": str(fixture), "sha256": hashlib.sha256(fixture_bytes).hexdigest(), "meta": meta}
        result["binaries"] = {key: {"path": str(path.resolve()),
                                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for key, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        assets = resource_templates(original / "FDOTHER.DAT", original / "FDTXT.DAT")
        result["palette"] = {"source": "FDOTHER[0] VGA six-bit <<2; SDL expansion tolerance <=3",
                              "channel_tolerance": TOLERANCE,
                              "button_indices": {str(i): list(assets["palette6"][i * 3:i * 3 + 3])
                                                 for i in (0x4a, 0x4d, 0xc5, 0xc7)}}
        oracle = Oracle(assets, focus)
        titles = title_templates(original / "FDOTHER.DAT")
        game_copy = output / "game"
        shutil.copytree(original, game_copy)
        shutil.copy2(fixture, game_copy / "FD2.SAV")
        sdl_save = output / "SDL-FD2.SAV"
        shutil.copy2(fixture, sdl_save)
        configuration = output / "dosbox.conf"
        configuration.write_text(
            "[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
            "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n"
            "[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n"
            f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
        environment = os.environ.copy()
        for variable in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER", "FD2SDL_FIELD_RNG_SEED"):
            environment.pop(variable, None)
        with running([str(args.dosbox.resolve()), "-conf", str(configuration), "-noconsole"],
                     output, environment, output / "dosbox.log", args.driver, result["processes"]) as process:
            capture = Capture(args.driver, process, output, result["inputs"])
            references = exercise(capture, "original", oracle, titles, result["checkpoints"])
        copied_save = (game_copy / "FD2.SAV").read_bytes()
        result["original_copy_save_sha256"] = hashlib.sha256(copied_save).hexdigest()
        result["original_copy_save_unchanged"] = copied_save == fixture_bytes
        if not result["original_copy_save_unchanged"]:
            raise AssertionError("原版取消路径修改了隔离存档")
        environment["FD2SDL_SAVE_PATH"] = str(sdl_save)
        with running([str(args.game.resolve())], root, environment, output / "sdl.log",
                     args.driver, result["processes"]) as process:
            capture = Capture(args.driver, process, output, result["inputs"])
            exercise(capture, "sdl", oracle, titles, result["checkpoints"], references)
        sdl_bytes = sdl_save.read_bytes()
        result["sdl_save_sha256"] = hashlib.sha256(sdl_bytes).hexdigest()
        result["sdl_save_unchanged"] = sdl_bytes == fixture_bytes
        if not result["sdl_save_unchanged"]:
            raise AssertionError("SDL 取消路径修改了隔离存档")
        result["ok"] = True
    except Exception as error:
        result["error"] = str(error)
    finally:
        result["original_after"] = hashes(original)
        result["original_unchanged"] = before == result["original_after"]
        result["fixture_unchanged"] = fixture.read_bytes() == fixture_bytes
        result["ok"] = result["ok"] and result["original_unchanged"] and result["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts", "save-fixture"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("原版确认框对照要求 Windows WGC 窗口驱动；本机未启动 GUI")
    try:
        result = run(args)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result["ok"] else 1
    except Exception as error:
        print(f"原版战场菜单对照无法启动：{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
