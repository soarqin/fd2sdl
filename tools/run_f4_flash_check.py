"""F4唯一flash代表：合法S5隔离档→正常250cc，仅录新flash前后。

默认只准备；--run由owned Job、严格ON与WGC串行原版/正式SDL。
全64000 RGB同轮A/B关系不等于绝对compositor、所有物理帧或壁钟等价。
"""
from __future__ import annotations
import argparse,json,os,sys
from contextlib import contextmanager
from pathlib import Path
import original_reference
import fd2_f4_flash as flash_source
import run_s3_stage_check as shared
import run_s5_stage_check as host
import run_s5_late_check as late_host
from native import s5_late_fixture as fixture
from native import s3_fixture as state
from fd2_f4_flash import source_model,find_relation,read_series,SOURCE_MANIFEST
from fd2_opening_oracle import page_match
from run_f2_input_check import source_facts,sha
from run_f2_prepare_check import command_configuration,adapt_legacy_g3_rate
from run_g4_detail_check import detail_series,series_checkpoint
from run_original_comparison import quiet_game_command
from run_original_battle_animation import owned_job_required
from verify_f4_resource_pixels import checked_output


def fact(path):
    path=Path(path).resolve(strict=True);stamp=path.stat().st_mtime_ns
    result={'sha256':sha(path),'mtime_ns':stamp,'size':path.stat().st_size}
    if path.stat().st_mtime_ns!=stamp:raise ValueError('读取身份期间来源变化：'+str(path))
    return result


def exercise(capture,p,save,guard,record):
    cp=shared.checkpoint(capture,record,save,guard)
    late_host.continue_result(capture,p,record,cp) #only8/13/14; not15/16
    pages=p['flash15_pages']
    if not pages or not p['flash16_pages']:raise ValueError('flash前后缺原文本等待门')
    late_host.acknowledge(capture,p,record,cp,pages[:-1],'flash15-prefix')
    with detail_series(capture,record) as scope:
        series_checkpoint(capture,scope,'flash15-last',
            lambda f:page_match(f,pages[-1],p['assets']),record['checkpoints'],timeout=60)
        after=scope['after']['index']
        capture.key('ENTER',fast=True) #do not spend250ms in legacysettle, nofollowupkeys
        series_checkpoint(capture,scope,'flash16-first',
            lambda f:page_match(f,p['flash16_pages'][0],p['assets']),record['checkpoints'],timeout=60)
        before=scope['after']['index']
        hwnd=scope['ready']['hwnd']
    series=record['detail_series']
    if series.get('checkpoint_PNGs_bound_to_closed_series')is not True:
        raise ValueError('flash尚未绑定已关闭WGC系列')
    samples=read_series(series['frames'],capture.process.pid,hwnd,after_index=after,before_index=before)
    result=find_relation(samples,p['flash_model'])
    record['flash_relation']={**result,'samples':len(samples),'after15_index':after,'before16_index':before,
        'input_after15':'exactly one fast ENTER; no input through first16 wait',
        'not_acknowledged_fragment16':True,'WGC_interval_ms':33,'WGC_maximum_ms':45000,
        'controlled_fixture':True,'source_before_model_sha256':p['flash_model_sha256']}
    host.require_source_unchanged(save,*guard)


