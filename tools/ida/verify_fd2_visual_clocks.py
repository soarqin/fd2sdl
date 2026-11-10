"""G4共享视觉clock：实际1297d与11eee首部原指令对C候选DWORD差分。

1297d完整执行；11eee只执行到12164（range门后、LUT/绘制前）。显式替身
仅Watcom stack RET4；stage0无背景。没有模拟clock公式、导入SDL冷表或
声称绘图、DOSBox实机或时钟同步等价。C JSONL仅提供输入和待比较结果。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[2]
IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTIONS={0x1297d:(0x129ec,"263adecede57e45788f1553d3ffd2619bc37d5b97746f9ef5016803e56233ee9"),
           0x11eee:(0x12263,"c5fb7419b95f9863fc66fc34f6b0311e191428b5f474dc2978e5fa8ccf0a5f95")}
GLOBALS=(0x53c0b,0x53c0f,0x53c07,0x53a40,0x53a00,0x53c1f,0x539f4)


def execute(image,row):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
    if row["kind"] not in (0,1) or not 0<=row["tick"]<65536 or len(row["initial"])!=7:
        raise ValueError("clock输入不是已证WORD/DWORD域")
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0,0x1000);cpu.mem_map(0x10000,0x54000)
    cpu.mem_write(0x10000,image[0x10000:0x64000]);cpu.mem_map(0x200000,0x10000)
    stack=0x208000;stop=0x400000;writes=[]
    def word(address):return struct.unpack("<I",cpu.mem_read(address,4))[0]
    def put(address,value):cpu.mem_write(address,struct.pack("<I",value&0xffffffff))
    for address,value in zip(GLOBALS,row["initial"]):put(address,value)
    cpu.mem_write(0x46c,struct.pack("<H",row["tick"]))
    put(0x53c03,0);put(0x51a93,row["override"])
    put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
    def hook(machine,address,size,data):
        if address==0x3702f:
            sp=machine.reg_read(UC_X86_REG_ESP)
            machine.reg_write(UC_X86_REG_EIP,word(sp))
            machine.reg_write(UC_X86_REG_ESP,sp+8)
        elif row["kind"]==1 and address==0x12164:
            machine.emu_stop()
        elif not ((0x1297d<=address<0x129ec) if row["kind"]==0 else 0x11eee<=address<0x12164):
            raise ValueError(f"clock逃出已证指令区 {address:#x}")
    def write(machine,access,address,size,value,data):
        if address in GLOBALS:writes.append({"va":hex(address),"size":size,"value":value})
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write)
    cpu.emu_start(0x11eee if row["kind"] else 0x1297d,stop,count=10000)
    reached=cpu.reg_read(UC_X86_REG_EIP)
    if reached!=(0x12164 if row["kind"] else stop):raise ValueError("未到clock完成边界")
    return [word(address) for address in GLOBALS],writes


def check(image,rows):
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError("strict relocated镜像身份不同")
    for start,(end,digest) in FUNCTIONS.items():
        if hashlib.sha256(image[start:end]).hexdigest()!=digest:
            raise ValueError(f"原函数完整字节不同 {start:#x}")
    if not rows:raise ValueError("C候选向量为空")
    cases=[]
    for row in rows:
        actual,writes=execute(image,row)
        if actual!=row["final"]:raise ValueError(f"C与原clock DWORD不同: {row}, original={actual}")
        cases.append({**row,"writes":writes})
    return {"schema":"fd2.g4.visual-clock-native.v1","ok":True,
        "case_count":len(cases),"image_sha256":IMAGE_SHA,"cases":cases,
        "scope":["complete1297d","11eee through range gate before12164"],
        "substitutions":["Watcom stack RET4"],"render_or_DOSBox_equivalence":False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,default=ROOT/"tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--c-jsonl",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    rows=[json.loads(line) for line in args.c_jsonl.read_text(encoding="utf-8-sig").splitlines() if line.strip()]
    result=check(args.image.read_bytes(),rows)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({key:result[key] for key in ("ok","case_count","scope")},ensure_ascii=False))


if __name__=="__main__":main()
