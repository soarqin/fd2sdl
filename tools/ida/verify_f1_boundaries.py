"""F1：原 turn255 分派前缀与 80B 构造写掩码的有界原码验证。

依据 1a813、10c50/112a5 完整原指令和独立 strict relocated 镜像。
仅 Watcom 栈探测、libc、地图标记及图形 cache 为显式替身；属性重算／
原 getter 实际执行。此工具不签正常整局可达性、GUI 或未知字节全读者。
输出仅为本地证据，不提交原字节／状态。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
from audit_stage_data import archive_entries, SOURCE_HASHES

IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
DISPATCH_SCOPE = {"result": (0x51b19,30), "callback": (0x51b91,90),
                  "magic": (0x51d01,28), "entry": (0x51d71,30), "victory": (0x51de9,30)}
# 手工核全 owner xref/参数外传者后冻结，不能由生成图或“未搜到”证明。
REVIEWED_OWNER_SHA = {0x53a51:"346c244212547d569467baa43b93c05859b99cb5dd40af189ae5c8a0da64337a",
                      0x53a55:"342765fb8150754b588f2f070d6c1f08a47879a8d34b6765b37603d5b38e5795"}
MAP_PARAMETER_WRITERS = (0x4df4c,0x4e390,0x4e42c,0x4e4be,0x4e4f6,
                         0x4e5cc,0x4e680,0x4e703,0x4e71f,0x4e751)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def digest_json(value):
    return digest(json.dumps(value,sort_keys=True,separators=(",",":")).encode())


def verify_dispatch_scope(tables):
    if len(tables)!=len(DISPATCH_SCOPE) or {t["name"] for t in tables}!=set(DISPATCH_SCOPE):
        raise ValueError("F1 dispatch scope missing/unsupported/duplicate table")
    for table in tables:
        if ((table["va"],table["count"])!=DISPATCH_SCOPE[table["name"]] or
                len(table["targets"])!=table["count"]):
            raise ValueError("F1 dispatch count/address differs from independent five-table scope")


def verify_function_bytes(function,image):
    for chunk in function["chunks"]:
        raw=bytes.fromhex(chunk["bytes"])
        if (not 0<=chunk["start"]<chunk["end"]<=len(image) or
                raw!=image[chunk["start"]:chunk["end"]] or digest(raw)!=chunk["sha256"]):
            raise ValueError("F1 Domain/strict original byte mismatch")


def verify_dispatch_bytes(table,image):
    va,count=table["va"],table["count"]
    if not 0<=va<va+count*4<=len(image):
        raise ValueError("F1 dispatch byte span outside independent source")
    raw=image[va:va+count*4]
    if digest(raw)!=table["sha256"] or list(struct.unpack(f"<{count}I",raw))!=table["targets"]:
        raise ValueError("F1 dispatch targets/hash differ from independent original bytes")


def verify_owner_snapshots(rows,expected):
    owners={r["owner"]:r for r in rows}
    for owner,sha in expected.items():
        if owner not in owners or digest_json(owners[owner])!=sha:
            raise ValueError("F1 reviewed map/metadata writer/xref scope changed")


def verify_unused_scope(source):
    verify_owner_snapshots(source["owner_refs"],REVIEWED_OWNER_SHA)
    functions={f["va"]:f for f in source["functions"]}
    if any(v not in functions for v in MAP_PARAMETER_WRITERS):
        raise ValueError("F1 map parameter writer closure missing")
    graph=source.get("opening_callgraph",[])
    forbidden={0x117e7,0x1a30b,0x1a813,0x190ac,0x1aa1d,0x1b653,0x1b6b7}
    if (not graph or 0x3231b not in {g["va"] for g in graph} or
            any(g["unresolved_calls"] for g in graph) or
            forbidden & {g["va"] for g in graph}):
        raise ValueError("F1 opening now has unresolved/battle consumer path")
    bindings=[(t["name"],i) for t in source["tables"] for i,v in enumerate(t["targets"]) if v==0x24754]
    if bindings!=[("victory",22)] or functions[0x24754]["callers"]!=[0x25bf4]:
        raise ValueError("F1 map replacement has a new caller/stage binding")


def verify_instruction_coverage(function):
    """独立拒绝仅主跨度／共享尾缺指令、重复或跨 chunk 的伪完整读集。"""
    instructions = sorted(function["instructions"], key=lambda row: row["va"])
    seen = set()
    for chunk in function["chunks"]:
        at = chunk["start"]
        for insn in instructions:
            if not chunk["start"] <= insn["va"] < chunk["end"]:
                continue
            if insn["va"] != at or type(insn["size"]) is not int or insn["size"] <= 0:
                raise ValueError("F1 incomplete/duplicate instruction chunk")
            if insn["va"] in seen:
                raise ValueError("F1 overlapping function chunks")
            seen.add(insn["va"])
            at += insn["size"]
        if at != chunk["end"]:
            raise ValueError("F1 shared-tail/instruction coverage missing")
    if len(seen) != len(instructions):
        raise ValueError("F1 instruction outside declared function chunks")


def verify_source(report, image):
    if report.get("schema") != "fd2-f1-domain-source-v1" or digest(image) != IMAGE_SHA:
        raise ValueError("F1 independent source identity mismatch")
    if not report.get("strict_byte_match"):
        raise ValueError("F1 incomplete Domain source")
    verify_dispatch_scope(report["tables"])
    for table in report["tables"]:
        verify_dispatch_bytes(table,image)
    pointer=report.get("callback255",{})
    if (pointer.get("va")!=0x51f8d or pointer.get("bytes")!=image[0x51f8d:0x51f91].hex()
            or pointer.get("value")!=int.from_bytes(image[0x51f8d:0x51f91],"little")
            or pointer.get("fixup_sources_touching")!=[]):
        raise ValueError("F1 callback255 source/fixup witness differs")
    for f in report["functions"]:
        if not f["chunks"] or not f["instructions"] or not f["blocks"]:
            raise ValueError("F1 incomplete function/CFG")
        verify_instruction_coverage(f)
        verify_function_bytes(f,image)
    for va in (0x1a813, 0x10c50, 0x112a5, 0x1b750):
        if va not in {f["va"] for f in report["functions"]}:
            raise ValueError("F1 required complete source missing")
    verify_unused_scope(report)


def machine(image):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:])
    cpu.mem_map(0x100000, 0x100000)
    active, roster, meta, placement = 0x100000, 0x110000, 0x120000, 0x130000
    stack, stop = 0x1f8000, 0x200000
    state = {"target": active, "writes": set(), "reads": set(), "pc": 0,
             "callback_reads": [], "u_target": None, "stubs": Counter()}
    def word(at):
        return struct.unpack("<I", cpu.mem_read(at, 4))[0]
    def put(at, value):
        cpu.mem_write(at, struct.pack("<I", value & 0xffffffff))
    def write(at, raw):
        cpu.mem_write(at, raw)
        if state["target"] <= at < state["target"] + 80:
            state["writes"].update(range(at-state["target"], min(at-state["target"]+len(raw),80)))
    def ret(value=None, discard=0):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if value is not None:
            cpu.reg_write(UC_X86_REG_EAX, value & 0xffffffff)
        cpu.reg_write(UC_X86_REG_EIP, word(sp))
        cpu.reg_write(UC_X86_REG_ESP, sp+4+discard)
    def code(_, address, size, data):
        state["pc"] = address
        if address == 0x3702f:
            state["stubs"]["watcom_stack_check"] += 1
            ret(discard=4)
        elif address in (0x145cd, 0x4df4c):
            state["stubs"]["map_only"] += 1
            ret(0)
        elif address == 0x11019:
            state["stubs"]["fdicon_cache"] += 1
            ret(0)
        elif address in (0x3771c, 0x37910):
            sp = cpu.reg_read(UC_X86_REG_ESP)
            dst, src, count = (word(sp+4+i*4) for i in range(3))
            if count > 80:
                raise ValueError("F1 libc stub outside bounded constructor")
            raw = bytes(cpu.mem_read(src, count)) if address == 0x3771c else bytes([src & 255])*count
            write(dst, raw)
            state["stubs"]["memmove" if address == 0x3771c else "memset"] += 1
            ret(dst)
    def memory(_, access, address, size, value, data):
        if state["target"] <= address < state["target"]+80:
            offsets = range(address-state["target"], min(address-state["target"]+size,80))
            state["writes" if access == unicorn.UC_MEM_WRITE else "reads"].update(offsets)
        if access == unicorn.UC_MEM_READ and address == 0x51f8d:
            state["callback_reads"].append({"pc": state["pc"], "address": address,
                                            "value": word(address)})
    def invalid(_, access, address, size, value, data):
        state["u_target"] = address
        return False
    cpu.hook_add(unicorn.UC_HOOK_CODE, code)
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, memory)
    cpu.hook_add(unicorn.UC_HOOK_MEM_FETCH_UNMAPPED, invalid)
    put(0x53a45, active); put(0x53bf7, roster)
    put(0x53a55, meta); put(0x53a59, placement)
    def run(va, args, target=active, preload=0xa5):
        state.update(target=target, writes=set(), reads=set(), callback_reads=[], u_target=None, stubs=Counter())
        cpu.mem_write(target, bytes([preload])*80)
        put(stack, stop)
        for i, arg in enumerate(args):
            put(stack+4+i*4, arg)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        try:
            cpu.emu_start(va, stop, count=20000)
        except unicorn.UcError:
            if state["u_target"] != 0x02030500:
                raise
        if not state["u_target"] and cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise ValueError("F1 original execution did not return within declared budget")
        return {"writes": sorted(state["writes"]), "reads": sorted(state["reads"]),
                "record": bytes(cpu.mem_read(target,80)), "callback_reads": state["callback_reads"],
                "u_target": state["u_target"], "stubs": dict(state["stubs"])}
    return cpu, put, run, (active, roster, meta, placement)


def opening_root_trace(image, source):
    """执行完整3231b外层控制流；callee仅按已声明边界记录，不签其效果。"""
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
    opening = next(f for f in source["functions"] if f["va"] == 0x3231b)
    calls = {target for insn in opening["instructions"]
             if insn["asm"].startswith("call") for target in insn["code_refs"]}
    forbidden = {0x117e7,0x1a30b,0x1a813,0x190ac,0x1aa1d,0x1b653,0x1b6b7}
    if calls & forbidden:
        raise ValueError("opening acquired a battle/descriptor consumer")
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000,0x54000); cpu.mem_write(0x10000,image[0x10000:])
    cpu.mem_map(0x100000,0x10000)
    stack, stop = 0x108000,0x200000
    def word(at): return struct.unpack("<I",cpu.mem_read(at,4))[0]
    cpu.mem_write(stack,struct.pack("<I",stop))
    cpu.reg_write(UC_X86_REG_ESP,stack)
    events=[]; stages=[]
    def code(_,pc,size,data):
        if pc in calls:
            sp=cpu.reg_read(UC_X86_REG_ESP)
            events.append({"callee":pc,"stage":word(0x53c03),"caller_return":word(sp)})
            cpu.reg_write(UC_X86_REG_EIP,word(sp))
            cpu.reg_write(UC_X86_REG_ESP,sp+(8 if pc==0x3702f else 4))
            cpu.reg_write(UC_X86_REG_EAX,0)
        elif not any(c["start"]<=pc<c["end"] for c in opening["chunks"]):
            raise ValueError("opening left its complete original source/stub set")
    def write(_,access,address,size,value,data):
        if address==0x53c03:stages.append(value)
    cpu.hook_add(unicorn.UC_HOOK_CODE,code)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,write)
    cpu.emu_start(0x3231b,stop,count=20000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or stages != [32,31,0] or word(0x53c03)!=0:
        raise ValueError("opening no longer returns only after stage0 initialization")
    return {"stage_writes":stages,"return_stage":0,"calls":events,
            "stage31_callees":sorted({e["callee"] for e in events if e["stage"]==31}),
            "battle_consumers_in_original_root":[],
            "scope":"complete original3231b root/loops; every callee is an explicit no-effect stub; transitive callee contracts require independent static review"}


def check(original, image, source):
    original = Path(original)
    verify_source(source, image)
    exe = (original/"FD2.EXE").read_bytes()
    field = (original/"FDFIELD.DAT").read_bytes()
    if digest(exe) != SOURCE_HASHES["FD2.EXE"] or digest(field) != SOURCE_HASHES["FDFIELD.DAT"]:
        raise ValueError("F1 original resource identity mismatch")
    entries = archive_entries(field)
    cpu, put, run, (active, roster, meta, placement) = machine(image)
    turn_cases = []
    for turn in (254,255,256,0xffffffff):
        for phase in (0,1,2):
            raw = bytearray(0x83)
            raw[3:6] = b"\xff\xff\0"
            cpu.mem_write(meta,bytes(raw)); put(0x53bef,turn)
            got = run(0x1a813,[phase])
            expected = turn == 255 and phase == 0
            if bool(got["u_target"]) != expected or bool(got["callback_reads"]) != expected:
                raise ValueError("F1 original turn matching/invalid-target counterexample changed")
            turn_cases.append({"turn":turn,"phase":phase,"callback255_read":got["callback_reads"],
                               "invalid_fetch":got["u_target"]})
    refusal_count = 0
    per_stage = []
    for stage in range(33):
        data = entries[stage*3+1][1]
        slots = [i for i in range(16) if data[3+i*3:6+i*3] == b"\xff\xff\0"]
        refusal_count += len(slots)
        per_stage.append({"stage":stage,"default_slots":slots})
    if refusal_count != 450:
        raise ValueError("F1 full default refusal source count changed")
    masks = Counter(); representatives = {}; constructor_cases = 0
    for stage in range(33):
        data = entries[stage*3+1][1]
        for index in range(data[2]):
            tpl = data[0x83+26*index:0x83+26*(index+1)]
            cpu.mem_write(meta,bytes(0x83)+tpl)
            cpu.mem_write(placement,struct.pack("<3H",0,3,4))
            put(0x53beb,0); cpu.mem_write(0x53afa,b"\x01")
            key = (tpl[1] < 68, tpl[5] == 255)
            results=[]
            for preload in (0xa5,0x5a):
                got = run(0x10c50,[0,0],preload=preload)
                untouched = tuple(i for i in range(80) if i not in got["writes"])
                expected = tuple(range(0x28,0x31))
                if tpl[5] == 255: expected=(0x0d,)+expected
                if untouched != expected or any(got["record"][i]!=preload for i in untouched):
                    raise ValueError("F1 original constructor write-mask changed")
                if set(untouched) & set(got["reads"]):
                    raise ValueError("F1 constructor reads an untouched byte before returning")
                results.append(untouched); constructor_cases += 1
                put(0x53beb,0)
            masks[str(results[0])] += 1
            representatives.setdefault(str(key), {"stage":stage,"template":index,
                "unit_id":tpl[1],"unwritten":list(results[0]),"constructor_reads_unwritten":False})
    roster_masks=Counter()
    for identity in range(32):
        put(0x53bfb,0)
        got=run(0x112a5,[identity],target=roster)
        untouched=tuple(i for i in range(80) if i not in got["writes"])
        expected=tuple(list(range(5))+[0x17,0x19]+list(range(0x28,0x31))+list(range(0x32,0x37))+[0x3d])
        if untouched != expected:
            raise ValueError("F1 original roster constructor mask changed")
        roster_masks[str(untouched)]+=1
    unused = []
    for stage in (1,31):
        grid, metadata = entries[stage*3][1], entries[stage*3+1][1]
        width,height=struct.unpack_from("<HH",grid)
        low12=[i for i in range(width*height) if grid[4+4*i+2]&31==12]
        if low12 or metadata[0x53+12*3:0x53+13*3]!=b"\x02\0\0":
            raise ValueError("unreferenced action12 source changed")
        unused.append({"source_id":f"stage:{stage:02}:cell_action:00012",
                       "action_bytes":"020000","all_map_cells_with_low5_equal12":len(low12),
                       "dynamic_map_writer_closure_proven":True,
                       "qualification":"normal original-generated map/metadata; external or edited saves not granted reachability"})
    return {"schema":"fd2-f1-boundary-original-cpu-v1","ok":True,
            "source_sha256":digest(json.dumps(source,sort_keys=True).encode()),
            "turn": {"vectors":turn_cases,"source_defaults":refusal_count,"per_stage":per_stage,
            "pointer_va":0x51f8d,"pointer_value":0x02030500,
            "first_invalid_source_operation":"1a85a reads callback table at index255; outside 90-slot dispatch; read is inside initialized object2 but resulting unrelocated non-code address has no valid FD2 target",
            "normal_255_reachability_proven":False,"valid_prior_callbacks_executed":False},
            "constructor":{"active_cases":constructor_cases,"resource_templates":constructor_cases//2,
            "masks":dict(masks),"branch_representatives":representatives,
            "roster_cases":32,"roster_masks":dict(roster_masks),
            "all_downstream_readers_closed":False,"fresh_heap_initialization_proven":False},
            "unused_records":{"action12":unused,"opening_root":opening_root_trace(image,source),
            "stage31_turn_and_drop_sources":["stage:31:turn:00000","stage:31:turn:00001",
            "stage:31:turn:00002","stage:31:turn:00003","stage:31:drop:00009","stage:31:drop:00014"],
            "full_transitive_consumer_and_map_writer_proof":True,
            "system_boundary_scope":"opening graph expands all gameplay PC paths including shared tails; Watcom/AIL/hardware leaves are explicit system boundaries, not emulated gameplay callbacks",
            "map_write_contracts":{
                "12263":"completed chest: tile WORD+=1, cell.byte2=0; never creates action12",
                "14625/146a7":"occupancy/ZOC OR40/80 preserve low5",
                "4df4c":"reset byte2&1f and byte1&3, byte3=ff; preserve low5",
                "146d1/14818/4e390/4e42c/4e4be":"destination/range writes byte3 only",
                "4e4f6/4e5cc/4e680":"path writes byte3 and byte1 upper6 only; low5 byte2 unchanged",
                "4e703/4e71f/4e751":"path output/witness/scratch, no action-index write",
                "10010/1088d":"load/init map and metadata from same original stage provenance",
                "24754":"only victory22; stage becomes23 before map69/shape46/47 replacement",
                "26152/2670e/2af28/2b439":"resource teardown or temporary0/1 no-map UI sentinel, not cell mutation",
                "metadata":"190ac only updates current action param WORD; event callbacks update turn bytes+3/+6/+9; no action12 index manufacture"}},
            "stubs":["Watcom stack probe RET4","libc memmove/memset byte semantics",
                     "145cd/4df4c map-only effects omitted","11019 graphics cache returns class0"],
            "not_claimed":["DOSBox/native","natural complete game","dynamic actor-count closure",
                           "unknown 80B readers beyond constructors","fresh/reused heap provenance"]}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original",required=True,type=Path)
    p.add_argument("--image",required=True,type=Path)
    p.add_argument("--source",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    args=p.parse_args()
    if args.output.exists() or args.output.resolve().is_relative_to(args.original.resolve()):
        raise ValueError("refusing to replace F1 evidence or write into original resources")
    result=check(args.original,args.image.read_bytes(),json.loads(args.source.read_text(encoding="utf8")))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open("x",encoding="utf8") as stream:
        stream.write(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps({"output":str(args.output),"sha256":digest(args.output.read_bytes()),
                      "ok":result["ok"],"templates":result["constructor"]["resource_templates"]}))


if __name__ == "__main__": main()
