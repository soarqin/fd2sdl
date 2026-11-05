#!/usr/bin/env python3
"""START玩家移动取消／满HP原地WAIT：原版先行，再验证SDL资源与状态门。

依据 @0x117e7/@0x18890/@0x18d8c，资源来自只读原版目录。
由总runner的driver run-job串行执行；仅WGC。图标资源绘制不等于菜单能力门或默认选择一致。
"""
from collections import deque
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys

from PIL import Image

ROOT = next(parent for parent in Path(__file__).resolve().parents
            if (parent / "tools/run_original_end_turn.py").is_file())
sys.path.insert(0, str(ROOT / "tools"))
from run_original_comparison import Capture, TOLERANCE, archive_entry, decode_image, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, wait_state, wait_title
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, browse_anywhere, decode_lmi, round_assets, validate_initial
from run_original_load import require_source_unchanged
from fd2_movement_oracle import movement_profile

def cursor_assets(icon_path, source, assets):
    """仅当前源actor身份的真实FDICON帧；未知/遮挡不猜测，找不到就失败。"""
    data = icon_path.read_bytes()
    if len(data) < 6 or struct.unpack_from("<HHH", data) != (24, 24, 1680):
        raise ValueError("FDICON directory unsupported")
    identities = sorted({(source[UNITS + i * STRIDE + 7], bool(source[UNITS + i * STRIDE + 5] & 0x80))
                         for i in range(source[META + 1])})
    sprites, composites = [], []
    for identity, is_acted in identities:
        for phase in range(12):
            index = identity * 12 + phase
            if index >= 1680:
                raise ValueError("source actor ID outside FDICON")
            start = struct.unpack_from("<I", data, 6 + index * 4)[0]
            end = struct.unpack_from("<I", data, 6 + (index + 1) * 4)[0] if index < 1679 else len(data)
            if not 6 + 1680 * 4 <= start < end <= len(data):
                raise ValueError("FDICON sprite bounds invalid")
            sprite = decode_image(struct.pack("<HH", 24, 24) + data[start:end])
            if is_acted:
                sprite = acted_sprite(sprite)
            sprite.putpalette(assets["palette"])
            sprites.append(sprite)
            # 当前格actor上移6px；下一格actor可能遮住空格cursor底缘。
            for height, cursor_y, sprite_y, offset in ((30, 6, 0, -6), (42, 0, 18, 0)):
                image = Image.new("P", (24, height), 0)
                image.putpalette(assets["palette"])
                for part, y in ((assets["cursor"], cursor_y), (sprite, sprite_y)):
                    image.paste(part, (0, y), part.point(lambda value: 255 if value else 0, mode="L"))
                composites.append((image, offset))
    return sprites, composites


def visual_focus(frame, assets, composites):
    matches = []
    for row in range(1, 7):
        for column in range(1, 12):
            x, y = 4 + column * 24, 4 + row * 24
            oracle = RoundOracle(assets, (x, y))
            matched = match_indexed(frame, assets["cursor"], (x, y))["ok"]
            if not matched:
                matched = any(match_indexed(frame, image, (x, y + offset))["ok"] for image, offset in composites)
            if matched and not oracle.menu(frame) and not oracle.buttons(frame) and not match_indexed(
                    frame, assets["dialog_top"], (5, 112))["ok"]:
                matches.append((column, row))
    if len(matches) == 1:
        return {"kind": "resource_proven_focus", "cell_screen": list(matches[0]),
                "focus_screen": [4 + matches[0][0] * 24, 4 + matches[0][1] * 24]}
    return None



DIRECTIONS = (("LEFT", -1, 0), ("RIGHT", 1, 0), ("UP", 0, -1), ("DOWN", 0, 1))


def sheet_frame(sheet, index, *, lmi=False):
    if lmi:
        if sheet[:4] != b"LMI1":
            raise ValueError("LMI目录无效")
        count, base = struct.unpack_from("<H", sheet, 4)[0], 6
    else:
        width, height, count = struct.unpack_from("<HHH", sheet)
        if (width, height) != (24, 24):
            raise ValueError("地形帧不是24x24")
        base = 6
    if not 0 <= index < count or base + count * 4 > len(sheet):
        raise ValueError("资源帧索引越界")
    start = struct.unpack_from("<I", sheet, base + index * 4)[0]
    end = struct.unpack_from("<I", sheet, base + (index + 1) * 4)[0] if index + 1 < count else len(sheet)
    if not base + count * 4 <= start < end <= len(sheet):
        raise ValueError("资源帧长度无效")
    return decode_lmi(sheet[start:end]) if lmi else decode_image(struct.pack("<HH", 24, 24) + sheet[start:end])


