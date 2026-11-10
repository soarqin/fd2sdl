#!/usr/bin/env python3
"""G3教会成功分支→真实任意键wait→单次音乐owner快照；不酒店Save或开录音。

复用已证P3/P4页面/输入前缀，去掉collector节点以免HOST_W消费业务ACK。
原版单MEMDUMPBIN、SDL同PDB停止态读取；只证明本轮资源/播放状态/loop配置。
曲17/16另列未修改caller必经路径推断及既有WAV候选，不冒称执行PC或波形等价。
"""
from __future__ import annotations

import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace
import wave

from PIL import Image

import fd2_cdb_bgm_snapshot as cdb_music
import fd2_cdb_prepare_snapshot as cdb_protocol
import fd2_dosbox_snapshot as snapshot
import fd2_g3_audio_observation as original_music
import run_g3_prepare_check as base
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze,unchanged,sha,fingerprint
from run_graphics_tests import decode_save
from run_original_battle_animation import owned_job_required
from run_original_comparison import Capture,running
from run_original_load import require_source_unchanged


SCENARIOS={'p3-revive':('revive-no-candidates-real-wait',1,17),
    'p4-promote-normal':('promotion-no-candidates-real-wait',0,16)}
write=base.write_json


def prepare(root,scenario):
    if scenario not in SCENARIOS:raise ValueError('音乐代表仅接受P3成功复活/P4普通转职')
    point,loop,intermediate=SCENARIOS[scenario]
    prepared=base.prepare(root,scenario);base.finalize_ready_waits(prepared,scenario)
    matches=[i for i,step in enumerate(prepared['steps']) if step.get('name')==point]
    if len(matches)!=1:raise ValueError('复用前缀缺少唯一真实wait来源门')
    prepared['steps']=[dict(step) for step in prepared['steps'][:matches[0]+1] if 'collector' not in step]
    if any('save' in step or step.get('key')=='HOST_W' for step in prepared['steps']):
        raise ValueError('音乐量测前缀不得保存或插入录音ACK')
    prepared['steps'][-1]['reason']='沿既有真实wait blink来源门；本入口不启录音，不再ACK，随后单次取样'
    names={step['name'] for step in prepared['steps']}
    prepared['checks']={name:check for name,check in prepared['checks'].items() if name in names}
    prepared['source_rois']={name:roi for name,roi in prepared['source_rois'].items() if name in names}
    prepared['measurement_checkpoint']=point
    prepared['expected_music']={'track':11,'loop':loop,'status':4,'intermediate_track':intermediate}
    prepared['runtime_protocol']={'ready':True,'source_prefix':'既有P3/P4 prepare+finalize_ready_waits',
        'measurement':'真实588/591 wait，随后不再投任何业务键，单次音乐owner读取',
        'new_recording_started':False,'manual_post_Save_claimed':False,
        'business_full_Save_completed':False,'full_frame_or_audio_equivalence_claimed':False}
    return prepared


