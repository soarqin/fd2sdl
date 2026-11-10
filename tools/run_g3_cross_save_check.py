#!/usr/bin/env python3
"""G3真实对端SAV→正式标题LOAD→成员/库存→酒店Save新槽1。

默认只准备；--execute才串行启动owned窗口。输入必须来自已通过实际G3报告的
completed_save，不生成另一份同fixture冒充交叉载入。期望只定义2968d写集，
完整22987B通过仍须接收端真实运行、WGC、实际新槽Save及owner清理收据。
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import struct
from types import SimpleNamespace

import fd2_g3_prepare_oracle as oracle
import fd2_g3_prepare_pages as pages
from fd2_item_list_oracle import inventory_templates
from fd2_transition_oracle import picker_image,picker_match,town_images
from fd2_display_requirement import windows_display_requirement
from run_original_comparison import Capture,running,title_templates,compare
from run_original_transition import town_match
from run_original_item_list import inventory_detail
from run_original_battle_animation import owned_job_required
from run_graphics_tests import decode_save
from run_g2_battle_check import freeze,unchanged,fingerprint,sha
import run_g3_prepare_check as service

SOURCE_SLOT=0
DESTINATION_SLOT=1


@contextmanager
def final_source_guard(report,inputs,original,before,root,sources):
    try:yield
    finally:report['sources_before_display_release']=unchanged(inputs,original,before,root,sources)


def state_from_slot(plain,slot=SOURCE_SLOT):
    oracle.valid_container(plain)
    if type(slot) is not int or not 0<=slot<4:raise ValueError('交叉源槽非法')
    at=oracle.SLOT_BASE+slot*oracle.SLOT_STRIDE;meta=at+oracle.ROSTER_SIZE
    state={'stage':plain[meta],'count':plain[meta+1],'resource':struct.unpack_from('<I',plain,meta+2)[0],
        'options':plain[meta+6:meta+10],'roster':plain[at:at+oracle.ROSTER_SIZE]}
    if state['stage']!=1 or not 1<=state['count']<=6:
        raise ValueError('本最小交叉运行门仅stage1首屏1..6人酒店；未扩展其它准备/战场')
    return state


def evidence(path,original):
    """绑定成功运行收据和真实encoded产物；不要求历史生产者App仍占用同一路径。"""
    path=Path(path).resolve(strict=True);sealed={}
    def seal(file,expected=None):
        file=Path(file).resolve(strict=True);value=sha(file)
        if expected is not None and value!=expected:raise ValueError('交叉来源SHA不同：'+str(file))
        sealed[str(file)]=value;return file
    report=json.loads(seal(path).read_text(encoding='utf-8'))
    if (report.get('ok') is not True or report.get('final_sources_unchanged') is not True or
        report.get('display_requirement',{}).get('released') is not True or report.get('original_before')!=fingerprint(original)):
        raise ValueError('交叉来源必须为已通过、显示释放且同原资源的G3实际聚合报告')
    if report.get('inherited_original'):
        if report.get('inherited_original_final_files_unchanged') is not True:
            raise ValueError('交叉来源继承原版未完成最终封存检查')
        for file,value in report['inherited_original'].get('sealed_files',{}).items():seal(file,value)
    binaries={name:{p:h for p,h in report.get('inputs',{}).items() if Path(p).name.lower()==name}
        for name in ('fd2sdl.exe','fd2_field_play_input_driver.exe','dosbox-x.exe')}
    if any(len(rows)!=1 for rows in binaries.values()):raise ValueError('来源缺唯一App/driver/DOSBox输入身份')
    result={}
    for kind in ('original','sdl'):
        cases=[case for case in report.get('cases',[]) if case.get('kind')==kind]
        if len(cases)!=1:raise ValueError('交叉来源需要唯一原版和SDL实际端点')
        case=cases[0];processes=case.get('processes',[]);measured=case.get('completed_save',{})
        if (case.get('ok') is not True or case.get('business_sequence_completed') is not True or
            case.get('guards')!={'sources_unchanged':True,'save_matches_completed':True,'cleanup':True} or
            case.get('state_comparison',{}).get('ok') is not True or
            case['state_comparison'].get('checked_container_bytes')!=oracle.FILE_SIZE or measured.get('slot')!=SOURCE_SLOT or
            len(processes)!=1 or processes[0].get('cleaned') is not True or processes[0].get('forced_cleanup') is not False or
            processes[0].get('exit_code')!=0):
            raise ValueError('来源端点缺真实业务/全SAV/正常owned退出：'+kind)
        save=seal(measured['path'],measured['sha256']);work=save.parent
        if json.loads(seal(work/'comparison.json').read_text(encoding='utf-8'))!=case:
            raise ValueError('来源case文件与聚合收据不同：'+kind)
        live=seal(work/'game/FD2.SAV' if kind=='original' else work/'FD2.SAV',measured['sha256'])
        if live.stat().st_mtime_ns!=measured.get('mtime_ns'):raise ValueError('来源实际writer文件时间身份不同')
        encoded=save.read_bytes();plain=decode_save(save);state=state_from_slot(plain)
        binary_name='dosbox-x.exe' if kind=='original' else 'fd2sdl.exe'
        producer_binary=dict(binaries[binary_name])
        for row in case.get('checkpoints',[]):
            if row.get('pid')!=processes[0].get('pid'):raise ValueError('来源checkpoint PID与本轮owned端点不同')
            if row.get('kind')=='wgc_transient':
                for key in ('last_valid_frame','last_raw_capture'):
                    if row.get(key):seal(work/row[key])
            else:seal(work/(row['name']+'.png'))
        result[kind]={'encoded':encoded,'plain':plain,'state':state,
            'receipt':{'producer':kind,'source_report':str(path),'source_report_sha256':sealed[str(path)],
                'case_path':str(work/'comparison.json'),'measured_save':str(save),'measured_sha256':sha(save),
                'completed_slot':SOURCE_SLOT,'runtime_pid':processes[0]['pid'],
                'aggregate_binary_identities_from_actual_receipt':binaries,
                'producer_runtime_binary_from_actual_receipt':producer_binary,
                'historical_App_file_not_revalidated_if_rebuilt':True,
                'source_encoded_copied_verbatim':True,'same_fixture_model_not_used_as_cross_source':True}}
    return result,sealed


def prepare(original,source):
    plain=source['plain'];state=source['state'];assets=pages.load(original);tables,_=oracle.tables(original)
    result={'original':original,'encoded':source['encoded'],'plain':plain,'before':state,'after':state,
        'assets':assets,'tables':tables,'titles':title_templates(original/'FDOTHER.DAT'),
        'towns':[town_images(original,assets,state,i) for i in range(5)],'checks':{},'steps':[]}
    def add(name,predicate,key=None,low=False,**detail):
        result['checks'][name]=(predicate,{'timeout':30,'allow_low_color_resource':low})
        result['steps'].append({'name':name,'key':key,**detail})
    def town(name,key):add(name,lambda f:town_match(f,result,0),key,page='town',selected=0)
    def menu(name,selected,fragment,key):
        expected=pages.notice(assets,fragment,selector=0)
        add(name,lambda f:pages.menu_match(f,assets,0,selected,expected),key,page='hotel_menu',selected=selected,fragment=fragment)
    def picker(name,data,selected,key):
        expected=picker_image(assets,data,selected)
        add(name,lambda f:picker_match(f,expected),key,True,page='picker',slot=selected)
    add('title-load-selected',lambda f:{'kind':'title_load','selected':1} if compare(result['titles'][3][1],f)['ok'] else None,'DOWN')
    picker('title-source-slot0',plain,0,'ENTER');town('town-loaded-source','ENTER')
    menu('hotel-greeting',0,585,'ENTER')
    members=pages.roster(assets,tables,state,0,selector=0)
    def member_page(f):return pages.page_match(f,members,kind='cross_loaded_members',selected=0,source=['0x29620','0x27d33'])
    add('loaded-members',member_page,'ENTER',page='members',count=state['count'])
    _,details,fields=inventory_templates(original,oracle.record(state,0))
    add('loaded-first-member-inventory',lambda f:inventory_detail(f,details,fields),'ENTER',page='detail',
        raw=0,unit_id=state['roster'][7],inventory_ids=[row['item_id'] for row in fields['slots']])
    add('loaded-members-after-detail',member_page,'ESC',page='members',count=state['count'])
    menu('hotel-after-members',0,586,'ESC');menu('hotel-save-selected',1,586,'RIGHT')
    picker('hotel-save-slot0',plain,0,'ENTER');picker('hotel-save-slot1',plain,1,'DOWN')
    result['steps'].append({'save':DESTINATION_SLOT,'key':'ENTER'})
    notice=pages.notice(assets,660,selector=0)
    add('hotel-save-notice660',lambda f:pages.notice_match(f,notice),page='notice',fragment=660)
    expected=oracle.write_slot(plain,state,DESTINATION_SLOT)
    picker('hotel-saved-slot1',expected,1,'ENTER');menu('hotel-after-save',1,586,'ESC');town('town-final','ESC')
    result['expected_plain']=expected
    return result


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve(strict=True);output=args.artifacts.resolve()
    if output.exists() or output==original or output.is_relative_to(original):raise ValueError('交叉SAV需要新的隔离产物目录')
    before=fingerprint(original);donors,sealed=evidence(args.evidence,original)
    prepared={kind:prepare(original,data) for kind,data in donors.items()}
    if fingerprint(original)!=before:raise ValueError('交叉prepare期间原资源变化')
    binaries=[p.resolve() for p in (args.sdl,args.driver,args.dosbox) if p is not None]
    inputs,original_before,sources=freeze(root,original,binaries);inputs.update(sealed)
    output.mkdir(parents=True);report={'ok':False,'status':'ready_not_executed','cases':[],
        'source_evidence':str(args.evidence.resolve()),'inputs':inputs,'original_before':original_before,
        'source_files':sources,'display_requirement':{},'directions':[],
        'scope':{'cross_sources_are_actual_measured_files':True,'source_slot':SOURCE_SLOT,'destination_slot':DESTINATION_SLOT,
            'producer_outputs_byte_equal':donors['original']['encoded']==donors['sdl']['encoded'],
            'comparison_bytes':oracle.FILE_SIZE,'expected_writer_model_is_not_native_execution':True,
            'original_slot_and_unknown_tails_and_active_protected':True,'audio_identity_claimed':False}}
    for producer,receiver in (('original','sdl'),('sdl','original')):
        name=producer+'-to-'+receiver;work=output/name;work.mkdir();data=donors[producer]
        fixture=work/'incoming.SAV';fixture.write_bytes(data['encoded'])
        if fixture.read_bytes()!=Path(data['receipt']['measured_save']).read_bytes():raise ValueError('交叉输入不是实际对端encoded原样副本')
        expected=work/'expected-writer-model.SAV';expected.write_bytes(oracle.crypt(prepared[producer]['expected_plain']))
        inputs.update({str(fixture):sha(fixture),str(expected):sha(expected)})
        direction={'direction':name,'producer':producer,'receiver':receiver,'source':data['receipt'],
            'incoming':{'path':str(fixture),'sha256':sha(fixture)},'steps':prepared[producer]['steps'],
            'expected_model':{'path':str(expected),'sha256':sha(expected),'observed_native_output':False}}
        report['directions'].append(direction)
    try:
        if not args.execute:return report
        owned_job_required()
        with windows_display_requirement(report['display_requirement']), final_source_guard(report,inputs,original,original_before,root,sources):
            for direction in report['directions']:
                if not unchanged(inputs,original,original_before,root,sources):raise ValueError('交叉窗口前来源变化')
                kind=direction['receiver'];work=output/direction['direction'];env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_APP_PCM_CAPTURE_DIR',
                    'FD2SDL_APP_PCM_CAPTURE_MAX_MS','FD2SDL_APP_PCM_CAPTURE_QUIET','FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(Path(direction['incoming']['path']).read_bytes());guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'direction':direction['direction'],'ok':False,'processes':[],'inputs':[],'checkpoints':[],
                    'incoming_actual_producer':direction['source']};report['cases'].append(case)
                try:
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs']);capture.g3_case_record=case
                        service.exercise(capture,prepared[direction['producer']],save,guard,case,SimpleNamespace(driver=args.driver))
                    case['ok']=True
                finally:
                    case['guards']={'sources_unchanged':unchanged(inputs,original,original_before,root,sources),
                        'save_matches_completed':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'cleanup':len(case['processes'])==1 and all(p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in case['processes'])}
                    case['ok'] &= all(case['guards'].values());service.write_json(work/'comparison.json',case)
                if not case['ok']:raise ValueError('交叉接收端真实Save/清理守卫未过')
        report['ok']=len(report['cases'])==2 and all(c['ok'] for c in report['cases'])
        report['status']='actual_bidirectional_load_new_slot_save_passed' if report['ok'] else 'failed'
    except BaseException as error:
        service.record_failure(report,error);report['status']='failed'
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,original_before,root,sources)
        if not report['final_sources_unchanged']:
            report['status']='failed';report.setdefault('error',{'message':'交叉SAV最终来源守卫不同'})
        if args.execute:report['ok'] &= (report['final_sources_unchanged'] and report.get('sources_before_display_release') is True and
            report['display_requirement'].get('released') is True)
        if args.execute and not report['ok']:report['status']='failed'
        service.write_json(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts','evidence'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):parser.add_argument('--'+name,type=Path)
    modes=parser.add_mutually_exclusive_group();modes.add_argument('--execute',action='store_true');modes.add_argument('--prepare-only',action='store_true')
    args=parser.parse_args(argv)
    if args.execute and any(getattr(args,key) is None or not getattr(args,key).is_file() for key in ('sdl','driver','dosbox')):
        parser.error('--execute需现有App/driver/DOSBox；默认仅准备不启动窗口')
    try:result=run(args)
    except (OSError,ValueError,AssertionError,KeyError) as error:
        print(json.dumps({'status':'failed','error':str(error)},ensure_ascii=False));return 1
    print(json.dumps({k:result[k] for k in ('ok','status')},ensure_ascii=False))
    return 0 if result['ok'] or (not args.execute and result['status']=='ready_not_executed') else 1


if __name__=='__main__':raise SystemExit(main())
