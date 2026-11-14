"""S5独立原资源夹具：原late SAV持久队伍→stage21正式手动LOAD/入场。

不导入生产冷图；1088d的16部署/50 group0与入口pan/script67/story0/focus
依据已冻结完整S2 caller，数据与移动流逐次取原DAT/EXE。只声明受控来源。
"""
from __future__ import annotations
import struct
from pathlib import Path

from native import s3_fixture as shared
from audit_g4_render_source import archive_entries, text_fragments
from fd2_g3_prepare_oracle import valid_container, write_slot
from fd2_stage1_complete_oracle import tables, apply_script
from fd2_transition_oracle import LATE_SAVE_SHA256
from run_graphics_tests import decode_save
from run_original_end_turn import META, UNITS
from run_original_focus import native_focus_reference
from run_original_item_list import drop_derived
from run_g4_runtime_check import encode


def prepare(original):
    original=Path(original);source=original/'FD2.SAV';raw=source.read_bytes()
    if shared.digest(raw)!=LATE_SAVE_SHA256:
        raise ValueError('S5代表要求既有已登记late SAV，不接受任意同名用户档')
    plain=valid_container(decode_save(source))
    if plain[META+2]!=29 or plain[META+9]!=31:
        raise ValueError('late SAV实际stage29/31持久记录不同')
    state={'stage':21,'count':16,'roster':bytes(plain[0x8a3:0x12a3]),
           'resource':struct.unpack_from('<I',plain,META+10)[0],
           'options':bytes(plain[META+i] for i in (15,14,16,17))}
    changed=write_slot(plain,state,0)
    data=tables(original);shared.add_stage(data,original,21)
    manifest={'controlled_fixture':True,'natural_whole_stage_claimed':False,
        'source':{'path':str(source),'sha256':shared.digest(raw),'mtime_ns':source.stat().st_mtime_ns},
        'source_plain_sha256':shared.digest(plain),'derived_plain_sha256':shared.digest(changed),
        'plaintext_patches':[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(plain,changed)) if a!=b],
        'injection':'manual slot0 stage21/count16 source prefix; complete original roster-area tail preserved; active source unchanged',
        'runtime_alias_or_API_injection':False}
    return {'original':original,'source':changed,'encoded':encode(changed),'manual_state':state,
            'manifest':manifest,'data':data}


def entry21(source,state,data,original):
    row=data['stages'][21];metadata=row['metadata'];quota,effective=metadata[1:3]
    if (quota,effective,state['count'])!=(16,70,16):
        raise ValueError('entry21独立原资源域不是16部署/70模板/16来源前缀')
    records=[];masks=[]
    for raw in range(quota):
        unit=bytearray(state['roster'][raw*80:(raw+1)*80])
        unit[:2]=bytes(row['placement'][effective+raw]);unit[3:5]=bytes(2)
        unit[6]=2;unit[49]=255;unit[34:40]=bytes(6)
        struct.pack_into('<4H',unit,72,*drop_derived(unit,data['items']))
        mask=bytearray([1]*80);mask[2]=0
        records.append(unit);masks.append(mask)
    shared.append_group(records,masks,data,21,0)
    camera=[16,28];focus=[16,28]
    apply_script(records,shared.script(data,original,67))
    for unit in records:unit[3]=0
    fields=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes())
    dimensions=struct.unpack_from('<HH',fields[63])
    texts=archive_entries((Path(original)/'FDTXT.DAT').read_bytes())
    tokens=text_fragments(texts[22])[0]
    for index,token in enumerate(tokens[:-1]):
        if token in (-19,-20):
            actor=tokens[index+1]
            if actor>=len(records):raise ValueError('原story0 actor引用越界')
        elif token in (-17,-18) and tokens[index+1]!=39:
            actor=next((i for i,u in enumerate(records) if u[8]==tokens[index+1] and not u[5]&1),None)
            if actor is None:continue
        else:continue
        point=list(records[actor][:2])
        camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    if len(records)!=66:raise ValueError('entry21独立16+50人数不同')
    view=bytearray(shared.as_view(source,records,21,1,camera,focus,16));view[0x8a3:0x12a3]=state['roster']
    return shared.checksum(view),records,masks
