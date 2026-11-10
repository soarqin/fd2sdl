"""完整原1db65小域CPU轨迹：13旋转方向写、flags提交、12消散和25wait。

使用strict relocated镜像及真实FDOTHER5目录。绘制/声音/分配/拷贝/等待均
明确替身，只核caller实参和原记录写点；不宣称这些叶的像素或DOSBox等价。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTION_SHA="03743c1db9ba91c24af9b9df114bbf110c1b67540772c7a1561bbb057af928ff"


def execute(image,ui,visible):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x100000);cpu.mem_map(0x200000,0x10000)
    page,units,temp,resource,stack,stop=0x100000,0x130000,0x140000,0x180000,0x208000,0x400000
    def word(address):return struct.unpack("<I",cpu.mem_read(address,4))[0]
    def put(address,value):cpu.mem_write(address,struct.pack("<I",value))
    records=bytearray(4*80)
    for raw,(x,y,direction,flags,hp) in enumerate(((2 if visible else 22,3,2,0,0),
            (22,31,3,0,0),(4,3,3,0x81,0),(5,3,1,0,17))):
        at=raw*80;records[at:at+2]=bytes((x,y));records[at+3]=direction;records[at+5]=flags
        struct.pack_into("<H",records,at+64,hp)
    cpu.mem_write(units,bytes(records));cpu.mem_write(resource,ui)
    for va,value in ((0x53a49,page),(0x53a45,units),(0x53beb,4),(0x53aa9,0),(0x53aad,0),
        (0x51a87,13),(0x51a8b,8),(0x53a81,resource),(0x53eec,0x190000),(0x53c0b,2),(0x53c07,3)):
        put(va,value)
    put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
    events=[];writes=[];waits=0
    stubs={0x129ec,0x11eb0,0x17aa9,0x11eee,0x127e0,0x3706e,0x127a9,0x25a96,
        0x4ebab,0x3771c,0x3776e,0x11cac}
    nargs={0x11eee:6,0x11eb0:6,0x17aa9:1,0x127e0:1,0x3706e:1,0x25a96:3,
        0x4ebab:3,0x3771c:3,0x3776e:1,0x11cac:1,0x129ec:0,0x127a9:0}
    def code(machine,address,size,data):
        nonlocal waits
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address in stubs:
            args=[word(sp+4+4*i) for i in range(nargs[address])]
            events.append({"call":hex(address),"from":hex(word(sp)-5),"after_waits":waits,
                "page":word(0x53a49),"args":args})
            if address==0x17aa9:waits+=1
            if address==0x3706e:cpu.reg_write(UC_X86_REG_EAX,temp)
            if address==0x3771c:cpu.reg_write(UC_X86_REG_EAX,args[0])
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if not 0x1db65<=address<0x1debe:raise ValueError(f"未声明原调用 {address:#x}")
    def write(machine,access,address,size,value,data):
        if units<=address<units+320:
            writes.append({"raw":(address-units)//80,"offset":(address-units)%80,
                "size":size,"value":value,"after_waits":waits,"pc":hex(cpu.reg_read(UC_X86_REG_EIP))})
    cpu.hook_add(unicorn.UC_HOOK_CODE,code);cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write)
    cpu.emu_start(0x1db65,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError("原1db65未正常返回")
    final=bytes(cpu.mem_read(units,320))
    directions=[w for w in writes if w["offset"]==3];flags=[w for w in writes if w["offset"]==5]
    if any(w["offset"] not in (3,5) or w["size"]!=1 for w in writes):raise ValueError("原helper有额外单位写")
    if [w["raw"] for w in flags]!=[0,1,2] or any(w["value"]!=1 for w in flags):raise ValueError("原flags不是全部HP0精确1")
    if visible:
        if [(w["raw"],w["value"],w["after_waits"]) for w in directions]!=[(raw,j%4,j) for j in range(13) for raw in (0,1)]:
            raise ValueError("原13轮逐raw方向写不同")
        if any(w["after_waits"]!=13 for w in flags):raise ValueError("原flags提交点不同")
        if [e["args"] for e in events if e["call"]=="0x17aa9"]!=[[1]]*25:raise ValueError("原25wait不同")
        blits=[e for e in events if e["call"]=="0x4ebab"]
        if len(blits)!=12:raise ValueError("原消散不是12帧")
        for i,event in enumerate(blits):
            frame=resource+struct.unpack_from("<I",ui,6+4*(68+i))[0]
            if event["args"]!=[page+114*456+96,frame,456] or event["after_waits"]!=13+i:
                raise ValueError("原消散锚/帧/时序不同")
        restores=[e for e in events if e["call"]=="0x3771c"]
        if [(e["after_waits"],e["args"]) for e in restores]!=[(j,[page,temp,153216]) for j in range(19,25)]:
            raise ValueError("原前6累积/后6恢复不同")
        if [e["args"] for e in events if e["call"]=="0x11cac"]!=[[0]]:raise ValueError("原尾11cac0不同")
        if [e["after_waits"] for e in events if e["call"]=="0x25a96"]!=[13]:raise ValueError("原cue点不同")
        if [e["args"] for e in events if e["call"]=="0x25a96"]!=[[0x190000,3,1]]:raise ValueError("原cue不同")
    elif events or directions or any(w["after_waits"] for w in flags):raise ValueError("无见证原路径有额外效果")
    if word(0x53a49)!=page or word(0x53c0b)!=2 or word(0x53c07)!=3:raise ValueError("原App/idle/cursor被替换")
    return {"visible":visible,"events":events,"record_writes":writes,"waits":waits,
        "initial_records_hex":records.hex(),"final_records_hex":final.hex()}


def check(image,fdother):
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError("strict relocated镜像身份不同")
    if hashlib.sha256(image[0x1db65:0x1debe]).hexdigest()!=FUNCTION_SHA:raise ValueError("原完整1db65字节不同")
    if fdother[:6]!=b"LLLLLL":raise ValueError("FDOTHER容器不同")
    a,b=struct.unpack_from("<II",fdother,6+5*4);ui=fdother[a:b]
    if ui[:4]!=b"LMI1" or len(ui)<326 or struct.unpack_from("<H",ui,4)[0]<80:raise ValueError("原UI5目录不同")
    return {"schema":"fd2.g4.defeat-original-cpu.v1","ok":True,
        "image_sha256":IMAGE_SHA,"function_va":"0x1db65","function_end":"0x1debe","function_sha256":FUNCTION_SHA,
        "address_view":"canonical relbase, loader-relocated; not raw-unrelocated FD2.EXE file window",
        "fdother_sha256":hashlib.sha256(fdother).hexdigest(),"entry5_sha256":hashlib.sha256(ui).hexdigest(),
        "cases":[execute(image,ui,True),execute(image,ui,False)],
        "substitutions":["Watcom RET4","malloc/free/memmove, tiles/actors/FG/blits/project, PCM and BIOS waits"],
        "scope":"完整原caller控制流与单位方向/flags实写。无RNG/HP写，无像素/PCM/真实时钟或原版窗口等价声明。"}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,required=True)
    parser.add_argument("--fdother",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();result=check(args.image.read_bytes(),args.fdother.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"ok":True,"cases":len(result["cases"]),"waits":[x["waits"] for x in result["cases"]]},ensure_ascii=False))


if __name__=="__main__":main()
