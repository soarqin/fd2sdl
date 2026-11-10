#!/usr/bin/env python3
"""G4 Round双caller、阵营横幅及整组恢复的正式原版/SDL代表。

期望从原EXE/FDOTHER和START记录提取。控制夹具只伤两名玩家并给敌军
原flags04跳过AI，以隔离1a30b恢复；不开放新关，不以SDL终态造原版来源。
持续WGC只证明列出的资源检查点，不声明完整逐帧/墙钟或声音等价。
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import struct
import time

from PIL import Image
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze, unchanged, sha
from run_graphics_tests import decode_save
from run_original_comparison import Capture, archive_entry, decode_image, normalize, running, title_templates
from run_original_battle_animation import OwnedHelper, owned_job_required, wait_ready, series_index
from run_original_battle_menu import match_indexed, wait_state, wait_title
from run_original_end_turn import (META, UNITS, STRIDE, RoundOracle, round_assets,
    decode_lmi, empty_focus_navigation, browse_anywhere, check_saved_round)
from static_data.verify_runtime_flows import phase_program


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


def encode(plain):
    data=bytearray(plain)
    data[-4:]=sum(data[:-4]).to_bytes(4,'little')
    state=0xa5
    for i in range(len(data)):
        state=(state+0x9014)&0xffff
        state=((state<<3)|(state>>13))&0xffff
        data[i]^=state&255
    return bytes(data)


def fixture(original):
    source=original/'START.SAV';before=decode_save(source);p=bytearray(before)
    if p[META:META+3] != bytes((1,12,0)):
        raise ValueError('G4代表需要原START stage0/turn1/12人')
    expected={}
    for raw in range(p[META+1]):
        at=UNITS+raw*STRIDE
        if p[at+6]!=2: p[at+5]|=4
    for raw,damage in ((0,10),(1,5)):
        at=UNITS+raw*STRIDE
        hp,maximum=struct.unpack_from('<HH',p,at+0x40)
        if p[at+6]!=2 or p[at+5]&0x81 or hp!=maximum or hp<=damage or any(p[at+0x22:at+0x28]):
            raise ValueError('G4恢复代表原玩家身份/HP/状态不同')
        hp-=damage;struct.pack_into('<H',p,at+0x40,hp)
        expected[str(raw)]={'before':hp,'maximum':maximum,'after':min(maximum,hp+maximum//5)}
    changes=[{'offset':hex(i),'before':a,'fixture':b} for i,(a,b) in enumerate(zip(before,p)) if a!=b]
    p[-4:]=sum(p[:-4]).to_bytes(4,'little')
    return bytes(p),encode(p),{'source':str(source),'source_sha256':sha(source),
        'controlled_changes_before_checksum':changes,'recovery':expected,
        'source_semantics':['1a30b flags04 AI gate/whole-group HP=maxHP/5','13536 full raw flags&7f'],
        'AI_RNG_or_complete_audio_claimed':False}


def frames(original):
    raw=archive_entry((original/'FDOTHER.DAT').read_bytes(),5)
    palette=[x<<2 for x in archive_entry((original/'FDOTHER.DAT').read_bytes(),0)]
    def frame(index,lmi=True):
        lo,hi=struct.unpack_from('<II',raw,6+4*index)
        image=(decode_lmi if lmi else decode_image)(raw[lo:hi]);image.putpalette(palette);return image
    def banner(turn):
        image=frame(91)
        for digit,x in zip(map(int,f'{turn:03d}'),(51,57,63)):
            number=frame(42+digit,False)
            image.paste(number,(x,7),number.point(lambda v:255 if v else 0,mode='L'))
        return image
    return banner(1),banner(2),{i:frame(i) for i in (80,81,82)}


@contextmanager
def series(capture,name,record):
    directory=capture.artifacts/name
    helper_record={};record['helper']=helper_record
    helper=OwnedHelper(capture.driver,[capture.driver,'capture-series',capture.process.pid,
        directory,120000,32],capture.artifacts,name+'-helper',helper_record)
    process=helper.start()
    ready=None
    try:
        ready=wait_ready(directory/'ready.json',process,capture.process)
        if ready.get('pid')!=capture.process.pid or not ready.get('hwnd'):
            raise AssertionError('G4 WGC ready owner错误')
        record['ready']=ready
        yield
    finally:
        (directory/'stop').touch()
        summary=helper.finish()
        if ready is not None:
            record['frames']=series_index(directory,summary,ready,capture.process.pid)
            record['summary']=summary


def banner_evidence(record,template,turn):
    hits=[]
    for row in record['frames']:
        with Image.open(row['path']) as image:actual=normalize(image)
        if match_indexed(actual,template,(120,84))['ok']:
            hits.append({'index':row['index'],'sha256':row['sha256'],'timestamp_ms':row['timestamp_ms']})
    if not hits:raise AssertionError(f'持续WGC没有完整Round {turn:03d}资源检查点')
    record['round_evidence']={'turn':turn,'frame91_and_three_digits':True,'hits':hits,
        'whole_frame_or_exact_timing_claimed':False}


def phase_evidence(original,record,templates):
    exe=(original/'FD2.EXE').read_bytes();evidence={}
    for enemy,label in ((True,82),(False,80)):
        # 坐标/位移从独立原指令提取器取得，不读取生产runtime.json。
        positions=set()
        for op,args in phase_program(exe,enemy):
            if op=='phase_slide':positions.add((args[1]-args[0],args[2]+args[0],args[3]))
        hits=[]
        for row in record['frames']:
            with Image.open(row['path']) as image:actual=normalize(image)
            for left,right,y in positions:
                a,b=templates[label],templates[81]
                if min(left,right,y)<0 or left+a.width>320 or right+b.width>320 or y+max(a.height,b.height)>200:
                    continue
                if match_indexed(actual,a,(left,y))['ok'] and match_indexed(actual,b,(right,y))['ok']:
                    hits.append({'index':row['index'],'sha256':row['sha256'],'positions':[left,right,y]});break
        if not hits:raise AssertionError('持续WGC没有完整阵营标签'+str(label)+'与公共81资源')
        evidence[str(label)]=hits
    if min(x['index'] for x in evidence['82'])>=min(x['index'] for x in evidence['80']):
        raise AssertionError('enemy/player横幅来源次序错误')
    if min(x['index'] for x in evidence['80'])>=min(x['index'] for x in record['round_evidence']['hits']):
        raise AssertionError('Round早于玩家阵营横幅')
    record['phase_evidence']=evidence


def exercise(capture,original,plain,manifest,save,record):
    assets=round_assets(original);one,two,phase=frames(original)
    titles=title_templates(original/'FDOTHER.DAT');checks=record['checkpoints']
    wait_title(capture,titles[3][0]);capture.key('UP');capture.wait(titles[3][2])
    cx,cy,fx,fy=plain[META+3:META+7]
    oracle=RoundOracle(assets,(4+(fx-cx)*24,4+(fy-cy)*24))
    record['load_series']={}
    with series(capture,'load-series',record['load_series']):
        capture.key('ENTER')
        wait_state(capture,'load-browse',oracle.initial_browse,checks,timeout=30)
    banner_evidence(record['load_series'],one,1)
    for n,(key,position) in enumerate(empty_focus_navigation(plain)):
        capture.key(key);oracle=RoundOracle(assets,position)
        wait_state(capture,'empty-step-'+str(n),oracle.browse,checks)
    capture.key('ENTER');wait_state(capture,'parent',lambda f:oracle.menu(f,0),checks)
    capture.key('DOWN');wait_state(capture,'end-selected',lambda f:oracle.menu(f,3),checks)
    capture.key('ENTER');wait_state(capture,'end-yes',lambda f:oracle.buttons(f,0),checks)
    record['end_series']={}
    with series(capture,'end-series',record['end_series']):
        capture.key('ENTER')
        wait_state(capture,'new-player-browse',lambda f:browse_anywhere(f,assets,leader=True),checks,timeout=60)
    banner_evidence(record['end_series'],two,2)
    phase_evidence(original,record['end_series'],phase)
    # 唯一一次END后从原leader左格正式Save，不用模型写回运行进程。
    capture.key('LEFT');_,detail=wait_state(capture,'save-cell',lambda f:browse_anywhere(f,assets),checks)
    oracle=RoundOracle(assets,tuple(detail['focus_screen']))
    capture.key('ENTER');wait_state(capture,'save-parent',lambda f:oracle.menu(f,0),checks)
    capture.key('UP');capture.key('ENTER')
    wait_state(capture,'secondary',lambda f:oracle.secondary(f,0,overview_available=True),checks)
    capture.key('LEFT');wait_state(capture,'save-selected',lambda f:oracle.secondary(f,1,overview_available=True),checks)
    capture.key('ENTER');wait_state(capture,'save-yes',lambda f:oracle.buttons(f,0),checks)
    previous=save.read_bytes();capture.key('ENTER');deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        if save.read_bytes()!=previous:
            try:after=decode_save(save);break
            except AssertionError:pass
        time.sleep(.05)
    else:raise TimeoutError('G4正式Save未产生新完整档')
    wait_state(capture,'saved-browse',oracle.closed_browse,checks)
    metadata=archive_entry((original/'FDFIELD.DAT').read_bytes(),0)
    state=check_saved_round(plain,after,len(metadata))
    observed={}
    for raw,expected in manifest['recovery'].items():
        hp=struct.unpack_from('<H',after,UNITS+int(raw)*STRIDE+0x40)[0]
        if hp!=expected['after']:raise AssertionError('G4整组恢复HP不等原maxHP/5：raw'+raw)
        observed[raw]=hp
    record['state']={'round':state,'recovered_HP':observed,'save_sha256':sha(save)}


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or original in output.parents or output.exists():raise ValueError('产物必须是原资源外的新目录')
    plain,encoded,manifest=fixture(original);output.mkdir(parents=True)
    write(output/'fixture.json',manifest);(output/'fixture.SAV').write_bytes(encoded)
    fixture_sha=sha(output/'fixture.SAV')
    report={'ok':False,'scope':'G4 one controlled group recovery; real Round Load and new-turn callers',
        'fixture':manifest,'cases':[],'display_requirement':{},'status':'prepared_only'}
    if not args.run:write(output/'comparison.json',report);return report
    owned_job_required();inputs,before,sources=freeze(root,original,[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()])
    inputs[str(output/'fixture.SAV')]=fixture_sha
    inputs[str(output/'fixture.json')]=sha(output/'fixture.json')
    kind=getattr(args,'kind','pair')
    kinds=('original','sdl') if kind=='pair' else (kind,)
    report['requested_endpoints']=list(kinds)
    report['paired_result']=False
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in kinds:
                work=output/kind;work.mkdir();env=os.environ.copy()
                for name in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(name,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                    inputs[str(config)]=sha(config)
                    for path in game.rglob('*'):
                        if path.is_file() and path.resolve()!=save.resolve() and path.relative_to(game).as_posix().casefold()!='fd2.tmp':
                            inputs[str(path)]=sha(path)
                else:save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(encoded);case={'kind':kind,'ok':False,'inputs':[],'checkpoints':[],'processes':[]};report['cases'].append(case)
                if kind=='original':
                    cache=game/'FD2.TMP'
                    case['mutable_cache']={'path':str(cache),'source':'docs/formats/data-formats.md §3.3; 4622d rebuild/53b6f load',
                        'before_sha256':sha(cache),'before_size':cache.stat().st_size,'expected_size':0x32a00,
                        'included_in_readonly_original_guard':True,'excluded_from_isolated_static_inputs':True}
                try:
                    if not unchanged(inputs,original,before,root,sources):raise ValueError('G4首窗口前来源改变')
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                        exercise(capture,original,plain,manifest,save,case)
                    case['ok']=True
                finally:
                    if kind=='original':
                        case['mutable_cache'].update(after_sha256=sha(cache),after_size=cache.stat().st_size,
                            owned_copy_only=cache.resolve().is_relative_to(game.resolve()) and not cache.resolve().is_relative_to(original))
                    case['sources_unchanged']=unchanged(inputs,original,before,root,sources)
                    case['ok'] &= case['sources_unchanged'] and len(case['processes'])==1 and all(
                        p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in case['processes'])
                    case['saved_bytes_unchanged_after_cleanup']=bool(case.get('state')) and sha(save)==case['state']['save_sha256']
                    case['ok'] &= case['saved_bytes_unchanged_after_cleanup']
                    if kind=='original':case['ok'] &= case['mutable_cache']['owned_copy_only'] and cache.stat().st_size==0x32a00
                    write(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('G4端点未通过：'+kind)
        report['ok']=len(report['cases'])==len(kinds) and all(x['ok'] for x in report['cases'])
        report['paired_result']=report['ok'] and len(kinds)==2
        report['status']=('passed' if len(kinds)==2 else 'endpoint_passed') if report['ok'] else 'failed'
    except BaseException as error:
        report['status']='failed';report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['sources_unchanged']=unchanged(inputs,original,before,root,sources)
        report['frozen_inputs']=inputs
        report['readonly_original_identity']=before
        report['ok'] &= report['sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):p.add_argument('--'+name,type=Path)
    p.add_argument('--run',action='store_true')
    p.add_argument('--kind',choices=('pair','original','sdl'),default='pair',
        help='单端只登记该端实际结果；不会冒称已完成双端配对')
    a=p.parse_args()
    if a.run and any(getattr(a,n) is None or not getattr(a,n).is_file() for n in ('sdl','driver','dosbox')):
        p.error('--run需已存在的sdl/driver/dosbox')
    result=run(a);print(json.dumps({k:result[k] for k in ('ok','status')},ensure_ascii=False))
    return 0 if not a.run or result['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
