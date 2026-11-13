"""S3 总览条件：完整1b41d/1b5f1/33499/34894原码与实际C只读query差分。

仅替代Watcom栈检查、blit/数字/文本绘制；真实执行完整条件和roster循环，
捕获原两次文本实参。不是原版WGC、真实字体/渲染或完整Game/RNG证明。
Domain脚本可先调用collect(database,image_path,output_dir)重取完整只读来源。
"""
from __future__ import annotations
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/static_data'))
from static_data.stage_source_ledger import validate_output

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
FUNCTIONS={0x1b41d:0x1b5f1,0x1b5f1:0x1b653,0x33499:0x334d9,0x34894:0x348bb}
ROSTER,ACTIVE,STACK,STOP=0x110000,0x100000,0x208000,0x400000


def digest(value):return hashlib.sha256(value).hexdigest()


def validate(image,source):
    database_sha=source.get('database_before_sha256')
    if (digest(image)!=IMAGE_SHA or source.get('image_sha256')!=IMAGE_SHA or
        source.get('schema')!='fd2-s3-overview-source-v1' or
        not isinstance(database_sha,str) or len(database_sha)!=64 or
        any(ch not in '0123456789abcdef' for ch in database_sha) or
        source.get('database_before_sha256')!=source.get('database_after_sha256')):raise ValueError('strict image/readset身份不同')
    rows={row['va']:row for row in source['functions']}
    if len(rows)!=len(source['functions']) or set(rows)!=set(FUNCTIONS):raise ValueError('总览完整函数集合不符')
    for va,end in FUNCTIONS.items():
        row=rows[va]
        if row['end']!=end or [(c['start'],c['end']) for c in row['chunks']]!=[(va,end)]:raise ValueError('原边界/完整chunks不同')
        chunk=row['chunks'][0];raw=bytes.fromhex(chunk['raw_hex'])
        if raw!=image[va:end] or digest(raw)!=chunk['sha256']:raise ValueError('Domain/strict source bytes不同')
        instructions=row['instructions']
        cursor=va
        for i in instructions:
            if type(i['size']) is not int or i['size']<=0 or i['ea']!=cursor or cursor+i['size']>end:
                raise ValueError('Domain指令未完整覆盖原函数')
            if bytes.fromhex(i['raw_hex'])!=image[i['ea']:i['ea']+i['size']]:raise ValueError('原指令身份不同')
            cursor+=i['size']
        if cursor!=end:raise ValueError('Domain指令未完整覆盖原函数')
    return rows


def collect(database,image_path,output_dir):
    """由ida-domain-scripting完整脚本调用；不保存、标注或修改数据库。"""
    from ida_domain import Database
    from ida_domain.database import IdaCommandOptions
    from g1_preparation_evidence import function_record
    output=validate_output(Path(output_dir),root=ROOT,original=(ROOT/'original_game').resolve())
    database=Path(database).resolve();image_path=Path(image_path).resolve()
    image=image_path.read_bytes();before=digest(database.read_bytes());records=[]
    if digest(image)!=IMAGE_SHA:raise ValueError('strict image身份不同')
    with Database.open(str(database),IdaCommandOptions(auto_analysis=False),save_on_close=False) as db:
        for va in FUNCTIONS:
            row=function_record(db,va);instructions={}
            for chunk in row['chunks']:
                for insn in db.instructions.get_between(chunk['start'],chunk['end']):
                    instructions[insn.ea]={'ea':insn.ea,'size':insn.size,
                        'raw_hex':db.bytes.get_bytes_at(insn.ea,size=insn.size).hex(),
                        'disasm':db.instructions.get_disassembly(insn)}
            row['instructions']=[instructions[ea] for ea in sorted(instructions)]
            records.append(row)
    report={'schema':'fd2-s3-overview-source-v1','functions':records,'image_sha256':digest(image),
        'database_before_sha256':before,'database_after_sha256':digest(database.read_bytes()),
        'full_instruction_bytes':sum(c['end']-c['start'] for row in records for c in row['chunks']),
        'scope':'full original overview CFG + active counts + independent roster predicate; no save or guessed ABI'}
    validate(image,report)
    if digest(image_path.read_bytes())!=IMAGE_SHA:raise ValueError('读取期间strict image变化')
    output.mkdir(parents=True);path=output/'domain.json'
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    return {'path':str(path),'functions':len(records),'bytes':report['full_instruction_bytes'],'sha256':digest(path.read_bytes())}


def cases():
    for stage in range(30):
        for count,present,where,flags,hp,side in itertools.product((0,1,32),(False,True),(0,-1),(0,255),(0,65535),(0,2,255)):
            if not count and (present or where or flags or hp or side):continue
            # 普通stage也有roster噪声；不能把active18或HP/hidden/side当私有门。
            if stage!=16 and (where or flags or hp or side):continue
            roster=bytearray(count*80)
            for raw in range(count):roster[raw*80+8]=17
            if present and count:
                at=(count-1 if where else 0)*80;roster[at+8]=18;roster[at+5]=flags;roster[at+6]=side
                struct.pack_into('<H',roster,at+64,hp)
            yield {'stage':stage,'roster':roster.hex(),'count':count,'active_text18':True}


