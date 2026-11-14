"""完整AFM dispatcher/十opcode执行实际ANI0/2：临时52766与base/DAC归属。

动画文件I/O与等待不执行；每个实际frame的全部字节码原指令执行，不替身
decoder/临时palette。此门不声称ANI播放时长或GUI，相应C桥由私有TU验证。
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
from audit_stage_data import archive_entries,unique_keys
from verify_s5_cinematic import IMAGE_SHA,validate_output

REQUIRED={0x20421,0x190ac,0x36fd3,0x36ff4,0x36e3d,0x36e57,0x36e65,
          0x36ea7,0x36ee0,0x36f08,0x36f24,0x36f69,0x36f82,0x36fac}
PROGRAM,TEMP,PAGE,BASE,STACK,RET=0x100000,0x200000,0x300000,0x400000,0x508000,0x700000

def sha(raw):return hashlib.sha256(raw).hexdigest()

def validate(image,report):
    if sha(image)!=IMAGE_SHA or report.get('image_sha256')!=IMAGE_SHA:
        raise ValueError('strict ANI source identity differs')
    by={r['va']:r for r in report.get('functions',[])}
    if set(by)!=REQUIRED:raise ValueError('complete caller/tail/AFM owner set missing')
    for row in by.values():
        covered=set();decoded=set()
        for c in row['chunks']:
            data=image[c['start']:c['end']]
            if data.hex()!=c['raw_hex'] or sha(data)!=c['sha256']:raise ValueError('AFM chunk differs')
            covered.update(range(c['start'],c['end']))
        for i in row['instructions']:
            if image[i['ea']:i['ea']+i['size']].hex()!=i['raw_hex']:raise ValueError('AFM instruction differs')
            decoded.update(range(i['ea'],i['ea']+i['size']))
        if not covered or covered!=decoded:raise ValueError('AFM instruction/chunk coverage incomplete')
    return by

def execute(image,entry,functions):
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x64000);u.mem_write(0,image[:0x64000])
    for address,size in ((PROGRAM,0x10000),(TEMP,0x1000),(PAGE,0x10000),(BASE,0x1000),(0x500000,0x10000)):
        u.mem_map(address,size)
    base=bytes([7])*768;u.mem_write(BASE,base);u.mem_write(0x53a65,struct.pack('<I',BASE))
    u.mem_write(TEMP,bytes([0xa5])*768);u.mem_write(PAGE,bytes([83])*64000)
    allowed={i['ea'] for va,row in functions.items() if va>=0x36e3d for i in row['instructions']}
    instructions={i['ea']:i['disasm'].split(';')[0] for row in functions.values() for i in row['instructions']}
    palette_writes=0;frame_writes=0;visited=set()
    def code(cpu,pc,size,userdata):
        if pc not in allowed:raise ValueError(f'undeclared AFM decoder PC {pc:#x}')
        if instructions[pc].strip().split()[0] in ('out','outp','ins','outs'):
            raise ValueError('source unexpectedly wrote hardware DAC')
        visited.add(pc)
    def write(cpu,access,address,size,value,userdata):
        nonlocal palette_writes,frame_writes
        if TEMP<=address and address+size<=TEMP+768:palette_writes+=size;return
        if PAGE<=address and address+size<=PAGE+64000:frame_writes+=size;return
        if 0x500000<=address and address+size<=0x510000:return
        if 0x52760<=address and address+size<=0x5276a:return
        raise ValueError(f'AFM wrote outside temporary palette/frame/stack {address:#x}')
    u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,write)
    def call(pc,args):
        u.mem_write(STACK,struct.pack('<'+'I'*(len(args)+1),RET,*args));u.reg_write(UC_X86_REG_ESP,STACK)
        u.emu_start(pc,RET,count=10000000)
        if u.reg_read(UC_X86_REG_EIP)!=RET:raise ValueError('AFM source did not return')
    call(0x36fd3,(64000,PAGE,TEMP))
    if entry[:28]!=b'AFM - Animation File Manager':raise ValueError('actual ANI format differs')
    count,width,height,screen=struct.unpack_from('<4H',entry,0xa5)
    if (width,height,screen)!=(320,200,64000):raise ValueError('actual ANI dimensions differ')
    cursor=0xad;frames=[]
    for index in range(count):
        length,commands=struct.unpack_from('<HH',entry,cursor);cursor+=8
        data=entry[cursor:cursor+length];cursor+=length
        if len(data)!=length:raise ValueError('actual ANI frame truncated')
        u.mem_write(PROGRAM,data);call(0x36ff4,(commands,PROGRAM))
        if bytes(u.mem_read(BASE,768))!=base or bytes(u.mem_read(0x53a65,4))!=struct.pack('<I',BASE):
            raise ValueError('AFM source changed original base palette')
        frames.append({'frame':index,'commands':commands,'payload_bytes':length,
            'pixels_sha256':sha(bytes(u.mem_read(PAGE,64000))),
            'temporary_palette_sha256':sha(bytes(u.mem_read(TEMP,768)))})
    if cursor!=len(entry):raise ValueError('ANI source full entry not consumed')
    return {'frames':frames,'frame_count':count,'temporary_palette_bytes_written':palette_writes,
        'framebuffer_bytes_written':frame_writes,'base_pointer_and_bytes_unchanged':True,
        'hardware_port_instructions_executed':0,'visited_PCs':len(visited)}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image',required=True,type=Path);p.add_argument('--readset',required=True,type=Path)
    p.add_argument('--ani',default='original_game/ANI.DAT',type=Path);p.add_argument('--output',required=True,type=Path)
    a=p.parse_args(argv);validate_output(a.output,(a.image,a.readset,a.ani))
    image=a.image.read_bytes();raw=a.readset.read_bytes();ani=a.ani.read_bytes()
    functions=validate(image,json.loads(raw,object_pairs_hook=unique_keys));entries=archive_entries(ani)
    results=[{'entry':index,**execute(image,entries[index][1],functions)} for index in (0,2)]
    if image!=a.image.read_bytes() or raw!=a.readset.read_bytes() or ani!=a.ani.read_bytes():
        raise ValueError('ANI source changed while executing')
    report={'schema':'fd2-s5-ani-palette-source-v1','image_sha256':IMAGE_SHA,'readset_sha256':sha(raw),
        'ani_sha256':sha(ani),'cases':results,'frames':sum(r['frame_count'] for r in results),
        'substitutes':[],'limits':['complete AFM dispatcher/opcodes only, caller file I/O and timing not executed',
            'no actual C/GUI assertion; S5 bridge restores borrowed base/DAC before present, legacy ANI unchanged']}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'output':str(a.output),'frames':report['frames'],'sha256':sha(a.output.read_bytes())}))

if __name__=='__main__':main()
