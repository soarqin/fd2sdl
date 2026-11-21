#!/usr/bin/env python3
"""G4 runtime.round的独立原指令常量投影；不读generated .inc或SDL结果。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
from functools import lru_cache

from verify_unit_tables import EXE_SHA256, source_range


def immediate(exe,va,prefix,size=1):
    opcode=bytes.fromhex(prefix);raw=source_range(exe,va,len(opcode)+size)
    if raw[:len(opcode)]!=opcode:raise ValueError('Round原指令opcode不同：'+hex(va))
    return int.from_bytes(raw[len(opcode):],'little')


def require_bytes(exe,va,expected):
    raw=bytes.fromhex(expected)
    if source_range(exe,va,len(raw))!=raw:raise ValueError('runtime原指令不同：'+hex(va))


def call_target(exe,at,target):
    raw=source_range(exe,at,5)
    if raw[0]!=0xe8 or at+5+struct.unpack_from('<i',raw,1)[0]!=target:
        raise ValueError('Round/phase原call目标不同：'+hex(at))


@lru_cache(maxsize=4)
def program(exe,caller):
    """仅解两处已确认循环的立即数和真实call目标，不解释任意x86。"""
    if hashlib.sha256(exe).hexdigest()!=EXE_SHA256:raise ValueError('Round原EXE身份不同')
    if caller=='load':
        count,first,y,x,threshold,digits,bank,number=0x104b9,0x104be,0x104c2,0x104c4,0x104e0,0x104e5,0x104e7,0x104f4
        ms,hold,begin,drop_y,drop_x,number_y=0x10501,0x10510,0x1051f,0x10530,0x10534,0x10565
        height,width,dest,tick,limit,skip,last,tail=0x10583,0x10588,0x105a2,0x105af,0x105d3,0x105d8,0x105e1,0x105f5
        cmp_prefix,lea_prefix,mov_prefix,drop_prefix,num_prefix='83fb','8d43','bb','8d46','8d56'
        calls=[(0x104d6,0x15f0e),(0x104b0,0x15e71),(0x104f9,0x187d6),
            (0x1054c,0x15f0e),(0x1057b,0x187d6),(0x105ca,0x15e71),(0x105ed,0x11cac)]
    elif caller=='new_turn':
        count,first,y,x,threshold,digits,bank,number=0x1a638,0x1a63d,0x1a641,0x1a643,0x1a65f,0x1a664,0x1a666,0x1a673
        ms,hold,begin,drop_y,drop_x,number_y=0x1a680,0x1a68f,0x1a69e,0x1a6af,0x1a6b3,0x1a6e4
        height,width,dest,tick,limit,skip,last,tail=0x1a702,0x1a707,0x1a721,0x1a72e,0x1a752,0x1a757,0x1a760,0x1a774
        cmp_prefix,lea_prefix,mov_prefix,drop_prefix,num_prefix='83fe','8d46','be','8d47','83c7'
        calls=[(0x1a655,0x15f0e),(0x1a62f,0x15e71),(0x1a678,0x187d6),
            (0x1a6cb,0x15f0e),(0x1a6fa,0x187d6),(0x1a749,0x15e71),(0x1a76c,0x11cac)]
    else:raise ValueError('Round未知caller')
    for at,target in calls:call_target(exe,at,target)
    n=immediate(exe,count,cmp_prefix);frame=immediate(exe,first,lea_prefix)
    px=immediate(exe,x,'6a');py=immediate(exe,y,'6a');digit_after=immediate(exe,threshold,cmp_prefix)
    if (immediate(exe,digits,'6a'),immediate(exe,bank,'6a'))!=(3,42):raise ValueError('Round数字域不是bank42三位')
    drop_digits,drop_bank=(0x10556,0x10558) if caller=='load' else (0x1a6d5,0x1a6d7)
    if (immediate(exe,drop_digits,'6a'),immediate(exe,drop_bank,'6a'))!=(3,42):raise ValueError('Round下落数字域不是bank42三位')
    number_at=immediate(exe,number,'68',4)-0xa0000;ny,nx=divmod(number_at,320)
    delay=immediate(exe,ms,'6a');extra=immediate(exe,hold,'68',4)
    start=immediate(exe,begin,mov_prefix,4);stop=immediate(exe,limit,cmp_prefix);omit=immediate(exe,skip,cmp_prefix)
    special=immediate(exe,last,mov_prefix,4)
    destination=immediate(exe,dest,'68',4)-0xa0000;dy,dx=divmod(destination,320)
    geometry=(immediate(exe,width,'68',4),immediate(exe,height,'68',4),dx,dy)
    if geometry!=(312,192,4,4):raise ValueError('Round可见区不是312x192→VGA(4,4)')
    fx=immediate(exe,drop_x,'6a')+dx;fy=immediate(exe,drop_y,drop_prefix)+dy
    digit_y=immediate(exe,number_y,num_prefix)+dy
    # 两处实际page+0x812f和page+0x8088的差为数字x167。
    pointer_at=0x10573 if caller=='load' else 0x1a6f2
    if immediate(exe,pointer_at,'05',4)!=0x812f:raise ValueError('Round数字工作页偏移不同')
    field_at=0x10540 if caller=='load' else 0x1a6bf
    base=immediate(exe,field_at,'05',4)
    if base!=32904:raise ValueError('Round源工作页基址不是(72,72)')
    digit_x=0x812f-base+dx
    ticks=immediate(exe,tick,'6a');tail_ms=immediate(exe,tail,'68',4)
    result=[]
    for i in range(n):
        result.append(('round_frame',[frame+i,px,py]))
        if i>digit_after:result.append(('round_number',[nx,ny]))
        result.append(('round_wait_ms',[delay]))
        if i==n-1:result.append(('round_wait_ms',[extra]))
        result.append(('round_restore_vga',[]))
    result.append(('round_map_background',[]))
    for m in [i for i in range(start,stop) if i!=omit]+[special]:
        result.extend([('round_drop',[frame+n-1,fx,fy+m*m,digit_x,digit_y+m*m]),('round_wait_ticks',[ticks])])
    result.extend([('round_refresh',[]),('round_wait_ms',[tail_ms]),('round_finish',[])])
    return result


@lru_cache(maxsize=4)
def phase_program(exe,enemy):
    if hashlib.sha256(exe).hexdigest()!=EXE_SHA256:raise ValueError('phase原EXE身份不同')
    label=immediate(exe,0x1a525 if enemy else 0x1a5cf,'6a')
    hold=immediate(exe,0x1a52f if enemy else 0x1a5d9,'6a' if enemy else '68',1 if enemy else 4)
    if immediate(exe,0x1f239,'68',4)!=64000:raise ValueError('phase不是原64000B prefix借用')
    for at,target in [(0x1f210,0x1f42d),(0x1f223,0x1f42d),(0x1f231,0x1f42d),
        (0x1f26c,0x4eb59),(0x1f288,0x15f0e),(0x1f2ae,0x15f0e),(0x1f2c7,0x11d40),
        (0x1f300,0x4e381),(0x1f34e,0x4eb59),(0x1f36a,0x15f0e),(0x1f390,0x15f0e),
        (0x1f3a9,0x11d40),(0x1f401,0x11eee),(0x1f409,0x127a9),(0x1f41a,0x1f42d),
        (0x1f461,0x15f0e),(0x1f48f,0x15f0e),(0x1f4e0,0x15e71),(0x1f4f9,0x15e71)]:call_target(exe,at,target)
    # 源sub1f42d的page坐标经可见区投影+4，不取SDL表或采集结果。
    left=immediate(exe,0x1f43f,'bb',4)+4;right=immediate(exe,0x1f473,'05',4)+4
    y=immediate(exe,0x1f43d,'6a')+4;common=immediate(exe,0x1f46b,'6a')
    if (immediate(exe,0x1f27a,'6a'),immediate(exe,0x1f29d,'68',4),immediate(exe,0x1f278,'6a'))!=(left,right,y):
        raise ValueError('phase slide/mosaic坐标不一致')
    if (immediate(exe,0x4eb66,'81c6',4),immediate(exe,0x4eb6c,'81c7',4),
        immediate(exe,0x4eb74,'66b8',2),immediate(exe,0x4eb80,'b4'),
        immediate(exe,0x4eb85,'66b9',2),immediate(exe,0x4eb99,'83c7'))!=(1284,1284,320,192,312,8):
        raise ValueError('phase mosaic几何不同')
    require_bytes(exe,0x1f25d,'31ff');require_bytes(exe,0x1f3ce,'4d4f85ff0f8d6effffff')
    # 4eb59真实byte计数、逐块source加法与WORD乘法，避免替换为普通缩放。
    require_bytes(exe,0x4eb78,'66f7e3');require_bytes(exe,0x4eb94,'03f3')
    stride=immediate(exe,0x1f20c,'6bc3');close_stride=immediate(exe,0x1f416,'6bc6')
    if stride!=close_stride:raise ValueError('phase两slide步长不同')
    start=immediate(exe,0x1f201,'bb',4);open_factor=immediate(exe,0x1f1da,'bd',4)
    count=immediate(exe,0x1f2ee,'83ff');close_factor=immediate(exe,0x1f318,'bd',4)
    close_depth=immediate(exe,0x1f33c,'bf',4);close_steps=immediate(exe,0x1f423,'83fe')
    first=immediate(exe,0x1f2c5,'6a');last=immediate(exe,0x1f2c0,'68',4)
    if (first,last)!=(immediate(exe,0x1f3a7,'6a'),immediate(exe,0x1f3a2,'68',4)):
        raise ValueError('phase开合调色板范围不同')
    ticks=immediate(exe,0x1f4c5,'6a')
    if ticks!=immediate(exe,0x1f2e2,'6a') or ticks!=immediate(exe,0x1f3c4,'6a'):raise ValueError('phase tick等待不同')
    result=[('phase_parameters',[label,hold]),('round_map_background',[])]
    offsets=[i*stride for i in range(start,-1,-1)]+[immediate(exe,0x1f221,'6a'),immediate(exe,0x1f22f,'6a')]
    for offset in offsets:result.extend([('phase_slide',[offset,left,right,y,common]),('round_wait_ticks',[ticks])])
    result.append(('phase_capture_prefix',[]))
    for i in range(count):result.extend([('phase_mosaic',[open_factor+i,left,right,y,common]),
        ('phase_darkness',[first,last,i]),('round_wait_ticks',[ticks])])
    result.extend([('phase_clear_keys',[]),('phase_hold',[])])
    for i in range(close_depth,-1,-1):result.extend([('phase_mosaic',[close_factor-close_depth+i,left,right,y,common]),
        ('phase_darkness',[first,last,i]),('round_wait_ticks',[ticks])])
    result.extend([('phase_redraw',[]),('round_map_background',[])])
    for i in range(close_steps+1):result.extend([('phase_slide',[i*close_stride,left,right,y,common]),('round_wait_ticks',[ticks])])
    result.append(('round_finish',[]));return result


@lru_cache(maxsize=4)
def turn_program(exe):
    """1a30b真实call/写点投影；scheduler三边界只切断等待，不改源顺序。"""
    if hashlib.sha256(exe).hexdigest()!=EXE_SHA256:raise ValueError('turncycle原EXE身份不同')
    for at,target in [(0x1a32a,0x17aa9),(0x1a393,0x1da16),(0x1a3c6,0x11eb0),
        (0x1a3df,0x25a96),(0x1a3ea,0x17aa9),(0x1a46e,0x1da16),(0x1a477,0x13512),
        (0x1a4a8,0x11eb0),(0x1a4b3,0x17aa9),(0x1a4bd,0x11cac),
        (0x1a4c7,0x1a813),(0x1a4d1,0x1a866),(0x1a51d,0x25977),
        (0x1a527,0x1f1cc),(0x1a53b,0x1f30a),(0x1a543,0x13536),(0x1a54a,0x11cac),
        (0x1a554,0x1a813),(0x1a55e,0x1a866),(0x1a5c7,0x25977),
        (0x1a5d1,0x1f1cc),(0x1a5e8,0x1f30a),(0x1a5fa,0x13536),
        (0x1a601,0x11cac),(0x1a618,0x25977),(0x1a78d,0x1a813),
        (0x1a797,0x1a866),(0x1a7ab,0x12d7b),(0x1a7b3,0x4e381)]:call_target(exe,at,target)
    # 两次独立raw扫描同门；HP先写，normal blit随后，acted最后。
    for va,raw in [(0x1a35c,'83f802'),(0x1a361,'f6430581'),(0x1a367,'0fb64325'),
        (0x1a36f,'0fb64326'),(0x1a377,'39cf'),(0x1a425,'83f802'),(0x1a42a,'f6430581'),
        (0x1a430,'0fb64325'),(0x1a438,'0fb64326'),(0x1a440,'39cf'),
        (0x1a455,'66897b40'),(0x1a5b9,'ff05ef3b0000'),
        (0x1a5f0,'c705831a000000000000'),(0x1a79f,'c705831a000001000000'),
        (0x1a781,'c705e93a000000000000'),(0x1a517,'740c'),(0x1a5c1,'740c'),
        (0x1a31c,'c7042400000000'),(0x1a3e7,'ff3424'),(0x1a4b0,'ff3424')]:require_bytes(exe,va,raw)
    if (immediate(exe,0x1a380,'6a'),immediate(exe,0x1a383,'68',4),
        immediate(exe,0x1a3d7,'6a'),immediate(exe,0x1a3f4,'bd',4),
        immediate(exe,0x1a459,'6a'),immediate(exe,0x1a45b,'6a'))!=(2,456,4,5,0,0):
        raise ValueError('turncycle组恢复mode/stride/cue/divisor不同')
    sides=[immediate(exe,va,'6a') for va in (0x1a4c5,0x1a4cf,0x1a552,0x1a55c,0x1a78b,0x1a795)]
    if sides!=[1,1,0,0,2,2]:raise ValueError('turncycle事件与status阵营不同')
    # 51AAC恢复属于13565/March/END caller，不属于1a30b、clear或Load。
    for va in (0x135b4,0x170ba,0x1726b):require_bytes(exe,va,'c605ac1a000000')
    for va in (0x135d4,0x17163,0x17277):require_bytes(exe,va,'c605ac1a000001')
    for at in (0x135c5,0x17154,0x17272):call_target(exe,at,0x1a30b)
    require_bytes(exe,0x135ca,'c705831a000001000000')
    require_bytes(exe,0x17159,'c705831a000001000000')
    reset=source_range(exe,0x134e4,46)
    if hashlib.sha256(reset).hexdigest()!='dc6f3ac1946072a996c259f094f6bad5da08d74223ad1b35fa150204a655578a':
        raise ValueError('turncycle方向reset完整原函数不同')
    require_bytes(exe,0x13507,'6a14');call_target(exe,0x13509,0x3790a)
    call_target(exe,0x13e88,0x134e4);require_bytes(exe,0x13e8d,'6a00');call_target(exe,0x13e8f,0x11cac)
    reset_ms=immediate(exe,0x13507,'6a')
    def path(*steps):return [('turn_'+op,args) for op,args in steps]+[('turn_done',[])]
    return {
      'player_done':path(('recovery_wait',[immediate(exe,0x1a328,'6a')]),('recovery_highlight',[]),
        ('recovery_cue',[]),('recovery_wait',[0]),('recovery_commit',[]),('recovery_project',[]),
        ('recovery_wait',[0]),('refresh',[]),('enter_phase',[sides[0]])),
      'side1_done':path(('stop_if_different',[]),('enemy_title',[]),('clear_acted',[]),
        ('refresh',[]),('enter_phase',[sides[2]])),
      'side0_done':path(('increment',[]),('stop_if_different',[]),('player_title',[]),
        ('display_mode',[0]),('clear_acted',[]),('refresh',[]),('primary_music',[]),
        ('round',[]),('focus_reset',[]),('enter_phase',[sides[4]])),
      'player_ready':path(('display_mode',[1]),('focus_zero',[]),('clear_keys',[])),
      'caller_return':path(('info_visible',[1])),
      'directions_reset':path(('reset_directions',[]),('delay_ms',[reset_ms])),
      'directions_reset_refresh':path(('reset_directions',[]),('delay_ms',[reset_ms]),('refresh',[]))}


def choice_program(exe):
    """19953与197e5的真实loop、key Jcc、共享timer及关闭写点。"""
    if hashlib.sha256(exe).hexdigest()!=EXE_SHA256:raise ValueError('choice原EXE身份不同')
    for at,target in [(0x1997c,0x4ebe3),(0x19be0,0x4ebe3),(0x19ab4,0x4ed34),
        (0x19ae6,0x11eb0),(0x19b07,0x10620),(0x19b14,0x4e31c),
        (0x19a12,0x1297d),(0x19a37,0x11eee),(0x19a3f,0x127a9),
        (0x19b5f,0x1297d),(0x19b84,0x11eee),(0x19b8c,0x127a9),
        (0x19824,0x1297d),(0x19849,0x11eee),(0x19851,0x127a9)]:call_target(exe,at,target)
    for va,raw in [(0x19972,'c705573c000000000000'),(0x19983,'bb1e000000'),
        (0x1998d,'8d6a02'),(0x19a09,'833d513a000001'),(0x19b56,'833d513a000001'),
        (0x19c4b,'833d513a000001'),(0x19c52,'7648'),
        (0x199e0,'8d46fc'),(0x199e3,'69c0c8010000'),(0x199ef,'81c284800000'),
        (0x19a70,'8d466c'),(0x19a73,'69d0c8010000'),(0x19a7e,'0589800000'),
        (0x19be7,'bb1e000000'),(0x19bf1,'8d6a0a'),
        (0x19af2,'837c241004'),(0x19afb,'836c240804'),(0x19b00,'8344240c04'),
        (0x19b1e,'0fbf00'),(0x19b27,'83f802'),(0x19b2a,'7d04'),(0x19b2e,'7dd7'),
        (0x19d23,'89c2c1fa1f2bc2d1f8'),(0x19d99,'c705133c000000000000'),
        (0x19db7,'c705133c000000000000'),(0x19d8a,'83f852'),(0x19d8f,'83f81c'),
        (0x19d94,'83f839'),(0x19dad,'83f801'),(0x19db2,'83f853'),
        (0x19dcb,'83f84b'),(0x19ddf,'83f84d'),
        (0x1980b,'c7442408f0ffffff'),(0x19813,'c744240c10000000'),
        (0x198fc,'83fd04'),(0x19903,'8344240804'),(0x19908,'836c240c04')]:require_bytes(exe,va,raw)
    # 图的wait先检测键，只有无键才执行tick；confirm/cancel都先清phase，
    # 然后原197e5四步关闭。业务YES/NO选择仍只在原caller持有。
    nodes=[]
    def node(i,op,args,on):nodes.append((i,'choice_'+op,args,on))
    node('begin','begin',[],{'next':'snapshot.begin'})
    node('snapshot.begin','snapshot',[],{'next':'field.begin'})
    node('field.begin','has_field',[],{'yes':'redraw.begin','no':'open.1'})
    node('redraw.begin','field_redraw',[],{'next':'open.1'})
    for i in range(1,5):node('open.'+str(i),'draw',[i*4],
        {'next':'open.'+str(i+1) if i<4 else 'wait'})
    node('wait','wait_key',[],{'yes':'read','no':'tick'})
    node('read','read_key',[],{'next':'wait','ack':'finish','cancel':'finish'})
    node('tick','tick',[],{'yes':'field.tick','no':'wait'})
    node('field.tick','has_field',[],{'yes':'redraw.tick','no':'blink'})
    node('redraw.tick','field_redraw',[],{'next':'blink'})
    node('blink','blink',[],{'next':'field.copy'})
    node('field.copy','has_field',[],{'yes':'steady','no':'snapshot.tick'})
    node('snapshot.tick','snapshot',[],{'next':'steady'})
    node('steady','steady',[16],{'next':'wait'})
    node('finish','finish',[],{'next':'field.close'})
    node('field.close','has_field',[],{'yes':'redraw.close','no':'close.1'})
    node('redraw.close','field_redraw',[],{'next':'close.1'})
    for i in range(1,5):node('close.'+str(i),'draw',[16-i*4],
        {'next':'close.'+str(i+1) if i<4 else 'done'})
    node('done','done',[],{});return nodes


@lru_cache(maxsize=4)
def defeat_program(exe,visible):
    """原完整1db65的有限原循环投影；raw镜像未应用fixup，区别Domain SHA。"""
    raw=source_range(exe,0x1db65,0x1debe-0x1db65)
    if hashlib.sha256(raw).hexdigest()!='434c25c5e2d79e14b2c6b4eb729001cde3b6d38e63a8a2a911a5dc452aff6806':
        raise ValueError('defeat原完整函数不同')
    for at,target in [(0x1dcd4,0x11eee),(0x1dd1b,0x127e0),(0x1dc6b,0x129ec),
        (0x1dc96,0x11eb0),(0x1dca0,0x17aa9),(0x1dd81,0x11eee),(0x1dd95,0x127a9),
        (0x1ddaa,0x25a96),(0x1ddcd,0x4ebab),(0x1de3a,0x4ebab),
        (0x1de00,0x11eb0),(0x1de6d,0x11eb0),(0x1de0a,0x17aa9),
        (0x1de77,0x17aa9),(0x1de94,0x3771c),(0x1dea4,0x3776e),(0x1deae,0x11cac)]:call_target(exe,at,target)
    rotations=immediate(exe,0x1dca9,'83ff');half=immediate(exe,0x1de13,'83ff')
    frames=immediate(exe,0x1de80,'83ff');first=immediate(exe,0x1ddb6,'8d5f')
    divisor=immediate(exe,0x1dd09,'b9',4)
    if (rotations,half,frames,first,divisor)!=(13,6,12,68,4):raise ValueError('defeat原循环域不同')
    for at in (0x1dc9e,0x1de08,0x1de75):require_bytes(exe,at,'6a01')
    for at in (0x1dc61,0x1dd4c):require_bytes(exe,at,'c6430501')
    require_bytes(exe,0x1dda0,'6a016a03');require_bytes(exe,0x1deac,'6a00')
    result=[('defeat_has_visible',[])]
    if visible:
        result.append(('defeat_begin',[]))
        for i in range(rotations):result += [('defeat_rotate_page',[]),('defeat_rotate_actors',[i%divisor]),
            ('defeat_rotate_project',[]),('defeat_wait',[1])]
    result.append(('defeat_mark_hidden',[]))
    if visible:
        result += [('defeat_base',[]),('defeat_cue',[])]
        for i in range(frames):result += [('defeat_frame',[first+i]),('defeat_wait',[1])]
        result += [('defeat_close',[]),('turn_refresh',[])]
    return result+[('defeat_finish',[])]


@lru_cache(maxsize=4)
def detail_program(exe,magic):
    raw=source_range(exe,0x17aed,0x17d6f-0x17aed)
    if hashlib.sha256(raw).hexdigest()!='fdabaa88e2e33df5cb9b8040e9b8b7d3caabf3bbf3c9fba30537968801f471c5':
        raise ValueError('detail原完整函数不同')
    # 独立只读Domain完整642B与xref，flip保原1839b/1ceed底板路径而非猜新函数。
    for at,target in [(0x17afc,0x17e0b),(0x17b06,0x16c57),(0x17b14,0x1c269),
        (0x17b94,0x1839b),(0x17c44,0x1ceed),(0x17cbf,0x1839b),
        (0x17ce6,0x16c57),(0x17d05,0x18409)]:call_target(exe,at,target)
    require_bytes(exe,0x17b04,'6a00');require_bytes(exe,0x17ce4,'6a00')
    result=[('detail_open',[]),('detail_wait',[]),('detail_has_magic',[])]
    if magic:result += [('detail_flip',[]),('detail_wait',[])]
    return result+[('detail_close',[]),('detail_done',[])]


def verify_finite_branches(document,identity,query,expected,views,terminal,failure):
    flows=[f for f in document['flows'] if f['id']==identity]
    if len(flows)!=1:raise ValueError(identity+'冷图缺失或重复')
    flow=flows[0];nodes={n['id']:n for n in flow['nodes']};all_seen=set()
    for selected in (False,True):
        actual=[];seen=set();pc=flow['entry']
        while True:
            if pc not in nodes or pc in seen:raise ValueError(identity+'冷图循环或坏边')
            seen.add(pc);all_seen.add(pc);node=nodes[pc];op=node['op']
            if node.get('views')!=views:raise ValueError(identity+'视图错误')
            actual.append((op,node['args']))
            if op==terminal:
                if node['on']:raise ValueError(identity+'终点不能续写')
                break
            ports={'yes','no','error'} if op==query else {'next','error'}
            if set(node['on'])!=ports or node['on'].get('error')!='failed':
                raise ValueError(identity+'错误／取消端口不同')
            pc=node['on']['yes' if selected else 'no'] if op==query else node['on']['next']
        if actual!=expected(selected):raise ValueError(identity+'原顺序／参数不同')
    failed=nodes.get('failed')
    if failed is None or failed['op']!=failure or failed['args'] or failed['on'] or failed.get('views')!=views:
        raise ValueError(identity+'错误出口不同')
    if set(nodes)!=all_seen|{'failed'}:raise ValueError(identity+'有不可达节点')
    return len(nodes)


def verify(exe,document):
    load=program(exe,'load');new=program(exe,'new_turn')
    if load!=new:raise ValueError('Round两个原caller显示序列不同')
    matches=[f for f in document.get('flows',[]) if f.get('id')=='round']
    if document.get('domain')!='runtime' or len(matches)!=1:raise ValueError('Round冷flow不存在或重复')
    flow=matches[0];nodes={n['id']:n for n in flow['nodes']};seen=[];pc=flow['entry'];actual=[]
    while pc is not None:
        if pc in seen or pc not in nodes:raise ValueError('Round冷图循环或边缺失')
        seen.append(pc);node=nodes[pc];actual.append((node['op'],node['args']))
        if node.get('views')!=['present'] or set(node['on'])-{'next'}:raise ValueError('Round不允许业务/ACK/取消分支')
        pc=node['on'].get('next')
    if len(seen)!=len(nodes) or actual!=load:raise ValueError('Round冷顺序/参数不等于原双caller投影')
    phases=[f for f in document['flows'] if f['id']=='phase']
    if len(phases)!=1:raise ValueError('phase冷图不存在或重复')
    phase=phases[0];phase_nodes={n['id']:n for n in phase['nodes']};phase_seen=set()
    if phase.get('entry')!='player' or phase.get('entries')!={'player':'player','enemy':'enemy'}:raise ValueError('phase入口不同')
    for name,enemy in [('player',False),('enemy',True)]:
        pc=phase['entries'][name];seen=set();actual=[]
        while pc is not None:
            if pc in seen or pc not in phase_nodes:raise ValueError('phase冷图循环或边缺失')
            seen.add(pc);phase_seen.add(pc);node=phase_nodes[pc];actual.append((node['op'],node['args']))
            if node.get('views')!=['present'] or set(node['on'])-{'next'}:raise ValueError('phase不得新增业务/ACK/取消分支')
            pc=node['on'].get('next')
        if actual!=phase_program(exe,enemy):raise ValueError('phase冷顺序/参数不等于原函数投影：'+name)
    if len(phase_seen)!=len(phase_nodes):raise ValueError('phase有不可达节点')
    cycles=[f for f in document['flows'] if f['id']=='turncycle']
    if len(cycles)!=1:raise ValueError('turncycle冷图不存在或重复')
    cycle=cycles[0];cycle_nodes={n['id']:n for n in cycle['nodes']};cycle_seen=set()
    expected=turn_program(exe)
    if cycle.get('entry')!='player_done' or cycle.get('entries')!={x:x for x in expected}:
        raise ValueError('turncycle源caller入口不同')
    for entry,wanted in expected.items():
        pc=entry;seen=set();actual=[]
        while pc is not None:
            if pc in seen or pc not in cycle_nodes:raise ValueError('turncycle循环或边缺失')
            seen.add(pc);cycle_seen.add(pc);node=cycle_nodes[pc];actual.append((node['op'],node['args']))
            if node.get('views')!=['logic','present'] or set(node['on'])-{'next'}:
                raise ValueError('turncycle不得新增ACK/取消或第二执行游标')
            pc=node['on'].get('next')
        if actual!=wanted:raise ValueError('turncycle冷顺序/参数不等于原指令投影：'+entry)
    if len(cycle_seen)!=len(cycle_nodes):raise ValueError('turncycle有不可达节点')
    choices=[f for f in document['flows'] if f['id']=='choice']
    if len(choices)!=1 or choices[0].get('entry')!='begin':raise ValueError('choice冷图入口不同')
    choice=choices[0]
    if [(n['id'],n['op'],n['args'],n['on']) for n in choice['nodes']]!=choice_program(exe) or any(
        n.get('views')!=['present'] for n in choice['nodes']):raise ValueError('choice源等待/确认/取消/收回顺序不同')
    defeat_nodes=verify_finite_branches(document,'runtime.defeat','defeat_has_visible',
        lambda visible:defeat_program(exe,visible),['logic','present'],'defeat_finish','defeat_fail')
    detail_nodes=verify_finite_branches(document,'runtime.detail','detail_has_magic',
        lambda magic:detail_program(exe,magic),['present'],'detail_done','detail_fail')
    return {'ok':True,'checked_source_callers':['0x10010','0x1a30b'],'source_equal_nodes':len(load),
        'phase_entries':['player','enemy'],'phase_source_equal_nodes':len(phase_nodes),
        'turncycle_entries':list(expected),'turncycle_source_equal_nodes':len(cycle_nodes),
        'choice_source_equal_nodes':len(choice['nodes']),
        'defeat_source_equal_nodes':defeat_nodes,'detail_source_equal_nodes':detail_nodes,
        'source_sha256':hashlib.sha256(exe).hexdigest(),'original_machine_execution_claimed':False,
        'new_GUI_started':False,'turn_phase_events_or_music_executed':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);parser.add_argument('--output',type=Path)
    args=parser.parse_args();result=verify((args.root/'original_game/FD2.EXE').read_bytes(),json.loads((args.root/'data/static/flows/runtime.json').read_text(encoding='utf-8')))
    rendered=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
    if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(rendered,encoding='utf-8')
    print(rendered,end='')


if __name__=='__main__':main()