def screen(source, world):
    return (4 + (world[0] - source[META + 3]) * 24,
            4 + (world[1] - source[META + 4]) * 24)


def navigation(source, target):
    camera = tuple(source[META + 3:META + 5])
    start = tuple(source[META + 5:META + 7])
    queue, visited = deque([(start, [])]), {start}
    while queue:
        (x, y), path = queue.popleft()
        if (x, y) == target:
            return path
        for key, dx, dy in DIRECTIONS:
            rx, ry = x - camera[0], y - camera[1]
            if (dx < 0 and rx <= 2) or (dx > 0 and rx >= 11) or (dy < 0 and ry <= 2) or (dy > 0 and ry >= 6):
                continue
            cell = x + dx, y + dy
            if cell not in visited and 1 <= cell[0] - camera[0] <= 11 and 1 <= cell[1] - camera[1] <= 6:
                visited.add(cell)
                queue.append((cell, path + [(key, cell)]))
    raise ValueError("源快照无法在不滚屏的有界路径内导航至actor0")


def require_full_hp(source):
    hp, maximum = struct.unpack_from("<HH", source, UNITS + 0x40)
    if hp == 0 or hp != maximum:
        raise ValueError("WAIT场景只支持源主角HP==max且非0；未覆盖受伤恢复分支")
    return {"hp": hp, "max_hp": maximum, "injured_recovery_branch_covered": False}


def acted_sprite(sprite):
    """sub_4E1A6 @0x4e1a6：只映实际opaque非0 sprite，不能映cursor。"""
    image = Image.frombytes("P", sprite.size, bytes((color & 7) + 24 if color else 0 for color in sprite.getdata()))
    image.putpalette(sprite.getpalette())
    return image


def upper_command_composite(icon, sprite):
    """field_command_menu_draw @0x179d5末尾单actor覆盖上icon底6行。"""
    image, overlap = icon.copy(), sprite.crop((0, 0, 24, 6))
    image.paste(overlap, (0, 14), overlap.point(lambda color: 255 if color else 0, mode="L"))
    return image


