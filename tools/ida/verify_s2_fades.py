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
TRANSITION = (0x24618, 0x24754, "8eead431d886bfd59b72887b743ccd754477ee73c5082696144178b95d8dc8af")
TRANSITION_ENTRY, TRANSITION_STOP = 0x24717, 0x2474a
ADDITIVE = (0x11df2, 0x11eb0, "60a949e31a64bec1dc8eed0c8a16fe7a152b208f65de037bb4407a58c8ee0759")


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


def validate_transition_source(image, readset):
    """完整24618 owner先独立核身份；只执行其中已解的调色尾，非整场FX。"""
    start, end, expected = TRANSITION
    rows=[row for row in readset.get("functions", []) if row.get("va")==start]
    if len(rows)!=1 or hashlib.sha256(image).hexdigest()!=IMAGE_SHA:
        raise ValueError("transition完整owner/镜像不同")
    row=rows[0]; chunks=row.get("chunks", [])
    if (row.get("end")!=end or len(chunks)!=1 or
            (chunks[0].get("start"),chunks[0].get("end"))!=(start,end) or
            not row.get("blocks") or not row.get("callers") or
            not {0x11df2,0x3790a}<=set(row.get("callees", []))):
        raise ValueError("transition完整CFG/chunk/xref缺失")
    raw=image[start:end]
    if chunks[0].get("raw_hex")!=raw.hex() or chunks[0].get("sha256")!=expected or hashlib.sha256(raw).hexdigest()!=expected:
        raise ValueError("transition完整owner原字节不同")
    addresses={instruction.get("ea") for instruction in row.get("instructions", [])}
    if not {start,TRANSITION_ENTRY,TRANSITION_STOP}<=addresses:
        raise ValueError("transition尾入口/停止点没有完整指令来源")
    a,b,expected=ADDITIVE
    if hashlib.sha256(image[a:b]).hexdigest()!=expected:
        raise ValueError("transition实际11df2来源不同")


