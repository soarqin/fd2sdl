#!/usr/bin/env python3
"""S2 stage20真特殊格WORD47与SDL显式source47的连续WGC/只读状态代表。

默认准备并可作真实C无窗口预检；--run才串行启动owned窗口。
复用190ac已有caller合同；新覆盖17aa9/四pan反馈，不宣称完整caller或自然stage20。
"""
from __future__ import annotations
import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from PIL import Image
import fd2_s0_native as s0
import fd2_natural_oracle as natural
from fd2_display_requirement import windows_display_requirement
from fd2_loot_oracle import message_pages,message_match
from field_play_input_test import wait_log
from run_g2_battle_check import freeze,unchanged,sha,series
from run_g4_controls_check import actor_templates
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import wait_state,wait_title
from run_original_comparison import Capture,running,title_templates,normalize
from run_original_load import focused_actor,require_source_unchanged
from run_original_player import exact_tile
from run_s2_result_check import output_guard,write
import s2_result_native_fixture as reader
import s2_ticks_native_fixture as fixture
import fd2_s2_snapshot as s2_snapshot
from fd2_s2_wgc import FreshPlateauWatcher


def prepare_browse_contract(p):
    plain=p['plain'];at=fixture.UNITS
    if (plain[fixture.META+1:fixture.META+3]!=bytes((18,20)) or plain[at:at+2]!=bytes((7,19)) or
        plain[at+5]&0x85 or plain[at+6]!=2 or plain[fixture.META+3:fixture.META+9]!=bytes((3,15,7,19,4,4))):
        raise ValueError('47首次Z要求固定stage20/raw0合格且位于原camera内区，不追加滚动或重试键')
    with fixture.resource_context(p['original'],plain):
        p['actor_template']=actor_templates(p['original'],plain,p['assets'],0)
        natural.actor_foreground_resources(p['original'],plain,p['assets'],0,p['assets']['cursor'])
    p['manifest']['browse_input_contract']={'initial_full_focus_and_pose0':True,'planned_Z_count':1,
        'focus_cycle_reset_canonical_va':'0x10602','raw_actor':0,'world':[7,19],'camera':[3,15],
        'focus_screen':[100,100],'extra_retry_keys':False}


def browse_ready(frame,p,*,after_Z=False):
    hit=(focused_actor(frame,p['assets'],p['actor_template']) if after_Z else
         natural.focus_match(frame,p['original'],p['plain'],p['assets']))
    pose=fixture.pose_match(frame,p['poses'],0)
    if not hit or not pose or hit.get('focus_screen')!=[100,100]:return None
    return {'kind':'source47_closed_browse_raw0','focus':hit,'camera_pose':pose,'after_planned_Z':after_Z}


def check_owner(state,plain,*,original,final):
    if state["stage"]!=20 or state["count"]!=18 or state["cached_result"]!=0:raise AssertionError("47源结果/人数/关卡改变")
    if not original and state.get("event_active")!=0:raise AssertionError("47 SDL source owner尚未完成或开始前意外活动")
    expected=bytearray(plain[fixture.UNITS:fixture.UNITS+18*80])
    if original and final:expected[5]|=0x80
    actual=bytearray.fromhex(state["records_hex"])
    # 原10010重建cache class；其它未知记录字节不因机制夹具省略。
    for raw in range(18):actual[raw*80+2]=expected[raw*80+2]
    if actual!=expected:raise AssertionError("47 active记录超出声明cache/原WAIT acted后缀写集")
    camera,focus=([0,32],[4,36]) if final else ([3,15],[7,19])
    if state["camera"]!=camera or state["focus"]!=focus:raise AssertionError("47 pan/focus源轨迹未达声明锚")
    return {"ok":True,"camera":camera,"focus":focus,"raw0_acted":bool(actual[5]&128),
            "caller_suffix_compared_separately":True,"unknown_active_bytes_compared":True,"cache_byte2_excluded":True,
            "scope":"active80B prefix/camera/focus/latch only; not whole runtime/RNG"}