def prepare(original, fixture):
    metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
    source, focus = validate_initial(fixture, metadata[0])
    actor = source[UNITS:UNITS + STRIDE]
    if actor[7] != 0 or actor[0x20] != 1 or actor[0x3b] == 0:
        raise ValueError("本场景只支持真实START主角的已确认profile1")
    leader = tuple(actor[:2])
    path = navigation(source, leader)
    map_data = archive_entry((original / "FDFIELD.DAT").read_bytes(), 0)
    width, height = struct.unpack_from("<HH", map_data)
    if len(map_data) != 4 + width * height * 4:
        raise ValueError("地图长度无效")
    shape_data = (original / "FDSHAP.DAT").read_bytes()
    terrain = archive_entry(shape_data, metadata[0] * 2)
    attrs = archive_entry(shape_data, metadata[0] * 2 + 1)
    # canonical object3表由既有LE page map提取，禁止硬编码历史file/runtime偏移。
    profile, profile_evidence = movement_profile(original / "FD2.EXE", actor[0x20])
    units = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(source[META + 1])]
    occupied = {tuple(u[:2]) for u in units if not u[5] & 1}
    enemies = [tuple(u[:2]) for u in units if not u[5] & 1 and u[6] == 0]
    candidates = []
    for key, dx, dy in DIRECTIONS:
        cell = leader[0] + dx, leader[1] + dy
        rx, ry = cell[0] - source[META + 3], cell[1] - source[META + 4]
        if not (2 <= rx <= 10 and 2 <= ry <= 5) or cell in occupied or (cell[0], cell[1] + 1) in occupied:
            continue
        if any(abs(x - leader[0]) + abs(y - leader[1]) <= 1 or abs(x - cell[0]) + abs(y - cell[1]) <= 1 for x, y in enemies):
            continue
        value = struct.unpack_from("<I", map_data, 4 + (cell[1] * width + cell[0]) * 4)[0]
        tile = value & 0x3ff
        if tile * 4 + 4 > len(attrs):
            raise ValueError("地形属性越界")
        flags, cost_class = attrs[tile * 4:tile * 4 + 2]
        if flags or (value >> 16) & 31 or cost_class >= len(profile) or not 0 < profile[cost_class] <= min(actor[0x3b], 19):
            continue
        image = sheet_frame(terrain, tile)
        if 0 in image.getdata():
            continue
        candidates.append((key, cell, image, tile, cost_class, profile[cost_class]))
    if not candidates:
        raise ValueError("没有无事件/无占用/无ZOC/静态底图的一格合法候选")
    key, destination, tile, tile_id, cost_class, cost = candidates[0]
    assets = round_assets(original)
    tile.putpalette(assets["palette"])
    assets["probe_base_tile"] = tile
    sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
    if sheet[:4] != b"LMI1":
        raise ValueError("范围LUT目录无效")
    count = struct.unpack_from("<H", sheet, 4)[0]
    ranges = []
    for index in (0, 1, 2, 4, 5, 8):
        if index >= count:
            raise ValueError("范围LUT缺失")
        start = struct.unpack_from("<I", sheet, 6 + index * 4)[0]
        end = struct.unpack_from("<I", sheet, 6 + (index + 1) * 4)[0] if index + 1 < count else len(sheet)
        lut = sheet[start:end]
        if len(lut) != 256:
            raise ValueError("范围LUT长度无效")
        image = Image.frombytes("P", (24, 24), bytes(lut[p] for p in tile.getdata()))
        image.putpalette(assets["palette"])
        changed = sum(max(abs(a - b) for a, b in zip(want, got)) > TOLERANCE
                      for want, got in zip(tile.convert("RGB").getdata(), image.convert("RGB").getdata()))
        if changed >= 24:
            ranges.append((index, image))
    if not ranges:
        raise ValueError("范围模板与普通地形没有足够的可分辨像素")
    _, composites = cursor_assets(original / "FDICON.B24", source, assets)
    icons = (original / "FDICON.B24").read_bytes()
    assets["leader_sprites"] = [sheet_frame(icons, actor[7] * 12 + phase) for phase in range(12)]
    for image in assets["leader_sprites"]:
        image.putpalette(assets["palette"])
    assets["upper_commands"] = {index: [upper_command_composite(assets["commands"][index], sprite)
                                        for sprite in assets["leader_sprites"]]
                                 for index in range(3)}
    assets["acted_leader_cursors"] = []
    for sprite in assets["leader_sprites"]:
        composite = Image.new("P", (24, 30), 0)
        composite.putpalette(assets["palette"])
        for part, y in ((assets["cursor"], 6), (acted_sprite(sprite), 0)):
            composite.paste(part, (0, y), part.point(lambda color: 255 if color else 0, mode="L"))
        assets["acted_leader_cursors"].append(composite)
    detail = archive_entry((original / "FDOTHER.DAT").read_bytes(), 5)
    assets["detail_edges"] = [(sheet_frame(detail, index, lmi=True).crop((0, 0, width, 2)), at)
                              for index, width, at in ((20, 223, (92, 7)), (21, 310, (5, 94)))]
    for image, _ in assets["detail_edges"]:
        image.putpalette(assets["palette"])
    plan = {"leader_world": list(leader), "leader_screen": list(screen(source, leader)),
            "navigation": path, "step_key": key, "destination_world": list(destination),
            "destination_screen": list(screen(source, destination)), "tile_id": tile_id,
            "cost_class": cost_class, "cost": cost, "movement_profile_oracle": profile_evidence, "range_lut_indices": [i for i, _ in ranges]}
    return source, focus, assets, composites, ranges, plan


def exact_tile(frame, template, position):
    x, y = position
    if x < 0 or y < 0 or x + template.width > 320 or y + template.height > 200:
        return None
    deltas = [max(abs(a - b) for a, b in zip(want, got))
              for want, got in zip(template.convert("RGB").getdata(), frame.crop((x, y, x + template.width, y + template.height)).convert("RGB").getdata())]
    return {"checked_pixels": len(deltas), "max_channel_delta": max(deltas)} if deltas and max(deltas) <= TOLERANCE else None


