#!/usr/bin/env python3
"""自然START完整32→31→0原版/SDL配对：逐wait资源ACK，首可操作画面和UI Save。

依据new_game_opening_play @0x3231b（原登记同名）；使用fd2_opening_oracle的
纯资源门。仅driver run-job串行WGC，不接受ignored研究目录或捕获reference。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import sys

from run_original_comparison import Capture, hashes, running, title_templates
from run_original_battle_menu import wait_state, wait_title
from run_original_end_turn import META, RoundOracle, round_assets
from run_original_load import require_source_unchanged, wait_measured_save
from run_graphics_tests import check_roster_count, decode_save
from fd2_opening_oracle import build_pages, page_match, first_browse, geometry, measurement, compare_measurements


def title_menu_counts(source):
    # source先由decode_save验证完整容器；SDL getter另要求两种count在容量内。
    if source[META+1] > 96:
        raise ValueError("source active count exceeds the save codec capacity")
    check_roster_count(source)
    stage = source[META+2]
    return {"original":2 if stage==0xff else 3,"sdl":3 if stage==0 else 2}


def load_menu_availability(source):
    title_menu_counts(source)  # 沿用完整容器之后的active/roster容量前置。
    stage0 = source[META+2] == 0
    # 不复制validate_import：stage0仅接受已确认可导入的真实START完整快照。
    if stage0 and hashlib.sha256(source).hexdigest() != "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975":
        raise ValueError("此opening对照工具只验证真实START的stage0 Load判据，尚未验证其它stage0来源；这是工具范围限制，不是游戏限制")
    # native19df7仅fopen失败才disabled；存在的FF文件仍有Load，区别于标题门。
    return {"original":True,"sdl":stage0}


def exercise(capture,assets,pages,titles,save,source_bytes,stamp,expected,report):
    def checkpoint(name,predicate,**kwargs):
        frame,detail = wait_state(capture,name,predicate,report["checkpoints"],**kwargs)
        require_source_unchanged(save,source_bytes,stamp)
        return frame,detail
    wait_title(capture,titles[report["title_menu_count"]][0])
    capture.key("ENTER")  # 资源已证明默认START0；绝不CONTINUE。
    for ordinal,page in enumerate(pages,1):
        name = f"wait-{ordinal:03d}-stage{page['stage']}-fragment{page['fragment']}"
        page["text"].save(capture.artifacts/(name+"-expected-text.png"))
        checkpoint(name,lambda frame,p=page:page_match(frame,p,assets),timeout=90)
        capture.key("ENTER")  # 每个已证明wait恰一键，无skip/补键。
    _,first = checkpoint("first-operable-before-input",lambda frame:first_browse(frame,assets,expected["first_focus_world"]),timeout=30)
    report["first_browse"] = first
    sx,sy = first["focus_screen"]
    if sx<52 or not (28<=sy<=148):
        raise ValueError("no verified left-adjacent Save menu cell without scrolling")
    capture.key("LEFT")
    oracle = RoundOracle(assets,(sx-24,sy))
    checkpoint("measurement-left-empty",oracle.closed_browse)
    capture.key("ENTER")
    checkpoint("measurement-parent",lambda frame:oracle.menu(frame,0))
    capture.key("UP")
    checkpoint("measurement-parent-upper",lambda frame:oracle.menu(frame,0))
    capture.key("ENTER")
    checkpoint("measurement-secondary",lambda frame:oracle.secondary(frame,0,load_available=report["load_available"]))
    capture.key("LEFT")
    checkpoint("measurement-save-selected",lambda frame:oracle.secondary(frame,1,load_available=report["load_available"]))
    capture.key("ENTER")
    checkpoint("measurement-save-default-yes",lambda frame:oracle.buttons(frame,0))
    before = decode_save(save)
    capture.key("ENTER")
    measured = wait_measured_save(save,source_bytes,stamp)
    wait_state(capture,"measurement-saved-browse",oracle.closed_browse,report["checkpoints"])
    report["measurement"] = measurement(measured,before,first,expected)
    report["measured_save_sha256"] = hashlib.sha256(save.read_bytes()).hexdigest()
    (capture.artifacts/"natural-stage0-measured.SAV").write_bytes(save.read_bytes())


def run(args):
    root,output = args.root.resolve(),args.artifacts.resolve()
    original = root / "original_game"
    if output == original or original in output.parents:
        raise ValueError("测试产物不能写入original_game")
    output.mkdir(parents=True, exist_ok=False)
    before = hashes(original)
    original_save_stamps = {p.name:p.stat().st_mtime_ns for p in original.glob("*.SAV")}
    report = {"ok":False,"probe_only":False,"capture":"Windows.Graphics.Capture HWND only",
              "scope":"original then SDL natural START -> stage32/31/0, each resource-proven wait -> first browse -> UI Save",
              "source_load_gate_scope":"stage0 source limited to the exact verified START snapshot; tool scope, not a game limitation",
              "excluded":"RNG alignment, audio/timing, full scene pixels and unknown active/raw roster equality",
              "platform":platform.platform(),"original_before":before,"processes":[],"inputs":[],"cases":[]}
    try:
        report["binaries"] = {name:{"path":str(path.resolve()),
                                     "sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name,path in (("game",args.game),("driver",args.driver),("dosbox",args.dosbox))}
        assets = round_assets(original)
        pages = build_pages(original,assets)
        expected = geometry(original)
        report["wait_count_per_case"] = len(pages)
        report["geometry_contract"] = expected
        titles = title_templates(original / "FDOTHER.DAT")
        source_bytes = (original / "FD2.SAV").read_bytes()
        source = decode_save(original / "FD2.SAV")
        menu_counts = title_menu_counts(source)
        load_available = load_menu_availability(source)
        report["pre_start_save"] = {"sha256":hashlib.sha256(source_bytes).hexdigest(),
                                    "active_stage":source[META+2],"loaded_as_continue":False}
        environment = os.environ.copy()
        for name in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER"):
            environment.pop(name,None)
        for kind in ("original","sdl"):
            work = output / kind
            work.mkdir()
            env = environment.copy()
            if kind == "original":
                game_copy = work / "game"
                shutil.copytree(original,game_copy)
                save = game_copy / "FD2.SAV"
                config = work / "dosbox.conf"
                config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
                    f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                command,cwd = [str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
            else:
                save = work / "FD2.SAV"
                save.write_bytes(source_bytes)
                env["FD2SDL_SAVE_PATH"] = str(save)
                command,cwd = [str(args.game.resolve())],root
            stamp = save.stat().st_mtime_ns
            record = {"kind":kind,"status":"failed","checkpoints":[],"pre_start_save_mtime_ns":stamp,
                      "title_menu_count":menu_counts[kind],"load_available":load_available[kind],
                      "load_gate_basis":"native existing file" if kind=="original" else
                          "verified START stage0" if load_available[kind] else "SDL rejects nonstage0 source"}
            report["cases"].append(record)
            with running(command,cwd,env,work/"game.log",args.driver.resolve(),report["processes"]) as process:
                record["pid"] = process.pid
                capture = Capture(args.driver.resolve(),process,work,report["inputs"])
                exercise(capture,assets,pages,titles,save,source_bytes,stamp,expected,record)
                completed_bytes,completed_stamp = save.read_bytes(),save.stat().st_mtime_ns
            require_source_unchanged(save,completed_bytes,completed_stamp)
            record["status"] = "passed"
        report["comparison"] = compare_measurements(report["cases"][0]["measurement"],
                                                     report["cases"][1]["measurement"])
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        for record in report["cases"]:
            work = output / record["kind"]
            record["capture_and_save_hashes"] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                for p in work.iterdir() if p.is_file() and p.suffix.lower() in (".bmp",".png",".sav")}
        report["original_after"] = hashes(original)
        report["original_unchanged"] = before == report["original_after"]
        report["original_save_mtimes_unchanged"] = original_save_stamps == {
            p.name:p.stat().st_mtime_ns for p in original.glob("*.SAV")}
        report["ok"] = report["ok"] and report["original_unchanged"] and report["original_save_mtimes_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root","game","driver","dosbox","artifacts"):
        parser.add_argument("--"+name,type=Path,required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("自然新游戏原版配对要求Windows WGC；未启动窗口")
    try:
        report = run(args)
        print(json.dumps(report,ensure_ascii=False,indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"自然新游戏配对无法启动：{error}。检查参数并使用新产物目录重跑。",file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())


