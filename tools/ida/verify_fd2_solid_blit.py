#!/usr/bin/env python3
"""原4e127完整机器码的stride/argC/RLE写点差分；无patch、无GUI。

使用已经Domain审阅的完整127B，从strict LE页独立取原码。Unicorn仅提供
caller栈和数据，不hook指令、不替换callee；与独立RLE写点模型逐字节比较。
此门不代替SDL真实像素回归或原版WGC，不宣称所有调用者的业务时序。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"static_data"))
from verify_unit_tables import source_range,EXE_SHA256


def fixtures():
    rows={"repeat":bytes((23,0)),"literal":bytes((0x97,))+bytes(range(24)),
        "alternate":bytes((0x4b,37)),"skip":bytes((0xd7,)),
        "mixed":bytes((5,0,0xc2,0x43,27,0x86,0,1,2,0,4,5,0))}
    return {name:row*24 for name,row in rows.items()}


def expected(payload,stride):
    result=bytearray((109,))*(32+stride*24+32)
    writes=set();at=0
    for y in range(24):
        x=0
        while x<24:
            control=payload[at];at+=1
            mode,count=control&192,(control&63)+1
            if mode==192:
                x+=count
                continue
            if mode==128:
                at+=count;columns=range(x,x+count);x+=count
            elif mode==64:
                at+=1;columns=range(x+1,x+2*count,2);x+=2*count
            else:
                at+=1;columns=range(x,x+count);x+=count
            for column in columns:
                offset=32+y*stride+column
                result[offset]=stride&255;writes.add(offset)
        if x!=24:
            raise ValueError("局部RLE夹具横向越界")
    if at!=len(payload):
        raise ValueError("局部RLE夹具未完整消费")
    return bytes(result),writes


def verify(executable):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_ESP
    data=Path(executable).read_bytes();code=source_range(data,0x4e127,0x7f)
    if source_range(data,0x4e133,13)!=bytes.fromhex("8b551083ea1833c98b45108ae0"):
        raise ValueError("4e127完整读集内的stride/AH指令不同")
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x4e000,0x1000);cpu.mem_write(0x4e127,code)
    cpu.mem_map(0x100000,0x1000);cpu.mem_map(0x200000,0x10000)
    cpu.mem_map(0x300000,0x10000);cpu.mem_map(0x400000,0x1000)
    stack,source,destination,stop=0x308000,0x100000,0x200000,0x400000
    observed=set()
    def writes(machine,access,address,size,value,userdata):
        if 0x300000<=address and address+size<=0x310000:
            return
        if not destination<=address or address+size>destination+0x10000:
            raise RuntimeError("原callee写出声明的destination/stack")
        observed.update(range(address-destination,address-destination+size))
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,writes)
    count=0;digest=hashlib.sha256();by_stride={}
    for name,payload in fixtures().items():
        for stride in (24,25,256,320,456,512):
            wanted,write_points=expected(payload,stride)
            first=None
            for color in (0,13,146,200,255,0x12345678):
                cpu.mem_write(source,payload);cpu.mem_write(destination,bytes((109,))*len(wanted))
                cpu.mem_write(stack,struct.pack("<5I",stop,source,destination+32,stride,color))
                cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBP,0x1234)
                observed.clear();cpu.emu_start(0x4e127,stop,count=50000)
                actual=bytes(cpu.mem_read(destination,len(wanted)))
                if (cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+4 or
                        cpu.reg_read(UC_X86_REG_EBP)!=0x1234 or actual!=wanted or observed!=write_points):
                    raise RuntimeError(f"原callee输出/写点/返回边界不同：{name}/{stride}/{color}")
                if first is not None and actual!=first:
                    raise RuntimeError("仅arg_C不同却改变输出")
                first=actual;digest.update(struct.pack("<II",stride,color)+name.encode()+actual);count+=1
            by_stride[str(stride)]=stride&255
    if Path(executable).read_bytes()!=data:
        raise ValueError("执行期间EXE来源变化")
    return {"ok":True,"vectors":count,"rle_modes":["repeat","literal including opaque0","alternate odd writes","skip","mixed"],
        "strides_to_actual_color":by_stride,"arg_C_values":[0,13,146,200,255,0x12345678],
        "arg_C_changes_output":False,"all_destination_guard_and_padding_bytes_checked":True,
        "all_actual_write_addresses_checked":True,"callee_code_patched":False,"instruction_hooks_or_stubs":False,
        "executable_sha256":EXE_SHA256,"canonical_va":"0x4e127","function_bytes":len(code),
        "function_sha256":hashlib.sha256(code).hexdigest(),"vectors_sha256":digest.hexdigest(),
        "unicorn_version":unicorn.__version__,"native_WGC_acceptance":False,"real_C_pixels_compared":False,
        "scope":"entire original4e127 against independent RLE write model; map caller456 -> actual200"}


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable",type=Path)
    parser.add_argument("--json",type=Path)
    args=parser.parse_args();report=verify(args.executable)
    if args.json:
        output=args.json.resolve();original=args.executable.resolve().parent
        if output.exists() or output==original or output.is_relative_to(original):
            parser.error("结果文件须在原版目录外且尚不存在")
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(report,ensure_ascii=False))
