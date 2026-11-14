"""S5 31529的20组实际FIGANI→完整2ebe1控制流：显式未初始化栈边界。

原机器码是oracle；RNG返回值/未写局部栈以独立输入标明，不猜自然栈。
绘制、PCM、BIOS时间为记录替身；不声明真实像素、声音或整场运行通过。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from dat_extract import parse_archive

IMAGE_SHA = '8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
ENTRY, STACK, STOP, ATTACK, IDLE, UNITS = 0x2ebe1, 0x508000, 0x700000, 0x100000, 0x200000, 0x400000


def validate(image, report):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA or report.get('image_sha256') != IMAGE_SHA:
        raise ValueError('strict source identity differs')
    row = next((r for r in report.get('functions', []) if r['va'] == ENTRY), None)
    if not row or row['end'] != 0x2f4d4 or not row.get('blocks') or not row.get('instructions'):
        raise ValueError('complete 2ebe1 owner/CFG missing')
    covered=set();decoded=set()
    for chunk in row['chunks']:
        raw = image[chunk['start']:chunk['end']]
        if raw.hex() != chunk['raw_hex'] or hashlib.sha256(raw).hexdigest() != chunk['sha256']:
            raise ValueError('source chunk differs')
        covered.update(range(chunk['start'],chunk['end']))
    for ins in row['instructions']:
        if image[ins['ea']:ins['ea']+ins['size']].hex() != ins['raw_hex']:
            raise ValueError('source instruction differs')
        decoded.update(range(ins['ea'],ins['ea']+ins['size']))
    if covered!=set(range(ENTRY,0x2f4d4)) or decoded!=covered:
        raise ValueError('complete 2ebe1 chunks/instructions missing')
    return row


def execute(image, attack, idle, *, override, side, rng, stack_word):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0, 0x64000); cpu.mem_write(0, image[:0x64000])
    cpu.mem_map(ATTACK, 0x100000); cpu.mem_write(ATTACK, attack)
    cpu.mem_map(IDLE, 0x100000); cpu.mem_write(IDLE, idle)
    cpu.mem_map(UNITS, 0x1000)
    cpu.mem_map(0x500000, 0x10000)
    cpu.mem_write(0x500000, struct.pack('<I', stack_word & 0xffffffff)*0x4000)
    cpu.mem_write(0x53a45, struct.pack('<I', UNITS))
    cpu.mem_write(0x54133, struct.pack('<I', override))
    cpu.mem_write(UNITS+6, bytes([side]));cpu.mem_write(UNITS+80+64, struct.pack('<H', 777))
    cpu.mem_write(STACK, struct.pack('<9I', STOP, 0, 1, ATTACK, IDLE, 0x300000, 0x320000, 0x340000, 0))
    cpu.reg_write(UC_X86_REG_ESP, STACK)
    stats = {'rng_reads': 0, 'tick_calls': 0, 'attack_draws': [], 'hp_writes': [], 'local_reads': []}
    # 完整prologue：四个saved DWORD与local88h；var4C=entryESP-4Ch。
    local = STACK-0x4c
    initialized = False
    def get(addr): return struct.unpack('<I', cpu.mem_read(addr, 4))[0]
    def ret(value=0, clean=0):
        sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EAX,value & 0xffffffff)
        cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+clean)
    def hook(machine, address, size, userdata):
        if address == 0x3702f: ret(clean=4); return
        if address == 0x4ebe3:
            stats['rng_reads'] += 1; ret(rng); return
        if address == 0x2eb9f:
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if get(sp+4)==ATTACK:stats['attack_draws'].append(get(sp+8))
            ret();return
        if address == 0x17aa9:stats['tick_calls']+=1;ret();return
        if address in (0x11eb0,0x25a96,0x3790a,0x37ae5):ret();return
        if address in (0x2f7b6,0x2facd,0x2f4d4,0x2f631,0x37910):
            raise ValueError(f'override domain reached forbidden business/split {address:#x}')
        if not ENTRY <= address < 0x2f4d4:raise ValueError(f'undeclared original PC {address:#x}')
    def memory(machine, access, address, size, value, userdata):
        nonlocal initialized
        if address == local:
            if access == unicorn.UC_MEM_WRITE:initialized=True
            else:stats['local_reads'].append({'value':get(local),'initialized_by_function':initialized})
        if access == unicorn.UC_MEM_WRITE and address == UNITS+80+64:
            stats['hp_writes'].append(value)
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
    cpu.emu_start(ENTRY,STOP,count=1000000)
    if cpu.reg_read(UC_X86_REG_EIP)!=STOP:raise ValueError('original did not return')
    stats['result']=cpu.reg_read(UC_X86_REG_EAX)
    return stats


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image',required=True,type=Path);p.add_argument('--readset',required=True,type=Path)
    p.add_argument('--figani',default='original_game/FIGANI.DAT',type=Path)
    p.add_argument('--output',required=True,type=Path);a=p.parse_args(argv)
    from verify_s5_cinematic import validate_output
    validate_output(a.output,(a.image,a.readset,a.figani))
    image=a.image.read_bytes();raw=a.readset.read_bytes();row=validate(image,json.loads(raw))
    figraw=a.figani.read_bytes();_,clips=parse_archive(figraw);cases=[]
    for j in range(20):
        u,v,scene=(image[va+j] for va in (0x526e9,0x526fd,0x52711))
        for direction,(attacker,target,override) in enumerate(((u,v,scene),(v,u,1))):
            attack,idle=clips[3*attacker+1],clips[3*target]
            if attack[1] or not attack[0] or len(attack)>0x100000 or len(idle)>0x100000:
                raise ValueError('actual montage outside measured nonsplit resources')
            markers=[i for i in range(attack[0]) if attack[struct.unpack_from('<I',attack,8+4*i)[0]+4]]
            if not markers:raise ValueError('actual montage missing marker')
            for rng in (0,50):
                for word in (0,123,-1):
                    trace=execute(image,attack,idle,override=override,side=int(attacker<76)*2,rng=rng,stack_word=word)
                    cases.append({'pair':j,'direction':direction,'attacker':attacker,'target':target,
                        'scene_override':override,'rng':rng,'uninitialized_stack_word':word,
                        'clip_parts':attack[0],'marker_indices':markers,**trace})
    if image!=a.image.read_bytes() or raw!=a.readset.read_bytes() or figraw!=a.figani.read_bytes():
        raise ValueError('source changed while executing')
    report={'schema':'fd2-s5-montage-original-control-v1','source_sha256':hashlib.sha256(raw).hexdigest(),
        'image_sha256':IMAGE_SHA,'figani_sha256':hashlib.sha256(figraw).hexdigest(),'case_count':len(cases),
        'substitutes':['stack probe','scripted field RNG','render/copy/PCM/BIOS captured no-op'],
        'limits':['uninitialized stack is explicit independent input, not natural gameplay prediction',
                  'no actual C consumer or pixel/GUI claim; identifies source undefined HP/repeat boundary'],
        'cases':cases}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'output':str(a.output),'cases':len(cases),'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest()}))

if __name__=='__main__':main()
