"""F1：原81合并排程、82 raw域及22253 inactive槽实际读写。

依据362c5/362e8、1b6b7、13565/20bf5、22253完整原码；本工具仅连接
已明确的owner/调用点，不代签自然输入通关。原资源只读，输出独占创建。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
from audit_stage_data import archive_entries,SOURCE_HASHES
from verify_f1_boundaries import IMAGE_SHA,verify_source


def sha(data):return hashlib.sha256(data).hexdigest()


def teleport_access(image,other,actor,dx=21,dy=18,drawx=21,drawy=18):
    """完整22253及三个图形helper实际执行；terrain/actor绘制是无写替身。"""
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE,UC_MEM_WRITE
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x800000);u.mem_write(0,image)
    actors,page,temp,lut,bank,lmi,stack,stop=0x200000,0x300000,0x400000,0x500000,0x510000,0x520000,0x700000,0x7ffff0
    size=456*336
    def rd(a):return struct.unpack('<I',u.mem_read(a,4))[0]
    def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def args(n):return [rd(u.reg_read(UC_X86_REG_ESP)+4+4*i) for i in range(n)]
    def ret(v=0,extra=0):
        sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EIP,rd(sp));u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EAX,v)
    raw=bytearray(b'\xa5'*(96*80))
    for i in range(29):raw[i*80:(i+1)*80]=bytes(80)
    u.mem_write(actors,bytes(raw));u.mem_write(lut,other[3][1]);u.mem_write(bank,other[81][1]);u.mem_write(lmi,other[6][1])
    for a,v in ((0x53a45,actors),(0x53beb,29),(0x53a49,page),(0x53a6d,lut),(0x53ad1,lmi),
                (0x53aa9,16),(0x53aad,14),(0x53ab9,5),(0x53abd,4),(0x54133,0)):
        put(a,v)
    u.mem_write(0x53ef1,b'\0');u.mem_write(0x51e62,b'\x01')
    put(stack,stop)
    for i,value in enumerate((actor,dx,dy,drawx,drawy)):put(stack+4+4*i,value)
    u.reg_write(UC_X86_REG_ESP,stack)
    accesses=[];pc_seen=set();pc_now=0;pending_write=None
    before=bytes(u.mem_read(actors+actor*80,80))
    prefixes=[{'tag':'begin','record_hex':before.hex()}]
    def code(_,pc,n,data):
        nonlocal pc_now,pending_write
        if pending_write is not None:
            prefixes.append({'tag':'xy_write','pc':pending_write,
                             'record_hex':bytes(u.mem_read(actors+actor*80,80)).hex()})
            pending_write=None
        pc_now=pc;pc_seen.add(pc)
        if pc==0x3702f:ret(u.reg_read(UC_X86_REG_EAX),4)
        elif pc==0x111ba:
            if args(3)[1:]!=[0,81]:raise ValueError('unexpected teleport resource')
            ret(bank)
        elif pc==0x3706e:
            if args(1)!=[size]:raise ValueError('unexpected teleport allocation')
            ret(temp)
        elif pc in (0x3776e,0x11eee,0x127a9,0x17aa9,0x3790a):ret()
        elif pc==0x3771c:
            dst,src,count=args(3);u.mem_write(dst,bytes(u.mem_read(src,count)));ret(dst)
    def access(_,kind,address,n,value,data):
        nonlocal pending_write
        base=actors+actor*80
        if address<base+80 and address+n>base:
            accesses.append({'pc':pc_now,'write':kind==UC_MEM_WRITE,'offset':address-base,'size':n,'value':value if kind==UC_MEM_WRITE else None})
            if kind==UC_MEM_WRITE:pending_write=pc_now
    u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,access)
    u.emu_start(0x22253,stop,count=30000000)
    if u.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError('teleport failed to return')
    after=bytes(u.mem_read(actors+actor*80,80));expected=bytes((dx,dy))+b'\xa5'*78
    if (after!=expected or any(not row['write'] for row in accesses) or
            [(row['offset'],row['size'],row['value']) for row in accesses]!=[(0,1,dx),(1,1,dy)]):
        raise ValueError('teleport acquired an inactive-record read or new write')
    prefixes.append({'tag':'return','record_hex':after.hex()})
    return {'actor':actor,'arguments':[actor,dx,dy,drawx,drawy],
            'active_count':29,'allocation_capacity':96,'record_accesses':accesses,
            'before_hex':before.hex(),'after_hex':after.hex(),'record_prefixes':prefixes,
            'target_record_reads':0,'only_xy_written':True,'whole_function_instructions':len(pc_seen),
            'substitutes':['Watcom stack probe','libc memmove/allocate/free','terrain and active actor graphics','delays; audio option0'],
            'not_claimed':['pixels','sound','OS timing','complete original App']}


def coalesced_caller(image):
    """两个原callback81在同一turn写后，原82的确切参数；不补原玩法逻辑。"""
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x800000);u.mem_write(0,image)
    actors,state,meta,stack,stop=0x200000,0x210000,0x220000,0x700000,0x7ffff0
    def rd(a):return struct.unpack('<I',u.mem_read(a,4))[0]
    def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def args(n):return [rd(u.reg_read(UC_X86_REG_ESP)+4+4*i) for i in range(n)]
    def ret(v=0,extra=0):
        sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EIP,rd(sp));u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EAX,v)
    for a,v in ((0x53a45,actors),(0x53ad5,state),(0x53a55,meta),(0x53beb,27),(0x53bef,5)):
        put(a,v)
    for i in range(27):
        raw=bytearray(80);raw[6]=2 if i<20 else 0;raw[64:68]=struct.pack('<HH',100,100)
        u.mem_write(actors+i*80,bytes(raw))
    for actor in (21,22):u.mem_write(actors+actor*80+5,b'\x01')
    events=[]
    def code(_,pc,n,data):
        if pc==0x3702f:ret(u.reg_read(UC_X86_REG_EAX),4)
        elif pc in (0x135dd,0x12cea):ret()
        elif pc==0x15f84:events.append(['story',args(9)[1]]);ret()
        elif pc==0x10b4e:
            group=args(1)[0]
            if group!=2:raise ValueError('coalesced original group differs')
            put(0x53beb,29);events.append(['append_group_boundary',group,27,29]);ret()
        elif pc==0x22253:events.append(['teleport',*args(5)]);ret()
        elif pc==0x1a30b:raise ValueError('second action was forced to change phase')
    u.hook_add(UC_HOOK_CODE,code)
    def run(pc):
        put(stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(pc,stop,count=100000)
        if u.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError('caller did not return')
    prefixes=[]
    for _ in range(2):
        run(0x362c5);run(0x13565);run(0x20bf5)
        prefixes.append({'state16':u.mem_read(state+16,1)[0],'turn_slot0':u.mem_read(meta+3,1)[0],
                         'turn':rd(0x53bef),'result':rd(0x53ecc),'active_count':rd(0x53beb)})
    run(0x362e8)
    expected=[['story',4],['teleport',22,22,18,22,18],['append_group_boundary',2,27,29],
              ['teleport',29,21,18,21,18],['teleport',30,23,18,23,18]]
    if events!=expected or [x['state16'] for x in prefixes]!=[1,2] or any(x['result'] for x in prefixes):
        raise ValueError('coalesced original prefix changed')
    return {'prefixes':prefixes,'callback82_events':events,'same_turn_not_forced_to_advance':True,
            'no_result_from_other_boss_deaths':True,'group_constructor_substituted':True,
            'damage_and_legal_setup_prefix_substituted_not_natural_input':True,
            'domain':'raw29/30 are allocated inactive slots;22253 separately proves no target-record read'}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--original',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    if args.output.exists() or args.output.resolve().is_relative_to(args.original.resolve()):raise ValueError('unsafe/existing F1 output')
    image=args.image.read_bytes();source=json.loads(args.source.read_text(encoding='utf8'));verify_source(source,image)
    other=(args.original/'FDOTHER.DAT').read_bytes()
    if sha(other)!='882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667':raise ValueError('FDOTHER identity mismatch')
    entries=archive_entries(other)
    result={'schema':'fd2-f1-callback82-order-v1','image_sha256':IMAGE_SHA,
            'caller':coalesced_caller(image),'inactive_teleports':[teleport_access(image,entries,a,21 if a==29 else 23,18,21 if a==29 else 23,18) for a in (29,30)],
            'original_natural_input_executed':False,'full_semantic_gate':False}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x',encoding='utf8') as stream:stream.write(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'output':str(args.output),'sha256':sha(args.output.read_bytes()),'inactive_record_reads':0}))


if __name__=='__main__':main()
