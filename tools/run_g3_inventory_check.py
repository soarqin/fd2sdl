#!/usr/bin/env python3
"""G3 EQT与不同成员Give的正式页面→酒店Save代表；默认只准备，不启动窗口。

复用既有Capture、来源/owned/WGC与P2正式酒店Save叶，不修改旧prepare/exercise。
期望来自原EXE表/真实80B队伍、1c142/1b750及28f65写序；不证明UI RNG同轨或全RGB。
"""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import fd2_g3_prepare_oracle as oracle
import fd2_g3_prepare_pages as pages
from fd2_item_list_oracle import inventory_templates
from fd2_display_requirement import windows_display_requirement
import run_g3_prepare_check as base
from run_g2_battle_check import freeze,unchanged,sha,fingerprint
from run_graphics_tests import decode_save
from run_original_battle_animation import owned_job_required
from run_original_comparison import Capture,running
from run_original_item_list import inventory_detail

SCENARIOS=('eqt-reselect','eqt-empty','give-different-cancel')
write=base.write_json


def _equip(state,raw,slot,t):
    result=copy.deepcopy(state);unit=oracle.equip(oracle.record(result,raw),slot)
    unit=oracle.recompute(unit,t);roster=bytearray(result['roster'])
    roster[raw*80:(raw+1)*80]=unit;result['roster']=bytes(roster)
    return result


