#!/usr/bin/env python3
"""B1.3a：原版与SDL肯定END，等待第二回合，再由正式UI保存活动快照。

依据 field_empty_focus_menu_execute @0x16f55、field_turn_cycle_run @0x1a30b、
ui_number_draw @0x187d6、field_secondary_menu_execute @0x19df7。
只比较列出的确定性状态；未对齐的RNG、伤害和AI结果不作为一致性判据。
Windows启动时由宿主driver run-job约束本轮整树，不与其他图形任务并行。
"""
from __future__ import annotations

import argparse
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import sys
import time

from PIL import Image

from run_graphics_tests import check_roster_count, decode_save
from run_original_comparison import Capture, archive_entry, decode_image, hashes, running, title_templates
from run_original_battle_menu import (MENU_OFFSETS, Oracle, match_indexed,
                                      resource_templates,
                                      wait_state, wait_title)

META = 0x30c3
UNITS = 0x12a3
STRIDE = 0x50


def decode_lmi(data):
    """复用portrait.c已确认的LMI编码：c>=0xc1为重复，否则为一个原色索引。"""
    if len(data) < 4:
        raise ValueError("LMI header truncated")
    width, height = struct.unpack_from("<HH", data)
    if not 0 < width <= 320 or not 0 < height <= 200:
        raise ValueError("LMI dimensions invalid")
    pixels, source = bytearray(), 4
    while len(pixels) < width * height and source < len(data):
        control = data[source]
        source += 1
        if control >= 0xc1:
            if source == len(data):
                raise ValueError("LMI run truncated")
            pixels.extend(bytes([data[source]]) * (control - 0xc0))
            source += 1
        else:
            pixels.append(control)
    if len(pixels) != width * height or source != len(data):
        raise ValueError("LMI payload length mismatch")
    return Image.frombytes("P", (width, height), bytes(pixels))


def round_two_template(resource, palette):
    sheet = archive_entry(resource.read_bytes(), 5)
    if len(sheet) < 6 or sheet[:4] != b"LMI1":
        raise ValueError("FDOTHER[5] LMI directory invalid")
    count = struct.unpack_from("<H", sheet, 4)[0]
    boundary = 6 + count * 4
    if count <= 91 or boundary > len(sheet):
        raise ValueError("FDOTHER[5] round frames missing")

    def frame(index, decoder):
        start = struct.unpack_from("<I", sheet, 6 + index * 4)[0]
        end = struct.unpack_from("<I", sheet, 6 + (index + 1) * 4)[0]
        if not boundary <= start < end <= len(sheet):
            raise ValueError("FDOTHER[5] frame bounds invalid")
        return decoder(sheet[start:end])

    banner = frame(91, decode_lmi)
    if banner.size != (87, 20):
        raise ValueError("round banner dimensions changed")
    # 原版在(120,84)显示frame91；ui_number_draw以(171,91)、6px步长写002。
    for digit, x in zip((0, 0, 2), (51, 57, 63)):
        image = frame(42 + digit, decode_image)
        if image.width > 6 or image.height != 8:
            raise ValueError("round digit dimensions changed")
        banner.paste(image, (x, 7), image.point(lambda value: 255 if value else 0, mode="L"))
    banner.putpalette(palette)
    return banner


def leader_cursor_templates(icon_path, cursor, palette):
    """按@0x11cac绘制顺序合成frame0光标及actor0，其sprite上移6px。"""
    data = icon_path.read_bytes()
    if len(data) < 6 or struct.unpack_from("<HHH", data) != (24, 24, 1680):
        raise ValueError("FDICON.B24 leader frame directory invalid")
    templates = []
    for index in range(12):
        start, finish = struct.unpack_from("<II", data, 6 + index * 4)
        if not 6 + 1680 * 4 <= start < finish <= len(data):
            raise ValueError("FDICON.B24 leader frame bounds invalid")
        sprite = decode_image(struct.pack("<HH", 24, 24) + data[start:finish])
        composite = Image.new("P", (24, 30), 0)
        composite.putpalette(palette)
        for image, position in ((cursor, (0, 6)), (sprite, (0, 0))):
            composite.paste(image, position, image.point(lambda value: 255 if value else 0, mode="L"))
        templates.append(composite)
    return templates


