#!/usr/bin/env python3
"""S2原版CONTINUE→20bf5与SDL source-only真实Play/Scene机制代表。

默认仅准备受控SAV/资源页与可选无窗口C预检；--run才启动owned串行WGC。
不写原资源/用户档，不改内存，不开放stage29或声称自然关卡/完整音频验收。
"""
from __future__ import annotations

import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import shutil
import subprocess

import fd2_natural_oracle as natural
import fd2_s0_native as s0
import fd2_s2_snapshot as s2_snapshot
from fd2_display_requirement import windows_display_requirement
from fd2_loot_oracle import story_pages
from fd2_opening_oracle import page_match
from field_play_input_test import wait_log
from run_g2_battle_check import freeze, unchanged, sha
from run_g4_controls_check import actor_templates
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import Capture, running, title_templates
from run_original_end_turn import META, UNITS, STRIDE, round_assets
from run_original_load import focused_actor, require_source_unchanged
from run_original_player import exact_tile
from run_original_result import result_templates, result_branch
import s2_result_native_fixture as fixture


def write(path,value):
    Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")


def output_guard(root,original,output):
    root,original,output=(Path(p).resolve() for p in (root,original,output))
    if not output.is_relative_to(root/"build/s2") or output==original or original in output.parents or output.exists():
        raise ValueError("S2产物必须在build/s2内、原资源实际目标外的新目录")


def check_owner(state,plain,result,actors):
    if state["cached_result"]!=result:raise AssertionError("结果锁存不在声明的文本前/后写点")
    if actors and (state["stage"]!=29 or state["count"]!=21 or state.get("actors")!=
            s0.active_fields(plain[UNITS:UNITS+21*STRIDE],21)):
        raise AssertionError("受控active flags/side/ID/HP在无动作路径改变")
    return {"ok":True,"cached_result":result,"actors_checked":actors,
            "scope":"all active flags/side/ID/HP plus latch" if actors else "static53ECC latch only after original story; no freed owner read"}


def observe(capture,p,args,rpc,work,case,label,expected,*,actors=True):
    work=Path(work)/label;work.mkdir();record={"label":label,"pid":capture.process.pid}
    if case["kind"]=="original":
        def decode(memory,snapshot):
            state,mapping=fixture.original_owner(memory,p["original"],snapshot["DS_descriptor_before"],actors=actors)
            check_owner(state,p["plain"],expected,actors);return state,mapping
        _,snapshot=s2_snapshot.original_snapshot(rpc,work,"result",record,owner_decoder=decode,explicit_selectors=True)
        state,mapping=record.pop("validated_owner");record.update(snapshot=snapshot,mapping=mapping)
    else:
        state,record_cdb=fixture.sdl_owner(args.cdb,capture.process,args.sdl,args.pdb,work)
        record["cdb"]=record_cdb
    record.update(actual=state,check=check_owner(state,p["plain"],expected,actors))
    case.setdefault("runtime_observations",[]).append(record);write(work/"receipt.json",record)
    return state