def wav_candidates(path,prepared,original_before):
    """只封存旧录音身份，不继承旧业务PASS、不以混合WAV辨认曲目。"""
    if path is None:return {'recording_candidates':[],'sealed_files':{},'runtime_gate_reused':False}
    path=Path(path).resolve(strict=True);folder=path.parent;sealed={}
    def seal(file,expected=None):
        file=Path(file).resolve(strict=True);value=sha(file)
        if not file.is_relative_to(folder) or expected is not None and value!=expected:
            raise ValueError('既有WAV候选越收据目录或文件SHA不同')
        sealed[str(file)]=value;return file
    report=json.loads(seal(path).read_text(encoding='utf-8'))
    if report.get('scenario')!=prepared['manifest']['scenario'] or report.get('final_sources_unchanged') is not True or \
        report.get('original_before')!=original_before or report.get('display_requirement',{}).get('released') is not True:
        raise ValueError('既有WAV候选的场景/原源/显示释放收据不同')
    if seal(folder/'controlled.SAV').read_bytes()!=prepared['encoded']:
        raise ValueError('既有WAV候选夹具不是本轮完整22987B来源')
    candidates=[]
    for case in report.get('cases',[]):
        if case.get('ok') is not True or case.get('collection_completed') is not True:continue
        kind=case.get('kind')
        if kind not in ('original','sdl') or not case.get('guards') or not all(case['guards'].values()):
            raise ValueError('既有WAV成功端缺少来源/清理门')
        if json.loads(seal(folder/kind/'comparison.json').read_text(encoding='utf-8'))!=case:
            raise ValueError('既有WAV聚合与端点收据不同')
        records=case.get('native_audio',{}).get('files',[]) if kind=='original' else [case.get('audio_capture',{}).get('wav',{})]
        for fact in records:
            if not fact.get('path') or not fact.get('sha256'):raise ValueError('既有WAV没有真实文件身份')
            file=seal(fact['path'],fact['sha256'])
            with wave.open(str(file),'rb') as stream:
                actual={'channels':stream.getnchannels(),'sample_width':stream.getsampwidth(),
                    'sample_rate':stream.getframerate(),'frames':stream.getnframes(),'compression':stream.getcomptype()}
            if actual['channels']!=2 or actual['sample_width']!=2 or actual['frames']<=0 or actual['compression']!='NONE' or \
                any(actual[key]!=fact[key] for key in actual if key in fact):
                raise ValueError('既有WAV实际header或正长度与收据不同')
            candidates.append({'kind':kind,'path':str(file),'sha256':sealed[str(file)],**actual,
                'prior_run_only':True,'track_or_sample_identification_claimed':False})
    if not candidates:raise ValueError('既有收据没有成功并关闭的WAV候选')
    return {'source_report':str(path),'source_report_sha256':sealed[str(path)],'recording_candidates':candidates,
        'sealed_files':sealed,'runtime_gate_reused':False,'waveform_or_timbre_equivalence_claimed':False}


def checkpoint_binding(capture,prepared,case):
    point=prepared['measurement_checkpoint'];pid=capture.process.pid
    if capture.process.poll() is not None or not case['checkpoints'] or \
        case['checkpoints'][-1].get('name')!=point or case['checkpoints'][-1].get('pid')!=pid:
        raise ValueError('音乐owner取样缺少同本轮存活PID的最后真实wait页面')
    image=capture.artifacts/(point+'.png')
    with Image.open(image) as frame:match=prepared['checks'][point][0](frame.convert('RGB'))
    deliveries=[row['delivery'] for row in capture.trace if 'delivery' in row]
    if match is None or not deliveries or deliveries[-1].get('pid')!=pid or not deliveries[-1].get('hwnd'):
        raise ValueError('音乐owner取样缺少独立原资源PNG或owned最后输入')
    return {'pid':pid,'checkpoint':point,'source_png':str(image),'source_png_sha256':sha(image),
        'source_ROI_rechecked':True,'last_input_hwnd':deliveries[-1]['hwnd'],'input_count':len(capture.trace),
        'page':case['checkpoints'][-1],'no_business_keys_during_observation':False}


