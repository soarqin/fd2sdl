#!/usr/bin/env python3
"""F1/PageUp全场缩放图WGC配对：原版先行，再验证SDL。

依据 sub_2000A @0x2000a、shape_sheet_decode_cache @0x1399c、
field_view_render_scaled @0x1f558、sub_1F6EF @0x1f6ef。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import time

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from audit_fdicon_opaque_zero import audit_fdicon
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image
from run_original_comparison import Capture, archive_entry, compare, decode_image, driver_call, hashes, running, title_templates
from run_original_battle_menu import wait_state, wait_title
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, browse_anywhere, empty_focus_navigation, round_assets, validate_initial
from run_original_load import require_source_unchanged
from run_original_player import cursor_assets, navigation, screen, visual_focus


def tile_payload(sheet, index):
    width, height, count = struct.unpack_from("<HHH", sheet)
    if (width, height) != (24, 24) or not 0 <= index < count:
        raise ValueError("FDSHAP frame尺寸/索引不符")
    start = struct.unpack_from("<I", sheet, 6 + index * 4)[0]
    end = struct.unpack_from("<I", sheet, 6 + (index + 1) * 4)[0] if index + 1 < count else len(sheet)
    if not 6 + count * 4 <= start < end <= len(sheet):
        raise ValueError("FDSHAP frame边界无效")
    return sheet[start:end]


def scaled_base(width, height, cells, tiles, palette):
    """独立复现tile跨度0xc00/像素>>7；完整frame预清0源于1399c/1f558。"""
    if width != 24 or height != 24 or len(cells) != width * height:
        raise ValueError("本runner仅核实真实stage0的24x24地图")
    step = 768
    start_x, start_y = width * 1536 - 156 * step, height * 1536 - 96 * step
    pixels = bytearray(320 * 200)
    for y in range(192):
        ty, iy = divmod(start_y + y * step, 3072)
        if not 0 <= ty < height:
            continue
        for x in range(312):
            tx, ix = divmod(start_x + x * step, 3072)
            if 0 <= tx < width:
                index = cells[ty * width + tx] & 0x3ff
                pixels[(y + 4) * 320 + x + 4] = tiles[index][(iy >> 7) * 24 + (ix >> 7)]
    image = Image.frombytes("P", (320, 200), bytes(pixels))
    image.putpalette(palette)
    return image


def marker_frames(base, source, focus, bases, width=24, height=24):
    frames, actors = [], []
    for i in range(source[META + 1]):
        record = source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE]
        if record[5] & 1 or record[7] == 121 or record[0x1f] == 10:
            continue
        if record[6] > 2 or record[0] >= width or record[1] >= height:
            raise ValueError("可见actor阵营或地图坐标越界")
        actors.append((i, record[0], record[1], record[6]))
    if not 0 <= focus[0] < width or not 0 <= focus[1] < height:
        raise ValueError("focus坐标越界")
    positions = []
    for phase in range(8):
        image = base.copy()
        for index, x, y, side in actors:
            sx, sy = 4 * x + 160 - 2 * width, 4 * y + 100 - 2 * height
            image.paste(bases[side] + phase, (sx, sy, sx + 3, sy + 3))
            if phase == 0:
                positions.append({"actor": index, "world": [x, y], "side": side, "screen": [sx, sy]})
        sx, sy = 4 * focus[0] + 160 - 2 * width, 4 * focus[1] + 100 - 2 * height
        image.paste(24 + phase, (sx, sy, sx + 3, sy + 3))
        frames.append(image)
    return frames, {"actors": positions, "focus_world": list(focus), "focus_screen": [sx, sy],
                    "focus_drawn_last": True, "marker_size": [3, 3]}


def prepare(original, fixture):
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata = archive_entry(field, 1)
    source, focus = validate_initial(fixture, metadata[0])
    data = archive_entry(field, 0)
    width, height = struct.unpack_from("<HH", data)
    if (width, height) != (24, 24) or len(data) != 4 + width * height * 4:
        raise ValueError("stage0 map布局不符")
    cells = struct.unpack_from("<" + "I" * (width * height), data, 4)
    sheet = archive_entry((original / "FDSHAP.DAT").read_bytes(), source[0] * 2)
    sheet_audit = audit_fdicon(sheet, expected_frames=288)
    tiles, used_skips, used_zero_writes = {}, {}, 0
    for index in sorted({value & 0x3ff for value in cells}):
        payload = tile_payload(sheet, index)
        audit = audit_fdicon(struct.pack("<HHHI", 24, 24, 1, 10) + payload, expected_frames=1)
        skips = audit["counts"]["explicit_skip_pixels"] + audit["counts"]["alternating_skip_pixels"]
        if skips:
            used_skips[index] = skips
        used_zero_writes += audit["opaque_zero_pixels"]
        # 原1399c指令VA13a04明确先memset(cache+6,0,576*count)，随后RLE。
        # decoder预清0与该证据相符，skip不是任意填补的未知cache字节。
        tiles[index] = bytes(decode_image(struct.pack("<HH", 24, 24) + payload).getdata())
    assets = round_assets(original)
    exe = (original / "FD2.EXE").read_bytes()
    expected_exe = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
    if hashlib.sha256(exe).hexdigest() != expected_exe:
        raise ValueError("FD2.EXE不是已验证的LE布局；未解析未知版本或分配其声明大小")
    le = parse_le(exe)
    raw = build_relbase_image(exe, le, parse_objects(exe, le))
    bases = struct.unpack_from("<III", raw, 0x5208a)
    if bases != (32, 80, 72):
        raise ValueError("原版阵营颜色表不再是已确认的32/80/72")
    # sub_4E31C在普通browse等待中更新DAC224..239；F1不调用它，保持打开前
    # 的某一个合法cycle。每页只用完整帧校准一次，然后冻结该页palette。
    cycles = []
    for phase in range(16):
        rgb6 = raw[0x60003 + phase * 3:0x60003 + (phase + 16) * 3]
        if len(rgb6) != 48 or max(rgb6) > 63:
            raise ValueError("原版水色DAC周期表不符合16项RGB6契约")
        palette = assets["palette"].copy()
        palette[224 * 3:240 * 3] = [value << 2 for value in rgb6]
        cycles.append({"phase": phase, "palette": palette, "rgb6": list(rgb6)})
    assets["f1_palette_cycles"] = cycles
    base = scaled_base(width, height, cells, tiles, assets["palette"])
    steps = empty_focus_navigation(source)
    safe_screen = steps[-1][1] if steps else focus
    safe_world = (source[META + 3] + (safe_screen[0] - 4) // 24,
                  source[META + 4] + (safe_screen[1] - 4) // 24)
    leader_world = tuple(source[UNITS:UNITS + 2])
    moved = bytearray(source)
    moved[META + 5:META + 7] = bytes(safe_world)
    actor_steps = navigation(moved, leader_world)
    _, composites = cursor_assets(original / "FDICON.B24", source, assets)
    safe_frames, safe_markers = marker_frames(base, source, safe_world, bases)
    actor_frames, actor_markers = marker_frames(base, source, leader_world, bases)
    audit = {"shape": source[0], "map": [width, height], "shape_frames": sheet_audit["validated_frames"],
             "map_used_tiles": len(tiles), "map_used_tiles_with_rle_skip": used_skips,
             "map_used_opaque_zero_writes": used_zero_writes, "cache_zero_preclear_basis": "1399c: instruction VA 13a04",
             "safe_navigation": steps, "safe_world": list(safe_world), "safe_screen": list(safe_screen),
             "leader_world": list(leader_world), "actor_navigation": actor_steps, "marker_color_bases": list(bases),
             "water_dac_basis": "sub_4E31C @0x4e31c; object3 0x60003 + phase*3; DAC224..239",
             "water_dac_cycles_rgb6": [cycle["rgb6"] for cycle in cycles],
             "safe_markers": safe_markers, "actor_focus_markers": actor_markers}
    return source, focus, assets, composites, base, safe_frames, actor_frames, audit


class FullFrameOracle:
    def __init__(self, frames, cycles=None):
        if len(frames) != 8 or any(frame.size != (320, 200) for frame in frames):
            raise ValueError("必须提供8组完整320x200颜色phase")
        if cycles and any(frame.mode != "P" for frame in frames):
            raise ValueError("DAC周期oracle需要原始索引帧，不能逆推RGB颜色索引")
        self.indexed = frames
        self.frames = [frame.convert("RGB") for frame in frames]
        self.cycles = cycles
        self.water_candidates = None

    def _full_matches(self, actual, candidates):
        matches = []
        for cycle in candidates:
            expected_frames = self.frames
            if cycle is not None:
                expected_frames = []
                for indexed in self.indexed:
                    image = indexed.copy()
                    image.putpalette(cycle["palette"])
                    expected_frames.append(image.convert("RGB"))
            for phase, expected in enumerate(expected_frames):
                result = compare(expected, actual)  # 完整64000像素，包含黑色与index0。
                if result["ok"]:
                    matches.append((phase, cycle, expected_frames, result))
        return matches

    def match(self, actual, excluded=()):
        candidates = self.cycles if self.water_candidates is None and self.cycles else (None,)
        matches = self._full_matches(actual, candidates)
        if not matches and self.water_candidates is not None and self.cycles:
            legal = self._full_matches(actual, self.cycles)
            cycles = {item[1]["phase"] for item in legal}
            if cycles and cycles.isdisjoint(self.water_candidates):
                # 首帧已证明稳定页；另一完整合法cycle是漂移，不能让wait_state
                # 将其当成未到目标phase并等待颜色恢复。原首帧候选有交集时不猜。
                raise AssertionError(f"F1页内水DAC漂移：首帧{self.water_candidates}，当前{sorted(cycles)}")
        phases = {item[0] for item in matches}
        if len(phases) == 1 and next(iter(phases)) not in excluded:
            phase, _, frames, result = matches[0]
            if self.water_candidates is None and self.cycles:
                self.water_candidates = [item[1]["phase"] for item in matches]
                self.frames = frames  # 只在首个完整匹配后锁定，不逐帧追随实际色。
            return {"kind": "full_scaled_map_and_markers", "pulse_phase": phase,
                    "checked_pixels": 64000, "black_and_zero_included": True,
                    "water_palette_cycle_candidates": self.water_candidates, **result}
        return None


def exercise(capture, source, focus, assets, composites, safe_frames, actor_frames, plan, titles,
             save, data, stamp, report, original_observations=None):
    observations = {}
    def checkpoint(name, predicate, **kwargs):
        image, detail = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        require_source_unchanged(save, data, stamp)
        return image, detail
    def open_page(name, key, frames):
        capture.key(key)
        oracle = FullFrameOracle(frames, assets["f1_palette_cycles"])
        image, first = checkpoint(name + "-pulse-first", oracle.match)
        for phase, expected in enumerate(oracle.frames):
            expected.save(capture.artifacts / f"{name}-oracle-phase{phase}.png")
        captured = {}
        if original_observations is None:
            captured[first["pulse_phase"]] = image
            image, second = checkpoint(name + "-pulse-distinct", lambda image: oracle.match(image, (first["pulse_phase"],)))
            captured[second["pulse_phase"]] = image
        else:
            # 首帧只校准该端合法DAC；随后等到实际原版的两个marker phase。
            # 不补键、不调整时钟，不要求两端进入前水色周期同步。
            for phase in original_observations[name]["frames"]:
                def same_phase(frame):
                    detail = oracle.match(frame)
                    return detail if detail and detail["pulse_phase"] == phase else None
                image, _ = checkpoint(f"{name}-native-phase{phase}", same_phase)
                captured[phase] = image
        summary = {"name": name, "phases": list(captured),
                   "water_palette_cycle_candidates": oracle.water_candidates,
                   "resource_checked_pixels_per_frame": 64000,
                   "water_black_and_zero_included": True}
        report["observed_pulses"].append(summary)
        observations[name] = {**summary, "frames": captured}
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    checkpoint("initial-browse", RoundOracle(assets, focus).initial_browse, timeout=30)
    for index, (key, position) in enumerate(plan["safe_navigation"], 1):
        capture.key(key)
        checkpoint(f"safe-navigation-{index}", RoundOracle(assets, tuple(position)).closed_browse)
    safe = RoundOracle(assets, tuple(plan["safe_screen"]))
    checkpoint("safe-empty-browse", safe.closed_browse)
    for label, opening, closing in (("f1-escape", "F1", "ESC"),
                                    ("pageup-right", "PAGEUP", "RIGHT"),
                                    ("f1-letter", "F1", "A")):
        open_page(label, opening, safe_frames)
        capture.key(closing)
        checkpoint(label + "-same-focus-browse", safe.closed_browse)
    for index, (key, world) in enumerate(plan["actor_navigation"], 1):
        capture.key(key)
        expected = screen(source, world)
        def acknowledged(image):
            detail = visual_focus(image, assets, composites)
            return detail if detail and tuple(detail["focus_screen"]) == expected else None
        checkpoint(f"actor0-navigation-{index}", acknowledged)
    expected = screen(source, tuple(plan["leader_world"]))
    def leader(image):
        detail = browse_anywhere(image, assets, leader=True)
        return detail if detail and tuple(detail["focus_screen"]) == expected else None
    checkpoint("actor0-resource-proven", leader)
    open_page("actor0-focus-overlays-side-marker", "F1", actor_frames)
    require_source_unchanged(save, data, stamp)
    event = {"kind": "owned_host_close", "pid": capture.process.pid, "time": time.monotonic()}
    report["inputs"].append(event)
    with (capture.artifacts / "operation-trace.jsonl").open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(event) + "\n")
    driver_call(capture.driver, "close", capture.process)
    code = capture.process.wait(timeout=8)
    report["host_close"] = {**event, "exit_code": code, "ended_before_finally_cleanup": capture.process.poll() is not None}
    if code != 0:
        raise RuntimeError(f"页内宿主关闭退出码异常：{code}")
    require_source_unchanged(save, data, stamp)
    return observations


def compare_observations(original, sdl):
    if set(original) != set(sdl) or len(original) != 4:
        raise AssertionError("双端必须完成相同的四组F1页面")
    result = []
    for name, native in original.items():
        actual = sdl[name]
        if len(native["frames"]) < 2 or set(native["frames"]) != set(actual["frames"]):
            raise AssertionError("每组双端必须取得同样的至少两个marker phase")
        native_cycles = native["water_palette_cycle_candidates"]
        sdl_cycles = actual["water_palette_cycle_candidates"]
        if not native_cycles or not sdl_cycles:
            raise AssertionError("每端每页必须已锁定合法水DAC周期")
        differences = []
        for phase, image in native["frames"].items():
            other = actual["frames"][phase]
            detail = compare(image, other)
            exact = sum(left != right for left, right in zip(image.convert("RGB").getdata(), other.convert("RGB").getdata()))
            differences.append({"pulse_phase": phase, "checked_pixels": 64000,
                                "exact_rgb_different_pixels": exact, **detail})
        cycle_disjoint = not set(native_cycles).intersection(sdl_cycles)
        result.append({"name": name, "original_water_cycle_candidates": native_cycles,
                       "sdl_water_cycle_candidates": sdl_cycles,
                       "independent_preopen_cycles_differ": cycle_disjoint,
                       "full_raw_rgb_equivalence": all(item["exact_rgb_different_pixels"] == 0 for item in differences),
                       "within_vga_tolerance": all(item["ok"] for item in differences),
                       "raw_rgb_equality_is_acceptance_gate": False,
                       "color_normalization_applied": False,
                       "resource_full_frames_passed_on_each_side": True,
                       "raw_frame_differences": differences})
    return result


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = root / "original_game"
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents or output == fixture or output in fixture.parents:
        raise ValueError("产物目录不能覆盖original_game或源夹具")
    output.mkdir(parents=True, exist_ok=False)
    before, data, fixture_stamp = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "capture": "Windows.Graphics.Capture HWND only",
              "scope": "stage0 raw terrain sampling and markers; full64000 RGB legal frozen water cycles per side; same-focus F1/PageUp any-key close; host close",
              "excluded": ["精确开合耗时", "首次undefined tick", "held/repeat实机时序", "音频", "全runtime字段/RNG", "进入前水DAC周期同步"],
              "raw_rgb_equality_is_acceptance_gate": False, "color_normalization_applied": False,
              "job_owner": "run_graphics_tests driver run-job serial owned tree receipt",
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(data).hexdigest(), "mtime_ns": fixture_stamp},
              "original_before": before, "inputs": [], "checkpoints": [], "processes": [], "cases": [], "paired_frames": []}
    try:
        source, focus, assets, composites, base, safe_frames, actor_frames, plan = prepare(original, fixture)
        report["plan"] = plan
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        report["runner_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        report["oracle_dependency_sha256"] = {name: hashlib.sha256((ROOT / "tools" / name).read_bytes()).hexdigest()
            for name in ("audit_fdicon_opaque_zero.py", "rebuild_fd2_analysis.py", "run_original_comparison.py",
                         "run_original_battle_menu.py", "run_original_end_turn.py", "run_original_player.py", "run_original_load.py")}
        base.save(output / "oracle-source-palette-base.png")
        for label, frames in (("safe", safe_frames), ("actor0-focus", actor_frames)):
            for phase, image in enumerate(frames):
                image.save(output / f"oracle-source-palette-{label}-phase{phase}.png")
        titles = title_templates(original / "FDOTHER.DAT")
        native_observations = None
        for kind in ("original", "sdl"):
            work = output / kind
            work.mkdir()
            environment = os.environ.copy()
            for key in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER", "FD2SDL_SAVE_PATH"):
                environment.pop(key, None)
            if kind == "original":
                game_copy = work / "game"
                shutil.copytree(original, game_copy)
                save = game_copy / "FD2.SAV"
                shutil.copy2(fixture, save)
                config = work / "dosbox.conf"
                config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                                  "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n"
                                  "[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n"
                                  f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"
                shutil.copy2(fixture, save)
                environment["FD2SDL_SAVE_PATH"] = str(save)
                command, cwd = [str(args.game.resolve())], root
            stamp = save.stat().st_mtime_ns
            case = {"name": kind, "status": "failed", "inputs": report["inputs"],
                    "checkpoints": [], "observed_pulses": [], "copy_mtime_before_start": stamp}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, work / "game.log", args.driver, report["processes"]) as process:
                    case["pid"] = process.pid
                    capture = Capture(args.driver, process, work, report["inputs"])
                    observations = exercise(capture, source, focus, assets, composites, safe_frames, actor_frames, plan,
                                            titles, save, data, stamp, case, native_observations)
                    require_source_unchanged(save, data, stamp)
                    if not case.get("host_close", {}).get("ended_before_finally_cleanup") or case["host_close"]["exit_code"] != 0:
                        raise AssertionError("页内host close必须在finally清理前自然退出0")
                require_source_unchanged(save, data, stamp)
                if kind == "original":
                    native_observations = observations
                else:
                    report["paired_frames"] = compare_observations(native_observations, observations)
                case.update(status="passed", disk_and_mtime_unchanged=True,
                            copy_mtime_after_cleanup=save.stat().st_mtime_ns,
                            full_runtime_fields_measured=False)
            except Exception as error:
                case["error"] = str(error)
                raise
            finally:
                report["checkpoints"].extend({"case": kind, **item} for item in case["checkpoints"])
                del case["inputs"]
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before
        report["fixture_unchanged"] = fixture.read_bytes() == data and fixture.stat().st_mtime_ns == fixture_stamp
        report["ok"] &= report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path, help="只读来源，默认original_game/START.SAV")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("F1配对要求Windows WGC；未启动GUI")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"F1配对无法启动：{error}。检查参数与取证文件后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
