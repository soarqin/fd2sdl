#!/usr/bin/env python3
"""G3准备领域：strict LE原指令与真实C probe边界差分，无GUI／原字节patch。

完整2b777/2b843/2a07a/2ae0e、库存写原语、正值资金动画控制循环；
2a43e从真实入口到HP写后或失败返回下一轮。Watcom栈、libc及明确UI／
资源调用替身逐项登记；不将UI替身的零RNG称作原交互RNG一致。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
EXE_SHA='bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
FUNCTIONS={
 0x2b777:(0x2b843,'6c7828de55f3dc2ae7309a998adf9e06e294397587acfce41f656d11efa286cf'),
 0x2b843:(0x2b996,'ef4edb6a214b7cd4c472e7c63ac6ae4f20a0696b5e6332c507291e5be08daf12'),
 0x2a07a:(0x2a0c2,'19ae09ded8610021ead2a6ef9e41583c4f6a7fe93ddd808b4d8757f2555e52b4'),
 0x2ae0e:(0x2aedb,'f375e1afdb04ee21b5cb9e2574c78043a7f1ca51f0fc3eca7e9ddcc3d78eb228'),
 0x2aedb:(0x2af28,'fc35d89149d339d698ad005cf34578a5f94e5f20ef57e37d4d6b7dcec6e437a0'),
 0x34894:(0x348bb,'b97def403c5c100687a4cf647cec7134074ca81a240790c72f77d0ba37316678'),
 0x1b722:(0x1b750,'7e68144f947bc3df640dda29d0a5ff7785fa05c4b3870d1e7f6e70cccff6cf05'),
 0x1b8a6:(0x1b8e7,'09dc39ccc1420a3283f057cb926328dc44f31aa6a0d711adab0d18f33dee25f2'),
 0x1b8e7:(0x1b932,'f9bf468b3129d83e3d3ce4f16e89489d33cdad896129469072e0830c535d7d12'),
 0x1bb8c:(0x1bbdc,'171a1a87b46050e9b053e785fb71796b0d3b77a9495b38f5cf35a9ae7564479b'),
 0x1c142:(0x1c1c3,'1ea2a583f863ae6bc6fedf69f4ac489f19415abbf7b216fe76496d8a84f557fa'),
 0x26a7a:(0x26b91,'c50045ea229470ca08c86b1f1ff30ece2fec3aafffe04fbf2e0c59bac8bba68a'),
 0x26b91:(0x26c9b,'0e30371f6947b2d6083f6cefe1fa19cba58e0203f4f339d490b3c68e66b7999e'),
 0x2968d:(0x2986f,'943b4dafdb2bc94d0678ee2d37d8c019ef00c68f92adc9c5a89c5a72a47f7e8e'),
 0x2a43e:(0x2a694,'8bb0ec53ba565c36cf376957755003c8699c73dbca3b4e41fc798cdfc70e1887'),
 0x2872b:(0x28b41,'35437e227c3ca99d2ce46fa95f77a195087f2685f7bdad6227d00a844e4fb618'),
 0x28cbd:(0x28efe,'858a1b317e640bfda197c62982bcb697f4f3ab874368625d241fd4e84e43406c'),
 0x28f65:(0x2921a,'c51dba918672d4cbbe7ce55c9c20402c75e376fc7c58508931b0485cd62c293f')}
EXPECTED_IDS={f'reorder-{q}-{n}' for q in (16,20) for n in (q,q+1,32)}|{
 'front-unique','front-last-match','promotion-ordinary','promotion-special',
 'item-append-full','item-append-hole'}|{f'hidden-{n}' for n in (0,1,7,16,32)}|{
 f'revive-{i}' for i in range(6)}|{f'money-{op}-{i}' for op in ('add','sub') for i in range(4)}|{
 f'item-{op}-{i}' for op in ('remove','equip') for i in range(8)}
KINDS={'reorder','front','hidden','promotion','revive','money','remove','append','equip'}

def digest(data):return hashlib.sha256(data).hexdigest()
def signed(value):return value if value<0x80000000 else value-0x100000000

def validate_sources(image,readset,executable):
    if digest(image)!=IMAGE_SHA or digest(executable)!=EXE_SHA:raise ValueError('原EXE或strict relocated身份不同')
    if readset.get('schema')!='fd2-g3-preparation-domain-readset-v1' or set(readset.get('targets',[]))!=set(FUNCTIONS):raise ValueError('Domain函数集合不同')
    functions=readset.get('functions',[])
    if len(functions)!=len(FUNCTIONS) or {f.get('va') for f in functions}!=set(FUNCTIONS):raise ValueError('Domain函数缺失/重复')
    for f in functions:
        end,sha=FUNCTIONS[f['va']];chunks=f.get('chunks')
        if not f.get('instructions') or not f.get('blocks') or not chunks or len(chunks)!=1:raise ValueError('Domain函数读集不完整')
        c=chunks[0];raw=bytes.fromhex(c['raw_hex'])
        if (c['start'],c['end'],c['sha256'])!=(f['va'],end,sha) or raw!=image[f['va']:end] or digest(raw)!=sha:raise ValueError('Domain/strict LE完整chunk不同')

def load_cases(raw):
    cases=[json.loads(line) for line in raw.decode('utf-8-sig').splitlines() if line]
    if len(cases)!=len(EXPECTED_IDS) or {c.get('case_id') for c in cases}!=EXPECTED_IDS:raise ValueError('C probe代表覆盖缺失/重复')
    for c in cases:
        if c.get('kind') not in KINDS or type(c.get('count')) is not int or not 0<=c['count']<=32:raise ValueError('未知C probe kind/count')
        for key in ('input_roster_hex','finish_roster_hex'):
            if len(bytes.fromhex(c[key]))!=0xa00:raise ValueError('C probe完整a00长度不同')
        if type(c.get('money')) is not int or not 0<=c['money']<=0xffffffff or type(c.get('finish_money')) is not int or not 0<=c['finish_money']<=0xffffffff:raise ValueError('C probe DWORD资金越界')
        if len(c.get('args',[]))!=2 or any(type(a) is not int for a in c['args']):raise ValueError('C probe参数不同')
        name=c['case_id'];args=c['args'];kind=c['kind'];count=c['count']
        if name.startswith('reorder-'):
            _,quota,total=name.split('-');selection=bytes.fromhex(c['selection_hex'])
            if kind!='reorder' or args!=[int(quota),0] or count!=int(total) or len(selection)!=31 or any(b>1 for b in selection) or sum(selection[:count-1])!=int(quota)-1:raise ValueError('选阵代表名额/数量/位图不同')
        elif name.startswith('front-'):
            raw=bytes.fromhex(c['input_roster_hex'])
            matches=[i for i in range(1,count) if raw[i*80+8]==30]
            if kind!='front' or count!=32 or args!=[30,0] or matches!=([4,30] if name=='front-last-match' else [30]):raise ValueError('前置唯一/重复text代表域不同')
        elif name.startswith('hidden-'):
            if kind!='hidden' or count!=int(name.split('-')[1]) or args!=[0,0]:raise ValueError('hidden容量代表不同')
        elif name.startswith('promotion-'):
            if kind!='promotion' or count!=32 or args!=[0,0]:raise ValueError('转职候选代表不同')
        elif name.startswith('revive-'):
            mode=int(name.split('-')[1]);money=(10000,9600,9599,0x80000000,10000,10000)[mode]
            if kind!='revive' or count!=2 or args!=[mode,1] or c['money']!=money:raise ValueError('复活取消/资金边界代表不同')
        elif name.startswith('money-'):
            _,op,index=name.split('-')
            if kind!='money' or count!=1 or args!=[int(op=='sub'),1] or c['money']!=(1000,0,0xffffffff,0x80000000)[int(index)]:raise ValueError('DWORD资金代表不同')
        elif name.startswith(('item-remove-','item-equip-')):
            _,op,slot=name.split('-')
            if kind!=op or count!=2 or args!=[1,int(slot)]:raise ValueError('库存raw槽代表不同')
        elif name.startswith('item-append-'):
            raw=bytes.fromhex(c['input_roster_hex']);full=all(not raw[80+10+2*i]&128 for i in range(8))
            if kind!='append' or count!=2 or args!=[1,214] or full!=(name=='item-append-full'):raise ValueError('库存容量代表不同')
    return cases

class Original:
    def __init__(self,image,case):
        import unicorn
        from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
        self.unicorn=unicorn;self.EAX=UC_X86_REG_EAX;self.ESP=UC_X86_REG_ESP;self.EIP=UC_X86_REG_EIP
        self.cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32);self.image=image;self.case=case
        self.cpu.mem_map(0x10000,0x54000);self.cpu.mem_write(0x10000,image[0x10000:0x64000])
        for at in (0x100000,0x200000,0x300000):self.cpu.mem_map(at,0x10000)
        self.units=0x100000;self.selection=0x104000;self.candidates=0x105000;self.targets=0x105040;self.stack=0x208000;self.stop=0x400000;self.heap=0x300000
        self.cpu.mem_write(self.units,bytes.fromhex(case['input_roster_hex']))
        self.set32(0x53a45,self.units);self.set32(0x53bf7,self.units);self.set32(0x53bfb,case['count']);self.set32(0x53bf3,case['money'])
        self.cpu.mem_write(0x627b8,struct.pack('<H',0x7a18))
        self.calls={};self.writes=[];self.commits=[];self.cache=[];self.text=[];self.loop_count=0;self.boundary=None
        self.ui_stubs={0x1956b:'notice',0x15f84:'text',0x16559:'portrait',0x16c57:'wait',0x26996:'page_close',0x2a29d:'revive_picker',0x19953:'confirm',0x197e5:'confirm_close',0x4ebff:'image_blit',0x187d6:'number',0x25977:'music',0x28b41:'service_animation'}
        self.cpu.hook_add(unicorn.UC_HOOK_CODE,self.hook)
        self.cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,self.write_hook)
    def u32(self,at):return struct.unpack('<I',self.cpu.mem_read(at,4))[0]
    def set32(self,at,value):self.cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def ret(self,value=None,extra=0):
        sp=self.cpu.reg_read(self.ESP);target=self.u32(sp)
        if value is not None:self.cpu.reg_write(self.EAX,value&0xffffffff)
        self.cpu.reg_write(self.ESP,sp+4+extra);self.cpu.reg_write(self.EIP,target)
    def state(self):return bytes(self.cpu.mem_read(self.units,0xa00)),self.u32(0x53bf3)
    def record_commit(self,label,address,size,value):
        actor=self.case['args'][1];raw=bytearray(self.cpu.mem_read(self.units+actor*80,80));money=self.u32(0x53bf3)
        if label=='money':money=value&0xffffffff
        else:
            at=address-(self.units+actor*80);raw[at:at+size]=value.to_bytes(size,'little')
        self.commits.append({'label':label,'money':money,'actor_hex':raw.hex()})
    def write_hook(self,machine,access,address,size,value,data):
        if 0x200000<=address and address+size<=0x210000:return
        if self.units<=address and address+size<=self.units+0xa00:
            self.writes.append({'kind':'record','offset':address-self.units,'size':size,'value':value,'pc':machine.reg_read(self.EIP)})
            if self.case['kind']=='revive':
                base=self.units+self.case['args'][1]*80
                if address==base+5 and size==1:self.record_commit('flags',address,size,value)
                elif address==base+64 and size==2:self.record_commit('hp',address,size,value)
                else:raise RuntimeError('复活写出flags/HP声明域')
            return
        if self.candidates<=address and address+size<=self.targets+32:return
        if address==0x53bf3 and size==4:
            self.writes.append({'kind':'money','size':4,'value':value,'pc':machine.reg_read(self.EIP)})
            if self.case['kind']=='revive':self.record_commit('money',address,size,value)
            return
        if address in (0x53bdf,0x53ad9,0x53ae1) and size==4:return
        raise RuntimeError(f'原码写出声明域：{address:#x}/{size} PC={machine.reg_read(self.EIP):#x}')
    def hook(self,machine,address,size,data):
        c=self.case;sp=machine.reg_read(self.ESP)
        if c['kind']=='revive':
            if address==0x2a44d:
                self.loop_count+=1
                if self.loop_count==2:self.boundary='failed/no/cancel next-iteration entry';machine.emu_stop();return
            if address==0x2a623:self.boundary='post-money/flags/HP, before UI redraw';machine.emu_stop();return
        if c['kind']=='money' and self.partial_money and address==(0x26bd2 if c['args'][0] else 0x26abb):
            self.boundary='DWORD commit before potentially undefined signed digit slices';machine.emu_stop();return
        stubs={0x3702f:'Watcom stack probe RET4',0x3706e:'malloc',0x3776e:'free',0x3771c:'memmove',0x37324:'FDICON fopen',0x3759c:'FDICON fclose',0x11019:'FDICON cache append',0x37b29:'sprintf precision8',0x3790a:'delay',0x26c9b:'money digit draw'}
        if address in stubs or (c['kind']=='revive' and address in self.ui_stubs):
            name=stubs.get(address,self.ui_stubs.get(address));self.calls[name]=self.calls.get(name,0)+1
            if address==0x3702f:self.ret(extra=4);return
            if address==0x3706e:
                n=self.u32(sp+4)
                if n!=0xa00 or self.heap+n>0x310000:raise RuntimeError('非声明malloc范围')
                allocation=self.heap;self.heap+=(n+15)&~15;self.ret(allocation);return
            if address==0x3771c:
                dst,src,n=(self.u32(sp+4*i) for i in (1,2,3))
                def permitted(at,length):return (self.units<=at and at+length<=self.units+0xa00) or (0x300000<=at and at+length<=self.heap)
                if n>0xa00 or not permitted(dst,n) or not permitted(src,n):raise RuntimeError('memmove替身出声明域')
                if n:
                    raw=bytes(machine.mem_read(src,n));machine.mem_write(dst,raw)
                    if self.units<=dst<self.units+0xa00:self.writes.append({'kind':'libc-copy','offset':dst-self.units,'size':n,'sha256':digest(raw),'caller':self.u32(sp)-5})
                self.ret(dst);return
            if address==0x11019:self.cache.append(self.u32(sp+4));self.ret(0);return
            if address==0x37b29:
                dst,fmt,value=(self.u32(sp+4*i) for i in (1,2,3))
                if bytes(machine.mem_read(fmt,6))!=b'%0.8d\0' or not 0x200000<=dst<0x210000-12:raise RuntimeError('sprintf替身格式/目的域不同')
                value=signed(value);raw=(('-' if value<0 else '')+str(abs(value)).zfill(8)).encode()+b'\0';machine.mem_write(dst,raw);self.ret(len(raw)-1);return
            if address==0x2a29d:
                self.set32(0x53c57,0);self.ret(1);return
            if address==0x19953:
                mode=c['args'][0];self.set32(0x53c57,1 if mode==4 else 0);self.ret(-1 if mode==5 else 0);return
            if address==0x15f84:self.text.append(self.u32(sp+8))
            self.ret(1 if address==0x37324 else 0);return
        if not any(start<=address<end for start,(end,_) in FUNCTIONS.items()):raise RuntimeError(f'原码执行逃出完整Domain函数：{address:#x}')
        if 0x2968d<=address<0x2986f and not 0x29862<=address<0x2986f:raise RuntimeError('reorder只允许Save函数共享free epilogue，不执行Save')
    def run(self):
        c=self.case;kind=c['kind'];arg0,arg1=c['args'];self.partial_money=False
        entry={'reorder':0x2b777,'front':0x2b843,'hidden':0x2a07a,'promotion':0x2ae0e,'revive':0x2a43e,'money':0x26b91 if arg0 else 0x26a7a,'remove':0x1b8e7,'append':0x1bb8c,'equip':0x1c142}[kind]
        if kind=='reorder':self.cpu.mem_write(self.selection,bytes.fromhex(c['selection_hex']));args=[self.selection]
        elif kind=='front':args=[arg0]
        elif kind=='hidden':args=[self.candidates]
        elif kind=='promotion':args=[self.candidates,self.targets]
        elif kind in ('remove','append','equip'):args=[arg0,arg1]
        elif kind=='money':
            args=[arg1];final=(c['money']+(-arg1 if arg0 else arg1))&0xffffffff
            self.partial_money=not (0<=signed(c['money'])<=99999999 and 0<=signed(final)<=99999999)
        else:args=[]
        self.cpu.mem_write(self.stack,struct.pack('<'+'I'*(1+len(args)),self.stop,*args));self.cpu.reg_write(self.ESP,self.stack)
        self.cpu.emu_start(entry,self.stop,count=500000)
        if not self.boundary and (self.cpu.reg_read(self.EIP)!=self.stop or self.cpu.reg_read(self.ESP)!=self.stack+4):raise RuntimeError('原函数未通过已声明返回边界')
        raw,money=self.state();actual={'finish_roster_hex':raw.hex(),'finish_money':money}
        n=self.cpu.reg_read(self.EAX)
        if kind in ('hidden','promotion'):
            if n>32:raise RuntimeError('原候选计数超声明容量')
            actual['candidates_hex']=bytes(self.cpu.mem_read(self.candidates,n)).hex()
            if kind=='promotion':actual['promotion_targets_hex']=bytes(self.cpu.mem_read(self.targets,n)).hex()
        if kind=='revive':actual['commit_trace']=self.commits
        for key,value in actual.items():
            if c[key]!=value:raise ValueError(json.dumps({'case_id':c['case_id'],'field':key,'original':value,'C':c[key]},ensure_ascii=False))
        if kind=='append' and c['result']!=(0 if n==0xffffffff else n):raise ValueError('append原-1满包到C0的显式返回适配不同')
        if kind=='revive' and c['result']!=int(bool(self.commits)):raise ValueError('复活成功/拒绝不同')
        if kind=='front' and self.cache!=[raw[i*80+7] for i in range(c['count'])]:raise ValueError('原front后cache重建UID调用序不同')
        if bytes(self.cpu.mem_read(0x627b8,2))!=struct.pack('<H',0x7a18):raise RuntimeError('有限领域原码意外写RNG')
        return {'case_id':c['case_id'],'kind':kind,'matched':list(actual),'original':actual,
            'boundary':self.boundary or 'complete function return','original_actual_writes':self.writes,
            'explicit_stub_calls':self.calls,'cache_rebuild_UID_sequence':self.cache,'text_fragments_at_stub':self.text,
            'raw_return':n,'price_at_boundary':self.u32(0x53ae1) if kind=='revive' else None}

def compare(image,readset,executable,C_bytes):
    validate_sources(image,readset,executable);cases=load_cases(C_bytes)
    results=[Original(image,c).run() for c in cases]
    import unicorn
    return {'ok':True,'schema':'fd2-g3-preparation-machine-C-differential-v1','cases':results,
        'original_machine_code_cases':len(results),'image_sha256':IMAGE_SHA,'executable_sha256':EXE_SHA,
        'readset_sha256':digest(json.dumps(readset,sort_keys=True).encode()),'C_results_sha256':digest(C_bytes),
        'unicorn_version':unicorn.__version__,'function_chunks':{hex(va):{'end':hex(end),'sha256':sha} for va,(end,sha) in FUNCTIONS.items()},
        'C_boundary_observation':'complete a00/money; revive snapshots after each real API; no C individual-memory-write tracer',
        'source_boundary':'complete pure/candidate/item functions; positive-money animation control loops; revive entry to post-HP/next-iteration; signed unsafe digit paths only through real DWORD commit',
        'explicit_substitutions':['Watcom stack probe RET4','malloc/free','overlap-safe libc memmove bytecopy','FDICON fopen/fclose/cache append resource operations','sprintf exact precision8 signed formatting','money digit drawing/delay','revive input/confirm/text/portrait/page/music/service animation'],
        'original_RNG_executed':False,'UI_RNG_equivalence':False,'native_DOSBox_acceptance':False,'pixel_or_audio_equivalence':False,
        'fixture_provenance':'controlled finite-domain records with preserved arbitrary unknown bytes, not natural-game saves; no original resource edited',
        'scope_notes':['2b843 repeated text chooses last match; absent target original undefined copy is outside this comparison, C safe reject separately tested',
            'append original full returns-1; C primitive maps full to0; remove/equip/front/reorder raw incidental return values are not public ABI equivalence',
            'libc memmove substitute writes are labeled separately from observed original CPU stores',
            'buy/sell/give full function bytes are pinned as source context; this version executes item/money primitives, not their full UI service handlers']}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,default=ROOT/'tools/fd2_le_relocated_relbase.bin')
    parser.add_argument('--executable',type=Path,default=ROOT/'original_game/FD2.EXE')
    parser.add_argument('--readset',type=Path,default=ROOT/'build/g3/preparation-domain-readset.json')
    parser.add_argument('--c-probe',type=Path,required=True)
    parser.add_argument('--c-results',type=Path,required=True);parser.add_argument('--json',type=Path,required=True)
    args=parser.parse_args();paths=[args.image,args.executable,args.readset,args.c_results,args.c_probe];snapshots=[p.read_bytes() for p in paths]
    replay=subprocess.run([str(args.c_probe.resolve())],capture_output=True,check=True,timeout=30)
    if replay.stdout!=snapshots[3]:raise ValueError('C JSONL并非当前声明probe的实际输出')
    report=compare(snapshots[0],json.loads(snapshots[2].decode('utf-8')),snapshots[1],snapshots[3]);report['readset_file_sha256']=digest(snapshots[2])
    report['C_probe_executable_sha256']=digest(snapshots[4]);report['C_JSONL_reexecuted_from_declared_probe']=True
    if any(p.read_bytes()!=old for p,old in zip(paths,snapshots)):raise ValueError('原码/C输入在执行期间变化')
    report['input_files_unchanged']=True
    if args.json.exists() or args.json.resolve().is_relative_to(args.executable.resolve().parent):raise ValueError('结果需新文件且在原资源目录外')
    args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='cases'},ensure_ascii=False));return 0

if __name__=='__main__':raise SystemExit(main())
