"""执行原11cac/135dd/1366a及205da前段，只核水色门和原调用参数。

固定strict relocated镜像与完整函数SHA；绘制、脚步、等待、constructor均显式
替身。此工具不导入SDL算法或冷表，不声称像素、真实时钟或DOSBox运行等价。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTIONS={
    0x11cac:(0x11d40,"037e53eb44434dd69adb6735c6e5f4e95271b34bd7617ff6c2423bca03a53a69"),
    0x135dd:(0x1366a,"60f0ad6883f634fc4d3eec83a5e32b0d2af9ed285ff0c934b3e68bfea7a158bd"),
    0x1366a:(0x1399c,"9fa0bc803cd6303e62a2c49bb86b1bfdd139affc15de18b84c21e3c141a10ddf"),
    0x205da:(0x2067d,"a3eaf64974c436cd30b00524a58760156835507d7a3dbe309081f0cb3bc22ada"),
}


def execute(image,entry,args=(),script=None,fade=0):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x10000);cpu.mem_map(0x200000,0x10000)
    stack,stop=0x208000,0x400000;calls=[];waters=[]
    def word(address):return struct.unpack("<I",cpu.mem_read(address,4))[0]
    def put(address,value):cpu.mem_write(address,struct.pack("<I",value))
    put(0x53a49,0x101000);put(0x53a45,0x102000);put(0x53afb,fade)
    put(0x53beb,1);put(0x53aa9,0);put(0x53aad,0);put(0x53ab1,0);put(0x53ab5,0)
    cpu.mem_write(0x102000,bytes([3,3])+bytes(78))
    if script is not None:cpu.mem_write(0x100000,bytes(script))
    cpu.mem_write(stack,struct.pack("<"+"I"*(len(args)+1),stop,*args))
    cpu.reg_write(UC_X86_REG_ESP,stack)
    stubs={0x1297d,0x4e31c,0x11eee,0x122dc,0x127a9,0x1acf3,0x11eb0,
        0x4e381,0x4eb48,0x17aa9,0x32230,0x11d40,0x127e0,0x129ec,0x1088d,0x37910}
    boundary=0x13181 if entry==0x135dd else 0x11cac if entry==0x205da else stop
    def hook(machine,address,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address==0x11cac and entry!=0x11cac:
            calls.append({"return_va":hex(word(sp)),"arg":word(sp+4)})
            if entry==0x205da:machine.emu_stop();return
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if address==boundary:machine.emu_stop();return
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address in stubs:
            if address==0x4e31c:waters.append(hex(word(sp)-5))
            if address==0x4eb48:cpu.reg_write(UC_X86_REG_EAX,0x100000)
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if not entry<=address<FUNCTIONS[entry][0]:raise ValueError(f"未声明调用 {address:#x}")
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    cpu.emu_start(entry,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=boundary:raise ValueError("原函数未达指定返回边界")
    return {"entry":hex(entry),"input":list(args),"fade":fade,"script":script,
        "11cac_calls":calls,"water_calls":waters,"end":hex(boundary)}


def check(image):
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError("strict镜像身份不同")
    for va,(end,digest) in FUNCTIONS.items():
        if hashlib.sha256(image[va:end]).hexdigest()!=digest:raise ValueError(f"原函数字节不同 {va:#x}")
    cases=[]
    for arg in (0,1):
        row=execute(image,0x11cac,(arg,))
        if len(row["water_calls"])!=(1 if arg==0 else 0):raise ValueError("原11cac水色门不同")
        cases.append(row)
    row=execute(image,0x135dd,(2,1))
    if [x["arg"] for x in row["11cac_calls"]]!=[0,0,0]:raise ValueError("原pan调用参数不同")
    cases.append(row)
    row=execute(image,0x205da)
    if [x["arg"] for x in row["11cac_calls"]]!=[1]:raise ValueError("原entry调用参数不同")
    cases.append(row)
    for script,fade,expected in (
        ([1,1,1,0,3],0,[0]*6+[1]),([1,1,1,0,3],1,[1]*7),
        ([1,1,1,0,3],63,[1]+[0]*5+[1]),([1,1,1,0,3],64,[0]*6+[1]),
        ([1,0x82,1,0,0],0,[0,0,1]),([1,0x80,1,0,0],0,[0,1]),
    ):
        row=execute(image,0x1366a,(0,),script,fade)
        if [x["arg"] for x in row["11cac_calls"]]!=expected:raise ValueError(f"原脚本参数序列不同 {row}")
        cases.append(row)
    return {"schema":"fd2.g4.scene-water-arguments.v1","ok":True,"cases":cases,
        "image_sha256":IMAGE_SHA,"functions":[{"va":hex(v),"end":hex(e),"sha256":h} for v,(e,h) in FUNCTIONS.items()],
        "substitutions":["Watcom RET4","all drawing/footsteps/waits/DAC leaf calls",
            "movement script getter returns explicit synthetic bytes","205da constructor and memset"],
        "scope":"原指令水色分支与11cac实参；不比较绘图/业务/宿主时钟/Windows或DOSBox运行"}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();result=check(args.image.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"ok":True,"case_count":len(result["cases"]),"output":str(args.output)},ensure_ascii=False))


if __name__=="__main__":main()
