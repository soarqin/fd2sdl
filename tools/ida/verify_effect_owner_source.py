#!/usr/bin/env python3
"""G4效果页owner原字节与1cac7参数栈；绘制/分配/等待为明确替身。

输入是已保存的Domain读集和独立strict relocated镜像，均只读。只核调用与栈，
不验证像素、业务或DOSBox运行；Unicorn仅在执行CPU向量时加载。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
FUNCTION_CHUNKS={
    0x1c2da:((0x1c2da,0x1c4cc,'0b454c5ae0ebe969b6e490a17be3695eb95905595dd7cf31ac1b06703512156a'),),
    0x1c4cc:((0x1c4cc,0x1c75e,'9ab69b54c8753610449d18d5869a55145a90f4d3f638e4d8895b34afd510b4e8'),),
    0x1cac7:((0x1cac7,0x1cb94,'1210d0b96be5275f3c893984bfe3d908a20c309b62187c2d352f76b036654741'),),
    0x1cb94:((0x1cb94,0x1cd17,'7bf45203e80d5f3002b3c2a562aca93bbd0f52eefbd04c194a09b40464f239c6'),),
    0x1cd17:((0x1cd17,0x1ceed,'b03aea49ff2a9dc82e23ef03cbd9283814556194d877535863e56a24e2e9be2a'),),
    0x2111a:((0x2111a,0x211a4,'8d4f2fdc84137cc9c5e5c5d4ddc941fd470b2ac7a8598dafbdb7083fbeaaf616'),),
    0x213b7:((0x213b7,0x21449,'e192c789223e56d3c902953158aeda9dc06ff4f2fbecfb02ffacbd68fdbe3256'),),
    0x21548:((0x21548,0x2185f,'e6bb94a938a4c7b107e63670836f88817f3345d554a29704e9dcd26a0dc70282'),),
    0x1f558:((0x1f558,0x1f6ef,'6703db5d5e21709e55904be7e41c75d0a1befa7782d2f318acc0489a44f83fb9'),),
    0x22046:((0x22046,0x2211c,'0fab60c0de6eebc0e5c98bfdd5b2b72d3898a5040c396fa68fb17b1079ea28e5'),
             (0x21dad,0x21db2,'d978504e1457d0725af077634a31c32f4114be767c50517de0fe054530ec7941')),
    0x21eb1:((0x21eb1,0x22046,'f21e043995cdaccc65f3ce77704db6207ce8478eff95874e8f0a0b4d29abaa70'),),
    0x24618:((0x24618,0x24754,'8eead431d886bfd59b72887b743ccd754477ee73c5082696144178b95d8dc8af'),),
}


def digest(data):return hashlib.sha256(data).hexdigest()


def validate_sources(readset,image):
    if digest(image)!=IMAGE_SHA:raise ValueError('strict relocated镜像身份不同')
    functions={}
    for function in readset.get('functions',[]):
        va=function.get('va')
        if va in functions:raise ValueError('Domain函数入口重复')
        functions[va]=function
    verified=[]
    for va,wanted in FUNCTION_CHUNKS.items():
        function=functions.get(va)
        if not function:raise ValueError('Domain完整来源函数缺失：'+hex(va))
        parts=function.get('chunks',[])
        if (function.get('end')!=wanted[0][1] or
                [(c.get('start'),c.get('end'),c.get('sha256')) for c in parts]!=list(wanted)):
            raise ValueError('Domain函数边界/chunk/hash不同：'+hex(va))
        for part in parts:
            code=bytes.fromhex(part.get('raw_hex',''));start,end=part['start'],part['end']
            if len(code)!=end-start or image[start:end]!=code or digest(code)!=part['sha256']:
                raise ValueError('Domain原字节不同：'+hex(va))
        verified.append({'va':hex(va),'name':function.get('name'),
            'chunks':[{key:value for key,value in chunk.items() if key!='raw_hex'} for chunk in parts]})
    return functions,verified


def execute(image,functions,entry,args,boundary,stubs,capture=()):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EDI
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x200000,0x10000);stack=0x208000;stop=0x400000;events=[]
    def read(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    cpu.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args))
    cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBP,0xbad0eb00);cpu.reg_write(UC_X86_REG_EDI,0xbad0ed00)
    cpu.mem_write(0x53a49,struct.pack('<I',0x100000))
    def hook(machine,address,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address in capture:events.append({'pc':hex(address),'args':[read(sp+4+4*i) for i in range(4)]})
        if address==boundary:machine.emu_stop();return
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address in stubs:
            if address==0x3706e:cpu.reg_write(UC_X86_REG_EAX,0x140000)
            cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if not entry<=address<functions[entry]['end']:raise ValueError('CPU逸出未列指令：'+hex(address))
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook);cpu.emu_start(entry,stop,count=3000)
    if cpu.reg_read(UC_X86_REG_EIP)!=boundary:raise ValueError('CPU未到已证调用边界')
    return events,{'ebp':cpu.reg_read(UC_X86_REG_EBP),'edi':cpu.reg_read(UC_X86_REG_EDI)}


def verify(readset_path,image_path):
    raw=Path(readset_path).read_bytes();image=Path(image_path).read_bytes()
    functions,verified=validate_sources(json.loads(raw),image);cases=[]
    for count in (0,1,16,96):
        actor,effect,targets=7,4,0x170000+count*16
        events,registers=execute(image,functions,0x1cac7,[actor,effect,count,targets],0x1cb8f,
            {0x1cb94,0x3706e,0x3776e,0x11eb0,0x3790a,0x11cac},(0x1cb94,))
        if [e['args'] for e in events]!=[[0x100000,count,targets,74],[0x140000,count,targets,75]]:
            raise ValueError('两次1cb94的真实栈参不同')
        if registers!={'ebp':0xbad0eb00,'edi':0xbad0ed00}:raise ValueError('poisoned入寄存器被读取/覆盖')
        callers=[]
        for caller,args in ((0x2111a,[actor,count,targets,effect]),(0x213b7,[actor,effect,count,targets])):
            delegated,_=execute(image,functions,caller,args,0x1cac7,{0x1c4cc},(0x1cac7,))
            if len(delegated)!=1 or delegated[0]['args']!=[actor,effect,count,targets]:raise ValueError('caller的四栈参不同')
            callers.append({'entry':hex(caller),'input':args,'call':delegated[0]})
        _,consumer=execute(image,functions,0x1cb94,[0x100000,count,targets,74],0x1cbad,set())
        if consumer!={'ebp':count,'edi':targets}:raise ValueError('1cb94自身count/list加载不同')
        cases.append({'count':count,'targets':targets,'render_calls':events,
            'preserved_poison_registers':registers,'callers':callers,'1cb94_loaded_registers':consumer})
    if Path(readset_path).read_bytes()!=raw or Path(image_path).read_bytes()!=image:raise ValueError('运行期间来源改变')
    return {'schema':'fd2.g4.effect-page-owner-source.v1','ok':True,
        'domain_readset':str(Path(readset_path).resolve()),'domain_readset_sha256':digest(raw),
        'image':str(Path(image_path).resolve()),'image_sha256':digest(image),'verified_functions':verified,
        'duotone_argument_cases':cases,
        'duotone_stack_instructions':[i for i in functions[0x1cac7].get('instructions',[]) if i['ea']<0x1cb0d],
        'owner_evidence_windows':{hex(va):functions[va].get('instructions',[]) for va in (0x1c2da,0x21548,0x21eb1,0x22046,0x24618)},
        'substitutions':['Watcom stack RET4','malloc returns a fixed temporary page address',
            '1cb94/1c4cc drawing calls captured or omitted','memcpy_vga, delay, free, 11cac omitted'],
        'scope':'Original calling instructions and stack delegation only; drawing, business, Windows/DOSBox and PCM equivalence are not asserted.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('readset','image','output'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    try:
        output=args.output.resolve()
        if output.exists() or output in (args.readset.resolve(),args.image.resolve()):raise ValueError('输出须新文件且不得覆盖输入')
        result=verify(args.readset,args.image);output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    except (ValueError,OSError,KeyError,TypeError) as error:
        parser.exit(1,'G4效果页来源验证未通过：'+str(error)+'。请核对只读输入。\n')
    print(json.dumps({'ok':True,'verified_functions':len(result['verified_functions']),'CPU_cases':len(result['duotone_argument_cases'])}))
    return 0


if __name__=='__main__':raise SystemExit(main())
