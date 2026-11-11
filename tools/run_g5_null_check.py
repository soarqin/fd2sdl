#!/usr/bin/env python3
"""G5当前正式App的native-video/null-audio验收；默认只准备，--run才开窗。

复用现owned Job/WGC/display/source guard，原START整档→标题CONTINUE→
实际Round→浏览→空地SYSTEM菜单→Esc→owned正常关闭，不进调试战场。
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze, unchanged, sha
from run_g5_f1_check import modal_series
from run_g4_detail_check import series_checkpoint
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import match_indexed, wait_title
from run_original_comparison import Capture, driver_call, running, title_templates
from run_original_end_turn import META, RoundOracle, empty_focus_navigation, round_assets, validate_initial
from run_original_load import require_source_unchanged
from run_original_reward import load_round_state, load_round_template

START_SHA='2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310'
INVALID_AUDIO_DRIVER='fd2-g5-deliberately-unavailable-audio'


def write(path,value):
    Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def file_fact(path):
    path=Path(path).resolve()
    if not path.exists():return None
    before=path.stat();digest=sha(path);after=path.stat()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):
        raise ValueError('读取期间文件变化：'+str(path))
    return {'path':str(path),'sha256':digest,'size':after.st_size,'mtime_ns':after.st_mtime_ns}


def project_save_guards(root):
    result=subprocess.run(['git','-C',str(root),'rev-parse','--path-format=absolute','--git-common-dir'],
        text=True,encoding='utf-8',capture_output=True,check=True)
    common=Path(result.stdout.strip()).resolve()
    return {str(path.resolve()):file_fact(path) for path in {root/'FD2.SAV',common.parent/'FD2.SAV'}}


def saves_unchanged(guards):
    return all(file_fact(path)==before for path,before in guards.items())


def guard_state(inputs,original,before,root,sources,users,save,saved,stamp):
    """finally中的来源失败留事实，不以文件异常遮盖更早的业务／清理错误。"""
    result={};errors=[]
    checks=(('sources_unchanged',lambda:unchanged(inputs,original,before,root,sources)),
        ('user_Save_unchanged',lambda:saves_unchanged(users)),
        ('whole_22987B_Save_and_mtime_unchanged',lambda:len(saved)==22987 and
            save.read_bytes()==saved and save.stat().st_mtime_ns==stamp))
    for name,check in checks:
        try:result[name]=check()
        except Exception as error:
            result[name]=False;errors.append({'guard':name,'type':type(error).__name__,'message':str(error)})
    result['ok']=all(result.values());result['errors']=errors
    return result


def prepare(original):
    fixture=original/'START.SAV'
    if sha(fixture)!=START_SHA:raise ValueError('原START.SAV身份不同，未准备替代档')
    source,focus=validate_initial(fixture,0)
    assets=round_assets(original);titles=title_templates(original/'FDOTHER.DAT')
    steps=empty_focus_navigation(source)
    if not steps or len(steps)>4:raise ValueError('原START没有预期的有界空地导航')
    final=steps[-1][1]
    # 菜单四资源必须完整位于viewport；不靠屏幕外像素补判据。
    if not 28<=final[0]<=268 or not 24<=final[1]<=152:raise ValueError('SYSTEM菜单位置越界')
    return {'source':source,'encoded':fixture.read_bytes(),'assets':assets,'titles':titles,
        'focus':focus,'steps':steps,'round':load_round_template(original,assets['palette'],source[META]),
        'round_exit':load_round_template(original,assets['palette'],source[META],exiting=True),
        'manifest':{'fixture':file_fact(fixture),'whole_original_START_without_patches':True,
            'turn':source[META],'stage':source[META+2],'initial_focus_screen':list(focus),
            'empty_focus_navigation':[{'key':key,'position':list(at)} for key,at in steps],
            'native_video':True,'invalid_audio_driver':INVALID_AUDIO_DRIVER,
            'debug_field_entry':False,'device_output_validated':False,'mixer_changed':False}}


def browse_state(frame,prepared,focus,*,initial=False):
    assets=prepared['assets'];oracle=RoundOracle(assets,focus)
    # Load关闭phase仍可能露cursor；每个实际top边沿消失后再接受浏览正证。
    for y in (112,125,138,151,164,177):
        if match_indexed(frame,assets['dialog_top'],(5,y))['ok']:return None
    found=oracle.initial_browse(frame) if initial else oracle.closed_browse(frame)
    return {**found,'all_Load_dialog_phase_edges_absent':True} if found else None


def native_environment(inherited,save):
    removed=[key for key in inherited if key.upper() in (
        'SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_SAVE_PATH','FD2SDL_FIELD_RNG_SEED','FD2SDL_TEST_AUDIO_QUIET')
        or key.upper().startswith('FD2SDL_APP_PCM_CAPTURE_')]
    environment={key:value for key,value in inherited.items() if key not in removed}
    environment.update(SDL_AUDIODRIVER=INVALID_AUDIO_DRIVER,FD2SDL_SAVE_PATH=str(save.resolve()),
        FD2SDL_TEST_AUDIO_QUIET='1')
    return environment,removed


def validate_log(text):
    lines=text.splitlines()
    pattern=r'audio: null backend, 48000 Hz, FDOTHER\[31\]=(\d+), \[77\]=(\d+), \[78\]=(\d+), \[80\]=(\d+), \[95\]=(\d+) samples'
    null=[re.fullmatch(pattern,line) for line in lines if line.startswith('audio:')]
    if len(null)!=1 or null[0] is None or any(int(value)<=0 for value in null[0].groups()):
        raise AssertionError('正式audio日志未证明唯一null backend及已加载PCM banks')
    play=[line for line in lines if line.startswith('field play:')]
    if len(play)!=1 or not play[0].startswith('field play: stage=0, map=24x24, units=12,') or \
            ', turn=1, phase=2, handoff=no' not in play[0] or ', once' in play[0]:
        raise AssertionError('正式CONTINUE战场入口/回合日志不符')
    expected='music request: stage=0 track=19 reason=continue available=1 result=0 current=19 loop=0'
    if lines.count(expected)!=1 or lines.count('field input ready: stage=0 turn=1 phase=2')!=1:
        raise AssertionError('null后端CONTINUE音乐请求或输入ready不符')
    if any(line.startswith(('field action:','field end turn:','field march:','field option:',
            'field battle:','field settlement:','field session stopped:')) for line in lines):
        raise AssertionError('null路径意外进入业务提交或错误尾')
    return {'null_backend_line':null[0].group(0),'PCM_bank_sample_counts':list(map(int,null[0].groups())),
        'formal_continue_entry_once':True,'continue_BGM_request_succeeded':True,
        'device_output_validated':False,'queue_and_all_device_faults_proven_by_this_window':False}


def exercise(capture,prepared,save,record):
    saved,stamp=save.read_bytes(),save.stat().st_mtime_ns
    def guard():require_source_unchanged(save,saved,stamp)
    wait_title(capture,prepared['titles'][3][0]);guard()
    capture.key('UP');capture.wait(prepared['titles'][3][2]);guard()
    focus=prepared['focus'];turn=prepared['source'][META]
    with modal_series(capture,record,'formal-native-null') as (scope,points):
        capture.key('ENTER')
        series_checkpoint(capture,scope,'continued-actual-round',
            lambda frame:load_round_state(frame,prepared['round'],turn,prepared['round_exit']),points)
        guard()
        series_checkpoint(capture,scope,'continued-actual-browse',
            lambda frame:browse_state(frame,prepared,focus,initial=True),points)
        guard()
        for index,(key,at) in enumerate(prepared['steps']):
            capture.key(key);focus=at
            series_checkpoint(capture,scope,'empty-focus-'+str(index),
                lambda frame:browse_state(frame,prepared,focus),points);guard()
        menu=RoundOracle(prepared['assets'],focus)
        capture.key('ENTER')
        series_checkpoint(capture,scope,'formal-SYSTEM-menu',lambda frame:menu.menu(frame,0),points);guard()
        capture.key('ESC')
        series_checkpoint(capture,scope,'cancelled-actual-browse',
            lambda frame:browse_state(frame,prepared,focus),points);guard()
    driver_call(capture.driver,'close',capture.process)
    code=capture.process.wait(timeout=8)
    if code!=0:raise AssertionError('null正式窗口owned正常关闭退出码非0')
    guard();record['host_close']={'pid':capture.process.pid,'exit_code':code,'normal_close_before_cleanup':True}


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output.exists() or output==original or original in output.parents:
        raise ValueError('null验收需要原证据外的新产物目录')
    prepared=prepare(original);users=project_save_guards(root)
    output.mkdir(parents=True);write(output/'fixture-manifest.json',prepared['manifest'])
    (output/'fixture.SAV').write_bytes(prepared['encoded'])
    report={'ok':False,'status':'prepared_only','manifest':prepared['manifest'],
        'scope':'current shipping App native title CONTINUE, actual Round/browse, empty SYSTEM menu/Esc, owned normal close with unavailable audio driver',
        'paired_result':False,'original_runtime_executed':False,'historical_runtime_pass_inherited':False,
        'device_output_validated':False,'display_requirement':{},'processes':[],'checkpoints':[],'inputs':[],
        'user_Save_before':users}
    if not args.run:
        write(output/'comparison.json',report);return report
    owned_job_required()
    sdl=args.sdl.resolve();driver=args.driver.resolve();fixture=original/'START.SAV'
    binaries=[sdl,driver,fixture]+[Path(path) for path,fact in users.items() if fact is not None]
    if sdl.with_suffix('.pdb').is_file():binaries.append(sdl.with_suffix('.pdb'))
    inputs,before,sources=freeze(root,original,binaries)
    inputs[str((output/'fixture.SAV').resolve())]=sha(output/'fixture.SAV')
    inputs[str((output/'fixture-manifest.json').resolve())]=sha(output/'fixture-manifest.json')
    runtime=output/'runtime';runtime.mkdir();resources=runtime/'original_game'
    shutil.copytree(original,resources)
    copied={}
    for path in sorted(resources.rglob('*')):
        if not path.is_file():continue
        relative=path.relative_to(resources).as_posix();source=original/relative
        if path.read_bytes()!=source.read_bytes():raise AssertionError('null隔离资源副本不同：'+relative)
        copied[relative]=file_fact(path);inputs[str(path.resolve())]=sha(path)
    save=runtime/'FD2.SAV';shutil.copy2(fixture,save)
    saved,stamp=save.read_bytes(),save.stat().st_mtime_ns
    environment,removed=native_environment(os.environ,save);log=output/'game.log'
    report.update(command=[str(sdl)],cwd=str(runtime.resolve()),removed_environment_names=removed,
        native_video=True,dummy_video=False,application_quiet_requested=True,mixer_changed=False,
        SDL_AUDIODRIVER=INVALID_AUDIO_DRIVER,actual_resources=copied,
        actual_Save={'path':str(save.resolve()),'sha256':sha(save),'size':len(saved),'mtime_ns':stamp})
    try:
        with windows_display_requirement(report['display_requirement']):
            try:
                if not unchanged(inputs,original,before,root,sources) or not saves_unchanged(users):
                    raise AssertionError('null窗口前来源或用户档变化')
                with running([str(sdl)],runtime,environment,log,driver,report['processes']) as process:
                    report['pid']=process.pid
                    exercise(Capture(driver,process,output,report['inputs']),prepared,save,report)
                report['audio_log']=validate_log(log.read_text(encoding='utf-8',errors='replace'))
            finally:
                # running已先完成owned清理；来源核验随后发生，最后才释放同线程display。
                pending_error=sys.exc_info()[0] is not None
                guarded=guard_state(inputs,original,before,root,sources,users,save,saved,stamp)
                guarded['owned_process_cleanup_complete']=all(row.get('cleaned') is True for row in report['processes'])
                guarded['owned_process_count']=len(report['processes'])
                report['guards_before_display_release']=guarded
                if (not guarded['ok'] or not guarded['owned_process_cleanup_complete']) and not pending_error:
                    raise AssertionError('null显示需求释放前的owned清理／来源／用户档／整SAV守卫失败')
        report['ok']=True;report['status']='passed'
    except BaseException as error:
        report['status']='failed';report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        guarded=guard_state(inputs,original,before,root,sources,users,save,saved,stamp)
        report['guards_after_display_release']=guarded
        for key in ('sources_unchanged','user_Save_unchanged','whole_22987B_Save_and_mtime_unchanged'):
            report[key]=guarded[key]
        report['frozen_inputs']=inputs;report['readonly_original_identity']=before
        valid=(guarded['ok'] and report.get('guards_before_display_release',{}).get('ok') is True and
            report['display_requirement'].get('released') is True and
            len(report['processes'])==1 and all(row.get('cleaned') is True and not row.get('forced_cleanup') and
                row.get('exit_code')==0 for row in report['processes']))
        report['ok'] &= valid
        if not report['ok']:
            report['status']='failed'
            report.setdefault('error',{'type':'AssertionError','message':'null最终来源／用户档／整SAV／清理／显示恢复守卫失败'})
        write(output/'comparison.json',report)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--run',action='store_true');args=parser.parse_args()
    if args.run and (sys.platform!='win32' or any(getattr(args,key) is None or not getattr(args,key).is_file()
            for key in ('sdl','driver'))):parser.error('--run要求Windows及实际sdl/driver；未启动窗口')
    try:report=run(args)
    except Exception as error:parser.exit(1,'null验收前置失败：'+str(error)+'；检查来源和路径后重试。\n')
    print(json.dumps({key:report.get(key) for key in ('ok','status','native_video')},ensure_ascii=False))
    return 0 if not args.run or report.get('ok') else 1


if __name__=='__main__':raise SystemExit(main())
