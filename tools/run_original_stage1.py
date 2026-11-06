#!/usr/bin/env python3
"""B5第二关新增合同：真实Continue、总览、turn3/script13及村民私有败北。

依据field_stage1_turn3_execute @0x34778（原sub_34778）、
field_stage1_battle_result_check @0x206c5（原sub_206C5）、
sub_1B41D @0x1b41d 的总览绘制（保留原中性登记）。
原版先正常清理再SDL；仅WGC/真实键，原版-only永不顶层ok。
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

from fd2_stage1_oracle import SCENARIOS, derive_fixture, stage_tables, turn3_pages, check_measurement
from fd2_item_list_oracle import number
from fd2_opening_oracle import page_match
from fd2_transition_oracle import entry_browse
from fd2_movement_oracle import movement_profile
from run_original_comparison import Capture, archive_entry, hashes, running, title_templates
from run_original_battle_menu import wait_state, wait_title, match_indexed
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle, round_assets
from run_original_focus import native_focus_reference
from run_original_load import require_source_unchanged, wait_measured_save, focused_actor
from run_original_magic_list import actor_focus_resources
from run_original_player import screen, sheet_frame, DIRECTIONS, exact_tile
from run_original_result import result_templates, exercise_original, exercise_sdl_defeat_continuation
from run_original_overview import overview_templates, match_overview, compare_overview
from run_original_item_list import paired_checkpoint_records, rgb_difference
from run_original_reward import endpoint_guard
from run_graphics_tests import decode_save


def selection_probe(original, source, assets, dimensions):
    """第二关仅一格、无占用／ZOC的原移动范围资源门；不移动单位。"""
    actor = source[UNITS:UNITS + STRIDE]
    world = tuple(actor[:2])
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata, data = archive_entry(field, 4), archive_entry(field, 3)
    shape = (original / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shape, metadata[0] * 2), archive_entry(shape, metadata[0] * 2 + 1)
    profile, _ = movement_profile(original / "FD2.EXE", actor[0x20])
    units = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(21)]
    occupied = {tuple(u[:2]) for u in units if not u[5] & 1}
    enemies = [tuple(u[:2]) for u in units if u[6] == 0 and not u[5] & 1]
    sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
    count = struct.unpack_from("<H", sheet, 4)[0]
    for _, dx, dy in DIRECTIONS:
        cell = world[0] + dx, world[1] + dy
        at = screen(source, cell)
        if (cell in occupied or (cell[0], cell[1] + 1) in occupied or
                not 4 <= at[0] <= 292 or not 4 <= at[1] <= 148 or
                not 0 <= cell[0] < dimensions[0] or not 0 <= cell[1] < dimensions[1] or
                any(abs(x-world[0])+abs(y-world[1]) <= 1 or abs(x-cell[0])+abs(y-cell[1]) <= 1 for x,y in enemies)):
            continue
        value = struct.unpack_from("<I", data, 4 + 4 * (cell[1] * dimensions[0] + cell[0]))[0]
        tile = value & 0x3ff
        flags, cost = attrs[4 * tile:4 * tile + 2]
        if flags or value >> 10 or cost >= len(profile) or not 0 < profile[cost] <= actor[0x3b]:
            continue
        base = sheet_frame(tiles, tile); pixels = bytes(base.getdata())
        if 0 in pixels or any(224 <= p < 240 for p in pixels): continue
        base.putpalette(assets["palette"])
        ranges = []
        for phase in range(count):
            start = struct.unpack_from("<I", sheet, 6 + phase * 4)[0]
            end = struct.unpack_from("<I", sheet, 10 + phase * 4)[0] if phase + 1 < count else len(sheet)
            lut = sheet[start:end]
            if len(lut) != 256: raise ValueError("stage1范围LUT不是256B")
            image = Image.frombytes("P", (24,24), bytes(lut[p] for p in pixels)); image.putpalette(assets["palette"])
            if exact_tile(base.convert("RGB"), image, (0,0)) is None: ranges.append((phase,image))
        if ranges: return {"position": at, "world": cell, "ranges": ranges, "base": base, "actor_position": screen(source, world)}
    raise ValueError("stage1私有败北缺完整静态selection ROI；未启动窗口")


def prepare(original, fixture, scenario):
    encoded, source, manifest = derive_fixture(fixture, scenario, original=original)
    tables = stage_tables(original)
    assets = round_assets(original)
    dimensions = struct.unpack_from("<HH", archive_entry((original / "FDFIELD.DAT").read_bytes(), 3))
    prepared = {"encoded": encoded, "source": source, "manifest": manifest, "tables": tables,
        "assets": assets, "titles": title_templates(original / "FDOTHER.DAT"), "dimensions": dimensions,
        "leader_world": tuple(source[UNITS:UNITS + 2]), "scenario": scenario}
    if scenario == "turn3":
        if any(source[UNITS + i * STRIDE + 0x22:UNITS + i * STRIDE + 0x28] != bytes(6) or
            source[UNITS + i * STRIDE + 0x40:UNITS + i * STRIDE + 0x42] != source[UNITS + i * STRIDE + 0x42:UNITS + i * STRIDE + 0x44]
            for i in range(21)):
            raise ValueError("turn3隔离夹具要求原21人满HP／无状态；不能豁免未知tick副作用")
        prepared["pages"] = turn3_pages(original, assets, source, tables)
        prepared["overview"] = overview_templates(original, source, assets["palette"])
        sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 5)
        prepared["banners"] = {}
        for turn in (2,3,4):
            banner = sheet_frame(sheet, 91, lmi=True); banner.putpalette(assets["palette"])
            number(banner,sheet,turn,51,7,42,3)
            prepared["banners"][turn] = banner
        if not prepared["pages"]: raise ValueError("stage1片段4没有资源等待门")
    else:
        geometry = bytearray(source)
        reference = native_focus_reference(tuple(source[META + 3:META + 5]), tuple(source[META + 5:META + 7]), prepared["leader_world"], dimensions)
        geometry[META + 3:META + 5] = bytes(reference["camera"])
        prepared["actor"] = actor_focus_resources(original, bytes(geometry), assets, 0, assets["cursor"])
        prepared["probe"] = selection_probe(original, bytes(geometry), assets, dimensions)
        prepared["result"] = result_templates(original, assets)
    return prepared


def check_phase_log(text):
    prefixes=[]
    for turn in (1,2,3): prefixes.extend((f"field end turn: turn={turn} phase=2 ",
        f"field phase: turn={turn} phase=1 ",f"field phase: turn={turn} phase=0 ",
        f"field phase: turn={turn + 1} phase=2 "))
    lines=text.splitlines();positions=[]
    for prefix in prefixes:
        found=[index for index,line in enumerate(lines) if line.startswith(prefix)]
        if len(found)!=1:raise AssertionError("stage1三次END的唯一phase边界缺失或重复")
        positions.append(found[0])
    if positions!=sorted(positions) or any(line.startswith("field phase:") and "turn=5 " in line for line in lines):
        raise AssertionError("stage1 phase顺序错误或多推进一轮")
    return {"exact_end_count":3,"ordered_phase_count":9,"banner_image_compared":False}


def exercise(capture, prepared, save, guard, report):
    source, assets = prepared["source"], prepared["assets"]
    def checkpoint(name, predicate, **options):
        result = wait_state(capture, name, predicate, report["checkpoints"], **options)
        require_source_unchanged(save, *guard)
        return result
    if prepared["scenario"] == "villager-defeat":
        if report["kind"] == "original":
            case = {"outcome": "defeat", "actor": prepared["actor"], "probe": prepared["probe"]}
            report["defeat"] = exercise_original(capture, case, assets, prepared["result"], prepared["titles"],
                screen(source, tuple(source[META + 5:META + 7])), report["checkpoints"], title_timeout=90)
            capture.wait(prepared["titles"][3][0], blank_transition=True, timeout=45)
            checkpoint("stage1-defeat-returned-title", lambda frame: {"kind": "full_title_resource", **found}
                if (found := exact_tile(frame, prepared["titles"][3][0], (0,0))) else None)
            report["defeat"]["returned_title_before_cleanup"] = True
        else:
            report["defeat"] = exercise_sdl_defeat_continuation(capture, assets, prepared["result"], prepared["titles"], report["checkpoints"])
            text=(capture.artifacts/"game.log").read_text(encoding="utf-8",errors="replace")
            if text.splitlines().count("field battle: defeat, stage=1")!=1 or "field battle: victory" in text or "field end turn:" in text:
                raise AssertionError("stage1私有败北没有唯一正确分派，或误入胜利／END")
        require_source_unchanged(save, *guard)
        return
    wait_title(capture, prepared["titles"][3][0], timeout=90)
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); capture.key("ENTER")
    geometry = bytearray(source)
    world = list(source[META + 5:META + 7])
    oracle = RoundOracle(assets, screen(source, world))
    checkpoint("stage1-continued", oracle.closed_browse, timeout=45)
    capture.key("ENTER"); checkpoint("stage1-overview-parent", lambda frame: oracle.menu(frame,0))
    capture.key("ENTER"); checkpoint("stage1-overview-secondary", lambda frame: oracle.secondary(frame,0))
    capture.key("ENTER"); checkpoint("stage1-overview", lambda frame: match_overview(frame,prepared["overview"]))
    capture.key("A"); checkpoint("stage1-overview-closed", oracle.closed_browse)
    report["overview_read_only_bytes_and_mtime"] = True
    for step in range(1,4):
        if step != 1:
            target = prepared["leader_world"][0] - 1, prepared["leader_world"][1]
            ref = native_focus_reference(tuple(geometry[META + 3:META + 5]), tuple(world), target, prepared["dimensions"])
            geometry[META + 3:META + 5] = bytes(ref["camera"]); world = list(target)
            capture.key("LEFT"); oracle = RoundOracle(assets, screen(geometry, world))
            checkpoint(f"stage1-end-{step}-empty", oracle.closed_browse)
        capture.key("ENTER"); checkpoint(f"stage1-end-{step}-parent", lambda frame: oracle.menu(frame,0))
        capture.key("DOWN"); checkpoint(f"stage1-end-{step}-selected", lambda frame: oracle.menu(frame,3))
        capture.key("ENTER"); checkpoint(f"stage1-end-{step}-yes", lambda frame: oracle.buttons(frame,0))
        capture.key("ENTER")
        if step == 3:
            report["story_acknowledgements"] = []
            for i,page in enumerate(prepared["pages"]):
                name = f"stage1-turn3-story-{i:02d}"
                checkpoint(name, lambda frame,p=page: page_match(frame,p,assets), timeout=120)
                report["story_acknowledgements"].append({"name":name,"fragment":4,"token_offset":page["token_offset"],"key":"ENTER"})
                capture.key("ENTER")
        if report["kind"] == "original":
            wait_state(capture,f"stage1-native-round-{step + 1}",
                lambda frame,n=step + 1: {"kind":"native_round_banner","turn":n,**found}
                    if (found := match_indexed(frame,prepared["banners"][n],(120,84)))["ok"] else None,
                report.setdefault("native_round_banners",[]),timeout=120)
        else:
            from field_play_input_test import wait_log
            wait_log(capture.process,capture.artifacts/"game.log",f"field phase: turn={step + 1} phase=2 ",timeout=120)
        _, ready = checkpoint(f"stage1-round-{step + 1}-leader", lambda frame: entry_browse(frame, assets,
            {"first_focus_world":list(prepared["leader_world"])}), timeout=120)
        geometry[META + 3:META + 5] = bytes(ready["inferred_camera"])
        world = list(prepared["leader_world"])
    target = world[0] - 1, world[1]
    ref = native_focus_reference(tuple(geometry[META + 3:META + 5]), tuple(world), target, prepared["dimensions"])
    geometry[META + 3:META + 5] = bytes(ref["camera"])
    capture.key("LEFT"); oracle = RoundOracle(assets, screen(geometry,target))
    checkpoint("stage1-save-empty", oracle.closed_browse)
    capture.key("ENTER"); checkpoint("stage1-save-parent", lambda frame: oracle.menu(frame,0))
    capture.key("ENTER"); checkpoint("stage1-save-secondary", lambda frame: oracle.secondary(frame,0))
    capture.key("LEFT"); checkpoint("stage1-save-selected", lambda frame: oracle.secondary(frame,1))
    capture.key("ENTER"); checkpoint("stage1-save-yes", lambda frame: oracle.buttons(frame,0))
    capture.key("ENTER")
    measured = wait_measured_save(save,*guard)
    snapshot = capture.artifacts / "stage1-turn4-measured.SAV"; snapshot.write_bytes(save.read_bytes())
    report["measurement"] = check_measurement(source, measured, prepared["tables"],
        {"camera":ref["camera"], "measuring_focus":target})
    guard[:] = [save.read_bytes(),save.stat().st_mtime_ns]
    report["completed_save"] = {"snapshot":str(snapshot),"sha256":hashlib.sha256(guard[0]).hexdigest(),"mtime_ns":guard[1]}
    checkpoint("stage1-save-returned-browse", oracle.closed_browse)
    if report["kind"] == "sdl":
        text = (capture.artifacts / "game.log").read_text(encoding="utf-8",errors="replace")
        phase = [line for line in text.splitlines() if line.startswith("field phase:")]
        report["phase_log"] = phase
        report["phase_log_check"] = check_phase_log(text)


def compare_cases(cases, output, prepared):
    if [case["kind"] for case in cases] != ["original","sdl"] or not all(case["probe_cases_passed"] for case in cases):
        raise AssertionError("stage1必须原版先完整通过并清理，再SDL通过")
    if prepared["scenario"] == "villager-defeat":
        for case in cases:
            detail=case.get("defeat",{})
            final=detail.get("game_over") if case["kind"]=="original" else detail.get("final_composite")
            if not detail.get("returned_title_before_cleanup") or not final or final.get("frame")!=1 or final.get("full_frame_pixels")!=64000:
                raise AssertionError("stage1败北缺完整GAME OVER帧1／真实返回标题凭据")
        return {"both_complete_game_over_and_actual_title":True,"full_game_over_resource_pixels":64000,"shared_rng_aligned":False}
    records = [paired_checkpoint_records(case,output / case["kind"])[0] for case in cases]
    if records[0].keys() != records[1].keys(): raise AssertionError("stage1业务检查点集合不同")
    frames = [Image.open(output / role / "stage1-overview.png").convert("RGB") for role in ("original","sdl")]
    overview = compare_overview(*frames)
    if not overview["ok"]: raise AssertionError("stage1总览4完整矩形实际RGB差异超过3")
    rgb = []
    for i,page in enumerate(prepared["pages"]):
        frames = [Image.open(output / role / f"stage1-turn3-story-{i:02d}.png").convert("RGB") for role in ("original","sdl")]
        diff = rgb_difference(*frames,(0,0),(320,200),page["text"])
        if not diff["within_tolerance"]: raise AssertionError("stage1片段4实际文字RGB差异超过3")
        rgb.append(diff)
    values = [bytearray(decode_save(Path(case["completed_save"]["snapshot"]))) for case in cases]
    from fd2_stage1_oracle import NEW_UNKNOWN
    for data in values:
        for raw in range(21,27):
            for offset in (2,*NEW_UNKNOWN): data[UNITS + raw * STRIDE + offset] = 0
        data[-4:] = bytes(4)
    if values[0] != values[1]: raise AssertionError("stage1全容器已确认字节／实际几何跨端不同")
    return {"overview_four_rectangles":overview,"story_resource_rgb":rgb,
        "container_equal_except_exact_new_constructor_holes_cache_and_checksum":True,
        "unknown_offsets_each_new_record":list(NEW_UNKNOWN),"new_cache_offset":2,
        "whole_encoded_equal":Path(cases[0]["completed_save"]["snapshot"]).read_bytes()==Path(cases[1]["completed_save"]["snapshot"]).read_bytes(),
        "shared_rng_aligned":False}


def endpoint_checks(case, original, before, stamps, fixture, data, stamp, save, guard):
    endpoint_guard(case,original,before,fixture,data,stamp,save,guard)
    case["original_all_mtimes_unchanged"] = case.get("original_all_mtimes_unchanged",True) and all(
        (original/name).stat().st_mtime_ns==value for name,value in stamps.items())
    case["probe_cases_passed"] &= case["original_all_mtimes_unchanged"]
    if not case["probe_cases_passed"]:
        case["status"]="failed"
        case.setdefault("error","stage1端点最后来源／时间戳／清理守卫失败")


def run(args):
    root,output = args.root.resolve(),args.artifacts.resolve()
    original,fixture = (root / "original_game").resolve(),args.save_fixture.resolve()
    if output == original or original in output.parents or output == fixture.parent or fixture.parent in output.parents:
        raise ValueError("stage1输出不能写入原版或只读来源证据目录")
    for path in (args.driver,args.dosbox,fixture) + ((args.sdl,) if args.sdl is not None else ()):
        if not path.is_file(): raise ValueError(f"stage1必需文件不存在：{path}")
    prepared = prepare(original,fixture,args.scenario)
    before = hashes(original); stamps={name:(original/name).stat().st_mtime_ns for name in before}
    fixture_data,fixture_stamp=fixture.read_bytes(),fixture.stat().st_mtime_ns
    output.mkdir(parents=True,exist_ok=False)
    report={"ok":False,"probe_only":args.sdl is None,"probe_cases_passed":False,"status":"failed",
        "scenario":args.scenario,"scope":"B5 controlled stage1 new paths only; no inherited GUI passes",
        "excluded":"natural stage1 combat, new NPC malloc0x28..0x30/cache, whole FIGANI/audio/shared RNG timing",
        "capture_backend":"Windows.Graphics.Capture HWND","derived_fixture":prepared["manifest"],"original_before":before,"cases":[]}
    report["binaries"]={name:{"path":str(path.resolve()),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
        for name,path in (("driver",args.driver),("dosbox",args.dosbox)) + ((("sdl",args.sdl),) if args.sdl is not None else ())}
    project=Path(__file__).resolve().parents[1]
    report["tool_sources"]={name:{"path":str(Path(module.__file__).resolve()),"sha256":hashlib.sha256(Path(module.__file__).read_bytes()).hexdigest()}
        for name,module in tuple(sys.modules.items()) if getattr(module,"__file__",None) and Path(module.__file__).suffix==".py"
        and (Path(module.__file__).resolve().is_relative_to(project/"tools") or Path(module.__file__).resolve().is_relative_to(project/"src/tests"))}
    (output/"fixture-manifest.json").write_text(json.dumps(prepared["manifest"],ensure_ascii=False,indent=2),encoding="utf-8")
    guards={}
    try:
        for role in (("original",) if args.sdl is None else ("original","sdl")):
            work=output/role;work.mkdir();environment=os.environ.copy()
            for key in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER"):environment.pop(key,None)
            if role=="original":
                game=work/"game";shutil.copytree(original,game);save=game/"FD2.SAV"
                config=work/"dosbox.conf"
                config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
                    f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                command,cwd=[str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
            else:
                save=work/"FD2.SAV";environment["FD2SDL_SAVE_PATH"]=str(save);command,cwd=[str(args.sdl.resolve())],root
            save.write_bytes(prepared["encoded"]);guard=[prepared["encoded"],save.stat().st_mtime_ns];guards[role]=(save,guard)
            if decode_save(save)!=prepared["source"]:raise AssertionError("stage1派生XOR/checksum往返不符；未启动窗口")
            case={"kind":role,"status":"failed","probe_cases_passed":False,"probe_only":True,"ok":False,
                "checkpoints":[],"inputs":[],"processes":[],"binaries":report["binaries"],"tool_sources":report["tool_sources"]}
            report["cases"].append(case)
            try:
                with running(command,cwd,environment,work/"game.log",args.driver.resolve(),case["processes"]) as process:
                    capture=Capture(args.driver.resolve(),process,work,case["inputs"],startup_skip=True)
                    exercise(capture,prepared,save,guard,case)
                    require_source_unchanged(save,*guard)
                case["probe_cases_passed"],case["status"]=True,"passed"
            except Exception as error:case.setdefault("error",str(error))
            finally:
                endpoint_checks(case,original,before,stamps,fixture,fixture_data,fixture_stamp,save,guard)
                case["capture_hashes"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in work.iterdir() if p.is_file() and p.suffix.lower() in (".png",".bmp",".sav")}
                (work/"comparison.json").write_text(json.dumps(case,ensure_ascii=False,indent=2),encoding="utf-8")
            if not case["probe_cases_passed"]:raise AssertionError(case.get("error","stage1端点失败"))
        if args.sdl is not None:report["paired"]=compare_cases(report["cases"],output,prepared)
        report["probe_cases_passed"],report["status"]=True,"passed"
    except Exception as error:report.setdefault("error",str(error))
    finally:
        report["original_unchanged"]=hashes(original)==before
        report["original_all_mtimes_unchanged"]=all((original/name).stat().st_mtime_ns==stamp for name,stamp in stamps.items())
        for case in report["cases"]:
            save,guard=guards[case["kind"]]
            endpoint_checks(case,original,before,stamps,fixture,fixture_data,fixture_stamp,save,guard)
            (output/case["kind"]/"comparison.json").write_text(json.dumps(case,ensure_ascii=False,indent=2),encoding="utf-8")
        report["probe_cases_passed"] &= report["original_unchanged"] and report["original_all_mtimes_unchanged"] and bool(report["cases"]) and all(case["probe_cases_passed"] for case in report["cases"])
        report["ok"]=args.sdl is not None and report["probe_cases_passed"] and len(report["cases"])==2
        if not report["probe_cases_passed"]:report["status"]="failed"
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("root","driver","dosbox","artifacts","save-fixture"):parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--sdl",type=Path);parser.add_argument("--scenario",choices=SCENARIOS,required=True)
    args=parser.parse_args()
    if sys.platform!="win32":parser.error("stage1原版对照需要Windows HWND WGC；未启动窗口")
    try:
        report=run(args);print(json.dumps(report,ensure_ascii=False,indent=2));return 0 if report["probe_cases_passed"] else 1
    except Exception as error:print(f"stage1前置失败：{error}；未补键／重抽。",file=sys.stderr);return 1


if __name__=="__main__":sys.exit(main())