def run(args):
    root=args.root.resolve(strict=True);original=root/'original_game'
    out=checked_output(root,args.artifacts,args.output_root)
    if any(k.upper().startswith('DOSBOX_') for k in os.environ):raise ValueError('拒绝未冻结DOSBOX环境覆盖')
    out.mkdir(parents=True,exist_ok=False)
    source_before=source_facts(root)
    original_before={str(p.resolve()):fact(p) for p in original.rglob('*') if p.is_file()}
    paths=[Path(__file__),Path(flash_source.__file__),SOURCE_MANIFEST,args.rgb_source]
    for module in tuple(sys.modules.values()):
        p=getattr(module,'__file__',None)
        if p and Path(p).resolve().is_relative_to(root/'tools'):paths.append(Path(p))
    if args.run:
        owned_job_required()
        for name in ('sdl','driver','dosbox','bootstrap','snapshot_probe','build_dir'):
            value=getattr(args,name)
            if value is None or not value.exists():raise ValueError('--run缺实际参数：'+name)
        paths.extend([args.sdl,args.sdl.with_suffix('.pdb'),args.driver,args.dosbox,args.bootstrap,
            args.snapshot_probe,Path(original_reference.REFERENCE)])
    guards={str(Path(p).resolve()):fact(p) for p in paths}
    report={'schema':'fd2.f4.flash-native.v1','ok':False,'status':'preparing','controlled_fixture':True,
        'source_before':source_before,'original_before':original_before,'guards_before':guards,
        'references':[],'rate_adaptations':[],'launch_attempts':[],'launched_roles':[],
        'limitations':['Same-run fullRGB ABABA relation, not absolute A14-C04 compositor qualification',
            'Finite33ms WGC does not observe everyCPU copy or prove exactPIT walltiming',
            'No natural whole-stage, sharedRNG, audio, credits/montage or otherstage claim']}
    previous_running=host.running
    previous_output,previous_import_output=host.validate_output,late_host.validate_output
    def checks():
        return {'sources_unchanged':source_facts(root)==source_before,
            'original_unchanged':{str(p.resolve()):fact(p) for p in original.rglob('*') if p.is_file()}==original_before,
            'guards_unchanged':all(fact(p)==v for p,v in guards.items())}
    def output(requested,original):
        if Path(original).resolve()!=(root/'original_game').resolve():raise ValueError('输出借用原来源身份不符')
        return checked_output(root,Path(requested),args.output_root)
    @contextmanager
    def running(command,cwd,env,log,driver,receipts,**options):
        index=len(report['launched_roles']);role=('original','sdl')[index] if index<2 else None
        executable=Path(command[0]).resolve()
        expected=args.dosbox.resolve() if role=='original' else args.sdl.resolve() if role=='sdl' else None
        if executable!=expected:raise ValueError('flash必须原版后SDL各一次串行启动')
        if not all(checks().values()):raise ValueError('首窗口前冻结来源改变')
        if role=='original':
            command,actual,adaptation=adapt_legacy_g3_rate(command,env,args.bootstrap)
            report['rate_adaptations'].append(adaptation)
            configs,overrides=command_configuration(actual)
            for identity in adaptation['configs']:guards[identity['path']]=fact(identity['path'])
            reference=original_reference.build(original,args.dosbox,configs,overrides,args.bootstrap)
            if reference['configs']!=[{key:identity[key] for key in ('path','size','sha256')}
                    for identity in adaptation['configs']]:raise ValueError('实际conf来源在资格门改变')
            report['references'].append(reference)
        else:actual,_,_=quiet_game_command(command,env)
        report['launch_attempts'].append({'role':role,'command':actual})
        if not all(checks().values()):raise ValueError('参照资格后启动前冻结来源改变')
        count=len(receipts)
        with previous_running(command,cwd,env,log,driver,receipts,**options) as process:
            if len(receipts)!=count+1 or receipts[-1].get('command')!=actual:
                raise ValueError('实际Popen命令与冻结参照不同')
            report['launched_roles'].append(role)
            yield process
    try:
        p=fixture.derive(original,fixture.SCENARIOS[2]);model,records=source_model(p,args.rgb_source)
        model_path=out/'source-model.json';shared.write(model_path,model)
        p.update(flash_model=model,flash_model_sha256=sha(model_path),assets=host.round_assets(original),
            titles=host.title_templates(original/'FDOTHER.DAT'))
        with state.resource_domain(original,(26,)):
            p['victory_pages']=late_host.pages_for(p,records,26,(8,13,14),p['manual_state']['roster'])
            p['flash15_pages']=late_host.pages_for(p,records,26,(15,),p['manual_state']['roster'])
            p['flash16_pages']=late_host.pages_for(p,records,26,(16,),p['manual_state']['roster'])
        p['source_evidence_paths']=[SOURCE_MANIFEST,args.rgb_source,model_path]
        p['manifest']['source_guards']={str(Path(path).resolve()):sha(path) for path in p['source_evidence_paths']}
        guards[str(model_path.resolve())]=fact(model_path)
        (out/'controlled.SAV').write_bytes(p['encoded']);shared.write(out/'fixture.json',p['manifest'])
        report.update(source_model=str(model_path),source_model_sha256=sha(model_path),
            encoded_SAV_sha256=state.digest(p['encoded']),status='prepared')
        if args.run:
            host.running=running;host.validate_output=output;late_host.validate_output=output
            pair_args=argparse.Namespace(**vars(args));pair_args.artifacts=out/'paired'
            late_host.snapshot_preflight(pair_args,p) #actualsameSHA C gate BEFOREfirstwindow
            result=host.run_prepared(pair_args,p,exercise,(26,),'stage26-single-source-flash',report['limitations'],
                kinds=('original','sdl'),require_display_on=True)
            report['comparison']=str(out/'paired/comparison.json');report['pair_ok']=result['ok']
            report['display_requirement']=result.get('display_requirement',{})
            cases=result.get('cases',[])
            report['ok']=result['ok'] and report['launched_roles']==['original','sdl'] and len(report['references'])==1 and \
                len(cases)==2 and all(c.get('flash_relation',{}).get('whole_before_relation_qualified')is True for c in cases)
            report['status']='passed' if report['ok'] else 'failed'
            if not result['ok']:report['error']=result.get('error',{'message':'见pair收据'})
        else:report['native_executed']=False
    except BaseException as error:
        report.update(ok=False,status='failed',error={'type':type(error).__name__,'message':str(error)})
    finally:
        host.running=previous_running;host.validate_output=previous_output;late_host.validate_output=previous_import_output
        report['guards_after']={}
        for name,operation in (
            ('sources_unchanged',lambda:source_facts(root)==source_before),
            ('original_unchanged',lambda:{str(p.resolve()):fact(p) for p in original.rglob('*') if p.is_file()}==original_before),
            ('guards_unchanged',lambda:all(fact(p)==v for p,v in guards.items()))):
            try:report[name]=operation()
            except BaseException as error:
                report[name]=False;report.setdefault('secondary_errors',[]).append({'phase':name,'error':str(error)})
        for path in guards:
            try:report['guards_after'][path]=fact(path)
            except OSError as error:report['guards_after'][path]={'error':str(error)}
        report['guards_before']=guards
        valid=all(report.get(k)is True for k in ('sources_unchanged','original_unchanged','guards_unchanged'))
        if not valid:
            report['ok']=False;report['status']='failed'
            if 'error'not in report:report['error']={'type':'LateGuardChanged','message':'冻结source/resource/binary晚守卫改变'}
        shared.write(out/'report.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts','rgb-source'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox','build-dir','snapshot-probe','bootstrap','output-root'):
        parser.add_argument('--'+name,type=Path)
    parser.add_argument('--run',action='store_true');args=parser.parse_args(argv)
    if args.run and sys.platform!='win32':parser.error('此代表要求Windows WGC；未启动GUI')
    try:result=run(args)
    except (OSError,ValueError,RuntimeError) as error:parser.exit(1,'闪烁代表未能准备：'+str(error)+'。检查新目录与来源。\n')
    print(json.dumps({k:result.get(k)for k in ('ok','status','error')},ensure_ascii=False))
    return int(result['status']=='failed' or args.run and not result['ok'])
if __name__=='__main__':sys.exit(main())
