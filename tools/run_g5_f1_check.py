#!/usr/bin/env python3
"""G5 F1两尺寸域：shipping已存entry，或3槽隔离map7 view alias；默认只准备。

复用原F1完整64000像素/DAC锁定门、owned WGC连续帧与现App正式CONTINUE/F1。
只投F1/PageUp与关闭键，不走移动/END/故事/保存，不放开shipping晚关门。
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import sys
from types import SimpleNamespace

from PIL import ImageChops
import fd2_g5_f1_oracle as model
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze, unchanged, sha
from run_g4_detail_check import detail_series, series_checkpoint
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import match_indexed, wait_title
from run_original_comparison import Capture, running, title_templates, driver_call, TOLERANCE, compare
from run_original_end_turn import META, RoundOracle
from run_original_f1 import FullFrameOracle
from run_original_load import require_source_unchanged
from run_original_reward import load_round_template, load_round_state


def write(path,value):
    Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def exact_rect(actual,template,position):
    x,y=position
    if actual.size!=(320,200) or x<0 or y<0 or x+template.width>320 or y+template.height>200:
        raise ValueError('F1返回tile ROI越界')
    difference=ImageChops.difference(actual.crop((x,y,x+template.width,y+template.height)).convert('RGB'),template.convert('RGB'))
    bad=sum(max(pixel)>TOLERANCE for pixel in difference.getdata())
    return {'ok':bad==0,'checked_pixels':template.width*template.height,'bad_pixels':bad,
        'black_and_opaque_zero_included':True,'max_channel_delta':max(high for _,high in difference.getextrema())}


def browse_state(frame,prepared):
    assets=prepared['assets'];at=prepared['focus'];oracle=RoundOracle(assets,at)
    for y in (112,125,138,151,164,177):
        if match_indexed(frame,assets['dialog_top'],(5,y))['ok']:return None
    if oracle.menu(frame) or oracle.buttons(frame):return None
    cursor=match_indexed(frame,assets['cursor'],at)
    if not cursor['ok']:
        cursor=next((found for image,offset in prepared['composites']
            if (found:=match_indexed(frame,image,(at[0],at[1]+offset)))['ok']),None)
        if cursor is None:return None
    tiles=[{'world':tile['world'],'position':tile['position'],
        **exact_rect(frame,tile['image'],tile['position'])} for tile in prepared['return_tiles']]
    if not all(tile['ok'] for tile in tiles):return None
    return {'kind':'F1_resource_proven_return_browse','focus_screen':list(at),'cursor':cursor,
        'dialog_all_phase_edges_absent':True,'full_dry_source_tiles':tiles,
        'runtime_or_RNG_state_measured':False}


def setup_resources(original,work,aliases):
    """两端都在自己的cwd资源树加载；所有声明外payload/文件保持原字节。"""
    runtime=work/'runtime';runtime.mkdir();resources=runtime/'original_game'
    shutil.copytree(original,resources)
    for name,payload in aliases.items():(resources/name).write_bytes(payload)
    facts={}
    for path in sorted(resources.rglob('*')):
        if not path.is_file():continue
        name=path.relative_to(resources).as_posix();source=original/name
        digest=sha(path)
        if name in aliases:
            if digest!=model.sha(aliases[name]):raise AssertionError('alias实际副本身份不符')
        elif not source.is_file() or source.read_bytes()!=path.read_bytes():
            raise AssertionError('alias声明外实际文件变化：'+name)
        facts[name]={'path':str(path.resolve()),'sha256':digest,'size':path.stat().st_size,
            'declared_view_alias':name in aliases,'before_fixture_install':True,
            'mutable_owned_copy':name.casefold() in ('fd2.sav','fd2.tmp')}
    # main/field_game/scene的original_game/...均为相对cwd；绝不从root原目录取alias。
    return runtime,resources,{'actual_process_cwd':str(runtime.resolve()),
        'relative_resource_prefix':'original_game','resources':facts,
        'all_non_alias_files_identical_before_fixture_install':True,
        'actual_map0_sha256':model.sha(model.dat_entries((resources/'FDFIELD.DAT').read_bytes())[0]),
        'actual_shape0_sheet_sha256':model.sha(model.dat_entries((resources/'FDSHAP.DAT').read_bytes())[0]),
        'actual_shape0_attr_sha256':model.sha(model.dat_entries((resources/'FDSHAP.DAT').read_bytes())[1])}


@contextmanager
def modal_series(capture,record,name):
    """每个完整 modal 单独连续采集，避免前一页判图积压耗尽600帧。

    沿用相同严格stop/owner/原PNG门；入页前ready，返回页命中后才停。
    """
    folder=capture.artifacts/name;folder.mkdir()
    item={'checkpoints':[]};record.setdefault('capture_series',{})[name]=item
    owner=SimpleNamespace(driver=capture.driver,process=capture.process,artifacts=folder)
    try:
        with detail_series(owner,item) as scope:yield scope,item['checkpoints']
    finally:
        record['checkpoints'].extend(item['checkpoints'])


def exercise(capture,original,prepared,save,record,expected=None):
    encoded=save.read_bytes();stamp=save.stat().st_mtime_ns;assets=prepared['assets']
    def guard():require_source_unchanged(save,encoded,stamp)
    titles=title_templates(original/'FDOTHER.DAT')
    wait_title(capture,titles[3][0]);guard();capture.key('UP');capture.wait(titles[3][2]);guard()
    turn=prepared['source'][META];banner=load_round_template(original,assets['palette'],turn)
    exit_banner=load_round_template(original,assets['palette'],turn,exiting=True)
    observations={}
    with modal_series(capture,record,'load') as (scope,points):
        capture.key('ENTER')
        series_checkpoint(capture,scope,'continued-actual-round',lambda f:load_round_state(f,banner,turn,exit_banner),points)
        guard()
        series_checkpoint(capture,scope,'continued-actual-browse',lambda f:browse_state(f,prepared),points)
        guard()
    for name,key,closing in (('F1','F1','A'),('PageUp','PAGEUP','ESC')):
        with modal_series(capture,record,name) as (scope,points):
            capture.key(key);oracle=FullFrameOracle(prepared['frames'],assets['f1_palette_cycles'])
            images={};required=None if expected is None else expected[name]['phases']
            def first(frame):
                detail=oracle.match(frame)
                if detail and (required is None or detail['pulse_phase'] in required):return detail
                return None
            image,detail=series_checkpoint(capture,scope,name+'-full-first',first,points)
            images[detail['pulse_phase']]=image;guard()
            targets=(None,) if required is None else tuple(p for p in required if p not in images)
            for target in targets:
                def distinct(frame,t=target):
                    found=oracle.match(frame,tuple(images))
                    return found if found and (t is None or found['pulse_phase']==t) else None
                image,detail=series_checkpoint(capture,scope,name+'-full-phase-'+str(target),distinct,points)
                images[detail['pulse_phase']]=image;guard()
            if len(images)<2:raise AssertionError('F1每页必须两个独立完整marker相位')
            observations[name]={'phases':list(images),'frames':images,'water_cycles':oracle.water_candidates}
            capture.key(closing)
            series_checkpoint(capture,scope,name+'-read-one-return-page',lambda f:browse_state(f,prepared),points)
            guard()
    # 停WGC后由同一owned目标正常关闭；没有HOST_W/业务补键。
    driver_call(capture.driver,'close',capture.process)
    code=capture.process.wait(timeout=8)
    if code!=0:raise AssertionError('F1正常owned关闭退出码非0')
    guard();record['host_close']={'pid':capture.process.pid,'exit_code':code,'ended_before_finally_cleanup':True}
    record['whole_22987B_Save_and_mtime_unchanged']=True
    record['observed_pages']=[{'name':name,'phases':row['phases'],'water_cycles':row['water_cycles'],
        'full_resource_pixels_per_frame':64000} for name,row in observations.items()]
    return observations


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    fixture=(args.save_fixture or original/'START.SAV').resolve()
    if output.exists() or output==original or original in output.parents or output in fixture.parents:
        raise ValueError('F1产物须原证据外的新目录')
    prepared=model.prepare(original,args.scenario,fixture,args.source_report)
    output.mkdir(parents=True);write(output/'fixture-manifest.json',prepared['manifest'])
    (output/'fixture.SAV').write_bytes(prepared['encoded'])
    for name,data in prepared['aliases'].items():(output/('alias-'+name)).write_bytes(data)
    for phase,image in enumerate(prepared['frames']):image.save(output/f'oracle-phase{phase}.png')
    report={'ok':False,'status':'prepared_only','scenario':args.scenario,'manifest':prepared['manifest'],
        'cases':[],'display_requirement':{},'paired_result':False,
        'scope':'two actual F1/PageUp pages per endpoint, each full64000 resource pixels and two marker phases; real read-one return source tiles/cursor; disk fullSAV guard',
        'excluded':['native full heat/RNG dump','absolute clocks/first undefined tick','new stage battle/END/move/story','audio/full waveform','all return pixels'],
        'shipping_binary_required':True}
    if not args.run:write(output/'comparison.json',report);return report
    owned_job_required();kinds=('original','sdl') if args.kind=='pair' else (args.kind,)
    binaries=[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve(),fixture]
    if args.source_report:binaries.append(args.source_report.resolve())
    origin=prepared['manifest'].get('source_report')
    source_files=origin.get('source_files',[]) if origin else []
    if not model.stage2_sources_unchanged(origin):raise ValueError('F1 stage2来源在预检后变化；未启动窗口')
    binaries.extend(Path(row['path']) for row in source_files)
    user=original.parent/'FD2.SAV'
    if user.is_file():binaries.append(user)
    inputs,before,sources=freeze(root,original,binaries)
    if any(inputs.get(row['path'])!=row['sha256'] for row in source_files) or not model.stage2_sources_unchanged(origin):
        raise ValueError('F1 stage2来源与窗口前冻结不一致；未启动窗口')
    for path in output.iterdir():
        if path.is_file() and path.name!='comparison.json':inputs[str(path.resolve())]=sha(path)
    native=None
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in kinds:
                work=output/kind;work.mkdir();runtime,resources,resource_receipt=setup_resources(original,work,prepared['aliases'])
                case={'kind':kind,'ok':False,'checkpoints':[],'inputs':[],'processes':[],
                    'actual_resource_objects':resource_receipt};report['cases'].append(case)
                for path in resources.rglob('*'):
                    if path.is_file() and path.name.casefold() not in ('fd2.sav','fd2.tmp'):
                        inputs[str(path.resolve())]=sha(path)
                environment=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_SAVE_PATH'):
                    environment.pop(key,None)
                if kind=='original':
                    save=resources/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(resources)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    inputs[str(config.resolve())]=sha(config);command=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'];cwd=runtime
                    case['actual_resource_objects']['DOS_mount_C']=str(resources.resolve())
                    cache=resources/'FD2.TMP'
                    case['mutable_cache']={'path':str(cache.resolve()),'before_sha256':sha(cache) if cache.is_file() else None,
                        'source':'10010/FD2.TMP load/cache contract; isolated cache only, readonly original remains guarded',
                        'expected_after_size':0x32a00,'excluded_from_isolated_static_guard':True}
                else:
                    save=runtime/'FD2.SAV';environment['FD2SDL_SAVE_PATH']=str(save.resolve())
                    command=[str(args.sdl.resolve())];cwd=runtime
                shutil.copy2(fixture,save);saved=save.read_bytes();stamp=save.stat().st_mtime_ns
                case['actual_active_Save']={'path':str(save.resolve()),'source':str(fixture),
                    'sha256':sha(save),'mtime_ns':stamp,'metadata_units_RNG_not_patched':True}
                try:
                    if not unchanged(inputs,original,before,root,sources) or not model.stage2_sources_unchanged(origin):raise ValueError('F1首窗口前来源变化')
                    with running(command,cwd,environment,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        case['pid']=process.pid;case['command']=command;case['cwd']=str(cwd.resolve())
                        capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                        observed=exercise(capture,original,prepared,save,case,native)
                    require_source_unchanged(save,saved,stamp)
                    if kind=='original':native=observed
                    elif native is not None:
                        pairs=[]
                        for name,row in native.items():
                            actual=observed[name]
                            if set(row['phases'])!=set(actual['phases']):raise AssertionError('F1双端相位集合不符')
                            for phase,image in row['frames'].items():
                                paired=compare(image,actual['frames'][phase])
                                pairs.append({'page':name,'phase':phase,'checked_pixels':64000,**paired})
                        case['raw_RGB_differences']=pairs
                        # 原已有：两端入页前合法水DAC可不同；两端独立完整资源门为验收，原差异原样登记。
                        case['raw_RGB_equality_is_acceptance_gate']=False
                    case['ok']=True
                except BaseException as error:
                    case['error']={'type':type(error).__name__,'message':str(error)}
                    raise
                finally:
                    case['sources_unchanged']=unchanged(inputs,original,before,root,sources) and model.stage2_sources_unchanged(origin)
                    case['ok'] &= case['sources_unchanged'] and len(case['processes'])==1 and all(
                        p.get('cleaned') is True and not p.get('forced_cleanup') and p.get('exit_code')==0 for p in case['processes'])
                    case['whole_Save_and_mtime_after_cleanup']=save.read_bytes()==saved and save.stat().st_mtime_ns==stamp
                    case['ok'] &= case['whole_Save_and_mtime_after_cleanup']
                    if kind=='original':
                        cache_ok=cache.is_file() and cache.stat().st_size==0x32a00 and \
                            cache.resolve().is_relative_to(resources.resolve()) and not cache.resolve().is_relative_to(original)
                        case['mutable_cache'].update(after_sha256=sha(cache) if cache.is_file() else None,
                            after_size=cache.stat().st_size if cache.is_file() else None,owned_copy_only=cache_ok)
                        case['ok'] &= cache_ok
                    write(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('F1端点未通过：'+kind)
        report['endpoint_passed']=len(report['cases'])==len(kinds) and all(c['ok'] for c in report['cases'])
        report['paired_result']=report['endpoint_passed'] and len(kinds)==2
        report['ok']=report['paired_result'];report['status']='passed' if report['ok'] else 'endpoint_passed'
    except BaseException as error:
        report['status']='failed';report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['sources_unchanged']=unchanged(inputs,original,before,root,sources) and model.stage2_sources_unchanged(origin)
        report['frozen_inputs']=inputs;report['readonly_original_identity']=before
        valid=report['sources_unchanged'] and report['display_requirement'].get('released') is True
        report['ok'] &= valid
        if not valid:report['status']='failed';report['endpoint_passed']=False
        write(output/'comparison.json',report)
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox','save-fixture','source-report'):p.add_argument('--'+name,type=Path)
    p.add_argument('--scenario',choices=model.SCENARIOS,required=True)
    p.add_argument('--run',action='store_true');p.add_argument('--kind',choices=('pair','original','sdl'),default='pair')
    args=p.parse_args()
    if args.run and (sys.platform!='win32' or any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('sdl','driver','dosbox'))):
        p.error('--run要求Windows及现有sdl/driver/dosbox；未启动窗口')
    try:report=run(args)
    except Exception as error:p.exit(1,'F1前置失败：'+str(error)+'；未启动窗口。\n')
    print(json.dumps({k:report.get(k) for k in ('ok','status','paired_result')},ensure_ascii=False))
    return 0 if not args.run or report.get('ok') or args.kind!='pair' and report.get('endpoint_passed') else 1


if __name__=='__main__':raise SystemExit(main())
