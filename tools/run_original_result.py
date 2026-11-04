#!/usr/bin/env python3
"""受控START终局门：原版分支资源与SDL独立结果分派，不验整关实战或B5后续。

依据sub_205B4 @0x205b4、sub_22EF6 @0x22ef6、sub_22E5C @0x22e5c（原中性登记同名）。
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
import subprocess
import sys

from PIL import Image

from run_graphics_tests import decode_save, terminal_save_fixture
from run_original_comparison import (Capture, TOLERANCE, archive_entry, compare, decode_image,
                                     hashes, running, title_templates)
from run_original_battle_menu import match_indexed, save_checkpoint, wait_state, wait_title
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets, validate_initial
from run_original_load import focus_cycle_assets, focused_actor, require_source_unchanged
from run_original_player import DIRECTIONS, exact_tile, screen, sheet_frame
from fd2_movement_oracle import movement_profile


def derive_terminal(source, encrypted_source, outcome):
    """按sub_205B4 @0x205b4（原登记同名）只改hidden和checksum，保留HP与未知字段。"""
    if len(encrypted_source) != len(source):
        raise ValueError("source encrypted/plain lengths differ")
    encrypted = terminal_save_fixture(source, outcome)
    expected = bytearray(source)
    targets = [i for i in range(source[META + 1]) if source[UNITS + i * STRIDE + 6] == 0]
    if outcome == "defeat":
        targets = [0]
    for raw in targets:
        expected[UNITS + raw * STRIDE + 5] |= 1
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    changes = [{"offset": i, "before": old, "after": new,
                "owner": "checksum" if i >= len(source) - 4 else "active flags bit0"}
               for i, (old, new) in enumerate(zip(source, expected)) if old != new]
    allowed = {UNITS + raw * STRIDE + 5 for raw in targets} | set(range(len(source)-4, len(source)))
    if not changes or any(change["offset"] not in allowed for change in changes):
        raise ValueError("terminal fixture changed an unconfirmed byte")
    actor = next((i for i in range(source[META + 1])
                  if expected[UNITS + i * STRIDE + 6] == 2 and
                  not expected[UNITS + i * STRIDE + 5] & 0x85), None)
    if actor is None or expected[UNITS + actor * STRIDE + 0x26] != 0:
        raise ValueError("terminal fixture has no supported controller actor")
    return encrypted, bytes(expected), {
        "outcome": outcome, "synthetic_terminal_state": True, "whole_battle_played": False,
        "source_encrypted_sha256": hashlib.sha256(encrypted_source).hexdigest(),
        "source_plain_sha256": hashlib.sha256(source).hexdigest(),
        "derived_encrypted_sha256": hashlib.sha256(encrypted).hexdigest(),
        "derived_plain_sha256": hashlib.sha256(expected).hexdigest(),
        "changed_plain_bytes": changes, "flags_target_raw_actors": targets,
        "changed_encrypted_bytes": [{"offset": i,"before":old,"after":new}
                                    for i,(old,new) in enumerate(zip(encrypted_source,encrypted)) if old != new],
        "controller_actor": actor, "hp_changed": False,
        "reason": "native205b4 tests hidden, not HP; controller selection cancel evaluates the stage gate"}


def result_templates(original, assets):
    """sub_22EF6 @0x22ef6／sub_22E5C @0x22e5c（原登记同名）的固定资源门。"""
    data = archive_entry((original / "FDTXT.DAT").read_bytes(), 1)
    start, end = struct.unpack_from("<HH", data, 18)
    if not 0 <= start < end <= len(data) or (end-start) & 1:
        raise ValueError("stage0 victory fragment9 bounds invalid")
    tokens = list(struct.unpack("<" + "h" * ((end-start)//2), data[start:end]))
    if tokens[:2] != [-18, 4]:
        raise ValueError("stage0 victory first speaker changed")
    lines = [[]]
    for token in tokens[2:]:
        if token in (-17, -18, -19, -20, -1, -3):
            break  # 第一说话人切换前会等待，不补键推进结算。
        if token == -2:
            lines.append([])
        elif token >= 0:
            lines[-1].append(token)
        else:
            raise ValueError("unconfirmed control in first victory page")
    lines = [line for line in lines if line]
    if len(lines) != 2:
        raise ValueError("stage0 victory first page is not the two confirmed lines")
    font = archive_entry((original / "FDOTHER.DAT").read_bytes(), 4)
    text = Image.new("P", (1 + max(map(len, lines)) * 16, 36), 0)
    text.putpalette(assets["palette"])
    for line, glyphs in enumerate(lines):
        for column, token in enumerate(glyphs):
            if (token + 1) * 32 > len(font):
                raise ValueError("victory glyph outside FDOTHER4")
            if token == 10:
                continue
            glyph = font[token*32:(token+1)*32]
            for y in range(16):
                bits = int.from_bytes(glyph[y*2:y*2+2], "big")
                for x in range(16):
                    if bits & (0x8000 >> x):
                        px, py = 1 + column*16 + x, line*19 + y
                        text.putpixel((px, py), 205)
                        text.putpixel((px-1, py+1), 76)
                        text.putpixel((px, py+1), 76)
    # text_dialog_render_tokens -18在693535，即VGA(95,119)；模板左扩1px含shadow。
    if 94 + text.width > 320:
        raise ValueError("victory text ROI outside viewport")
    entry = archive_entry((original / "FDOTHER.DAT").read_bytes(), 79)
    if len(entry) < 16 or struct.unpack_from("<HHI", entry) != (2, 2, 0):
        raise ValueError("defeat FDOTHER79 directory changed")
    offsets = struct.unpack_from("<II", entry, 8)
    canvas = Image.new("P", (320, 200), 0)
    canvas.putpalette(assets["palette"])
    defeat = []
    for index, offset in enumerate(offsets):
        finish = offsets[index+1] if index+1 < 2 else len(entry)
        if not 16 <= offset < offset+13 <= finish <= len(entry):
            raise ValueError("defeat image bounds invalid")
        x, y = struct.unpack_from("<HH", entry, offset)
        image = decode_image(entry[offset+9:finish])  # sub_2EB9F: embedded image at header+9。
        if x + image.width > 320 or y + image.height > 200:
            raise ValueError("defeat image ROI outside viewport")
        image.putpalette(assets["palette"])
        canvas.paste(image, (x, y))  # native透明参数-1，零色也复制。
        defeat.append(canvas.convert("RGB"))
    return {"victory_text": text, "victory_tokens": lines, "defeat": defeat}


def result_branch(frame, outcome, assets, templates):
    if outcome == "victory":
        text = match_indexed(frame, templates["victory_text"], (94, 119))
        border = match_indexed(frame, assets["dialog_top"], (5, 112))
        return {"kind": "native_victory_first_page", "fragment": "FDTXT[1]:9",
                "first_speaker": 4, "text_roi": [94, 119, *templates["victory_text"].size],
                "first_page_only": True, "settlement_continuation_verified": False,
                "text": text, "dialog_border": border} if text["ok"] and border["ok"] else None
    for index, expected in enumerate(templates["defeat"]):
        found = compare(expected, frame)
        if found["ok"]:
            return {"kind": "native_defeat_resource", "resource": "FDOTHER[79]",
                    "frame": index, "game_over_label_verified": index == 1,
                    "defeat_continuation_verified": False,"full_frame_pixels": 64000, **found}
    return None


def selection_probe(original, source, actor_index, assets):
    """只选无占用/无事件/无ZOC的一格静态地形作selection ACK，不移动actor。"""
    actor = source[UNITS + actor_index*STRIDE:UNITS + (actor_index+1)*STRIDE]
    world = tuple(actor[:2])
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata, map_data = archive_entry(field, 1), archive_entry(field, 0)
    width, height = struct.unpack_from("<HH", map_data)
    if (width, height) != (24, 24) or len(map_data) != 4 + width*height*4:
        raise ValueError("result selection requires stage0 map24x24")
    shape = (original / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shape, metadata[0]*2), archive_entry(shape, metadata[0]*2+1)
    profile, _ = movement_profile(original / "FD2.EXE", actor[0x20])
    units = [source[UNITS+i*STRIDE:UNITS+(i+1)*STRIDE] for i in range(source[META+1])]
    occupied = {tuple(u[:2]) for u in units if not u[5] & 1}
    enemies = [tuple(u[:2]) for u in units if u[6] == 0 and not u[5] & 1]
    for _, dx, dy in DIRECTIONS:
        cell = world[0]+dx, world[1]+dy
        rx, ry = cell[0]-source[META+3], cell[1]-source[META+4]
        if not (2 <= rx <= 10 and 2 <= ry <= 5) or cell in occupied or (cell[0],cell[1]+1) in occupied:
            continue
        if any(abs(x-world[0])+abs(y-world[1]) <= 1 or abs(x-cell[0])+abs(y-cell[1]) <= 1 for x,y in enemies):
            continue
        value = struct.unpack_from("<I", map_data, 4+(cell[1]*width+cell[0])*4)[0]
        tile = value & 0x3ff
        if tile*4+4 > len(attrs):
            raise ValueError("selection terrain attribute outside resource")
        flags, cost = attrs[tile*4:tile*4+2]
        if flags or (value >> 16) & 31 or cost >= len(profile) or not 0 < profile[cost] <= min(actor[0x3b],19):
            continue
        base = sheet_frame(tiles, tile)
        if 0 in base.getdata():
            continue
        base.putpalette(assets["palette"])
        sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
        if sheet[:4] != b"LMI1":
            raise ValueError("selection range LUT directory invalid")
        count = struct.unpack_from("<H", sheet, 4)[0]
        ranges = []
        for phase in range(count):
            start = struct.unpack_from("<I", sheet, 6+phase*4)[0]
            finish = struct.unpack_from("<I", sheet, 10+phase*4)[0] if phase+1<count else len(sheet)
            lut = sheet[start:finish]
            if len(lut) != 256:
                raise ValueError("selection LUT length invalid")
            image = Image.frombytes("P", (24,24), bytes(lut[p] for p in base.getdata()))
            image.putpalette(assets["palette"])
            if sum(max(abs(a-b) for a,b in zip(x,y)) > TOLERANCE
                   for x,y in zip(base.convert("RGB").getdata(),image.convert("RGB").getdata())) >= 24:
                ranges.append((phase,image))
        if ranges:
            return {"position": screen(source,cell), "world": cell, "ranges": ranges,
                    "base": base, "actor_position": screen(source,world)}
    raise ValueError("no resource-proven static selection ROI for controller actor")


def selection_state(frame, assets, actor, probe):
    focused = focused_actor(frame, assets, actor)
    if not focused or tuple(focused["focus_screen"]) != probe["actor_position"]:
        return None
    for phase, template in probe["ranges"]:
        found = exact_tile(frame, template, probe["position"])
        if found:
            return {"kind": "controller_unit_selected", "actor": focused, "range_lut": phase,
                    "range_roi": [*probe["position"],24,24], **found}
    return None


def exercise_original(capture, case, assets, templates, titles, focus, checkpoints):
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    outcome = case["outcome"]
    branch = lambda frame: result_branch(frame,outcome,assets,templates)
    initial = RoundOracle(assets,focus)
    def checkpoint(label,predicate):
        return wait_state(capture,outcome+"-original-"+label,
                          lambda frame: branch(frame) or predicate(frame),checkpoints,timeout=45)[1]
    def finish(trigger,detail):
        observation = {"trigger":trigger,"branch":detail}
        if outcome == "defeat":
            def game_over(frame):
                found = branch(frame)
                return found if found and found["frame"] == 1 else None
            # 22e5c在frame0淡入后delay9再绘frame1；只等自动推进，不发送键。
            observation["game_over"] = detail if detail["frame"] == 1 else wait_state(
                capture,"defeat-original-game-over",game_over,checkpoints,timeout=15)[1]
        return observation
    detail = checkpoint("continued",initial.initial_browse)
    if detail["kind"].startswith("native_"):
        return finish("Continue",detail)
    capture.key("Z")
    detail = checkpoint("focused",lambda frame: focused_actor(frame,assets,case["actor"]))
    if detail["kind"].startswith("native_"):
        return finish("Z",detail)
    capture.key("ENTER")
    detail = checkpoint("selected",lambda frame: selection_state(frame,assets,case["actor"],case["probe"]))
    if detail["kind"].startswith("native_"):
        return finish("Enter",detail)
    capture.key("ESC")  # 只在selection资源门通过后取消，触发controller后置stage门。
    detail = wait_state(capture,outcome+"-original-result",branch,checkpoints,timeout=45)[1]
    return finish("selection Esc",detail)


def exercise_sdl(capture, outcome, titles, log, checkpoints):
    wait_title(capture,titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    try:
        code = capture.process.wait(timeout=20)
    except subprocess.TimeoutExpired:
        frame = capture.frame()
        save_checkpoint(capture,outcome+"-sdl-not-exited",frame,{"kind":"natural_exit_timeout"},checkpoints)
        raise TimeoutError("SDL result did not naturally exit2 before cleanup")
    text = log.read_text(encoding="utf-8",errors="replace")
    required = f"field battle: {outcome}, stage=0"
    reason = "stage 0 settlement not implemented" if outcome == "victory" else "stage 0 defeat continuation not implemented"
    other = "defeat" if outcome == "victory" else "victory"
    if code != 2 or text.splitlines().count(required) != 1 or reason not in text or f"field battle: {other}" in text:
        raise AssertionError(f"SDL {outcome} result/exit2 checkpoint missing or wrong")
    if any(line.startswith(("field phase:","field session stopped:")) for line in text.splitlines()):
        raise AssertionError("SDL terminal load entered an automatic phase or session failure")
    return {"natural_exit": True,"observed_before_cleanup": True,"exit_code": code,
            "result_log":required,"continuation_boundary":reason,"post_continue_inputs":[]}


def run(args, *, probe_original=False):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root/"original_game").resolve()
    fixture = (args.save_fixture or original/"START.SAV").resolve()
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入 original_game/")
    output.mkdir(parents=True,exist_ok=False)
    before, source_bytes, source_mtime = hashes(original),fixture.read_bytes(),fixture.stat().st_mtime_ns
    report = {"ok":False,"probe_only":probe_original,
              "scope":"controlled START flags -> native stage0 branch resource; SDL independent result/exit2",
              "excluded":"whole-battle victory/defeat play, combat/RNG equality, B5 settlement/preparation/continuation and precise timing",
              "capture":"Windows.Graphics.Capture HWND only","platform":platform.platform(),
              "fixture":{"path":str(fixture),"sha256":hashlib.sha256(source_bytes).hexdigest(),"mtime_ns":source_mtime},
              "original_before":before,"inputs":[],"checkpoints":[],"processes":[],"cases":[],"derived_fixtures":[]}
    try:
        report["binaries"] = {name:{"path":str(path.resolve()),
                                    "sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name,path in (("game",args.game),("driver",args.driver),("dosbox",args.dosbox))}
        driver = args.driver.resolve()
        source,focus = validate_initial(fixture,archive_entry((original/"FDFIELD.DAT").read_bytes(),1)[0])
        assets,titles = round_assets(original),title_templates(original/"FDOTHER.DAT")
        templates = result_templates(original,assets)
        actors = focus_cycle_assets(original,source,assets)["actors"]
        derived = []
        for outcome in ("victory","defeat"):
            encrypted,plain,manifest = derive_terminal(source,source_bytes,outcome)
            actor = actors[manifest["controller_actor"]]
            probe = selection_probe(original,plain,actor["raw_actor"],assets)
            path = output/(outcome+"-derived.SAV")
            path.write_bytes(encrypted)
            if decode_save(path) != plain:
                raise AssertionError("derived SAV checksum/XOR roundtrip differs from exact manifest")
            report["derived_fixtures"].append(manifest)
            derived.append({"outcome":outcome,"bytes":encrypted,"plain":plain,"actor":actor,"probe":probe})
        environment = os.environ.copy()
        for key in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER"):
            environment.pop(key,None)
        for kind in (("original",) if probe_original else ("original","sdl")):
            for case in derived:
                outcome = case["outcome"]
                work = output/(kind+"-"+outcome)
                work.mkdir()
                if kind == "original":
                    game_copy = work/"game"
                    shutil.copytree(original,game_copy)
                    save = game_copy/"FD2.SAV"
                    config = work/"dosbox.conf"
                    config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                        "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
                        f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                    command,cwd = [str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
                else:
                    save = work/"FD2.SAV"
                    command,cwd = [str(args.game.resolve())],root
                save.write_bytes(case["bytes"])
                expected_mtime = save.stat().st_mtime_ns
                env = environment.copy()
                env["FD2SDL_SAVE_PATH"] = str(save)
                record = {"kind":kind,"outcome":outcome,"status":"failed"}
                report["cases"].append(record)
                log = work/"game.log"
                with running(command,cwd,env,log,driver,report["processes"],expected_exit=0 if kind=="original" else 2) as process:
                    record["pid"] = process.pid
                    capture = Capture(driver,process,work,report["inputs"])
                    if kind == "original":
                        record["observation"] = exercise_original(capture,case,assets,templates,titles,focus,report["checkpoints"])
                        record["checkpoint_observed_before_host_cleanup"] = True
                    else:
                        record["observation"] = exercise_sdl(capture,outcome,titles,log,report["checkpoints"])
                    require_source_unchanged(save,case["bytes"],expected_mtime)
                require_source_unchanged(save,case["bytes"],expected_mtime)
                record.update(status="passed",save_bytes_and_mtime_unchanged=True)
        report["probe_cases_passed"] = True
        report["ok"] = not probe_original  # ignored原版校准入口不能冒充配对验收。
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = report["original_after"] == before
        report["fixture_unchanged"] = fixture.read_bytes() == source_bytes and fixture.stat().st_mtime_ns == source_mtime
        report["ok"] = report["ok"] and report["original_unchanged"] and report["fixture_unchanged"]
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root","game","driver","dosbox","artifacts"):
        parser.add_argument("--"+option,type=Path,required=True)
    parser.add_argument("--save-fixture",type=Path,help="只读真实START来源，只复制并按manifest派生")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("原版结果门对照要求 Windows WGC；未启动 GUI")
    try:
        report = run(args)
        print(json.dumps(report,ensure_ascii=False,indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"原版结果门对照无法启动：{error}。检查参数或保留现场后重跑。",file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
