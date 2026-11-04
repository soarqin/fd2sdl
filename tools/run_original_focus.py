#!/usr/bin/env python3
"""受控 START 边缘镜头：原版两场先行，再验证 SDL 的 Z／选择／COMMAND。

依据 field_focus_move_to @0x12cea 与 field_controller_input @0x117e7；源相机及焦点
不变，只派生 actor0 坐标与 checksum。不是自然开场或完整菜单能力验收。
由 Windows driver run-job 串行执行；仅使用共享 WGC HWND 捕获及所属进程清理。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from run_graphics_tests import decode_save
from run_original_comparison import Capture, TOLERANCE, archive_entry, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, wait_state, wait_title
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle, browse_anywhere, round_assets, validate_initial
from run_original_load import require_source_unchanged
from run_original_player import DIRECTIONS, PlayerOracle, exact_tile, sheet_frame, upper_command_composite
from fd2_movement_oracle import movement_profile

# 专用合成前置，不是游戏默认位置；第二场避开全水邻格以严格检查完整范围贴图。
TARGETS = (("upper-left", (2, 5)), ("lower-right", (19, 21)))


def native_focus_reference(camera, focus, target, dimensions=(24, 24)):
    """发布的 @0x12cea X→Y 单步规则；阈值来自 @0x117e7 原始比较指令。"""
    cx, cy = camera
    x, y = focus
    width, height = dimensions
    if not (width >= 13 and height >= 8 and 0 <= cx <= width - 13 and
            0 <= cy <= height - 8 and 0 <= x < width and 0 <= y < height and
            0 <= target[0] < width and 0 <= target[1] < height):
        raise ValueError("聚焦参考坐标越界")
    steps = []
    while x != target[0]:
        right = x < target[0]
        if right and x - cx > 10 and cx < width - 13:
            cx += 1
        elif not right and x - cx < 2 and cx > 0:
            cx -= 1
        x += 1 if right else -1
        steps.append({"axis": "x", "world": [x, y], "camera": [cx, cy]})
    while y != target[1]:
        down = y < target[1]
        if down and y - cy > 5 and cy < height - 8:
            cy += 1
        elif not down and y - cy < 2 and cy > 0:
            cy -= 1
        y += 1 if down else -1
        steps.append({"axis": "y", "world": [x, y], "camera": [cx, cy]})
    return {"camera": [cx, cy], "screen": [4 + (x - cx) * 24, 4 + (y - cy) * 24],
            "steps": steps, "canonical_va": "0x12cea", "move_va": "0x117e7"}


def derive_fixture(source, encrypted, target):
    if len(source) != 0x59cb or len(encrypted) != len(source) or sum(source[:-4]) != int.from_bytes(source[-4:], "little"):
        raise ValueError("来源 SAV 长度／checksum 无效")
    if tuple(source[META + 3:META + 9]) != (5, 11, 8, 15, 3, 4):
        raise ValueError("边缘夹具要求已确认 START 的源 camera/focus，不能重写源镜头")
    if not (0 <= target[0] < 24 and 0 <= target[1] < 24):
        raise ValueError("合成目标超出 START 地图")
    actor = source[UNITS:UNITS + STRIDE]
    if actor[5] & 0x85 or actor[6] != 2 or actor[7] != 0 or actor[0x20] != 1 or not actor[0x3b]:
        raise ValueError("来源 actor0 不符合已确认 START 控制与 profile 前置")
    if target == tuple(actor[:2]):
        raise ValueError("合成目标必须实际改变 actor0 坐标")
    for index in range(1, source[META + 1]):
        unit = source[UNITS + index * STRIDE:UNITS + (index + 1) * STRIDE]
        if not unit[5] & 1 and tuple(unit[:2]) == target:
            raise ValueError("合成目标被其他可见 actor 占用")
    plain = bytearray(source)
    plain[UNITS:UNITS + 2] = bytes(target)
    plain[-4:] = sum(plain[:-4]).to_bytes(4, "little")
    # 复用来源同一 XOR 流，只异或已知明文差异；不另写一套编码器。
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, plain))
    changes = [{"offset": i, "before": a, "after": b,
                "owner": "actor0 x/y" if i in (UNITS, UNITS + 1) else "checksum"}
               for i, (a, b) in enumerate(zip(source, plain)) if a != b]
    allowed = {UNITS, UNITS + 1, *range(len(plain) - 4, len(plain))}
    if any(item["offset"] not in allowed for item in changes):
        raise AssertionError("派生夹具改动了坐标／checksum 之外的字段")
    return encoded, bytes(plain), {"synthetic_coordinate_fixture": True, "natural_opening_covered": False,
        "source_actor0": list(actor[:2]), "target_actor0": list(target),
        "camera_focus_unchanged": list(plain[META + 3:META + 9]),
        "changed_plain_bytes": changes,
        "changed_encrypted_bytes": [{"offset": i, "before": a, "after": b}
                                    for i, (a, b) in enumerate(zip(encrypted, encoded)) if a != b],
        "source_encrypted_sha256": hashlib.sha256(encrypted).hexdigest(),
        "source_plain_sha256": hashlib.sha256(source).hexdigest(),
        "derived_encrypted_sha256": hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256": hashlib.sha256(plain).hexdigest()}


def overlaps(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def prepare(original, source, target):
    """全部 ROI 从地图／资源导出；不引入水色排除 mask 或捕获坐标默认值。"""
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata = archive_entry(field, 1)
    map_data = archive_entry(field, 0)
    width, height = struct.unpack_from("<HH", map_data)
    if (width, height) != (24, 24) or len(map_data) != 4 + width * height * 4 or source[0] != metadata[0]:
        raise ValueError("边缘镜头只支持资源一致的 START 24x24 地图")
    cells = struct.unpack_from("<576I", map_data, 4)
    sheet = (original / "FDSHAP.DAT").read_bytes()
    terrain = archive_entry(sheet, metadata[0] * 2)
    attrs = archive_entry(sheet, metadata[0] * 2 + 1)
    assets = round_assets(original)
    profile, profile_receipt = movement_profile(original / "FD2.EXE", source[UNITS + 0x20])
    units = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(source[META + 1])]
    if any(unit[0] >= width or unit[1] >= height for unit in units):
        raise ValueError("来源 actor 坐标超出地图，不能推导无覆盖 ROI")
    occupied = {tuple(unit[:2]) for unit in units if not unit[5] & 1}
    enemies = [tuple(unit[:2]) for unit in units if not unit[5] & 1 and unit[6] == 0]
    reference = native_focus_reference(source[META + 3:META + 5], source[META + 5:META + 7], target, (width, height))
    camera, leader_at = reference["camera"], reference["screen"]
    if (leader_at[0] - 4) // 24 not in (1, 11) or (leader_at[1] - 4) // 24 not in (1, 6):
        raise ValueError("目标不是本场受控边缘镜头的完整 COMMAND 边界")
    cache = {}

    def tile_at(world):
        x, y = world
        if not (0 <= x < width and 0 <= y < height):
            raise ValueError("地形候选越界")
        cell = cells[y * width + x]
        index = cell & 0x3ff
        if index * 4 + 4 > len(attrs):
            raise ValueError("地形属性越界")
        if index not in cache:
            image = sheet_frame(terrain, index)
            image.putpalette(assets["palette"])
            cache[index] = image
        return cell, index, attrs[index * 4:index * 4 + 4], cache[index]

    def position(world):
        return (4 + (world[0] - camera[0]) * 24, 4 + (world[1] - camera[1]) * 24)

    def dry_static(world):
        cell, index, attr, image = tile_at(world)
        pixels = bytes(image.getdata())
        if attr[0] or cell >> 10 or 0 in pixels or any(224 <= pixel < 240 for pixel in pixels):
            return None
        return index, attr, image

    cell, _, attr, _ = tile_at(target)
    budget = min(source[UNITS + 0x3b], 19)
    if attr[0] or cell >> 10 or attr[1] >= len(profile) or not 0 < profile[attr[1]] <= budget:
        raise ValueError("合成 actor0 目标不是无事件且 profile 可站立的静态格")
    if any(abs(x - target[0]) + abs(y - target[1]) <= 1 for x, y in enemies):
        raise ValueError("合成 actor0 目标位于敌方 ZOC，不能提供本场独立范围门")
    ranges_sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
    if ranges_sheet[:4] != b"LMI1":
        raise ValueError("范围 LUT 目录无效")
    count = struct.unpack_from("<H", ranges_sheet, 4)[0]
    ranges, range_plan = [], None
    for _, dx, dy in DIRECTIONS:
        world = target[0] + dx, target[1] + dy
        at = position(world)
        if not (4 <= at[0] <= 292 and 4 <= at[1] <= 172) or world in occupied or (world[0], world[1] + 1) in occupied:
            continue
        if any(abs(x - world[0]) + abs(y - world[1]) <= 1 for x, y in enemies):
            continue
        candidate = dry_static(world)
        if candidate is None:
            continue
        tile_id, attr, image = candidate
        if attr[1] >= len(profile) or not 0 < profile[attr[1]] <= budget:
            continue
        for index in (0, 1, 2, 4, 5, 8):
            if index >= count or 6 + count * 4 > len(ranges_sheet):
                raise ValueError("范围 LUT 缺失")
            start = struct.unpack_from("<I", ranges_sheet, 6 + index * 4)[0]
            end = struct.unpack_from("<I", ranges_sheet, 6 + (index + 1) * 4)[0] if index + 1 < count else len(ranges_sheet)
            lut = ranges_sheet[start:end]
            if not 6 + count * 4 <= start < end <= len(ranges_sheet) or len(lut) != 256:
                raise ValueError("范围 LUT 长度／边界无效")
            pixels = bytes(lut[pixel] for pixel in image.getdata())
            if any(224 <= pixel < 240 for pixel in pixels):
                continue
            translated = Image.frombytes("P", (24, 24), pixels)
            translated.putpalette(assets["palette"])
            changed = sum(max(abs(a - b) for a, b in zip(old, new)) > TOLERANCE
                          for old, new in zip(image.convert("RGB").getdata(), translated.convert("RGB").getdata()))
            if changed >= 24:
                ranges.append((index, translated))
        if ranges:
            assets["probe_base_tile"] = image
            range_plan = {"world": list(world), "screen": list(at), "tile_id": tile_id,
                          "cost_class": attr[1], "cost": profile[attr[1]], "rows": 24, "pixels": 576,
                          "water_mask": False, "lut_indices": [index for index, _ in ranges]}
            break
    if range_plan is None:
        raise ValueError("没有完整 24x24、干燥／无 actor 遮挡且可达的邻格范围 ROI")

    # 全部候选都避开角色的完整 sprite 矩形、四个菜单矩形和底部信息面板。
    sprite_rects = [(position(unit[:2])[0], position(unit[:2])[1] - 6,
                     position(unit[:2])[0] + 24, position(unit[:2])[1] + 24)
                    for unit in units if not unit[5] & 1]
    menu_rects = [(leader_at[0] + dx, leader_at[1] + dy,
                   leader_at[0] + dx + 24, leader_at[1] + dy + 20) for dx, dy in MENU_OFFSETS]
    candidates = []
    for ry in range(5):
        for rx in range(13):
            world = camera[0] + rx, camera[1] + ry
            at = position(world)
            rect = at[0], at[1], at[0] + 24, at[1] + 24
            if any(overlaps(rect, other) for other in sprite_rects + menu_rects):
                continue
            candidate = dry_static(world)
            if candidate is None:
                continue
            tile_id, _, image = candidate
            signature = image.tobytes()
            compatible = set()
            for cy in range(height - 8 + 1):
                for cx in range(width - 13 + 1):
                    _, _, other_attr, other_image = tile_at((cx + rx, cy + ry))
                    # 不猜动态／遮挡地形某相位的结果；这些候选不能由本 ROI 排除。
                    if other_attr[0] or other_image.tobytes() == signature:
                        compatible.add((cx, cy))
            candidates.append((compatible, image, {"world": list(world), "screen": list(at),
                "tile_id": tile_id, "rows": 24, "pixels": 576, "water_mask": False,
                "source_pixel_sha256": hashlib.sha256(signature).hexdigest()}))
    remaining = {(cx, cy) for cy in range(height - 8 + 1) for cx in range(width - 13 + 1)}
    camera_tiles, camera_plan = [], []
    while len(remaining) > 1 and candidates:
        best = min(candidates, key=lambda candidate: len(remaining & candidate[0]))
        if len(remaining & best[0]) == len(remaining):
            break
        candidates.remove(best)
        remaining &= best[0]
        camera_tiles.append((best[1], tuple(best[2]["screen"])))
        camera_plan.append(dict(best[2], compatible_camera_count=len(remaining)))
    if remaining != {tuple(camera)} or not camera_tiles:
        raise ValueError("静态完整地形 ROI 无法独立唯一确认目标 camera")
    icons = (original / "FDICON.B24").read_bytes()
    sprites = [sheet_frame(icons, phase) for phase in range(12)]
    for sprite in sprites:
        sprite.putpalette(assets["palette"])
    assets["upper_commands"] = {index: [upper_command_composite(assets["commands"][index], sprite) for sprite in sprites]
                                 for index in range(3)}
    plan = {"leader_world": list(target), "leader_screen": leader_at,
            "source_camera_focus": list(source[META + 3:META + 9]),
            "expected_camera": camera, "reference": reference,
            "range_probe": range_plan, "camera_rois": camera_plan,
            "camera_uniquely_identified": True, "movement_profile_oracle": profile_receipt}
    return assets, ranges, camera_tiles, plan


class FocusOracle:
    def __init__(self, source, assets, ranges, camera_tiles, plan):
        self.assets, self.ranges, self.camera_tiles, self.plan = assets, ranges, camera_tiles, plan
        self.player = PlayerOracle(source, assets, ranges, plan)
        self.position = tuple(plan["leader_screen"])

    def leader(self, frame):
        found = browse_anywhere(frame, self.assets, leader=True)
        return found if found and tuple(found["focus_screen"]) == self.position else None

    def browse(self, frame):
        leader = self.leader(frame)
        if not leader or self.command(frame):
            return None
        base = exact_tile(frame, self.assets["probe_base_tile"], tuple(self.plan["range_probe"]["screen"]))
        tiles = [exact_tile(frame, image, at) for image, at in self.camera_tiles]
        if not base or not all(tiles):
            return None
        return {"kind": "edge_actor_browse", "leader": leader,
                "expected_screen": self.plan["leader_screen"], "actual_screen": leader["focus_screen"],
                "expected_camera": self.plan["expected_camera"], "terrain_proven_camera": self.plan["expected_camera"],
                "independent_camera_rois": tiles, "untranslated_range_tile": base}

    def selection(self, frame):
        if not self.leader(frame) or self.command(frame):
            return None
        for index, image in self.ranges:
            match = exact_tile(frame, image, tuple(self.plan["range_probe"]["screen"]))
            if match:
                return {"kind": "edge_selection", "range_lut": index, "range_rows": 24,
                        "range_world": self.plan["range_probe"]["world"], "focus_screen": list(self.position), **match}
        return None

    def command(self, frame):
        return self.player.command(frame, self.position)


def exercise(capture, source, assets, ranges, camera_tiles, plan, titles, save, data, stamp, report):
    def guard():
        require_source_unchanged(save, data, stamp)

    def checkpoint(name, predicate, **kwargs):
        _, detail = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        guard()
        return detail

    wait_title(capture, titles[3][0])
    guard()
    capture.key("UP")
    capture.wait(titles[3][2])
    guard()
    capture.key("ENTER")
    initial = tuple(4 + value * 24 for value in source[META + 7:META + 9])
    checkpoint("source-browse", RoundOracle(assets, initial).initial_browse, timeout=30)
    oracle = FocusOracle(source, assets, ranges, camera_tiles, plan)
    capture.key("Z")
    report["focus_evidence"] = checkpoint("one-z-edge-focus", oracle.browse)
    capture.key("ENTER")
    checkpoint("edge-unit-selection", oracle.selection)
    capture.key("ENTER")
    report["command_evidence"] = checkpoint("edge-zero-step-command", oracle.command)
    capture.key("ESC")
    checkpoint("command-cancel-selection", oracle.selection)
    capture.key("ESC")
    checkpoint("selection-cancel-browse", oracle.browse)
    guard()


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents or output == fixture or output in fixture.parents:
        raise ValueError("产物目录不能覆盖 original_game 或源夹具")
    output.mkdir(parents=True, exist_ok=False)
    before, encrypted, source_stamp = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "capture": "Windows.Graphics.Capture HWND only",
              "scope": "synthetic START actor0 edge-focus camera / full range tile / four full command icons with actor occlusion / cancel lifecycle",
              "natural_opening_covered": False, "original_first": True,
              "excluded": ["自然开场镜头", "菜单能力门／默认选择跨端一致性", "瞬时动画完整对照", "完整 runtime／RNG 测量", "全地图像素对照"],
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(encrypted).hexdigest(), "mtime_ns": source_stamp},
              "original_before": before, "cases": [], "fixtures": [], "inputs": [], "checkpoints": [], "processes": []}
    try:
        metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
        source, _ = validate_initial(fixture, metadata[0])
        derived = []
        for name, target in TARGETS:
            encoded, plain, manifest = derive_fixture(source, encrypted, target)
            fixture_path = output / f"{name}.SAV"
            fixture_path.write_bytes(encoded)
            if decode_save(fixture_path) != plain:
                raise AssertionError("坐标夹具 XOR/checksum roundtrip 与 manifest 不符")
            assets, ranges, camera_tiles, plan = prepare(original, plain, target)
            report["fixtures"].append(dict(manifest, name=name, path=str(fixture_path), plan=plan))
            derived.append((name, encoded, plain, assets, ranges, camera_tiles, plan))
        titles = title_templates(original / "FDOTHER.DAT")
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        report["runner_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        for kind in ("original", "sdl"):
            for name, data, plain, assets, ranges, camera_tiles, plan in derived:
                work = output / f"{kind}-{name}"
                work.mkdir()
                env = os.environ.copy()
                for key in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
                    env.pop(key, None)
                if kind == "original":
                    game_copy = work / "game"
                    shutil.copytree(original, game_copy)
                    save = game_copy / "FD2.SAV"
                    save.write_bytes(data)
                    config = work / "dosbox.conf"
                    config.write_text(
                        "[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                        "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n"
                        "[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n"
                        f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                    command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
                else:
                    save = work / "FD2.SAV"
                    save.write_bytes(data)
                    env["FD2SDL_SAVE_PATH"] = str(save)
                    command, cwd = [str(args.game.resolve())], root
                stamp = save.stat().st_mtime_ns
                case = {"name": f"{kind}-{name}", "kind": kind, "status": "failed", "checkpoints": [],
                        "fixture": name, "source_mtime_ns_before_start": stamp}
                report["cases"].append(case)
                try:
                    with running(command, cwd, env, work / "game.log", args.driver, report["processes"]) as process:
                        case["pid"] = process.pid
                        capture = Capture(args.driver, process, work, report["inputs"])
                        exercise(capture, plain, assets, ranges, camera_tiles, plan, titles, save, data, stamp, case)
                        require_source_unchanged(save, data, stamp)
                    require_source_unchanged(save, data, stamp)
                    case.update(status="passed", source_bytes_and_mtime_unchanged=True,
                                source_mtime_ns_after_cleanup=save.stat().st_mtime_ns,
                                full_runtime_fields_measured=False)
                except Exception as error:
                    case["error"] = str(error)
                    raise
                finally:
                    report["checkpoints"].extend({"case": case["name"], **item} for item in case["checkpoints"])
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before
        report["fixture_unchanged"] = fixture.read_bytes() == encrypted and fixture.stat().st_mtime_ns == source_stamp
        report["ok"] &= report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path, help="只读来源，默认 original_game/START.SAV")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("边缘镜头配对要求 Windows WGC；未启动 GUI")
    try:
        result = run(args)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result["ok"] else 1
    except Exception as error:
        print(f"边缘镜头配对无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
