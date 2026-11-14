#!/usr/bin/env python3
"""S5正式stage21手动LOAD→入场→实际输入/活动存取的双端定向门。

默认只准备；--run必须由owned run-job包裹，原版先行、SDL后行。原资源和
完整输入冻结，图形WGC单HWND；不调用source API、不称自然整关或全游戏。
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

from native import s5_fixture as oracle
from native import s3_fixture as state_oracle
import run_s3_stage_check as shared
import fd2_natural_oracle as natural
import run_original_natural as protocol
from fd2_display_requirement import windows_display_requirement
from fd2_opening_oracle import page_match
from fd2_transition_oracle import picker_match,picker_image,notice_match,notice_resources
from run_original_battle_menu import wait_title
from run_original_battle_animation import owned_job_required
from run_original_comparison import Capture,running,title_templates
from run_original_end_turn import round_assets,RoundOracle,META,UNITS
from run_original_load import require_source_unchanged
from field_play_input_test import wait_log
from static_data.s5_output import validate_output
from static_data.generate import decode_json
from run_g2_battle_check import sha


def title_case_policy(p,kind,stage_definition):
    """仅已登记S5夹具的SDL发布门UI；原版仍三项，不作原义oracle。"""
    if kind not in ('original','sdl'):raise ValueError('未知S5标题backend')
    active=p['source'][META+2];manual=p['manual_state']['stage']
    if p['source'][META]!=1 or (active,manual) not in ((29,21),(29,24),(20,20),(26,26),(29,29)):
        raise ValueError('S5标题只接受登记的active/manual 29/21、29/24、20/20、26/26、29/29夹具')
    stage_definition=Path(stage_definition).resolve();raw=stage_definition.read_bytes()
    document=decode_json(raw);capability_id=f'stage.capability.{active:03d}'
    capabilities=[flow for flow in document['flows'] if flow['id']==capability_id]
    if len(capabilities)>1:raise ValueError('重复S5标题capability')
    if capabilities:
        flow=capabilities[0];entries=[n for n in flow.get('nodes',[]) if n.get('id')==flow.get('entry')]
        if len(entries)!=1 or entries[0].get('op')!='stage_capability' or len(entries[0].get('args',[]))!=4 or any(
            type(value) is not int or not 0<=value<=255 for value in entries[0]['args']):
            raise ValueError('S5标题capability不是正式入口节点')
    bound=bool(capabilities)
    if (active,manual) in ((20,20),(26,26),(29,29)) and not bound:
        raise ValueError('S5 CONTINUE代表要求已发布capability')
    count=3 if kind=='original' or bound else 2
    return {'menu_count':count,'active_snapshot_stage':active,'manual_slot0_stage':manual,
        'active_capability_id':capability_id,'active_capability_bound':bound,
        'stage_definition_path':str(stage_definition),'stage_definition_sha256':state_oracle.digest(raw),
        'basis':'original title has three items' if kind=='original' else
            'main title_continue_available uses published stage capability; SDL UI policy, not original oracle'}


def initial_load_availability(record):
    """仅首次Save前的原active档；cap缺失可证明禁用，存在不等于可导入。"""
    kind=record['kind'];policy=record['title_menu_policy']
    domain=(policy['active_snapshot_stage'],policy['manual_slot0_stage'])
    if kind not in ('original','sdl') or domain not in ((29,21),(29,24),(20,20)):
        raise ValueError('未知S5首次活动Load资格域')
    if kind=='original':
        available=True;basis='fixed late SAV original enabled Load gate; no SDL resource claim'
    elif domain==(20,20) and policy['active_capability_bound']:
        available=True;basis='controlled source20 snapshot; real CONTINUE must precede this Save'
    elif domain in ((29,21),(29,24)) and not policy['active_capability_bound']:
        available=False;basis='snapshot_is_importable rejects missing capability before resource checks'
    else:
        raise ValueError('capability存在不足证明原active29可Load；需同SHA快照实际导入预检')
    record['initial_load_availability']={'available':available,'basis':basis,
        'active_snapshot_stage':policy['active_snapshot_stage'],
        'stage_definition_sha256':policy['stage_definition_sha256'],
        'title_support_is_not_full_Load_qualification':True}
    return available


class NoTurn2Event:
    """源metadata21在turn2三阵营均无记录；出现未知等待就停，不猜ACK。"""
    victory_text_complete=False
    story=reward=None
    def message(self,frame):return None


def end21(capture,p,state,save,guard,record,cp):
    metadata=p['data']['stages'][21]['metadata']
    if any(metadata[3+i*3]==2 for i in range(16)):
        raise ValueError('原stage21 turn2新增来源，不沿用空事件观察域')
    before=state_oracle.decode_save(save);frozen=bytearray(before)
    for raw,unit in enumerate(state_oracle.units(before)):
        if unit[6]!=2:frozen[UNITS+80*raw+5]|=4
        if unit[64:66]!=unit[66:68] or unit[68:70]!=unit[70:72] or any(unit[34:40]):
            raise ValueError('END受控域要求全员满HP/MP且无计时状态')
    frozen=state_oracle.checksum(frozen)
    record['end_controlled_injection']={'natural_whole_stage_claimed':False,
        'before_sha256':state_oracle.digest(before),'after_sha256':state_oracle.digest(frozen),
        'reason':'nonplayer flags04 suppress AI decisions; real Load and complete END remain formal',
        'patches':[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(before,frozen)) if a!=b]}
    (capture.artifacts/'s5-before-END-injection.SAV').write_bytes(save.read_bytes())
    save.write_bytes(oracle.encode(frozen));guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    (capture.artifacts/'s5-END-input.SAV').write_bytes(guard[0])
    loaded=shared.load_activity(capture,p,frozen,save,guard,record,cp)
    record.update(native_round_banners=[],resource_acknowledgements=[])
    view=protocol.end_phase(capture,p['original'],loaded,p['assets'],
        natural.cursor_bank(p['original'],loaded,p['assets']),NoTurn2Event(),record,cp)
    view=protocol.wait_progress(capture,p['original'],view,1,p['assets'],NoTurn2Event(),record,cp,phase=True)
    records=state_oracle.loaded_units(loaded,record['kind'])
    for unit in records:unit[5]&=127
    wanted=state_oracle.as_view(loaded,records,21,2,view[META+3:META+5],view[META+5:META+7])
    shared.save_activity(capture,p,wanted,save,guard,record,cp,'s5-stage21-turn2',
        lambda before,after,v:state_oracle.check_activity(before,after,v,records,[bytearray([1]*80) for _ in records],
            metadata,kind=record['kind']))
    record['formal_END']={'confirmed':True,'turn':2,'phase':2,'active_count':len(records),
        'all_nonplayer_AI_decisions_suppressed_by_declared_input':True,'whole_shared_RNG_claimed':False}


def title_entry(capture,p,save,guard,record):
    cp=shared.checkpoint(capture,record,save,guard);state=p['manual_state']
    town=shared.preparation_town(p,state);assets=town['assets']
    titles=p['titles'][record['title_menu_policy']['menu_count']]
    wait_title(capture,titles[0],timeout=90)
    capture.key('DOWN');capture.wait(titles[1]);capture.key('ENTER')
    before=state_oracle.decode_save(save)
    cp('s5-title-load-slot0',lambda f:picker_match(f,picker_image(assets,before,0)),allow_low_color_resource=True)
    capture.key('ENTER');cp('s5-stage21-prepare',lambda f:shared.town_match(f,town,0),timeout=90)
    for selected in (1,2):
        capture.key('LEFT');cp('s5-deploy-'+str(selected),lambda f,s=selected:shared.town_match(f,town,s))
    button=RoundOracle(p['assets'],(0,0));notice=notice_resources(p['original'],assets,513,portrait=75,origin=(95,119))
    capture.key('ENTER')
    cp('s5-entry21-confirm',lambda f:dict(a,b=b) if (a:=button.buttons(f,0)) and (b:=notice_match(f,notice,p['assets'])) else None)
    capture.key('ENTER')
    view,records,masks=oracle.entry21(before,state,p['data'],p['original'])
    pages=state_oracle.pages(p['original'],p['assets'],p['data'],view,21,(0,))
    record['entry_acknowledgements']=[]
    for index,page in enumerate(pages):
        cp('s5-entry21-story0-'+str(index),lambda f,page=page:page_match(f,page,p['assets']),timeout=90)
        record['entry_acknowledgements'].append({'fragment':0,'token_offset':page['token_offset'],'portrait':page['portrait_id']})
        capture.key('ENTER')
    cp('s5-entry21-browse',lambda f:natural.focus_match(f,p['original'],view,p['assets']),timeout=90)
    selected=p['selected_actor']
    view=protocol.navigate(capture,p['original'],view,p['assets'],
        natural.cursor_bank(p['original'],view,p['assets']),tuple(records[selected][:2]),cp,'s5-select-navigate')
    summary=natural.selection_summary(p['original'],view,p['assets'],selected)
    ranges=natural.range_probe(p['original'],view,p['assets'],selected)
    anchor='field result query: stage=21 result=0';log=capture.artifacts/'game.log'
    previous=log.read_text(encoding='utf8',errors='replace').count(anchor) if record['kind']=='sdl' else None
    capture.key('ENTER')
    cp('s5-real-selection',lambda f:natural.focus_match(f,p['original'],view,p['assets'],summary=summary)
        if natural.range_match(f,ranges) else None)
    capture.key('ESC')
    cp('s5-selection-cancelled',lambda f:natural.focus_match(f,p['original'],view,p['assets'])
        if natural.range_match(f,ranges) is None else None)
    if record['kind']=='sdl' and wait_log(capture.process,log,anchor,timeout=30).count(anchor)!=previous+1:
        raise AssertionError('S5真实取消未完成唯一controller后置结果查询')
    record['controller_input']={'selection_and_cancel':True,'result':0,'stage':21,
        'actor_raw':selected,'SDL_unique_post_log':record['kind']=='sdl','original_runtime_latch_dumped':False}
    saved=shared.save_activity(capture,p,view,save,guard,record,cp,'s5-entry21-active',
        lambda before,after,v:state_oracle.check_activity(before,after,v,records,masks,
            p['data']['stages'][21]['metadata'],kind=record['kind'],fresh_metadata=True,roster=state['roster']),
        load_available=initial_load_availability(record))
    loaded=shared.load_activity(capture,p,saved,save,guard,record,cp)
    end21(capture,p,loaded,save,guard,record,cp)
    return loaded


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve(strict=True)
    p=oracle.prepare(original);p['assets']=round_assets(original);p['titles']=title_templates(original/'FDOTHER.DAT')
    with state_oracle.resource_domain(original,(21,)):
        view,records,_=oracle.entry21(p['source'],p['manual_state'],p['data'],original)
        pages=state_oracle.pages(original,p['assets'],p['data'],view,21,(0,))
        candidates=[]
        for raw in range(16):
            probes=natural.range_probe(original,view,p['assets'],raw,allow_empty=True)
            if probes:candidates.append(raw)
        if not candidates:raise ValueError('S5来源缺可处理选择资源')
        p['selected_actor']=candidates[0]
        natural.selection_summary(original,view,p['assets'],p['selected_actor'])
    p['manifest']['preflight']={'stage':21,'active_count':len(records),'full_story0_pages':len(pages),
        'selected_actor':p['selected_actor'],'native_execution_claimed':False,
        'source':'complete1088d/S2entry21 caller + original DAT/EXE/SAV'}
    return run_prepared(args,p,title_entry,(21,),'stage21-entry-storage',
        ['natural whole-stage combat','unselected bindings','new cinematics (separate representatives)',
         'whole shared RNG equality','Linux/macOS'])


def run_prepared(args,p,exercise,stages,scenario,excluded,*,kinds=('original','sdl')):
    """有限代表共用同一host/owned/source/SAV闭包；各原资源模型及业务输入显式传入。"""
    root=args.root.resolve();original=(root/'original_game').resolve(strict=True)
    if type(kinds)is not tuple or kinds not in(('original','sdl'),('sdl',)):
        raise ValueError('S5后端只准原/SDL完整pair或明确SDL-only')
    if Path(p['original']).resolve()!=original:raise ValueError('S5代表必须借同一正式原资源目录')
    if not callable(exercise) or not stages or any(type(s) is not int or not 0<=s<30 for s in stages):
        raise ValueError('S5代表缺有限场景/业务端点')
    output=validate_output(args.artifacts,original=original)
    if 'assets' not in p:p['assets']=round_assets(original)
    if 'titles' not in p:p['titles']=title_templates(original/'FDOTHER.DAT')
    output.mkdir(parents=True);(output/'controlled.SAV').write_bytes(p['encoded'])
    shared.write(output/'fixture.json',p['manifest'])
    report={'schema':'fd2-s5-stage-native-v1','scenario':scenario,'ok':False,'status':'prepared',
        'fixture':p['manifest'],'cases':[],'display_requirement':{},'formal_App_required':True,
        'excluded':list(excluded),'title_menu_policy':{kind:title_case_policy(p,kind,
            root/'data/static/flows/stage.json') for kind in kinds},'required_backends':list(kinds)}
    if not args.run:shared.write(output/'comparison.json',report);return report
    if sys.platform!='win32':raise ValueError('此入口当前仅Windows WGC可运行，其他平台未执行')
    owned_job_required()
    if args.sdl.name.lower() not in ('fd2sdl.exe','fd2sdl'):raise ValueError('必须传正式App，不接受机制fixture')
    binaries=[args.sdl.resolve(),args.sdl.with_suffix('.pdb').resolve(),args.driver.resolve(),args.dosbox.resolve(),original/'FD2.SAV']
    user_save=original.parent/'FD2.SAV'
    if user_save.exists():binaries.append(user_save)
    inputs,baseline,sources=shared.freeze(root,original,binaries,args.build_dir.resolve())
    for policy in report['title_menu_policy'].values():
        if inputs.get(policy['stage_definition_path'])!=policy['stage_definition_sha256']:
            raise ValueError('标题UI政策与冻结stage定义身份不一致')
    inputs[str((root/'src/CMakeLists.txt').resolve())]=sha(root/'src/CMakeLists.txt')
    report.update(input_sha256=inputs,product_source_paths=sources,actual_build_directory=str(args.build_dir.resolve()))
    try:
        with windows_display_requirement(report['display_requirement']),state_oracle.resource_domain(original,stages):
            for kind in kinds:
                work=output/kind;work.mkdir();record={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[],
                    'title_menu_policy':report['title_menu_policy'][kind]}
                report['cases'].append(record);env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf8')
                    inputs[str(config)]=sha(config)
                    for file in game.rglob('*'):
                        if file.is_file() and file.resolve() not in (save.resolve(),(game/'FD2.TMP').resolve()):inputs[str(file.resolve())]=sha(file)
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(p['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                try:
                    if not shared.unchanged(inputs,original,baseline,root,sources):raise ValueError('首窗口前冻结来源变化')
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),record['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,record['inputs'])
                        exercise(capture,p,save,guard,record)
                    require_source_unchanged(save,*guard)
                    record['ok']=len(record['processes'])==1 and all(r['cleaned'] and not r['forced_cleanup'] and r['exit_code']==0 for r in record['processes'])
                finally:
                    record['sources_unchanged']=shared.unchanged(inputs,original,baseline,root,sources)
                    record['save_unchanged_since_measured_write']=save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1]
                    record['ok'] &= record['sources_unchanged'] and record['save_unchanged_since_measured_write']
                    shared.write(work/'comparison.json',record)
                if not record['ok']:raise AssertionError('S5正式输入端未通过：'+kind)
        report.update(ok=True,status='passed',original_runtime_executed='original'in kinds,SDL_runtime_executed='sdl'in kinds)
    except BaseException as error:report.update(status='failed',error={'type':type(error).__name__,'message':str(error)})
    finally:
        report['sources_unchanged']=shared.unchanged(inputs,original,baseline,root,sources)
        report['ok'] &= report['sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        shared.write(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox','build-dir'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--run',action='store_true');args=parser.parse_args(argv)
    if args.run and (not args.build_dir or any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('sdl','driver','dosbox'))):
        parser.error('--run需正式App/driver/DOSBox及实际构建目录')
    try:report=run(args)
    except Exception as error:parser.exit(1,'S5正式输入工具未能准备：'+str(error)+'。检查来源与新输出目录。\n')
    print(json.dumps({k:report[k] for k in ('ok','status','scenario')},ensure_ascii=False))
    return int(args.run and not report['ok'])


if __name__=='__main__':raise SystemExit(main())
