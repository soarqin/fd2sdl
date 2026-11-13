"""S2：完整1f882/1f525及11d40减法DAC对真实VGA C原语，原资源只读。

原source的Watcom栈检查、outp和毫秒等待为显式替身；冷图按采得的完整
palette_subtract/delay序列展开，不用宿主brightness近似或生成器自己充当oracle。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from verify_s2_visual import machine, PALETTE, STACK, STOP

IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
SPANS = {0x1f882: ((0x1f882, 0x1f894), (0x1f503, 0x1f525)),
         0x11d40: ((0x11d40, 0x11df2),), 0x13536: ((0x13536, 0x13565),),
         0x22af6: ((0x22af6, 0x22bc6),), 0x233c6: ((0x233c6, 0x234bb),),
         0x1f525: ((0x1f525, 0x1f558),)}


def validate_source(image, readset):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA or readset.get("schema") != "fd2-s2-fade-layout-source-v1" or readset.get("image_sha256") != IMAGE_SHA:
        raise ValueError("fade原镜像/Domain来源身份不同")
    if [f["va"] for f in readset["functions"]] != list(SPANS): raise ValueError("fade完整源集合不同")
    for f in readset["functions"]:
        if tuple((c["start"], c["end"]) for c in f["chunks"]) != SPANS[f["va"]] or not f.get("blocks") or not f.get("callers"):
            raise ValueError("fade完整函数/shared chunk/CFG/xref缺失")
        for chunk in f["chunks"]:
            raw = image[chunk["start"]:chunk["end"]]
            if raw.hex() != chunk["raw_hex"] or hashlib.sha256(raw).hexdigest() != chunk["sha256"]: raise ValueError("fade原chunk不等")
        if sum(len(bytes.fromhex(i["raw_hex"])) for i in f["instructions"]) != sum(b-a for a, b in SPANS[f["va"]]): raise ValueError("fade未全读shared指令")
    tail = readset["palette_shared_return"]
    if (tail["start"], tail["end"], tail["raw_hex"]) != (0x1951f, 0x19523, "5f5e5bc3") or image[0x1951f:0x19523].hex() != tail["raw_hex"]:
        raise ValueError("11d40共享返回尾不同")


def execute(image, first=0, last=255, darkness=0, seed=17, wrapper=None):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    cpu = machine(image); base = bytes((i*17+seed) % 64 for i in range(768)); dac = bytearray([0xa5]*768)
    cpu.mem_write(PALETTE, base); cpu.mem_write(0x53a65, struct.pack("<I", PALETTE))
    args = () if wrapper else (first, last, darkness)
    cpu.mem_write(STACK, struct.pack("<"+"I"*(len(args)+1), STOP, *args)); cpu.reg_write(UC_X86_REG_ESP, STACK)
    events, visited = [], set(); port_state = [0, 0]; ranges = [span for spans in SPANS.values() for span in spans] + [(0x1951f, 0x19523)]
    def get(at): return struct.unpack("<I", cpu.mem_read(at, 4))[0]
    def ret(clean=0, value=0):
        sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+clean);cpu.reg_write(UC_X86_REG_EAX,value)
    def code(machine, address, size, data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address == 0x3702f: ret(4); return
        if address == 0x3790a:
            events.append(["delay_ms", get(sp+4)]);ret();return
        if address == 0x11d40 and wrapper: events.append(["palette_subtract", get(sp+4), get(sp+8), get(sp+12)])
        if address == 0x37ae5:
            port,value=get(sp+4),get(sp+8)
            if port == 0x3c8: port_state[:]=[value,0]
            elif port == 0x3c9:
                index,channel=port_state
                if not 0 <= index <= 255 or not 0 <= value <= 63 or channel >= 3: raise ValueError("原DAC端口值越域")
                dac[index*3+channel]=(value<<2)|(value>>4);port_state[1]+=1
            else: raise ValueError("未声明硬件端口")
            ret(value=value);return
        if not any(a<=address<b for a,b in ranges): raise ValueError(f"fade未声明原指令 {address:#x}")
        visited.add(address)
    def write(machine, access, address, size, value, data):
        if not 0x200000 <= address < 0x210000: raise ValueError("原fade写入非栈owner")
    cpu.hook_add(unicorn.UC_HOOK_CODE,code);cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write)
    cpu.emu_start(wrapper or 0x11d40,STOP,count=10000000)
    if cpu.reg_read(UC_X86_REG_EIP)!=STOP or bytes(cpu.mem_read(PALETTE,768))!=base: raise ValueError("原fade未返回/改base palette")
    return {"kind":"subtract","status":0,"dac_hex":dac.hex(),"other_unchanged":True,"events":events,"visited":sorted(visited)}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--readset',type=Path,required=True)
    p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    root=Path(__file__).resolve().parents[2];output=args.output.resolve()
    if not output.is_relative_to((root/'build').resolve()) or output.is_relative_to((root/'original_game').resolve()):
        raise ValueError("fade证据只写本地build且不能写入原资源子树")
    if args.output.exists(): raise ValueError("不覆盖旧fade收据")
    image=args.image.read_bytes();source=args.readset.read_bytes();readset=json.loads(source);validate_source(image,readset)
    vectors=[(0,255,dark,17) for dark in range(65)]+[(first,last,dark,0) for first,last in ((0,0),(255,255),(5,9),(9,5)) for dark in (0,1,31,63,64)]
    expected=[execute(image,*v) for v in vectors]
    before=hashlib.sha256(args.probe.read_bytes()).hexdigest()
    process=subprocess.run([str(args.probe.resolve()),'--s2-visual-vectors'],input='\n'.join('subtract '+' '.join(map(str,v)) for v in vectors)+'\n',capture_output=True,text=True,check=True)
    rows=[json.loads(line) for line in process.stdout.splitlines()]
    if len(rows)!=len(expected): raise ValueError("C减法向量数量不同")
    for i,(a,b) in enumerate(zip(rows,expected)):
        if any(a.get(k)!=b[k] for k in ('kind','status','dac_hex','other_unchanged')): raise ValueError(f"C/原11d40向量不同: {i}")
    fades={hex(va):execute(image,wrapper=va) for va in (0x1f882,0x1f525)}
    for va,levels in ((0x1f882,range(64)),(0x1f525,range(64,-1,-1))):
        wanted=[entry for dark in levels for entry in (["palette_subtract",0,255,dark],["delay_ms",2])]
        if fades[hex(va)]['events']!=wanted: raise ValueError("完整原fade步宽/尾等待不同")
    if args.image.read_bytes()!=image or args.readset.read_bytes()!=source or hashlib.sha256(args.probe.read_bytes()).hexdigest()!=before: raise ValueError("fade输入在验证期间变化")
    report={'schema':'fd2-s2-fade-source-C-v1','readset_sha256':hashlib.sha256(source).hexdigest(),'image_sha256':IMAGE_SHA,'C_probe_sha256':before,
        'subtract_vectors':len(vectors),'all_64_base_components_x_65_darkness_values_included':True,
        'fades':{key:{'events':v['events'],'source_instruction_count':len(v['visited'])} for key,v in fades.items()},
        'substitutes':['Watcom stack probe','outp captured DAC','delay_ms call trace, not physical clock'],
        'limits':['pureVGA C primitive and complete original fade call order','no SDL window/WGC/native timing','existing legacy multiplicative brightness/fade callers unchanged']}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'output':str(args.output),'subtract_vectors':len(vectors),'source_fades':2}))


if __name__=='__main__':main()
