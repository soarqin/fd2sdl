"""S5后批正式代表的原资源模型；不读取生产冷图或SDL输出。

250cc/11506/25089→27及33c9d；25757→1088d(30)/31槽文本。
沿用登记late完整SAV，受控active26用原16部署+47group0构造，全部修改登记。
不把该近胜状态称自然到达，不为未写字节制造默认值。
"""
from pathlib import Path
import struct

from native import s3_fixture as state
from native.s5_cinematic_fixture import item_prefix_slot, speech_view
from audit_g4_render_source import archive_entries
from dump_fd2_fixup_table import collect_internal_offset_fixups
from fd2_stage1_complete_oracle import tables, apply_script
from fd2_transition_oracle import LATE_SAVE_SHA256
from fd2_g3_prepare_oracle import valid_container, write_slot
from run_graphics_tests import decode_save
from run_original_end_turn import META, UNITS
from run_original_item_list import drop_derived
from run_original_focus import native_focus_reference
from run_g4_runtime_check import encode

SCENARIOS=('stage26-victory27-notown','stage29-credits-montage-terminal','stage26-bad-ending-sdl')


def script(data,original,identity):
    """1366a按header组数读流；84后四字节为S1已证未引用gap，显式保留。"""
    if identity not in (82,83,84,85,88,89):raise ValueError('未登记late脚本')
    fixups=collect_internal_offset_fixups(Path(original)/'FD2.EXE')
    obj,_,start=fixups[0x627d8+4*identity];nobj,_,end=fixups[0x627d8+4*(identity+1)]
    if obj!=3 or nobj!=3 or not start<end:raise ValueError('late脚本原fixup不同')
    raw=bytes(data['raw'][start:end]);at=1;blocks=[]
    for _ in range(raw[0]):
        if at+2>len(raw):raise ValueError('脚本组header截断')
        mode,count=raw[at:at+2];at+=2
        if at+2*count>len(raw):raise ValueError('脚本pair截断')
        pairs=[tuple(raw[at+i*2:at+i*2+2]) for i in range(count)];at+=2*count
        if any(direction>3 for _,direction in pairs):raise ValueError('脚本direction越界')
        blocks.append((mode,pairs))
    excluded=raw[at:]
    if excluded!=(bytes.fromhex('88010102') if identity==84 else b''):
        raise ValueError('脚本计数组外字节不是已登记gap')
    return {'blocks':blocks,'va':hex(start),'sha256':state.digest(raw[:at]),
        'length':at,'excluded_after_count':{'start':hex(start+at),'raw_hex':excluded.hex()}}


