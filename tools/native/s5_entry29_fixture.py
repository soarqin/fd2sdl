"""33e3c正式入场代表的独立原资源投影；不读生产冷图或SDL输出。

原1088d的20部署+7 group0、1366a脚本87、33f78七个确切caller及3313b尾。
NoTown选择复用已证2b777/2b439/2b843模型，全部原持久80B和未知尾保留。
"""
from pathlib import Path
import struct

from native import s3_fixture as state
from native import s5_late_fixture as late
from native.s5_cinematic_fixture import speech_view
from fd2_g3_prepare_oracle import model_action
from fd2_stage1_complete_oracle import apply_script
from run_original_focus import native_focus_reference
from run_original_item_list import drop_derived
from audit_g4_render_source import archive_entries
from dump_fd2_fixup_table import collect_internal_offset_fixups

SCENARIO='stage29-title-load-seven-teleports'
ENTRY_SHA256='d541bf343db065dc9c95429a389628b7a68fb74e8144499a13b51362d05726da'
ENTRY_RAW_SHA256='9deff89a0dd278d614cf5ab425685dcfbd71720ee4c9bd43635b0eb7a3743fbd'
CALL_SITES=(0x33ea4,0x33eb2,0x33ec0,0x33ece,0x33f45,0x33f53,0x33f61)
SHARED_TAIL=bytes.fromhex('e8a403feff6a00e834fcfdff83c404c3')


def source_calls(data,original):
    raw=data['raw']
    if state.digest(bytes(raw[0x33e3c:0x33f78]))!=ENTRY_RAW_SHA256:
        raise ValueError('33e3c完整原入口字节不同')
    if bytes(raw[0x3313b:0x3314b])!=SHARED_TAIL:
        raise ValueError('33e3c显式JMP共享尾不再是134e4→12d7b(0)→return')
    canonical=bytearray(raw[0x33e3c:0x33f78])
    for address,(_,_,target)in collect_internal_offset_fixups(Path(original)/'FD2.EXE').items():
        if 0x33e3c<=address<0x33f78:
            if address+4>0x33f78:raise ValueError('33e3c fixup跨越完整函数界')
            struct.pack_into('<I',canonical,address-0x33e3c,target)
    if state.digest(canonical)!=ENTRY_SHA256:raise ValueError('33e3c原fixup与canonical Domain身份不同')
    result=[]
    # 完整CFG已确认这七个直线调用；只解码各自紧邻的已证三push ABI。
    for pc in CALL_SITES:
        args=bytes(raw[pc-6:pc])
        if args[::2]!=b'\x6a'*3 or raw[pc]!=0xe8 or pc+5+struct.unpack_from('<i',raw,pc+1)[0]!=0x33f78:
            raise ValueError('33e3c明确caller的push/CALL ABI改变')
        y,x,actor=args[1::2]
        result.append({'source_pc':hex(pc),'actor':actor,'destination':[x,y],'draw':[x,y]})
    return result


def derive(original):
    p=late.derive(original,late.SCENARIOS[1])
    p['manifest']['scenario']=SCENARIO
    p['manifest']['entry29_input']='受控原late active29仅提供标题CONTINUE资格；本场明确选择slot0 LOAD→NoTown→新入场'
    p['manifest']['natural_whole_stage_claimed']=False
    p['manifest']['teleport_calls']=source_calls(p['data'],p['original'])
    p['manifest']['entry_source_relocated_Domain_sha256']=ENTRY_SHA256
    p['manifest']['entry_source_raw_unrelocated_sha256']=ENTRY_RAW_SHA256
    p['manifest']['shared_tail']={'va':'0x3313b','raw_hex':SHARED_TAIL.hex(),
        'operations':['134e4 all direction reset','12d7b(0) focus raw0','return'],
        'owner_evidence':'complete S2 chunks32e8c/33049:3312d..33140 and33140..3314b'}
    p['manifest']['callback82_not_natural_at_entry']={'turn_slot0_hex':p['data']['stages'][29]['metadata'][3:6].hex(),
        'meaning':'turn255/action82/phase0；本场turn1不触该绑定'}
    return p


def selected(p):
    prepared,proof=model_action(p['manual_state'],{'kind':'deployment','selected':list(range(1,20)),'confirm':True},{})
    if prepared['stage']!=29 or prepared['count']!=31 or proof['outcome']!='accepted':
        raise ValueError('原NoTown29二十人选择未闭合')
    return prepared,proof


def entry(p):
    prepared,reorder=selected(p);row=p['data']['stages'][29]
    quota,effective=row['metadata'][1:3]
    if (quota,effective,len(row['placement']))!=(20,70,90):raise ValueError('29原部署容量不同')
    records=[];masks=[]
    for i in range(quota):
        unit=bytearray(prepared['roster'][80*i:80*(i+1)])
        unit[:2]=bytes(row['placement'][effective+i]);unit[3:5]=bytes(2);unit[6]=2;unit[49]=255;unit[34:40]=bytes(6)
        struct.pack_into('<4H',unit,72,*drop_derived(unit,p['data']['items']))
        mask=bytearray([1]*80);mask[2]=0;records.append(unit);masks.append(mask)
    state.append_group(records,masks,p['data'],29,0)
    if len(records)!=27:raise ValueError('29须20部署+7真实group0')
    script=state.script(p['data'],p['original'],87);apply_script(records,script)
    fields=archive_entries((p['original']/'FDFIELD.DAT').read_bytes())
    dimensions=struct.unpack_from('<HH',fields[87]);camera=[0,0];focus=[0,0]
    snapshots={};teleports=[]
    def pan(x,y):
        nonlocal camera,focus
        focus=[focus[0]+x-camera[0],focus[1]+y-camera[1]];camera=[x,y]
    def dialogue(fragment):
        nonlocal camera,focus
        camera,focus=speech_view(p['original'],records,29,(fragment,),camera,focus)
        snapshots[fragment]=[bytearray(u)for u in records]
    def transfer(call):
        nonlocal camera,focus
        point=call['destination'];camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=list(point)
        teleports.append({**call,'camera':list(camera),'focus':list(focus),'before_records':[bytearray(u)for u in records]})
        records[call['actor']][:2]=bytes(point)
    pan(16,19);dialogue(0);pan(16,1)
    for call in p['manifest']['teleport_calls'][:4]:transfer(call)
    dialogue(1)  # 361b0仅加白/等待/恢复，无units/camera写点。
    dialogue(2);pan(16,14)
    for call in p['manifest']['teleport_calls'][4:]:transfer(call)
    for unit in records:unit[3]=0
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    view=bytearray(state.as_view(p['source'],records,29,1,camera,focus,31))
    view[0x8a3:0x12a3]=prepared['roster'];view[0x30a3:0x30c3]=bytes(32)
    return {'view':state.checksum(view),'records':records,'masks':masks,'prepared':prepared,'reorder':reorder,
        'script':script,'story_records':snapshots,'teleports':teleports,'dimensions':dimensions}
