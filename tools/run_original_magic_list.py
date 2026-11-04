#!/usr/bin/env python3
"""原版-only MAGIC列表/取消校准；不施法，不开启SDL正式MAGIC。

依据field_player_magic_command_execute @0x1cff0、field_player_command_execute
@0x18d8c（原登记同名）；field_unit_magic_list_draw @0x1ceed（原sub_1CEED）
仅叠名字/费用，field_player_magic_list_select_step @0x1d51d（原sub_1D51D）处理选择。
复用fd2_magic_list_oracle纯资源构造；仅由driver run-job串行运行，owned PID清理。
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
from fd2_magic_list_oracle import prepare as list_templates
from run_original_comparison import Capture,hashes,running,title_templates
from run_original_battle_menu import wait_state,wait_title
from run_original_end_turn import UNITS,STRIDE,RoundOracle,round_assets,validate_initial
from run_original_load import focus_cycle_assets,focused_actor,require_source_unchanged
from run_original_player import PlayerOracle,exact_tile,screen,sheet_frame,upper_command_composite
from run_original_result import selection_probe,selection_state


def prepare(original):
    fixture = original/"START.SAV"
    if hashlib.sha256(fixture.read_bytes()).hexdigest()!="2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310":
        raise ValueError("列表校准仅支持已确认的只读真实START来源；未修改或生成夹具")
    templates,fields = list_templates(original,fixture)
    source,focus = validate_initial(fixture,0)
    assets = round_assets(original)
    actors = focus_cycle_assets(original,source,assets)["actors"]
    actor = source[UNITS+STRIDE:UNITS+STRIDE*2]
    sprites = (original/"FDICON.B24").read_bytes()
    actor_sprites = [sheet_frame(sprites,actor[7]*12+phase) for phase in range(12)]
    for image in actor_sprites:
        image.putpalette(assets["palette"])
    assets["upper_commands"] = {index:[upper_command_composite(assets["commands"][index],sprite)
                                       for sprite in actor_sprites] for index in range(3)}
    probe = selection_probe(original,source,1,assets)
    return source,focus,assets,actors,probe,templates,fields


def list_page(frame,templates,fields):
    # 三块都是明确矩形；exact_tile包含palette0，不产生新的像素排除。
    checks = {}
    for name,(position,image) in templates.items():
        if name=="unselected":
            continue
        found = exact_tile(frame,image,position)
        if found is None:
            return None
        checks[name] = {"position":list(position),"size":list(image.size),**found}
    return {"kind":"native_magic_list","selected":0,"magic_ids":[0],"cost":fields["cost"],
            "current_mp":fields["current_mp"],"max_mp":fields["max_mp"],"rectangles":checks,
            "checked_pixels":sum(check["checked_pixels"] for check in checks.values())}


def command_page(frame,command,position,templates,selected=None,*,after_cancel=False):
    detail = command.command(frame,position,selected)
    if detail is None:
        return None
    indices = [match["frame"] for match in detail["matches"]]
    # 真实raw1的weapon52 range1无敌目标、magic有效、两装备槽可计数、WAIT有效。
    if indices[0]!=2 or indices[1] not in ((4,) if selected==1 else (3,4)) or indices[2:]!=[6,9]:
        return None
    if after_cancel:
        at,image = templates["selected"]
        if exact_tile(frame,image,at) is not None:
            return None
    return {**detail,"capability_gates_compared":True,"list_resource_absent":after_cancel}


def compare_reopened(first,second,templates):
    checks = {}
    for name,(at,image) in templates.items():
        if name=="unselected":
            continue
        x,y = at
        expected = first.crop((x,y,x+image.width,y+image.height))
        found = exact_tile(second,expected,at)
        if found is None:
            raise AssertionError(f"reopened list changed the proven {name} rectangle")
        checks[name] = found
    return {"rectangles":checks,"name_mp_cost_selection_unchanged":True,
            "runtime_acted_pending_and_rng_not_measured":True}


def exercise(capture,source,focus,assets,actors,probe,templates,fields,titles,save,data,stamp,report):
    def checkpoint(name,predicate,**kwargs):
        frame,detail = wait_state(capture,name,predicate,report["checkpoints"],**kwargs)
        require_source_unchanged(save,data,stamp)
        return frame,detail
    wait_title(capture,titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    checkpoint("continued-source-browse",RoundOracle(assets,focus).initial_browse,timeout=45)
    for actor in actors[:2]:
        capture.key("Z")
        _,detail = checkpoint(f"z-raw{actor['raw_actor']}",lambda frame,a=actor:focused_actor(frame,assets,a))
        if tuple(detail["focus_screen"])!=screen(source,tuple(actor["world"])):
            raise AssertionError("focus cycle moved the camera outside the confirmed source geometry")
    capture.key("ENTER")
    checkpoint("raw1-movement-selection",lambda frame:selection_state(frame,assets,actors[1],probe))
    capture.key("ENTER")
    command = PlayerOracle(source,assets,[],{})
    position = screen(source,tuple(actors[1]["world"]))
    checkpoint("raw1-zero-move-command",lambda frame:command_page(frame,command,position,templates))
    capture.key("LEFT")
    checkpoint("raw1-magic-selected",lambda frame:command_page(frame,command,position,templates,1))
    capture.key("ENTER")
    first,_ = checkpoint("magic-list-first",lambda frame:list_page(frame,templates,fields))
    capture.key("ESC")
    checkpoint("list-cancel-back-command",lambda frame:command_page(frame,command,position,templates,1,after_cancel=True))
    capture.key("ENTER")
    second,_ = checkpoint("magic-list-reopened",lambda frame:list_page(frame,templates,fields))
    report["reopened_comparison"] = compare_reopened(first,second,templates)
    report["input_scope"] = {"list_input":["ESC"],"second_list_input":[],"cast_confirmation_sent":False}


def run(args):
    root,output = args.root.resolve(),args.artifacts.resolve()
    original = (root/"original_game").resolve()
    if output==original or original in output.parents:
        raise ValueError("测试产物不能写入original_game")
    output.mkdir(parents=True,exist_ok=False)
    before = hashes(original)
    fixture = original/"START.SAV"
    data,fixture_stamp = fixture.read_bytes(),fixture.stat().st_mtime_ns
    report = {"ok":False,"probe_only":True,"probe_cases_passed":False,
              "capture":"Windows.Graphics.Capture HWND only","scope":"real START raw1 fire0 list -> Esc COMMAND -> reopened list; no cast",
              "excluded":"SDL pairing, cast/target/effect, runtime acted/pending snapshot, UI RNG alignment, animation/timing/audio, healing13",
              "ui_rng_static_evidence":"1d51d calls16c57(0); that wait consumes shared RNG; actual count/alignment not measured",
              "original_before":before,"fixture":{"path":str(fixture),"sha256":hashlib.sha256(data).hexdigest(),"mtime_ns":fixture_stamp},
              "processes":[],"inputs":[],"checkpoints":[]}
    save = None
    save_stamp = None
    try:
        report["binaries"] = {name:{"path":str(path.resolve()),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name,path in (("driver",args.driver),("dosbox",args.dosbox))}
        source,focus,assets,actors,probe,templates,fields = prepare(original)
        report["resource_fields"] = fields
        report["plan"] = {"actors":[{key:actor[key] for key in ("raw_actor","unit_id","world")} for actor in actors[:2]],
                          "selection_probe_world":list(probe["world"]),"selection_probe_screen":list(probe["position"])}
        for name,(at,image) in templates.items():
            image.save(output/("expected-"+name+".png"))
        game_copy = output/"game"
        shutil.copytree(original,game_copy)
        save = game_copy/"FD2.SAV"
        save.write_bytes(data)
        save_stamp = save.stat().st_mtime_ns
        config = output/"dosbox.conf"
        config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
            "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
            f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
        env = os.environ.copy()
        for name in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER"):
            env.pop(name,None)
        with running([str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],output,env,
                     output/"game.log",args.driver.resolve(),report["processes"]) as process:
            capture = Capture(args.driver.resolve(),process,output,report["inputs"])
            exercise(capture,source,focus,assets,actors,probe,templates,fields,title_templates(original/"FDOTHER.DAT"),
                     save,data,save_stamp,report)
        require_source_unchanged(save,data,save_stamp)
        report["probe_cases_passed"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = before==report["original_after"]
        report["fixture_unchanged"] = fixture.read_bytes()==data and fixture.stat().st_mtime_ns==fixture_stamp
        report["copy_save_unchanged_after_cleanup"] = (save is not None and save.exists() and
            save.read_bytes()==data and save.stat().st_mtime_ns==save_stamp)
        report["capture_hashes"] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()
                                  if p.is_file() and p.suffix.lower() in (".png",".bmp")}
        report["probe_cases_passed"] = (report["probe_cases_passed"] and report["original_unchanged"] and
            report["fixture_unchanged"] and report["copy_save_unchanged_after_cleanup"])
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root","driver","dosbox","artifacts"):
        parser.add_argument("--"+name,type=Path,required=True)
    args = parser.parse_args()
    if sys.platform!="win32":
        parser.error("原版法术列表校准要求Windows WGC；未启动窗口")
    try:
        report = run(args)
        print(json.dumps(report,ensure_ascii=False,indent=2))
        return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"法术列表校准无法启动：{error}。检查参数并使用新产物目录重跑。",file=sys.stderr)
        return 1


if __name__=="__main__":
    sys.exit(main())
