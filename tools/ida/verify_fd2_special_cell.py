#!/usr/bin/env python3
"""执行原190ac的callback/No路径，只核原控制流、WORD与caller传递。

固定strict relocated镜像及G1完整Domain chunk；map/query、UI、声音、等待、
callback均为显式边界替身。高WORD在间接call前停止，不执行原表外读取。
不读SDL后态，不声称像素、RNG、callback业务或DOSBox运行等价。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from audit_g4_render_source import archive_entries

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
FUNCTION_SHA='7a444271d84c9633086c744770043f9b718aef2268d05bced52758b0a8e0a643'
RESOURCES={'FDFIELD.DAT':'81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d',
    'FDSHAP.DAT':'901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c'}


def validate_source(image,readset):
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError('strict镜像身份不同')
    rows=[f for f in readset['functions'] if f['va']==0x190ac]
    if len(rows)!=1 or rows[0]['end']!=0x1956b:raise ValueError('190ac Domain边界不同')
    chunks=rows[0]['chunks']
    if len(chunks)!=1 or chunks[0]['start']!=0x190ac or chunks[0]['end']!=0x1956b:
        raise ValueError('190ac完整chunk缺失')
    original=bytes.fromhex(chunks[0]['raw_hex'])
    if original!=image[0x190ac:0x1956b] or hashlib.sha256(original).hexdigest()!=FUNCTION_SHA:
        raise ValueError('190ac Domain与strict原字节不同')
    return rows[0]


def callback_table(image):
    targets=struct.unpack_from('<90I',image,0x51b91)
    if any(not 0x10000<=t<0x4ef29 for t in targets) or image[0x51cf9:0x51d01]!=struct.pack('<II',18,18):
        raise ValueError('90项callback表／相邻非指针边界不同')
    return {'va':'0x51b91','count':90,'end':'0x51cf9','slot0_target':hex(targets[0]),
        'adjacent_nonpointer_values':[18,18],'independent_magic_table':'0x51d01',
        'not_the_208_multitable_slot_total':True}


def audit_bindings(original):
    data={name:(original/name).read_bytes() for name in RESOURCES}
    if any(hashlib.sha256(data[n]).hexdigest()!=sha for n,sha in RESOURCES.items()):
        raise ValueError('special-cell原资源身份不同')
    fields=archive_entries(data['FDFIELD.DAT']);shapes=archive_entries(data['FDSHAP.DAT'])
    modes=[];ordinary=[];outside=[]
    for stage in range(33):
        map_data,meta=fields[stage*3:stage*3+2];attrs=shapes[meta[0]*2+1]
        width,height=struct.unpack_from('<HH',map_data);refs={}
        for y in range(height):
            for x in range(width):
                word=struct.unpack_from('<I',map_data,4+4*(y*width+x))[0]
                tile,index=word&1023,word>>16&31
                if tile*4>=len(attrs) or not attrs[tile*4]&0x60:continue
                refs.setdefault(index,[]).append([x,y])
        for index,positions in refs.items():
            if index>=16:outside.append({'stage':stage,'index':index,'initial_positions':positions})
            else:
                mode=meta[0x53+index*3];value=struct.unpack_from('<H',meta,0x54+index*3)[0]
                ordinary.append({'stage':stage,'index':index,'mode':mode,'value':value,'initial_positions':positions})
        for index in range(16):
            mode=meta[0x53+index*3];value=struct.unpack_from('<H',meta,0x54+index*3)[0]
            if mode not in (0,1):modes.append({'stage':stage,'index':index,'mode':mode,'callback_word':value,
                'initial_positions':refs.get(index,[]),'dynamic_reachability_claimed':False})
    return {'resource_sha256':RESOURCES,'initial_special_cells':ordinary,
        'nonordinary_metadata':modes,'initial_indices_outside_metadata16':outside,
        'scope':'原初始map/shape/metadata引用，不证明动态换图可达性，不把未引用改称无需求'}


def execute(image,mode=2,word=39,actor=1,yes=True,flags=0x20,completed=False,observe_word=False):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x10000);cpu.mem_map(0x200000,0x10000)
    stop,stack,units,state,metadata=0x400000,0x208000,0x100000,0x101000,0x102000
    def read(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value))
    put(0x53a45,units);put(0x53ad5,state);put(0x53a55,metadata);put(0x53c57,0)
    cpu.mem_write(units+actor*80+7,b'\x09');cpu.mem_write(state+2,bytes([int(completed)]))
    cpu.mem_write(metadata+0x53+2*3,struct.pack('<BH',mode,word))
    cpu.mem_write(stack,struct.pack('<II',stop,actor));cpu.reg_write(UC_X86_REG_ESP,stack)
    calls=[];observed=None
    stub_targets={0x12e38:'map',0x4e381:'clear_keys',0x1956b:'open',0x15f84:'text',
        0x16559:'portrait',0x19953:'choice',0x197e5:'choice_close',0x25b45:'cue',
        0x4e127:'delay',0x196cb:'close'}
    # j___delay是静态FLIRT别名；从实际190ac的E8取得其已核入口。
    delay=0x191b6+5+struct.unpack_from('<i',image,0x191b7)[0];stub_targets.pop(0x4e127);stub_targets[delay]='delay'
    callback=struct.unpack_from('<I',image,0x51b91+39*4)[0];stub_targets[callback]='callback'
    def hook(cpu,address,size,userdata):
        nonlocal observed
        if address==stop:cpu.emu_stop();return
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address==0x19511 and observe_word:
            observed=cpu.reg_read(UC_X86_REG_EAX);cpu.emu_stop();return
        name=stub_targets.get(address)
        if name is None:return
        args=[read(sp+4+i*4) for i in range(10)];result=0
        if name=='map':cpu.mem_write(args[2],struct.pack('<HHB',0,2,flags))
        elif name=='choice':result=int(yes)
        # 本caller实际9个push：entry,fragment,dest,...；不保存伪C猜测ABI。
        if name=='text':calls.append(['text',args[1]])
        elif name in ('delay','portrait','open','callback'):calls.append([name,args[0]])
        elif name=='cue':calls.append([name,args[1],args[2]])
        elif name not in ('map',):calls.append([name])
        cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    cpu.emu_start(0x190ac,stop,count=20000)
    if not observe_word and cpu.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError('原函数未返回')
    expected=[]
    if flags&0x60 and not completed:
        expected=[['clear_keys'],['open',9],['text',421 if flags&0x20 else 428],['portrait',0],['choice'],['choice_close']]
        if yes:
            expected += [['cue',12,1],['delay',300],['delay',200],['close']]
            if not observe_word:expected += [['callback',actor]]
        else:expected += [['delay',100],['text',412],['delay',200],['close']]
    if calls!=expected or (observe_word and observed!=word):
        raise ValueError('190ac原轨迹/WORD/caller与独立期望不同:'+str({'actual':calls,'expected':expected,'word':observed}))
    if cpu.mem_read(state+2,1)!=bytes([int(completed)]):raise ValueError('callback前缀擅自设置cell完成位')
    return {'mode':mode,'callback_word':word,'actor':actor,'yes':yes,'flags':flags,'already_complete':completed,
        'calls':calls,'word_before_indirect_call':observed,'callback_body_stubbed':True,
        'unknown_target_executed':False,'returned':not observe_word}


def verify_cold(document):
    f=next(f for f in document['flows'] if f['id']=='loot');n={x['id']:x for x in f['nodes']}
    if 'preflight.cell.stage' in n or n['preflight.cell.done']['on']['yes']!='preflight.cell.index':
        raise ValueError('非原义stage门仍存在')
    expected={
        'cell.yes.cue':('cell_open_cue',[],{'next':'cell.yes.delay','error':'failure'}),
        'cell.yes.delay':('emit',[5,0,0,300,0,0],{'error':'failure','ack':'cell.mode'}),
        'cell.money_mode':('branch_descriptor_mode',[0,1],{'yes':'cell.money.nonzero','no':'cell.callback.wait','error':'failure'}),
        'cell.callback.wait':('emit',[5,0,0,200,0,0],{'error':'failure','ack':'cell.callback.close'}),
        'cell.callback.close':('emit',[5,0,4,0,0,0],{'error':'failure','ack':'cell.callback'}),
        'cell.callback':('event_dispatch',[],{'next':'cell.callback.completed','error':'failure'}),
        'preflight.cell.cue':('cell_cue_preflight',[],{'next':'preflight.cell.mode','error':'reject'}),
        'preflight.cell.callback':('cell_callback_preflight',[],{'next':'preflight.cell.chest','error':'reject'})}
    for label,(op,args,on) in expected.items():
        actual=n.get(label)
        if actual is None or (actual['op'],actual['args'],actual['on'])!=(op,args,on):raise ValueError('190ac冷前缀不同:'+label)
    for label in ('cell.ask_chest','cell.ask_other'):
        if n[label]['on']['yes']!='cell.yes.cue':raise ValueError('确认不能略过cue12')
    return {'cold_prefix_nodes':len(expected),'original_200ms_before_close':True,'stage_gate_removed':True}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('image','readset','original','cold','output'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    image=args.image.read_bytes();validate_source(image,json.loads(args.readset.read_text(encoding='utf-8')))
    cases=[execute(image,mode=mode,actor=actor,yes=yes,flags=flags)
        for mode,actor,yes,flags in ((2,0,True,0x20),(2,1,True,0x20),(3,1,True,0x40),
            (255,1,True,0x20),(2,1,False,0x20),(255,1,False,0x40))]
    cases += [execute(image,completed=True),execute(image,flags=0),execute(image,word=0x127,observe_word=True)]
    result={'ok':True,'schema':'fd2.g4.special-cell-original.v1','image_sha256':IMAGE_SHA,
        'readset_sha256':hashlib.sha256(args.readset.read_bytes()).hexdigest(),
        'function':{'va':'0x190ac','end':'0x1956b','relocated_sha256':FUNCTION_SHA},'cases':cases,
        'callback_table':callback_table(image),
        'bindings':audit_bindings(args.original),'cold':verify_cold(json.loads(args.cold.read_text(encoding='utf-8'))),
        'scope':'完整原190ac CPU前缀；明确替换UI/map/声音/等待/callback，非像素、RNG、handler业务或原版窗口验收'}
    if args.output.exists():raise ValueError('输出需新路径，不覆盖旧证据')
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'ok':True,'cases':len(cases),'output':str(args.output)},ensure_ascii=False))
    return 0


if __name__=='__main__':raise SystemExit(main())
