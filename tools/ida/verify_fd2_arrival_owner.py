"""原32999完整指令的12帧、App写点与6/7/8重建参数取证。

显式替身资源加载/append/分配/拷贝/绘制/等待，仅记录原caller给出的实参。
不导入SDL，不声称所替身像素、完整constructor或真实声音/窗口等价。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTION_SHA="15a88f6d53a73f1644f046cdef06a09247cabd207f48638eebb93f180e2b69a3"


def check(image,fdother):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError("strict镜像身份不同")
    if hashlib.sha256(image[0x32999:0x32d18]).hexdigest()!=FUNCTION_SHA:raise ValueError("原32999完整字节不同")
    if fdother[:6]!=b"LLLLLL":raise ValueError("FDOTHER容器不同")
    a,b=struct.unpack_from("<II",fdother,6+9*4)
    if not 6+11*4<=a<b<=len(fdother):raise ValueError("FDOTHER9边界不同")
    lmi=fdother[a:b]
    if lmi[:4]!=b"LMI1" or len(lmi)<54 or struct.unpack_from("<H",lmi,4)[0]<12:raise ValueError("到达LMI目录不同")
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x100000);cpu.mem_map(0x200000,0x10000)
    page,temporary,units,resource,stack,stop=0x101000,0x140000,0x102000,0x180000,0x208000,0x400000
    cpu.mem_write(resource,lmi)
    def word(address):return struct.unpack("<I",cpu.mem_read(address,4))[0]
    def put(address,value):cpu.mem_write(address,struct.pack("<I",value))
    for va,value in [(0x53a49,page),(0x53a45,units),(0x53beb,1),(0x53aa9,0),(0x53aad,0),
        (0x51a87,13),(0x51a8b,8),(0x53c0b,2),(0x53c07,3)]:put(va,value)
    cpu.mem_write(units,bytes([2,3])+bytes(78)+bytes([5,3,1])+bytes(77))
    cpu.mem_write(stack,struct.pack("<II",stop,1));cpu.reg_write(UC_X86_REG_ESP,stack)
    events=[];projections=[]
    stubs={0x111ba,0x3706e,0x3771c,0x3776e,0x25a96,0x11eb0,0x11eee,
        0x127e0,0x129ec,0x127a9,0x10b4e,0x4e381,0x17aa9,0x4ebab}
    def hook(machine,address,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        def args(count):return [word(sp+4+4*i) for i in range(count)]
        if address==0x3702f:
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8);return
        if address in stubs:
            event={"call":hex(address),"frame_after_project":len(projections)-1,"page":word(0x53a49)}
            if address==0x111ba:
                event["args"]=args(3);cpu.reg_write(UC_X86_REG_EAX,resource if event["args"][2]==9 else 0x190000)
            elif address==0x3706e:cpu.reg_write(UC_X86_REG_EAX,temporary)
            elif address==0x10b4e:put(0x53beb,2)
            elif address==0x3771c:
                event["args"]=args(3);cpu.reg_write(UC_X86_REG_EAX,event["args"][0])
            elif address==0x4ebab:event["args"]=args(3)
            elif address==0x11eb0:
                event["args"]=args(6);projections.append(event)
            elif address==0x127e0:event["args"]=args(1)
            elif address==0x17aa9:event["args"]=args(1)
            events.append(event)
            cpu.reg_write(UC_X86_REG_EIP,word(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if not 0x32999<=address<0x32d18:raise ValueError(f"原32999未声明调用 {address:#x}")
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook);cpu.emu_start(0x32999,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError("原32999未正常返回")
    if len(projections)!=12:raise ValueError("原投影数不是12")
    if any(e["args"]!=[0xa0504,320,page+32904,456,312,192] for e in projections):raise ValueError("原投影参数不同")
    stamps=[e for e in events if e["call"]=="0x4ebab"]
    for i,e in enumerate(stamps):
        frame=resource+struct.unpack_from("<I",lmi,6+4*i)[0]
        if e["args"]!=[page+32904+24*(5-1)+10944*3-2736,frame,456]:raise ValueError("原LMI定位不同")
    if len(stamps)!=12:raise ValueError("原LMI帧数不同")
    actor_calls=[(e["frame_after_project"],e["args"][0],e["page"]) for e in events if e["call"]=="0x127e0"]
    if actor_calls!=[(6,0,page),(6,1,page-3648),(7,0,page),(7,1,page-2280)]:raise ValueError("原6/7 actor写点不同")
    if [e["frame_after_project"] for e in events if e["call"]=="0x11eee"]!=[7,8]:raise ValueError("原terrain写点不同")
    if [e["frame_after_project"] for e in events if e["call"]=="0x127a9"]!=[8]:raise ValueError("原正常actor写点不同")
    if [e["args"] for e in events if e["call"]=="0x17aa9"]!=[[1]]*12:raise ValueError("原等待不同")
    if word(0x53a49)!=page or word(0x53c0b)!=2 or word(0x53c07)!=3:raise ValueError("原owner或相位发生额外变化")
    return {"schema":"fd2.g4.arrival-owner-arguments.v1","ok":True,"image_sha256":IMAGE_SHA,
        "function":{"va":"0x32999","end":"0x32d18","sha256":FUNCTION_SHA},
        "fdother_sha256":hashlib.sha256(fdother).hexdigest(),"entry9_sha256":hashlib.sha256(lmi).hexdigest(),
        "project_count":len(projections),"events":events,"final_page":page,
        "substitutions":["Watcom RET4","res_load uses real FDOTHER9, synthetic PCM handle",
            "group append only raises count1 to2; explicit synthetic actors","malloc/free, memmove, all blits, PCM and waits"],
        "scope":"原caller完整控制流/原实参/owner重定向；无1297d/水色/尾11cac。替身不验证像素或业务/运行。"}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,required=True)
    parser.add_argument("--fdother",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();result=check(args.image.read_bytes(),args.fdother.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"ok":True,"project_count":result["project_count"],"output":str(args.output)},ensure_ascii=False))


if __name__=="__main__":main()
