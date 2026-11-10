#!/usr/bin/env python3
"""G4 browse键、设置4信息板与活动存读档的定向双端验收。

默认仅准备3个START派生机制夹具；--run才在owned Job内串行运行原版/SDL。
期望来自原EXE/资源/START，不读取SDL后态作oracle，不开放新关或改用户档。
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import time

from PIL import Image, ImageChops
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze, unchanged, sha
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import MENU_OFFSETS, match_indexed, wait_state, wait_title
from run_original_comparison import Capture, TOLERANCE, decode_image, running, title_templates
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets
from run_original_load import check_load_measurement, focused_actor, require_source_unchanged, wait_measured_save
from run_original_magic_list import actor_focus_resources
from static_data.verify_unit_tables import source_range

SCENARIOS=('browse-gates','browse-cycle','settings-panel-load')
START_SHA='2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310'
EXE_SHA='bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
# 原1acf3 frame130的完整69×34范围；OFF比较不只检查其中80像素底边。
PANEL_BOXES=((5,161,74,195),(246,161,315,195))


def write(path,value):
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def source_contract(original):
    executable=original/'FD2.EXE';raw=executable.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=EXE_SHA:raise ValueError('原FD2.EXE身份不同')
    spans={0x117f8:'83f801740e83f82c740983f84c0f8579000000',
        0x11832:'f640058575350fb6400683f802',0x1184e:'8d4301a3e93a0000',
        0x119b5:'83f8220f84e5000000',0x11aa3:'31c05e5bc3',
        0x173ba:'8035ab1a000001',
        0x19e98:'f640050175dff640058074d9c744240401000000'}
    for va,hexdata in spans.items():
        expected=bytes.fromhex(hexdata)
        if source_range(raw,va,len(expected))!=expected:raise ValueError('原控制条件/写点不同：'+hex(va))
    return {'EXE_sha256':EXE_SHA,'source_view':'raw LE initialized pages; addresses canonical, absolute operands before fixup',
        'instruction_spans':{hex(k):v for k,v in spans.items()},
        'cycle_gate':'flags&0x85==0 && side==2; raw index+1, scan full active count',
        'G':'119b5 scan22 ->11aa3 no-action return; no index write',
        'panel':'1acf3 option51aab and temporary51aac; low-left→right, low-right→left',
        'option4':'173ba XOR1; active plain30d2 raw byte',
        'save_gate':'19e98..19eab: any nonhidden actor flags80 disables Save; no side filter',
        'original_runtime_claimed':False}


def fixture(original,scenario):
    if scenario not in SCENARIOS:raise ValueError('未知G4控制场景')
    source=original/'START.SAV';before=decode_save(source)
    if sha(source)!=START_SHA or before[META:META+3]!=bytes((1,12,0)):
        raise ValueError('G4控制代表要求已核原START stage0/turn1/12人')
    plain=bytearray(before)
    # 有证、空且不被下格actor遮挡的fixture焦点，保原camera(5,11)。
    plain[META+5:META+9]=bytes((8,13,3,2))
    allowed=set(range(META+5,META+9))
    expected=[]
    if scenario=='browse-gates':
        for raw,flags in ((0,4),(1,1),(2,128)):
            plain[UNITS+raw*STRIDE+5]=flags;allowed.add(UNITS+raw*STRIDE+5)
        plain[UNITS+3*STRIDE+0x26]=1;allowed.add(UNITS+3*STRIDE+0x26)
        expected=[('G',None),('ESC',3),('KP5',3)]
    elif scenario=='browse-cycle':
        plain[UNITS+2*STRIDE+5]=4;allowed.add(UNITS+2*STRIDE+5)
        plain[UNITS+3*STRIDE+0x26]=1;allowed.add(UNITS+3*STRIDE+0x26)
        expected=[('ESC',0),('G',0),('Z',1),('KP5',3),('G',3),('Z',0)]
    else:
        if plain[META+15]!=1:raise ValueError('设置代表原option4必须是raw1')
    changes=[{'offset':hex(i),'before':a,'fixture':b} for i,(a,b) in enumerate(zip(before[:-4],plain[:-4])) if a!=b]
    if any(int(row['offset'],16) not in allowed for row in changes):raise AssertionError('夹具越出声明字段')
    plain[-4:]=sum(plain[:-4]).to_bytes(4,'little')
    return bytes(plain),encode(plain),{'source':str(source),'source_sha256':START_SHA,
        'controlled_changes_before_checksum':changes,'cycle_inputs':expected,
        'natural_progression_claimed':False,'source_camera':list(before[META+3:META+5]),
        'allowed_runtime_changes':'only explicit navigation/options; full Save comparison excludes only active+2 cache and checksum'}


def actor_templates(original,plain,assets,raw):
    actor=plain[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]
    if not actor[0x26]:return actor_focus_resources(original,plain,assets,raw,assets['cursor'])
    # 127e0原raw26：固定direction*3+0，水平0/1，保cursor不偏移。
    data=(original/'FDICON.B24').read_bytes();width,height,count=struct.unpack_from('<HHH',data)
    if (width,height,count)!=(24,24,1680):raise ValueError('FDICON目录不同')
    index=actor[7]*12+(actor[3]&3)*3
    lo=struct.unpack_from('<I',data,6+4*index)[0]
    hi=struct.unpack_from('<I',data,6+4*(index+1))[0] if index+1<count else len(data)
    if not 6+count*4<=lo<hi<=len(data):raise ValueError('raw26原帧越界')
    sprite=decode_image(struct.pack('<HH',24,24)+data[lo:hi]);sprite.putpalette(assets['palette'])
    templates=[]
    for shift in (0,1):
        image=Image.new('P',(25,30),0);image.putpalette(assets['palette'])
        image.paste(assets['cursor'],(0,6),assets['cursor'].point(lambda p:255 if p else 0,mode='L'))
        image.paste(sprite,(shift,0),sprite.point(lambda p:255 if p else 0,mode='L'))
        templates.append(image.crop((0,0,25,24)))
    return {'raw_actor':raw,'unit_id':actor[7],'world':list(actor[:2]),'templates':templates,
        'raw26_fixed_phase':0,'jitter_offsets':[0,1]}


def options(frame,oracle,values,selected):
    if len(values)!=4 or any(type(v) is not int or not 0<=v<=255 for v in values):raise ValueError('设置raw值无效')
    commands=(18+(values[0]==0),20+(values[1]==0),22+(values[2]!=0),24+(values[3]==0))
    matches=[]
    for item,command in enumerate(commands):
        at=tuple(a+b for a,b in zip(oracle.focus,MENU_OFFSETS[item]))
        found=oracle.choices(frame,(command*3+(item==selected),),at)
        if not found:return None
        matches.append(found)
    return {'kind':'settings','selected':selected,'raw_values':values,'matches':matches}


def panel_state(frame,assets,side):
    matches={name:match_indexed(frame,assets['restoration_border'],at)
        for name,at in (('left',(16,190)),('right',(257,190)))}
    if any(row['checked_pixels']!=80 for row in matches.values()):raise ValueError('信息板静态边框不是完整80像素')
    if side=='off':ok=not any(row['ok'] for row in matches.values())
    elif side in matches:ok=matches[side]['ok'] and not matches['right' if side=='left' else 'left']['ok']
    else:raise ValueError('未知信息板侧别')
    return {'kind':'info_panel','expected':side,'borders':matches} if ok else None


def native_panel_off(frame,assets,references):
    """OFF须匹配原版同checkpoint完整双侧ROI，不能把损坏残板当成消失。"""
    if not references:raise ValueError('SDL OFF缺原版WGC参考，拒绝弱化为边框不完整')
    if not panel_state(frame,assets,'off'):return None
    if frame.size!=(320,200):raise ValueError('OFF实际帧尺寸不同')
    for row in references:
        path=Path(row['path'])
        if (row.get('source_kind')!='original' or row.get('capture_backend')!='Windows.Graphics.Capture HWND' or
                row.get('ROIs')!=[list(box) for box in PANEL_BOXES] or sha(path)!=row['sha256']):
            raise ValueError('OFF参考来源/摘要不同')
        with Image.open(path) as image:expected=image.convert('RGB')
        if expected.size!=(320,200):raise ValueError('OFF参考尺寸不同')
        matches=[]
        for box in PANEL_BOXES:
            want=expected.crop(box);got=frame.crop(box).convert('RGB')
            delta=ImageChops.difference(want,got)
            maximum=max(high for low,high in delta.getextrema())
            matches.append({'ROI':list(box),'checked_pixels':want.width*want.height,'max_channel_delta':maximum})
        if all(item['checked_pixels']==2346 and item['max_channel_delta']<=TOLERANCE for item in matches):
            return {'kind':'info_panel_off','source_kind':'original','reference_sha256':row['sha256'],
                'full_both_ROIs':matches,'channel_tolerance':TOLERANCE,'mask_added':False}
    return None


def measured_state(expected,after,world):
    cx,cy=expected[META+3:META+5]
    steps=[('explicit-navigation',(4+(world[0]-cx)*24,4+(world[1]-cy)*24))]
    return check_load_measurement(expected,after,steps)


def save_available(plain):
    return not any(not plain[UNITS+raw*STRIDE+5]&1 and plain[UNITS+raw*STRIDE+5]&0x80
        for raw in range(plain[META+1]))


def exercise(capture,original,plain,manifest,scenario,save,record,references=None):
    assets=round_assets(original);titles=title_templates(original/'FDOTHER.DAT')
    actors={i:actor_templates(original,plain,assets,i) for i in range(4)}
    world=list(plain[META+5:META+7]);cx,cy=plain[META+3:META+5]
    values=[plain[META+16],plain[META+17],plain[META+14],plain[META+15]]
    saved_bytes=save.read_bytes();saved_stamp=save.stat().st_mtime_ns
    def oracle():return RoundOracle(assets,(4+(world[0]-cx)*24,4+(world[1]-cy)*24))
    def checkpoint(name,predicate,timeout=15):
        observed=wait_state(capture,name,predicate,record['checkpoints'],timeout=timeout)
        guard()
        return observed
    def guard():require_source_unchanged(save,saved_bytes,saved_stamp)
    def browse(frame):
        raw=next((i for i in range(4) if not plain[UNITS+i*STRIDE+5]&1 and list(plain[UNITS+i*STRIDE:UNITS+i*STRIDE+2])==world),None)
        if raw is None:return oracle().closed_browse(frame)
        found=focused_actor(frame,assets,actors[raw])
        return found if found and found['focus_screen']==list(oracle().focus) else None
    def navigate(target):
        # 场景坐标均在camera不滚动域；每格只有一个键并独立资源ACK。
        for axis,keypos,keyneg in ((0,'RIGHT','LEFT'),(1,'DOWN','UP')):
            while world[axis]!=target[axis]:
                change=1 if world[axis]<target[axis] else -1
                next_world=world.copy();next_world[axis]+=change
                rx,ry=world[0]-cx,world[1]-cy
                if (axis==0 and (change<0 and rx<2 or change>0 and rx>10) or
                    axis==1 and (change<0 and ry<2 or change>0 and ry>5)):
                    raise ValueError('本定向导航将滚动镜头，拒绝猜坐标')
                capture.key(keypos if change>0 else keyneg);world[:]=next_world
                checkpoint('navigate-'+str(len(record['checkpoints'])),browse);guard()
    def parent():
        capture.key('ENTER');checkpoint('parent-'+str(len(record['checkpoints'])),lambda f:oracle().menu(f,0))
    def toggle():
        parent();capture.key('RIGHT');checkpoint('settings-parent',lambda f:oracle().menu(f,2))
        capture.key('ENTER');checkpoint('settings-open',lambda f:options(f,oracle(),values,0))
        capture.key('DOWN');checkpoint('settings-fourth',lambda f:options(f,oracle(),values,3))
        capture.key('ENTER');values[3]^=1
        checkpoint('settings-toggled',lambda f:options(f,oracle(),values,3));guard()
        capture.key('ESC');checkpoint('settings-parent-return',lambda f:oracle().menu(f,0))
        capture.key('ESC');checkpoint('settings-browse-return',browse);guard()
    def storage(which):
        nonlocal saved_bytes,saved_stamp
        parent();capture.key('UP');checkpoint('storage-parent',lambda f:oracle().menu(f,0))
        capture.key('ENTER');checkpoint('secondary',lambda f:oracle().secondary(f,0))
        selected=1 if which=='save' else 2
        capture.key('LEFT' if which=='save' else 'RIGHT');checkpoint(which+'-selected',lambda f:oracle().secondary(f,selected))
        capture.key('ENTER');checkpoint(which+'-yes',lambda f:oracle().buttons(f,0));guard()
        previous,stamp=saved_bytes,saved_stamp
        capture.key('ENTER')
        if which=='save':
            after=wait_measured_save(save,previous,stamp);saved_bytes=save.read_bytes();saved_stamp=save.stat().st_mtime_ns
            checkpoint('saved-browse',browse);return after
        checkpoint('loaded-browse',browse,45);guard();return None
    def off_checkpoint(name):
        if record['kind']=='sdl':
            checkpoint(name,lambda frame:native_panel_off(frame,assets,(references or {}).get(name)))
            return
        frame,_=checkpoint(name,lambda frame:panel_state(frame,assets,'off'))
        rows=[]
        # 原版idle采样只提供实际见到的完整合法帧，不生成/平均/拟合颜色。
        # SDL必须同时匹配某一原帧左右各2346像素，不跨帧拼接两侧。
        for index in range(16):
            if index:
                time.sleep(.11);frame=capture.frame();guard()
            if not browse(frame) or not panel_state(frame,assets,'off'):
                raise AssertionError('原版OFF样本未保持同一browse/来源前置')
            path=capture.artifacts/(name+'-original-reference-'+str(index)+'.png');frame.save(path)
            rows.append({'path':str(path.resolve()),'sha256':sha(path),'source_kind':'original',
                'capture_backend':'Windows.Graphics.Capture HWND','checkpoint':name,
                'ROIs':[list(box) for box in PANEL_BOXES]})
        record.setdefault('panel_off_native_refs',{})[name]=rows
    wait_title(capture,titles[3][0]);capture.key('UP');capture.wait(titles[3][2]);capture.key('ENTER')
    checkpoint('initial-browse',browse,40);guard()
    if scenario.startswith('browse'):
        record['focus_sequence']=[]
        for key,raw in manifest['cycle_inputs']:
            capture.key(key)
            if raw is not None:world[:]=plain[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]
            _,detail=checkpoint('key-'+key+'-'+str(len(record['focus_sequence'])),browse)
            guard();record['focus_sequence'].append({'key':key,'expected_raw':raw,'evidence':detail})
        # 原START两列之间的已证空格，不经过行动或phase。
        navigate((9,15))
        if save_available(plain):
            after=storage('save');record['state']=measured_state(plain,after,world)
        else:
            # gates夹具故意保留raw2 acted，原19df7禁止Save。核真实disabled
            # frame和LEFT无效/取消，不伪造业务写回，也不临时清acted绕过门。
            parent();capture.key('UP');checkpoint('storage-parent',lambda f:oracle().menu(f,0))
            capture.key('ENTER');checkpoint('secondary-save-disabled',lambda f:oracle().secondary(f,0,save_available=False))
            capture.key('LEFT');checkpoint('save-disabled-key-noop',lambda f:oracle().secondary(f,0,save_available=False))
            capture.key('ESC');checkpoint('disabled-save-parent-return',lambda f:oracle().menu(f,0))
            capture.key('ESC');checkpoint('disabled-save-browse-return',browse)
            guard();record['state']={'save_disabled_by_original_acted_gate':True,
                'disk_source_unchanged':True,'whole_runtime_saved_or_compared':False}
    else:
        toggle();off_checkpoint('panel-disabled')
        # 经空列9绕开actor0上方头部；每格ACK避免把遮挡误作输入完成。
        navigate((9,13));navigate((9,17));navigate((7,17))
        off_checkpoint('low-left-disabled')
        toggle();checkpoint('low-left-panel-right',lambda f:panel_state(f,assets,'right'))
        navigate((15,17));checkpoint('low-right-panel-left',lambda f:panel_state(f,assets,'left'))
        first=storage('save');record['save_A']=measured_state(plain,first,world)
        if first[META+15]!=1:raise AssertionError('原UI Save A没有raw option4=1')
        toggle();off_checkpoint('live-off-before-load');guard()
        storage('load');values[3]=1
        checkpoint('loaded-option-panel-left',lambda f:panel_state(f,assets,'left'))
        second=storage('save');record['save_B']=check_load_measurement(first,second,[])
        if second[META+15]!=first[META+15]:raise AssertionError('活动Load没有恢复原Save A raw option')
        record['raw_option_restored']={'offset':'0x30d2','A':first[META+15],'live_unsaved':0,'B':second[META+15]}
    record['final_save_sha256']=sha(save)


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or original in output.parents or output.exists():raise ValueError('产物必须是原资源外的新目录')
    selected=getattr(args,'scenario',None) or list(SCENARIOS)
    if len(set(selected))!=len(selected) or any(name not in SCENARIOS for name in selected):raise ValueError('场景重复或未知')
    contract=source_contract(original);prepared=[(name,*fixture(original,name)) for name in selected]
    output.mkdir(parents=True)
    report={'ok':False,'status':'prepared_only','source_contract':contract,'cases':[],
        'display_requirement':{},'scope':'battle0 browse key gates/index, option4 panels and full active Save/Load',
        'excluded':['新关卡绑定','全历史回归','RNG/墙钟完整对齐','音频保真','Linux实机']}
    for name,plain,encoded,manifest in prepared:
        (output/(name+'.SAV')).write_bytes(encoded);write(output/(name+'.json'),manifest)
        if decode_save(output/(name+'.SAV'))!=plain:raise AssertionError('派生夹具编码/校验和不一致')
        assets=round_assets(original)
        for raw in range(4):actor_templates(original,plain,assets,raw)
    if not args.run:write(output/'comparison.json',report);return report
    owned_job_required()
    binaries=[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()]
    user_save=original.parent/'FD2.SAV'
    if user_save.is_file():binaries.append(user_save)
    inputs,before,sources=freeze(root,original,binaries)
    for name,*_ in prepared:
        for suffix in ('.SAV','.json'):inputs[str(output/(name+suffix))]=sha(output/(name+suffix))
    try:
        original_cases={}
        with windows_display_requirement(report['display_requirement']):
            for kind in ('original','sdl'):
                for name,plain,encoded,manifest in prepared:
                    work=output/(kind+'-'+name);work.mkdir();env=os.environ.copy()
                    for variable in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(variable,None)
                    if kind=='original':
                        game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                            '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                        command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                        inputs[str(config)]=sha(config)
                        # FD2.TMP是原10010重建的sprite cache；只有它和本轮Save可写。
                        mutable={save.resolve(),(game/'FD2.TMP').resolve()}
                        for path in game.rglob('*'):
                            if path.is_file() and path.resolve() not in mutable:inputs[str(path)]=sha(path)
                    else:
                        save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save)
                        command,cwd=[str(args.sdl.resolve())],root
                    save.write_bytes(encoded)
                    case={'kind':kind,'scenario':name,'ok':False,'inputs':[],'checkpoints':[],'processes':[]}
                    case['owned_mutable_paths']=[str(save.resolve())]+([str((game/'FD2.TMP').resolve())] if kind=='original' else [])
                    report['cases'].append(case)
                    try:
                        if not unchanged(inputs,original,before,root,sources):raise ValueError('首窗口前来源改变')
                        with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                            capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                            references=original_cases.get(name,{}).get('panel_off_native_refs')
                            exercise(capture,original,plain,manifest,name,save,case,references)
                        case['ok']=True
                    except BaseException as error:
                        case['error']={'type':type(error).__name__,'message':str(error)}
                        raise
                    finally:
                        case['sources_unchanged']=unchanged(inputs,original,before,root,sources)
                        case['ok'] &= case['sources_unchanged'] and len(case['processes'])==1 and all(
                            p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in case['processes'])
                        write(work/'comparison.json',case)
                    if not case['ok']:raise AssertionError('端点未通过：'+kind+'/'+name)
                    if kind=='original':
                        original_cases[name]=case
                        for rows in case.get('panel_off_native_refs',{}).values():
                            for row in rows:inputs[row['path']]=row['sha256']
        report['ok']=len(report['cases'])==2*len(prepared) and all(row['ok'] for row in report['cases'])
        report['status']='passed' if report['ok'] else 'failed'
    except BaseException as error:
        report['status']='failed';report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['sources_unchanged']=unchanged(inputs,original,before,root,sources)
        report['ok'] &= report['sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',choices=SCENARIOS,nargs='+')
    parser.add_argument('--run',action='store_true');args=parser.parse_args()
    if args.run and any(getattr(args,n) is None or not getattr(args,n).is_file() for n in ('sdl','driver','dosbox')):
        parser.error('--run需现有sdl/driver/dosbox，默认仅prepare')
    try:result=run(args)
    except Exception as error:
        parser.exit(1,'G4控制验收未能启动：'+str(error)+'。请检查参数或来源后重试。\n')
    print(json.dumps({'ok':result['ok'],'status':result['status']},ensure_ascii=False))
    return 0 if not args.run or result['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