class PlayerOracle:
    def __init__(self, source, assets, ranges, plan):
        self.source, self.assets, self.ranges, self.plan = source, assets, ranges, plan

    def command(self, frame, position, selected=None):
        matches = []
        for item in range(4):
            at = tuple(a + b for a, b in zip(position, MENU_OFFSETS[item]))
            states = (1,) if item == selected else (0, 2) if selected is not None else (0, 1, 2)
            found = None
            for state in states:
                index = item * 3 + state
                templates = self.assets["upper_commands"][index] if item == 0 else [self.assets["commands"][index]]
                for phase, template in enumerate(templates):
                    check = match_indexed(frame, template, at)
                    if check["ok"]:
                        found = {"frame": index, "position": list(at), "resource_rows": template.height,
                                 "actor_frame_candidate": phase if item == 0 else None, **check}
                        break
                if found:
                    break
            if not found:
                return None
            matches.append(found)
        return {"kind": "player_command_resource_overlay", "full_resource_height_checked": True,
                "capability_gates_compared": False, "default_selection_compared": False, "focus_screen": list(position), "selected": selected, "matches": matches}

    def selection(self, frame, *, moved_focus=False):
        leader = browse_anywhere(frame, self.assets, leader=True) if not moved_focus else None
        destination = tuple(self.plan["destination_screen"])
        leader_at = tuple(self.plan["leader_screen"])
        if not moved_focus and (not leader or tuple(leader["focus_screen"]) != leader_at):
            return None
        if self.command(frame, leader_at) or self.command(frame, destination):
            return None
        for index, original in self.ranges:
            image = original.copy()
            if moved_focus:
                cursor = self.assets["cursor"]
                image.paste(cursor, (0, 0), cursor.point(lambda value: 255 if value else 0, mode="L"))
            match = exact_tile(frame, image, destination)
            if match:
                return {"kind": "move_selection", "focus_world": self.plan["destination_world"] if moved_focus else self.plan["leader_world"],
                        "range_tile_world": self.plan["destination_world"], "range_lut": index, **match}
        return None

    def detail(self, frame):
        checks = [match_indexed(frame, image, at) for image, at in self.assets["detail_edges"]]
        return {"kind": "native_unit_detail_edge_resources", "full_page_visual_equivalence": False,
                "edge_resources": checks} if all(check["ok"] for check in checks) else None

    def browse(self, frame, *, acted=False):
        leader_at = tuple(self.plan["leader_screen"])
        assets = dict(self.assets, leader_cursors=self.assets["acted_leader_cursors"]) if acted else self.assets
        leader = browse_anywhere(frame, assets, leader=True)
        if not leader or tuple(leader["focus_screen"]) != leader_at or self.command(frame, leader_at):
            return None
        base = exact_tile(frame, self.assets["probe_base_tile"], tuple(self.plan["destination_screen"]))
        return {"kind": "acted_browse_with_untranslated_terrain" if acted else "browse_with_untranslated_terrain",
                "leader": leader, "base_tile": base, "acted_sprite_proven": acted} if base else None


def exercise(capture, source, focus, assets, composites, ranges, plan, titles, save, data, mtime, report, scenario="move-cancel"):
    if scenario == "wait-full-hp":
        require_full_hp(source)
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    def checkpoint(name, predicate, **kwargs):
        frame, detail = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        require_source_unchanged(save, data, mtime)
        return frame, detail
    checkpoint("initial-browse", RoundOracle(assets, focus).initial_browse, timeout=30)
    for index, (key, world) in enumerate(plan["navigation"], 1):
        capture.key(key)
        expected = screen(source, world)
        def reached(frame):
            result = visual_focus(frame, assets, composites)
            return result if result and tuple(result["focus_screen"]) == expected else None
        checkpoint(f"actor0-navigation-{index}", reached)
    leader_at = tuple(plan["leader_screen"])
    def leader(frame):
        match = browse_anywhere(frame, assets, leader=True)
        return match if match and tuple(match["focus_screen"]) == leader_at else None
    checkpoint("actor0-resource-proven", leader)
    oracle = PlayerOracle(source, assets, ranges, plan)
    capture.key("ENTER")
    checkpoint("actor0-selection", oracle.selection)
    if scenario == "move-cancel":
        capture.key(plan["step_key"])
        checkpoint("one-step-focus-selected", lambda frame: oracle.selection(frame, moved_focus=True))
        capture.key("ENTER")
        checkpoint("one-step-command", lambda frame: oracle.command(frame, tuple(plan["destination_screen"])))
        capture.key("ESC")
        checkpoint("cancel-restored-selection", oracle.selection)
        capture.key("ENTER")
        checkpoint("restored-zero-step-direct-command", lambda frame: oracle.command(frame, leader_at))
        capture.key("ESC")
        checkpoint("second-command-cancel-selection", oracle.selection)
        capture.key("ESC")
        checkpoint("selection-cancel-browse", oracle.browse)
        report["observed"] = "一步移动后COMMAND取消恢复actor0/焦点/selection；再Enter直接COMMAND；selection取消回browse"
    else:
        capture.key("ENTER")
        checkpoint("zero-step-command", lambda frame: oracle.command(frame, leader_at))
        capture.key("DOWN")
        checkpoint("wait-highlight-proven", lambda frame: oracle.command(frame, leader_at, 3))
        capture.key("ENTER")
        checkpoint("wait-acted-closed-browse", lambda frame: oracle.browse(frame, acted=True))
        capture.key("ENTER")
        checkpoint("acted-enter-opens-detail", oracle.detail)
        report["observed"] = "满HP原地WAIT后acted sprite+cursor返回browse；Enter原生详情页；未覆盖受伤恢复"



