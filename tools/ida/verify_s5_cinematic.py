"""S5实际FDOTHER34/54/58的完整2eb9f→4e98d原码与真实C逐像素差分。

只替身Watcom栈探测；RLE/目录/定位均执行原指令。无GUI/时间/终章整体声明。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
from audit_stage_data import archive_entries, unique_keys

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
SHEET,PAGE,STACK,RET=0x100000,0x300000,0x508000,0x700000

def digest(raw):return hashlib.sha256(raw).hexdigest()

def validate_source(image,report):
    if digest(image)!=IMAGE_SHA or report.get('image_sha256')!=IMAGE_SHA:
        raise ValueError('strict source identity differs')
    functions={r['va']:r for r in report.get('functions',[])}
    if set(functions)!={0x2eb9f,0x4e98d}:raise ValueError('two complete decoder owners required')
    for va,row in functions.items():
        if not row.get('instructions') or not row.get('blocks'):raise ValueError('source CFG missing')
        end={0x2eb9f:0x2ebe1,0x4e98d:0x4eb48}[va]
        covered=set();decoded=set()
        for chunk in row['chunks']:
            raw=image[chunk['start']:chunk['end']]
            if raw.hex()!=chunk['raw_hex'] or digest(raw)!=chunk['sha256']:
                raise ValueError('decoder raw chunk differs')
            covered.update(range(chunk['start'],chunk['end']))
        for ins in row['instructions']:
            if image[ins['ea']:ins['ea']+ins['size']].hex()!=ins['raw_hex']:
                raise ValueError('decoder instruction differs')
            decoded.update(range(ins['ea'],ins['ea']+ins['size']))
        if covered!=set(range(va,end)) or decoded!=covered:
            raise ValueError('complete decoder instruction/chunk extent missing')
    return functions

def validate_output(output,inputs=()):
    path=output.resolve();original=(Path(__file__).resolve().parents[2]/'original_game').resolve()
    if path.exists() or path==original or original in path.parents or any(path==p.resolve() for p in inputs if p):
        raise ValueError('output must be a new non-original evidence file')

def vectors(entries):
    for entry,count in ((34,101),(54,111),(58,20)):
        data=entries[entry][1]
        if data[0]!=count:raise ValueError('source sheet count differs')
        for frame in range(count):yield entry,frame,320,0,0,83
        for frame in (0,count//2,count-1):
            for x in (0,160):yield entry,frame,640,x,0,17

def execute(image,data,frame,stride,x,y,seed):
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x64000);u.mem_write(0,image[:0x64000])
    u.mem_map(SHEET,0x100000);u.mem_write(SHEET,data)
    u.mem_map(PAGE,0x20000);u.mem_write(PAGE,bytes([seed])*0x20000)
    u.mem_map(0x500000,0x10000)
    size=stride*200;visited=set()
    offset=struct.unpack_from('<I',data,8+frame*4)[0]
    px,py=struct.unpack_from('<HH',data,offset);w,h=struct.unpack_from('<HH',data,offset+9)
    if min(x+px,y+py)<0 or x+px+w>stride or y+py+h>200:
        raise ValueError('source pointer draw outside explicit full page owner')
    u.mem_write(STACK,struct.pack('<6I',RET,SHEET,frame,PAGE+x+y*stride,stride,0xffffffff))
    u.reg_write(UC_X86_REG_ESP,STACK)
    def code(cpu,pc,length,userdata):
        if pc==0x3702f:
            sp=u.reg_read(UC_X86_REG_ESP);pc2=struct.unpack('<I',u.mem_read(sp,4))[0]
            u.reg_write(UC_X86_REG_EIP,pc2);u.reg_write(UC_X86_REG_ESP,sp+8);return
        if not (0x2eb9f<=pc<0x2ebe1 or 0x4e98d<=pc<0x4eb48):
            raise ValueError(f'undeclared original decoder PC {pc:#x}')
        visited.add(pc)
    def write(cpu,access,address,length,value,userdata):
        if PAGE<=address and address+length<=PAGE+size:return
        if 0x500000<=address and address+length<=0x510000:return
        if 0x627b4<=address and address+length<=0x627b8:return
        raise ValueError(f'original decoder outside page/stack/local scratch {address:#x}')
    u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,write)
    u.emu_start(0x2eb9f,RET,count=3000000)
    if u.reg_read(UC_X86_REG_EIP)!=RET or bytes(u.mem_read(SHEET,len(data)))!=data:
        raise ValueError('original decoder return/source guard failed')
    return bytes(u.mem_read(PAGE,size)),visited

def validate_probe(rows,originals):
    if len(rows)!=len(originals):raise ValueError('C probe vector count differs')
    for index,(row,expected) in enumerate(zip(rows,originals)):
        if set(row)!={'status','pixels_hex'} or type(row['status']) is not int or row['status']!=0:
            raise ValueError(f'C probe status/schema differs at {index}')
        if not isinstance(row['pixels_hex'],str) or row['pixels_hex']!=expected.hex():
            raise ValueError(f'original/C full-page pixel differs at {index}')

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image',required=True,type=Path);p.add_argument('--readset',required=True,type=Path)
    p.add_argument('--fdother',type=Path,default=Path('original_game/FDOTHER.DAT'))
    p.add_argument('--probe',type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args(argv)
    validate_output(a.output,(a.image,a.readset,a.fdother,a.probe))
    image=a.image.read_bytes();raw=a.readset.read_bytes();fdother=a.fdother.read_bytes()
    functions=validate_source(image,json.loads(raw,object_pairs_hook=unique_keys))
    entries=archive_entries(fdother);cases=list(vectors(entries));originals=[];visited=set();results=[]
    for case in cases:
        entry,frame,stride,x,y,seed=case
        pixels,pcs=execute(image,entries[entry][1],frame,stride,x,y,seed)
        originals.append(pixels);visited.update(pcs)
        results.append({'entry':entry,'frame':frame,'stride':stride,'x':x,'y':y,'seed':seed,
            'source_pixels_sha256':digest(pixels),'bytes':len(pixels)})
    identity=None
    if a.probe:
        before=digest(a.probe.read_bytes())
        output=subprocess.run([str(a.probe.resolve()),'--cin-vectors'],
            input='\n'.join(' '.join(map(str,c)) for c in cases)+'\n',text=True,capture_output=True,check=True)
        after=digest(a.probe.read_bytes())
        if before!=after:raise ValueError('actual C probe changed')
        rows=[json.loads(line,object_pairs_hook=unique_keys) for line in output.stdout.splitlines()]
        validate_probe(rows,originals);identity={'before_sha256':before,'after_sha256':after}
    if image!=a.image.read_bytes() or raw!=a.readset.read_bytes() or fdother!=a.fdother.read_bytes():
        raise ValueError('sources changed while executing')
    report={'schema':'fd2-s5-cinematic-decoder-v1','image_sha256':IMAGE_SHA,
        'readset_sha256':digest(raw),'fdother_sha256':digest(fdother),'case_count':len(cases),
        'actual_c_compared':len(cases) if identity else 0,'c_probe_identity':identity,'cases':results,
        'coverage':[{'va':hex(va),'unreached_blocks':[hex(b['start']) for b in r['blocks']
            if b['start'] not in visited]} for va,r in functions.items()],
        'substitutes':['Watcom stack probe only'],
        'limits':['tint -1 / nonsplit actual sheet resource domain; other tint paths not new S5 proof',
            'owner-bounded positive pointers only; original out-of-page writes not imitated',
            'not timing/notice/credits/montage/GUI/gameplay evidence']}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'output':str(a.output),'cases':len(cases),'actual_c_compared':report['actual_c_compared'],
        'sha256':digest(a.output.read_bytes())}))

if __name__=='__main__':main()
