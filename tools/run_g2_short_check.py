#!/usr/bin/env python3
"""G2真实CONTINUE→END→AI SHORT弓→正式Save；原版先行，主执行者串行启动。

owned Job/WGC-HWND/显示恢复/原源闭包/本轮进程清理由既有设施负责。
最低门为独立22987B有限结果、面板→FX→合法HP条与bank64声音身份；非全帧/A7。
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import fd2_g2_short_oracle as oracle
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze,unchanged,sha,banner_match,fingerprint
from run_graphics_tests import decode_save
from run_original_battle_animation import AnimationCollectors,owned_job_required
from run_original_battle_menu import wait_title,wait_state
from run_original_comparison import Capture,running,title_templates
from run_original_end_turn import RoundOracle,round_assets,round_two_template,check_round_phase_log
from run_original_load import focused_actor,require_source_unchanged,wait_measured_save
from run_original_magic_list import actor_focus_resources
from run_original_player import screen
from field_play_input_test import wait_log


def prepare(root):
    original=Path(root)/'original_game';encoded,source,manifest=oracle.derive_fixture(original)
    assets=round_assets(original)
    return {'original':original,'encoded':encoded,'source':source,'manifest':manifest,'assets':assets,
        'hero':actor_focus_resources(original,source,assets,0,assets['cursor']),
        'titles':title_templates(original/'FDOTHER.DAT'),'banner':round_two_template(original/'FDOTHER.DAT',assets['palette']),
        'visual':oracle.visual_resources(original,source),'sound':oracle.sound_declaration(original)}


def save_measurement(capture,prepared,save,guard,record,checkpoint):
    view=RoundOracle(prepared['assets'],screen(prepared['source'],oracle.SAVE_WORLD))
    capture.key('LEFT');checkpoint('short-empty-save',view.closed_browse)
    capture.key('ENTER');checkpoint('short-save-parent',lambda f:view.menu(f,0))
    capture.key('ENTER');checkpoint('short-save-secondary',lambda f:view.secondary(f,0))
    capture.key('LEFT');checkpoint('short-save-selected',lambda f:view.secondary(f,1))
    capture.key('ENTER');checkpoint('short-save-yes',lambda f:view.buttons(f,0))
    require_source_unchanged(save,*guard);capture.key('ENTER')
    measured=wait_measured_save(save,*guard);encoded=save.read_bytes();stamp=save.stat().st_mtime_ns
    require_source_unchanged(save,encoded,stamp)
    snapshot=capture.artifacts/'measured.SAV';snapshot.write_bytes(encoded)
    if decode_save(snapshot)!=measured:raise AssertionError('SHORT明文与同一份encoded快照身份不同')
    result=oracle.check_result(prepared['original'],prepared['source'],measured,engine=record['kind'])
    record['completed_save']={'path':str(snapshot),'sha256':sha(snapshot),'mtime_ns':stamp}
    record['state_comparison']=result;guard[:]=[encoded,stamp]
    if not result['ok']:raise AssertionError('SHORT全SAV超出原源有限分支：'+str(result))
    checkpoint('short-saved-browse',view.closed_browse)
    return result['hero_hp']


def exercise(capture,prepared,save,guard,record,args):
    def checkpoint(name,predicate,**options):
        result=wait_state(capture,name,predicate,record['checkpoints'],**options)
        require_source_unchanged(save,*guard);return result
    def hero(frame):
        hit=focused_actor(frame,prepared['assets'],prepared['hero'])
        return hit if hit and hit['focus_screen']==list(screen(prepared['source'],oracle.HERO_WORLD)) else None
    wait_title(capture,prepared['titles'][3][0],timeout=90)
    capture.key('UP');capture.wait(prepared['titles'][3][2]);capture.key('ENTER')
    checkpoint('short-continued-hero',hero,timeout=45)
    view=RoundOracle(prepared['assets'],screen(prepared['source'],oracle.SAVE_WORLD))
    capture.key('LEFT');checkpoint('short-end-empty',view.closed_browse)
    capture.key('ENTER');checkpoint('short-end-parent',lambda f:view.menu(f,0))
    capture.key('DOWN');checkpoint('short-end-selected',lambda f:view.menu(f,3))
    capture.key('ENTER');checkpoint('short-end-yes',lambda f:view.buttons(f,0))
    collectors=AnimationCollectors(capture,args,record)
    try:
        collectors.start()
        collectors.confirm()
        if record['kind']=='original':checkpoint('short-round002',lambda f:banner_match(f,prepared),timeout=90)
        else:
            text=wait_log(capture.process,capture.artifacts/'game.log','field phase: turn=2 phase=2 ',timeout=90)
            check_round_phase_log(text);record['sdl_single_end_phase_order']=True
        checkpoint('short-turn2-hero',hero,timeout=90)
        record['business_sequence_completed']=True
    finally:collectors.stop()
    hp=save_measurement(capture,prepared,save,guard,record,checkpoint)
    record['visual_gate']=oracle.check_visual(record['animation_capture']['frames'],prepared['visual'],hp)
    wav=record['audio_capture']['wav']['path']
    if sha(wav)!=record['audio_capture']['wav']['sha256']:raise AssertionError('SHORT已封口WAV身份不同')
    record['sound_gate']=oracle.check_sound(prepared['original'],prepared['sound'],wav,hp)
    if not record['visual_gate']['ok'] or not record['sound_gate']['ok']:
        raise AssertionError('SHORT面板/FX/HP条或bank64最低声音身份未通过')


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or output.is_relative_to(original) or output.exists():raise ValueError('SHORT需要新的独立产物目录')
    binaries=[] if args.prepare_only else [args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()]
    before=fingerprint(original)
    prepared=prepare(root)
    if fingerprint(original)!=before:raise AssertionError('SHORT只读准备来源变化')
    inputs,frozen_original,sources=freeze(root,original,binaries)
    if frozen_original!=before:raise AssertionError('SHORT准备与冻结之间来源变化')
    output.mkdir(parents=True);fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded'])
    if decode_save(fixture)!=prepared['source']:raise AssertionError('SHORT夹具codec往返失败')
    (output/'fixture-manifest.json').write_text(json.dumps(prepared['manifest'],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (output/'sound-declaration.json').write_text(json.dumps(prepared['sound'],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    inputs[str(fixture)]=sha(fixture)
    report={'ok':False,'status':'ready_not_executed','scenario':oracle.SCENARIO,'cases':[],
        'inputs':inputs,'original_before':before,'source_files':sources,'display_requirement':{},
        'animation_capture_interval_ms':16,'sdl_client_default':[960,600],
        'whole_frame_or_full_audio_or_A7_claimed':False,'shared_rng_aligned':False,
        'visual_declaration':{phase:[{k:v for k,v in rule.items() if k not in ('anchor','source_mask')} for rule in rules]
            for phase,rules in prepared['visual'].items()},
        'visual_gate_scope':'panel then, for hit, full FDICON opaque SOLID64+original FX6 overlay (all13 covered points retained), then/same-frame FX7..11 and full5x71 legal HP columns; miss requires no solid witness; source-only anchors require24 positive pixels; no all-frame equivalence'}
    if args.prepare_only:
        (output/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');return report
    owned_job_required()
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in ('original','sdl'):
                if not unchanged(inputs,original,before,root,sources):raise AssertionError('SHORT冻结来源变化')
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_APP_PCM_CAPTURE_DIR',
                    'FD2SDL_APP_PCM_CAPTURE_MAX_MS','FD2SDL_APP_PCM_CAPTURE_QUIET','FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';(work/'captures').mkdir()
                    config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'captures={(work/"captures").as_posix()}\nshow recorded filename=false\n'
                        '[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';app_mix=work/'app-mix';app_mix.mkdir()
                    env.update(FD2SDL_SAVE_PATH=str(save),FD2SDL_APP_PCM_CAPTURE_DIR=str(app_mix),
                        FD2SDL_APP_PCM_CAPTURE_MAX_MS='45000',FD2SDL_APP_PCM_CAPTURE_QUIET='1')
                    command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[]};report['cases'].append(case)
                try:
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                        collector_args=SimpleNamespace(driver=args.driver,sdl_audio_source='app-mix',animation_capture_interval_ms=16)
                        exercise(capture,prepared,save,guard,case,collector_args)
                    case['ok']=True
                finally:
                    case['guards']={'sources_unchanged':unchanged(inputs,original,before,root,sources),
                        'save_matches_completed':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'cleanup':len(case['processes'])==1 and all(p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in case['processes'])}
                    case['ok'] &= all(case['guards'].values())
                    (work/'comparison.json').write_text(json.dumps(case,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
                if not case['ok']:raise AssertionError('SHORT端点守卫失败')
        report['ok']=len(report['cases'])==2 and all(case['ok'] for case in report['cases'])
        report['status']='passed' if report['ok'] else 'failed'
    except BaseException as error:report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        report['ok'] &= report['final_sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        (output/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--prepare-only',action='store_true');args=parser.parse_args()
    args.sdl_audio_source='app-mix';args.animation_capture_interval_ms=16
    if not args.prepare_only and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('sdl','driver','dosbox')):
        parser.error('SHORT实际运行需存在的SDL/driver/DOSBox')
    report=run(args);print(json.dumps({k:report[k] for k in ('ok','status','scenario')},ensure_ascii=False))
    return 0 if report['ok'] or args.prepare_only else 1


if __name__=='__main__':raise SystemExit(main())
