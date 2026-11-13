"""S2：完整11df2/17aa9原机器码对真实VGA/Play C叶，另记两原flash caller轨迹。

outp、Watcom栈探测、BIOS时钟和caller的pan/group/ms-delay/refresh为显式替身。
保留11df2的1951f共享返回尾；不把brightness、真实时间或GUI当作oracle。
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess

from s2_visual_source import CHUNKS, TAIL, validate_source

PALETTE, STACK, STOP = 0x100000, 0x208000, 0x400000
TICK_CASES = (
    ("one", 1, 100, (100, 101), 101),
    ("five", 5, 100, (100, 104, 105), 106),
    ("zero_once", 0, 100, (100,), 101),
    ("negative_once", -1, 100, (100,), 101),
    ("signed_min_once", -2147483648, 100, (100,), 101),
    ("signed_half_wrap", 1, 0x7fff, (0x8000,), 0x8001),
    ("ffff_wrap", 1, 0xffff, (0,), 1),
    ("negative_start", 2, 0x8000, (0x8000, 0x8001, 0x8002), 0x8003),
    ("reverse_means_full_cycle", 65535, 100, (99,), 99),
    ("maximum_goal", 65535, 0xffff, (0xfffe,), 0xfffe),
    ("multi_wrap", 3, 0xfffe, (0xfffe, 0xffff, 0, 1), 2),
)


def palette_cases():
    for seed, add in itertools.product(range(64), (0, 1, 32, 63, 255)):
        yield 5, 5, add, seed
    for first, last, add in itertools.product((0,), (255,), (0, 1, 32, 63, 255)):
        yield first, last, add, 17
    for first, last in ((5, 9), (9, 5)):
        for add in (0, 1, 32, 63, 255): yield first, last, add, 63


def machine(image):
    import unicorn
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0, 0x1000); cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(PALETTE, 0x1000); cpu.mem_map(0x200000, 0x10000)
    return cpu


def execute_palette(image, first, last, add, seed, *, wrapper=None):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    cpu = machine(image)
    base = bytes((i*17+seed) % 64 for i in range(768)); dac = bytearray([0xa5]*768)
    cpu.mem_write(PALETTE, base); cpu.mem_write(0x53A65, struct.pack("<I", PALETTE))
    def get(address): return struct.unpack("<I", cpu.mem_read(address, 4))[0]
    entry = wrapper or 0x11DF2
    args = (7, 9, 0x123) if wrapper == 0x35B78 else () if wrapper else (first, last, add)
    cpu.mem_write(STACK, struct.pack("<"+"I"*(len(args)+1), STOP, *args)); cpu.reg_write(UC_X86_REG_ESP, STACK)
    ranges = [(a, b) for spans in CHUNKS.values() for a, b, _ in spans] + [TAIL]
    index, channel, writes, events, visited = 0, 0, 0, [], set()
    def ret(value=0, clean=0):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EAX, value); cpu.reg_write(UC_X86_REG_EIP, get(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+4+clean)
    def code(machine, address, size, data):
        nonlocal index, channel, writes
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if address == 0x3702F: ret(clean=4); return
        if address == 0x37AE5:
            port, value = get(sp+4), get(sp+8)
            if port == 0x3c8: index, channel = value, 0
            elif port == 0x3c9:
                if not 0 <= index <= 255 or not 0 <= value <= 63: raise ValueError("original DAC outside source domain")
                dac[index*3+channel] = (value << 2) | (value >> 4); channel += 1; writes += 1
            else: raise ValueError("unexpected original hardware port")
            ret(value); return
        if wrapper and address in (0x135DD, 0x10B4E, 0x3790A, 0x11CAC):
            name, count = {0x135DD: ("pan", 2), 0x10B4E: ("append_group", 1),
                           0x3790A: ("delay_ms", 1), 0x11CAC: ("refresh", 1)}[address]
            events.append([name, *[get(sp+4+4*i) for i in range(count)]]); ret(); return
        if address == 0x11DF2 and wrapper: events.append(["palette_add", get(sp+4), get(sp+8), get(sp+12)])
        if not any(a <= address < b for a, b in ranges): raise ValueError(f"undeclared visual instruction {address:#x}")
        visited.add(address)
    def write(machine, access, address, size, value, data):
        if not 0x200000 <= address < 0x210000: raise ValueError("original palette changed nonstack state")
    cpu.hook_add(unicorn.UC_HOOK_CODE, code); cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)
    cpu.emu_start(entry, STOP, count=10000000)
    if cpu.reg_read(UC_X86_REG_EIP) != STOP or bytes(cpu.mem_read(PALETTE, 768)) != base:
        raise ValueError("original palette failed return/base preservation")
    return {"kind": "palette", "status": 0, "dac_hex": dac.hex(), "other_unchanged": True,
            "dac_component_writes": writes, "caller_events": events, "visited": sorted(visited)}


def execute_ticks(image, case):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
    name, goal, start, polls, tail = case
    sequence = (start, *polls, tail); read_at = 0; anchor_writes = []; visited = set()
    cpu = machine(image); cpu.mem_write(STACK, struct.pack("<II", STOP, goal & 0xffffffff)); cpu.reg_write(UC_X86_REG_ESP, STACK)
    def get(address): return struct.unpack("<I", cpu.mem_read(address, 4))[0]
    def read(machine, access, address, size, value, data):
        nonlocal read_at
        if address != 0x46c: return
        if size != 2 or read_at >= len(sequence): raise ValueError("original consumed extra BIOS clock")
        cpu.mem_write(0x46c, struct.pack("<H", sequence[read_at])); read_at += 1
    def code(machine, address, size, data):
        if address == 0x3702F:
            sp = cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP, get(sp));cpu.reg_write(UC_X86_REG_ESP, sp+8);return
        if not 0x17AA9 <= address < 0x17AED: raise ValueError("unexpected original tick callee")
        visited.add(address)
    def write(machine, access, address, size, value, data):
        if address == 0x53A2C: anchor_writes.append(value)
        elif not 0x200000 <= address < 0x210000: raise ValueError("original tick wrote external state")
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ, read);cpu.hook_add(unicorn.UC_HOOK_CODE, code);cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)
    cpu.emu_start(0x17AA9, STOP, count=10000)
    if cpu.reg_read(UC_X86_REG_EIP) != STOP or read_at != len(sequence): raise ValueError("original tick trace did not close")
    signed = lambda word: word if word < 0x8000 else word - 65536
    if anchor_writes != [signed(start) & 0xffffffff, signed(tail) & 0xffffffff]: raise ValueError("original tick scratch write differs")
    return {"kind": "ticks", "name": name, "status": 0, "clock_reads": read_at,
            "frames": len(polls)-1, "owner_unchanged": True, "script_exact": True,
            "original_scratch_writes": anchor_writes, "visited": sorted(visited)}


def compare_probe(probe, inputs, expected):
    before = hashlib.sha256(probe.read_bytes()).hexdigest()
    result = subprocess.run([str(probe.resolve()), "--s2-visual-vectors"], input="\n".join(inputs)+"\n",
                            text=True, capture_output=True, check=True)
    after = hashlib.sha256(probe.read_bytes()).hexdigest()
    if before != after: raise ValueError("visual C probe changed while executing")
    actual = result.stdout.splitlines()
    if len(actual) != len(expected): raise ValueError("visual C case count differs")
    for index, (line, source) in enumerate(zip(actual, expected)):
        row = json.loads(line)
        keys = ("kind", "status", "dac_hex", "other_unchanged") if source["kind"] == "palette" else (
            "kind", "status", "clock_reads", "frames", "owner_unchanged", "script_exact")
        if any(row.get(key) != source[key] for key in keys): raise ValueError(f"visual original/C trace differs at {index}")
    return {"before_sha256": before, "after_sha256": after, "unchanged": True}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path);parser.add_argument("--readset", required=True, type=Path)
    parser.add_argument("--probe", type=Path);parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    if args.output.exists(): raise ValueError("output already exists")
    image = args.image.read_bytes(); source_bytes = args.readset.read_bytes();source = json.loads(source_bytes)
    functions = validate_source(image, source)
    palettes = list(palette_cases()); results = [execute_palette(image, *case) for case in palettes]
    results += [execute_ticks(image, case) for case in TICK_CASES]
    inputs = ["palette " + " ".join(map(str, case)) for case in palettes]
    inputs += ["ticks " + " ".join(map(str, (goal, start, len(polls), *polls, tail))) for _, goal, start, polls, tail in TICK_CASES]
    identity = compare_probe(args.probe, inputs, results) if args.probe else None
    callers = [{"target": hex(va), **execute_palette(image, 0, 255, 0, 17, wrapper=va)} for va in (0x35B78, 0x361B0)]
    visited = set().union(*(row.get("visited", []) for row in results+callers))
    coverage = [{"target": hex(va), "blocks": len(row["blocks"]),
                 "unreached_block_starts": [hex(b["start"]) for b in row["blocks"] if b["start"] not in visited]}
                for va, row in functions.items()]
    if args.image.read_bytes() != image or args.readset.read_bytes() != source_bytes: raise ValueError("visual source changed while executing")
    report = {"schema": "fd2-s2-visual-cpu-v1", "readset_sha256": hashlib.sha256(source_bytes).hexdigest(),
              "image_sha256": hashlib.sha256(image).hexdigest(), "source_bytes": 494,
              "counts": {"palette_cases": len(palettes), "tick_cases": len(TICK_CASES),
                         "c_compared": len(results) if args.probe else 0, "caller_source_only": 2},
              "c_probe_identity": identity, "coverage": coverage,
              "cases": [{k: v for k, v in row.items() if k not in {"visited", "dac_hex"}} for row in results],
              "callers": [{k: v for k, v in row.items() if k not in {"visited", "dac_hex"}} for row in callers],
              "substitutes": ["stack probe", "outp->captured DAC", "scripted BIOS WORD", "caller pan/group/ms-delay/refresh"],
              "limits": ["C tick host is nominal SDL/55 low WORD, not real BIOS timing; final scratch has no external reader",
                         "palette add source domain0..255/6bit base; negative/overflow/hardware masking outside domain",
                         "two full caller traces are source-only, not whole callback C/gameplay or natural stage/GUI evidence"],
              "original_runtime_executed": False, "new_gameplay_enabled": False}
    args.output.parent.mkdir(parents=True, exist_ok=True);args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), **report["counts"], "all_visual_blocks_reached": all(not r["unreached_block_starts"] for r in coverage)}))


if __name__ == "__main__": main()
