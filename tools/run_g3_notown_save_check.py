#!/usr/bin/env python3
"""NoTown正式Save槽1→直接返回picker→Esc选阵的最短双端验收。

默认仅准备，--execute才启动owned窗口。复用已证21人NoTown夹具与原资源页，
不选择19人、不进战场、不把独立writer模型当实际运行。唯一Save确认后不发660 ACK。
"""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import run_g3_deployment_check as deployment_run
import run_g3_prepare_check as service
import fd2_g3_deployment_pages as deployment
from fd2_transition_oracle import picker_image,picker_match
from fd2_display_requirement import windows_display_requirement
from run_g3_cross_save_check import final_source_guard
from run_g2_battle_check import freeze,unchanged,fingerprint,sha
from run_graphics_tests import decode_save
from run_original_comparison import Capture,running
from run_original_battle_animation import owned_job_required

oracle=deployment_run.oracle
SCENARIO='p5-select21-final-no'
DESTINATION_SLOT=1
STEP_KEYS=(('title-load-selected','DOWN'),('title-slot0','ENTER'),
    ('notown-optional-save','ENTER'),('notown-picker0','ENTER'),
    ('notown-picker1','DOWN'),('notown-save-slot1',None),
    ('notown-saved-picker1',None),('notown-selection-after-save', 'ESC'))


def validate_plan(prepared):
    """本入口只有一个已证有限路径；额外ACK/不完整roster不能静默加入。"""
    state=prepared['before'];steps=prepared['steps']
    if (state['stage']!=27 or state['count']!=21 or len(state['roster'])!=oracle.ROSTER_SIZE or
        prepared['after']!=state or tuple((s['name'],s.get('key')) for s in steps)!=STEP_KEYS or
        [(s['name'],s['save']) for s in steps if 'save' in s]!=[('notown-save-slot1',1)] or
        any(s.get('fragment')==660 for s in steps)):
        raise ValueError('NoTown最短Save合同不同：必须完整21人、槽1单次确认、零额外ACK且不选阵')
    expected=oracle.write_slot(prepared['plain'],state,DESTINATION_SLOT)
    if prepared['expected_plain']!=expected:raise ValueError('NoTown完整22987B期望不等于唯一writer写集')
    return True


