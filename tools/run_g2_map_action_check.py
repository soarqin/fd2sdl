#!/usr/bin/env python3
"""G2恢复代表：正式CONTINUE/END、持续WGC/WAV、原资源和完整SAV独立门。

本入口只实现ai-behavior11-recover；SHORT物理面板/武器FX另行验收。
--prepare-only只写已预声明资源/声音/业务合同，不启动窗口。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import fd2_g2_map_action_oracle as oracle
import run_original_battle_animation as battle
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import fingerprint,freeze,unchanged
from run_graphics_tests import decode_save
from run_original_battle_menu import wait_state,wait_title
from run_original_comparison import Capture,running
from run_original_end_turn import RoundOracle
from run_original_load import require_source_unchanged
from run_original_player import screen


def make_checkpoint(capture,save,guard,record):
    def checkpoint(name,predicate,*,source_guard=True,**options):
        result=wait_state(capture,name,predicate,record["checkpoints"],**options)
        if source_guard:require_source_unchanged(save,*guard)
        return result
    return checkpoint


def exercise(capture,prepared,save,guard,record,args):
    source,assets=prepared["source"],prepared["assets"]
    checkpoint=make_checkpoint(capture,save,guard,record)
    wait_title(capture,prepared["titles"][3][0],timeout=90)
    capture.key("UP");capture.wait(prepared["titles"][3][2]);capture.key("ENTER")
    checkpoint("g2-recovery-continued",RoundOracle(assets,screen(source,prepared["initial_focus"])).initial_browse,timeout=45)
    battle.reward.navigate(capture,prepared,source,prepared["initial_focus"],(6,14),checkpoint,"g2-recovery-to-END")
    battle.open_end(capture,prepared,checkpoint)
    # 显式新增持续画面/音频采集，绕开旧BUSINESS_SCENARIOS的“不请求采集”分支。
    with battle.AnimationCollectors(capture,args,record).active() as collectors:
        collectors.confirm()
        battle.wait_phase(capture,prepared,checkpoint,record)
    record["recovery_visual_gate"]=oracle.visual_gate(record["animation_capture"]["frames"],prepared["recovery_resources"])
    wav=record["audio_capture"]["wav"]
    record["recovery_audio_gate"]=oracle.audio_gate(prepared["original"],wav["path"],wav["sha256"])
    if not record["recovery_visual_gate"]["ok"] or not record["recovery_audio_gate"]["ok"]:
        raise AssertionError("恢复实体有序阶段或field31/cue4声音身份尚未通过")
    battle.measure(capture,prepared,save,guard,record,checkpoint)
    snapshot=Path(record["completed_save"]["snapshot"])
    encoded=snapshot.read_bytes()
    if hashlib.sha256(encoded).hexdigest()!=record["completed_save"]["sha256"]:
        raise AssertionError("恢复已测SAV快照身份改变")
    measured=decode_save(SimpleNamespace(read_bytes=lambda:encoded))
    record["recovery_full_save_gate"]=oracle.save_gate(prepared,measured,record["kind"])


def run(args):
    args.animation_capture_interval_ms=16  # 三wait中的单实体阶段约55ms，明确请求而不改共享默认。
    root,output=args.root.resolve(),args.artifacts.resolve()
    original=(root/"original_game").resolve()
    if output==original or output.is_relative_to(original) or output.exists():
        raise ValueError("恢复需要新的独立产物目录，不能写原资源或覆盖旧现场")
    before=fingerprint(original)
    prepared=oracle.prepare(original)
    if fingerprint(original)!=before:
        raise AssertionError("恢复来源在只读准备期间改变")
    output.mkdir(parents=True)
    fixture=output/"fixture.SAV";fixture.write_bytes(prepared["encoded"])
    (output/"fixture-manifest.json").write_text(json.dumps(prepared["manifest"],ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    (output/"predeclared-recovery-plan.json").write_text(json.dumps(prepared["recovery_plan"],ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    report={"ok":False,"status":"ready_not_executed","scenario":oracle.SCENARIO,"cases":[],
        "predeclared_plan":prepared["recovery_plan"],"whole_animation_or_audio_fidelity":False,
        "SHORT_physical_panels_verified":False,"shared_RNG_aligned":False,
        "requested_animation_capture_interval_ms":16}
    if args.prepare_only:
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        return report
    battle.owned_job_required()
    binaries=[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()]
    inputs,frozen,sources=freeze(root,original,binaries)
    if frozen!=before:
        raise AssertionError("恢复来源在准备和冻结之间改变")
    inputs[str(fixture)]=hashlib.sha256(fixture.read_bytes()).hexdigest()
    report.update(status="failed",inputs=inputs,source_files=sources,original_before=before)
    display={};report["display_requirement"]=display
    def guard_sources():
        return unchanged(inputs,original,before,root,sources)
    try:
        with windows_display_requirement(display):
            for kind in ("original","sdl"):
                if not guard_sources():raise AssertionError("恢复冻结来源在窗口启动前改变")
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER","FD2SDL_FIELD_RNG_SEED","FD2SDL_SAVE_PATH",
                    "FD2SDL_APP_PCM_CAPTURE_DIR","FD2SDL_APP_PCM_CAPTURE_MAX_MS","FD2SDL_APP_PCM_CAPTURE_QUIET","FD2SDL_TEST_AUDIO_QUIET"):
                    env.pop(key,None)
                if kind=="original":
                    game=work/"game";shutil.copytree(original,game);save=game/"FD2.SAV";(work/"captures").mkdir()
                    config=work/"dosbox.conf"
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'
                        f'captures={(work/"captures").as_posix()}\nshow recorded filename=false\n'
                        '[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        f'[autoexec]\nmount c "{game}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                    command,cwd=[str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
                else:
                    save=work/"FD2.SAV";mix=work/"app-mix";mix.mkdir()
                    env.update(FD2SDL_SAVE_PATH=str(save),FD2SDL_APP_PCM_CAPTURE_DIR=str(mix),
                        FD2SDL_APP_PCM_CAPTURE_MAX_MS=str(battle.CAPTURE_MAX_MS),FD2SDL_APP_PCM_CAPTURE_QUIET="1")
                    command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared["encoded"]);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                record={"kind":kind,"ok":False,"processes":[],"inputs":[],"checkpoints":[]};report["cases"].append(record)
                try:
                    with running(command,cwd,env,work/"game.log",args.driver.resolve(),record["processes"]) as process:
                        capture=Capture(args.driver.resolve(),process,work,record["inputs"])
                        exercise(capture,prepared,save,guard,record,args)
                    record["ok"]=True
                finally:
                    record["guards"]={"sources_unchanged":guard_sources(),
                        "save_matches_completed":save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        "cleanup":len(record["processes"])==1 and all(p["cleaned"] and not p["forced_cleanup"] and p["exit_code"]==0 for p in record["processes"])}
                    record["ok"] &= all(record["guards"].values())
                    (work/"comparison.json").write_text(json.dumps(record,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
                if not record["ok"]:raise AssertionError("恢复端点守卫或清理未通过")
        report["ok"]=len(report["cases"])==2 and all(c["ok"] for c in report["cases"])
        report["status"]="passed" if report["ok"] else "failed"
    except BaseException as error:
        report["error"]={"type":type(error).__name__,"message":str(error)}
    finally:
        report["final_sources_unchanged"]=guard_sources()
        report["ok"] &= report["final_sources_unchanged"] and display.get("released") is True
        if not report["ok"]:report["status"]="failed"
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("root","artifacts"):parser.add_argument("--"+name,type=Path,required=True)
    for name in ("sdl","driver","dosbox"):parser.add_argument("--"+name,type=Path)
    parser.add_argument("--scenario",choices=(oracle.SCENARIO,),default=oracle.SCENARIO)
    parser.add_argument("--prepare-only",action="store_true")
    args=parser.parse_args();args.sdl_audio_source="app-mix"
    if not args.prepare_only and any(getattr(args,n) is None or not getattr(args,n).is_file() for n in ("sdl","driver","dosbox")):
        parser.error("实际恢复采集需要存在的SDL/driver/DOSBox程序")
    report=run(args);print(json.dumps({k:report[k] for k in ("ok","status","scenario")},ensure_ascii=False))
    return 0 if report["ok"] or args.prepare_only else 1


if __name__=="__main__":raise SystemExit(main())