def derive(original,scenario):
    if scenario not in SCENARIOS:raise ValueError('未知late代表')
    original=Path(original);path=original/'FD2.SAV';raw=path.read_bytes()
    if state.digest(raw)!=LATE_SAVE_SHA256:raise ValueError('late SAV原件身份不同')
    plain=valid_container(decode_save(path));data=tables(original)
    if plain[META:META+3]!=bytes((1,27,29)) or plain[META+9]!=31:raise ValueError('late原27active/31roster域不同')
    for stage in (26,27,29,30):state.add_stage(data,original,stage)
    stage=29 if scenario==SCENARIOS[1] else 26
    changed=bytearray(plain);changed[:len(data['stages'][stage]['metadata'])]=data['stages'][stage]['metadata']
    changed[META+2]=stage;records=state.units(plain)
    holes=[]
    if stage==26:
        prepared={'stage':26,'count':31,'roster':bytes(plain[0x8a3:0x12a3])}
        records,masks=deployment({'data':data},prepared,26)
        for n,(unit,mask)in enumerate(zip(records,masks)):
            for j,known in enumerate(mask):
                if not known:unit[j]=plain[UNITS+n*80+j];holes.append([n,j])
        # 输入明确重排到250cc本身的真实布局；不假称已跑entry26或nearest-free。
        for n in range(1,16):
            records[n][0]=data['raw'][0x52306+n];records[n][1]=data['raw'][0x52316+n]
        cache=[]
        for unit in records:
            if unit[7]not in cache:cache.append(unit[7])
            unit[2]=cache.index(unit[7])
        dimensions=struct.unpack_from('<HH',archive_entries((original/'FDFIELD.DAT').read_bytes())[stage*3])
        point=list(records[0][:2]);camera=native_focus_reference([0,0],[0,0],point,dimensions)['camera']
        changed[META+1]=len(records);changed[META+3:META+9]=bytes((*camera,*point,point[0]-camera[0],point[1]-camera[1]))
    for unit in records:
        if unit[6]==0:unit[5]|=1
    item_slot=None
    if scenario==SCENARIOS[0]:
        item_slot=item_prefix_slot(records[0],100)
        if item_slot is None:
            item_slot=next((i for i in range(8) if records[0][10+2*i]&128),None)
            if item_slot is None:raise ValueError('raw0无原1bb8c首hidden槽放100')
            records[0][10+2*item_slot:12+2*item_slot]=bytes((0,100))
        if item_prefix_slot(records[0],100)!=item_slot:raise ValueError('原count前缀看不到item100')
    if scenario==SCENARIOS[2]:
        # 2aedb按count前缀查找；每次沿1b8e7原压槽，不能只清一个位置或查八槽集合。
        for unit in records[:16]:
            for _ in range(8):
                slot=item_prefix_slot(unit,100)
                if slot is None:break
                unit[10+2*slot:24]=unit[12+2*slot:26];unit[24]=128
            if item_prefix_slot(unit,100)is not None:raise ValueError('26坏臂仍含prefix100')
    for n,unit in enumerate(records):changed[UNITS+n*80:UNITS+(n+1)*80]=unit
    changed=state.checksum(changed)
    manual={'stage':stage,'count':31,'roster':bytes(plain[0x8a3:0x12a3]),
        'resource':struct.unpack_from('<I',plain,META+10)[0],
        'options':bytes(plain[META+i] for i in (15,14,16,17))}
    changed=write_slot(changed,manual,0)
    fields=archive_entries((original/'FDFIELD.DAT').read_bytes());w,h=struct.unpack_from('<HH',fields[stage*3])
    if any(u[0]>=w or u[1]>=h for u in records):raise ValueError('借用late active坐标超出目标原地图')
    if records[0][5]&0x85 or records[0][6]!=2 or not int.from_bytes(records[0][64:66],'little'):
        raise ValueError('raw0不能走真实controller选择')
    manifest={'scenario':scenario,'controlled_fixture':True,'natural_whole_stage_claimed':False,
        'runtime_alias_or_API_injection':False,'source':{'path':str(path),'sha256':state.digest(raw),'mtime_ns':path.stat().st_mtime_ns},
        'source_plain_sha256':state.digest(plain),'derived_plain_sha256':state.digest(changed),
        'injection':'26原16部署/47group0已初始化写集+同SAV槽未知洞，玩家显式移至250cc原数组；29借原27完整active；敌side0置hidden，真实选择取消触发结果',
        'active_records_origin':'original1088d/10c50 source constructors (26)'if stage==26 else'registered late original27 complete80B records',
        'constructed_unknown_holes_preserved_from_same_SAV_slot':holes,
        'item100_raw0_slot':item_slot,'source_stage':stage,'roster_count':31,'active_count':len(records),
        'plaintext_patches':[{'offset':i,'before':a,'after':b}for i,(a,b)in enumerate(zip(plain,changed))if a!=b]}
    return {'original':original,'source':changed,'encoded':encode(changed),'manual_state':manual,'data':data,'manifest':manifest}


def victory_records(p,kind):
    units=state.loaded_units(p['source'],kind);raw=p['data']['raw'];stage=p['source'][META+2]
    if stage not in (26,29) or len(units)!=(63 if stage==26 else 27):raise ValueError('late胜利模型域不同')
    for unit in units:unit[5]&=127
    count=16 if stage==26 else 20
    xs,ys,dirs=(0x52306,0x52316,None)if stage==26 else(0x52327,0x5233b,0x5234f)
    for i in range(count):
        if stage==26:units[i][5]=0
        units[i][0]=raw[xs+i];units[i][1]=raw[ys+i];units[i][3]=2 if dirs is None else raw[dirs+i]
    sequence=(82,84,82)if p['manifest']['scenario']==SCENARIOS[2]else(82,83,84)if stage==26 else(88,)
    for identity in sequence:apply_script(units,script(p['data'],p['original'],identity))
    if p['manifest']['scenario']==SCENARIOS[2]:units[1][:2]=bytes((255,255))
    return units