def prepare(root,scenario):
    if scenario not in SCENARIOS:raise ValueError('库存验收只接受登记的EQT/不同成员Give代表')
    # 同P2受控五人原记录；不增加物品/钱，不改原资源或共用oracle。
    original=base.prepare(root,'p2-self-give');before=copy.deepcopy(original['before']);t=original['tables']
    if before['stage']!=1 or before['count']!=5:raise ValueError('G3库存代表原五人来源改变')
    empty_patches=[]
    if scenario=='eqt-empty':
        unit=oracle.record(before,4);initial=unit
        # 用已证remove逐次删首项，保末槽stale ID与其余80B；不是全槽清零。
        while oracle.inventory_count(unit):unit=oracle.remove(unit,0)
        unit=oracle.recompute(unit,t)
        roster=bytearray(before['roster']);roster[4*80:5*80]=unit;before['roster']=bytes(roster)
        plain=oracle.write_slot(original['plain'],before,0)
        original=dict(original,plain=plain,encoded=oracle.crypt(plain))
        empty_patches=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(initial,unit)) if a!=b]
        after=copy.deepcopy(before)
        parameters={'kind':'equipment_empty','raw':4,'inventory_count':0,
            'fixture_derivation':'repeat original remove(slot0), then original recompute',
            'record_patches':empty_patches,'source_cva':['0x28efe','0x1bffe','0x1b9de','0x1b750']}
    elif scenario=='eqt-reselect':
        unit=oracle.record(before,4)
        if unit[7]!=51 or [(unit[10+2*i],unit[11+2*i]) for i in range(4)]!=[(0,33),(0,195),(64,43),(64,162)]:
            raise ValueError('原late raw5的EQT未装备33/已装备43来源不同')
        if oracle.item(t,33)[0] not in t['profiles'][unit[32]*7:unit[32]*7+6] or oracle.item(t,43)[0] not in t['profiles'][unit[32]*7:unit[32]*7+6]:
            raise ValueError('原profile不允许本代表两件装备')
        first=_equip(before,4,0,t);after=_equip(first,4,2,t)
        if first==before or first==after:raise ValueError('EQT两次确认缺少可区别的真实装备写集')
        parameters={'kind':'equipment','raw':4,'slots':[0,2],'source_cva':['0x28efe','0x1bffe','0x1c142','0x1b750']}
    else:
        parameters={'kind':'give','sender':2,'recipient':0,'rank':0}
        after,action=oracle.model_action(before,parameters,t)
        if action['outcome']!='completed' or oracle.inventory_count(oracle.record(before,0))!=7 or oracle.selected_id(oracle.record(before,2),0)!=20:
            raise ValueError('不同Give原sender20/recipient7槽前置不同')
    result=dict(original);result.update(before=before,after=after,steps=[],checks={},source_rois={})
    result['manifest']=copy.deepcopy(original['manifest']);result['manifest'].update(scenario=scenario,parameters=parameters,
        base_scenario='p2-self-give',expected_state_roster_sha256=oracle.digest(after['roster']),
        expected_action={'source':'原1c142→1b750两次重选' if scenario=='eqt-reselect' else '原28f65移除→追加→只重算sender',
            'cancel_commits_business':False,'shared_UI_RNG_or_whole_frame_claimed':False})
    if scenario=='eqt-empty':
        source_plain=decode_save(Path(result['original'])/'START.SAV')
        source_encoded=(Path(result['original'])/'START.SAV').read_bytes()
        previous_reasons={row['offset']:row['reason'] for row in result['manifest']['plaintext_patches']}
        result['manifest'].update(fixture_plain_sha256=oracle.digest(result['plain']),
            fixture_sha256=oracle.digest(result['encoded']),
            plaintext_patches=[{'offset':i,'before':a,'after':b,
                'reason':'空EQT受控成员：按原remove(slot0)删空并重算；其余来源保持' if
                    oracle.SLOT_BASE+4*80 <= i < oracle.SLOT_BASE+5*80 else
                    previous_reasons.get(i,'完整容器checksum')}
                for i,(a,b) in enumerate(zip(source_plain,result['plain'])) if a!=b],
            encrypted_patches=[{'offset':i,'before':a,'after':b,'reason':'原save_xor_crypt编码完整受控明文'}
                for i,(a,b) in enumerate(zip(source_encoded,result['encoded'])) if a!=b],
            expected_action={'source':'原1bffe空库存仍开1b9de等待；确认后count门返回，Esc取消',
                             'business_write_trace':[],'ui_rng_or_timing_claimed':False},
            supplemental_record_patches=empty_patches,
            controlled_fixture=True,natural_arrival_claimed=False)
    assets=result['assets'];steps=result['steps']
    def use(step):
        step=dict(step);steps.append(step)
        if 'name' in step:
            name=step['name'];result['checks'][name]=original['checks'][name]
            if name in original['source_rois']:result['source_rois'][name]=original['source_rois'][name]
    for step in original['steps']:
        use(step)
        if step.get('name')=='service-greeting':break
    def add(name,predicate,key=None,**metadata):
        result['checks'][name]=(predicate,{'timeout':35,'allow_low_color_resource':False})
        steps.append({'name':name,'key':key,**metadata})
    def menu(name,index,fragment,key):
        expected=pages.notice(assets,fragment,selector=1)
        add(name,lambda f,e=expected,i=index:pages.menu_match(f,assets,1,i,e),key,page='service_menu',selected=index,fragment=fragment)
    def members(name,state,selected,key):
        images=pages.roster(assets,t,state,selected)
        result['source_rois'][name]=[(image,(5,112),'complete_member_variant'+str(i)) for i,image in enumerate(images)]
        add(name,lambda f,e=images,s=selected:pages.page_match(f,e,kind='prepare_members',selected=s,source=['0x2810b','0x27079']),key,
            page='members',selected=selected,checked_pixels=26660)
    def message(name,fragment,key):
        expected=pages.notice(assets,fragment,selector=1)
        result['source_rois'][name]=[(expected['text'],(0,0),'text_nonzero_mask')]
        add(name,lambda f,e=expected:pages.notice_match(f,e),key,page='notice',fragment=fragment)
    def items(name,state,key):
        unit=oracle.record(state,2);ids=[unit[11+2*i] for i in range(8) if not unit[10+2*i]&128]
        images=[pages.items(assets,t,ids,0,selling=True,arrow_phase=0)]
        result['source_rois'][name]=[(images[0],(5,112),'complete_item_page')]
        add(name,lambda f,e=images:pages.page_match(f,e,kind='prepare_item_page',selected=0,source=['0x272d0','0x275e6']),key,
            page='items',sender=2,selected=0,checked_pixels=26660)
    def equipment(name,state,selected,key):
        _,templates,fields=inventory_templates(result['original'],oracle.record(state,4))
        # 1bffe已经选中当前slot；上页规则仍17fc0，底页184c0用该slot高亮。
        templates=dict(templates)
        if fields['slots']:templates['unselected']=templates['selected_'+str(selected)]
        result['source_rois'][name]=[(image,xy,label) for label,(xy,image) in templates.items() if not label.startswith('selected_')]
        add(name,lambda f,e=templates,a=fields:inventory_detail(f,e,a),key,page='equipment_detail',raw=4,selected=selected,
            source_cva=['0x28efe','0x1bffe','0x17fc0','0x184c0'])
    if scenario=='eqt-empty':
        menu('equipment-command1',1,501,'RIGHT');menu('equipment-command2',2,501,'RIGHT')
        members('equipment-members',before,0,'ENTER');members('equipment-member2',before,2,'DOWN');members('equipment-member4',before,4,'DOWN')
        equipment('equipment-empty-open',before,0,'ENTER')
        equipment('equipment-empty-still-open',before,0,None)
        equipment('equipment-empty-up',before,0,'UP');equipment('equipment-empty-down',before,0,'DOWN')
        equipment('equipment-empty-down2',before,0,'DOWN')
        members('equipment-empty-confirm-return',after,0,'ENTER')
        members('equipment-empty-member2-again',after,2,'DOWN');members('equipment-empty-member4-again',after,4,'DOWN')
        equipment('equipment-empty-reopen',after,0,'ENTER')
        members('equipment-empty-cancel-return',after,0,'ESC')
        menu('service-returned',2,503,'ESC')
    elif scenario=='eqt-reselect':
        menu('equipment-command1',1,501,'RIGHT');menu('equipment-command2',2,501,'RIGHT')
        members('equipment-members-cancel',before,0,'ENTER');menu('equipment-cancel-return',2,503,'ESC')
        members('equipment-members',before,0,'ENTER');members('equipment-member2',before,2,'DOWN');members('equipment-member4',before,4,'DOWN')
        equipment('equipment-before',before,0,'ENTER');equipment('equipment-after33',first,0,'ENTER')
        equipment('equipment-select1',first,1,'DOWN');equipment('equipment-select2',first,2,'DOWN')
        equipment('equipment-after43',after,2,'ENTER');members('equipment-members-after',after,0,'ESC')
        menu('service-returned',2,503,'ESC')
    else:
        for index in range(1,4):menu('give-command'+str(index),index,501,'RIGHT')
        message('give-start',512,'ENTER')
        for suffix in ('cancel','commit'):
            members('give-senders-'+suffix,before,0,'ENTER');members('give-sender2-'+suffix,before,2,'DOWN')
            items('give-items-'+suffix,before,'ENTER');message('give-recipient-prompt-'+suffix,510,'ENTER')
            members('give-recipients-'+suffix,before,0,'ENTER')
            if suffix=='cancel':message('give-recipient-cancelled',512,'ESC')
        message('give-committed',512,'ENTER');members('give-members-after',after,0,'ENTER')
        menu('service-returned',3,503,'ESC')
    # 共用已经验过的同stage/同人数酒店链；picker只读stage/count，实际Save叶
    # 使用本prepared.after独立比较全部22987B，不能沿旧selfGive模型误签。
    tail=next(i for i,s in enumerate(original['steps']) if s.get('name')=='town-after-service')
    for step in original['steps'][tail:]:use(step)
    result['runtime_protocol']={'ready':True,'hotel_slot':0,'source_fixture':'原P2五人完整记录/同源完整22987B',
        'business_scope':parameters,'actual_cancels':'空EQT确认返回再重开Esc' if scenario=='eqt-empty' else 'EQT成员Esc重入' if scenario=='eqt-reselect' else '收件人Esc后完整重选并转交',
        'state_gate':'正式酒店Save；完整roster/钱/未知字节/其它slot/active与源模型比较',
        'not_claimed':['UI等待RNG同轨','全动作RGB','新音频录音/音色/曲目身份','自然到达该受控夹具']}
    return result