def exercise(capture,prepared,save,guard,case,rpc,work,args):
    capture.g3_case_record=case
    base.exercise(capture,prepared,save,guard,case,SimpleNamespace(driver=args.driver,
        sdl_audio_source='app-mix',animation_capture_interval_ms=16))
    if case.pop('business_sequence_completed',False) is not True:raise AssertionError('教会前缀没有完成')
    case['source_prefix_completed']=True;case['business_full_Save_completed']=False
    binding=checkpoint_binding(capture,prepared,case);expected=prepared['expected_music']
    require_source_unchanged(save,*guard)
    if case['kind']=='original':
        memory,receipt=rpc.snapshot(work,'music-wait');write(work/'music-wait-receipt.json',receipt)
        observation=original_music.observe_music(memory,prepared['original'],receipt['DS_descriptor_before'],11,expected['loop'])
        if case['rpc'].get('pid')!=capture.process.pid or case['rpc'].get('tcp_owner_verified') is not True:
            raise ValueError('音乐RAM没有本轮live RPC PID归属')
        observation['scope']['actual_RPC_owner_revalidated_offline']=False
        observation['scope']['actual_RPC_owner_revalidated_in_this_run']=True
        case['music_snapshot']=receipt
    else:
        observation,_=cdb_music.take(args.cdb,capture.process,args.sdl,args.pdb,work/'music-observer',
            case['cdb_music'],prepared['original'],11,expected['loop'])
        if case['cdb_music'].get('target_pid')!=capture.process.pid:
            raise ValueError('音乐CDB取样不是本轮owned PID')
    require_source_unchanged(save,*guard)
    if capture.process.poll() is not None or len(capture.trace)!=binding['input_count']:
        raise ValueError('音乐读取时目标退出或有额外业务键')
    binding['no_business_keys_during_observation']=True
    case.update(music_observation=observation,measurement=binding)
    write(work/'music-observation.json',observation)
    if observation.get('ok') is not True:raise AssertionError('本轮音乐资源/播放状态/loop不符：'+str(observation.get('checks')))
    case['intermediate_music']={'track':expected['intermediate_track'],
        'source_callsite':next(row for row in observation.get('sources',observation.get('source',{}))['g3_music_call_sites']
            if row['track']==expected['intermediate_track']),
        'evidence':'本轮成功页面前缀+未修改原完整caller的必经路径推断；不是中段owner或PC命中取样',
        'runtime_PC_hit_claimed':False,'intermediate_owner_observed':False,'acoustic_identity_validated':False}


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or output.is_relative_to(original) or output.exists():raise ValueError('音乐验收需要新的隔离产物目录')
    before=fingerprint(original);prepared=prepare(root,args.scenario)
    _,_,audio_sources=original_music.sources(original)
    if fingerprint(original)!=before:raise ValueError('音乐准备期间只读原源变化')
    candidates=wav_candidates(args.wav_evidence,prepared,before)
    binaries=[path.resolve() for path in (args.driver,args.dosbox,args.sdl) if path is not None]
    symbol_identity=None
    if not args.original_only and args.sdl is not None:
        binaries.extend([args.cdb.resolve(),args.pdb.resolve(),args.cdb.resolve().parent/'dbghelp.dll'])
        symbol_identity=cdb_protocol.symbols(args.sdl,args.pdb,args.cdb.resolve().parent/'dbghelp.dll')
    inputs,frozen,sources=freeze(root,original,binaries);inputs.update(candidates['sealed_files'])
    if frozen!=before:raise ValueError('音乐窗口前冻结来源变化')
    output.mkdir(parents=True);fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded'])
    if decode_save(fixture)!=prepared['plain']:raise ValueError('音乐完整22987B夹具往返不同')
    write(output/'fixture-manifest.json',prepared['manifest']);write(output/'expected-model-not-measured.json',prepared['after'])
    roi=output/'source-rois';roi.mkdir()
    for name,rows in prepared['source_rois'].items():
        for index,(image,xy,role) in enumerate(rows):image.save(roi/(name+'-'+str(index)+'-'+role+'.png'))
    for file in (fixture,output/'fixture-manifest.json',output/'expected-model-not-measured.json',*roi.glob('*.png')):
        inputs[str(file)]=sha(file)
    report={'schema':'fd2-g3-audio-runtime-check-v1','ok':False,'probe_only':bool(args.original_only),
        'original_case_passed':False,'status':'ready_not_executed','scenario':args.scenario,
        'steps':prepared['steps'],'expected_music':prepared['expected_music'],'audio_sources':audio_sources,
        'runtime_protocol':prepared['runtime_protocol'],'prior_WAV_candidates':candidates,
        'inputs':inputs,'original_before':before,'source_files':sources,'symbol_identity':symbol_identity,
        'cases':[],'display_requirement':{},'new_recording_started':False,
        'manual_post_Save_claimed':False,'full_frame_or_waveform_or_complete_RNG_equivalence_claimed':False,
        'music_device_output_or_completed_loops_or_timbre_validated':False}
    if args.prepare_only:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        write(output/'comparison.json',report);return report
    owned_job_required()
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in ('original',) if args.original_only else ('original','sdl'):
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_TEST_AUDIO_QUIET'):
                    env.pop(key,None)
                for key in tuple(env):
                    if key.startswith('FD2SDL_APP_PCM_CAPTURE'):env.pop(key)
                if kind=='original':game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV'
                else:save=work/'FD2.SAV'
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[],'rpc':{},'cdb_music':{},
                    'initial_Save':{'path':str(save),'size':len(guard[0]),'sha256':sha(save),'mtime_ns':guard[1]}}
                report['cases'].append(case)
                rpc_context=snapshot.OwnedRPC(case['rpc']) if kind=='original' else nullcontext()
                try:
                    with rpc_context as rpc:
                        if kind=='original':
                            config=work/'dosbox.conf'
                            config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                                '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'mcp_server={rpc.port}\n'
                                '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                                '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                            inputs[str(config)]=sha(config)
                            for filename in ('FD2.EXE','FDOTHER.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT','FDMUS.DAT'):
                                inputs[str(game/filename)]=sha(game/filename)
                            command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                        else:env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                        if not unchanged(inputs,original,before,root,sources):raise ValueError('音乐首窗口前来源变化')
                        with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                            try:
                                if rpc is not None:rpc.bind(process)
                                capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                                exercise(capture,prepared,save,guard,case,rpc,work,args)
                            except BaseException as error:
                                case.setdefault('first_failure',{'checkpoint':prepared['measurement_checkpoint'],
                                    'error':{'type':type(error).__name__,'message':str(error)}})
                                raise
                    case['ok']=True
                finally:
                    case['final_Save']={'path':str(save),'size':save.stat().st_size,'sha256':sha(save),'mtime_ns':save.stat().st_mtime_ns}
                    case['guards']={'sources_unchanged':unchanged(inputs,original,before,root,sources),
                        'whole_Save_and_mtime_unchanged':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'normal_owned_cleanup':len(case['processes'])==1 and all(row.get('cleaned') is True and
                            row.get('forced_cleanup') is False and row.get('exit_code')==0 for row in case['processes']),
                        'observer_closed':case['rpc'].get('closed') is True if kind=='original' else
                            case['cdb_music'].get('detached') is True and case['cdb_music'].get('closed') is True}
                    case['ok'] &= all(case['guards'].values());write(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('音乐端点门未通过：'+kind)
                if kind=='original':report['original_case_passed']=True
                for file in work.rglob('*'):
                    if file.is_file() and (file.parent.name=='music-observer' or file.name.startswith('music-') or
                        file.parent==work and (file.suffix=='.png' or file.name in ('input-trace.jsonl','input-delivery.jsonl'))):
                        inputs[str(file)]=sha(file)
            report['ok']=not args.original_only and len(report['cases'])==2 and all(row['ok'] for row in report['cases'])
            report['status']='paired_music_owner_passed' if report['ok'] else 'original_music_probe_passed_SDL_not_run'
    except BaseException as error:
        base.record_failure(report,error);report['status']='failed'
        for case in report['cases']:
            if case.get('first_failure'):write(output/case['kind']/'comparison.json',case)
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        guard=report['final_sources_unchanged'] and report['display_requirement'].get('released') is True
        report['original_case_passed'] &= guard;report['ok'] &= guard
        if not guard:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('driver','dosbox','sdl'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',choices=tuple(SCENARIOS),required=True)
    parser.add_argument('--cdb',type=Path,default=Path('C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe'))
    parser.add_argument('--pdb',type=Path)
    parser.add_argument('--wav-evidence',type=Path,help='同夹具已完成并关闭的旧G3 WAV收据，仅作混合录音候选，不继承PASS')
    parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--original-only',action='store_true')
    args=parser.parse_args(argv)
    if args.sdl is not None and args.pdb is None:args.pdb=args.sdl.with_suffix('.pdb')
    if not args.prepare_only and any(getattr(args,name) is None or not getattr(args,name).is_file() for name in ('driver','dosbox')):
        parser.error('音乐实际运行需存在的--driver/--dosbox')
    if not args.prepare_only and not args.original_only and any(getattr(args,name) is None or not getattr(args,name).is_file() for name in ('sdl','cdb','pdb')):
        parser.error('音乐双端运行需存在的--sdl/--cdb/--pdb；原端探针须显式--original-only')
    report=run(args);print(json.dumps({key:report[key] for key in ('ok','probe_only','original_case_passed','status','scenario')},ensure_ascii=False))
    return 0 if args.prepare_only or report['ok'] or args.original_only and report['original_case_passed'] else 1


if __name__=='__main__':raise SystemExit(main())
