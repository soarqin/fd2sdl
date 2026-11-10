#!/usr/bin/env python3
"""P5/P6真实选择→最终NO/必须成员拒绝→原版单RAM后态量测。

--original-only为原端定向探针，overall ok=false；正常两端各自量测唯一热owner。
全SAV须始终22987B+mtime不变；不把选择前NoTown Save充当后态。
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import shutil

import fd2_dosbox_snapshot as snapshot
import fd2_cdb_prepare_snapshot as cdb_snapshot
import fd2_g3_prepare_oracle as oracle
import fd2_g3_prepare_pages as pages
import fd2_g3_deployment_pages as deployment
from fd2_display_requirement import windows_display_requirement
from fd2_transition_oracle import picker_image,picker_match,town_images
from run_g2_battle_check import freeze,unchanged,sha,fingerprint
from run_graphics_tests import decode_save
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import wait_title,wait_state
from run_original_comparison import Capture,running,title_templates,compare
from run_original_end_turn import RoundOracle,round_assets
from run_original_load import require_source_unchanged
from run_original_transition import town_match

SCENARIOS=oracle.SCENARIOS[-3:]
PRE_REUSE_RUNNER_SHA256='aa4a327d2b649ce272e8079f77f136c708ca31159f67d9b669e123accd5adca3'
PRE_REUSE_EXECUTION_AST='abfa252cfb2edc0dfa1e8081587434e2c804f4a41a0e44516b397cb3fb8105e0'


def serial(value):
    if isinstance(value,bytes):return value.hex()
    if isinstance(value,dict):return {k:serial(v) for k,v in value.items()}
    if isinstance(value,(tuple,list)):return [serial(v) for v in value]
    return value


def write(path,value):path.write_text(json.dumps(serial(value),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def prepare(root,scenario):
    original=Path(root)/'original_game';encoded,plain,before,after,manifest=oracle.derive_fixture(original,scenario)
    assets=pages.load(original);field=round_assets(original);buttons=RoundOracle(field,(0,0))
    result={'original':original,'encoded':encoded,'plain':plain,'before':before,'after':after,'manifest':manifest,
        'assets':assets,'titles':title_templates(original/'FDOTHER.DAT'),'steps':[],'checks':{},'rois':{}}
    if before['stage']==1:result['towns']=[town_images(original,assets,before,i) for i in range(5)]
    def add(name,predicate,key=None,**declaration):
        result['checks'][name]=predicate;result['steps'].append({'name':name,'key':key,**declaration})
    def confirmation(name,fragment,key,selected,xy=(95,119)):
        words=pages.notice(assets,fragment,portrait=75,xy=xy)
        def predicate(frame):
            message=pages.notice_match(frame,words);choice=buttons.buttons(frame,selected)
            return {'kind':'deployment_confirmation','fragment':fragment,'selected':selected,'message':message,'buttons':choice} if message and choice else None
        add(name,predicate,key,fragment=fragment,selected=selected,origin=list(xy))
    def selection(name,state,chosen,cursor,key):
        expected=deployment.selection(assets,state,chosen,cursor,19 if state['stage']>26 else 15)
        result['rois'][name]=expected
        add(name,lambda f,e=expected:deployment.match(f,e),key,cursor=cursor,chosen=list(chosen),remaining=expected['remaining'])
    add('title-load-selected',lambda f:{'kind':'title_load','selected':1} if compare(result['titles'][3][1],f)['ok'] else None,'DOWN')
    picker=picker_image(assets,plain,0);add('title-slot0',lambda f:picker_match(f,picker),'ENTER',low_color=True)
    if before['stage']==1:
        add('town-initial',lambda f:town_match(f,result,0),'ENTER')
        add('town-weapon',lambda f:town_match(f,result,1),'LEFT')
        add('town-exit',lambda f:town_match(f,result,2),'LEFT')
        confirmation('depart-confirm',513,'ENTER',0)
        selection('selection-initial',before,[],0,'ENTER')
    else:
        confirmation('notown-optional-save',410,'ENTER',0,xy=(100,119))
        confirmation('notown-save-no',410,'RIGHT',1,xy=(100,119))
        selection('selection-initial',before,[],0,'ENTER')
    chosen=[];cursor=0;selected=manifest['parameters']['selected'];quota=19 if before['stage']>26 else 15
    for raw in selected:
        while cursor!=raw-1:
            cursor=(cursor+1)%(before['count']-1)
            selection('selection-move-'+str(len(result['steps'])),before,chosen,cursor,'RIGHT')
        chosen.append(raw);cursor=(cursor+1)%(before['count']-1)
        if len(chosen)<quota:
            selection('selection-chosen-'+str(len(chosen)),before,chosen,cursor,'ENTER')
    if scenario.startswith('p6-'):
        expected=pages.notice(assets,657,portrait=75,dynamic=25,xy=(95,119))
        add('required-member24-missing',lambda f:pages.notice_match(f,expected),'ENTER',fragment=657,dynamic=25)
    else:
        confirmation('deployment-final-confirm',658,'ENTER',0)
        confirmation('deployment-final-no',658,'RIGHT',1)
        if before['stage']==1:add('town-after-final-no',lambda f:town_match(f,result,2),'ENTER')
        else:selection('notown-selection-reentered-after-no',after,[],0,'ENTER')
    result['measurement_checkpoint']=result['steps'][-1]['name']
    return result


def exercise(capture,prepared,save,guard,case,rpc,work,args):
    wait_title(capture,prepared['titles'][3][0],timeout=90)
    for step in prepared['steps']:
        require_source_unchanged(save,*guard)
        if step['key']:capture.key(step['key'])
        wait_state(capture,step['name'],prepared['checks'][step['name']],case['checkpoints'],timeout=45,
            allow_low_color_resource=step.get('low_color',False))
        require_source_unchanged(save,*guard)
    # 停键后同一原子emulation-thread dump；没有BREAK/console/额外按键。
    if case['kind']=='original':
        memory,receipt=rpc.snapshot(work,'post-action')
        actual,mapping=snapshot.resolve(memory,prepared['original'],receipt['DS_descriptor_before'])
    else:
        actual,receipt=cdb_snapshot.take(args.cdb,capture.process,args.sdl,args.pdb,work,case['cdb'])
        mapping={'source':'本轮PDB main frame→app_context.preparation','roster_scan_used':False,
            'all_fields_from_one_writemem':True,'layout':receipt['layout']}
    require_source_unchanged(save,*guard)
    comparison=snapshot.check_state(actual,prepared['after'])
    case.update(measurement_checkpoint=prepared['measurement_checkpoint'],snapshot=receipt,mapping=mapping,
        runtime_state_comparison=comparison,completed_runtime_state=True,
        whole_original_save_unchanged_bytes=len(save.read_bytes()),real_manual_post_Save_claimed=False)
    write(work/'runtime-state.json',actual);write(work/'runtime-mapping.json',mapping)
    if not comparison['ok']:raise AssertionError(case['kind']+' P5/P6完整热owner后态不同：'+str(comparison))
    case['business_sequence_completed']=True


def inherit_original(path,prepared,args,inputs,original_before):
    path=Path(path).resolve();folder=path.parent;root=args.root.resolve();sealed={}
    def seal(file,expected=None):
        file=Path(file).resolve()
        if not file.is_file() or expected is not None and sha(file)!=expected:raise ValueError('deployment原件文件缺失/SHA不同：'+str(file))
        sealed[str(file)]=sha(file);return file
    report=json.loads(seal(path).read_text(encoding='utf-8'))
    if report.get('scenario')!=args.scenario or report.get('original_case_passed') is not True or \
        report.get('final_sources_unchanged') is not True or report.get('original_before')!=original_before or \
        report.get('display_requirement',{}).get('released') is not True:
        raise ValueError('deployment原件没有独立成功原端/来源/显示恢复门')
    cases=[case for case in report.get('cases',[]) if case.get('kind')=='original']
    if len(cases)!=1:raise ValueError('deployment原件必须唯一原版端')
    case=cases[0];work=folder/'original'
    if case.get('ok') is not True or case.get('business_sequence_completed') is not True or \
        case.get('completed_runtime_state') is not True or case.get('guards')!={
            'sources_unchanged':True,'whole_Save_and_mtime_unchanged':True,'normal_owned_cleanup':True,'observer_closed':True}:
        raise ValueError('不能继承真正失败或未完成的deployment原版端')
    if json.loads(seal(work/'comparison.json').read_text(encoding='utf-8'))!=case:raise ValueError('deployment原版端与聚合收据不同')
    def tools(mapping):return {file:value for file,value in mapping.items() if Path(file).is_relative_to(root/'tools') and not Path(file).is_relative_to(root/'tools/tests')}
    old=report['inputs'];current_tools=tools(inputs);old_tools=tools(old);own=str(Path(__file__).resolve())
    if current_tools.keys()!=old_tools.keys():raise ValueError('deployment实际import闭包集合不同')
    endpoint_specific=[]
    for file,value in old_tools.items():
        if file==own and value==PRE_REUSE_RUNNER_SHA256:
            module=ast.parse(Path(own).read_text(encoding='utf-8'))
            protected=[ast.dump(node,include_attributes=False) for node in module.body if isinstance(node,ast.FunctionDef) and node.name in ('prepare','exercise')]
            if hashlib.sha256('\n'.join(protected).encode()).hexdigest()!=PRE_REUSE_EXECUTION_AST:raise ValueError('deployment原期望/执行AST已改变')
        elif Path(file).name=='fd2_cdb_prepare_snapshot.py':
            # 原01只运行case.kind=original；exercise的另一分支未调用CDB。
            # SDL量测适配可改，旧/新SHA单列；原RPC、ROI和模型仍全SHA相同。
            endpoint_specific.append({'file':file,'old':value,'current':current_tools[file],
                'reason':'仅SDL量测adapter；原端exercise原分支未调用，protected AST仍锁定'})
        elif value!=current_tools[file]:raise ValueError('deployment原端RPC/ROI/模型工具SHA已改变：'+file)
    for binary in (args.driver,args.dosbox):
        key=str(binary.resolve())
        if key not in inputs or old.get(key)!=inputs[key]:raise ValueError('deployment原driver/DOSBox身份不同')
    fixture=seal(folder/'controlled.SAV',old.get(str(folder/'controlled.SAV')))
    if fixture.read_bytes()!=prepared['encoded'] or json.loads(seal(folder/'fixture-manifest.json').read_text(encoding='utf-8'))!=serial(prepared['manifest']):
        raise ValueError('deployment原fixture/donor/patch manifest不同')
    processes=case['processes']
    if len(processes)!=1 or processes[0].get('cleaned') is not True or processes[0].get('forced_cleanup') is not False or processes[0].get('exit_code')!=0:
        raise ValueError('deployment原owned进程未正常清理')
    process=processes[0];pid=process['pid']
    if Path(process['command'][0]).resolve()!=args.dosbox.resolve():raise ValueError('deployment原执行目标不同')
    if seal(work/'game/FD2.SAV').read_bytes()!=prepared['encoded']:raise ValueError('deployment原全22987B SAV并非保持受控来源')
    for name in ('FD2.EXE','FDOTHER.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT','FDMUS.DAT'):
        seal(work/'game'/name,original_before[name]['sha256'])
    names=[step['name'] for step in prepared['steps']]
    checks=[row for row in case['checkpoints'] if row.get('name') in prepared['checks']]
    if [row['name'] for row in checks]!=names or case.get('measurement_checkpoint')!=prepared['measurement_checkpoint']:
        raise ValueError('deployment原页面/量测点顺序不同')
    from PIL import Image
    for row in checks:
        image=seal(work/(row['name']+'.png'))
        with Image.open(image) as frame:valid=prepared['checks'][row['name']](frame.convert('RGB'))
        if valid is None or row.get('pid')!=pid:raise ValueError('deployment原PNG没有过同原资源门：'+row['name'])
    keys=[row for row in case['inputs'] if 'key' in row]
    raw=[json.loads(line) for line in seal(work/'input-trace.jsonl').read_text(encoding='utf-8').splitlines()]
    deliveries=[json.loads(line) for line in seal(work/'input-delivery.jsonl').read_text(encoding='utf-8').splitlines()]
    if len(keys)!=len(raw) or len(keys)!=len(deliveries):raise ValueError('deployment原输入文件数量不同')
    for key,plain,delivery in zip(keys,raw,deliveries):
        receipt=key.get('delivery',{})
        if {k:v for k,v in key.items() if k!='delivery'}!=plain or delivery.get('delivery')!=receipt or \
            delivery.get('accepted') is not True or delivery.get('transport_ok') is not True or \
            delivery.get('receipts')!=[receipt] or receipt.get('backend')!='PostMessageW HWND' or receipt.get('pid')!=pid or \
            receipt.get('ok') is not True or receipt.get('error')!=0 or receipt.get('sent')!=1 or receipt.get('release')!=1 or \
            receipt.get('kind')!='key' or receipt.get('keys')!=1 or receipt.get('noactivate_verified') is not True or receipt.get('foreground_guard_ok') is not True:
            raise ValueError('deployment原owned输入/释放收据不同')
    observed=[row['key'] for row in keys]
    while observed and observed[0]=='ESC':observed=observed[1:]
    if observed!=[step['key'] for step in prepared['steps'] if step.get('key')]:raise ValueError('deployment原输入业务顺序不同')
    rpc=case['rpc'];requests=rpc.get('requests',[])
    if rpc.get('closed') is not True or rpc.get('pid')!=pid or rpc.get('tcp_owner_verified') is not True or \
        rpc.get('debugger_break_used') is not False or len(requests)!=7 or any(row.get('ok') is not True or row.get('id')!=str(i+1) for i,row in enumerate(requests)):
        raise ValueError('deployment原RPC owner/正常关闭/REQ收据不同')
    if requests[0]['command']!='PING' or requests[0]['response']!=['PONG']:raise ValueError('deployment原RPC没有原生PING')
    registers=snapshot.parse_registers(requests[1]['response']);ds=snapshot.parse_selector(requests[3]['response'])
    after=snapshot.parse_registers(requests[5]['response']);last_ds=snapshot.parse_selector(requests[6]['response'])
    receipt=case['snapshot'];dump=seal(receipt['path'],receipt['sha256'])
    if dump!=work/'post-action-ram.bin' or dump.stat().st_size!=snapshot.RAM_SIZE or \
        registers!=receipt['registers_before'] or after!=receipt['registers_after'] or ds!=receipt['DS_descriptor_before'] or \
        last_ds!=receipt['DS_descriptor_after'] or not registers['CR0']&1 or registers['EFLAGS']&(1<<17) or \
        any(ds[k]!=last_ds[k] for k in ('base','limit','type','dpl','flags')):
        raise ValueError('deployment原16MiB单snapshot/本轮descriptor视图不同')
    dump_command=requests[4]['command'].split(' ',4)
    if len(dump_command)!=5 or dump_command[:4]!=['EXEC','MEMDUMPBIN',f'{registers["DS"]:X}:0',f'{snapshot.RAM_SIZE:X}'] or \
        Path(dump_command[4]).resolve()!=dump or \
        not any('Memory dump binary success.' in line for line in requests[4]['response']):
        raise ValueError('deployment原单条MEMDUMPBIN请求不同')
    actual,mapping=snapshot.resolve(dump.read_bytes(),prepared['original'],ds)
    comparison=snapshot.check_state(actual,prepared['after'])
    if not comparison['ok'] or comparison!=case['runtime_state_comparison'] or mapping!=case['mapping']:
        raise ValueError('deployment原码锚/fixup/完整模型热后态复核不同')
    for name,expected in (('runtime-state.json',serial(actual)),('runtime-mapping.json',mapping)):
        if json.loads(seal(work/name).read_text(encoding='utf-8'))!=expected:raise ValueError('deployment原snapshot解码文件不同')
    proof={'source_report':str(path),'source_report_sha256':sealed[str(path)],'sealed_files':sealed,
        'original_case_reused':True,'new_original_process_started':False,'old_aggregate_ok':report.get('ok'),
        'case_success_not_aggregate_success_is_gate':True,'old_inputs':old,'current_inputs':dict(inputs),
        'old_production_sources':report['source_files'],'old_original':report['original_before'],
        'SDL_specific_adapter_sources':endpoint_specific,'source_execution_ast':PRE_REUSE_EXECUTION_AST,
        'native_business_pngs_rechecked':len(checks),'single_RAM_snapshot_redecoded':True,
        'legacy_initial_copy_mtime_not_separately_stored':True,'whole_SAV_bytes_rechecked':22987,
        'manual_post_Save_claimed':False}
    return json.loads(json.dumps(case)),proof


def inherited_unchanged(receipt):return all(Path(file).is_file() and sha(file)==value for file,value in receipt['sealed_files'].items())


def normalize_sdl_snapshot_metadata(case,work):
    """仅迁移SDL报告标签；原版prepare/exercise业务AST及历史RPC收据保持不变。"""
    if case['kind']!='sdl':return True
    if 'mapping' not in case:return False
    receipt=case.get('cdb',{});read=receipt.get('memory_read',{});layout=receipt.get('layout',{})
    valid=read.get('method')=='ReadProcessMemory' and read.get('single_ReadProcessMemory') is True and \
        read.get('all_requested_bytes_read') is True and receipt.get('all_fields_from_one_ReadProcessMemory') is True and \
        receipt.get('all_reads_in_one_stopped_attach') is True and read.get('address')==layout.get('address') and \
        read.get('size')==layout.get('size') and read.get('sha256')==receipt.get('snapshot',{}).get('sha256')
    case['mapping'].pop('all_fields_from_one_writemem',None)
    case['mapping'].update(measurement_method=read.get('method'),all_fields_from_one_ReadProcessMemory=valid)
    write(work/'runtime-mapping.json',case['mapping'])
    return valid


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or output.is_relative_to(original) or output.exists():raise ValueError('P5/P6需要新的隔离产物目录')
    before=fingerprint(original);prepared=prepare(root,args.scenario)
    if fingerprint(original)!=before:raise ValueError('P5/P6只读准备来源变化')
    binaries=[p.resolve() for p in (args.driver,args.dosbox,args.sdl) if p is not None]
    symbol_identity=None
    if args.sdl is not None and not args.original_only:
        binaries.extend([args.cdb.resolve(),args.pdb.resolve(),args.cdb.resolve().parent/'dbghelp.dll'])
        symbol_identity=cdb_snapshot.symbols(args.sdl,args.pdb,args.cdb.resolve().parent/'dbghelp.dll')
    inputs,frozen,sources=freeze(root,original,binaries)
    if frozen!=before:raise ValueError('P5/P6冻结来源变化')
    output.mkdir(parents=True);fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded'])
    if decode_save(fixture)!=prepared['plain']:raise ValueError('P5/P6夹具codec往返不同')
    inputs[str(fixture)]=sha(fixture);write(output/'fixture-manifest.json',prepared['manifest'])
    write(output/'expected-hot-state.json',prepared['after'])
    roi=output/'source-rois';roi.mkdir()
    for name,expected in prepared['rois'].items():
        expected['label'].save(roi/(name+'-label.png'))
        for phase,image in enumerate(expected['grids']):image.save(roi/(name+'-grid-phase'+str(phase)+'.png'))
    report={'ok':False,'probe_only':bool(args.original_only),'original_case_passed':False,'status':'ready_not_executed',
        'scenario':args.scenario,'steps':prepared['steps'],'inputs':inputs,'original_before':before,'source_files':sources,
        'cases':[],'display_requirement':{},'SDL_owner_measurement_implemented':True,'symbol_identity':symbol_identity,
        'whole_manual_post_Save_claimed':False,'all_hot_fields_from_one_original_dump':True,
        'independent_source_pages':True,'full_frame_or_audio_or_UI_RNG_equivalence_claimed':False}
    inherited=None
    if getattr(args,'original_evidence',None):
        case,inherited=inherit_original(args.original_evidence,prepared,args,inputs,before)
        report['cases'].append(case);report['original_case_passed']=True;report['inherited_original']=inherited
    if args.prepare_only:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        if inherited:report['inherited_original_final_files_unchanged']=inherited_unchanged(inherited)
        write(output/'comparison.json',report);return report
    owned_job_required()
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in (('sdl',) if inherited else ('original',) if args.original_only else ('original','sdl')):
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV'
                else:save=work/'FD2.SAV'
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'inputs':[],'checkpoints':[],'processes':[],'rpc':{},'cdb':{}};report['cases'].append(case)
                rpc_context=snapshot.OwnedRPC(case['rpc']) if kind=='original' else None
                if rpc_context:rpc_context.__enter__()
                try:
                    if kind=='original':
                        config=work/'dosbox.conf'
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'mcp_server={rpc_context.port}\n'
                            '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                            '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                        inputs[str(config)]=sha(config)
                        for filename in ('FD2.EXE','FDOTHER.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT','FDMUS.DAT'):
                            inputs[str(game/filename)]=sha(game/filename)
                        command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                    else:
                        env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                    if not unchanged(inputs,original,before,root,sources):raise ValueError('P5/P6窗口前来源变化')
                    if inherited and not inherited_unchanged(inherited):raise ValueError('deployment继承原件在新窗口前变化')
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        if rpc_context:rpc_context.bind(process)
                        capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                        exercise(capture,prepared,save,guard,case,rpc_context,work,args)
                    case['ok']=True
                finally:
                    if rpc_context:rpc_context.__exit__(None,None,None)
                    snapshot_method_valid=normalize_sdl_snapshot_metadata(case,work)
                    case['guards']={'sources_unchanged':unchanged(inputs,original,before,root,sources),
                        'whole_Save_and_mtime_unchanged':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'normal_owned_cleanup':len(case['processes'])==1 and all(p.get('cleaned') is True and p.get('forced_cleanup') is False and p.get('exit_code')==0 for p in case['processes']),
                        'observer_closed':case['rpc'].get('closed') is True if kind=='original' else
                            snapshot_method_valid and case['cdb'].get('detached') is True and case['cdb'].get('closed') is True}
                    case['ok'] &= all(case['guards'].values());write(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('P5/P6端点门失败：'+kind)
                if kind=='original':report['original_case_passed']=True
                for pattern in ('*.bin','runtime-*.json','cdb-transcript.txt'):
                    for path in work.glob(pattern):inputs[str(path)]=sha(path)
            report['original_case_passed']=report['cases'][0]['ok']
            report['probe_only']=bool(args.original_only)
            report['ok']=not args.original_only and len(report['cases'])==2 and all(c['ok'] for c in report['cases'])
            report['status']='paired_runtime_owner_passed' if report['ok'] else 'original_probe_passed_SDL_not_run'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)};report['status']='failed'
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        if inherited:
            report['inherited_original_final_files_unchanged']=inherited_unchanged(inherited)
            report['final_sources_unchanged'] &= report['inherited_original_final_files_unchanged']
        report['original_case_passed'] &= report['final_sources_unchanged'] and report['display_requirement'].get('released') is True
        report['ok'] &= report['original_case_passed']
        if not report['original_case_passed']:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('driver','dosbox','sdl'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--cdb',type=Path,default=Path('C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe'))
    parser.add_argument('--pdb',type=Path)
    parser.add_argument('--original-evidence',type=Path,help='同scenario已成功原版RPC端的聚合comparison.json；只跑SDL/CDB')
    parser.add_argument('--scenario',choices=SCENARIOS,required=True)
    parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--original-only',action='store_true')
    args=parser.parse_args(argv)
    if args.original_evidence and args.original_only:parser.error('--original-evidence与--original-only不能同时使用')
    if args.sdl is not None and args.pdb is None:args.pdb=args.sdl.with_suffix('.pdb')
    if not args.prepare_only and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('driver','dosbox')):
        parser.error('运行需存在的--driver/--dosbox')
    if not args.prepare_only and not args.original_only and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('sdl','cdb','pdb')):
        parser.error('双端运行需存在的--sdl/--cdb/--pdb；单原版原型须显式--original-only')
    report=run(args);print(json.dumps({k:report[k] for k in ('ok','probe_only','original_case_passed','status','scenario')},ensure_ascii=False))
    return 0 if args.prepare_only or report['ok'] or args.original_only and report['original_case_passed'] else 1


if __name__=='__main__':raise SystemExit(main())