def execute(image, first=0, last=255, darkness=0, seed=17, wrapper=None, *, transition=False):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    cpu = machine(image); base = bytes((i*17+seed) % 64 for i in range(768)); dac = bytearray([0xa5]*768)
    cpu.mem_write(PALETTE, base); cpu.mem_write(0x53a65, struct.pack("<I", PALETTE))
    args = () if wrapper or transition else (first, last, darkness)
    cpu.mem_write(STACK, struct.pack("<"+"I"*(len(args)+1), STOP, *args)); cpu.reg_write(UC_X86_REG_ESP, STACK)
    events, visited, trace = [], set(), []; pending = []
    port_state = [0, 0]; ranges = [span for spans in SPANS.values() for span in spans] + [(0x1951f, 0x19523)]
    if transition:ranges.extend([(TRANSITION[0],TRANSITION[1]),ADDITIVE[:2]])
    def get(at): return struct.unpack("<I", cpu.mem_read(at, 4))[0]
    def ret(clean=0, value=0):
        sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+clean);cpu.reg_write(UC_X86_REG_EAX,value)
    def code(machine, address, size, data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if transition and address==TRANSITION_STOP:machine.emu_stop();return
        if address == 0x3702f: ret(4); return
        if address == 0x3790a:
            if pending:
                op,values=pending.pop()
                trace.append({"op":op,"args":values,"status":0,"dac_hex":dac.hex()})
            trace.append({"op":"delay_ms","args":[get(sp+4)],"status":0,"dac_hex":dac.hex()})
            events.append(["delay_ms", get(sp+4)]);ret();return
        if address in (0x11d40,0x11df2) and (wrapper or transition):
            if pending:raise ValueError("原调色调用未完成便开始下一次")
            op="palette_subtract" if address==0x11d40 else "palette_add"
            values=[get(sp+4),get(sp+8),get(sp+12)]
            events.append([op,*values]);pending.append((op,values))
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
    cpu.emu_start(TRANSITION_ENTRY if transition else wrapper or 0x11d40,STOP,count=10000000)
    if cpu.reg_read(UC_X86_REG_EIP)!=(TRANSITION_STOP if transition else STOP) or pending or bytes(cpu.mem_read(PALETTE,768))!=base:
        raise ValueError("原fade未到指定尾/改base palette")
    return {"kind":"subtract","status":0,"dac_hex":dac.hex(),"other_unchanged":True,"events":events,"trace":trace,"visited":sorted(visited)}


def compare_probe(probe, lines, expected, option):
    before=hashlib.sha256(probe.read_bytes()).hexdigest()
    process=subprocess.run([str(probe.resolve()),option],input='\n'.join(lines)+'\n',capture_output=True,text=True,check=True)
    rows=[json.loads(line) for line in process.stdout.splitlines()]
    if len(rows)!=len(expected):raise ValueError("C调色向量数量不同")
    for i,(actual,wanted) in enumerate(zip(rows,expected)):
        # 严格JSON类型；True不能被整数1冒充，缺尾、额外事件和未知字段均拒绝。
        if json.dumps(actual,sort_keys=True,separators=(',',':'))!=json.dumps(wanted,sort_keys=True,separators=(',',':')):
            raise ValueError(f"C/原调色轨迹或owner不同: {i}")
    after=hashlib.sha256(probe.read_bytes()).hexdigest()
    if before!=after:raise ValueError("C调色探针在比较期间变化")
    return {"before_sha256":before,"after_sha256":after,"vectors":len(rows)}


def scene_expected(kind, fast, seed, trace):
    if kind not in ('public_in','public_out','legacy_in','legacy_entry','gameover_in','gameover_out','source_in','source_out','transition'):
        raise ValueError("未声明Scene调色caller")
    if fast not in (0,1) or kind=='transition' and fast:raise ValueError("未声明Scene fast域")
    operations=sum(row['op']!='delay_ms' for row in trace)
    return {'kind':kind,'fast':fast,'seed':seed,'status':0,
        'uploads':operations if not fast or kind.startswith('source_') else 0,
        'presents':1 if fast and kind=='legacy_in' else 0,'owner_unchanged':True,
        'trace':[row for row in trace if not fast or row['op']!='delay_ms']}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--readset',type=Path,required=True)
    p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--scene-probe',type=Path);p.add_argument('--transition-readset',type=Path);args=p.parse_args()
    if bool(args.scene_probe)!=bool(args.transition_readset):p.error("--scene-probe与--transition-readset须同时提供")
    root=Path(__file__).resolve().parents[2];output=args.output.resolve()
    if not output.is_relative_to((root/'build').resolve()) or output.is_relative_to((root/'original_game').resolve()):
        raise ValueError("fade证据只写本地build且不能写入原资源子树")
    if args.output.exists(): raise ValueError("不覆盖旧fade收据")
    image=args.image.read_bytes();source=args.readset.read_bytes();readset=json.loads(source);validate_source(image,readset)
    vectors=[(0,255,dark,17) for dark in range(65)]+[(first,last,dark,0) for first,last in ((0,0),(255,255),(5,9),(9,5)) for dark in (0,1,31,63,64)]
    expected=[execute(image,*v) for v in vectors]
    before=hashlib.sha256(args.probe.read_bytes()).hexdigest()
    compare_probe(args.probe,['subtract '+' '.join(map(str,v)) for v in vectors],
        [{k:row[k] for k in ('kind','status','dac_hex','other_unchanged')} for row in expected],'--s2-visual-vectors')
    setter_values=[*range(65),-1,65,255]
    setter_expected=[execute(image,darkness=max(0,min(64,value))) for value in setter_values]
    compare_probe(args.probe,[f'brightness 0 255 {value} 17' for value in setter_values],
        [{**{k:row[k] for k in ('status','dac_hex','other_unchanged')},'kind':'brightness'} for row in setter_expected],'--s2-visual-vectors')
    fades={hex(va):execute(image,wrapper=va) for va in (0x1f882,0x1f525)}
    for va,levels in ((0x1f882,range(64)),(0x1f525,range(64,-1,-1))):
        wanted=[entry for dark in levels for entry in (["palette_subtract",0,255,dark],["delay_ms",2])]
        if fades[hex(va)]['events']!=wanted: raise ValueError("完整原fade步宽/尾等待不同")
    scene_report=None;transition_bytes=None
    if args.scene_probe:
        transition_bytes=args.transition_readset.read_bytes();validate_transition_source(image,json.loads(transition_bytes))
        lines=[];wanted=[];source_runs=[]
        for seed in (0,17,63):
            source_in=execute(image,seed=seed,wrapper=0x1f525)
            source_out=execute(image,seed=seed,wrapper=0x1f882)
            transition=execute(image,seed=seed,transition=True)
            if transition['events']!=[['delay_ms',500]]+[event for value in range(0,64,2) for event in
                    (['palette_add',0,255,value],['delay_ms',4])]:raise ValueError("原24618调色尾步序不同")
            source_runs.append({'seed':seed,'in_steps':len(source_in['trace']),'out_steps':len(source_out['trace']),
                'transition_steps':len(transition['trace']),'transition_source_instructions':len(transition['visited'])})
            for kind in ('public_in','public_out','legacy_in','legacy_entry','gameover_in','gameover_out','source_in','source_out'):
                original=source_out if kind.endswith('_out') else source_in
                for fast in (0,1):
                    lines.append(f'{kind} {fast} {seed}');wanted.append(scene_expected(kind,fast,seed,original['trace']))
            lines.append(f'transition 0 {seed}');wanted.append(scene_expected('transition',0,seed,transition['trace']))
        scene_report=compare_probe(args.scene_probe,lines,wanted,'--s4-palette-vectors')
        scene_report.update({'source_runs':source_runs,'transition_readset_sha256':hashlib.sha256(transition_bytes).hexdigest(),
            'transition_scope':'complete24618 owner verified; CPU executes only24717..2474a palette tail, before1d4cb; no preceding layout/FX or suffix execution'})
    if args.image.read_bytes()!=image or args.readset.read_bytes()!=source or hashlib.sha256(args.probe.read_bytes()).hexdigest()!=before: raise ValueError("fade输入在验证期间变化")
    if transition_bytes is not None and args.transition_readset.read_bytes()!=transition_bytes:raise ValueError("transition读集在验证期间变化")
    report={'schema':'fd2-s2-fade-source-C-v1','readset_sha256':hashlib.sha256(source).hexdigest(),'image_sha256':IMAGE_SHA,'C_probe_sha256':before,
        'subtract_vectors':len(vectors),'all_64_base_components_x_65_darkness_values_included':True,
        'compatibility_setter_vectors':len(setter_values),'setter_clamp_not_original_negative_ABI':True,'scene_consumer':scene_report,
        'fades':{key:{'events':v['events'],'source_instruction_count':len(v['visited'])} for key,v in fades.items()},
        'substitutes':['Watcom stack probe','outp captured DAC','delay_ms call trace, not physical clock'],
        'limits':['pureVGA C primitive and complete original fade call order','no SDL window/WGC/native timing',
            'optional private Scene TU uses actual dispatcher/helper, with delay and upload as explicit I/O substitutes; no formal App/native claim']}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'output':str(args.output),'subtract_vectors':len(vectors),'source_fades':2}))


if __name__=='__main__':main()