def prepare(root):
    source=deployment_run.prepare(root,SCENARIO)
    cut=next(i for i,s in enumerate(source['steps']) if s['name']=='notown-optional-save')+1
    steps=copy.deepcopy(source['steps'][:cut])
    result={key:source[key] for key in ('original','encoded','plain','before','assets','titles')}
    result.update(after=source['before'],manifest=copy.deepcopy(source['manifest']),steps=steps,
        checks={s['name']:(source['checks'][s['name']],{'timeout':45,
            'allow_low_color_resource':s.get('low_color',False)}) for s in steps},source_rois={})
    # 只复用donor夹具，原P5排序动作未执行；报告不得沿用其后态/选择元数据。
    result['manifest'].update(scenario='notown-save-slot1-21-roster',
        parameters={'kind':'notown_manual_save','slot':1,'selected':[]},
        expected_state_roster_sha256=oracle.digest(source['before']['roster']),
        expected_action={'outcome':'slot1_saved_then_selection_unmodified','business_write_trace':[],
            'manual_writer_cva':'0x2968d','original_runtime_executed':False},
        native_protocol='410 YES→picker0→DOWN→picker1→single ENTER Save→direct picker1→ESC→unmodified selection')
    result['manifest']['notown_save_scope']={'source_scenario':SCENARIO,'actual_fixture_not_natural_reach':True,
        'save_slot':1,'roster_count':21,'companion_choices':0,'additional_ACKs':0,
        'source_cva':['0x26152','0x2968d','0x29bcb'],'NoTown_660_and_16c57_used':False}
    def add(name,predicate,key=None,*,low=False,**declaration):
        steps.append({'name':name,'key':key,**declaration})
        result['checks'][name]=(predicate,{'timeout':45,'allow_low_color_resource':low})
    for name,selected,key in (('notown-picker0',0,'ENTER'),('notown-picker1',1,'DOWN')):
        image=picker_image(source['assets'],source['plain'],selected)
        add(name,lambda frame,e=image:picker_match(frame,e),key,low=True,selected=selected)
    steps.append({'name':'notown-save-slot1','key':None,'save':1,'formal_confirmation_count':1})
    result['expected_plain']=oracle.write_slot(source['plain'],source['before'],1)
    image=picker_image(source['assets'],result['expected_plain'],1)
    add('notown-saved-picker1',lambda frame:picker_match(frame,image),None,low=True,selected=1,additional_ACKs=0)
    selection=deployment.selection(source['assets'],source['before'],[],0,19)
    add('notown-selection-after-save',lambda frame:deployment.match(frame,selection),'ESC',
        cursor=0,chosen=[],remaining=19,label_and_grid_pixels=38086)
    result['source_rois']['selection']=selection
    validate_plan(result)
    return result


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve(strict=True);output=args.artifacts.resolve()
    if output.exists() or output==original or output.is_relative_to(original):raise ValueError('NoTown Save需要新的隔离产物目录')
    before=fingerprint(original);prepared=prepare(root)
    if fingerprint(original)!=before:raise ValueError('NoTown只读准备期间来源变化')
    inputs,frozen,sources=freeze(root,original,[p.resolve() for p in (args.sdl,args.driver,args.dosbox) if p is not None])
    if frozen!=before:raise ValueError('NoTown冻结来源变化')
    output.mkdir(parents=True);fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded'])
    if decode_save(fixture)!=prepared['plain']:raise ValueError('NoTown夹具codec往返不同')
    expected=output/'expected-writer-model.SAV';expected.write_bytes(oracle.crypt(prepared['expected_plain']))
    inputs.update({str(fixture):sha(fixture),str(expected):sha(expected)})
    service.write_json(output/'fixture-manifest.json',prepared['manifest'])
    roi=output/'source-rois';roi.mkdir();selection=prepared['source_rois']['selection']
    selection['label'].save(roi/'selection-label.png')
    for phase,image in enumerate(selection['grids']):image.save(roi/('selection-grid-phase'+str(phase)+'.png'))
    report={'ok':False,'status':'ready_not_executed','scenario':'notown-save-slot1-21-roster',
        'steps':prepared['steps'],'inputs':inputs,'original_before':before,'source_files':sources,
        'cases':[],'display_requirement':{},'scope':{'full_container_bytes':oracle.FILE_SIZE,
            'source_slot0_and_active_and_unknown_tails_protected':True,'full_roster_bytes':oracle.ROSTER_SIZE,
            'save_slot':1,'formal_Save_confirmation_count':1,'additional_ACKs':0,'companion_choices':0,
            'runtime_hot_owner_snapshot_claimed':False,'audio_identity_claimed':False,
            'expected_writer_model_is_not_native_execution':True}}
    try:
        if not args.execute:return report
        owned_job_required()
        with windows_display_requirement(report['display_requirement']),final_source_guard(report,inputs,original,before,root,sources):
            for kind in ('original','sdl'):
                validate_plan(prepared)
                if not unchanged(inputs,original,before,root,sources):raise ValueError('NoTown窗口前来源变化')
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_APP_PCM_CAPTURE_DIR',
                    'FD2SDL_APP_PCM_CAPTURE_MAX_MS','FD2SDL_APP_PCM_CAPTURE_QUIET','FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                    inputs[str(config)]=sha(config)
                    for filename in ('FD2.EXE','FDOTHER.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT','FDMUS.DAT'):
                        inputs[str(game/filename)]=sha(game/filename)
                else:
                    save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[]};report['cases'].append(case)
                try:
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs']);capture.g3_case_record=case
                        service.exercise(capture,prepared,save,guard,case,SimpleNamespace(driver=args.driver))
                    case['ok']=True
                finally:
                    case['guards']={'sources_unchanged':unchanged(inputs,original,before,root,sources),
                        'save_matches_completed':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'cleanup':len(case['processes'])==1 and all(p.get('cleaned') is True and p.get('forced_cleanup') is False and p.get('exit_code')==0 for p in case['processes'])}
                    case['ok'] &= all(case['guards'].values());service.write_json(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('NoTown真实Save/页面/清理门失败：'+kind)
        report['ok']=len(report['cases'])==2 and all(c['ok'] for c in report['cases'])
        report['status']='paired_notown_actual_save_slot1_passed' if report['ok'] else 'failed'
    except BaseException as error:
        service.record_failure(report,error);report['status']='failed'
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        if args.execute:
            report['ok'] &= report['final_sources_unchanged'] and report.get('sources_before_display_release') is True and report['display_requirement'].get('released') is True
            if not report['ok']:report['status']='failed'
        elif not report['final_sources_unchanged']:report['status']='failed'
        service.write_json(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('driver','dosbox','sdl'):parser.add_argument('--'+name,type=Path)
    modes=parser.add_mutually_exclusive_group();modes.add_argument('--execute',action='store_true');modes.add_argument('--prepare-only',action='store_true')
    args=parser.parse_args(argv)
    if args.execute and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('driver','dosbox','sdl')):
        parser.error('--execute需现有App/driver/DOSBox；默认仅准备不启动窗口')
    try:report=run(args)
    except (OSError,ValueError,AssertionError,KeyError) as error:
        print(json.dumps({'status':'failed','error':str(error)},ensure_ascii=False));return 1
    print(json.dumps({k:report[k] for k in ('ok','status','scenario')},ensure_ascii=False))
    return 0 if report['ok'] or not args.execute and report['status']=='ready_not_executed' else 1


if __name__=='__main__':raise SystemExit(main())
