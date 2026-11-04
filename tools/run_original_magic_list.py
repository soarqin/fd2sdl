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
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
from fd2_magic_list_oracle import prepare as list_templates
from audit_fdicon_opaque_zero import audit_fdicon
from run_graphics_tests import decode_save
from run_original_comparison import Capture,archive_entry,hashes,running,title_templates
from run_original_battle_menu import wait_state,wait_title
from run_original_end_turn import META,UNITS,STRIDE,RoundOracle,round_assets,validate_initial
from run_original_load import (check_load_measurement,focus_cycle_assets,focused_actor,
                               require_source_unchanged,wait_measured_save)
from run_original_focus import native_focus_reference
from run_original_player import PlayerOracle,exact_tile,screen,sheet_frame,upper_command_composite
from run_original_result import selection_probe,selection_state

START_SHA256 = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"
START_PLAIN_SHA256 = "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975"


def derive_controls(source,encrypted):
    """只教raw1五项以测输入；保留persistent roster，不声称自然学习或施法通过。"""
    if (len(source)!=0x59cb or len(encrypted)!=len(source) or
            hashlib.sha256(source).hexdigest()!=START_PLAIN_SHA256 or
            hashlib.sha256(encrypted).hexdigest()!=START_SHA256):
        raise ValueError("controls仅支持已确认真实START完整来源，拒绝其它或已修改的存档")
    plain = bytearray(source)
    bitmap = UNITS+STRIDE+0x1a
    plain[bitmap:bitmap+2] = b"\x0f\x20"
    plain[-4:] = sum(plain[:-4]).to_bytes(4,"little")
    encoded = bytes(a^b^c for a,b,c in zip(encrypted,source,plain))
    changes = [{"offset":i,"before":a,"after":b,
                "owner":"raw1 active magic bitmap" if i in (bitmap,bitmap+1) else "checksum"}
               for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    if {change["offset"] for change in changes}!={bitmap,bitmap+1,len(plain)-4}:
        raise AssertionError("controls派生改动超出已确认位图/checksum")
    return encoded,bytes(plain),{"synthetic_magic_bitmap":True,"natural_learning_covered":False,
        "magic_ids":[0,1,2,3,13],"changed_plain_bytes":changes,
        "changed_encrypted_bytes":[{"offset":i,"before":a,"after":b}
                                    for i,(a,b) in enumerate(zip(encrypted,encoded)) if a!=b],
        "source_encrypted_sha256":hashlib.sha256(encrypted).hexdigest(),
        "source_plain_sha256":hashlib.sha256(source).hexdigest(),
        "derived_encrypted_sha256":hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256":hashlib.sha256(plain).hexdigest()}


def prepare(original,*,source=None):
    fixture = original/"START.SAV"
    encrypted = fixture.read_bytes()
    if hashlib.sha256(encrypted).hexdigest()!=START_SHA256:
        raise ValueError("列表校准仅支持已确认的只读真实START来源；未修改或生成夹具")
    base,focus = validate_initial(fixture,0)
    if source is None:
        source = base
    else:
        expected = derive_controls(base,encrypted)[1]
        if source!=expected:
            raise ValueError("controls资源仅接受完整exact manifest派生")
        focus = screen(source,tuple(source[META+5:META+7]))
    templates,fields = list_templates(original,fixture,source=source)
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
    if fields["ids"]==[0,1,2,3,13]:
        # 1cff0→14818→115b6：13的filter1、cast4/area0首目标为raw0。
        candidates = [i for i in range(source[META+1])
            if not source[UNITS+i*STRIDE+5]&1 and source[UNITS+i*STRIDE+6]!=0 and
            abs(source[UNITS+i*STRIDE]-actor[0])+abs(source[UNITS+i*STRIDE+1]-actor[1])<=4]
        if candidates!=[0,1,2,3] or abs(probe["world"][0]-actor[0])+abs(probe["world"][1]-actor[1])>4:
            raise ValueError("controls目标候选或静态范围ROI超出已确认START场景")
        reference = native_focus_reference(tuple(source[META+3:META+5]),tuple(actor[:2]),tuple(actors[0]["world"]))
        if reference["camera"]!=list(source[META+3:META+5]):
            raise ValueError("controls目标聚焦需要未校准卷屏")
        # 122dc mode2先画frame1，再127a9画actor（同格上移6px）；不将LUT套actor。
        selection_sheet = archive_entry((original/"FDOTHER.DAT").read_bytes(),1)
        begin,end = struct.unpack_from("<II",selection_sheet,10)
        audit = audit_fdicon(struct.pack("<HHHI",24,24,1,10)+selection_sheet[begin:end],expected_frames=1)
        if audit["opaque_zero_pixels"]:
            raise ValueError("目标frame1含实际opaque0，现合成资源门不能把它当透明")
        target_cursor = sheet_frame(selection_sheet,1)
        target_cursor.putpalette(assets["palette"])
        target_actor = focus_cycle_assets(original,source,dict(assets,cursor=target_cursor))["actors"][0]
        assets["magic_target"] = {"actor":target_actor,"range":probe,"screen":reference["screen"]}
        fields["target_contract"] = {"spell":13,"cast":4,"area":0,"filter":1,
            "raw_candidates":candidates,"initial_raw_actor":0,"initial_focus_world":actors[0]["world"],
            "initial_focus_screen":reference["screen"],"selection_frame":1,
            "range_world":list(probe["world"]),"range_screen":list(probe["position"]),
            "selection_opaque_zero_pixels":audit["opaque_zero_pixels"]}
    return source,focus,assets,actors,probe,templates,fields


def list_page(frame,templates,fields,selected=0):
    # 三块都是明确矩形；exact_tile包含palette0，不产生新的像素排除。
    checks = {}
    if not 0<=selected<len(fields["ids"]):
        raise ValueError("列表选项超出已确认资源")
    selected_key = "selected" if selected==0 else f"selected_{selected}"
    for name in (selected_key,"actor_name","current_max_mp"):
        position,image = templates[name]
        found = exact_tile(frame,image,position)
        if found is None:
            return None
        checks[name] = {"position":list(position),"size":list(image.size),**found}
    return {"kind":"native_magic_list","selected":selected,"magic_ids":fields["ids"],
            "cost":fields["costs"][selected],
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
    for name in ("selected","actor_name","current_max_mp"):
        at,image = templates[name]
        x,y = at
        expected = first.crop((x,y,x+image.width,y+image.height))
        found = exact_tile(second,expected,at)
        if found is None:
            raise AssertionError(f"reopened list changed the proven {name} rectangle")
        checks[name] = found
    return {"rectangles":checks,"name_mp_cost_selection_unchanged":True,
            "runtime_acted_pending_and_rng_not_measured":True}


def target_state(frame,assets,templates,fields):
    """只识别13首次目标，不发送目标确认；115b6/122dc，原登记同名。"""
    target = assets["magic_target"]
    focused = focused_actor(frame,assets,target["actor"])
    if not focused or focused["focus_screen"]!=target["screen"] or list_page(frame,templates,fields):
        return None
    for phase,image in target["range"]["ranges"]:
        found = exact_tile(frame,image,target["range"]["position"])
        if found:
            return {"kind":"magic13_target_selection","selection_mode":2,"selection_frame":1,
                    "raw_actor":0,"focus":focused,"range_lut":phase,
                    "range_world":list(target["range"]["world"]),"range_roi":list(target["range"]["position"])+[24,24],
                    "range":found,"target_confirmation_sent":False}
    return None


def cancelled_browse(frame,assets,actor,probe):
    focused = focused_actor(frame,assets,actor)
    base = exact_tile(frame,probe["base"],probe["position"])
    if focused and tuple(focused["focus_screen"])==probe["actor_position"] and base:
        return {"kind":"cancelled_browse","actor":focused,"untranslated_terrain":base}
    return None


def exercise(capture,source,focus,assets,actors,probe,templates,fields,titles,save,data,stamp,report,*,scenario="single"):
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
    if scenario=="controls":
        exercise_controls(capture,source,assets,actors,probe,templates,fields,save,data,stamp,report,first,checkpoint)
        return
    capture.key("ESC")
    checkpoint("list-cancel-back-command",lambda frame:command_page(frame,command,position,templates,1,after_cancel=True))
    capture.key("ENTER")
    second,_ = checkpoint("magic-list-reopened",lambda frame:list_page(frame,templates,fields))
    report["reopened_comparison"] = compare_reopened(first,second,templates)
    report["input_scope"] = {"list_input":["ESC"],"second_list_input":[],"cast_confirmation_sent":False}


def exercise_controls(capture,source,assets,actors,probe,templates,fields,save,data,stamp,report,first,checkpoint):
    # 1d51d的上下回绕、左右±4与不足MP返回0；每个单键后都核对完整三块资源。
    navigation = (("up-wrap","UP",4),("right-edge","RIGHT",4),
        ("left-column","LEFT",0),("left-edge","LEFT",0),("down-first","DOWN",1),
        ("down-expensive","DOWN",2),("insufficient-enter","ENTER",2),
        ("insufficient-space","SPACE",2),("right-partial-edge","RIGHT",2),
        ("down-fourth","DOWN",3),("down-fifth","DOWN",4),("down-wrap","DOWN",0),
        ("right-column","RIGHT",4))
    report["controls_navigation"] = []
    for name,key,selected in navigation:
        capture.key(key)
        _,detail = checkpoint("controls-"+name,lambda frame,s=selected:list_page(frame,templates,fields,s))
        report["controls_navigation"].append({"key":key,**detail})
    capture.key("ENTER")  # 列表确认ID13，只进入target；不确认施法中心。
    checkpoint("controls-magic13-target",lambda frame:target_state(frame,assets,templates,fields))
    capture.key("ESC")
    reopened,_ = checkpoint("controls-target-cancel-list0",lambda frame:list_page(frame,templates,fields))
    report["reopened_comparison"] = compare_reopened(first,reopened,templates)
    capture.key("ESC")
    position = screen(source,tuple(actors[1]["world"]))
    command = PlayerOracle(source,assets,[],{})
    checkpoint("controls-list-cancel-command-first-enabled",lambda frame:command_page(
        frame,command,position,templates,1,after_cancel=True))
    capture.key("ESC")
    checkpoint("controls-command-cancel-selection",lambda frame:selection_state(frame,assets,actors[1],probe))
    capture.key("ESC")
    checkpoint("controls-selection-cancel-browse",lambda frame:cancelled_browse(frame,assets,actors[1],probe))
    report["input_scope"] = {"list_navigation_and_low_mp":True,"list_confirmation_spell":13,
        "target_input":["ESC"],"target_confirmation_sent":False,"cast_confirmation_sent":False,
        "runtime_pending_and_rng_not_measured":True}
    # 原版19df7：取消后仍无acted，才可用Save量测；保持既有严格Load测量写集。
    save_cell = actors[1]["world"][0]-1,actors[1]["world"][1]
    occupied = {tuple(source[UNITS+i*STRIDE:UNITS+i*STRIDE+2]) for i in range(source[META+1])
                if not source[UNITS+i*STRIDE+5]&1}
    if save_cell in occupied or not (2<=save_cell[0]-source[META+3]<=10 and
                                    2<=save_cell[1]-source[META+4]<=5):
        raise ValueError("controls没有已确认不卷屏的左邻Save空格")
    capture.key("LEFT")
    oracle = RoundOracle(assets,screen(source,save_cell))
    checkpoint("controls-measurement-left-empty",oracle.closed_browse)
    capture.key("ENTER")
    checkpoint("controls-measurement-parent",lambda frame:oracle.menu(frame,0))
    capture.key("UP")
    checkpoint("controls-measurement-parent-upper",lambda frame:oracle.menu(frame,0))
    capture.key("ENTER")
    checkpoint("controls-measurement-secondary",lambda frame:oracle.secondary(frame,0))
    capture.key("LEFT")
    checkpoint("controls-measurement-save-enabled",lambda frame:oracle.secondary(frame,1))
    capture.key("ENTER")
    checkpoint("controls-measurement-save-default-yes",lambda frame:oracle.buttons(frame,0))
    report["measurement_save_requested"] = True
    capture.key("ENTER")
    measured = wait_measured_save(save,data,stamp)
    # 保留真实输出后再严格检查；未知diff会失败并留下文件，不增加allowlist或恢复键。
    measured_bytes = save.read_bytes()
    measured_stamp = save.stat().st_mtime_ns
    (capture.artifacts/"controls-measured.SAV").write_bytes(measured_bytes)
    report["measured_save_sha256"] = hashlib.sha256(measured_bytes).hexdigest()
    report["measurement"] = check_load_measurement(source,measured,[("LEFT",screen(source,save_cell))])
    wait_state(capture,"controls-measurement-saved-browse",oracle.closed_browse,report["checkpoints"])
    require_source_unchanged(save,measured_bytes,measured_stamp)


def run(args):
    root,output = args.root.resolve(),args.artifacts.resolve()
    original = (root/"original_game").resolve()
    if output==original or original in output.parents:
        raise ValueError("测试产物不能写入original_game")
    output.mkdir(parents=True,exist_ok=False)
    before = hashes(original)
    fixture = original/"START.SAV"
    data,fixture_stamp = fixture.read_bytes(),fixture.stat().st_mtime_ns
    scenario = getattr(args,"scenario","single")
    if scenario not in ("single","controls"):
        raise ValueError("未知法术列表校准场景")
    report = {"ok":False,"probe_only":True,"probe_cases_passed":False,
              "scenario":scenario,
              "capture":"Windows.Graphics.Capture HWND only","scope":"real START raw1 fire0 list -> Esc COMMAND -> reopened list; no cast",
              "excluded":"SDL pairing, cast/target/effect, runtime acted/pending snapshot, UI RNG alignment, animation/timing/audio, healing13",
              "ui_rng_static_evidence":"1d51d calls16c57(0); that wait consumes shared RNG; actual count/alignment not measured",
              "original_before":before,"fixture":{"path":str(fixture),"sha256":hashlib.sha256(data).hexdigest(),"mtime_ns":fixture_stamp},
              "processes":[],"inputs":[],"checkpoints":[]}
    save = None
    save_stamp = None
    save_data = data
    completed_bytes,completed_stamp = None,None
    try:
        report["binaries"] = {name:{"path":str(path.resolve()),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name,path in (("driver",args.driver),("dosbox",args.dosbox))}
        if scenario=="controls":
            save_data,derived,manifest = derive_controls(decode_save(fixture),data)
            (output/"controls-start.SAV").write_bytes(save_data)
            (output/"fixture-manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf-8")
            report["derived_fixture"] = manifest
            source,focus,assets,actors,probe,templates,fields = prepare(original,source=derived)
            report["scope"] = "synthetic five-spell START controls: navigation, insufficient MP, target/list/command/selection cancel, explicit UI Save; no cast"
            report["excluded"] = "SDL pairing, spell effects, natural learning, runtime pending/RNG, animation/timing/audio"
        else:
            source,focus,assets,actors,probe,templates,fields = prepare(original)
        report["resource_fields"] = fields
        report["plan"] = {"actors":[{key:actor[key] for key in ("raw_actor","unit_id","world")} for actor in actors[:2]],
                          "selection_probe_world":list(probe["world"]),"selection_probe_screen":list(probe["position"])}
        for name,(at,image) in templates.items():
            image.save(output/("expected-"+name+".png"))
        game_copy = output/"game"
        shutil.copytree(original,game_copy)
        save = game_copy/"FD2.SAV"
        save.write_bytes(save_data)
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
            arguments = (capture,source,focus,assets,actors,probe,templates,fields,title_templates(original/"FDOTHER.DAT"),
                         save,save_data,save_stamp,report)
            if scenario=="single":
                exercise(*arguments)  # 保持默认单项协议与输入不变。
            else:
                exercise(*arguments,scenario=scenario)
                if "measurement" not in report:
                    raise AssertionError("controls未完成严格Save量测")
            completed_bytes,completed_stamp = save.read_bytes(),save.stat().st_mtime_ns
        require_source_unchanged(save,completed_bytes,completed_stamp)
        report["probe_cases_passed"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = before==report["original_after"]
        report["fixture_unchanged"] = fixture.read_bytes()==data and fixture.stat().st_mtime_ns==fixture_stamp
        report["copy_save_unchanged_after_cleanup"] = (save is not None and save.exists() and
            save.read_bytes()==save_data and save.stat().st_mtime_ns==save_stamp)
        report["copy_save_matches_completed_state_after_cleanup"] = (save is not None and save.exists() and
            completed_bytes is not None and save.read_bytes()==completed_bytes and save.stat().st_mtime_ns==completed_stamp)
        report["capture_hashes"] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()
                                  if p.is_file() and p.suffix.lower() in (".png",".bmp")}
        report["probe_cases_passed"] = (report["probe_cases_passed"] and report["original_unchanged"] and
            report["fixture_unchanged"] and report["copy_save_matches_completed_state_after_cleanup"] and
            (scenario=="controls" or report["copy_save_unchanged_after_cleanup"]))
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root","driver","dosbox","artifacts"):
        parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--scenario",choices=("single","controls"),default="single")
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
