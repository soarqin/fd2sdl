"""G4真实4e016/4e0a2与C工作页的SKIP/opaque0/隔点RUN indexed差分。

完整执行两个原叶，无绘图替身、不导入SDL解码器或颜色公式。合成24²输入
只验证这两RLE/LUT语义，不声称整地图、DOSBox窗口或最终13点截图等价。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[2]
IMAGE_SHA="8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTIONS=((0x4e016,0x4e0a2,"e8f64ea788b023e894ee89411555e03e74f497fcd486ca5e958a2ba723160c0b"),
           (0x4e0a2,0x4e127,"56ba96794467e8db8b7b7d0c1d647716a96b1d5c3c6cb4a4cbea52cf65287147"))


def execute(image,row):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
    if row["kind"] not in (0,1) or len(row["initial"])!=576 or len(row["lut"])!=256:
        raise ValueError("输入不是两个24²源叶的有效域")
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000);cpu.mem_write(0x10000,image[0x10000:0x64000])
    cpu.mem_map(0x100000,0x10000);cpu.mem_map(0x200000,0x10000)
    source,lut,dest,stack,stop=0x100000,0x101000,0x102000,0x208000,0x400000
    cpu.mem_write(source,bytes(row["rle"]));cpu.mem_write(lut,bytes(row["lut"]))
    for y in range(24):cpu.mem_write(dest+y*456,bytes(row["initial"][y*24:(y+1)*24]))
    cpu.mem_write(stack,struct.pack("<IIIII",stop,source,dest,456,lut))
    cpu.reg_write(UC_X86_REG_ESP,stack)
    start,end,_=FUNCTIONS[row["kind"]]
    def hook(machine,address,size,data):
        if not start<=address<end:raise ValueError(f"原LUT叶逃出 {address:#x}")
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    cpu.emu_start(start,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise ValueError("原LUT叶没有正常返回")
    actual=b"".join(bytes(cpu.mem_read(dest+y*456,24)) for y in range(24))
    return list(actual)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",type=Path,required=True)
    parser.add_argument("--c-jsonl",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();image=args.image.read_bytes()
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError("strict镜像身份不同")
    for start,end,digest in FUNCTIONS:
        if hashlib.sha256(image[start:end]).hexdigest()!=digest:raise ValueError("原LUT完整函数不同")
    rows=[json.loads(line) for line in args.c_jsonl.read_text(encoding="utf-8-sig").splitlines() if line.strip()]
    if len(rows)!=2 or {row["kind"] for row in rows}!={0,1}:raise ValueError("必须覆盖base及overlay两叶")
    for row in rows:
        actual=execute(image,row)
        if actual!=row["final"]:raise ValueError(f"原LUT与C页像素不同 kind={row['kind']}")
    result={"schema":"fd2.g4.page-lut-native.v1","ok":True,"case_count":len(rows),
        "pixels_compared":len(rows)*576,"image_sha256":IMAGE_SHA,
        "functions":[{"start":hex(a),"end":hex(b),"sha256":digest} for a,b,digest in FUNCTIONS],
        "substitutions":[],"SDL_window_or_DOSBox_equivalence":False}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,ensure_ascii=False))


if __name__=="__main__":main()