def _exercise(capture,p,args,rpc,work,case,save):
    plain=p["plain"];guard=(save.read_bytes(),save.stat().st_mtime_ns)
    if case["kind"]=="original":
        wait_title(capture,p["titles"][3][0],timeout=90)
        capture.key("UP");capture.wait(p["titles"][3][2]);capture.key("ENTER")
        wait_state(capture,"continue-restored",lambda frame:natural.focus_match(frame,p["original"],plain,p["assets"]),case["checkpoints"],timeout=90)
        observe(capture,p,args,rpc,work,case,"continue-before-query",0)
        capture.key("Z")
        actor=p["actor_template"]
        wait_state(capture,"z-focus-raw0",lambda frame:focused_actor(frame,p["assets"],actor),case["checkpoints"])
        # 原Z焦点在现camera内；只用于已知selection资源，不改磁盘/单位。
        summary,probes=p["summary"],p["range_probes"]
        capture.key("ENTER")
        wait_state(capture,"actor-selection",lambda frame:{"kind":"resource_proven_selection","summary":found,"range":reached}
            if (found:=exact_tile(frame,summary["image"],summary["position"])) and (reached:=natural.range_match(frame,probes)) else None,case["checkpoints"])
        capture.key("ESC");case["trigger"]="real117e7 selection ESC -> result slot29 ->20bf5"
    else:
        wait_log(capture.process,work/"game.log","S2 result source ready:",timeout=60)
        wait_state(capture,"source-only-ready",lambda frame:natural.focus_match(frame,p["original"],plain,p["assets"]),case["checkpoints"],timeout=60)
        observe(capture,p,args,rpc,work,case,"source-before-explicit-entry",0)
        capture.key("ENTER");case["trigger"]="nonshipping explicit common.result.source_020bf5; not central controller/stage binding"
    for index,page in enumerate(p["pages"]):
        wait_state(capture,"story7-wait-"+str(index),lambda frame,page=page:page_match(frame,page,p["assets"]),case["checkpoints"],timeout=90)
        observe(capture,p,args,rpc,work,case,"story7-before-ack-"+str(index),0)
        capture.key("SPACE")
    if case["kind"]=="original":
        wait_state(capture,"original-defeat-after-story",lambda frame:result_branch(frame,"defeat",p["assets"],p["result"]),
                   case["checkpoints"],timeout=60,allow_low_color_resource=True)
        observe(capture,p,args,rpc,work,case,"story-returned-result-written",2,actors=False)
    else:
        wait_log(capture.process,work/"game.log","S2 result source done: result=2",timeout=60)
        observe(capture,p,args,rpc,work,case,"story-returned-result-written",2)
    require_source_unchanged(save,*guard);case["controlled_save_unchanged"]=True


def exercise(capture,p,args,rpc,work,case,save):
    with fixture.resource_matcher_context(p["original"],p["plain"]):
        return _exercise(capture,p,args,rpc,work,case,save)


