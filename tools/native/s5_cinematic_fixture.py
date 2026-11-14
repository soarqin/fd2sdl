"""S5新cinematic代表的独立原资源模型；不读生产JSON/不调用SDL。

24:523e7 NoTown、1088d/3396a；20:1088d及240fa/233c6/24336。
完整late SAV只读派生，未写构造洞沿既有80B槽保留并逐项声明。
"""
from pathlib import Path
import struct

from native import s3_fixture as shared
from audit_g4_render_source import archive_entries, text_fragments
from fd2_g3_prepare_oracle import valid_container,write_slot
from fd2_stage1_complete_oracle import tables,template_record,apply_script
from fd2_transition_oracle import LATE_SAVE_SHA256
from run_graphics_tests import decode_save
from run_original_end_turn import META,UNITS
from run_original_focus import native_focus_reference
from run_original_item_list import drop_derived
from run_g4_runtime_check import encode

SCENARIOS=('stage24-notown-shake','stage20-sixhits-sheet101')

def item_prefix_slot(unit,item):
    """2aedb→1b8a6/1b722：先数八槽visible数，再直接读raw[0..count)。"""
    count=sum(not unit[10+2*i]&128 for i in range(8))
    return next((i for i in range(count) if unit[11+2*i]==item),None)


def six_presence(records):
    return sum(item_prefix_slot(unit,item) is not None for unit in records[:16] for item in range(209,215))

def source(original):
    original=Path(original);path=original/'FD2.SAV';raw=path.read_bytes()
    if shared.digest(raw)!=LATE_SAVE_SHA256:raise ValueError('S5cin只接受登记late完整SAV')
    plain=valid_container(decode_save(path))
    if plain[META+2]!=29 or plain[META+9]!=31:raise ValueError('late stage29/31roster来源不同')
    data=tables(original)
    for stage in (20,24):shared.add_stage(data,original,stage)
    return original,path,raw,plain,data

def constructor(source,state,data,stage):
    row=data['stages'][stage];metadata=row['metadata'];quota,effective=metadata[1:3]
    if (stage,quota,effective) not in ((20,16,80),(24,16,70)) or state['count']<quota:
        raise ValueError('S5cin原部署/模板有效域不同')
    records=[];masks=[]
    for i in range(quota):
        unit=bytearray(state['roster'][i*80:(i+1)*80]);unit[:2]=bytes(row['placement'][effective+i])
        unit[3:5]=bytes(2);unit[6]=2;unit[49]=255;unit[34:40]=bytes(6)
        struct.pack_into('<4H',unit,72,*drop_derived(unit,data['items']))
        mask=bytearray([1]*80);mask[2]=0;records.append(unit);masks.append(mask)
    shared.append_group(records,masks,data,stage,0)
    expected={20:75,24:62}[stage]
    if len(records)!=expected:raise ValueError('原group0人数来源不同')
    return records,masks

