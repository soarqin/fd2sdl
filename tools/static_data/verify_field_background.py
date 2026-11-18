"""10652原完整机器码执行的资源调用投影，不以SDL表或伪C作预期。

只执行dispatcher；malloc/free/res_load/blit/24d22为显式记录替身，故本工具
不证明RLE绘制、滚动像素、SDL C逐写、原版GUI或声音。wave/初值另读原字节。
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from verify_unit_tables import EXE_SHA256, ROOT, source_range
from table_schema import TABLE_SCHEMA_VERSION, load_table_document

IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTION_SHA="cd800188db185e379bbf2b47c33e028ade7a42a70d7fcde7d2e4685a11113282"


def original_dispatch(image,stage):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x100000);cpu.mem_map(0x200000,0x10000)
    stack=0x208000;stop=0x400000;heap=0x100000;calls=[]
    def word(address):return struct.unpack("<I",cpu.mem_read(address,4))[0]
    def put(address,value):cpu.mem_write(address,struct.pack("<I",value))
    def ret(value=0,extra=0):
        sp=cpu.reg_read(UC_X86_REG_ESP);target=word(sp)
        cpu.reg_write(UC_X86_REG_EAX,value)
        cpu.reg_write(UC_X86_REG_ESP,sp+4+extra);cpu.reg_write(UC_X86_REG_EIP,target)
    stubs={0x3706e:("malloc",1),0x3776e:("free",1),0x111ba:("resource",3),
           0x4e98d:("blit",6),0x24d22:("scroll",1)}
    def hook(machine,address,size,data):
        nonlocal heap
        if address==0x3702f:ret(extra=4);return
        if address in stubs:
            name,count=stubs[address];sp=cpu.reg_read(UC_X86_REG_ESP)
            args=[word(sp+4+4*i) for i in range(count)]
            row={"operation":name,"args":args};calls.append(row)
            if name in ("malloc","resource"):
                pointer=heap;heap+=0x20000;row["result"]=pointer;ret(pointer)
            else:ret()
            return
        if not 0x10652<=address<0x1088d:
            raise ValueError(f"10652执行逃出已证函数/替身 {address:#x}")
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    put(0x53c03,stage);put(0x53aff,0x1f0000);put(0x53b03,0x1e0000)
    put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
    cpu.emu_start(0x10652,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+4:
        raise ValueError("10652未正常cdecl返回")
    if calls[:2]!=[{"operation":"free","args":[0x1f0000]},
                   {"operation":"free","args":[0x1e0000]}]:
        raise ValueError("10652未先释放原两资源")
    body=calls[2:]
    resources=[row["args"][2] for row in body if row["operation"]=="resource"]
    blits=[row["args"] for row in body if row["operation"]=="blit"]
    mallocs=[row["args"][0] for row in body if row["operation"]=="malloc"]
    scrolls=[row for row in body if row["operation"]=="scroll"]
    if not body:config=[0,0,0,0]
    elif len(resources)==1 and not blits and mallocs==[64000] and not scrolls:
        config=[1,resources[0],320,200]
    elif len(resources)==2 and len(blits)==2 and len(mallocs)==1 and not scrolls:
        width=blits[0][4];height=mallocs[0]//width
        if (mallocs[0]!=width*height or height%2 or resources[1]!=resources[0]+1 or
            blits[0][1:3]!=[0,0] or blits[1][1:3]!=[0,height//2] or
            blits[1][4]!=width or blits[0][5]!=0xffffffff or blits[1][5]!=0xffffffff):
            raise ValueError("原双图调用几何不同")
        config=[2,resources[0],width,height]
    elif len(resources)==1 and len(blits)==1 and mallocs==[59904] and len(scrolls)==1:
        if blits[0][1:3]!=[0,0] or blits[0][4:]!=[312,0xffffffff] or scrolls[0]["args"]!=[0]:
            raise ValueError("原滚动背景初始化几何不同")
        config=[3,resources[0],312,192]
    else:raise ValueError("10652调用轨迹不是已解码的有限背景族")
    return config,calls


def check(document,executable,image):
    if type(document.get("schema_version")) is not int or document["schema_version"] != TABLE_SCHEMA_VERSION:
        raise ValueError("背景冷表版本不符")
    if hashlib.sha256(executable).hexdigest()!=EXE_SHA256 or hashlib.sha256(image).hexdigest()!=IMAGE_SHA:
        raise ValueError("原EXE/strict relocated镜像身份不同")
    if hashlib.sha256(image[0x10652:0x1088d]).hexdigest()!=FUNCTION_SHA:
        raise ValueError("10652完整571B与G1 Domain闭包不同")
    tables={table["name"]:table for table in document["tables"]}
    names={"k_field_background_config","k_field_background_wave","k_field_background_scroll_initial"}
    if len(document["tables"])!=3 or set(tables)!=names:raise ValueError("背景冷表集合不同")
    traces=[original_dispatch(image,stage) for stage in range(33)]
    config=tables["k_field_background_config"]
    if (config.get("c_type"),config.get("dimensions"),config.get("values"))!=(
            "uint16_t",[33,4],[trace[0] for trace in traces]):
        raise ValueError("背景dispatch投影与原10652实际调用不同")
    for name,va,length in (("k_field_background_wave",0x627c8,16),
                           ("k_field_background_scroll_initial",0x51a10,1)):
        table=tables[name];raw=source_range(executable,va,length)
        if (table.get("c_type"),table.get("dimensions"),table.get("values"))!=("uint8_t",[length],list(raw)):
            raise ValueError("背景原byte不同")
    return {"ok":True,"schema":"fd2.g4.background-source.v1",
            "original_dispatch_cases":33,"original_function_sha256":FUNCTION_SHA,
            "traces":[{"stage":stage,"config":trace[0],"calls":trace[1]} for stage,trace in enumerate(traces)],
            "explicit_substitutions":["Watcom stack RET4","malloc/free pointer allocation only",
                "res_load identity only","blit_image_clipped records only","24d22 records only"],
            "wave_and_initial_byte_verified":True,"SDL_or_DOSBox_or_pixel_equivalence":False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--document",type=Path,default=ROOT/"data/static/tables/field_background.json")
    parser.add_argument("--executable",type=Path,default=ROOT/"original_game/FD2.EXE")
    parser.add_argument("--image",type=Path,default=ROOT/"tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    report=check(load_table_document(args.document, "field_background"),args.executable.read_bytes(),args.image.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print(json.dumps({"ok":report["ok"],"original_dispatch_cases":report["original_dispatch_cases"]}))


if __name__=="__main__":main()
