#!/usr/bin/env python3
"""B1.4b2：原版／SDL战况总览静态四区、任意键关闭及只读状态WGC配对。

依据 sub_1B1E7 @0x1b1e7、sub_1B41D @0x1b41d、sub_1B5F1 @0x1b5f1、
font_glyph_blit_16x16 @0x4ed7a。
仅验稳定页；地图idle与开合瞬时帧/时长不在本工具的像素通过范围。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import sys

from PIL import Image

from run_original_comparison import (Capture, SIZE, archive_entry, compare, decode_image,
                                     driver_call, hashes, running, title_templates)
from run_original_battle_menu import match_indexed, wait_state, wait_title
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle, empty_focus_navigation, round_assets, validate_initial
from run_original_load import check_load_measurement, require_source_unchanged, wait_measured_save

RECTANGLES = ((133, 109, 19, 102, 17), (134, 75, 37, 170, 117),
              (135, 75, 155, 170, 16), (136, 129, 172, 63, 15))
CLOSE_KEYS = ("ENTER", "ESC", "RIGHT", "A")


def overview_counts(source):
    counts = {0: 0, 1: 0, 2: 0}
    for index in range(source[META + 1]):
        unit = source[UNITS + index * STRIDE:UNITS + (index + 1) * STRIDE]
        if unit[6] in counts and unit[7] != 121 and unit[0x1f] != 10 and not unit[5] & 1:
            counts[unit[6]] += 1
    return counts


def draw_condition(canvas, text, font, fragment, x, y):
    if fragment * 2 + 4 > len(text):
        raise ValueError("overview text directory truncated")
    start, end = struct.unpack_from("<HH", text, fragment * 2)
    if not 0 <= start < end <= len(text) or (end - start) % 2:
        raise ValueError("overview condition bounds invalid")
    tokens = struct.unpack("<" + "h" * ((end - start) // 2), text[start:end])
    if not tokens or tokens[-1] != -1 or -1 in tokens[:-1]:
        raise ValueError("overview condition must have exactly one final terminator")
    origin = x
    for token in tokens[:-1]:
        if token == -2:
            x, y = origin, y + 19
            continue
        if token < 0 or (token + 1) * 32 > len(font) or not (76 <= x and x + 16 <= 245 and 37 <= y and y + 17 <= 154):
            raise ValueError("overview glyph/control outside supported condition panel")
        if token != 10:
            glyph = font[token * 32:(token + 1) * 32]
            for row in range(16):
                bits = int.from_bytes(glyph[row * 2:row * 2 + 2], "big")
                for column in range(16):
                    if bits & (0x8000 >> column):
                        # 保持原版逐bit写入顺序，不能把全部前景最后覆盖阴影。
                        px, py = x + column, y + row
                        canvas.putpixel((px, py), 205)
                        canvas.putpixel((px - 1, py + 1), 76)
                        canvas.putpixel((px, py + 1), 76)
        x += 16
    return {"fragment": fragment, "tokens": list(tokens)}


def overview_templates(original, source, palette):
    sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 5)
    if len(sheet) < 6 or sheet[:4] != b"LMI1":
        raise ValueError("overview LMI directory invalid")
    count = struct.unpack_from("<H", sheet, 4)[0]
    boundary = 6 + count * 4
    if count <= 136 or boundary > len(sheet):
        raise ValueError("overview frames missing")
    images = {}

    def frame(index):
        if index not in images:
            start = struct.unpack_from("<I", sheet, 6 + index * 4)[0]
            end = struct.unpack_from("<I", sheet, 6 + (index + 1) * 4)[0] if index + 1 < count else len(sheet)
            if not boundary <= start < end <= len(sheet):
                raise ValueError("overview image frame bounds invalid")
            images[index] = decode_image(sheet[start:end])
        return images[index]

    canvas = Image.new("P", SIZE, 0)
    canvas.putpalette(palette)
    for index, x, y, width, height in RECTANGLES:
        image = frame(index)
        if image.size != (width, height) or 0 in image.getdata():
            raise ValueError(f"overview frame {index} must be a complete opaque rectangle")
        canvas.paste(image, (x, y))

    def number(value, x, y, base, digits):
        value = max(0, value)
        if (digits == 2 and value > 99) or (digits == 3 and value > 999):
            image = frame(93 if digits == 2 else base + 10)
            canvas.paste(image, (x, y), image.point(lambda pixel: 255 if pixel else 0, mode="L"))
            return
        for index, digit in enumerate(f"{value:0{digits}d}"[:digits]):
            image = frame(base + int(digit))
            if not 0 < image.width <= 6 or image.height != 8:
                raise ValueError("overview digit frame dimensions invalid")
            canvas.paste(image, (x + index * 6, y), image.point(lambda pixel: 255 if pixel else 0, mode="L"))

    stage, turn = source[META + 2], source[META]
    if stage != 0:
        raise ValueError("overview comparison currently supports stage 0 only")
    resource = int.from_bytes(source[META + 10:META + 14], "little", signed=True)
    counts = overview_counts(source)
    number(stage + 1, 143, 24, 42, 2)
    number(turn, 188, 24, 42, 3)
    number(resource, 140, 176, 31, 8)
    for side, x in ((0, 120), (2, 182), (1, 228)):
        number(counts[side], x, 159, 42, 2)
    text = archive_entry((original / "FDTXT.DAT").read_bytes(), 0)
    font = archive_entry((original / "FDOTHER.DAT").read_bytes(), 4)
    conditions = [draw_condition(canvas, text, font, 597, 80, 61),
                  draw_condition(canvas, text, font, 598, 80, 116)]
    return {"canvas": canvas, "conditions": conditions,
            "fields": {"stage": stage + 1, "turn": turn, "resource": max(0, resource),
                       "side_counts_left_to_right": [counts[0], counts[2], counts[1]]}}


def match_overview(frame, templates):
    matches = []
    for index, x, y, width, height in RECTANGLES:
        expected = templates["canvas"].crop((x, y, x + width, y + height))
        result = match_indexed(frame, expected, (x, y))
        if not result["ok"] or result["checked_pixels"] != width * height:
            return None
        matches.append({"frame": index, "position": [x, y], **result})
    return {"kind": "overview", "rectangles": matches, "fields": templates["fields"],
            "conditions": templates["conditions"], "scope": "four complete stable rectangles"}


def compare_overview(original, actual):
    expected, measured = Image.new("RGB", SIZE), Image.new("RGB", SIZE)
    for _, x, y, width, height in RECTANGLES:
        box = (x, y, x + width, y + height)
        expected.paste(original.crop(box), box[:2])
        measured.paste(actual.crop(box), box[:2])
    return {**compare(expected, measured), "checked_pixels": sum(w * h for _, _, _, w, h in RECTANGLES),
            "scope": "overview rectangles only; map/animation excluded"}


def exercise(capture, kind, assets, templates, focus, steps, titles, save, source_bytes,
             source_mtime, checkpoints, observation, references=None):
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    oracle = RoundOracle(assets, focus)

    def checkpoint(name, predicate, **kwargs):
        return wait_state(capture, kind + "-" + name, predicate, checkpoints, **kwargs)

    checkpoint("initial-browse", oracle.initial_browse, timeout=30)
    for index, (key, position) in enumerate(steps, 1):
        capture.key(key)
        oracle = RoundOracle(assets, position)
        checkpoint(f"empty-step-{index}", oracle.browse)
    observed = {}

    def open_overview(label):
        capture.key("ENTER")
        checkpoint(label + "-parent", lambda image: oracle.menu(image, 0))
        capture.key("UP")
        capture.key("ENTER")
        checkpoint(label + "-default-secondary-zero", lambda image: oracle.secondary(image, 0, overview_available=True))
        capture.key("ENTER")
        image, _ = checkpoint(label + "-overview", lambda image: match_overview(image, templates))
        if references is not None:
            comparison = compare_overview(references[label], image)
            checkpoints[-1]["original_comparison"] = comparison
            if not comparison["ok"]:
                raise AssertionError(f"{kind} overview differs from original: {comparison}")
        observed[label] = image

    observation["close_keys"] = []
    for key in CLOSE_KEYS:
        label = key.lower()
        open_overview(label)
        require_source_unchanged(save, source_bytes, source_mtime)
        capture.key(key)
        checkpoint(label + "-closed-browse", oracle.closed_browse)
        require_source_unchanged(save, source_bytes, source_mtime)
        observation["close_keys"].append({"key": key, "same_focus_browse": True, "disk_and_mtime_unchanged": True})
    # 独立、明确的测量Save；页面本身从未写文件。
    capture.key("ENTER")
    checkpoint("measurement-parent", lambda image: oracle.menu(image, 0))
    capture.key("UP")
    capture.key("ENTER")
    checkpoint("measurement-secondary-zero", lambda image: oracle.secondary(image, 0, overview_available=True))
    capture.key("LEFT")
    checkpoint("measurement-save-selected", lambda image: oracle.secondary(image, 1, overview_available=True))
    capture.key("ENTER")
    checkpoint("measurement-default-yes", lambda image: oracle.buttons(image, 0))
    require_source_unchanged(save, source_bytes, source_mtime)
    previous, previous_mtime = save.read_bytes(), save.stat().st_mtime_ns
    capture.key("ENTER")
    measured = wait_measured_save(save, previous, previous_mtime)
    checkpoint("measurement-saved-browse", oracle.closed_browse)
    observation["read_only_state_measurement"] = True
    measured_bytes, measured_mtime = save.read_bytes(), save.stat().st_mtime_ns
    open_overview("host-close")
    driver_call(capture.driver, "close", capture.process)
    if capture.process.wait(timeout=10) != 0:
        raise AssertionError("host close during overview did not exit normally")
    require_source_unchanged(save, measured_bytes, measured_mtime)
    observation["host_close_while_overview"] = True
    return measured, observed


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = root / "original_game"
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    output.mkdir(parents=True, exist_ok=False)
    before, source_bytes = hashes(original), fixture.read_bytes()
    report = {"ok": False, "scope": "START overview stable rectangles/fields/conditions, any-key close, read-only state and host close",
              "excluded": "opening/closing transient phase pixels and timing; full-map/idle/background/audio equivalence",
              "capture": "Windows.Graphics.Capture HWND only", "platform": platform.platform(),
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(source_bytes).hexdigest()},
              "original_before": before, "processes": [], "inputs": [], "checkpoints": [], "cases": []}
    try:
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                               for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
        source, focus = validate_initial(fixture, metadata[0])
        steps = empty_focus_navigation(source)
        assets = round_assets(original)
        templates = overview_templates(original, source, assets["palette"])
        templates["canvas"].save(output / "overview-resource-oracle.png")
        report["oracle"] = {"fields": templates["fields"], "conditions": templates["conditions"],
                            "checked_rectangles": [list(rectangle) for rectangle in RECTANGLES]}
        titles = title_templates(original / "FDOTHER.DAT")
        environment = os.environ.copy()
        for variable in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            environment.pop(variable, None)
        references = None
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
                environment["FD2SDL_SAVE_PATH"] = str(save)
                command, cwd = [str(args.game.resolve())], root
            source_mtime = save.stat().st_mtime_ns
            case = {"name": kind, "status": "failed", "source_mtime_ns_before_start": source_mtime, "observation": {}}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, work / "game.log", args.driver, report["processes"]) as process:
                    case["pid"] = process.pid
                    capture = Capture(args.driver, process, work, report["inputs"])
                    measured, pages = exercise(capture, kind, assets, templates, focus, steps, titles,
                                               save, source_bytes, source_mtime, report["checkpoints"],
                                               case["observation"], references)
                    shutil.copy2(save, work / "overview-measured.SAV")
                    case["measurement"] = check_load_measurement(source, measured, steps)
                case["status"] = "passed"
                if kind == "original":
                    references = pages
            except Exception as error:
                case["error"] = str(error)
                raise
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before
        report["fixture_unchanged"] = fixture.read_bytes() == source_bytes
        report["ok"] = report["ok"] and report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path, help="只读来源，默认original_game/START.SAV")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("总览配对要求Windows WGC；未启动GUI")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"总览配对无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