def run(args):
    root=args.root.resolve();original=(args.original or root/"original_game").resolve(strict=True);output=args.artifacts.resolve()
    output_guard(root,original,output)
    p=fixture.prepare(original);p.update(assets=round_assets(original),root=root)
    # 所有会依赖stage/map的判据在首窗口之前实际构造；adapter退出即还原。
    before_dimensions=dict(natural.STAGE_DIMENSIONS)
    with fixture.resource_matcher_context(original,p["plain"]) as matcher_scope:
        natural._stage_map(original,p["plain"])
        focused=bytearray(p["plain"]);x,y=focused[UNITS:UNITS+2]
        focused[META+5:META+9]=bytes((x,y,x-focused[META+3],y-focused[META+4]))
        p["actor_template"]=actor_templates(original,focused,p["assets"],0)
        natural.actor_foreground_resources(original,focused,p["assets"],0,p["assets"]["cursor"])
        p["summary"]=natural.selection_summary(original,focused,p["assets"],0)
        p["range_probes"]=natural.range_probe(original,focused,p["assets"],0)
        p["pages"]=story_pages(original,p["plain"],p["assets"],7,stage=29)
    p["titles"]=title_templates(original/"FDOTHER.DAT");p["result"]=result_templates(original,p["assets"])
    if len(p["pages"])!=1 or p["pages"][0]["portrait_id"]!=9:raise ValueError("20bf5代表不再是一页合法portrait9 story7")
    output.mkdir(parents=True);controlled=output/"controlled.SAV";controlled.write_bytes(p["encoded"])
    write(output/"fixture.json",p["manifest"])
    report={"schema":"fd2-s2-result-native-v1","scenario":fixture.SCENARIO,"ok":False,"status":"prepared",
            "manifest":p["manifest"],"cases":[],"display_requirement":{},"original_runtime_executed":False,
            "resource_matcher_scope":matcher_scope,"headless_predicates":{"selection_summary":True,
                "range_resources":len(p["range_probes"]),"range_worlds":[p["world"] for p in p["range_probes"]],
                "story_pages":len(p["pages"]),"portrait_id":9,"map_adapter_restored":before_dimensions==natural.STAGE_DIMENSIONS},
            "SDL_runtime_executed":False,"new_gameplay_enabled":False,
            "scope":"original real CONTINUE/controller20bf5 vs nonshipping source-only Game/real Play+Scene; story-entry0 then post-ACK defeat2",
            "excluded":["natural stage29 reachability","shipping stage/controller binding","complete Load/whole hot owner/RNG/audio equality","Linux/macOS"]}
    if args.sdl:
        checked=subprocess.run([str(args.sdl.resolve()),"--check",str(controlled)],cwd=root,text=True,capture_output=True,check=True)
        report["SDL_headless_resource_check"]={"exe_sha256":sha(args.sdl),"stdout":checked.stdout,"stderr":checked.stderr,
                                                 "window_started":False}
    if not args.run:write(output/"comparison.json",report);return report
    owned_job_required()
    binaries=[args.sdl.resolve(),args.pdb.resolve(),args.driver.resolve(),args.dosbox.resolve(),args.cdb.resolve(),
              args.cdb.resolve().parent/"dbghelp.dll",root/"src/tests/s2_result_native_fixture.c",root/"src/tests/s2_result_native_game_fixture.c"]
    user_save=original.parent/"FD2.SAV"
    if user_save.is_file():binaries.append(user_save)
    inputs,before,sources=freeze(root,original,binaries);report.update(input_sha256=inputs,product_source_paths=sources)
    try:
        with windows_display_requirement(report["display_requirement"]):
            for kind in ("original","sdl"):
                work=output/kind;work.mkdir();case={"kind":kind,"ok":False,"inputs":[],"checkpoints":[],"processes":[],"rpc":{}}
                report["cases"].append(case);env=os.environ.copy()
                for name in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER","FD2SDL_FIELD_RNG_SEED"):env.pop(name,None)
                with (s2_snapshot.OwnedRPC(case["rpc"]) if kind=="original" else nullcontext()) as rpc:
                    if kind=="original":
                        game=work/"game";shutil.copytree(original,game);save=game/"FD2.SAV";config=work/"dosbox.conf"
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'mcp_server={rpc.port}\n'
                            '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                            '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                        command,cwd=[str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
                        mutable={save.resolve(),(game/"FD2.TMP").resolve()}
                        for path in game.rglob("*"):
                            if path.is_file() and path.resolve() not in mutable:inputs[str(path.resolve())]=sha(path)
                        inputs[str(config)]=sha(config)
                    else:save=work/"controlled.SAV";command,cwd=[str(args.sdl.resolve()),"--save",str(save)],root
                    save.write_bytes(p["encoded"])
                    try:
                        if not unchanged(inputs,original,before,root,sources):raise ValueError("首窗口前来源改变")
                        with running(command,cwd,env,work/"game.log",args.driver.resolve(),case["processes"]) as process:
                            if rpc:rpc.bind(process)
                            capture=Capture(args.driver.resolve(),process,work,case["inputs"])
                            exercise(capture,p,args,rpc,work,case,save)
                        case["ok"]=len(case["processes"])==1 and all(r["cleaned"] and not r["forced_cleanup"] and r["exit_code"]==0 for r in case["processes"])
                    finally:
                        case["sources_unchanged"]=unchanged(inputs,original,before,root,sources);case["ok"]&=case["sources_unchanged"]
                        write(work/"comparison.json",case)
                    if not case["ok"]:raise AssertionError("S2结果机制端点未通过："+kind)
        report.update(ok=all(c["ok"] for c in report["cases"]),status="passed",original_runtime_executed=True,SDL_runtime_executed=True)
    except BaseException as error:
        report.update(status="failed",error={"type":type(error).__name__,"message":str(error)})
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
    parser.add_argument("--cdb",type=Path,default=Path("C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"))
    parser.add_argument("--run",action="store_true");args=parser.parse_args(argv)
    if args.sdl and args.pdb is None:args.pdb=args.sdl.with_suffix(".pdb")
    if args.run and any(getattr(args,name) is None or not getattr(args,name).is_file() for name in ("sdl","pdb","driver","dosbox","cdb")):
        parser.error("原生运行需本轮实际--sdl/--pdb/--driver/--dosbox/--cdb")
    report=run(args);print(json.dumps({k:report[k] for k in ("ok","status","scenario")},ensure_ascii=False))
    return 0 if not args.run or report["ok"] else 1


if __name__=="__main__":raise SystemExit(main())