def speech_view(original,records,stage,fragments,camera,focus):
    fields=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes())
    dimensions=struct.unpack_from('<HH',fields[stage*3]);texts=archive_entries((Path(original)/'FDTXT.DAT').read_bytes())
    for fragment in fragments:
        tokens=text_fragments(texts[stage+1])[fragment]
        for i,token in enumerate(tokens[:-1]):
            if token in (-19,-20):
                actor=tokens[i+1]
                if actor>=len(records):raise ValueError('原文本直接raw越界')
            elif token in (-17,-18):
                actor=next((n for n,u in enumerate(records) if u[8]==tokens[i+1] and not u[5]&1),None)
                if actor is None:continue
            else:continue
            point=list(records[actor][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    return list(camera),list(focus)

def entry24(plain,state,data,original):
    records,masks=constructor(plain,state,data,24)
    camera,focus=speech_view(original,records,24,(1,2),[5,0],[5,0])
    dims=struct.unpack_from('<HH',archive_entries((Path(original)/'FDFIELD.DAT').read_bytes())[72])
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,dims)['camera'];focus=point
    view=bytearray(shared.as_view(plain,records,24,1,camera,focus,state['count']))
    view[0x8a3:0x12a3]=state['roster'];view[0x30a3:0x30c3]=bytes(32)
    return shared.checksum(view),records,masks

def derive(original,scenario):
    if scenario not in SCENARIOS:raise ValueError('未知S5cin代表')
    original,path,raw,plain,data=source(original);stage=24 if scenario==SCENARIOS[0] else 20
    count=17 if stage==24 else 16
    donors=([0,1,3,6,17,20,21,24,2,4,5,7,8,9,10,11,12] if stage==24 else
            [0,1,3,17,5,6,7,8,9,10,11,12,13,14,15,16])
    roster=bytearray(plain[0x8a3:0x12a3])
    for target,donor in enumerate(donors):
        roster[target*80:(target+1)*80]=plain[0x8a3+donor*80:0x8a3+(donor+1)*80]
    state={'stage':stage,'count':count,'roster':bytes(roster),
        'resource':struct.unpack_from('<I',plain,META+10)[0],
        'options':bytes(plain[META+i] for i in (15,14,16,17))}
    if data['raw'][0x523e7+stage]!=(1 if stage==24 else 0):raise ValueError('原NoTown dispatch不同')
    changed=write_slot(plain,state,0)
    p={'original':original,'source':changed,'encoded':encode(changed),'manual_state':state,'data':data}
    manifest={'scenario':scenario,'controlled_fixture':True,'natural_whole_stage_claimed':False,
        'runtime_alias_or_API_injection':False,'source':{'path':str(path),'sha256':shared.digest(raw),
            'mtime_ns':path.stat().st_mtime_ns},'source_plain_sha256':shared.digest(plain),
        'source_family':'3396a NoTown→bank88/24b4d' if stage==24 else '240fa sixpresence→24336',
        'unused_persistent_tail_preserved_from':count*80,'selected_roster_count':count,
        'source_complete_roster_copies':[{'target_raw':target,'source_raw':donor,
            'unit_id':roster[target*80+7],'text_id':roster[target*80+8],
            'sha256':shared.digest(roster[target*80:(target+1)*80])} for target,donor in enumerate(donors)]}
    if stage==20:
        records,masks=constructor(plain,state,data,20)
        # 实际source-derived构造已知字段；未写洞从原SAV同slot保留，绝不默认0。
        holes=[]
        for n,(record,mask) in enumerate(zip(records,masks)):
            old=plain[UNITS+n*80:UNITS+(n+1)*80]
            for j,known in enumerate(mask):
                if not known:record[j]=old[j];holes.append([n,j])
            if record[6]==0:record[5]|=1
            for slot in range(8):
                if 209<=record[11+2*slot]<=214:record[10+2*slot:12+2*slot]=bytes((128,255))
        rare_slots=[]
        for n in range(6):
            slot=next((k for k in range(8) if records[n][10+2*k]&128),None)
            if slot is None:raise ValueError('sixpresence受控源缺原1bb8c首hidden追加槽')
            records[n][10+2*slot:12+2*slot]=bytes((0,209+n));rare_slots.append(slot)
        if any(records[n][5]&1 or not struct.unpack_from('<H',records[n],64)[0] for n in (0,16,17)):
            raise ValueError('原result20保护raw0/16/17必须visible正HP')
        if six_presence(records)!=6:raise ValueError('sixpresence原raw前缀独立源计数不为6')
        fields=archive_entries((original/'FDFIELD.DAT').read_bytes());dims=struct.unpack_from('<HH',fields[60])
        focus=list(records[0][:2]);camera=native_focus_reference([0,0],[0,0],focus,dims)['camera']
        changed=bytearray(shared.as_view(plain,records,20,1,camera,focus,16));changed[0x8a3:0x12a3]=roster
        changed[:len(data['stages'][20]['metadata'])]=data['stages'][20]['metadata']
        changed[0x30a3:0x30c3]=bytes(32);changed=shared.checksum(changed)
        p.update(source=changed,encoded=encode(changed),records=records,masks=masks)
        manifest.update(constructed_unknown_holes_preserved_from_same_SAV_slot=holes,
            source_init_claim='controlled active snapshot from complete1088d known writes, no naturalentry20 claim',
            victory_trigger='enemy flags.hidden then real selection cancel→original stage result',
            rare_items={'raw0_to5_first_hidden_slots':rare_slots,'item_ids':[209,210,211,212,213,214],
                'placement_source':'1bb8c first flag80 slot; no existing visible item overwritten',
                'condition':'2aedb inventory_count then raw[0,count), sum(actor,item) presence==6, not distinct item set'})
    manifest.update(derived_plain_sha256=shared.digest(changed),
        plaintext_patches=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(plain,changed)) if a!=b])
    p['manifest']=manifest;return p

def victory20_records(p,kind='sdl'):
    """原CONTINUE cache政策→233c6/库存写点→63/64；只读源模型，不读SDL。"""
    units=shared.loaded_units(p['source'],kind);raw=p['data']['raw']
    if len(units)!=75 or six_presence(units)!=6:raise ValueError('240fa true分支需实际原前缀sixpresence')
    for unit in units:unit[5]&=127
    for n in range(25):units[n][0]=raw[0x52228+n];units[n][1]=raw[0x52241+n];units[n][3]=raw[0x5225a+n]
    units[25][:2]=bytes((23,14));units[25][3]=1
    for item in range(209,215):
        for unit in units[:16]:
            slot=item_prefix_slot(unit,item)
            if slot is not None:
                unit[10+2*slot:24]=unit[12+2*slot:26];unit[24]=128
    for unit in units:
        if unit[6]!=2:continue
        slot=next((k for k in range(8) if unit[10+2*k]&128),None)
        if slot is not None:unit[10+2*slot:12+2*slot]=bytes((0,100));break
    for sid in (63,64):apply_script(units,shared.script(p['data'],p['original'],sid))
    return units


def victory20_settlement(p,kind):
    """240fa完整尾：112a5(24)、112a5(23)先发生，再11506 active外层同步。"""
    units=victory20_records(p,kind);source=p['source'];data=p['data']
    count=source[META+9];roster=bytearray(source[0x8a3:0x12a3])
    if count!=16:raise ValueError('native20固定16→18原队伍域不同')
    for identity in (24,23):
        at=count*80;roster[at:at+80]=shared.persistent_create(roster[at:at+80],identity,data);count+=1
    for active in units:
        for raw in range(count):
            if active[8]!=roster[80*raw+8] or active[8]==0 and active[5]&1:continue
            unit=bytearray(active);unit[34:40]=bytes(6);unit[5]&=1
            if not unit[5]&1:unit[64:66]=unit[66:68]
            unit[68:70]=unit[70:72];struct.pack_into('<4H',unit,72,*drop_derived(unit,data['items']))
            roster[80*raw:80*(raw+1)]=unit
    return {'stage':21,'count':count,'roster':bytes(roster),
        'resource':struct.unpack_from('<I',source,META+10)[0],
        'options':bytes(source[META+i] for i in (15,14,16,17))}