def observe(capture,p,args,rpc,work,case,label,*,final=False):
    directory=work/label;directory.mkdir();record={"label":label,"pid":capture.process.pid}
    if case["kind"]=="original":
        def decode(memory,snapshot):
            state,mapping=reader.original_owner(memory,p["original"],snapshot["DS_descriptor_before"],stage=20,count=18)
            check_owner(state,p["plain"],original=True,final=final);return state,mapping
        _,snapshot=s2_snapshot.original_snapshot(rpc,directory,"ticks",record,owner_decoder=decode,explicit_selectors=True)
        state,mapping=record.pop("validated_owner");record.update(snapshot=snapshot,mapping=mapping)
    else:
        state,cdb=reader.sdl_owner(args.cdb,capture.process,args.sdl,args.pdb,directory,stage=20,count=18);record["cdb"]=cdb
    record.update(actual=state,check=check_owner(state,p["plain"],original=case["kind"]=="original",final=final))
    case.setdefault("runtime_observations",[]).append(record);write(directory/"receipt.json",record)


def continuous_gate(frames,poses):
    runs=[];previous=None
    for row in frames:
        frame=normalize(Image.open(row["path"]))
        match=next((i for i in range(5) if fixture.pose_match(frame,poses,i)),None)
        if match is None:previous=None;continue
        if previous!=match:runs.append({"pose":match,"frames":[]})
        runs[-1]["frames"].append({k:row[k] for k in ("index","sha256","wgc_timestamp_100ns")})
        previous=match
    corners=[];at=0
    for wanted in (1,2,3,4):
        while at<len(runs):
            run=runs[at];timestamps=[r["wgc_timestamp_100ns"] for r in run["frames"]]
            span=(timestamps[-1]-timestamps[0])/10000 if len(timestamps)>1 else 0
            if run["pose"]==wanted and len(timestamps)>=2 and (wanted==4 or span>=200):break
            at+=1
        if at==len(runs):raise AssertionError("连续WGC缺少有序且停顿足够的47 camera："+str(fixture.POSES[wanted]))
        corners.append({"camera":list(fixture.POSES[wanted]),"frames":run["frames"],"observed_stable_span_ms":span});at+=1
    return {"ok":True,"ordered_corners":corners,"source_goals":[1,8,8,8,8],
        "first_three_8tick_plateaus_minimum_observed_ms":200,"first1tick_exact_native_phase_claimed":False,
        "last8tick_completion":"original acted after return / SDL real source-done log and inactive owner",
        "last_plateau_includes_postflow_observation":True,"absolute_BIOS_or_full_frame_equivalence_claimed":False}


