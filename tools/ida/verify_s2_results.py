"""执行18个完整原结果函数，与真实C source-query/common续态作局部差分。

只替代Watcom栈探测及15684文本呈现；34894/33499执行原机器码。
文本入口记录片段、9参和53ECC快照；stop_at_story是明确的局部前缀，
不是DOSBox输入/取消oracle。正常域、缺raw的C安全拒绝及未开放绑定分列。
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess

from s2_result_source import (TARGETS, HELPERS, CHUNKS, MINIMUM, EFFECTFUL,
                              FAMILIES, source_flow, closure_mapping, validate_source, dispatch_source)

UNIT, ROSTER, STATE, STACK, STOP = 0x100000, 0x110000, 0x120000, 0x208000, 0x400000
RESULT, TEXT = 0x53ECC, 0x15F84
RAW_TO_SDL = {0: 0, 1: 2, 2: 1}
SDL_TO_RAW = {0: 0, 2: 1, 1: 2}


def records(hidden=(), *, enemy=True, leader=False, hp=100, count=83):
    data = bytearray(count * 80)
    for i in range(count):
        data[i*80+6] = 1
        struct.pack_into("<H", data, i*80+0x40, hp)
    data[6] = 2
    if count > 1:
        data[86] = 0 if enemy else 1
    for i in set(hidden) | ({0} if leader else set()):
        data[i*80+5] = 0x81
    return bytes(data)


def vector_inputs():
    """按原共享谓词的真假组合生成，不读取冷图；每段range另验全部边界raw。"""
    predicates = {
        0x205B4: ((),), 0x206C5: (tuple(range(5, 11)),),
        0x20707: ((50,), (51,)), 0x2073D: ((14,),),
        0x20765: (tuple(range(15, 27)), (59,)), 0x20822: ((64,),),
        0x2084A: ((65,),), 0x20872: ((52,),),
        0x208CF: ((0,), (16,), (17,), (52,)), 0x20926: ((64,),),
        0x20957: (tuple(range(53, 61)), (0,), (52,), tuple(range(36, 52)), tuple(range(61, 83))),
        0x20A51: ((16,), (17,)), 0x20A87: ((1,),),
        0x20AAF: ((0,), (1,), (16,), (17,), (18,)),
        0x20B14: ((16,),), 0x20B3C: ((1,), (2,)),
        0x20B72: ((0,), (1,)), 0x20BF5: ((20,), (0,), (1,)),
    }
    for target in TARGETS:
        groups = predicates[target]
        turns = (-2147483648, 5, 6, 7, 2147483647) if target in (0x20765, 0x20926) else (1,)
        rosters = (b"", bytes(8)+bytes([18])+bytes(71),
                   bytes(8)+bytes([17])+bytes(71)) if target == 0x20872 else (b"",)
        states = (bytes(18)+bytes(v)+bytes(11) for v in itertools.product((0, 255), repeat=3)) if target == 0x20B72 else (bytes(32),)
        states = tuple(states)
        for mask, turn, roster, state, initial in itertools.product(
                range(1 << len(groups)), turns, rosters, states, (0, 1, 2)):
            hidden = {raw for bit, group in enumerate(groups) if mask & (1 << bit) for raw in group}
            yield {"target": target, "records": records(hidden), "turn": turn & 0xFFFFFFFF,
                   "roster": roster, "state": state, "initial": initial, "stop": 0}
        # 普通共享尾的三条终态、HP0／flags高位及最小有效前缀。
        for enemy, leader, hp in itertools.product((False, True), (False, True), (0, 1, 65535)):
            yield {"target": target, "records": records(enemy=enemy, leader=leader, hp=hp, count=MINIMUM[target]),
                   "turn": 0xFFFFFFFF, "roster": b"", "state": bytes(32), "initial": 2, "stop": 0}
        # 每个多raw全hidden谓词逐一留一人，避免只证明range首尾。
        for group in groups:
            if len(group) > 1:
                for visible in group:
                    yield {"target": target, "records": records(set(group)-{visible}), "turn": 7,
                           "roster": b"", "state": bytes(32), "initial": 0, "stop": 0}
    # 5效果图在每个实际文本入口停止，核晚写/第二文本不提前到达。
    for target, hidden in ((0x20765, set(range(15, 27))|{59}), (0x20872, {52}),
                           (0x20957, set(range(53, 61))), (0x20B72, {1}), (0x20BF5, {1, 20})):
        for stop in range(1, 3 if target == 0x20765 else 2):
            yield {"target": target, "records": records(hidden), "turn": 7,
                   "roster": b"", "state": bytes(32), "initial": 0, "stop": stop}


def execute(image, functions, case):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    target, unit = case["target"], case["records"]
    if (target not in TARGETS or len(unit) % 80 or not MINIMUM[target] <= len(unit)//80 <= 96
            or len(case["roster"]) % 80 or len(case["roster"])//80 > 96
            or len(case["state"]) != 32 or case["initial"] not in RAW_TO_SDL):
        raise ValueError("case outside original result domain")
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000); cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(UNIT, 0x30000); cpu.mem_map(0x200000, 0x10000)
    cpu.mem_write(UNIT, unit); cpu.mem_write(ROSTER, case["roster"]); cpu.mem_write(STATE, case["state"])
    def get(address): return struct.unpack("<I", cpu.mem_read(address, 4))[0]
    def put(address, value): cpu.mem_write(address, struct.pack("<I", value))
    for address, value in ((0x53A45, UNIT), (0x53BEB, len(unit)//80), (0x53BF7, ROSTER),
                           (0x53BFB, len(case["roster"])//80), (0x53AD5, STATE),
                           (0x53BEF, case["turn"]), (RESULT, case["initial"]), (0x53A79, 0x130000)):
        put(address, value)
    put(STACK, STOP); cpu.reg_write(UC_X86_REG_ESP, STACK)
    ranges = [span for spans in CHUNKS.values() for span in spans]
    writes, stories, visited, edges, last = [], [], set(), set(), {}
    blocks = {b["start"]: (va, b) for va, row in functions.items() for b in row["blocks"]}
    def code(machine, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if address == 0x3702F:
            cpu.reg_write(UC_X86_REG_EIP, get(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+8); return
        if address == TEXT:
            args = [get(sp+4+4*i) for i in range(9)]
            if args[0] != 0x130000 or args[2:] != [655360, 320, 205, 76, 74, 19, 1]:
                raise ValueError("original result text layout differs from existing story contract")
            stories.append({"fragment": args[1], "latch": RAW_TO_SDL[get(RESULT)], "args": args})
            if case["stop"] == len(stories): machine.emu_stop(); return
            cpu.reg_write(UC_X86_REG_EAX, 0)
            cpu.reg_write(UC_X86_REG_EIP, get(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+4); return
        if not any(a <= address < b for a, b in ranges):
            raise ValueError(f"undeclared original instruction {address:#x}")
        visited.add(address)
        if address in blocks:
            va, block = blocks[address]
            previous = last.get(va)
            if previous is not None and address in blocks[previous][1]["successors"]:
                edges.add((previous, address))
            last[va] = address
    def write(machine, access, address, size, value, data):
        if address == RESULT and size == 4:
            if value not in RAW_TO_SDL: raise ValueError("invalid result write")
            writes.append(RAW_TO_SDL[value])
        elif not 0x200000 <= address < 0x210000:
            raise ValueError(f"undeclared original write {address:#x}")
    cpu.hook_add(unicorn.UC_HOOK_CODE, code); cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)
    cpu.emu_start(target, STOP, count=100000)
    stopped = case["stop"] != 0 and len(stories) == case["stop"]
    if not stopped and cpu.reg_read(UC_X86_REG_EIP) != STOP:
        raise ValueError("original result did not return")
    if bytes(cpu.mem_read(UNIT, len(unit))) != unit or bytes(cpu.mem_read(STATE, 32)) != case["state"]:
        raise ValueError("original changed records/event state")
    return {"target": hex(target), "result": RAW_TO_SDL[get(RESULT)], "writes": writes,
            "stories": stories, "stopped_at_story": stopped, "visited": sorted(visited), "edges": sorted(edges)}


def probe_line(case):
    _, flow = source_flow(case["target"])
    return " ".join((flow, str(len(case["records"])//80), str(case["turn"]),
                     str(RAW_TO_SDL[case["initial"]]), str(case["stop"]), case["state"].hex(),
                     case["roster"].hex() or "-", case["records"].hex()))


def validate_probe(expected, actual):
    try: row = json.loads(actual)
    except (ValueError, TypeError) as error: raise ValueError("invalid C probe reply") from error
    stories = [{"fragment": s["fragment"], "latch": s["latch"]} for s in expected["stories"]]
    if (row.get("status") != 0 or row.get("result") != expected["result"] or row.get("stories") != stories
            or row.get("stopped_at_story") != expected["stopped_at_story"] or row.get("owner_unchanged") is not True):
        raise ValueError(f"C/original result trace differs: {expected['target']}: {row}")


def compare_c(probe, cases, results):
    """执行前后绑定同一二进制；机器码expected不由探针生成。"""
    before = hashlib.sha256(probe.read_bytes()).hexdigest()
    completed = subprocess.run([str(probe.resolve()), "--s2-vectors"],
                               input="\n".join(map(probe_line, cases))+"\n", text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    if hashlib.sha256(probe.read_bytes()).hexdigest() != before:
        raise ValueError("C probe changed while executing")
    rows = completed.stdout.splitlines()
    if len(rows) != len(results): raise ValueError("C probe case count differs")
    for expected, actual in zip(results, rows): validate_probe(expected, actual)
    return {"before_sha256": before, "after_sha256": hashlib.sha256(probe.read_bytes()).hexdigest(),
            "unchanged": True}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--readset", required=True, type=Path)
    parser.add_argument("--original", required=True, type=Path)
    parser.add_argument("--ida-directory", type=Path, default=Path("docs/generated/ida-canonical"))
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    if args.output.exists(): raise ValueError("output already exists")
    image = args.image.read_bytes(); readset_bytes = args.readset.read_bytes()
    readset = json.loads(readset_bytes)
    functions = validate_source(image, readset)
    table = dispatch_source(args.original / "FD2.EXE", args.ida_directory)
    cases = list(vector_inputs()); results = [execute(image, functions, case) for case in cases]
    probe_identity = compare_c(args.probe, cases, results) if args.probe else None
    if args.image.read_bytes() != image or args.readset.read_bytes() != readset_bytes:
        raise ValueError("source evidence changed while executing")
    visited = set().union(*(r["visited"] for r in results))
    edges = set().union(*(set(map(tuple, r["edges"])) for r in results))
    coverage = [{"target": hex(va), "blocks": len(functions[va]["blocks"]),
                 "blocks_reached": sum(b["start"] in visited for b in functions[va]["blocks"]),
                 "unreached_block_starts": [hex(b["start"]) for b in functions[va]["blocks"] if b["start"] not in visited],
                 "edges": sum(len(b["successors"]) for b in functions[va]["blocks"]),
                 "edges_reached": sum((b["start"], target) in edges for b in functions[va]["blocks"] for target in b["successors"]),
                 "unreached_edges": [[hex(b["start"]), hex(target)] for b in functions[va]["blocks"]
                                     for target in b["successors"] if (b["start"], target) not in edges]}
                for va in TARGETS + HELPERS]
    report = {"schema": "fd2-s2-result-cpu-v1", "readset_sha256": hashlib.sha256(args.readset.read_bytes()).hexdigest(),
              "image_sha256": hashlib.sha256(image).hexdigest(), "dispatch_raw_sha256": table["sha256"],
              "counts": {"source_slots": 30, "different_targets": 18, "effectful_targets": 5,
                         "semantic_families": len(set(FAMILIES.values())), "converted_source_definitions": 18,
                         "new_source_definitions": 16, "new_runtime_bindings": 0,
                         "pure_targets": 13, "cases": len(results), "c_compared": len(results) if args.probe else 0,
                         "prefix_cases": sum(c["stop"] != 0 for c in cases)},
              "candidate_mapping": closure_mapping(), "coverage": coverage,
              "substitutes": ["watcom_stack_check", "text_dialog_render_tokens: no RNG/drawing/input; records entry and optional explicit stop"],
              "limits": ["synthetic normal raw domain", "not complete gameplay, DOSBox, text internals or natural stage reachability",
                         "S0 old query timing/Load contract reused; no S3 bindings"],
              "c_probe_identity": probe_identity,
              "cases": [{k: v for k, v in row.items() if k not in {"visited", "edges"}} for row in results],
              "original_runtime_executed": False, "new_gameplay_enabled": False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), **report["counts"],
                      "all_blocks_reached": all(not r["unreached_block_starts"] for r in coverage),
                      "all_edges_reached": all(not r["unreached_edges"] for r in coverage)}))


if __name__ == "__main__":
    main()
