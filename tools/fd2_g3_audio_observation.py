#!/usr/bin/env python3
"""G3只读音乐快照/原源合同；不采集、不发RPC、不控制设备或修改runner。

25977加载完整FDMUS后443d0写sequence+10h EVNT、44790启动status4；
44960写loop+28h，44a60读status+4，均保原源码地址，不保存猜测ABI。
快照只证明当时已加载/播放的资源和循环配置，不证明声卡输出、完整EOT或音色。
原RAM必须由既有OwnedRPC单MEMDUMPBIN取得；定位复用其两原码锚和合法fixup，
不使用调用者自报object基址、不在RAM中搜索曲目来代替真实owner指针。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

from analyze_fd2_audio import archive_entries
from rebuild_fd2_analysis import parse_le,parse_objects,parse_fixups
import fd2_dosbox_snapshot as snapshot
import fd2_g3_prepare_oracle as oracle

TRACKS=(10,11,13,14,15,16,17,18)  # 18由标题LOAD保留的真实来源也须能检查。
# 沿用已封存G3/A7原资源身份；不是将本轮读到的SHA自动当作预期。
FDMUS_SHA256='4105ebde543fe1c497e852728f6bc333bda80edeb7fb3671e487504bee74e998'
FDOTHER_SHA256='882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667'
# 三个无relocation的完整leaf原码；独立固定操作数约束字段，不能由JSON自报偏移。
LEAVES={0x44960:bytes.fromhex('8b44240485c074078b542408895028c3'),
        0x44a60:bytes.fromhex('8b44240485c074038b4004c3'),
        0x44a70:bytes.fromhex('8b44240485c07506b8ffffffffc38b4028c3')}
# 已发布2a43e/2aa00成功分支：push loop; push track; call25977。
# 声明和字节复核不等于执行PC trace；由实际成功case另建立必经路径推断。
G3_CALLS=((0x2a670,17,1),(0x2a681,11,1),(0x2abbf,16,1),(0x2abd8,11,0))
CALLER_RANGES=((0x25977,0x25a96),(0x2a43e,0x2a694),(0x2aa00,0x2ac7d))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_stable(path):
    path=Path(path).resolve(strict=True);before=path.stat();data=path.read_bytes();after=path.stat()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns) or len(data)!=after.st_size:
        raise ValueError('证据文件仍在写入或长度改变')
    return data,{'path':str(path),'size':len(data),'sha256':sha(data)}


def sources(original):
    original=Path(original);exe,exe_fact=read_stable(original/'FD2.EXE')
    if sha(exe)!=oracle.EXE_SHA256:
        raise ValueError('FD2.EXE不是已证原源')
    for va,expected in LEAVES.items():
        if oracle.source_range(exe,va,len(expected))!=expected:
            raise ValueError('sequence字段的独立原指令不符：'+hex(va))
    calls=[]
    for va,track,loop in G3_CALLS:
        raw=oracle.source_range(exe,va-4,9)
        if raw[:5]!=bytes((0x6a,loop,0x6a,track,0xe8)) or va+5+struct.unpack_from('<i',raw,5)[0]!=0x25977:
            raise ValueError('G3音乐原call指令/参数不同：'+hex(va))
        calls.append({'call_cva':hex(va),'callee_cva':'0x25977','track':track,'loop':loop,
            'source_sha256':sha(raw),'source_size':len(raw),'runtime_PC_hit_claimed':False})
    mus,mus_fact=read_stable(original/'FDMUS.DAT');tracks=archive_entries(mus)
    other,other_fact=read_stable(original/'FDOTHER.DAT');banks=archive_entries(other)
    if sha(mus)!=FDMUS_SHA256 or sha(other)!=FDOTHER_SHA256:
        raise ValueError('FDMUS/FDOTHER不是已封存原资源')
    voices=archive_entries(banks[31])
    if len(voices)!=13 or len(tracks)<18:
        raise ValueError('原FDMUS或UIbank31有效目录不同')
    return exe,tracks,{'executable':exe_fact,'fdmus':mus_fact,'fdother':other_fact,
        'tracks':[{'track':i,'size':len(tracks[i]),'sha256':sha(tracks[i])} for i in TRACKS],
        'ui_bank':31,'ui_voices':[{'sample':i,'source_rate':11025,'frames':len(raw),
            'sha256':sha(raw)} for i,raw in enumerate(voices)],
        'field_leaves':[{'canonical_va':hex(va),'size':len(raw),'sha256':sha(raw)} for va,raw in LEAVES.items()],
        'g3_music_call_sites':calls}


def observe_music(memory,original,descriptor,expected_track,expected_loop):
    if type(expected_track) is not int or expected_track not in TRACKS or type(expected_loop) is not int or expected_loop not in (0,1):
        raise ValueError('G3检查仅接受已证曲10/11/13..18与loop0/1')
    exe,tracks,source=sources(original)
    _,located=snapshot.resolve(memory,original,descriptor)
    code=located['object_DS_offsets']['1'];data=located['object_DS_offsets']['2']
    le=parse_le(exe);objects=parse_objects(exe,le);fixups=parse_fixups(exe,le,objects)
    runtime_code=[]
    for start,end in CALLER_RANGES:
        expected=bytearray(oracle.source_range(exe,start,end-start));applied=0
        for fixup in fixups:
            at=fixup.source_addr_relbase
            if at<end and at+4>start:
                target_base=located['object_DS_offsets'].get(str(fixup.target_object))
                if at<start or at+4>end or target_base is None:
                    raise ValueError('音乐完整caller fixup跨界或目标object未知')
                struct.pack_into('<I',expected,at-start,target_base+fixup.target_offset);applied+=1
        actual=memory[code+start-0x10000:code+end-0x10000]
        if actual!=expected:
            raise ValueError('本轮RAM完整音乐caller与原码合法重定位不同：'+hex(start))
        runtime_code.append({'start_cva':hex(start),'size':len(expected),'fixup_count':applied,
            'relocated_sha256':sha(actual),'all_source_bytes_match_legal_relocation':True,
            'runtime_PC_hit_claimed':False})
    for va,raw in LEAVES.items():
        if memory[code+va-0x10000:code+va-0x10000+len(raw)]!=raw:
            raise ValueError('本轮RAM sequence叶与原指令不同')
    def get(at,size):
        if at<=0 or at+size>len(memory):
            raise ValueError('音频真实owner指针越RAM界或为空')
        return memory[at:at+size]
    def u32(at):
        return struct.unpack('<I',get(at,4))[0]
    requested=get(data+0x1a11,1)[0]
    pointer=u32(data+0x3ee0);sequence=u32(data+0x3ed0)
    get(sequence,0x3c)
    raw=tracks[expected_track];loaded=get(pointer,len(raw))
    # 本8条原记录各有唯一EVNT；完整源块相等先于游标/状态检查，不按近似音频拟合。
    offsets=[i for i in range(len(raw)-7) if raw[i:i+4]==b'EVNT']
    if len(offsets)!=1:
        raise ValueError('原曲没有唯一EVNT头')
    evnt=offsets[0];event_size=struct.unpack_from('>I',raw,evnt+4)[0]
    if evnt+8+event_size>len(raw):
        raise ValueError('原EVNT越曲目记录界')
    status=u32(sequence+4);event_pointer=u32(sequence+0x10);cursor=u32(sequence+0x14)
    loops=u32(sequence+0x28);current=u32(sequence+0x34);target=u32(sequence+0x38)
    enabled=get(data+0x3ef0,1)[0];option=get(data+0x1e61,1)[0]
    checks={'requested_track_matches':requested==expected_track,
        'full_loaded_original_entry_matches':loaded==raw,
        'sequence_evnt_points_to_loaded_entry':event_pointer==pointer+evnt,
        'sequence_cursor_in_evnt':pointer+evnt+8<=cursor<=pointer+evnt+8+event_size,
        'sequence_playing_status4':status==4,'remaining_loop_matches':loops==expected_loop,
        'music_driver_enabled':enabled!=0,'music_option_enabled':option!=0,
        'sequence_target_volume127':target==127,'sequence_volume_in_original_range':current<=127}
    ok=all(checks.values())
    return {'schema':'fd2-g3-music-snapshot-v1','ok':ok,
        'status':'observed_resource_playing_loop_config' if ok else 'pending_or_mismatched_snapshot',
        'expected':{'track':expected_track,'loop':expected_loop},'checks':checks,
        'observed':{'requested_track_byte':requested,'resource_DS_offset':pointer,
            'resource_size':len(raw),'resource_sha256':sha(loaded),'sequence_DS_offset':sequence,
            'status':status,'evnt_DS_offset':event_pointer,'cursor_DS_offset':cursor,
            'remaining_loops':loops,'volume_current':current,'volume_target':target,
            'driver_enabled':enabled,'music_option':option},'sources':source,'locator':located,
        'runtime_source_code':runtime_code,
        'scope':{'actual_RPC_owner_revalidated_offline':False,'single_snapshot_fields':True,
            'acoustic_identity_claimed':False,'completed_loop_count_claimed':False,
            'device_output_claimed':False,'amplitude_or_timbre_equivalence_claimed':False,
            'request_stop_minus1_supported':False,
            'note':'loop0是无限配置；loop1只能在仍playing时验证1。结束后status2/loop0不能反推最初请求次数。'}}


def observe_snapshot(original,ram_path,receipt_path,track,loop):
    memory,fact=read_stable(ram_path);raw,receipt_fact=read_stable(receipt_path)
    receipt=json.loads(raw)
    if (receipt.get('sha256')!=fact['sha256'] or receipt.get('size')!=len(memory) or
        receipt.get('single_main_emulation_thread_command') is not True or receipt.get('break_or_extra_window') is not False):
        raise ValueError('RAM与OwnedRPC单快照收据不一致')
    before=receipt['DS_descriptor_before'];after=receipt['DS_descriptor_after']
    if any(before[k]!=after[k] for k in ('base','limit','type','dpl','flags')) or before['limit']<snapshot.RAM_SIZE-1:
        raise ValueError('RPC descriptor不一致或容量不足')
    for reg in ('registers_before','registers_after'):
        if not receipt[reg]['CR0']&1 or receipt[reg]['EFLAGS']&(1<<17):
            raise ValueError('RPC快照不在已证protected视图')
    result=observe_music(memory,original,before,track,loop)
    result.update(ram=fact,rpc_snapshot_receipt=receipt_fact)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original',type=Path,default=Path('original_game'))
    parser.add_argument('--ram',type=Path);parser.add_argument('--receipt',type=Path)
    parser.add_argument('--track',type=int);parser.add_argument('--loop',type=int)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:
        output=args.output.resolve();original=args.original.resolve(strict=True)
        if output==original or output.is_relative_to(original) or any(p.lower()=='original_game' for p in output.parts):
            raise ValueError('报告不可写原资源目录或别名')
        fields=(args.ram,args.receipt,args.track,args.loop)
        if all(x is None for x in fields):
            _,_,result=sources(args.original)
            result={'schema':'fd2-g3-audio-source-v1','status':'source_contract_only_not_runtime',**result}
        elif any(x is None for x in fields):
            raise ValueError('观察必须同时给ram/receipt/track/loop')
        else:
            result=observe_snapshot(args.original,args.ram,args.receipt,args.track,args.loop)
        with output.open('x',encoding='utf-8') as stream:
            json.dump(result,stream,ensure_ascii=False,indent=2,allow_nan=False);stream.write('\n')
        return 0 if result.get('ok',True) else 3
    except (OSError,ValueError,KeyError,TypeError,struct.error) as error:
        print(json.dumps({'status':'error','error':str(error)},ensure_ascii=False));return 1


if __name__=='__main__':
    raise SystemExit(main())