def exercise(capture,p,args,rpc,work,case,save):
    saved,stamp=save.read_bytes(),save.stat().st_mtime_ns;assets=p["assets"]
    def cp(label,predicate,**kw):
        result=wait_state(capture,label,predicate,case["checkpoints"],**kw);require_source_unchanged(save,saved,stamp);return result
    with fixture.resource_context(p["original"],p["plain"]):
        if case["kind"]=="original":
            wait_title(capture,p["titles"][3][0]);capture.key("UP");capture.wait(p["titles"][3][2]);capture.key("ENTER")
            cp("ticks-continued-real-stage20",lambda f:browse_ready(f,p),timeout=90)
            observe(capture,p,args,rpc,work,case,"before-WAIT")
            # 复用原10010重置53AE9=0后的单次环扫；不是selection失败后的补键。
            capture.key('Z');cp('ticks-z-focus-raw0',lambda f:browse_ready(f,p,after_Z=True))
            capture.key("ENTER")
            cp("ticks-zero-step-selection",lambda f:{"kind":"source_selection","summary":a,"range":b}
                if (a:=exact_tile(f,p["summary"]["image"],p["summary"]["position"])) and (b:=natural.range_match(f,p["range_probes"])) else None)
            capture.key("ENTER");cp("ticks-command-item",lambda f:natural.command_match(f,p["original"],p["plain"],assets,0,2))
            capture.key("DOWN");cp("ticks-command-WAIT",lambda f:natural.command_match(f,p["original"],p["plain"],assets,0,3))
            capture.key("ENTER");cp("ticks-real-cell421-YES",lambda f:message_match(f,p["choice"],assets))
            observe(capture,p,args,rpc,work,case,"190ac-before-YES-source-prefix")
            case["trigger"]="real stage20 zero-step WAIT ->190ac action2 mode2 WORD47"
        else:
            wait_log(capture.process,work/"game.log","S2 result source ready:",timeout=60)
            cp("ticks-source-only-ready",lambda f:fixture.pose_match(f,p["poses"],0),timeout=60)
            observe(capture,p,args,rpc,work,case,"before-explicit-source47")
            case["trigger"]="nonshipping explicit common.callback.047, excludes190ac/WAIT caller"
        def match_row(row):
            with Image.open(row['path']) as image:frame=normalize(image)
            return next((i for i in range(5) if fixture.pose_match(frame,p['poses'],i)),None)
        original=case['kind']=='original'
        case['backend_observation']={'backend':case['kind'],
            'observation_mode':'primary16_with_bounded_fresh' if original else 'primary16_only',
            'source_basis':'FD2.EXE bios_tick_delay @0x17aa9 waits without present' if original else
                'src/field_common_event_present.h common_event_delay_ticks calls fd2_vga_present_timed; field_play.c frame_ms=16',
            'final_gate':'same continuous_gate; no fallback or automatic pass'}
        with series(capture,case,interval_ms=16), (FreshPlateauWatcher(capture,case,match_row) if original else nullcontext(None)) as watcher:
            capture.key("ENTER")
            stable={"first":None,"frames":0}
            def finished(frame):
                found=fixture.pose_match(frame,p["poses"],4)
                if not found:stable.update(first=None,frames=0);return None
                if stable["first"] is None:stable["first"]=time.monotonic()
                stable["frames"]+=1
                return dict(found,stable_for_seconds=time.monotonic()-stable["first"]) if stable["frames"]>=8 and time.monotonic()-stable["first"]>=0.8 else None
            cp("ticks-final-camera-stable",finished,timeout=120)
            if case["kind"]=="sdl":wait_log(capture.process,work/"game.log","S2 result source done: result=0",timeout=30)
            observe(capture,p,args,rpc,work,case,"source47-finished-with-caller-suffix-separated",final=True)
        primary=case['continuous_wgc']['frames']
        case["continuous_gate"]=continuous_gate(watcher.merge(primary) if watcher is not None else primary,p["poses"])
    require_source_unchanged(save,saved,stamp);case["controlled_save_unchanged"]=True