def round_assets(original):
    assets = resource_templates(original / "FDOTHER.DAT", original / "FDTXT.DAT")
    assets["leader_cursors"] = leader_cursor_templates(original / "FDICON.B24", assets["cursor"], assets["palette"])
    sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 5)
    if len(sheet) < 6 or sheet[:4] != b"LMI1":
        raise ValueError("FDOTHER[5] dialog directory invalid")
    count = struct.unpack_from("<H", sheet, 4)[0]
    if count <= 21 or 6 + count * 4 > len(sheet):
        raise ValueError("FDOTHER[5] dialog directory truncated")
    start, finish = struct.unpack_from("<II", sheet, 6 + 21 * 4)
    if not 6 + count * 4 <= start < finish <= len(sheet):
        raise ValueError("FDOTHER[5] dialog frame bounds invalid")
    dialog = decode_lmi(sheet[start:finish])
    if dialog.size != (310, 99):
        raise ValueError("FDOTHER[5] dialog frame dimensions invalid")
    assets["dialog_top"] = dialog.crop((0, 0, 310, 2))
    assets["dialog_top"].putpalette(assets["palette"])
    return assets


def validate_initial(path, shape):
    plain = decode_save(path)
    count = plain[META + 1]
    units = [plain[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(count)]
    if plain[0] != shape or plain[META + 2] != 0 or plain[META] != 1 or count != 12:
        raise ValueError("END comparison requires the initial stage 0 turn 1 twelve-actor snapshot")
    check_roster_count(plain)
    cx, cy, fx, fy, rx, ry = plain[META + 3:META + 9]
    if not (cx <= 11 and cy <= 16 and fx < 24 and fy < 24 and
            (fx - cx, fy - cy) == (rx, ry) and rx < 13 and ry < 8):
        raise ValueError("initial fixture camera/focus coordinates inconsistent or off screen")
    focus = (4 + rx * 24, 4 + ry * 24)
    if sum(unit[6] == 2 for unit in units) != 4 or sum(unit[6] == 0 for unit in units) != 8:
        raise ValueError("initial stage 0 sides differ from the supported fixture")
    for unit in units:
        hp, maximum = struct.unpack_from("<HH", unit, 0x40)
        # START包含隐藏敌军；magic位图+0x1a..+0x1e不是这些状态字段，原样允许。
        if unit[5] & 0x84 or hp == 0 or any(unit[0x22:0x28]) or (unit[6] == 2 and hp != maximum):
            raise ValueError("END fixture requires unacted actors, no statuses, and full player HP")
    if units[0][6] != 2 or units[0][7] != 0 or units[0][0] == 0:
        raise ValueError("leader cannot provide the left-adjacent Save checkpoint")
    return plain, focus


def empty_focus_navigation(plain):
    """只在当前镜头内有界寻找空格；每步仍由WGC确认，不修改源快照。"""
    cx, cy, fx, fy = plain[META + 3:META + 7]
    occupied = {(plain[UNITS + i * STRIDE], plain[UNITS + i * STRIDE + 1])
                for i in range(plain[META + 1]) if not plain[UNITS + i * STRIDE + 5] & 1}
    queue = deque([((fx, fy), [])])
    visited = {(fx, fy)}
    while queue:
        (x, y), steps = queue.popleft()
        sx, sy = 4 + (x - cx) * 24, 4 + (y - cy) * 24
        # cursor 在单位之前绘制；下格 actor 的头部会覆盖其底缘。
        if (x, y) not in occupied and (x, y + 1) not in occupied and 28 <= sx <= 268 and 24 <= sy <= 152:
            return steps
        for key, dx, dy in (("LEFT", -1, 0), ("RIGHT", 1, 0), ("UP", 0, -1), ("DOWN", 0, 1)):
            nx, ny = x + dx, y + dy
            # 不跨原版镜头滚动阈值；当前START焦点在此范围内。
            if (nx, ny) not in visited and 2 <= nx - cx <= 10 and 2 <= ny - cy <= 5:
                visited.add((nx, ny))
                position = (4 + (nx - cx) * 24, 4 + (ny - cy) * 24)
                queue.append(((nx, ny), steps + [(key, position)]))
    raise ValueError("fixture has no bounded empty-focus path without camera scrolling")


def check_round_phase_log(text):
    """本脚本只核对一次提交与阶段顺序，不把其他夹具的remaining当成原版事实。"""
    prefixes = ("field end turn: turn=1 phase=2 ", "field phase: turn=1 phase=1 ",
                "field phase: turn=1 phase=0 ", "field phase: turn=2 phase=2 ")
    lines = text.splitlines()
    positions = []
    for prefix in prefixes:
        matches = [index for index, line in enumerate(lines) if line.startswith(prefix)]
        if len(matches) != 1:
            raise AssertionError("END/phase checkpoint missing or repeated")
        positions.append(matches[0])
    if positions != sorted(positions):
        raise AssertionError("END phase checkpoints out of order")
    if any(line.startswith("field phase:") and "turn=3 " in line for line in lines):
        raise AssertionError("one END advanced a second round")


def check_saved_round(before, after, metadata_size):
    if len(after) != 0x59cb or sum(after[:-4]) != int.from_bytes(after[-4:], "little"):
        raise AssertionError("saved round checksum/size invalid")
    count = before[META + 1]
    roster_count = check_roster_count(before)
    if after[META:META + 3] != bytes((2, count, 0)):
        raise AssertionError("END did not save exactly turn 2, stage 0, and the original actor count")
    if check_roster_count(after) != roster_count:
        raise AssertionError("first END changed the independent persistent roster count")
    expected_focus = [before[UNITS] - 1, before[UNITS + 1]]
    if list(after[META + 5:META + 7]) != expected_focus:
        raise AssertionError(f"new player round did not return to leader before Save: expected left cell {expected_focus}")
    x, y, fx, fy, rx, ry = after[META + 3:META + 9]
    if (fx - x, fy - y) != (rx, ry):
        raise AssertionError("saved focus/camera relative coordinates inconsistent")
    if after[META + 10:META + 18] != before[META + 10:META + 18]:
        raise AssertionError("first END changed resource or configuration fields")
    # CONTINUE从同一源A导入完整roster，此首轮没有招募，未用尾应仍为A；
    # 这不是fresh session覆盖另一容器时保留目标roster尾的规则。
    for start, end in ((0, metadata_size), (metadata_size, 0x8a3),
                       (0x8a3 + roster_count * STRIDE, UNITS),
                       (UNITS + count * STRIDE, 0x30a3), (0x30a3, META),
                       (META + 18, 0x59c7)):
        if after[start:end] != before[start:end]:
            raise AssertionError(f"unexpected first-round or unrelated save change in {start:#x}:{end:#x}")
    for index in range(count):
        offset = UNITS + index * STRIDE
        if after[offset + 5] & 0x80:
            raise AssertionError(f"new player round left actor {index} acted")
        if after[offset + 6:offset + 9] != before[offset + 6:offset + 9]:
            raise AssertionError(f"first END changed actor {index} identity/side")
    return {"turn": 2, "stage": 0, "unit_count": count, "roster_count": roster_count,
            "resource_and_options": list(after[META + 10:META + 18]),
            "cell_state": list(after[0x30a3:META]), "focus": expected_focus,
            "all_acted_cleared": True, "unrelated_bytes_preserved": True,
            "observed_camera": [x, y], "camera_comparison": "excluded: AI camera path is not RNG-aligned"}


class RoundOracle(Oracle):
    def closed_browse(self, frame):
        # 确认/自动提示共用FDOTHER[5] frame21上边沿。不能把随镜头和
        # 地形变动的信息缩略图当固定ROI；边沿消失后还需完整浏览光标。
        detail = self.browse(frame)
        border = match_indexed(frame, self.assets["dialog_top"], (5, 112))
        if detail and not border["ok"]:
            return {**detail, "dialog_top_resource_absent": True}
        return None

    def initial_browse(self, frame):
        # 真实 START 焦点下方的 actor 遮住 cursor 底三行。
        # 初始到达仅核对上方已知可见 21 行，随后导航到完整可见的空格。
        cursor = match_indexed(frame, self.assets["cursor"].crop((0, 0, 24, 21)), self.focus)
        if cursor["ok"] and not self.menu(frame) and not self.buttons(frame):
            return {"kind": "initial_browse", "visible_cursor_rows": 21, "cursor": cursor,
                    "focus_screen": list(self.focus)}
        return None

    def secondary(self, frame, selected, *, overview_available=True, load_available=True):
        if selected == 2 and not load_available:
            return None
        matches = []
        for item, command in enumerate((12, 13, 14, 15)):
            position = tuple(a + b for a, b in zip(self.focus, MENU_OFFSETS[item]))
            if item == 0 and not overview_available:
                # 原资源的disabled frame38完全透明；不把零像素当匹配。
                # 排除此SDL能力项，同时要求普通/高亮总览图不存在。
                if self.choices(frame, (36, 37), position):
                    return None
                matches.append({"capability": "overview unavailable", "excluded": True})
                continue
            states = (2,) if item == 2 and not load_available else (
                (0, 1) if selected is None else (1,) if selected == item else (0,))
            found = self.choices(frame, [command * 3 + state for state in states], position)
            if not found:
                return None
            matches.append(found)
        return {"kind": "secondary_menu", "selected": selected, "matches": matches}


def browse_anywhere(frame, assets, *, leader=False):
    matches = []
    for y in range(4, 173, 24):
        for x in range(4, 293, 24):
            oracle = RoundOracle(assets, (x, y))
            if leader:
                # 整个光标加原版角色合成图证明焦点已到actor0；不能在逐格
                # 聚焦动画途中的空格上看到光标就提前发送下一键。
                matched = False
                for template in assets["leader_cursors"]:
                    top = max(0, 6 - y)
                    if match_indexed(frame, template.crop((0, top, 24, 30)), (x, y - 6 + top))["ok"]:
                        matched = not oracle.menu(frame) and not oracle.buttons(frame)
                        break
            else:
                matched = oracle.browse(frame)
            if matched:
                matches.append((x, y))
    if len(matches) == 1:
        return {"kind": "browse", "focus_screen": list(matches[0])}
    return None


def exercise(capture, kind, assets, initial_focus, titles, banner, save, log, report,
             *, initial_steps=()):
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    oracle = RoundOracle(assets, initial_focus)

    def checkpoint(name, predicate, **kwargs):
        return wait_state(capture, kind + "-" + name, predicate, report, **kwargs)

    checkpoint("initial-browse", oracle.initial_browse, timeout=30)
    for index, (key, position) in enumerate(initial_steps, 1):
        capture.key(key)
        oracle = RoundOracle(assets, position)
        checkpoint(f"initial-empty-step-{index}", oracle.browse)
    capture.key("ENTER")
    checkpoint("parent", lambda frame: oracle.menu(frame, 0))
    capture.key("DOWN")
    checkpoint("end-selected", lambda frame: oracle.menu(frame, 3))
    capture.key("ENTER")
    checkpoint("default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")  # 只提交这一次，随后仅观察，不补发按键。
    if kind == "original":
        def round_banner(frame):
            matched = match_indexed(frame, banner, (120, 84))
            return {"kind": "round_banner", "turn": 2, **matched} if matched["ok"] else None
        checkpoint("round-002", round_banner, timeout=90)
    else:
        from field_play_input_test import wait_log
        text = wait_log(capture.process, log, "field phase: turn=2 phase=2 ", timeout=90)
        check_round_phase_log(text)
    checkpoint("new-player-browse", lambda frame: browse_anywhere(frame, assets, leader=True), timeout=30)
    capture.key("LEFT")
    _, detail = checkpoint("save-cell", lambda frame: browse_anywhere(frame, assets))
    oracle = RoundOracle(assets, tuple(detail["focus_screen"]))
    capture.key("ENTER")
    checkpoint("save-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("UP")
    capture.key("ENTER")
    checkpoint("secondary", lambda frame: oracle.secondary(frame, 0, overview_available=True))
    capture.key("LEFT")
    checkpoint("save-enabled", lambda frame: oracle.secondary(frame, 1, overview_available=True))
    capture.key("ENTER")
    checkpoint("save-default-yes", lambda frame: oracle.buttons(frame, 0))
    previous = save.read_bytes()
    capture.key("ENTER")
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if save.read_bytes() != previous:
            try:
                saved = decode_save(save)
                break
            except AssertionError:
                pass  # 原版fopen(wb)至fclose期间可能暂时读到未写完的文件。
        time.sleep(0.05)
    else:
        raise TimeoutError("Save did not produce a new complete checksum-valid snapshot")
    checkpoint("saved-browse", oracle.closed_browse)
    return saved


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = root / "original_game"
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    output.mkdir(parents=True, exist_ok=False)
    before_hashes, fixture_bytes = hashes(original), fixture.read_bytes()
    report = {"ok": False, "scope": "one affirmative END -> turn 2 -> active Save; deterministic fields only",
              "excluded": "RNG alignment, damage/hit/critical, AI positions/targets, camera equality, active roster-entry contents, other actor bytes, full-frame visuals, Load, victory/defeat",
              "platform": platform.platform(), "capture": "Windows.Graphics.Capture HWND only",
              "fixture_sha256": hashlib.sha256(fixture_bytes).hexdigest(),
              "synthetic_fixture": False, "original_before": before_hashes,
              "processes": [], "inputs": [], "checkpoints": []}
    try:
        report["binaries"] = {key: {"path": str(path.resolve()),
                                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for key, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
        plain, focus = validate_initial(fixture, metadata[0])
        initial_steps = empty_focus_navigation(plain)
        visible_counts = tuple(sum(plain[UNITS + i * STRIDE + 6] == side and
                                    not plain[UNITS + i * STRIDE + 5] & 1
                                    for i in range(plain[META + 1])) for side in (2, 1, 0))
        report["fixture"] = {"path": str(fixture), "active_count": plain[META + 1],
                             "roster_count": check_roster_count(plain),
                             "initial_visible_counts": list(visible_counts), "initial_empty_navigation": initial_steps}
        assets = round_assets(original)
        banner = round_two_template(original / "FDOTHER.DAT", assets["palette"])
        banner.save(output / "round-002-resource.png")
        titles = title_templates(original / "FDOTHER.DAT")
        game_copy = output / "game"
        shutil.copytree(original, game_copy)
        shutil.copy2(fixture, game_copy / "FD2.SAV")
        sdl_save = output / "SDL-FD2.SAV"
        shutil.copy2(fixture, sdl_save)
        config = output / "dosbox.conf"
        config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                          "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n"
                          "[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n"
                          f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
        environment = os.environ.copy()
        for variable in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
            environment.pop(variable, None)
        snapshots = {}
        report["saved_rounds"] = snapshots
        for kind, command, cwd, save in (
                ("original", [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], output, game_copy / "FD2.SAV"),
                ("sdl", [str(args.game.resolve())], root, sdl_save)):
            if kind == "sdl":
                environment.update(FD2SDL_SAVE_PATH=str(save), FD2SDL_FIELD_RNG_SEED="31256")
            log = output / f"{kind}.log"
            with running(command, cwd, environment, log, args.driver, report["processes"]) as process:
                capture = Capture(args.driver, process, output, report["inputs"])
                saved = exercise(capture, kind, assets, focus, titles, banner, save, log, report["checkpoints"],
                                 initial_steps=initial_steps)
                shutil.copy2(save, output / f"{kind}-round2.SAV")
                snapshots[kind] = check_saved_round(plain, saved, len(metadata))
        deterministic = lambda values: {key: value for key, value in values.items()
                                         if key not in ("observed_camera", "camera_comparison")}
        report["deterministic_state_equal"] = deterministic(snapshots["original"]) == deterministic(snapshots["sdl"])
        if not report["deterministic_state_equal"]:
            raise AssertionError("original/SDL deterministic round/save state differs")
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_unchanged"] = hashes(original) == before_hashes
        report["fixture_unchanged"] = fixture.read_bytes() == fixture_bytes
        report["ok"] = report["ok"] and report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path,
                        help="只读真实快照；默认original_game/START.SAV，仅复制到隔离目录")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("此原版END流程要求 Windows WGC；未启动 GUI")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"原版END对照无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
