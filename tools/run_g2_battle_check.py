#!/usr/bin/env python3
"""G2定向原版／SDL：正式CONTINUE、资源输入门、完整SAV有限结果。

复用既有owned Job、WGC单窗、后台投递和清理设施。只运行命名代表夹具，
不开放新关；持续PNG是原件采集，不自动等于完整动画RGB／声音对照。
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import sys

from PIL import Image
import fd2_g2_battle_oracle as oracle
import fd2_g2_derived_oracle as derived
from fd2_display_requirement import windows_display_requirement
from fd2_item_list_oracle import inventory_templates
from fd2_reward_oracle import magic_page, reward_pages, page_match
from run_original_comparison import Capture, archive_entry, running, title_templates, normalize
from run_original_battle_animation import OwnedHelper, owned_job_required, wait_ready, series_index
from run_original_battle_menu import wait_state, wait_title
from run_original_end_turn import RoundOracle, round_assets, round_two_template, check_round_phase_log
from run_original_load import focused_actor, require_source_unchanged, wait_measured_save
from run_original_magic_list import actor_focus_resources
from run_original_player import exact_tile, match_indexed, screen, sheet_frame
from run_original_result import selection_probe, selection_state
from run_original_reward import actor_view, command, list_state, navigate, quiet_browse
from run_graphics_tests import decode_save
from run_original_item_list import use_range_probe
from field_play_input_test import wait_log

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_paths(root):
    files = set()
    for p in (root / "src").rglob("*"):
        parts = p.relative_to(root / "src").parts
        if (p.is_file() and p.suffix.lower() in (".c", ".h", ".cpp") and
                not any(part == "tests" or part.startswith("build") or part in ("Debug", "Release", "_deps", "CMakeFiles") for part in parts)):
            files.add(p.resolve())
    files.update(p.resolve() for p in (root / "data/static").rglob("*.json"))
    files.update(p.resolve() for p in (root / "src/build-win/generated/static_data").glob("*.inc"))
    return files


def fingerprint(original):
    return {str(p.relative_to(original)): {"sha256": sha(p), "size": p.stat().st_size, "mtime_ns": p.stat().st_mtime_ns}
        for p in sorted(original.rglob("*")) if p.is_file()}


def freeze(root, original, binaries):
    files = set(binaries)
    sources = source_paths(root)
    files.update(sources)
    # 本轮真实import闭包；延迟用到的wait_log也在顶层加载，随后不另import新测试模块。
    for module in tuple(sys.modules.values()):
        path = getattr(module,"__file__",None)
        if path and Path(path).resolve().is_relative_to(root / "tools"):
            files.add(Path(path).resolve())
    return {str(p): sha(p) for p in sorted(files)}, fingerprint(original), sorted(map(str, sources))


def unchanged(inputs, original, original_before, root, sources):
    return (sorted(map(str, source_paths(root))) == sources and
        all(Path(p).is_file() and sha(p) == value for p, value in inputs.items()) and
        fingerprint(original) == original_before)


def prepare(root, scenario):
    original = (root / "original_game").resolve()
    model = derived if scenario == derived.SCENARIO else oracle
    encoded, source, manifest = model.derive_fixture(original, scenario)
    assets = round_assets(original)
    caster = oracle.actor(source, 1)
    _, upper, _ = inventory_templates(original, caster, assets)
    probe = model.phase_probes(original, source, scenario, assets)
    prepared = {"original": original, "source": source, "encoded": encoded, "manifest": manifest,
        "contract": model.expected_contract(original, source, scenario), "assets": assets, "scenario": scenario,
        "titles": title_templates(original / "FDOTHER.DAT"), "probe": probe,
        "source_actors": [actor_focus_resources(original, source, assets, raw, assets["cursor"]) for raw in (0, 1)],
        "movement": derived.movement_probe(original,source,assets) if scenario == derived.SCENARIO else
            selection_probe(original, source, 1, assets),
        "magic_lists": [magic_page(original, caster, assets, 0)[0]],
        "list_upper": {key: upper[key] for key in ("actor_name", "current_max_mp")}}
    if scenario != "teleport23-cancel-destination":
        prepared["measurement"] = model.expected_end_contract(original,source,scenario)
        prepared["banner"] = round_two_template(original / "FDOTHER.DAT",assets["palette"])
        if scenario == derived.SCENARIO:
            prepared["visual_after"] = model.expected_sources(original,source,engine="sdl")[0][0]
            prepared["visual_final"] = model.expected_sources(original,source,engine="sdl",after_end=True)[0][0]
            prepared["reward_alternatives"] = [(event["value"],reward_pages(original,assets,19,[event])[0])
                for event in prepared["contract"]["reward_pages"]]
        else:
            if scenario == "composite34":
                # 原122dc mode5；只用无actor/上格遮挡的完整选择sprite正证。
                pieces = [(0,0,1),(0,-3,2),(-3,0,3),(3,0,4),(0,3,5),(-1,-2,6),(-2,-1,6),
                (1,-2,7),(2,-1,7),(-1,2,8),(-2,1,8),(1,2,9),(2,1,9),(0,-2,10),
                (-2,0,11),(2,0,12),(0,2,13),(-1,-1,15),(1,-1,16),(-1,1,17),(1,1,18)]
                center = tuple(manifest["center"])
                occupied = {tuple(oracle.actor(source, raw)[:2]) for raw in range(source[oracle.META+1]) if not oracle.actor(source, raw)[5]&1}
                obscured = occupied | {(x,y-1) for x,y in occupied}
                sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 1)
                checks = []
                for dx, dy, frame in pieces:
                    world = (center[0]+dx, center[1]+dy)
                    if world in obscured: continue
                    position = screen(source, world)
                    if not (4 <= position[0] <= 292 and 4 <= position[1] <= 172):
                        raise ValueError("G2 mode5选择sprite不得靠裁切取得正证")
                    image = sheet_frame(sheet, frame); image.putpalette(assets["palette"])
                    checks.append((position, image, frame))
                if len(checks) < 16: raise ValueError("G2 mode5缺少足够完整未遮挡的选择sprite")
                cursor = sheet_frame(sheet, 1); cursor.putpalette(assets["palette"])
                prepared["target"] = actor_focus_resources(original, source, assets, 0, cursor)
                prepared["target_range"] = use_range_probe(original, source, assets, 5, 3, center, caster_raw=1)
                prepared["selector_checks"] = checks
            visual = bytearray(source)
            for change in prepared["contract"]["fixed_updates"]:
                data = bytes.fromhex(change["hex"]); visual[change["offset"]:change["offset"]+len(data)] = data
            for change in prepared["contract"]["finite_bytes"]: visual[change["offset"]] = 2
            prepared["visual_after"] = oracle.checksum(visual)
            final = bytearray(source)
            for change in prepared["measurement"]["fixed_updates"]:
                data = bytes.fromhex(change["hex"]); final[change["offset"]:change["offset"]+len(data)] = data
            for change in prepared["measurement"]["finite_bytes"]: final[change["offset"]] = change["values"][0]
            prepared["visual_final"] = oracle.checksum(final)
        icons = (original / "FDICON.B24").read_bytes()
        acted = []
        for phase in range(3):
            image = sheet_frame(icons, caster[7]*12+phase)
            image = image.point(lambda value: (value&7)+24 if value else 0)
            image.putpalette(assets["palette"]); acted.append(image)
        prepared["acted"] = (screen(source, tuple(caster[:2])), acted)
    else:
        prepared["measurement"] = prepared["contract"]
        prepared["visual_after"] = source
        prepared["visual_final"] = source
    # 运行前确认之后每个导航格可由既有完整资源模型构造。
    from run_original_reward import focus_steps
    for _, world, _ in focus_steps(source, tuple(prepared["contract"]["expected_focus_before_save_navigation"]), (7,14)):
        if not (0 <= world[0] < 24 and 0 <= world[1] < 24): raise ValueError("G2导航越出已核地图")
    return prepared


def range_match(frame, probe, *, base=False):
    if base:
        hit = exact_tile(frame, probe["base"], probe["position"])
        return {"kind": "complete_restored_base", "roi": hit} if hit else None
    for phase, image in probe["ranges"]:
        hit = exact_tile(frame, image, probe["position"])
        if hit: return {"kind": "complete_cast_range", "phase": phase, "roi": hit}
    return None


def target_match(frame, prepared, *, destination=False):
    assets, probe = prepared["assets"], prepared["probe"]
    if prepared["scenario"] != "composite34":
        actor = focused_actor(frame, assets, probe["target"])
        hit = range_match(frame, probe["cast_range"] if prepared["scenario"] == derived.SCENARIO else
            probe["first_layer_colored_range"], base=destination)
        return {"target": actor, "range": hit, "destination": destination} if actor and hit else None
    actor = focused_actor(frame, assets, prepared["target"])
    hit = range_match(frame, prepared["target_range"])
    selectors = [match_indexed(frame, image, position)["ok"] for position, image, _ in prepared["selector_checks"]]
    if actor and hit and all(selectors):
        return {"target": actor, "range": hit, "display": 5, "complete_unobscured_selector_tiles": len(selectors)}
    return None


def banner_match(frame,prepared):
    hit = match_indexed(frame,prepared["banner"],(120,84))
    return {"kind":"original_round_002","opaque_resource_pixels":hit,
        "transparent_background_compared":False} if hit["ok"] else None


def destination_match(frame,prepared):
    probe = prepared["probe"]
    destination = probe["destination_selected"]
    cursor = exact_tile(frame,destination["composite"],destination["position"])
    base = range_match(frame,probe["first_layer_colored_range"],base=True)
    return {"kind":"teleport23_empty_destination","full_tile_and_frame1":cursor,
        "restored_range_roi":base} if cursor and base else None


@contextmanager
def series(capture, record, *, interval_ms=33):
    if type(interval_ms) is not int or not 16<=interval_ms<=67:
        raise ValueError("连续WGC采样间隔必须为16..67整数毫秒")
    directory = capture.artifacts / "action-series"
    slot = {}; record["series_helper"] = slot
    helper = OwnedHelper(capture.driver, [capture.driver,"capture-series",capture.process.pid,directory,45000,interval_ms],
        capture.artifacts, "g2-series", slot)
    ready = None
    try:
        child = helper.start(); ready = wait_ready(directory / "ready.json", child, capture.process)
        if ready.get("pid") != capture.process.pid or not ready.get("hwnd"):
            raise AssertionError("G2持续WGC的ready owner不符")
        yield
    finally:
        if directory.exists(): (directory / "stop").write_text("stop\n", encoding="utf-8")
        helper.finish()
        if ready is not None:
            frames = series_index(directory, slot["summary"], ready, capture.process.pid)
            record["continuous_wgc"] = {"ready": ready, "summary": slot["summary"], "frames": frames,
                "complete_raw_collection": True, "full_RGB_equivalence_claimed": False}


def save_measurement(capture, prepared, save, guard, record, checkpoint):
    assets = prepared["assets"]; view = RoundOracle(assets, screen(prepared["source"], (6,14)))
    capture.key("LEFT"); checkpoint("g2-to-empty-save", view.closed_browse)
    capture.key("ENTER"); checkpoint("g2-save-parent", lambda f: view.menu(f,0))
    capture.key("ENTER"); checkpoint("g2-save-secondary", lambda f: view.secondary(f,0))
    capture.key("LEFT"); checkpoint("g2-save-selected", lambda f: view.secondary(f,1))
    capture.key("ENTER"); checkpoint("g2-save-default-yes", lambda f: view.buttons(f,0))
    require_source_unchanged(save, *guard)
    capture.key("ENTER")
    measured = wait_measured_save(save, *guard)
    stamp = save.stat().st_mtime_ns
    encoded = save.read_bytes()
    require_source_unchanged(save, encoded, stamp)
    snapshot = capture.artifacts / "measured.SAV"; snapshot.write_bytes(encoded)
    if decode_save(snapshot) != measured:
        raise AssertionError("G2已核明文与保存的同一份encoded快照不符")
    guard[:] = [encoded, stamp]
    model = derived if prepared["scenario"] == derived.SCENARIO else oracle
    result = model.check_result(prepared["original"], prepared["source"], prepared["scenario"], measured,
        engine=record["kind"], after_end=prepared["scenario"] != "teleport23-cancel-destination")
    record["completed_save"] = {"path": str(snapshot), "sha256": sha(snapshot), "mtime_ns": guard[1]}
    record["state_comparison"] = result
    if not result["ok"]: raise AssertionError("G2完整活动SAV不符合预声明原义："+str(result))
    if prepared["scenario"] == derived.SCENARIO:
        gain = (measured[oracle.UNITS+80+60]-prepared["source"][oracle.UNITS+80+60])&255
        if gain != record["reward_page"]["gain"]:
            raise AssertionError("G2派生奖励页面与同一次完整SAV中的EX不符")
    checkpoint("g2-saved-browse", view.closed_browse)


def end_once(capture,prepared,checkpoint,record):
    contract = prepared["measurement"]["end_turn"]
    view = RoundOracle(prepared["assets"],screen(prepared["source"],tuple(contract["end_menu_world"])))
    capture.key("ENTER"); checkpoint("g2-end-parent",lambda f:view.menu(f,0))
    capture.key("DOWN"); checkpoint("g2-end-selected",lambda f:view.menu(f,3))
    capture.key("ENTER"); checkpoint("g2-end-default-yes",lambda f:view.buttons(f,0))
    capture.key("ENTER")
    if record["kind"] == "original":
        checkpoint("g2-original-round002",lambda f:banner_match(f,prepared),timeout=120)
    else:
        text = wait_log(capture.process,capture.artifacts / "game.log","field phase: turn=2 phase=2 ",timeout=120)
        check_round_phase_log(text)
        record["sdl_phase_log"] = {"ordered_single_end_1_to_0_to_new2":True,"banner_image_compared":False}
    world = tuple(contract["focus_after_end"])
    checkpoint("g2-turn2-ready",quiet_browse(prepared,prepared["visual_final"],world,record,"g2-turn2-ready"),timeout=90)


def observed_reward(prepared,record):
    matches = {}
    for row in record["continuous_wgc"]["frames"]:
        encoded = Path(row["path"]).read_bytes()
        if hashlib.sha256(encoded).hexdigest() != row["sha256"]:
            raise AssertionError("G2奖励匹配的PNG与已登记连续原件身份不符")
        with Image.open(io.BytesIO(encoded)) as image:
            frame = normalize(image)
        for gain,page in prepared["reward_alternatives"]:
            hit = page_match(frame,page,prepared["assets"])
            if hit:
                matches.setdefault(gain,{"gain":gain,"index":row["index"],"path":row["path"],
                    "sha256":row["sha256"],"resource_match":hit,"key_sent":False})
    if len(matches) != 1:
        raise AssertionError("G2连续原件未取得唯一的完整488自动奖励资源门："+str(sorted(matches)))
    record["reward_page"] = next(iter(matches.values()))


def exercise(capture, prepared, save, guard, record):
    source, assets = prepared["source"], prepared["assets"]
    def checkpoint(name, predicate, **options):
        result = wait_state(capture, name, predicate, record["checkpoints"], **options)
        require_source_unchanged(save, *guard)
        return result
    wait_title(capture, prepared["titles"][3][0], timeout=90)
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); capture.key("ENTER")
    initial_at = list(screen(source, tuple(source[oracle.META+5:oracle.META+7])))
    def continued(frame):
        hit = focused_actor(frame,assets,prepared["source_actors"][1])
        return hit if hit and hit["focus_screen"] == initial_at else None
    checkpoint("g2-continued",continued,timeout=45)
    for actor in prepared["source_actors"]:
        capture.key("Z"); checkpoint("g2-focus-"+str(actor["raw_actor"]), lambda f,a=actor: focused_actor(f,assets,a))
    capture.key("ENTER")
    selection = lambda frame: selection_state(frame,assets,prepared["source_actors"][1],prepared["movement"])
    checkpoint("g2-movement", selection)
    capture.key("ENTER"); actor = actor_view(prepared["original"],source,assets,1)
    command_frames = [2,4,6,9]
    if prepared["scenario"] == derived.SCENARIO:
        gate = prepared["manifest"]["command_gate"]
        checkpoint("g2-attack-command-default",lambda f:command(f,source,actor,1,gate["default_selected"],gate["initial_frames"]))
        capture.key(gate["select_magic_key"])
        command_frames = gate["magic_frames"]
    command_gate = lambda frame: command(frame,source,actor,1,1,command_frames)
    checkpoint("g2-magic-command", command_gate)
    capture.key("ENTER"); checkpoint("g2-single-magic-list", lambda f:list_state(f,prepared,0))
    capture.key("ENTER"); checkpoint("g2-first-target", lambda f:target_match(f,prepared))
    if prepared["scenario"] != "teleport23-cancel-destination":
        if prepared["scenario"] == "teleport23-success":
            capture.key("ENTER"); checkpoint("g2-destination-positive",lambda f:target_match(f,prepared,destination=True))
            capture.key("RIGHT"); checkpoint("g2-destination-selected",lambda f:destination_match(f,prepared))
        world = tuple(prepared["contract"]["expected_focus_before_save_navigation"])
        browse = quiet_browse(prepared,prepared["visual_after"],world,record,"g2-action-complete")
        position, grey = prepared["acted"]
        def complete(frame):
            hit = browse(frame)
            phase = next((i for i,image in enumerate(grey) if match_indexed(frame,image,(position[0],position[1]-6))["ok"]),None)
            return {"browse": hit, "caster_acted_phase": phase} if hit and phase is not None else None
        with series(capture,record):
            capture.key("ENTER"); checkpoint("g2-action-complete",complete,timeout=120)
        if prepared["scenario"] == derived.SCENARIO:
            observed_reward(prepared,record)
        navigate(capture,prepared,prepared["visual_after"],world,(6,14),checkpoint,"g2-to-end")
        end_once(capture,prepared,checkpoint,record)
    else:
        capture.key("ENTER"); checkpoint("g2-destination-positive",lambda f:target_match(f,prepared,destination=True))
        capture.key("ESC"); checkpoint("g2-destination-cancel-list",lambda f:list_state(f,prepared,0))
        capture.key("ENTER"); checkpoint("g2-first-target-reopened",lambda f:target_match(f,prepared))
        capture.key("ESC"); checkpoint("g2-first-target-cancel-list",lambda f:list_state(f,prepared,0))
        capture.key("ESC"); checkpoint("g2-list-cancel-command",command_gate)
        capture.key("ESC"); checkpoint("g2-command-cancel-movement",selection)
        capture.key("ESC"); checkpoint("g2-selection-cancel-browse",quiet_browse(prepared,source,(7,14),record,"g2-selection-cancel-browse"))
    start = tuple(prepared["measurement"]["expected_focus_before_save_navigation"])
    navigate(capture,prepared,prepared["visual_final"],start,(7,14),checkpoint,"g2-after-action")
    save_measurement(capture,prepared,save,guard,record,checkpoint)


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    if output == original or output.is_relative_to(original): raise ValueError("G2产物不能写入原版只读目录")
    if output.exists(): raise ValueError("G2产物目录已存在；保留旧现场并使用新目录")
    original_before = fingerprint(original)
    prepared = prepare(root,args.scenario)
    if fingerprint(original) != original_before:
        raise AssertionError("G2只读来源在构造夹具／期望期间变化")
    output.mkdir(parents=True)
    (output / "fixture-manifest.json").write_text(json.dumps(prepared["manifest"],ensure_ascii=False,indent=2),encoding="utf-8")
    (output / "measurement-contract.json").write_text(json.dumps(prepared["measurement"],ensure_ascii=False,indent=2),encoding="utf-8")
    fixture = output / "controlled.SAV"; fixture.write_bytes(prepared["encoded"])
    if decode_save(fixture) != prepared["source"]: raise AssertionError("G2夹具codec往返不符")
    report = {"ok":False,"status":"ready_not_executed","scenario":args.scenario,"cases":[],
        "scope":"named shared battle semantic fixture; full bounded SAV + native input/WGC checkpoints",
        "full_animation_RGB_or_audio_equivalence_claimed":False,"shared_RNG_aligned":False,
        "unsaved_state_not_measured":prepared["manifest"]["unsaved_state_not_measured"]}
    if args.prepare_only:
        (output / "comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
        return report
    owned_job_required()
    binaries = [args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()]
    inputs, frozen_original, sources = freeze(root,original,binaries)
    if frozen_original != original_before:
        raise AssertionError("G2只读来源在准备与冻结之间变化")
    def sources_unchanged():
        return unchanged(inputs,original,original_before,root,sources)
    inputs[str(fixture)] = sha(fixture)
    report.update(status="failed",inputs=inputs,original_before=original_before,source_files=sources)
    display = {}; report["display_requirement"] = display
    try:
        with windows_display_requirement(display):
            for kind in ("original","sdl"):
                if not sources_unchanged(): raise AssertionError("G2冻结来源在窗口启动前变化")
                work = output / kind; work.mkdir(); env = os.environ.copy()
                for key in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER","FD2SDL_FIELD_RNG_SEED",
                        "FD2SDL_APP_PCM_CAPTURE_DIR","FD2SDL_APP_PCM_CAPTURE_QUIET","FD2SDL_TEST_AUDIO_QUIET"):
                    env.pop(key,None)
                if kind == "original":
                    game = work / "game"; shutil.copytree(original,game); save = game / "FD2.SAV"
                    config = work / "dosbox.conf"
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                    command_line,cwd=[str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
                else:
                    save=work/"FD2.SAV"; env["FD2SDL_SAVE_PATH"]=str(save)
                    command_line,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared["encoded"]); guard=[save.read_bytes(),save.stat().st_mtime_ns]
                record={"kind":kind,"ok":False,"processes":[],"inputs":[],"checkpoints":[]};report["cases"].append(record)
                try:
                    with running(command_line,cwd,env,work/"game.log",args.driver.resolve(),record["processes"]) as process:
                        capture=Capture(args.driver.resolve(),process,work,record["inputs"])
                        exercise(capture,prepared,save,guard,record)
                    record["ok"]=True
                finally:
                    record["guards"]={"sources_unchanged":sources_unchanged(),
                        "save_matches_completed":save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        "cleanup":len(record["processes"])==1 and all(r["cleaned"] and not r["forced_cleanup"] and r["exit_code"]==0 for r in record["processes"])}
                    record["ok"] &= all(record["guards"].values())
                    (work/"comparison.json").write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
                if not record["ok"]: raise AssertionError("G2端点守卫或清理未通过")
        report["ok"]=len(report["cases"])==2 and all(r["ok"] for r in report["cases"])
        report["status"]="passed" if report["ok"] else "failed"
    except BaseException as error:
        report["error"]={"type":type(error).__name__,"message":str(error)}
    finally:
        report["final_sources_unchanged"]=sources_unchanged()
        report["ok"] &= report["final_sources_unchanged"] and display.get("released") is True
        if not report["ok"]: report["status"]="failed"
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("root","artifacts"): parser.add_argument("--"+name,type=Path,required=True)
    for name in ("sdl","driver","dosbox"): parser.add_argument("--"+name,type=Path)
    parser.add_argument("--scenario",choices=(*oracle.SCENARIOS,*derived.SCENARIOS),required=True)
    parser.add_argument("--prepare-only",action="store_true")
    args=parser.parse_args()
    if not args.prepare_only and any(getattr(args,name) is None or not getattr(args,name).is_file() for name in ("sdl","driver","dosbox")):
        parser.error("实际运行必须指定存在的SDL、driver和DOSBox程序")
    report=run(args)
    print(json.dumps({key:report[key] for key in ("ok","status","scenario")},ensure_ascii=False))
    return 0 if report["ok"] or args.prepare_only else 1


if __name__ == "__main__":
    raise SystemExit(main())