def run(args):
    root=args.root.resolve();original=(args.original or root/"original_game").resolve(strict=True);output=args.artifacts.resolve()
    output_guard(root,original,output);p=fixture.prepare(original);prepare_browse_contract(p)
    p["titles"]=title_templates(original/"FDOTHER.DAT");p["choice"]=message_pages(original,p["assets"],[{"fragment":421,"open":True,"choice":True}])[0]
    output.mkdir(parents=True);controlled=output/"controlled.SAV";controlled.write_bytes(p["encoded"]);write(output/"fixture.json",p["manifest"])
    report={"schema":"fd2-s2-ticks-native-v1","scenario":fixture.SCENARIO,"ok":False,"status":"prepared",
        "manifest":p["manifest"],"cases":[],"display_requirement":{},"original_runtime_executed":False,"SDL_runtime_executed":False,
        "new_gameplay_enabled":False,"scope":"true stage20/190ac WORD47 and explicit SDL source47:17aa9+four pan; original WAIT suffix separate",
        "excluded":["natural stage20 binding","full caller equality","BIOS absolute timing/phase","complete RNG/audio/whole hot owner","Linux/macOS"]}
    if args.sdl:
        checked=subprocess.run([str(args.sdl.resolve()),"--check",str(controlled),"--flow","common.callback.047"],cwd=root,text=True,capture_output=True,check=True)
        report["SDL_headless_resource_check"]={"exe_sha256":sha(args.sdl),"stdout":checked.stdout,"stderr":checked.stderr,"window_started":False}
    if not args.run:write(output/"comparison.json",report);return report
    owned_job_required();binaries=[args.sdl.resolve(),args.pdb.resolve(),args.driver.resolve(),args.dosbox.resolve(),args.cdb.resolve(),
        args.cdb.resolve().parent/"dbghelp.dll",root/"src/tests/s2_result_native_fixture.c",root/"src/tests/s2_result_native_game_fixture.c"]
    user_save=original.parent/"FD2.SAV"
    if user_save.is_file():binaries.append(user_save)
    inputs,before,sources=freeze(root,original,binaries);report.update(input_sha256=inputs,product_source_paths=sources)
    try:
        with windows_display_requirement(report["display_requirement"]):
            for kind in ("original","sdl"):
                work=output/kind;work.mkdir();case={"kind":kind,"ok":False,"inputs":[],"checkpoints":[],"processes":[],"rpc":{}};report["cases"].append(case)
                env=os.environ.copy()
                for key in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER","FD2SDL_FIELD_RNG_SEED"):env.pop(key,None)
                with (s2_snapshot.OwnedRPC(case["rpc"]) if kind=="original" else nullcontext()) as rpc:
                    if kind=="original":
                        game=work/"game";shutil.copytree(original,game);save=game/"FD2.SAV";config=work/"dosbox.conf"
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'mcp_server={rpc.port}\n'
                            '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                            '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                        command,cwd=[str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
                        for path in game.rglob("*"):
                            if path.is_file() and path.resolve() not in {save.resolve(),(game/"FD2.TMP").resolve()}:inputs[str(path.resolve())]=sha(path)
                        inputs[str(config)]=sha(config)
                    else:save=work/"controlled.SAV";command,cwd=[str(args.sdl.resolve()),"--save",str(save),"--flow","common.callback.047"],root
                    save.write_bytes(p["encoded"])
                    try:
                        if not unchanged(inputs,original,before,root,sources):raise ValueError("首窗口前来源改变")
                        with running(command,cwd,env,work/"game.log",args.driver.resolve(),case["processes"]) as process:
                            if rpc:rpc.bind(process)
                            capture=Capture(args.driver.resolve(),process,work,case["inputs"]);exercise(capture,p,args,rpc,work,case,save)
                        case["ok"]=len(case["processes"])==1 and all(r["cleaned"] and not r["forced_cleanup"] and r["exit_code"]==0 for r in case["processes"])
                    finally:
                        case["sources_unchanged"]=unchanged(inputs,original,before,root,sources);case["ok"]&=case["sources_unchanged"];write(work/"comparison.json",case)
                    if not case["ok"]:raise AssertionError("S2 ticks机制端点未通过："+kind)
        report.update(ok=all(c["ok"] for c in report["cases"]),status="passed",original_runtime_executed=True,SDL_runtime_executed=True)
    except BaseException as error:report.update(status="failed",error={"type":type(error).__name__,"message":str(error)})
    finally:
        report["sources_unchanged"]=unchanged(inputs,original,before,root,sources)
        report["ok"]&=report["sources_unchanged"] and report["display_requirement"].get("released") is True
        if not report["ok"]:report["status"]="failed"
        write(output/"comparison.json",report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("root","artifacts"):parser.add_argument("--"+name,type=Path,required=True)
    for name in ("original","sdl","pdb","driver","dosbox"):parser.add_argument("--"+name,type=Path)
    parser.add_argument("--cdb",type=Path,default=Path("C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"));parser.add_argument("--run",action="store_true")
    args=parser.parse_args(argv)
    if args.sdl and args.pdb is None:args.pdb=args.sdl.with_suffix(".pdb")
    if args.run and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ("sdl","pdb","driver","dosbox","cdb")):
        parser.error("--run需实际source fixture exe/PDB/driver/DOSBox/CDB")
    report=run(args);print(json.dumps({k:report[k] for k in ("ok","status","scenario")},ensure_ascii=False));return 0 if not args.run or report["ok"] else 1


if __name__=="__main__":raise SystemExit(main())
