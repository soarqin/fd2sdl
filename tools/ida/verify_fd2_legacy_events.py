"""六个原既有事件的完整CPU调用/直接写序；独立于SDL定义生成器。

constructor/group/arrival/motion/text/refresh/BIOS/delay均明确stub，只采caller
参数/原直接SIDE与AI写点；134e4本体真实执行。不是全战斗、像素或文本RNG等价。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
FUNCTIONS={
    0x34531:(0x3460b,'3cf8c461d9d91352412b98d6a52500283882a38647dbe5ee1a04e4ec59df773a'),
    0x3460b:(0x34673,'88b5595c3d77534cac4e289f215346dab156a37cd95a0a77f35b41a75b724137'),
    0x34673:(0x346cd,'4e854f5e0220b54bed858c95917bc3eac128dc4c39b6fc2c4ec206c7303792b9'),
    0x346cd:(0x34738,'dae2e0f529c6590a31600249f1e5397c91228e55869003e42d4c310efef2cf7d'),
    0x34738:(0x34778,'18e472b6310606cdb2a4cbc788c3dd27de8e1235dbefa4a6f3a01c71b3d88797'),
    0x34778:(0x34818,'95395ee3b336a55bad9395cdcdfced2617dd0e155849354aedb5487c1a9f6774'),
    0x134e4:(0x13512,'1b3991fcb227ba71e5c2de0525206f8051787eb9c98c625366245ec31051554a'),
}

def source_groups(field,stage):
    if field[:6]!=b'LLLLLL':raise ValueError('原FDFIELD目录身份不同')
    a,b=struct.unpack_from('<II',field,6+(stage*3+1)*4);meta=field[a:b]
    if len(meta)<131 or (len(meta)-131)%26 or meta[2]>(len(meta)-131)//26:
        raise ValueError('原metadata有效模板范围不符')
    groups={}
    for i in range(meta[2]):
        group=meta[131+i*26+21];groups[group]=groups.get(group,0)+1
    return groups

def execute(image,field,entry):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x10000);cpu.mem_map(0x200000,0x10000)
    units,stack,stop=0x100000,0x208000,0x400000
    stage=entry==0x34778
    count={0x34531:12,0x3460b:14,0x34673:18,0x346cd:23,0x34738:27,0x34778:21}[entry]
    groups=source_groups(field,int(stage));events=[];writes=[];stories=[]
    def word(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
    def put(a,v):cpu.mem_write(a,struct.pack('<I',v))
    records=bytearray(96*80)
    for i in range(96):records[i*80+3]=3;records[i*80+53]=0xa0;records[i*80+54]=0xb0
    cpu.mem_write(units,bytes(records));put(0x53a45,units);put(0x53beb,count);put(0x53bfb,4)
    cpu.mem_write(0x53afa,b'\0');put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
    nargs={0x112a5:1,0x10b4e:1,0x135dd:2,0x11cac:1,0x3790a:1,0x1366a:1,
        0x4e381:0,0x15f84:9,0x32999:1}
    def code(machine,address,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address==0x134e4:events.append(('directions_reset',[]))
        if address in nargs:
            args=[word(sp+4+4*i) for i in range(nargs[address])]
            if address==0x112a5:events.append(('roster_append',args));put(0x53bfb,word(0x53bfb)+1)
            elif address==0x10b4e:
                force=cpu.mem_read(0x53afa,1)[0];events.append(('append_group',[args[0],force]))
                put(0x53beb,word(0x53beb)+groups.get(args[0],0))
            elif address==0x135dd:events.append(('pan',args))
            elif address==0x11cac:events.append(('refresh_map',args))
            elif address==0x3790a:events.append(('delay_ms',args))
            elif address==0x1366a:events.append(('move',[args[0],0]))
            elif address==0x4e381:events.append(('clear_keys',[]))
            elif address==0x15f84:
                events.append(('story',[args[1]]))
                stories.append({'fragment':args[1],'count':word(0x53beb),'roster_count':word(0x53bfb),
                    'ai':[[cpu.mem_read(units+i*80+53,1)[0],cpu.mem_read(units+i*80+54,1)[0]] for i in range(5,11)]})
            elif address==0x32999:
                events.extend([('arrival_prepare',[]),('append_group',[args[0],0]),
                    ('arrival_frames',[]),('arrival_close',[])])
                put(0x53beb,word(0x53beb)+groups.get(args[0],0))
            cpu.reg_write(UC_X86_REG_EAX,0)
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if not any(a<=address<b for a,(b,_) in FUNCTIONS.items()):
            raise ValueError(f'未声明原指令域 {address:#x}')
    def write(machine,access,address,size,value,data):
        if address==0x51a83:events.append(('display_mode',[value]))
        if units<=address<units+96*80:
            raw,offset=divmod(address-units,80)
            writes.append({'raw':raw,'offset':offset,'size':size,'value':value,'after_events':len(events)})
            if offset==6:events.append(('unit_field_write',[raw,raw,5,0,value]))
            if offset in (53,54):events.append(('anchor_byte',[raw,offset,value]))
    cpu.hook_add(unicorn.UC_HOOK_CODE,code);cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write)
    cpu.emu_start(entry,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError('原函数未正常返回')
    anchor=[e for e in events if e[0]=='anchor_byte']
    if anchor:
        if anchor!=[('anchor_byte',[i,o,v]) for i in range(5,11) for o,v in ((53,26),(54,15))]:
            raise ValueError('原AI锚点不是逐raw35/36成对写')
        at=next(i for i,e in enumerate(events) if e[0]=='anchor_byte')
        events=events[:at]+[('ai_anchor',[5,10,26,15])]+events[at+12:]
    if entry==0x34531 and [s['count'] for s in stories]!=[13,14]:raise ValueError('原两story人数写点不同')
    if entry==0x34778 and (stories[0]['count']!=27 or any(a!=[0xa0,0xb0] for a in stories[0]['ai'])):
        raise ValueError('原stage1锚点在story前写入')
    return {'entry':hex(entry),'program':events,'stories':stories,'record_writes':writes,'final_count':word(0x53beb)}

def cold_program(flow):
    by={n['id']:n for n in flow['nodes']};pc=flow['entry'];result=[]
    for _ in range(256):
        n=by[pc]
        if n['op']=='end':return result
        if n['op']=='fail':raise ValueError('正常source序落错误叶')
        result.append((n['op'],n['args']))
        port='ack' if n['op']=='story' else 'next'
        if set(n['on'])!={port,'error'}:raise ValueError('源等待/错误端口不同')
        pc=n['on'][port]
    raise ValueError('冷事件未正常结束')

def check(image,field,document):
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError('strict relocated镜像身份不同')
    if hashlib.sha256(field).hexdigest()!='81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d':
        raise ValueError('原FDFIELD身份不同')
    for a,(b,sha) in FUNCTIONS.items():
        if hashlib.sha256(image[a:b]).hexdigest()!=sha:raise ValueError('原完整函数字节不同')
    entries=[0x34531,0x3460b,0x34673,0x346cd,0x34738,0x34778];actions=[0,1,2,3,4,6]
    if any(struct.unpack_from('<I',image,0x51b91+action*4)[0]!=entry for action,entry in zip(actions,entries)):
        raise ValueError('原callback表绑定不同')
    expected={f'stage.{1 if a==6 else 0:03d}.action.{a:03d}':f'common.legacy.action.{a:03d}' for a in actions}
    if document.get('bindings')!=expected:raise ValueError('只可迁移原六绑定，不可新增S绑定')
    flows={f['id']:f for f in document['flows']};cases=[]
    for entry,action in zip(entries,actions):
        row=execute(image,field,entry)
        if cold_program(flows[f'common.legacy.action.{action:03d}'])!=row['program']:
            raise ValueError(f'冷图不等于完整原调用/写序 {entry:#x}')
        cases.append(row)
    return {'ok':True,'schema':'fd2.g5.legacy-original-cpu.v1','cases':cases,'image_sha256':IMAGE_SHA,
        'field_sha256':hashlib.sha256(field).hexdigest(),'original_cpu_executed':True,
        'scope':'six complete callers and real134e4; constructors/group/arrival/motion/text/BIOS/delay explicitly stubbed; not full gameplay, pixels, text RNG or native WGC'}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True)
    p.add_argument('--field',type=Path,required=True);p.add_argument('--flows',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=check(a.image.read_bytes(),a.field.read_bytes(),json.loads(a.flows.read_text()))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'ok':True,'cases':len(result['cases'])}))

if __name__=='__main__':main()
