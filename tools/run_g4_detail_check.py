#!/usr/bin/env python3
"""G4正式F2/acted Enter详情的两次16c57(0)等待；默认仅准备夹具。

原资源独立构造整80×80 portrait0/3及两页完整310×99底板；各页必须实际
见0→3→0后才发送一个键。F2以正式UI Save量测整SAV，acted Enter保留原
Save禁用门及整档mtime。controller恰一次由私有真实Core测试验证，本工具
不把截图/不变磁盘冒称执行PC计数、完整热owner或RNG序列量测。
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import time

from PIL import Image

from fd2_display_requirement import windows_display_requirement
from audit_g4_render_source import archive_entries
from fd2_item_list_oracle import inventory_templates
from fd2_magic_list_oracle import prepare as magic_templates
from fd2_opening_oracle import portraits
from run_g2_battle_check import freeze, unchanged, sha
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_battle_animation import OwnedHelper, owned_job_required, wait_ready, series_index, latest_series_frame
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import Capture, archive_entry, normalize, running, title_templates
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets, decode_lmi
from run_original_load import focused_actor, check_load_measurement, require_source_unchanged, wait_measured_save
from run_original_magic_list import actor_focus_resources, detail_close_terrain
from run_original_player import exact_tile
from static_data.verify_unit_tables import source_range

SCENARIOS=('f2-magic','enter-acted-magic')
START_SHA='2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310'
EXE_SHA='bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
DATO_SHA='dff859783606ee95d9811e1e564bcb230ef53c4df9130e8b6d42ad37bc8c27bd'
PROGRAMS={0x17aed:(0x17d6f,'fdabaa88e2e33df5cb9b8040e9b8b7d3caabf3bbf3c9fba30537968801f471c5'),
          0x16c57:(0x16e24,'9ee45fd0d376c2f05cedeff838be5f13f7fcc8724000b152f1ad17f44c5b865e')}


def write(path,value):
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def source_contract(original):
    exe=(original/'FD2.EXE').read_bytes()
    if hashlib.sha256(exe).hexdigest()!=EXE_SHA:raise ValueError('详情EXE不是已核原源')
    for start,(end,digest) in PROGRAMS.items():
        if hashlib.sha256(source_range(exe,start,end-start)).hexdigest()!=digest:
            raise ValueError('详情完整原函数身份不同：'+hex(start))
    calls=[]
    for at,target in ((0x17afc,0x17e0b),(0x17b06,0x16c57),(0x17b14,0x1c269),
            (0x17c44,0x1ceed),(0x17ce6,0x16c57),(0x17d05,0x18409)):
        raw=source_range(exe,at,5)
        if raw[0]!=0xe8 or at+5+struct.unpack_from('<i',raw,1)[0]!=target:
            raise ValueError('详情原调用锚不同：'+hex(at))
        calls.append({'call_cva':hex(at),'target_cva':hex(target)})
    for at in (0x17b04,0x17ce4):
        if source_range(exe,at,2)!=bytes.fromhex('6a00'):raise ValueError('详情wait参数不再是0')
    return {'EXE_sha256':EXE_SHA,'complete_functions':{hex(k):{'size':v[0]-k,'sha256':v[1]} for k,v in PROGRAMS.items()},
        'call_anchors':calls,'wait_parameter':0,'source_view':'raw initialized LE; canonical relbase VA',
        'native_PC_hits_claimed':False}


def portrait_domain(original):
    """严格原DATO全136项，不因新frame3门缩小旧合法详情域。"""
    data=(original/'DATO.DAT').read_bytes()
    if hashlib.sha256(data).hexdigest()!=DATO_SHA:raise ValueError('详情DATO身份不同')
    entries=archive_entries(data);count=len(entries)
    if count!=136:raise ValueError('详情DATO目录不是原136项')
    rows=[]
    for identity in range(count):
        entry=entries[identity];first=struct.unpack_from('<I',entry)[0]
        frames=first//4
        if first%4 or frames<4 or first>len(entry):raise ValueError('DATO原帧目录截断')
        for frame in (0,3):
            start=struct.unpack_from('<I',entry,frame*4)[0]
            end=struct.unpack_from('<I',entry,(frame+1)*4)[0] if frame+1<frames else len(entry)
            if not first<=start<end<=len(entry):raise ValueError('DATO 0/3原帧越界')
            image=decode_lmi(entry[start:end])
            if image.size!=(80,80):raise ValueError('DATO 0/3原帧不是80×80')
            rows.append({'portrait':identity,'frame':frame,'span':[start,end],
                'decoded_pixels_sha256':hashlib.sha256(image.tobytes()).hexdigest()})
    return {'DATO_sha256':DATO_SHA,'portraits':136,'frames':[0,3],'checked_images':len(rows),
        'all_dimensions':[80,80],'images':rows,'runtime_execution_claimed':False}


def prepare(original,scenario):
    if scenario not in SCENARIOS:raise ValueError('未知详情代表')
    source=original/'START.SAV'
    if sha(source)!=START_SHA:raise ValueError('详情代表要求原START')
    before=decode_save(source);plain=bytearray(before)
    actor=plain[UNITS+STRIDE:UNITS+2*STRIDE]
    if plain[META:META+3]!=bytes((1,12,0)) or actor[7]!=9 or actor[5]&0x85 or actor[0x1a:0x1f]!=bytes((1,0,0,0,0)):
        raise ValueError('详情raw1身份/原magic0/未acted前置不同')
    cx,cy=plain[META+3:META+5]
    plain[META+5:META+9]=bytes((actor[0],actor[1],actor[0]-cx,actor[1]-cy))
    allowed=set(range(META+5,META+9))
    if scenario.startswith('enter'):
        plain[UNITS+STRIDE+5]|=0x80;allowed.add(UNITS+STRIDE+5)
    changes=[{'offset':hex(i),'before':a,'fixture':b} for i,(a,b) in enumerate(zip(before[:-4],plain[:-4])) if a!=b]
    if any(int(row['offset'],16) not in allowed for row in changes):raise ValueError('详情夹具改出焦点/acted声明')
    plain[-4:]=sum(plain[:-4]).to_bytes(4,'little');plain=bytes(plain)
    assets=round_assets(original);actor=plain[UNITS+STRIDE:UNITS+2*STRIDE]
    _,equipment,fields=inventory_templates(original,actor,assets)
    magic,_=magic_templates(original,source,source=plain)
    images=portraits(original,9,assets['palette'],False)
    if any(set(image.getdata())&set(range(224,240)) for image in images):
        raise ValueError('详情portrait包含动态水色，不能用固定RGB oracle')
    terrain=detail_close_terrain(original,plain,assets)
    focus=actor_focus_resources(original,plain,assets,1,assets['cursor'])
    return {'plain':plain,'encoded':encode(plain),'assets':assets,'focus':focus,'terrain':terrain,
        'portraits':images,'equipment':equipment,'magic':magic,
        'manifest':{'source':str(source),'source_sha256':START_SHA,'scenario':scenario,
            'controlled_changes_before_checksum':changes,'raw_actor':1,'unit_id':9,'magic_ids':[0],
            'natural_progression_claimed':False,'native_controller_PC_count_claimed':False,
            'controller_once_evidence':'field_detail_flow private real-Core Enter/F2 route test; not this WGC run',
            'inputs':['F2' if scenario=='f2-magic' else 'ENTER','UP','ESC'],
            'portrait_ROI':[8,10,80,80],'bottom_ROI':[5,94,310,99],
            'full_inventory':fields['expected_inventory_hex'],
            'measurement':'formal UI Save' if scenario=='f2-magic' else 'original acted Save gate stays disabled; whole disk/mTime guard'}}


def detail_state(frame,prepared,page,phase):
    if page not in ('equipment','magic') or phase not in (0,3):raise ValueError('详情资源检查参数无效')
    templates=prepared[page];checks={}
    for name in ('unselected','actor_name','current_max_mp'):
        at,image=templates[name];found=exact_tile(frame,image,at)
        if found is None:return None
        checks[name]={'ROI':[*at,*image.size],**found}
    portrait=exact_tile(frame,prepared['portraits'][phase==3],(8,10))
    if portrait is None:return None
    return {'kind':'detail_wait_resources','page':page,'portrait_frame':phase,
        'portrait':{'ROI':[8,10,80,80],**portrait},'rectangles':checks,
        'full_portrait_including_index0':True,'no_arrow_full_bottom_checked':True,
        'runtime_RNG_count_measured':False,'native_PC_hit_claimed':False}


@contextmanager
def detail_series(capture,record):
    """一个modal只用一个持续WGC；异常也先stop/清helper，再退出游戏。"""
    directory=capture.artifacts/'detail-series';helper_record={}
    output={'helper':helper_record,'requested_interval_ms':33,'maximum_ms':45000,
        'capture_backend':'Windows.Graphics.Capture HWND','sampling_scope':'whole detail modal; no separate single-frame captures'}
    record['detail_series']=output
    helper=OwnedHelper(capture.driver,[capture.driver,'capture-series',capture.process.pid,
        directory,45000,33],capture.artifacts,'detail-series-helper',helper_record)
    ready=None;first_error=None
    try:
        process=helper.start();ready=wait_ready(directory/'ready.json',process,capture.process)
        if ready.get('pid')!=capture.process.pid or not ready.get('hwnd'):raise ValueError('详情持续WGC ready owner不同')
        output['ready']=ready
        yield {'directory':directory,'ready':ready,'process':process,'after':None}
    except BaseException as error:
        first_error=error
        output['error']=str(error)
        raise
    finally:
        try:
            if directory.is_dir():(directory/'stop').touch()
            summary=helper.finish()
            if ready is not None:
                frames=series_index(directory,summary,ready,capture.process.pid)
                output.update(summary=summary,frames=frames)
                for point in record['checkpoints']:
                    source=point.get('series_frame')
                    if source is None:continue
                    index=source['index']
                    if index>=len(frames) or any(source.get(k)!=frames[index].get(k)
                        for k in ('pid','hwnd','path','sha256','wgc_timestamp_100ns')):
                        raise AssertionError('详情检查点不是本轮关闭series中同一原PNG')
                    if sha(point['normalized_PNG']['path'])!=point['normalized_PNG']['sha256']:
                        raise AssertionError('详情归一检查图在采集期间改变')
                output['checkpoint_PNGs_bound_to_closed_series']=True
        except BaseException as error:
            output['cleanup_error']=str(error)
            if first_error is None:raise


def series_checkpoint(capture,scope,name,predicate,points,timeout=45):
    """读取已发布PNG，不另建WGC；逐个消费积压帧，不让Python判图跳过110ms闭眼。"""
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        latest=latest_series_frame(scope['directory'],scope['ready'],capture.process.pid,scope['after'])
        if latest is not None:
            last,_=latest
            complete=[line for line in (scope['directory']/'frames.jsonl').read_bytes().splitlines(keepends=True) if line.endswith(b'\n')]
            first=scope['after']['index']+1 if scope['after'] else 0
            # latest_series_frame已校验所有行的顺序/owner；只消费它已验过的前缀。
            for index in range(first,last['index']+1):
                row=json.loads(complete[index]);stamp=row.get('wgc_timestamp_100ns')
                if row.get('index')!=index or row.get('file')!=f'frame{index:05d}.png' or\
                    row.get('pid')!=capture.process.pid or row.get('hwnd')!=scope['ready']['hwnd'] or\
                    type(stamp) is not int or stamp>last['wgc_timestamp_100ns'] or\
                    scope['after'] is not None and stamp<=scope['after']['wgc_timestamp_100ns']:
                    raise AssertionError('详情已发布series前缀在读取时改变owner/顺序')
                path=scope['directory']/row['file'];raw=path.read_bytes()
                with Image.open(io.BytesIO(raw)) as image:
                    if image.format!='PNG' or image.size!=(row['client_width'],row['client_height']):
                        raise AssertionError('详情series原PNG尺寸/格式不同')
                    frame=normalize(image)
                source={**row,'path':str(path),'sha256':hashlib.sha256(raw).hexdigest()}
                scope['after']=source;capture.last=frame
                detail=predicate(frame)
                if detail:
                    output=capture.artifacts/(name+'.png');frame.save(output)
                    points.append({'name':name,'pid':capture.process.pid,'time':time.monotonic(),**detail,
                        'capture_backend':'Windows.Graphics.Capture HWND','series_frame':source,
                        'normalized_PNG':{'path':str(output),'sha256':sha(output)},'no_additional_WGC_session':True})
                    return frame,detail
        if capture.process.poll() is not None:raise RuntimeError('详情目标在检查点前退出：'+name)
        if scope['process'].poll() is not None:raise RuntimeError('详情WGC helper在显式stop前结束：'+name)
        time.sleep(.01)
    if capture.last is not None:capture.last.save(capture.artifacts/(name+'-timeout.png'))
    raise TimeoutError('等待详情持续WGC检查点超时：'+name+'；未发送额外按键')


def exercise(capture,original,prepared,scenario,save,record):
    plain=prepared['plain'];assets=prepared['assets'];world=list(plain[META+5:META+7]);cx,cy=plain[META+3:META+5]
    saved=save.read_bytes();stamp=save.stat().st_mtime_ns
    def guard():require_source_unchanged(save,saved,stamp)
    def checkpoint(name,predicate,timeout=20):
        result=wait_state(capture,name,predicate,record['checkpoints'],timeout=timeout);guard();return result
    at=(4+(world[0]-cx)*24,4+(world[1]-cy)*24)
    def initial(frame):
        found=focused_actor(frame,assets,prepared['focus'])
        return found if found and found['focus_screen']==list(at) else None
    def closed(frame):
        found=initial(frame);terrain=prepared['terrain']
        recovered=exact_tile(frame,terrain['image'],terrain['position'])
        if not found or recovered is None:return None
        if detail_state(frame,prepared,'equipment',0) or detail_state(frame,prepared,'magic',0):return None
        return {**found,'closing_final_dry_tile':{'ROI':[*terrain['position'],24,24],**recovered}}
    titles=title_templates(original/'FDOTHER.DAT')
    wait_title(capture,titles[3][0]);capture.key('UP');capture.wait(titles[3][2]);capture.key('ENTER')
    checkpoint('continued-raw1',initial,45)
    with detail_series(capture,record) as scope:
        capture.key('F2' if scenario=='f2-magic' else 'ENTER')
        for page,key in (('equipment','UP'),('magic','ESC')):
            # 至少见一次真实闭眼及随后睁眼，不能将动画首帧当等待成立。
            for label,phase in (('open',0),('blink',3),('restored',0)):
                series_checkpoint(capture,scope,page+'-'+label,
                    lambda f,p=page,n=phase:detail_state(f,prepared,p,n),record['checkpoints'])
                guard()
            capture.key(key)
        series_checkpoint(capture,scope,'details-closed-after-all-12-phases',closed,record['checkpoints'])
        guard()
    # 一格左移到(9,15)空地，固定camera，随后只走正式SYSTEM。
    world[0]-=1;capture.key('LEFT');focus=(4+(world[0]-cx)*24,4+(world[1]-cy)*24)
    menu=RoundOracle(assets,focus)
    checkpoint('measurement-empty-focus',menu.closed_browse)
    capture.key('ENTER');checkpoint('measurement-parent',lambda f:menu.menu(f,0))
    capture.key('UP');checkpoint('measurement-parent-storage',lambda f:menu.menu(f,0))
    capture.key('ENTER')
    enabled=scenario=='f2-magic'
    checkpoint('measurement-secondary',lambda f:menu.secondary(f,0,save_available=enabled))
    if not enabled:
        record['acted_Save_gate_preserved']=True
        capture.key('ESC');checkpoint('acted-parent-return',lambda f:menu.menu(f,0))
        capture.key('ESC');checkpoint('acted-final-browse',menu.closed_browse)
        record['whole_Save_and_mtime_unchanged']=True
        record['full_runtime_owner_measured']=False
        return
    capture.key('LEFT');checkpoint('measurement-save-selected',lambda f:menu.secondary(f,1))
    capture.key('ENTER');checkpoint('measurement-save-yes',lambda f:menu.buttons(f,0))
    previous,saved_stamp=saved,stamp;capture.key('ENTER')
    measured=wait_measured_save(save,previous,saved_stamp);saved=save.read_bytes();stamp=save.stat().st_mtime_ns
    record['full_Save_measurement']=check_load_measurement(plain,measured,[('explicit-left-to-empty',focus)])
    checkpoint('measurement-saved-browse',menu.closed_browse)
    record['measured_Save']={'path':str(save),'sha256':sha(save),'mtime_ns':stamp,'size':len(saved)}


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output.exists() or output==original or original in output.parents:raise ValueError('详情产物需要原资源外新目录')
    selected=getattr(args,'scenario',None) or list(SCENARIOS)
    if len(set(selected))!=len(selected) or any(s not in SCENARIOS for s in selected):raise ValueError('详情场景重复或未知')
    kind=getattr(args,'kind','pair')
    if kind not in ('pair','original','sdl'):raise ValueError('未知详情端点')
    kinds=('original','sdl') if kind=='pair' else (kind,)
    contract=source_contract(original);domain=portrait_domain(original)
    prepared=[(s,prepare(original,s)) for s in selected];output.mkdir(parents=True)
    report={'ok':False,'status':'prepared_only','source_contract':contract,'portrait_domain':domain,'cases':[],
        'capture':'Windows.Graphics.Capture HWND','display_requirement':{},'requested_endpoints':list(kinds),'paired_result':False,
        'scope':'formal F2/acted Enter two-page wait/blink/resources; F2 full UI Save; acted immutable disk and disabled Save',
        'excluded':['native controller PC计数','native精确UI RNG/墙钟对齐','acted完整热owner转储','声音保真','G5全历史回归','Linux实机']}
    for scenario,p in prepared:
        (output/(scenario+'.SAV')).write_bytes(p['encoded']);write(output/(scenario+'.json'),p['manifest'])
        if decode_save(output/(scenario+'.SAV'))!=p['plain']:raise ValueError('详情夹具编码往返不同')
    if not args.run:write(output/'comparison.json',report);return report
    owned_job_required();binaries=[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()]
    user_save=original.parent/'FD2.SAV'
    if user_save.is_file():binaries.append(user_save)
    inputs,before,sources=freeze(root,original,binaries)
    for scenario,p in prepared:
        for suffix in ('.SAV','.json'):inputs[str(output/(scenario+suffix))]=sha(output/(scenario+suffix))
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in kinds:
                for scenario,p in prepared:
                    work=output/(kind+'-'+scenario);work.mkdir();env=os.environ.copy()
                    for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(key,None)
                    env['FD2SDL_TEST_AUDIO_QUIET']='1'
                    if kind=='original':
                        game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                            '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                        command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work;inputs[str(config)]=sha(config)
                        mutable={save.resolve(),(game/'FD2.TMP').resolve()}
                        for file in game.rglob('*'):
                            if file.is_file() and file.resolve() not in mutable:inputs[str(file)]=sha(file)
                    else:
                        save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                    save.write_bytes(p['encoded'])
                    case={'kind':kind,'scenario':scenario,'ok':False,'inputs':[],'checkpoints':[],'processes':[],
                        'initial_Save_sha256':sha(save),'initial_Save_mtime_ns':save.stat().st_mtime_ns}
                    report['cases'].append(case)
                    try:
                        if not unchanged(inputs,original,before,root,sources):raise ValueError('详情首窗口前来源变化')
                        with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                            capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                            exercise(capture,original,p,scenario,save,case)
                        case['ok']=True
                    except BaseException as error:
                        case['error']={'type':type(error).__name__,'message':str(error)};raise
                    finally:
                        case['sources_unchanged']=unchanged(inputs,original,before,root,sources)
                        case['ok'] &= case['sources_unchanged'] and len(case['processes'])==1 and all(
                            row.get('cleaned') is True and row.get('forced_cleanup') is False and row.get('exit_code')==0 for row in case['processes'])
                        write(work/'comparison.json',case)
                    if not case['ok']:raise AssertionError('详情端点清理或来源门失败：'+kind+'/'+scenario)
                    series=case['detail_series']
                    for frame in series['frames']:inputs[frame['path']]=frame['sha256']
                    for point in case['checkpoints']:
                        if 'normalized_PNG' in point:inputs[point['normalized_PNG']['path']]=point['normalized_PNG']['sha256']
                    for file in (work/'detail-series').glob('*.json*'):inputs[str(file)]=sha(file)
        report['ok']=len(report['cases'])==len(kinds)*len(prepared) and all(c['ok'] for c in report['cases'])
        report['paired_result']=report['ok'] and len(kinds)==2
        report['status']=('passed' if len(kinds)==2 else 'endpoint_passed') if report['ok'] else 'failed'
    except BaseException as error:
        report['status']='failed';report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['sources_unchanged']=unchanged(inputs,original,before,root,sources)
        report['frozen_inputs']=inputs;report['readonly_original_identity']=before
        report['ok'] &= report['sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('root','artifacts'):parser.add_argument('--'+key,type=Path,required=True)
    for key in ('sdl','driver','dosbox'):parser.add_argument('--'+key,type=Path)
    parser.add_argument('--kind',choices=('pair','original','sdl'),default='pair',help='单端只登记该端本轮校准，不继承旧双端结果')
    parser.add_argument('--scenario',choices=SCENARIOS,nargs='+');parser.add_argument('--run',action='store_true');args=parser.parse_args()
    if args.run and any(getattr(args,key) is None or not getattr(args,key).is_file() for key in ('sdl','driver','dosbox')):
        parser.error('--run需要实际sdl/driver/dosbox')
    try:report=run(args)
    except Exception as error:parser.exit(1,'详情验收未能启动：'+str(error)+'。检查来源或参数后重试。\n')
    print(json.dumps({'ok':report['ok'],'status':report['status']},ensure_ascii=False))
    return 0 if not args.run or report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
