#!/usr/bin/env python3
"""G4有限事件原机器码与真实C probe差分，不启动GUI/不修改原数据。

完整35e0e/3623c/4df4c及13512/4ebe3执行；20926仅把既有205be基础胜败
检查替换为无写返回，保留该函数的真实signed Jcc和34894隐藏谓词。
Watcom stack helper为RET4替身。C观测是完整边界/真实RNG callback前，
原CPU的逐写另列，不宣称C逐写、全callback、像素或原生运行等价。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
EXE_SHA='bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
FUNCTIONS={
    0x35e0e:(0x35e5b,'05769f822c4dc4b014b73148fa806705006f4b2e4d04cb22de0bed34cec719fd'),
    0x3623c:(0x362b0,'973736e1cd16c6b6a6e22cf5f58afcb6bb829335af8aa06b0116aa35604daad8'),
    0x4df4c:(0x4df84,'5c8abec62abd33158b2e19102543ae6ab3ea93f96a730d4dba85347729ee675f'),
    0x13512:(0x13536,'926169aef8164f52c12c9d59d5273133364f3f33444debf9c17987172ddc7952'),
    0x4ebe3:(0x4ebff,'369b02f58875c3e09fa6c77dab67e316bd6ec5391c52234fd8ee3baecf323c2b'),
    0x20926:(0x20957,'3b4736a96cf6b32410f50b75dfcb5a7fa69170b2401e11dc102f4bd9eb825263'),
    0x34894:(0x348bb,'b97def403c5c100687a4cf647cec7134074ca81a240790c72f77d0ba37316678'),
}
SEEDS=(0,1,0x7a18,0xffff)
TURNS=(0,6,7,0x7fffffff,0x80000000,0xffffffff)
EXPECTED_IDS={f'relative-{s}-{g}-{e}' for s in (0,1) for g in (0,1) for e in (0,1)}|{
    f'pair-{count}-{seed}' for count in (3,96) for seed in SEEDS}|{
    f'turn-{turn}-{hidden}' for turn in TURNS for hidden in (0,1)}|{'reset-0','reset-1'}

def digest(raw):return hashlib.sha256(raw).hexdigest()

def load_cases(raw):
    cases=[json.loads(line) for line in raw.decode('utf-8-sig').splitlines() if line]
    if len(cases)!=30 or {c.get('case_id') for c in cases}!=EXPECTED_IDS:
        raise ValueError('C probe必需代表缺失/重复')
    for c in cases:
        kind=c.get('kind');name=c['case_id']
        if kind not in {'relative','pair','reset','turn'} or type(c.get('count')) is not int or not 0<c['count']<=96:
            raise ValueError('C probe未知kind/count')
        for field,n in [('units',7680),('state',32),('turn_slots',48)]:
            for boundary in ('input','finish'):
                if len(bytes.fromhex(c[f'{boundary}_{field}_hex']))!=n:
                    raise ValueError('C完整owner长度不同')
        if type(c.get('turn')) is not int or not 0<=c['turn']<=0xffffffff or type(c.get('seed')) is not int or not 0<=c['seed']<=65535:
            raise ValueError('C probe DWORD/seed越界')
        if type(c.get('actor')) is not int or not 0<=c['actor']<c['count']:
            raise ValueError('C probe actor越界')
        for boundary in ('input','finish'):
            if len(bytes.fromhex(c[f'{boundary}_cells_hex']))!=(24 if kind=='reset' else 0):
                raise ValueError('C map完整大小不同')
        if c['width']!=(3 if kind=='reset' else 0) or c['height']!=(2 if kind=='reset' else 0):
            raise ValueError('C map代表不同')
        expected_calls=1 if kind=='pair' else 0
        if c['rng_calls']!=expected_calls:
            raise ValueError('C共享RNG调用计数不同')
        for field,n in [('units',7680),('state',32),('slots',48)]:
            if len(bytes.fromhex(c[f'rng_input_{field}_hex']))!=(n if expected_calls else 0):
                raise ValueError('C真实RNG边界长度不同')
        if name.startswith('relative-'):
            _,side,gate,enabled=map(str,name.split('-'));u=bytes.fromhex(c['input_units_hex']);s=bytes.fromhex(c['input_state_hex'])
            if kind!='relative' or c['actor']!=1 or c['count']!=96 or c['turn']!=255 or u[86]!=(2 if int(side) else 0) or (s[17],s[18])!=(int(gate),int(enabled)):
                raise ValueError('相对回合触发等价类不同')
        elif kind=='pair':
            _,count,seed=name.split('-');s=bytes.fromhex(c['input_state_hex'])
            if c['count']!=int(count) or c['seed']!=int(seed) or s[21]!=c['count']-3 or c['turn']!=255:
                raise ValueError('单RNG配对代表不同')
        elif kind=='turn':
            _,turn,hidden=name.split('-');u=bytes.fromhex(c['input_units_hex'])
            if c['count']!=96 or c['actor']!=64 or c['turn']!=int(turn) or u[64*80+5]!=(0xa0|int(hidden)):
                raise ValueError('signed turn代表不同')
    return cases

def validate_sources(image,exe,readsets):
    if digest(image)!=IMAGE_SHA or digest(exe)!=EXE_SHA:
        raise ValueError('原strict relocated镜像/EXE身份不同')
    functions=[f for r in readsets for f in r.get('functions',[])]
    for va,(end,sha) in FUNCTIONS.items():
        matches=[f for f in functions if f.get('va')==va]
        if not matches:raise ValueError('缺完整Domain函数')
        f=matches[0];chunks=f.get('chunks',[])
        if len(chunks)!=1 or (chunks[0]['start'],chunks[0]['end'],chunks[0]['sha256'])!=(va,end,sha) or not f.get('instructions') or not f.get('blocks'):
            raise ValueError('Domain函数边界/读集不同')
        if digest(image[va:end])!=sha:
            raise ValueError('strict relocated字节与Domain不同')

class Original:
    def __init__(self,image,c):
        import unicorn
        from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
        self.EAX,self.ESP,self.EIP=UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
        self.cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32);self.c=c
        self.cpu.mem_map(0x10000,0x54000);self.cpu.mem_write(0x10000,image[0x10000:0x64000])
        for at in (0x100000,0x200000):self.cpu.mem_map(at,0x10000)
        self.units=0x100000;self.metadata=0x102000;self.state=0x103000;self.map=0x104000
        self.stack=0x208000;self.stop=0x400000;self.writes=[];self.rng_trace=[];self.hooks=[]
        self.cpu.mem_write(self.units,bytes.fromhex(c['input_units_hex']))
        self.cpu.mem_write(self.metadata,b'\0\0\0'+bytes.fromhex(c['input_turn_slots_hex']))
        self.cpu.mem_write(self.state,bytes.fromhex(c['input_state_hex']))
        if c['kind']=='reset':self.cpu.mem_write(self.map,struct.pack('<HH',3,2)+bytes.fromhex(c['input_cells_hex']))
        for at,value in [(0x53a45,self.units),(0x53a55,self.metadata),(0x53ad5,self.state),(0x53bef,c['turn']),(0x53ecc,0)]:
            self.cpu.mem_write(at,struct.pack('<I',value))
        self.cpu.mem_write(0x627b8,struct.pack('<H',c['seed']))
        self.cpu.hook_add(unicorn.UC_HOOK_CODE,self.code)
        self.cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,self.write)
    def u32(self,at):return struct.unpack('<I',self.cpu.mem_read(at,4))[0]
    def ret(self,value=None,extra=0):
        sp=self.cpu.reg_read(self.ESP);target=self.u32(sp)
        if value is not None:self.cpu.reg_write(self.EAX,value)
        self.cpu.reg_write(self.ESP,sp+4+extra);self.cpu.reg_write(self.EIP,target)
    def code(self,cpu,address,size,data):
        if address==0x3702f:self.hooks.append('Watcom RET4');self.ret(extra=4);return
        if address==0x205be and self.c['kind']=='turn':
            self.hooks.append('205be base result check excluded/no writes');self.ret(0);return
        if address==0x4ebe3:
            self.rng_trace.append({'units_hex':bytes(cpu.mem_read(self.units,7680)).hex(),
                'state_hex':bytes(cpu.mem_read(self.state,32)).hex(),'slots_hex':bytes(cpu.mem_read(self.metadata+3,48)).hex()})
        if not any(start<=address<end for start,(end,_) in FUNCTIONS.items()):
            raise RuntimeError(f'原码执行逃出完整Domain范围 {address:#x}')
    def write(self,cpu,access,address,size,value,data):
        if 0x200000<=address and address+size<=0x210000:return
        owner=None;offset=0
        for start,length,label in [(self.units,7680,'units'),(self.state,32,'state'),(self.metadata+3,48,'turn_slots'),(self.map+4,24,'map_cells'),(0x627b8,2,'RNG'),(0x53ecc,4,'result')]:
            if start<=address and address+size<=start+length:owner=label;offset=address-start;break
        if owner is None:raise RuntimeError(f'原码写出声明owner {address:#x}/{size}')
        self.writes.append({'owner':owner,'offset':offset,'size':size,'value':value,'pc':hex(cpu.reg_read(self.EIP))})
    def run(self):
        kind=self.c['kind'];entry={'relative':0x35e0e,'pair':0x3623c,'turn':0x20926,'reset':0x4df4c}[kind]
        args=[self.map] if kind=='reset' else [self.c['actor']] if kind=='relative' else []
        self.cpu.mem_write(self.stack,struct.pack('<'+'I'*(len(args)+1),self.stop,*args));self.cpu.reg_write(self.ESP,self.stack)
        self.cpu.emu_start(entry,self.stop,count=50000)
        if self.cpu.reg_read(self.EIP)!=self.stop or self.cpu.reg_read(self.ESP)!=self.stack+4:
            raise RuntimeError('原完整函数未按原cdecl返回')
        actual={'finish_units_hex':bytes(self.cpu.mem_read(self.units,7680)).hex(),
            'finish_state_hex':bytes(self.cpu.mem_read(self.state,32)).hex(),
            'finish_turn_slots_hex':bytes(self.cpu.mem_read(self.metadata+3,48)).hex(),
            'finish_cells_hex':bytes(self.cpu.mem_read(self.map+4,24)).hex() if kind=='reset' else '',
            'rng_after':struct.unpack('<H',self.cpu.mem_read(0x627b8,2))[0],'rng_calls':len(self.rng_trace)}
        if kind=='turn':actual['result']=self.u32(0x53ecc)
        for key,value in actual.items():
            if self.c[key]!=value:raise ValueError(f'{self.c["case_id"]}: C/原码边界不同 {key}')
        if self.rng_trace:
            if len(self.rng_trace)!=1:raise ValueError('原单RNG次数不同')
            for key,value in self.rng_trace[0].items():
                if self.c['rng_input_'+key]!=value:raise ValueError('真实RNG前的完整owner不同')
        return {'case_id':self.c['case_id'],'matched':list(actual),'actual_original_CPU_writes':self.writes,
            'RNG_boundary_matched':bool(self.rng_trace),'explicit_hooks':self.hooks}

def compare(image,exe,readsets,c_bytes):
    validate_sources(image,exe,readsets);cases=load_cases(c_bytes)
    results=[Original(image,c).run() for c in cases]
    import unicorn
    return {'ok':True,'schema':'fd2-g4-common-event-machine-C-v1','machine_code_C_cases':len(results),
        'cases':results,'executable_sha256':EXE_SHA,'strict_relocated_image_sha256':IMAGE_SHA,
        'C_results_sha256':digest(c_bytes),'unicorn_version':unicorn.__version__,
        'function_chunks':{hex(k):{'end':hex(v[0]),'relocated_sha256':v[1]} for k,v in FUNCTIONS.items()},
        'C_observation':'all96x80B/state32/metadata48/mapcells at boundary; real shared RNG callback before sample; not a C individual-memory-write tracer',
        'original_RNG_executed':True,'native_DOSBox_acceptance':False,'pixel_audio_equivalence':False,
        'explicit_substitutions':['Watcom stack helper RET4','20926 only: base205be omitted with no writes; not complete stage result equivalence'],
        'source_scope':'complete35e0e/3623c/4df4c and helper13512/4ebe3;20926 real signed branch+34894 with base helper excluded',
        'fixture_scope':'finite in-memory records and DWORD boundary classes; not natural-save/native acceptance'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image',type=Path,default=ROOT/'tools/fd2_le_relocated_relbase.bin')
    p.add_argument('--executable',type=Path,default=ROOT/'original_game/FD2.EXE')
    p.add_argument('--root-readset',type=Path,default=ROOT/'build/g1/ida/preparation/scope-root-readset.json')
    p.add_argument('--callee-readset',type=Path,default=ROOT/'build/g1/ida/preparation/scope-callee-closure.json')
    p.add_argument('--c-probe',type=Path,required=True);p.add_argument('--c-results',type=Path,required=True);p.add_argument('--json',type=Path,required=True)
    a=p.parse_args();paths=[a.image,a.executable,a.root_readset,a.callee_readset,a.c_probe,a.c_results]
    snapshots=[x.read_bytes() for x in paths]
    replay=subprocess.run([str(a.c_probe.resolve())],capture_output=True,check=True,timeout=30)
    if replay.stdout!=snapshots[5]:raise ValueError('C JSONL不是当前声明probe的实际输出')
    result=compare(snapshots[0],snapshots[1],[json.loads(x.decode('utf-8')) for x in snapshots[2:4]],snapshots[5])
    if any(x.read_bytes()!=old for x,old in zip(paths,snapshots)):raise ValueError('执行期间原来源/C结果改变')
    result['source_files_unchanged']=True;result['C_JSONL_reexecuted']=True;result['C_probe_sha256']=digest(snapshots[4])
    result['Domain_readset_sha256']=[digest(x) for x in snapshots[2:4]]
    if a.json.exists() or a.json.resolve().is_relative_to(a.executable.resolve().parent):raise ValueError('结果需新文件且在原资源目录外')
    a.json.parent.mkdir(parents=True,exist_ok=True);a.json.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='cases'},ensure_ascii=False))

if __name__=='__main__':main()