def execute(image,case):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(ACTIVE,0x30000);cpu.mem_map(0x200000,0x10000)
    roster=bytes.fromhex(case['roster']);active=bytearray(4*80)
    for raw in range(4):active[raw*80+6]=raw%3;active[raw*80+7]=raw;active[raw*80+8]=18
    active[80+5]=1;active[160+7]=121;active[240+31]=10
    cpu.mem_write(ROSTER,roster);cpu.mem_write(ACTIVE,bytes(active))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value))
    for at,value in ((0x53c03,case['stage']),(0x53bef,123),(0x53bf3,456),(0x53bf7,ROSTER),
        (0x53bfb,case['count']),(0x53a45,ACTIVE),(0x53beb,4),(0x53a7d,0x120000),(0x53a81,0x125000)):
        put(at,value)
    put(STACK,STOP);put(STACK+4,0);put(STACK+8,320);cpu.reg_write(UC_X86_REG_ESP,STACK)
    texts=[];visited=set();roster_calls=[]
    def code(machine,address,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address in (0x16886,0x187d6,0x15f84):
            if address==0x15f84:
                args=[get(sp+4+4*i) for i in range(9)]
                if args[0]!=0x120000 or args[3:]!=[320,205,76,0,19,0]:raise ValueError('原UI0文本九参合同不同')
                texts.append({'fragment':args[1],'args':args})
            cpu.reg_write(UC_X86_REG_EAX,0);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if not any(lo<=address<hi for lo,hi in FUNCTIONS.items()):raise ValueError(f'未声明原指令{address:#x}')
        if address==0x33499:roster_calls.append(get(sp+4))
        visited.add(address)
    def write(machine,access,address,size,value,data):
        if not 0x200000<=address<0x210000:raise ValueError(f'原条件意外改业务owner{address:#x}')
    def read(machine,access,address,size,value,data):
        if ROSTER<=address<ROSTER+0x10000 and address+size>ROSTER+len(roster):raise ValueError('原函数越出声明roster')
    cpu.hook_add(unicorn.UC_HOOK_CODE,code);cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write);cpu.hook_add(unicorn.UC_HOOK_MEM_READ,read)
    cpu.emu_start(0x1b41d,STOP,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=STOP or len(texts)!=2:raise ValueError('完整1b41d未正常返回两次文本')
    if roster_calls!=([18] if case['stage']==16 else []):raise ValueError('完整CFG的stage/roster调度不同')
    if bytes(cpu.mem_read(ROSTER,len(roster)))!=roster or bytes(cpu.mem_read(ACTIVE,len(active)))!=active:raise ValueError('原owner被修改')
    return {'fragments':[t['fragment'] for t in texts],'texts':texts,'roster_calls':roster_calls,'visited':sorted(visited),'owner_unchanged':True}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('readset','image','output-dir'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--c-probe',type=Path);args=parser.parse_args(argv)
    output=validate_output(args.output_dir,root=ROOT,original=(ROOT/'original_game').resolve())
    image=args.image.read_bytes();source=json.loads(args.readset.read_bytes());validate(image,source)
    vectors=list(cases());expected=[execute(image,v) for v in vectors]
    inputs={str(Path(p).resolve()):digest(Path(p).read_bytes()) for p in (args.image,args.readset,Path(__file__),ROOT/'data/static/tables/field_info.json')}
    compared=False
    if args.c_probe:
        probe=args.c_probe.resolve();before=digest(probe.read_bytes());lines=[f"{v['stage']} {v['count']} {v['roster'] or '-'}" for v in vectors]
        completed=subprocess.run([str(probe),'--overview-vectors'],input='\n'.join(lines)+'\n',text=True,capture_output=True,check=True,timeout=60)
        rows=completed.stdout.splitlines()
        if digest(probe.read_bytes())!=before or len(rows)!=len(expected):raise ValueError('C probe身份/向量数变化')
        for want,line in zip(expected,rows):
            actual=json.loads(line)
            if actual.get('status')!=0 or actual.get('fragments')!=want['fragments'] or actual.get('owner_unchanged') is not True:
                raise ValueError('实际C与原完整条件片段/只读owner不同: '+line)
        inputs[str(probe)]=before;compared=True
    if any(digest(Path(path).read_bytes())!=sha for path,sha in inputs.items()):raise ValueError('比较期间来源改变')
    report={'schema':'fd2-s3-overview-original-C-v1','vectors':len(vectors),'original_CPU_executed':True,'actualC_compared':compared,
        'inputs_sha256':inputs,'cases':[dict(v,original=e) for v,e in zip(vectors,expected)],
        'stage16_fragments':sorted({tuple(e['fragments']) for v,e in zip(vectors,expected) if v['stage']==16}),
        'scope':'complete condition/count/roster CPU; rendering/stack-probe explicit stubs; not original WGC or whole Game/RNG'}
    output.mkdir(parents=True);(output/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:report[k] for k in ('vectors','original_CPU_executed','actualC_compared','stage16_fragments')}))
    return 0


if __name__=='__main__':raise SystemExit(main())