def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents or output == fixture or output in fixture.parents:
        raise ValueError("产物目录不能覆盖original_game或源夹具")
    output.mkdir(parents=True, exist_ok=False)
    before, data, fixture_mtime = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "scenario": args.scenario, "capture": "Windows.Graphics.Capture HWND only",
              "scope": "START actor0 move-cancel or full-HP WAIT lifecycle, full-height command resource/actor overlay and acted sprite/cursor; disk read-only",
              "excluded": ["受伤HP恢复", "菜单能力门/默认选择跨端一致性", "详情页完整像素一致", "瞬时动画", "全地图/idle像素一致", "攻击目标取消", "未测量的完整runtime字段/RNG"],
              "command_icon_scope": {"upper_visible_rows": 20, "other_icons_rows": 20,
                                     "resource_overlay_checked": True, "capability_gates_compared": False,
                                     "default_selection_compared": False, "full_menu_visual_equivalence": False},
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(data).hexdigest(), "mtime_ns": fixture_mtime},
              "original_before": before, "inputs": [], "checkpoints": [], "processes": [], "cases": []}
    try:
        source, focus, assets, composites, ranges, plan = prepare(original, fixture)
        report["plan"] = plan
        if args.scenario == "wait-full-hp":
            report["wait_precondition"] = require_full_hp(source)
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                               for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        report["runner_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        titles = title_templates(original / "FDOTHER.DAT")
        for kind in ("original", "sdl"):
            work = output / kind
            work.mkdir()
            environment = os.environ.copy()
            for key in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
                environment.pop(key, None)
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
            mtime = save.stat().st_mtime_ns
            case = {"name": kind, "status": "failed", "checkpoints": [], "source_mtime_ns_before_start": mtime}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, work / "game.log", args.driver, report["processes"]) as process:
                    case["pid"] = process.pid
                    capture = Capture(args.driver, process, work, report["inputs"])
                    exercise(capture, source, focus, assets, composites, ranges, plan, titles,
                             save, data, mtime, case, args.scenario)
                    require_source_unchanged(save, data, mtime)
                require_source_unchanged(save, data, mtime)
                case.update(status="passed", disk_and_mtime_unchanged=True,
                            source_mtime_ns_after_cleanup=save.stat().st_mtime_ns,
                            full_runtime_fields_measured=False)
            except Exception as error:
                case["error"] = str(error)
                raise
            finally:
                report["checkpoints"].extend({"case": kind, **item} for item in case["checkpoints"])
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before
        report["fixture_unchanged"] = fixture.read_bytes() == data and fixture.stat().st_mtime_ns == fixture_mtime
        report["ok"] &= report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path, help="只读来源，默认original_game/START.SAV")
    parser.add_argument("--scenario", choices=("move-cancel", "wait-full-hp"), default="move-cancel")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("玩家移动取消配对要求Windows WGC；未启动GUI")
    try:
        result = run(args)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result["ok"] else 1
    except Exception as error:
        print(f"玩家移动取消配对无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
