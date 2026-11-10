"""G3独立CDB音乐owner观察；不修改现prepare snapshot或产品。

bgm.cpp真实fd2_bgm_player→driver.active_sequence→voices[2]→NativeSequence。
由同App PDB逐字段求偏移；同一次attach停止全部线程，再转储player及其FDMUS，
不调用目标函数、不写目标内存、不从current_track一个缓存数值宣称实际播放。
take仅供root在有源等待点显式调用；本模块没有自动attach/GUI入口。
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import struct

import fd2_cdb_prepare_snapshot as cdb_protocol
import fd2_g3_audio_observation as original_audio
from analyze_fd2_audio import archive_offsets,archive_entries


def sha(data):return hashlib.sha256(data).hexdigest()


def fields(pointer_size):
    result={'current_track':4,'music_enabled':1,'fdmus.data':pointer_size,
        'fdmus.size':pointer_size,'fdmus.count':pointer_size,'driver.active_sequence':pointer_size}
    for n in range(2):
        prefix=f'voices[{n}].'
        result.update({prefix+name:size for name,size in (
            ('magic',4),('track',4),('loop_count',4),('driver',pointer_size),
            ('started',1),('sequence.evnt',pointer_size),('sequence.size',pointer_size),
            ('sequence.cursor',pointer_size),('sequence.loop_remaining',4),('sequence.status',1),
            ('sequence_volume',4),('target_volume',4))})
    return result


def probe_layout(debugger,module):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*',module):
        raise ValueError('CDB module标识无效')
    def value(expression):
        return cdb_protocol.cpp_integer(debugger.request('?? '+expression))
    # C main的opaque pointer通过该模块完整C++类型读取；不硬编码anonymous基类或栈偏移。
    owner=f'(({module}!fd2_bgm_player *)app_context.bgm)'
    address=value(owner);size=value('sizeof(*'+owner+')');pointer_size=value('sizeof(void *)')
    if not address or pointer_size not in (4,8) or not 1024<=size<=1024*1024:
        raise ValueError('PDB音乐owner地址/sizeof/指针宽度无效')
    offsets={};sizes={}
    for name,width in fields(pointer_size).items():
        expression=owner+'->'+name
        sizes[name]=value('sizeof('+expression+')')
        offsets[name]=value('(char *)&('+expression+') - (char *)'+owner)
        if sizes[name]!=width or offsets[name]+width>size:
            raise ValueError('PDB音乐字段sizeof/范围不同：'+name)
    anchors={'driver':value('(char *)&('+owner+'->driver) - (char *)'+owner)}
    for n in range(2):
        anchors[f'voices[{n}]']=value('(char *)&('+owner+f'->voices[{n}]) - (char *)'+owner)
    if any(at<0 or at>=size for at in anchors.values()):
        raise ValueError('PDB音乐owner子结构越界')
    return {'address':address,'size':size,'pointer_size':pointer_size,'offsets':offsets,
        'sizes':sizes,'anchors':anchors,'from_PDB_symbol_expressions':True,'owner_expression':owner}


def decode(player,fdmus,layout,original,track,loop):
    if type(track) is not int or track not in original_audio.TRACKS or type(loop) is not int or loop not in (0,1):
        raise ValueError('SDL音乐观察仅接受已证G3曲与loop0/1')
    _,source_tracks,source=original_audio.sources(original)
    ptr=layout['pointer_size'];widths=fields(ptr)
    if ptr not in (4,8) or len(player)!=layout['size'] or not 1024<=len(player)<=1024*1024:
        raise ValueError('SDL音乐owner转储长度或指针宽度不同')
    spans=[]
    for name,width in widths.items():
        at=layout['offsets'][name]
        if type(at) is not int or at<0 or at+width>len(player) or layout['sizes'][name]!=width:
            raise ValueError('音乐字段转储范围无效：'+name)
        spans.append((at,at+width))
    if any(a<d and c<b for i,(a,b) in enumerate(spans) for c,d in spans[i+1:]):
        raise ValueError('音乐字段PDB偏移重叠')
    def get(name,signed=False):
        at=layout['offsets'][name];return int.from_bytes(player[at:at+widths[name]],'little',signed=signed)
    if sha(fdmus)!=original_audio.FDMUS_SHA256 or fdmus!=(Path(original)/'FDMUS.DAT').read_bytes():
        raise ValueError('实际SDL FDMUS内存不是完整已证原资源')
    if get('fdmus.size')!=len(fdmus) or get('fdmus.count')!=len(archive_entries(fdmus)):
        raise ValueError('SDL archive owner长度/目录数不同')
    address=layout['address'];anchors=layout['anchors']
    if type(address) is not int or address<=0 or any(type(x) is not int or not 0<=x<len(player) for x in anchors.values()):
        raise ValueError('PDB子owner锚越转储范围')
    matches=[i for i in range(2) if get('driver.active_sequence')==address+anchors[f'voices[{i}]']]
    if len(matches)!=1:
        raise ValueError('driver没有唯一指向本player voices[2]的真实active owner')
    n=matches[0];prefix=f'voices[{n}].';raw=source_tracks[track]
    offsets=archive_offsets(fdmus);evnt=raw.index(b'EVNT');event_size=struct.unpack_from('>I',raw,evnt+4)[0]
    expected_pointer=get('fdmus.data')+offsets[track]+evnt+8
    observed={name:get(prefix+name,name in ('track','loop_count','sequence_volume','target_volume')) for name in (
        'magic','track','loop_count','driver','started','sequence.evnt','sequence.size',
        'sequence.cursor','sequence.loop_remaining','sequence.status','sequence_volume','target_volume')}
    observed.update(current_track=get('current_track',True),music_enabled=get('music_enabled'),active_voice_index=n,
        active_voice_pointer=get('driver.active_sequence'),fdmus_pointer=get('fdmus.data'))
    checks={'current_track_matches':observed['current_track']==track,'active_voice_track_matches':observed['track']==track,
        'voice_magic_FDBG':observed['magic']==0x46444247,'voice_driver_is_same_owner':observed['driver']==address+anchors['driver'],
        'started':observed['started']==1,'playing_status4':observed['sequence.status']==4,
        'initial_loop_matches':observed['loop_count']==loop,'remaining_loop_matches':observed['sequence.loop_remaining']==loop,
        'fdmus_pointer_nonzero':observed['fdmus_pointer']!=0,
        'evnt_pointer_is_source_track':observed['sequence.evnt']==expected_pointer,
        'evnt_size_matches':observed['sequence.size']==event_size,
        'cursor_in_evnt':observed['sequence.cursor']<=event_size,'music_enabled':bool(observed['music_enabled']),
        'target_volume127':observed['target_volume']==127,'volume_original_range':0<=observed['sequence_volume']<=127}
    ok=all(checks.values())
    return {'schema':'fd2-sdl-bgm-owner-observation-v1','ok':ok,
        'status':'observed_active_resource_playing_loop_config' if ok else 'pending_or_mismatched_owner',
        'expected':{'track':track,'loop':loop},'checks':checks,'observed':observed,'source':source,
        'scope':{'no_new_product_dump_hook':True,'target_function_called':False,'current_track_alone_is_not_pass':True,
            'all_target_threads_stopped_during_reads':False,'PDB_collection_performed_by_this_decode':False,
            'acoustic_identity_validated':False,'completed_loop_count_validated':False,'device_output_validated':False}}


def _dump(target,exe,path,address,size):
    if path.exists() or not address or not 0<size<=1024*1024 or any(x in str(path) for x in ('"','\r','\n')):
        raise ValueError('音乐CDB仅允许新隔离文件及有界真实owner转储')
    data,receipt=cdb_protocol.read_owned_memory(target,address,size,exe)
    if len(data)!=size or receipt.get('sha256')!=sha(data):raise ValueError('实际音乐读取长度/SHA与owned收据不同')
    with path.open('xb') as stream:stream.write(data)
    if path.read_bytes()!=data:raise ValueError('Python保存音乐owner证据与实际读取字节不同')
    return data,{'path':str(path),'size':len(data),'sha256':sha(data),'owned_read':receipt}


def take(cdb,target,exe,pdb,work,record,original,track,loop):
    """root在稳定真wait显式调用；CDB scope负责失败也detach，并验证PE/PDB身份。"""
    work=Path(work).resolve();original=Path(original).resolve(strict=True)
    if work.is_relative_to(original) or any(p.lower()=='original_game' for p in work.parts):
        raise ValueError('CDB音乐产物不可写原资源')
    work.mkdir(parents=True,exist_ok=False)
    record['symbols']=cdb_protocol.symbols(exe,pdb,Path(cdb).resolve().parent/'dbghelp.dll')
    with cdb_protocol.CDB(cdb,target,work,record) as debugger:
        debugger.request('.sympath '+str(Path(pdb).resolve().parent));debugger.request('.reload /f '+Path(exe).name)
        trace=debugger.request('~*kn 0n128');frame=cdb_protocol.main_frame(trace,target.pid,Path(exe).stem)
        record['main_frame']=frame;debugger.request(f'~~[{frame["thread_id"]:x}]s');debugger.request(f'.frame /r {frame["frame"]:x}')
        local=debugger.request('dv /t /v app_context')
        if not any('app_context' in line and 'fd2_field_play_context' in line for line in local):
            raise ValueError('CDB未解析本轮main app_context真实类型')
        layout=probe_layout(debugger,Path(exe).stem)
        player,player_fact=_dump(target,exe,work/'sdl-bgm-owner.bin',layout['address'],layout['size'])
        offsets=layout['offsets'];width=layout['pointer_size']
        def pointer(name):
            at=offsets[name];return int.from_bytes(player[at:at+width],'little')
        fdmus,fdmus_fact=_dump(target,exe,work/'sdl-fdmus-owner.bin',pointer('fdmus.data'),pointer('fdmus.size'))
        observation=decode(player,fdmus,layout,original,track,loop)
        record.update(layout=layout,player_snapshot=player_fact,fdmus_snapshot=fdmus_fact,
            all_reads_in_one_stopped_attach=True,no_new_product_dump_hook=True)
    if cdb_protocol.sha(exe)!=record['symbols']['exe_sha256'] or cdb_protocol.sha(pdb)!=record['symbols']['pdb_sha256']:
        raise ValueError('CDB音乐观察期间App/PDB身份变化')
    observation['scope']['all_target_threads_stopped_during_reads']=True
    observation['scope']['PDB_collection_performed_by_this_decode']=False
    observation['scope']['PDB_collection_performed_by_take']=True
    return observation,record