def run(args):
    retention_only=getattr(args,'picker_retention_only',False)
    if retention_only and args.scenario!='eqt-empty':raise ValueError('画像反馈只接受eqt-empty')
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or output.is_relative_to(original) or output.exists():raise ValueError('G3库存验收需新的隔离产物目录')
    before=fingerprint(original);prepared=prepare(root,args.scenario)
    if retention_only:
        import fd2_f4_picker_retention as retention
        prepared=retention.narrow(prepared)
    if fingerprint(original)!=before:raise ValueError('G3库存准备期间原源变化')
    binaries=[p.resolve() for p in (args.driver,args.dosbox,args.sdl) if p is not None]
    inputs,frozen,sources=freeze(root,original,binaries)
    if frozen!=before:raise ValueError('G3库存冻结来源变化')
    output.mkdir(parents=True);fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded'])
    if decode_save(fixture)!=prepared['plain']:raise ValueError('G3库存完整22987B夹具往返不同')
    expected=output/'expected-model-not-measured.SAV';expected.write_bytes(oracle.crypt(oracle.write_slot(prepared['plain'],prepared['after'],0)))
    write(output/'fixture-manifest.json',prepared['manifest']);write(output/'expected-state.json',prepared['after'])
    rois=output/'source-rois';rois.mkdir()
    for name,rows in prepared['source_rois'].items():
        for i,(image,xy,role) in enumerate(rows):image.save(rois/(name+'-'+str(i)+'-'+role+'.png'))
    for path in (fixture,expected,output/'fixture-manifest.json',output/'expected-state.json',*rois.glob('*.png')):inputs[str(path)]=sha(path)
    report={'schema':'fd2-g3-inventory-check-v1','ok':False,'status':'ready_not_executed','scenario':args.scenario,
        'inputs':inputs,'original_before':before,'source_files':sources,'cases':[],'display_requirement':{},
        'steps':prepared['steps'],'runtime_protocol':prepared['runtime_protocol'],'new_recording_started':False,
        'full_frame_or_UI_RNG_or_audio_equivalence_claimed':False}
    if not args.run:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        if not report['final_sources_unchanged']:raise ValueError('G3库存prepare-only来源变化')
        write(output/'comparison.json',report);return report
    if retention_only:
        report.update(schema='fd2-f4-picker-retention-check-v1',feedback_only=True,
            full_frame_checkpoint_pixel_equality_claimed=True,Save_executed=False)
        report.pop('full_frame_or_UI_RNG_or_audio_equivalence_claimed',None)
    owned_job_required()
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in ('original','sdl'):
                work=output/kind;work.mkdir();env=os.environ.copy()
                for name in tuple(env):
                    if name in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_TEST_AUDIO_QUIET') or name.startswith('FD2SDL_APP_PCM_CAPTURE'):env.pop(name,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    inputs[str(config)]=sha(config)
                    for name in ('FD2.EXE','FDOTHER.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT','FDMUS.DAT'):inputs[str(game/name)]=sha(game/name)
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'inputs':[],'checkpoints':[],'processes':[]};report['cases'].append(case)
                try:
                    if not unchanged(inputs,original,before,root,sources):raise ValueError('G3库存首窗口前来源变化')
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs']);capture.g3_case_record=case
                        if retention_only:
                            retention.exercise(capture,prepared,save,guard,case,
                                SimpleNamespace(driver=args.driver,sdl_audio_source='app-mix'))
                            if case.get('picker_retention_completed') is not True or case.get('save_executed') is not False:
                                raise AssertionError('画像反馈缺完整phase/后继写集门')
                        else:
                            base.exercise(capture,prepared,save,guard,case,SimpleNamespace(driver=args.driver,sdl_audio_source='app-mix'))
                            if case.get('business_sequence_completed') is not True or not case.get('state_comparison',{}).get('ok') or not case.get('completed_save'):
                                raise AssertionError('G3库存缺少正式完成与完整Save量测')
                    case['ok']=True
                except BaseException as error:
                    case.setdefault('first_failure',{'checkpoint':'picker_feedback' if retention_only else 'inventory',
                        'error':{'type':type(error).__name__,'message':str(error)}})
                    raise
                finally:
                    case['guards']={}
                    for name,check in (
                        ('sources_unchanged',lambda:unchanged(inputs,original,before,root,sources)),
                        ('save_unchanged_without_Save' if retention_only else 'save_matches_measured',
                            lambda:save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1]),
                        ('normal_owned_cleanup',lambda:len(case['processes'])==1 and all(p.get('cleaned') is True and p.get('forced_cleanup') is False and p.get('exit_code')==0 for p in case['processes']))):
                        try:case['guards'][name]=check()
                        except BaseException as error:
                            case['guards'][name]=False
                            case.setdefault('secondary_errors',[]).append({'phase':'late_guard:'+name,
                                'error':{'type':type(error).__name__,'message':str(error)}})
                    case['ok'] &= all(case['guards'].values());write(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('G3库存端点门失败：'+kind)
                for path in work.glob('*'):
                    if path.is_file() and path.suffix in ('.png','.SAV','.jsonl','.bmp'):inputs[str(path)]=sha(path)
        report['ok']=len(report['cases'])==2 and all(c['ok'] for c in report['cases'])
        report['status']=('paired_picker_retention_full_page_checkpoints_passed' if retention_only else
            'paired_inventory_pages_full_save_passed') if report['ok'] else 'failed'
    except BaseException as error:base.record_failure(report,error);report['status']='failed'
    finally:
        try:report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        except BaseException as error:
            report['final_sources_unchanged']=False
            report.setdefault('secondary_errors',[]).append({'phase':'final_sources_guard',
                'error':{'type':type(error).__name__,'message':str(error)}})
        report['ok'] &= report['final_sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        try:write(output/'comparison.json',report)
        except BaseException as error:
            if report.get('error'):
                raise RuntimeError(str(report['error'])+'；同时报告写入失败：'+str(error)) from error
            raise
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('driver','dosbox','sdl'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',choices=SCENARIOS,required=True)
    parser.add_argument('--run',action='store_true',help='显式启动原版后SDL；默认仅prepare-only')
    parser.add_argument('--prepare-only',action='store_false',dest='run')
    parser.set_defaults(run=False);args=parser.parse_args(argv)
    if args.run and any(getattr(args,n) is None or not getattr(args,n).is_file() for n in ('driver','dosbox','sdl')):
        parser.error('G3库存--run需存在的--driver/--dosbox/--sdl')
    report=run(args);print(json.dumps({k:report[k] for k in ('ok','status','scenario')},ensure_ascii=False))
    return 0 if not args.run or report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
