"""F4 Magic12 normal-UI wrapper feedback; default is prepare-only.

2185f/21a9e->2189a->21548, 1cff0/18d8c/19df7. Original resources,
finite source arithmetic and source masks are the oracle, never SDL output.
The G2 host owns both processes, strict ON, HWND input and cleanup.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import hashlib
import io
import json
import math
import os
from pathlib import Path
import struct
import sys

from PIL import Image
import original_reference
import run_g2_battle_check as host
import fd2_g2_battle_oracle as codec
from fd2_g2_derived_oracle import source_range
from fd2_battle_animation_frames import rle_material
from fd2_item_list_oracle import inventory_templates
from fd2_reward_oracle import magic_page
from run_g4_detail_check import detail_series, series_checkpoint
from run_original_battle_menu import wait_state, wait_title
from run_original_battle_animation import qpc_stamp
from run_original_comparison import archive_entry, normalize, title_templates
from run_original_end_turn import RoundOracle, round_assets, decode_lmi
from run_original_item_list import focus_steps, inventory_detail
from run_original_load import active_load_cache_classes, focused_actor, require_source_unchanged
from run_original_magic_list import actor_focus_resources
from run_original_player import exact_tile, screen, sheet_frame
from run_original_reward import actor_view, command, navigate, quiet_browse
from verify_f4_resource_pixels import checked_output, RESOURCES

SCENARIO = 'magic12-quake-single'
FEEDBACK_MODES = ('strict-whole','compound-feedback')
META, UNITS, STRIDE = codec.META, codec.UNITS, codec.STRIDE
CASTER, TARGET, SAVE_AT = (9, 14), (10, 14), (8, 14)
DONOR_RECORD_SHA = '7fe46861f80372afbd0c68ac6c635e2291a1c77ec704545ec5cbf0d3da44c1e9'
SOURCE09_SHA = '18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517'
G1_SHA = '67ecf2532928a0359c63e5770eeace5d5feecf56098d2fe4d72b2dc1b5fc2c75'
MAGIC = bytes.fromhex('54015a00095000')
LIMITS = ['Controlled source-valid Load, not natural acquisition or full-stage play',
 'Finite WGC samples, not all 60 CPU submissions or absolute clock/audio/PIC qualification',
 'Default strict-whole needs three complete inner312x192 pages; compound-feedback uses declared Q0 local windows plus wholeQ1/Q2',
 'Compound mode is only native feedback: the independent original/C9 logical-page join remains external',
 'Visible acted caster forbids Save19df7: no END, no Save, unchanged source file; not persisted state measurement',
 'Ring and popup compare complete declared source write windows, not whole-frame compositors',
 'Mode4 has no unique visible tag: cancellation back to MAGIC is checked, not a claimed original PC trace',
 'Hit and MISS are independently declared; no shared RNG trace or forced successful hit']


def digest(raw): return hashlib.sha256(raw).hexdigest()


def require(ok, why):
    if not ok: raise ValueError(why)


def fact(path):
    p = Path(path).resolve(strict=True); before = p.stat()
    raw = p.read_bytes(); after = p.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), '读取来源时身份改变：'+str(p))
    return raw, {'sha256': digest(raw), 'size': len(raw), 'mtime_ns': after.st_mtime_ns}


def diff(a, b):
    require(len(a) == len(b), '明密文长度不符')
    return [{'offset': i, 'before': x, 'after': y} for i, (x, y) in enumerate(zip(a, b)) if x != y]


def derive(original, get):
    base = get(original/'START.SAV', codec.START_SHA256)
    donor = get(original/'FD2.SAV', codec.DONOR_SHA256)
    exe = get(original/'FD2.EXE', codec.EXE_SHA256)
    source = codec.valid_plain(codec.crypt(base)); d = codec.valid_plain(codec.crypt(donor))
    require(digest(source) == codec.START_PLAIN_SHA256, 'START明文身份不符')
    a = d[0x8a3:0x8a3+80]
    require(digest(a) == DONOR_RECORD_SHA and codec.magic_ids(a) == [3, 7, 8, 12, 15, 23], '真实R0 donor/能力不符')
    require(source[META:META+5] == bytes((1,12,0,5,11)), '只接受固定stage0 turn1/count12/camera5,11')
    require(source_range(exe, 0x619fd+12*7, 7) == MAGIC, 'Magic12原记录不符')
    require(struct.unpack('<i', source_range(exe, 0x51f96+7*4, 4))[0] == 10, 'target profile scale不符')
    require(source_range(exe, 0x61af9+(96-68)*10, 10) == bytes.fromhex('01070e00000701010415'), '原敌种族/profile/经验记录不符')
    # 18dd1 filter0 uses the equipped type0 slot; donor slot1 flags40/item10.
    require(a[12:14] == bytes((64,10)) and source_range(exe,0x602ad+10*23,23)[11:13] == b'\1\1', 'COMMAND equipped weapon10/range1不符')
    changed = bytearray(source); changed[UNITS:UNITS+80] = a
    changed[UNITS:UNITS+2] = bytes(CASTER)
    changed[UNITS+11*80:UNITS+11*80+2] = bytes(TARGET)
    plain = codec.checksum(changed); cipher = codec.crypt(plain)
    allowed = set(range(UNITS,UNITS+80)) | {UNITS+11*80,UNITS+11*80+1} | set(range(len(plain)-4,len(plain)))
    require(all(x['offset'] in allowed for x in diff(source,plain)), 'fixture越出R0运输/A11xy/checksum写集')
    targets = [r for r in range(12) if not codec.actor(plain,r)[5]&1 and codec.actor(plain,r)[6] == 0 and
               abs(codec.actor(plain,r)[0]-9)+abs(codec.actor(plain,r)[1]-14) <= 9]
    require(targets == [11], 'area9必须恰好一个敌人，不隔离其它AI或扩目标')
    require(struct.unpack_from('<HHHH',a,64) == (892,892,341,341) and a[33] == 31 and a[60] == 38,
            'donor HP/MP/LV/EX身份不符')
    victim = codec.actor(plain,11)
    require(victim[7] == 96 and victim[32:34] == bytes((7,2)) and victim[49] == 255 and
            struct.unpack_from('<HH',victim,64) == (28,28) and not any(victim[34:40]), '目标必须是原无掉落/零状态HP28 enemy96')
    require(2*21//(31+30) == 0, '本代表必须无奖励/额外ACK')
    return plain, cipher, {'controlled_fixture': True, 'natural_action_history': False,
        'source_sha256': digest(base), 'source_plain_sha256': digest(source), 'donor_sha256': digest(donor),
        'donor_owner': 'persistent R0 whole80 -> active A0, then xy only', 'donor_record_sha256': digest(a),
        'plaintext_patches': diff(source,plain), 'encrypted_patches': diff(base,cipher),
        'derived_sha256': digest(cipher), 'derived_plain_sha256': digest(plain),
        'caster': 0, 'target': 11, 'magic_id': 12, 'magic_record': MAGIC.hex(), 'targets': targets,
        'MP': [341,261], 'raw_damage': [306,339], 'accuracy_percent': 90,
        'experience': {'raw_kill':42, 'divisor':61, 'normalized':0, 'EX_before_after':[38,38]},
        'normal_Save_roster_unchanged': True, 'no_11506_settlement_sync': True,
        'no_END_no_AI_no_WAITLoad': True, 'rng_stream_equivalence_claimed': False,
        'unsaved_state_not_measured': ['RNG trajectory','intermediateMP/PIC/cue timestamps','UI owner memory'],
        'command_gate': {'weapon_item':10,'weapon_slot':1,'weapon_flags':64,'default':0,
            'initial_frames':[1,3,6,9],'magic_key':'LEFT','magic_frames':[0,4,6,9]},
        'magic_list_navigation':{'source':'1d51d scan80 increments; scan72 decrements/wraps',
            'keys':['DOWN','DOWN','DOWN'],'selected_indices':[1,2,3],'selected_magic':12},
        'checked_cancel_protocol':{'source':'1d3ec returns0 -> 19019 retries1900e -> 1d0be resetsindex0',
            'return_layer':'MAGIC_LIST','selected_after_cancel':0,'new_ENTER_before_DOWN':False}}


def postcast_states(source):
    """Source UI models only: not a runtime dump or an executed Save."""
    loaded = bytearray(source)
    for raw, value in enumerate(active_load_cache_classes(source)): loaded[UNITS+raw*80+2] = value
    loaded[META+5:META+9] = bytes((*CASTER,4,3))
    loaded[UNITS+5] |= 128
    struct.pack_into('<H',loaded,UNITS+68,261)
    outcomes = {}
    for kind in ('miss','hit'):
        value = bytearray(loaded)
        if kind == 'hit':
            struct.pack_into('<H',value,UNITS+11*80+64,0)
            value[UNITS+11*80+5] = 1
        outcomes[kind] = codec.checksum(value)
    return outcomes


def packed_frame(sheet, index):
    w,h,count = struct.unpack_from('<HHH',sheet)
    require((w,h)==(24,24) and 0 <= index < count, '24x24帧索引越界')
    lo = struct.unpack_from('<I',sheet,6+index*4)[0]
    hi = struct.unpack_from('<I',sheet,10+index*4)[0] if index+1<count else len(sheet)
    require(6+4*count <= lo < hi <= len(sheet),'帧真实span越界')
    return rle_material(struct.pack('<HH',24,24)+sheet[lo:hi])


def rgb(indexed, palette):
    return bytes(palette[3*c+k] for c in indexed for k in range(3))


def palettes(raw6, exe):
    result = []
    for phase in range(16):
        p = bytearray(raw6)
        water = source_range(exe,0x60003+phase*3,48)
        require(len(p)==768 and max(p)<=63 and max(water)<=63,'原DAC六位域')
        p[224*3:240*3] = water
        result.append(bytes((x<<2)|(x>>4) for x in p))
    return result


def make_visual_model(original, source, get):
    field = get(original/'FDFIELD.DAT',codec.FIELD_SHA256)
    shapes = get(original/'FDSHAP.DAT',codec.SHAPE_SHA256)
    icons = get(original/'FDICON.B24',RESOURCES['FDICON.B24'])
    other = get(original/'FDOTHER.DAT',RESOURCES['FDOTHER.DAT'])
    exe = get(original/'FD2.EXE',codec.EXE_SHA256)
    grid, metadata = archive_entry(field,0),archive_entry(field,1)
    w,h = struct.unpack_from('<HH',grid)
    require((w,h)==(24,24) and len(grid)==4+w*h*4,'原stage0 map extent')
    cells = struct.unpack_from('<'+'I'*(w*h),grid,4)
    sheet = archive_entry(shapes,metadata[0]*2); attrs=archive_entry(shapes,metadata[0]*2+1)
    tiles = {v&1023:bytes(packed_frame(sheet,v&1023)[0].getdata()) for v in cells}
    # 1399c clears every cache24x24 before RLE; SKIP in cache is defined0.
    for at in (CASTER,TARGET,SAVE_AT):
        c=cells[at[1]*24+at[0]]; attribute=attrs[(c&1023)*4:(c&1023)*4+4]
        require(len(attribute)==4 and not attribute[0]&0x60 and not (c>>16)&31,'站位或Save格存在事件/非法cell')
    profile=source_range(exe,0x61646+codec.actor(source,0)[32]*20,20)
    require(profile[attrs[(cells[14*24+9]&1023)*4+1]]<20,'caster站位不可站立')
    # Existing independent source DFS, not C runtime or C expected dump.
    from ida.derive_f4_movement_vectors import range_expected
    units=[(u[6],u[0],u[1],u[5]) for u in (codec.actor(source,i) for i in range(12))]
    movement=range_expected({'width':w,'height':h,'cells':cells,'classes':list(attrs[1::4]),
        'costs':list(profile),'units':units,'budget':codec.actor(source,0)[59],'apply':False})
    base_palette=archive_entry(other,0)
    ps=palettes(base_palette,exe)
    # Before the first eligible4e31c, the initial bank is also legal (each
    # component is one less here). Reject both sets; do not infer a tick from WGC.
    water_rgb={tuple(p[c*3:c*3+3]) for p in ps for c in range(224,240)}
    water_rgb.update(tuple((v<<2)|(v>>4) for v in base_palette[c*3:c*3+3]) for c in range(224,240))
    # Three source snapshots. The original scaled map uses cache base, not terrain animation.
    snapshots=[]
    for q,(dx,dy,step) in enumerate(((128,128,131),(0,0,128),(-128,128,125))):
        startx=5*3072+13*1536+dx-156*step; starty=11*3072+8*1536+dy-96*step
        terrain=bytearray(312*192)
        for y in range(192):
            ty,iy=divmod(starty+y*step,3072)
            for x in range(312):
                tx,ix=divmod(startx+x*step,3072)
                if 0<=tx<w and 0<=ty<h:terrain[y*312+x]=tiles[cells[ty*w+tx]&1023][(iy//128)*24+ix//128]
        for idle in range(3):
            page=Image.frombytes('P',(312,192),bytes(terrain))
            for raw in range(12):
                u=codec.actor(source,raw)
                if u[5]&1 or not 4<=u[0]<=18 or not 10<=u[1]<=20:continue
                require(u[3]==u[4]==0 and not any(u[34:40]),'本有限Q只接受零walk/status的实际来源')
                image,mask=packed_frame(icons,u[7]*12+idle)
                page.paste(image,((u[0]-5)*24,(u[1]-11)*24-6),mask)
            data=page.tobytes()
            snapshots.append({'q':q,'idle':idle,'indices':data,'rgb':[rgb(data,p) for p in ps]})
    snapshot_images=[(v,Image.frombytes('RGB',(312,192),v['rgb'][0])) for v in snapshots]
    lutraw=archive_entry(other,3)
    lut_count=struct.unpack_from('<H',lutraw,4)[0]
    require(lut_count==23,'原FDOTHER3 LUT目录不同')
    luts=[lutraw[struct.unpack_from('<I',lutraw,6+i*4)[0]:
        struct.unpack_from('<I',lutraw,10+i*4)[0] if i+1<lut_count else len(lutraw)] for i in range(lut_count)]
    require(all(len(x)==256 for x in luts),'原23张LUT长度')
    # One complete dry, unoccluded24x24 window establishes the ring before quake.
    ring=[]; q0_candidates=[]; movement_probe=None
    actor_boxes=[((u[0]-5)*24,(u[1]-11)*24-6,24,24) for u in (codec.actor(source,i) for i in range(12)) if not u[5]&1]
    for wy in range(12,18):
        for wx in range(6,17):
            c=cells[wy*w+wx]; base=tiles[c&1023]; x=(wx-5)*24;y=(wy-11)*24
            if c>>10 or attrs[(c&1023)*4] or set(base)&set(range(224,240)):continue
            if any(x<ax+aw and ax<x+24 and y<ay+ah and ay<y+24 for ax,ay,aw,ah in actor_boxes):continue
            if movement_probe is None and movement['cells'][wy*w+wx]>>24!=255:
                variants=[Image.frombytes('RGB',(24,24),rgb(bytes(lut[b] for b in base),ps[0])) for lut in luts[:10]]
                movement_probe={'world':(wx,wy),'position':(x+4,y+4),'rgb':variants}
            # Mode4 still paints ordinary reachable tiles with51A97 LUT0..10.
            # A full-tile colour match is not a ring; require disjoint RGB from
            # plain and every actual LUT, using a completely written source tile.
            if not all(packed_frame(sheet,c&1023)[1].tobytes()):continue
            ordinary_rgb={rgb(bytes(lut[b] for b in base),ps[0]) for lut in luts}
            ordinary_rgb.add(rgb(base,ps[0]))
            ring_rgb=set()
            for i,lut in enumerate(luts[:10]):
                radius=30+16*i; data=bytearray(base); wrong=bytearray(base)
                for row in range(24):
                    delta=90-(y+row)
                    if abs(delta)>=radius:continue
                    half=math.isqrt((radius*radius-delta*delta)*36//25)
                    for col in range(24):
                        if 108-half<=x+col<108+half:data[row*24+col]=lut[data[row*24+col]]
                    wrong_delta=88-(y+row)
                    if abs(wrong_delta)<radius:
                        oldhalf=math.isqrt((radius*radius-wrong_delta*wrong_delta)*36//25)
                        for col in range(24):
                            if 108-oldhalf<=x+col<108+oldhalf:wrong[row*24+col]=lut[wrong[row*24+col]]
                wanted=rgb(data,ps[0])
                ring_rgb.add(wanted)
                if data!=base and data!=wrong and wanted not in ordinary_rgb and not (set(data)&set(range(224,240))):
                    ring.append({'phase':i,'position':(x+4,y+4),'rgb':wanted,'radius':radius})
            # One source-selected window per row, never selected from WGC PNGs.
            crops=[(v,image.crop((x,y,x+24,y+24)).tobytes()) for v,image in snapshot_images]
            zero=[data for v,data in crops if v['q']==0]
            if len(set(zero))==1 and zero[0] not in ordinary_rgb|ring_rgb and all(zero[0]!=data for v,data in crops if v['q']!=0):
                q0_candidates.append({'position':(x+4,y+4),'size':(24,24),'world':(wx,wy),'rgb':zero[0]})
    require(movement_probe is not None and ring,'缺独立movement/ring资源正证；未创建窗口')
    q0_windows=[]
    for window in q0_candidates:
        if all(w['world'][1]!=window['world'][1] for w in q0_windows):q0_windows.append(window)
    require(q0_windows,'缺source预定义Q0局部窗；未创建窗口')
    # UI helpers retain their historical <=3 resource gates; effect RGB is always exact.
    return {'snapshots':snapshots,'palettes':ps,'water_rgb':water_rgb,
        'ring':ring,'Q0_local_windows':q0_windows,'movement':movement_probe,'other':other,'exe':exe,
        'summary':{'Q':3,'idle':3,'water':16,'Q_rgb_variants':144,'ring_windows':len(ring),
            'first_samples':[[15020,33632],[15360,33792],[15700,34208]],'VGA_projection':[4,4,312,192],
            'ring_disjoint_from_plain_and_all23_tile_LUTs':True,
            'Q0_local_window_rule':'first qualifying dry unoccluded24x24 per source row; disjoint from wholeQ1/Q2, plain, all23LUT and ten ring windows',
            'Q0_local_windows':[{'position':w['position'],'size':w['size'],'world':w['world'],'RGB_sha256':digest(w['rgb'])} for w in q0_windows],
            'source':'1f558 cache preclear1399c; 21548 snapshot order; 127e0 raw RLE masks; 2189a/219ad scale12'}}


def quake_matches(frame, model):
    if frame.size!=(320,200):return []
    inner=frame.convert('RGB').crop((4,4,316,196)).tobytes()
    return [(r['q'],r['idle'],water) for r in model['snapshots'] for water,wanted in enumerate(r['rgb']) if inner==wanted]


def border_bytes(frame):
    require(frame.size==(320,200),'完整VGA尺寸不符')
    data=frame.convert('RGB').tobytes()
    return b''.join(data[(y*320+x)*3:(y*320+x+1)*3] for y in range(200) for x in range(320)
        if x<4 or x>=316 or y<4 or y>=196)


def qualify_border(before, model):
    data=border_bytes(before)
    require(not any(tuple(data[i:i+3]) in model['water_rgb'] for i in range(0,len(data),3)),
            '本端before外框含动态water色，窄RGB保留关系无资格；不排除像素')
    return data


def ordered_quake(labels):
    """Observed changes must embed in the source60 sequence; repeated samples may be one upload."""
    seq=(0,1,2,1)*15; at=-1; previous=None
    for q in labels:
        require(type(q)is int and q in (0,1,2),'非法quake标签')
        if q==previous:continue
        at=next((i for i in range(at+1,60) if seq[i]==q),60)
        if at==60:return False
        previous=q
    return set(labels)=={0,1,2}


def popup_models(model, branch):
    require(branch in ('hit','miss'),'非法popup结果类别')
    other,exe=model['other'],model['exe'];sheet=archive_entry(other,5)
    curve=source_range(exe,0x5202c,25)
    # Only the final source frame21 is held500ms; no guessing missed2ms frames.
    values=range(306,340) if branch=='hit' else [None]
    for value in values:
        ids=[0,*[94+int(c) for c in str(value)]] if value is not None else list(source_range(exe,0x5204a,4))
        canvas=Image.new('P',(30,45),0);written=Image.new('L',canvas.size,0)
        for slot,id in enumerate(ids):
            if not id:continue
            count=struct.unpack_from('<H',sheet,4)[0]
            lo=struct.unpack_from('<I',sheet,6+id*4)[0]
            hi=struct.unpack_from('<I',sheet,10+id*4)[0] if id+1<count else len(sheet)
            image=decode_lmi(sheet[lo:hi])
            mask=Image.frombytes('L',image.size,bytes(255 if c else 0 for c in image.getdata()))
            x=5*slot+(3 if value is None and slot==1 else 2);y=curve[21+slot]
            require(x+image.width<=30 and y+image.height<=45,'源popup窗口超界')
            canvas.paste(image,(x,y),mask);written.paste(255,(x,y),mask)
        indices=canvas.tobytes();marks=written.tobytes()
        require(not(set(indices[i] for i,v in enumerate(marks) if v)&set(range(224,240))),'popup不能借water RGB')
        yield {'value':value,'position':(124,73),'size':canvas.size,'mask':marks,'rgb':rgb(indices,model['palettes'][0])}


def popup_match(frame, specs):
    for spec in specs:
        x,y=spec['position'];w,h=spec['size'];actual=frame.convert('RGB').crop((x,y,x+w,y+h)).tobytes()
        if all(actual[3*i:3*i+3]==spec['rgb'][3*i:3*i+3] for i,flag in enumerate(spec['mask']) if flag):
            return {'value':spec['value'],'mask_bytes':sum(bool(x) for x in spec['mask']),
                'source_frame':21,'RGB_tolerance':0,'whole_frame_retained_not_whole_popup_compositor':True}
    return None


def read_frame(row):
    data=Path(row['path']).read_bytes();require(digest(data)==row['sha256'],'已采原PNG身份改变')
    with Image.open(io.BytesIO(data)) as image:return normalize(image)


def completed_tap_frames(series, dispatch, pid, through):
    """Post-completed-tap requests only; never infer guest key consumption."""
    start,end=dispatch['dispatch_start'],dispatch['posted_end']
    for stamp in (start,end):
        require(type(stamp.get('qpc'))is int and stamp['qpc']>=0 and
            type(stamp.get('qpc_frequency'))is int and stamp['qpc_frequency']>0,'cast缺真实QPC上界')
    frequency=end['qpc_frequency']
    require(start['qpc_frequency']==frequency and start['qpc']<=end['qpc'],'cast QPC顺序或频率不同')
    summary,ready,rows=series['summary'],series['ready'],series['frames']
    require(type(pid)is int and pid>0 and summary.get('ok')is True and type(summary.get('pid'))is int and
        summary['pid']==pid and type(ready.get('pid'))is int and ready['pid']==pid and
        bool(summary.get('hwnd')) and summary['hwnd']==ready.get('hwnd'),
        'closed WGC owner不同')
    require(type(summary.get('qpc_frequency'))is int and summary['qpc_frequency']==frequency,
        'closed WGC与cast QPC频率不同')
    require(type(rows)is list and type(summary.get('frames'))is int and summary['frames']==len(rows) and rows and
        type(through)is int and 0<=through<len(rows),'closed WGC数量或return边界无效')
    metric_names=('poll_started_qpc','poll_finished_qpc','arrival_qpc','copy_finished_qpc','png_finished_qpc')
    selected=[];excluded=[];last_timestamp=-1;last_png=-1
    for index,row in enumerate(rows):
        require(type(row.get('index'))is int and row['index']==index and type(row.get('pid'))is int and row['pid']==pid and
            row.get('hwnd')==summary['hwnd'],'WGC帧owner或序号不同')
        timestamp=row.get('wgc_timestamp_100ns')
        metrics=[row.get(k) for k in metric_names]
        require(type(timestamp)is int and timestamp>last_timestamp and timestamp>=0 and
            all(type(v)is int and v>=0 for v in metrics) and metrics==sorted(metrics) and metrics[0]>=last_png,
            'WGC帧时钟或完整metrics顺序无效')
        last_timestamp=timestamp
        last_png=metrics[-1]
        reasons=[]
        if timestamp*frequency<=end['qpc']*10000000:reasons.append('frame_SRT_not_after_completed_tap')
        if metrics[0]<end['qpc']:reasons.append('poll_request_before_completed_tap')
        if index>through:reasons.append('after_return_checkpoint')
        if reasons:excluded.append({'index':index,'reasons':reasons})
        else:selected.append(row)
    return selected,{'scope':'sampling requests after completed150ms tap; not guest consumption or early DOWN150ms',
        'qpc_frequency':frequency,'posted_end_qpc':end['qpc'],'through_return_index':through,
        'checked_closed_frames':len(rows),'selected_indices':[r['index'] for r in selected],
        'excluded':excluded,'SRT_and_poll_both_required':True,'guest_consumption_claimed':False}


def feedback(rows, before, model, branch, mode='strict-whole'):
    require(mode in FEEDBACK_MODES,'未知反馈mode')
    border=qualify_border(before,model);hits=[];rings=[];popups=[]
    specs=list(popup_models(model,branch))
    for row in rows:
        image=read_frame(row); whole=quake_matches(image,model);q=list(whole);local=[]
        if mode=='compound-feedback':
            for window in model['Q0_local_windows']:
                x,y=window['position'];w,h=window['size']
                if image.convert('RGB').crop((x,y,x+w,y+h)).tobytes()==window['rgb']:
                    local.append({'position':window['position'],'size':window['size'],'RGB_sha256':digest(window['rgb'])})
            if local:q.extend((0,idle,water) for idle in range(3) for water in range(len(model['palettes'])) if (0,idle,water) not in q)
        if q:
            hits.append({'index':row['index'],'variants':q,'sha256':row['sha256'],
                'whole_page_variants':whole,'Q0_local_windows':local,
                'outer4096_RGB_matches':border_bytes(image)==border})
        for r in model['ring']:
            x,y=r['position']
            if image.convert('RGB').crop((x,y,x+24,y+24)).tobytes()==r['rgb']:
                rings.append({'index':row['index'],'phase':r['phase'],'radius':r['radius'],'position':r['position']});break
        p=popup_match(image,specs)
        if p:popups.append({'index':row['index'],**p})
    # Q1 is the source unscaled terrain: the prelude/tail can also produce it.
    # Select a finite ring->three-snapshot->popup witness, not every Q1-looking frame.
    witness=None
    for ring in rings:
        for popup in popups:
            candidates=[h for h in hits if ring['index']<h['index']<popup['index']]
            for start in range(len(candidates)):
                states={(a,b) for q,a,b in candidates[start]['variants']};window=[]
                for hit in candidates[start:]:
                    if not hit['outer4096_RGB_matches']:break
                    states&={(a,b) for q,a,b in hit['variants']};window.append(hit)
                    if not states:break
                    alternatives=[]
                    for idle,water in sorted(states):
                        labels=[next(q for q,a,b in h['variants'] if (a,b)==(idle,water)) for h in window]
                        if ordered_quake(labels):alternatives.append({'idle':idle,'water':water,'labels':labels})
                    if alternatives:
                        witness={'window':window,'alternatives':alternatives,'ring':ring,'popup':popup};break
                if witness:break
            if witness:break
        if witness:break
    alternatives=witness['alternatives'] if witness else []
    selected=witness['window'] if witness else []
    full_three=any(ordered_quake([next(q for q,a,b in h['whole_page_variants'] if (a,b)==(alt['idle'],alt['water']))
        for h in selected if any((a,b)==(alt['idle'],alt['water']) for q,a,b in h['whole_page_variants'])]) for alt in alternatives)
    result={'ok':bool(witness),'mode':mode,'quake_full_page_samples':[h for h in selected if h['whole_page_variants']],
        'Q0_local_samples':[h for h in selected if h['Q0_local_windows']], 'all_snapshot_pattern_candidates':hits,
        'source_consistent_snapshot_alternatives':alternatives,'ring_before_quake':[witness['ring']] if witness else [],
        'popup_after_quake':[witness['popup']] if witness else [],
        'full64000_quake_RGB_and4096_border':full_three,'RGB_tolerance':0,
        'complete_compound_qualification_claimed':False,'external_original_C9_logical_join_required':mode=='compound-feedback',
        'source_before_whole_RGB_sha256':digest(before.convert('RGB').tobytes()),
        'source_before_outer4096_RGB_sha256':digest(border),
        'Q1_is_not_a_unique_PC_or_epoch_tag':True,
        'all60_submissions_claimed':False,'same_rng_claimed':False,'midpointMP_measured':False,
        'missing_feedback_is_missing_evidence_not_implementation_error':True}
    return result


def prepare(root, scenario, get, mode='strict-whole'):
    require(scenario==SCENARIO,'只支持Magic12单动作，无其它法术/END分支')
    require(mode in FEEDBACK_MODES,'未知反馈mode')
    original=(root/'original_game').resolve();source,encoded,manifest=derive(original,get)
    model=make_visual_model(original,source,get);assets=round_assets(original)
    caster=codec.actor(source,0);_,upper,_=inventory_templates(original,caster,assets)
    lists=[magic_page(original,caster,assets,i)[0] for i in range(4)]
    expected=postcast_states(source)
    actor=codec.actor(expected['miss'],0)
    _,detail,fields=inventory_templates(original,actor,assets);fields['require_word_bars']=True
    at,image=detail['current_max_mp']
    require(not(set(image.tobytes())&set(range(224,240))),'MP261窗口含水色，不能签固定RGB')
    manifest['navigation_no_scroll']={'focus':focus_steps(source,tuple(source[META+5:META+7]),CASTER),
        'save':focus_steps(source,CASTER,SAVE_AT),'camera':[5,11]}
    p={'original':original,'source':source,'encoded':encoded,'manifest':manifest,'scenario':scenario,'assets':assets,'feedback_mode':mode,
        'titles':title_templates(original/'FDOTHER.DAT'),'expected':expected,'visual_model':model,'magic_lists':lists,
        'list_upper':{k:upper[k] for k in ('actor_name','current_max_mp')},
        'actor':actor_focus_resources(original,source,assets,0,assets['cursor']),
        'command_actor':actor_view(original,source,assets,0),
        'postcast_detail':(detail,fields),'postcast_magic':magic_page(original,actor,assets)[0],
        'measurement':{'source_UI_state_models':{k:digest(v) for k,v in expected.items()},'model_bytes_not_measured_runtime':22987,
            'Save_count':0,'END_count':0,'disk_must_remain_original_fixture':True,'feedback_mode':mode,'source':LIMITS}}
    # Mode4 has no unique target marker; ordinary reachable-tile LUT remains legal.
    return p


def exercise(capture,p,save,guard,record):
    source=p['source'];assets=p['assets']
    def cp(name,predicate,**options):
        result=wait_state(capture,name,predicate,record['checkpoints'],**options)
        require_source_unchanged(save,*guard);return result
    def list_at(frame,i):
        at,image=p['magic_lists'][i];checks={'bottom':bool(exact_tile(frame,image,at))}
        checks.update({k:bool(exact_tile(frame,v[1],v[0])) for k,v in p['list_upper'].items()})
        return {'kind':'Magic12-six-actual-abilities','selected':i,'checks':checks} if all(checks.values()) else None
    def cmd(frame,i):return command(frame,source,p['command_actor'],0,i,[1,3,6,9] if i==0 else [0,4,6,9])
    def move_selected(frame):
        actor=focused_actor(frame,assets,p['actor']);m=p['visual_model']['movement'];x,y=m['position']
        seen=[i for i,w in enumerate(m['rgb']) if frame.convert('RGB').crop((x,y,x+24,y+24)).tobytes()==w.tobytes()]
        return {'kind':'movement-source-DFS','actor':actor,'LUT_candidates':seen} if actor and seen else None
    wait_title(capture,p['titles'][3][0],timeout=90)
    capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    start=tuple(source[META+5:META+7])
    cp('quake-continued',quiet_browse(p,source,start,record,'quake-continued'),timeout=60)
    navigate(capture,p,source,start,CASTER,cp,'quake-focus')
    capture.key('ENTER');cp('quake-zero-move-selected',move_selected)
    capture.key('ENTER');cp('quake-command-attack-default',lambda f:cmd(f,0))
    capture.key('LEFT');cp('quake-command-magic',lambda f:cmd(f,1))
    def select_magic12():
        for i in range(1,4):
            capture.key('DOWN');cp('quake-magic-index'+str(i),lambda f,i=i:list_at(f,i))
        capture.key('ENTER')
        return cp('quake-source-mode4-wait',quiet_browse(p,source,CASTER,record,'quake-mode4'))
    capture.key('ENTER');cp('quake-magic-index0',lambda f:list_at(f,0))
    select_magic12()
    capture.key('ESC');cp('quake-checked-mode4-cancel',lambda f:list_at(f,0))
    require_source_unchanged(save,*guard)
    record['checked_cancel']={'source':'1d3c4/-1 -> 1d3c9 focus -> return0; 19019 retries MAGIC; 1d0be index0',
        'resource_layer':'MAGIC_LIST','selected':0,
        'disk_before_unchanged':True,'later_visible_checks_for_cancel':['reopened MP341','single-cast MP261','acted caster','same-popup target feedback'],
        'persisted_state_measured':False,'raw_cancel_write_set_measured':False,'original_PC_observed':False}
    select_magic12()
    # Caster return alone cannot distinguish hit/MISS; the source popup must do so.
    # HIT/MISS只改相邻raw11，caster及其下格相同；结果另由popup/目标检查判定。
    returned_caster=quiet_browse(p,p['expected']['miss'],CASTER,record,'quake-return-caster')
    def returned(frame):
        hit=returned_caster(frame)
        return {'kind':'quake-return-acted-caster','outcome_not_determined_by_caster_alone':True,'browse':hit} if hit else None
    with detail_series(capture,record) as scope:
        before,_=series_checkpoint(capture,scope,'quake-final-mode4-before',
            quiet_browse(p,source,CASTER,record,'quake-precommit'),record['checkpoints'],timeout=15)
        qualify_border(before,p['visual_model'])
        record['cast_dispatch']={'dispatch_start':qpc_stamp()}
        capture.key('ENTER',fast=True)
        record['cast_dispatch']['posted_end']=qpc_stamp()
        series_checkpoint(capture,scope,'quake-return-no-extra-input',returned,record['checkpoints'],timeout=35)
        before_index=scope['after']['index']
    require_source_unchanged(save,*guard)
    require(record['detail_series'].get('checkpoint_PNGs_bound_to_closed_series')is True,'WGC系列未完整归还')
    rows,record['post_completed_tap_sampling']=completed_tap_frames(record['detail_series'],
        record['cast_dispatch'],capture.process.pid,before_index)
    strict={k:feedback(rows,before,p['visual_model'],k) for k in ('miss','hit')}
    mode=p['feedback_mode']
    observed=strict if mode=='strict-whole' else {k:feedback(rows,before,p['visual_model'],k,mode) for k in ('miss','hit')}
    record['strict_three_whole_WGC']={k:v['ok'] for k,v in strict.items()}
    record['feedback_mode']=mode
    branches=[k for k,v in observed.items() if v['ok']]
    require(len(branches)==1,'Magic12反馈缺唯一source ring/Q/popup分支；不补键、不发送Save或扩大预算')
    branch=branches[0];post=p['expected'][branch]
    record['quake_feedback']=observed[branch]
    record['observed_postcast']={'outcome':branch,'basis':'same source popup and subsequent ordinary visible state',
        'runtime_raw_owner_dumped':False,'serialized_state_measured':False}
    navigate(capture,p,post,CASTER,TARGET,cp,'quake-target')
    cp('quake-target-'+branch,quiet_browse(p,post,TARGET,record,'quake-target-'+branch))
    record['observed_postcast']['target_feedback']='source-fixed cell empty after hit' if branch=='hit' else 'source96 sprite still visible after MISS; rawHP not dumped'
    navigate(capture,p,post,TARGET,CASTER,cp,'quake-back-caster')
    templates,fields=p['postcast_detail']
    def detail(frame):
        found=inventory_detail(frame,templates,fields)
        at,image=templates['current_max_mp'];x,y=at
        if found and frame.convert('RGB').crop((x,y,x+image.width,y+image.height)).tobytes()==rgb(image.tobytes(),p['visual_model']['palettes'][0]):
            return dict(found,MP261_complete44x8_RGB_tolerance=0)
        return None
    capture.key('F2');_,record['postcast_MP_detail']=cp('quake-MP261-detail',detail)
    at,image=p['postcast_magic'];capture.key('ESC')
    cp('quake-detail-magic',lambda frame:exact_tile(frame,image,at))
    capture.key('ESC');cp('quake-detail-closed',quiet_browse(p,post,CASTER,record,'quake-detail-closed'))
    # 19e98..19ea4 disables selector1 for any visible acted actor. No Save/END.
    navigate(capture,p,post,CASTER,SAVE_AT,cp,'quake-to-system-empty')
    view=RoundOracle(assets,screen(post,SAVE_AT))
    capture.key('ENTER');cp('quake-disabled-save-parent',lambda f:view.menu(f,0))
    capture.key('ENTER');_,disabled=cp('quake-save-disabled',lambda f:view.secondary(f,0,save_available=False))
    capture.key('ESC');cp('quake-secondary-cancel-parent',lambda f:view.menu(f,0))
    capture.key('ESC');cp('quake-parent-cancel-browse',view.closed_browse)
    require_source_unchanged(save,*guard)
    record['no_write_storage_check']={'Save_count':0,'END_count':0,'disk_fixture_unchanged':True,
        'secondary_source_mask':[0,1,0,0],'secondary_source_frames':[37,41,42,45],
        'disabled_resource':disabled,'cancel_source':'19ef3 return0 ->16ff2 ->118cc reopen parent; second Esc16fd8 return1',
        'persisted_state_measured':False}


def run(args):
    root=args.root.resolve(strict=True);original=(root/'original_game').resolve()
    out=checked_output(root,args.artifacts,args.output_root)
    require(args.scenario==SCENARIO,'不支持其它scenario')
    require(args.bootstrap is not None and args.bootstrap.is_file(),'必须冻结实际bootstrap，prepare也不得伪签profile')
    require(not any(k.upper().startswith('DOSBOX_') for k in os.environ),'拒绝未冻结DOSBOX环境覆盖')
    pins={};snapshots={}
    def get(path,expected=None):
        path=Path(path).resolve();raw,identity=fact(path)
        if expected:require(identity['sha256']==expected,'来源SHA不符：'+str(path))
        if str(path) in pins:require(identity==pins[str(path)] and raw==snapshots[str(path)],'重复输入改变')
        pins[str(path)]=identity;snapshots[str(path)]=raw;return raw
    original_before=host.fingerprint(original)
    get(root/'build/f1/domain-boundaries/source09.json',SOURCE09_SHA)
    get(args.g1_source,G1_SHA)
    get(args.bootstrap);get(__file__)
    mode=getattr(args,'feedback_mode','strict-whole')
    p=prepare(root,args.scenario,get,mode)
    require(host.fingerprint(original)==original_before,'准备期间原资源身份改变')
    out.mkdir(parents=True,exist_ok=False)
    (out/'postcast-models.json').write_text(json.dumps({'scope':'source UI models only; not captured or saved runtime state',
        'full_model_sha256':{k:digest(v) for k,v in p['expected'].items()},'persisted_state_measured':False},indent=2),encoding='utf-8')
    (out/'visual-contract.json').write_text(json.dumps(p['visual_model']['summary'],indent=2),encoding='utf-8')
    report={'schema':'fd2.f4.magic12-quake-native.v2','ok':False,'status':'prepared','scope':LIMITS,'feedback_mode':mode,
        'complete_compound_qualification_claimed':False,'external_original_C9_logical_join_required':mode=='compound-feedback',
        'persisted_state_measured':False,'F6_retained_gates':['normal END->Save complete22987B','natural before/shared-RNG/input joint qualification'],
        'other_stage_gates':['F5 PIC/PCM and absolute event timing'],
        'unmeasured_not_automatically_required':['wholeQ0 WGC capture','all60 physical presentations and absolute scan timing'],
        'source_inputs':pins,'pair':None,'original_before':original_before}
    previous_prepare,previous_exercise,previous_running=host.prepare,host.exercise,host.running
    references=[]
    def guards():
        result={}
        for path,before in pins.items():
            try:result[path]=fact(path)[1]==before
            except (OSError,ValueError):result[path]=False
        return result
    @contextmanager
    def running(command,cwd,env,log,driver,receipts,**options):
        # G2 has already adapted and qualified this actual command. Preserve its owner.
        require(all(guards().values()),'首窗口前来源改变')
        if Path(command[0]).resolve()==args.dosbox.resolve():
            from run_f2_prepare_check import command_configuration
            from run_original_comparison import quiet_game_command
            actual,_,_=quiet_game_command(command,env);configs,overrides=command_configuration(actual)
            receipt=original_reference.build(original,args.dosbox,configs,overrides,args.bootstrap)
            path=Path(log).parent/'quake-reference.json';original_reference.write_new(path,receipt)
            original_reference.verify(path);references.append({'path':str(path),'sha256':digest(path.read_bytes())})
            for c,declared in zip(configs,receipt['configs'],strict=True):
                get(c);identity=pins[str(c.resolve())]
                require(all(identity[k]==declared[k] for k in ('sha256','size')),'配置身份与冻结收据不一致')
            get(path)
            original_reference.verify(path)
        primary=None
        try:
            with previous_running(command,cwd,env,log,driver,receipts,**options) as process:yield process
        except BaseException as error:primary=error;raise
        finally:
            errors=[]
            for ref in references:
                try:original_reference.verify(ref['path'])
                except BaseException as error:errors.append(str(error))
            report['reference_late_errors']=errors
            if errors and primary is None:raise AssertionError('原配置晚守卫失败：'+str(errors))
    try:
        host.prepare=lambda r,s:p;host.exercise=exercise;host.running=running
        pair_args=argparse.Namespace(**vars(args));pair_args.prepare_only=not args.run;pair_args.artifacts=out/'paired'
        if args.run:
            for key in ('sdl','driver','dosbox'):require(getattr(args,key) is not None and getattr(args,key).is_file(),'实际run缺'+key)
            for module in tuple(sys.modules.values()):
                path=getattr(module,'__file__',None)
                if path and Path(path).resolve().is_relative_to(root/'tools'):get(path)
        result=host.run(pair_args);report['pair']=result
        result['scope']='Magic12 native presentation/visible-state component; no Save/END or persisted state measurement'
        result['persisted_state_measured']=False
        try:(out/'paired'/'comparison.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        except BaseException as error:
            if not result.get('error'):raise
            report.setdefault('late_errors',[]).append({'phase':'paired_report_scope','error':str(error)})
        report['ok']=args.run and result['ok'];report['status']=result['status']
        if args.run and result['ok']:
            report['pair_visible_outcomes']=[r['observed_postcast']['outcome'] for r in result['cases']]
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)};report['status']='failed'
    finally:
        host.prepare,host.exercise,host.running=previous_prepare,previous_exercise,previous_running
        report['source_guards_after']=guards();report['references']=references
        try:report['original_unchanged']=host.fingerprint(original)==original_before
        except BaseException as error:
            report['original_unchanged']=False
            report.setdefault('late_errors',[]).append({'phase':'original_guard','error':str(error)})
        report['ok'] &= all(report['source_guards_after'].values()) and report['original_unchanged']
        if args.run and not report['ok']:report['status']='failed'
        try:(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        except BaseException as error:
            first=report.get('error') or (report.get('pair') or {}).get('error')
            raise RuntimeError(('首业务错误：'+str(first)+'；' if first else '')+'report写入失败：'+str(error)) from error
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('root','artifacts','output-root','bootstrap','g1-source'):p.add_argument('--'+k,type=Path,required=True)
    for k in ('sdl','driver','dosbox'):p.add_argument('--'+k,type=Path)
    p.add_argument('--scenario',default=SCENARIO,choices=[SCENARIO]);p.add_argument('--run',action='store_true')
    p.add_argument('--feedback-mode',choices=FEEDBACK_MODES,default='strict-whole')
    a=p.parse_args();r=run(a);print(json.dumps({k:r[k] for k in ('ok','status')},ensure_ascii=False))
    return 0 if r['ok'] or not a.run and r['status']=='ready_not_executed' else 1


if __name__=='__main__':raise SystemExit(main())