def preparation27(p,kind):
    if p['source'][META+2]!=26:raise ValueError('仅250cc好分支导出27')
    units=victory_records(p,kind);roster=bytearray(p['source'][0x8a3:0x12a3]);count=p['source'][META+9]
    for active in units:
        for i in range(count):
            if active[8]!=roster[i*80+8] or active[8]==0 and active[5]&1:continue
            unit=bytearray(active);unit[34:40]=bytes(6);unit[5]&=1
            if not unit[5]&1:unit[64:66]=unit[66:68]
            unit[68:70]=unit[70:72];struct.pack_into('<4H',unit,72,*drop_derived(unit,p['data']['items']))
            roster[i*80:(i+1)*80]=unit
    # 25089在next27后仅清flags、HP=HPmax、MP=MPmax，不重写其它字节。
    for i in range(count):
        roster[i*80+5]=0;roster[i*80+64:i*80+66]=roster[i*80+66:i*80+68]
        roster[i*80+68:i*80+70]=roster[i*80+70:i*80+72]
    return {**p['manual_state'],'stage':27,'roster':bytes(roster)}


def selected27(prepared):
    if prepared['stage']!=27 or prepared['count']!=31:raise ValueError('NoTown27固定31持久域不同')
    # 1..19原序stable partition，mandatory text9最后命中移至slot1。
    roster=prepared['roster'];ids=[roster[i*80+8]for i in range(1,20)]
    if 9 not in ids:raise ValueError('NoTown27选阵缺原强制text9')
    found=max(i for i in range(1,31)if roster[i*80+8]==9)
    order=[0,found]+[i for i in range(1,31)if i!=found]
    return {**prepared,'roster':b''.join(roster[i*80:(i+1)*80]for i in order)+roster[31*80:]}


def deployment(p,prepared,stage):
    row=p['data']['stages'][stage];quota,effective=row['metadata'][1:3];records=[];masks=[]
    if (stage,quota,effective)not in ((26,16,80),(27,20,60),(30,31,1)):raise ValueError('late部署原metadata域不同')
    for i in range(quota):
        unit=bytearray(prepared['roster'][i*80:(i+1)*80]);unit[:2]=bytes(row['placement'][effective+i])
        unit[3:5]=bytes(2);unit[6]=2;unit[49]=255;unit[34:40]=bytes(6)
        struct.pack_into('<4H',unit,72,*drop_derived(unit,p['data']['items']))
        mask=bytearray([1]*80);mask[2]=0;records.append(unit);masks.append(mask)
    state.append_group(records,masks,p['data'],stage,0)
    return records,masks


def entry27(p,prepared):
    selected=selected27(prepared);records,masks=deployment(p,selected,27)
    for unit in records[:20]:unit[5]=0 if int.from_bytes(unit[64:66],'little') else 1
    for _ in range(3):apply_script(records,script(p['data'],p['original'],85))
    for unit in records:unit[3]=0
    camera,focus=speech_view(p['original'],records,27,(0,),[29,15],[29,15])
    state.append_group(records,masks,p['data'],27,6);state.append_group(records,masks,p['data'],27,7)
    # 原35b78先pan0,16，再pan7,16；末12d7b以当时focus跟随raw0。
    focus=[focus[0]+7-camera[0],focus[1]+16-camera[1]];camera=[7,16]
    dimensions=struct.unpack_from('<HH',archive_entries((p['original']/'FDFIELD.DAT').read_bytes())[81])
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    view=bytearray(state.as_view(p['source'],records,27,1,camera,focus,31));view[0x8a3:0x12a3]=selected['roster']
    view[0x30a3:0x30c3]=bytes(32)
    return state.checksum(view),records,masks,selected


def credits30(p,kind):
    if p['source'][META+2]not in(26,29):raise ValueError('仅250cc坏臂或25757尾声路径')
    roster=bytearray(p['source'][0x8a3:0x12a3])
    for i in range(31):
        roster[80*i+5]=0;roster[80*i+64:80*i+66]=roster[80*i+66:80*i+68]
        roster[80*i+68:80*i+70]=roster[80*i+70:80*i+72]
    prepared={**p['manual_state'],'stage':30,'roster':bytes(roster)}
    records,masks=deployment(p,prepared,30)
    if p['source'][META+2]==29:apply_script(records,script(p['data'],p['original'],89))
    return records,masks,prepared
