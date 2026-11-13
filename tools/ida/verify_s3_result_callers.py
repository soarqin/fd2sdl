"""S3五个结果表CALLsite的完整原caller局部机器码执行。

结果函数、AI/候选、输入/呈现/奖励/死亡清理是显式叶替身；不以本脚本证明
这些叶的业务、DOSBox或图像。完整916结果原码另验。本脚本仅证明原caller
raw顺序、pending先于结果、结果之后的13565/六状态写点及非零退出边界。
正式App scheduler拥有AI outer查询；同步commit_ai_*机制API仅动作helper，
不在本收据内，也不由本工具升级为完整outer等价。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from g1_preparation_evidence import RELOCATED_SHA256
from s3_result_callers import TARGETS

UNIT, STACK, STOP = 0x100000, 0x208000, 0x400000
TABLE, RESULT, STAGE = 0x51b19, 0x53ecc, 0x53c03
CALLS={0x1197b:0x117e7,0x1a94d:0x1a866,0x1d8a0:0x1d80b,0x1d96c:0x1d8ba,0x1d9fc:0x1d8ba}

def validate(image, report):
    if hashlib.sha256(image).hexdigest()!=RELOCATED_SHA256:raise ValueError('strict image differs')
    if report.get('schema')!='fd2-s3-result-callers-v1' or [f['va'] for f in report['functions']]!=list(TARGETS):
        raise ValueError('caller readset differs')
    found={};functions={f['va']:f for f in report['functions']}
    for f in functions.values():
        if not f['chunks'] or not f['blocks'] or not f['instructions']:raise ValueError('incomplete caller')
        for c in f['chunks']:
            raw=bytes.fromhex(c['raw_hex']);a,b=c['start'],c['end']
            if image[a:b]!=raw or hashlib.sha256(raw).hexdigest()!=c['sha256']:raise ValueError('caller bytes differ')
        for i in f['instructions']:
            if i['disasm'].startswith('call    funcs_1197B['):found[i['ea']]=f['va']
    if found!=CALLS:raise ValueError('five original table CALLsites differ')
    refs={r['from'] for r in report['table_refs']}
    if refs!=set(CALLS):raise ValueError('complete table xrefs differ')
    return functions

def records(side,count=5):
    data=bytearray(count*80)
    for i in range(count):
        data[i*80+6]=side;struct.pack_into('<HH',data,i*80+64,100,100)
    if count==5:
        data[6]=2;data[80+5]=1;data[160+5]=128;data[240+38]=1;data[320+5]=4
    return bytes(data)

def execute(image,functions,*,kind,stop_after=0,score=0,pending=False,timer=2):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    entry={'player':0x117e7,'status':0x1a866,'side1':0x1d80b,'side0':0x1d8ba}[kind]
    data=bytearray(records(1 if kind in ('side1','status') else 0,1 if kind in ('player','status') else 5))
    if kind=='player':data[6]=2
    if kind=='status':data[34:40]=bytes([timer])*6
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(UNIT,0x10000);cpu.mem_write(UNIT,bytes(data));cpu.mem_map(0x200000,0x10000)
    def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
    def put(a,v):cpu.mem_write(a,struct.pack('<I',v))
    for a,v in ((0x53a45,UNIT),(0x53beb,len(data)//80),(STAGE,12),(RESULT,0),(0x51a8f,255)):
        put(a,v)
    put(STACK,STOP);put(STACK+4,1);cpu.reg_write(UC_X86_REG_ESP,STACK)
    rows=[];substitutes={};visited=set();unit_writes=[];tail=[False]
    ranges=[(c['start'],c['end']) for f in functions.values() for c in f['chunks']]
    result_target=get(TABLE+12*4);pending_target=get(0x51b91)
    if result_target!=0x20765:raise ValueError('actual stage12 table target differs')
    def ret(value=0,cleanup=0):
        sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EAX,value)
        cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
    def add_stub(prefix,label,action=None):
        sites=[i for f in functions.values() for i in f['instructions'] if i['disasm'].startswith('call    '+prefix)]
        if not sites:raise ValueError('missing original leaf '+prefix)
        for insn in sites:
            at=insn['ea']
            if image[at]!=0xe8:raise ValueError('not direct leaf CALL')
            target=at+5+struct.unpack_from('<i',image,at+1)[0]
            substitutes[target]=(label,action)
    add_stub('watcom_stack_check','Watcom stack probe',lambda:ret(cleanup=4))
    add_stub('bios_keyboard_buffer_clear','BIOS clear',lambda:ret())
    def ai():
        sp=cpu.reg_read(UC_X86_REG_ESP);rows.append({'leaf':'ai','actor':get(sp+4),'side':get(sp+8)})
        if pending:put(0x51a8f,0)
        ret()
    add_stub('field_ai_unit_execute','AI body (no RNG)',ai)
    add_stub('field_ai_magic_candidate_evaluate','magic candidate score',lambda:(put(0x53c23,score),ret()))
    add_stub('field_ai_item_candidate_evaluate','item candidate score',lambda:(put(0x53c33,score),ret()))
    for prefix,label in (('field_focus_move_to_actor','focus'),('field_bottom_notice_open','open'),
            ('text_dialog_render_tokens','text'),('field_notice_wait_ticks','wait'),
            ('field_bottom_notice_close','close'),('field_unit_combat_stats_recompute','recompute')):
        add_stub(prefix,label,lambda:ret())
    def finalize():rows.append({'leaf':'finalize','timers':list(cpu.mem_read(UNIT+34,6))});ret()
    add_stub('field_defeated_units_finalize','death finalize (positive HP)',finalize)
    add_stub('sub_11AA8','one Enter input',lambda:ret(28))
    add_stub('field_visible_actor_at_focus','visible raw0',lambda:ret(0))
    add_stub('field_player_unit_action','completed selection/cancel',lambda:ret(1))
    add_stub('sfx_play','sound',lambda:ret())
    add_stub('sub_11CAC','render',lambda:ret())
    add_stub('field_unit_experience_reward_execute','reward',lambda:ret())
    def post():rows.append({'leaf':'13565','latch':get(RESULT)});ret()
    add_stub('sub_13565','controller post',post)
    def code(machine,address,size,userdata):
        if address==result_target:
            sp=cpu.reg_read(UC_X86_REG_ESP);site=get(sp)-7 # indexed CALL ff1485 disp32 is 7B
            if site not in CALLS:raise ValueError('unexpected actual result CALLsite')
            rows.append({'leaf':'result','callsite':site,'return_pc':get(sp),'actor':get(sp+4),
                'hp':struct.unpack('<H',cpu.mem_read(UNIT+64,2))[0],
                'timers':list(cpu.mem_read(UNIT+34,6))})
            if stop_after and sum(r['leaf']=='result' for r in rows)==stop_after:put(RESULT,1)
            ret();return
        if pending and address==pending_target:
            sp=cpu.reg_read(UC_X86_REG_ESP);rows.append({'leaf':'pending','actor':get(sp+4)});ret();return
        if address in substitutes:substitutes[address][1]();return
        if kind=='status' and address==0x190a7:
            # 原439B body的共享return跳转；未执行/替换为本地结束点，非完整ABI返回声明。
            tail[0]=True;machine.emu_stop();return
        if not any(a<=address<b for a,b in ranges):raise ValueError(f'undeclared original instruction {address:#x}')
        visited.add(address)
    def write(machine,access,address,size,value,userdata):
        if UNIT<=address<UNIT+len(data):
            unit_writes.append({'instruction_va':cpu.reg_read(UC_X86_REG_EIP),'offset':address-UNIT,'size':size,'value':value,'results_before':sum(r['leaf']=='result' for r in rows)})
        elif not (0x200000<=address<0x210000 or address in (RESULT,0x51a42,0x51a83,0x51a8f,0x53ec8,0x53ae1)):
            raise ValueError(f'undeclared original write {address:#x}')
    def read(machine,access,address,size,value,userdata):
        if UNIT<=address<UNIT+0x10000 and address+size>UNIT+len(data):
            raise ValueError('original caller read beyond declared raw prefix')
    cpu.hook_add(unicorn.UC_HOOK_CODE,code);cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write)
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ,read)
    cpu.emu_start(entry,STOP,count=100000)
    if not tail[0] and cpu.reg_read(UC_X86_REG_EIP)!=STOP:raise ValueError('original caller did not finish')
    results=[r for r in rows if r['leaf']=='result']
    expected=list(range(len(data)//80))*(2 if kind=='side0' else 1)
    if kind in ('player','status'):expected=[0]
    if stop_after:expected=expected[:stop_after]
    if [r['actor'] for r in results]!=expected:raise ValueError('original raw cadence differs')
    if kind=='status':
        if results[0]['timers']!=[timer]*6 or not rows or rows[0]['leaf']!='finalize':raise ValueError('status order differs')
        if list(cpu.mem_read(UNIT+34,6))!=[max(timer-1,0)]*6:raise ValueError('six timer suffix differs')
        if any(w['offset'] in range(34,40) and w['results_before']!=1 for w in unit_writes):raise ValueError('timer wrote before query')
    if kind=='player' and [r['leaf'] for r in rows]!=['result','13565']:raise ValueError('player post order differs')
    if pending:
        for i,r in enumerate(rows):
            if r['leaf']=='pending' and (i+1>=len(rows) or rows[i+1]['leaf']!='result' or rows[i+1]['actor']!=r['actor']):
                raise ValueError('pending/result order differs')
    return {'kind':kind,'score':score,'pending_substitute':pending,'stop_after':stop_after,
        'trace':rows,'unit_writes':unit_writes,'visited_instructions':len(visited),
        'body_completed':True,'shared_status_return_unexecuted':tail[0],
        'leaf_substitutes':sorted({label for label,_ in substitutes.values()}|{'stage12 result leaf'})}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True)
    p.add_argument('--readset',type=Path,required=True);p.add_argument('--probe',type=Path)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args(argv)
    if a.output.exists():raise ValueError('output exists')
    image=a.image.read_bytes();raw=a.readset.read_bytes();functions=validate(image,json.loads(raw))
    vectors=[{'kind':kind,'stop_after':stop,'score':score,'pending':pending}
        for kind in ('side0','side1') for stop in (0,1,3) for score in (0,6) for pending in (False,True)]
    vectors += [{'kind':'player'},{'kind':'status','timer':2},{'kind':'status','timer':1}]
    results=[execute(image,functions,**v) for v in vectors];receipt=None
    if a.probe:
        sha=hashlib.sha256(a.probe.read_bytes()).hexdigest()
        run=subprocess.run([str(a.probe.resolve()),'--caller-vectors'],capture_output=True,text=True,check=True)
        rows=[json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]
        expected=[{'side':side,'actors':[r['actor'] for r in next(x for x in results if x['kind']==f'side{side}' and not x['stop_after'] and not x['score'] and not x['pending_substitute'])['trace'] if r['leaf']=='result']} for side in (0,1)]
        if rows!=expected:raise ValueError('actual C caller cadence differs from original')
        after=hashlib.sha256(a.probe.read_bytes()).hexdigest()
        if after!=sha:raise ValueError('C probe changed')
        receipt={'before_sha256':sha,'after_sha256':after,'raw_cadence_matches':True,
            'additional_C_only_scope':'yield owner, wrong/HOST/ERROR/repeated ACK, reward/loot PC, nonplayer child, no early RNG'}
    report={'schema':'fd2-s3-result-caller-oracle-v1','cases':len(results),'passed':True,
        'five_callsites':CALLS,'image_sha256':RELOCATED_SHA256,'readset_sha256':hashlib.sha256(raw).hexdigest(),
        'tool_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'actual_c':receipt,'results':results,'original_runtime_executed':False,
        'limits':'complete original caller bytes, selected controlled CFG paths with declared leaf substitutes; not all input branches or original AI/result/render/clock/GUI or natural stages; synchronous commit_ai helper APIs excluded (formal App scheduler owns outer query)'}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'path':str(a.output),'cases':len(results),'passed':True,'actual_c':bool(receipt)}))
if __name__=='__main__':main()
