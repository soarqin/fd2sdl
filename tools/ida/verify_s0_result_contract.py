"""S0：执行完整原结果函数及 controller，再与真实 C 结果查询比较。

依据 Domain 完整 205b4/206c5（含2067d共享尾）、117e7及原活动Load调用链。
固定严格 relocated 镜像；仅栈探测、输入、页面、奖励及phase叶为显式替身。
不执行文件Load、绘图、真实输入/等待或DOSBox；原机器码结果不取冷图/C作期望。
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess

from s0_result_source import TARGETS
from g1_preparation_evidence import RELOCATED_SHA256

UNIT_BASE, STACK, STOP = 0x100000, 0x208000, 0x400000
RESULT = 0x53ECC
CHUNKS = {
    0x10010: [(0x10010, 0x10620)], 0x117E7: [(0x117E7, 0x11AA8)],
    0x12D7B: [(0x12D7B, 0x12DAC)], 0x16F55: [(0x16F55, 0x1728C)],
    0x19DF7: [(0x19DF7, 0x1A30B)],
    0x205B4: [(0x205B4, 0x205DA), (0x2067D, 0x206C5)],
    0x206C5: [(0x206C5, 0x20707)], 0x25BF4: [(0x25BF4, 0x25EBB)],
    0x25EBB: [(0x25EBB, 0x26152)],
}


def validate_source(image, readset):
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    if (readset.get("schema") != "fd2-s0-result-domain-v1"
            or readset.get("image_sha256") != RELOCATED_SHA256
            or [f["va"] for f in readset.get("functions", [])] != list(TARGETS)):
        raise ValueError("incomplete or wrong Domain readset")
    for function in readset["functions"]:
        if not function.get("chunks") or not function.get("instructions") or not function.get("blocks"):
            raise ValueError("incomplete Domain function")
        if [(c["start"], c["end"]) for c in function["chunks"]] != CHUNKS[function["va"]]:
            raise ValueError("missing full original function/shared chunk")
        for chunk in function["chunks"]:
            a, b = chunk["start"], chunk["end"]
            raw = bytes.fromhex(chunk["raw_hex"])
            if (not 0x10000 <= a < b <= 0x4EF29 or len(raw) != b-a
                    or raw != image[a:b] or hashlib.sha256(raw).hexdigest() != chunk["sha256"]):
                raise ValueError("Domain/strict chunk mismatch")
    tail = readset.get("load_return_shared_tail", {})
    raw = image[0x22BBE:0x22BC6]
    if (tail.get("start"), tail.get("end"), tail.get("raw_hex"), tail.get("sha256")) != (
            0x22BBE, 0x22BC6, raw.hex(), hashlib.sha256(raw).hexdigest()):
        raise ValueError("missing verified Load shared return tail")
    return {f["va"]: f for f in readset["functions"]}


def records_for_case(leader_flags, enemy_flags, leader_hp, enemy_hp, villagers_hidden=False):
    records = bytearray(11 * 80)
    for raw in range(11):
        records[raw*80+5] = 1
        records[raw*80+6] = 1
        struct.pack_into("<H", records, raw*80+0x40, 100)
    for raw, flags, side, hp in ((0, leader_flags, 2, leader_hp), (1, enemy_flags, 0, enemy_hp)):
        records[raw*80+5] = flags
        records[raw*80+6] = side
        struct.pack_into("<H", records, raw*80+0x40, hp)
    records[5*80+5] = int(villagers_hidden)
    return bytes(records)


def vector_inputs():
    for stage, leader, enemy, lh, eh, villagers in itertools.product(
            (0, 1), (0, 1, 0x80, 0x81), (0, 1, 0x80, 0x81), (0, 1, 65535), (0, 1, 65535), (False, True)):
        yield stage, records_for_case(leader, enemy, lh, eh, villagers)


def execute(image, functions, stage, records, *, key=None, actor=0, initial_result=0, main_dispatch=False):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    if stage not in (0, 1) or len(records) % 80 or not 1 <= len(records)//80 <= 96:
        raise ValueError("invalid case domain")
    if stage == 1 and len(records)//80 < 11:
        raise ValueError("stage1 requires raw0..10")
    entry = 0x25DCE if main_dispatch else 0x117E7 if key is not None else (0x205B4 if stage == 0 else 0x206C5)
    owners = (0x205B4, 0x206C5) + ((0x117E7,) if key is not None else ()) + ((0x25BF4,) if main_dispatch else ())
    ranges = [(c["start"], c["end"]) for va in owners for c in functions[va]["chunks"]]
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(UNIT_BASE, 0x10000)
    cpu.mem_map(0x200000, 0x10000)
    cpu.mem_write(UNIT_BASE, records)
    def word(address): return struct.unpack("<I", cpu.mem_read(address, 4))[0]
    def put(address, value): cpu.mem_write(address, struct.pack("<I", value))
    put(0x53A45, UNIT_BASE); put(0x53BEB, len(records)//80)
    put(0x53C03, stage); put(0x51A8F, 255); put(0x53AE9, 0)
    put(RESULT, initial_result)
    put(STACK, STOP); cpu.reg_write(UC_X86_REG_ESP, STACK)
    calls, result_writes, dispatch = [], [], []
    stubs = {0x11AA8: key, 0x12C0D: actor & 0xFFFFFFFF, 0x18890: 1,
             0x11CAC: 0, 0x1E292: 0, 0x13565: 0, 0x4E381: 0,
             0x12D7B: 0, 0x17AED: 0, 0x2000A: 0, 0x16F55: 1,
             0x11B48: 0, 0x11B9B: 0, 0x11BFA: 0, 0x11C59: 0, 0x25A96: 0, 0x25977: 0}
    def code(machine, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if main_dispatch and address in (0x25DCE, 0x25DE5, 0x25E23):
            if address == 0x25DCE and not dispatch:
                dispatch.append("controller_enter")
            else:
                dispatch.append({0x25DCE: "ongoing", 0x25DE5: "defeat", 0x25E23: "victory"}[address])
                machine.emu_stop()
                return
        if address == 0x3702F:
            cpu.reg_write(UC_X86_REG_EIP, word(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+8)
            return
        if key is not None and address in stubs:
            calls.append(hex(address))
            cpu.reg_write(UC_X86_REG_EAX, stubs[address])
            cpu.reg_write(UC_X86_REG_EIP, word(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+4)
            return
        if address in (0x205B4, 0x206C5): calls.append(hex(address))
        if not any(a <= address < b for a, b in ranges):
            raise ValueError(f"undeclared original instruction: {address:#x}")
    def write(machine, access, address, size, value, data):
        if address == RESULT: result_writes.append(value)
        elif UNIT_BASE <= address < UNIT_BASE+len(records):
            raise ValueError("result/controller unexpectedly wrote unit record")
    cpu.hook_add(unicorn.UC_HOOK_CODE, code)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)
    cpu.emu_start(entry, STOP, count=100000)
    if cpu.reg_read(UC_X86_REG_EIP) != STOP and not (main_dispatch and len(dispatch) == 2):
        raise ValueError("original did not return")
    if bytes(cpu.mem_read(UNIT_BASE, len(records))) != records:
        raise ValueError("original record side effect")
    raw = word(RESULT)
    if raw not in (0, 1, 2): raise ValueError("unexpected original result")
    return {"stage": stage, "count": len(records)//80, "records_hex": records.hex(),
            "result_raw": raw, "result_sdl": {0: 0, 1: 2, 2: 1}[raw],
            "calls": calls, "result_writes": result_writes, "main_dispatch": dispatch}


def load_tail_cases(image, readset):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
    ranges = [(0x105EB, 0x10620), (0x22BBE, 0x22BC6)]
    tail = readset["load_return_shared_tail"]
    if bytes.fromhex(tail["raw_hex"]) != image[0x22BBE:0x22BC6]:
        raise ValueError("missing verified Load return tail")
    cases = []
    for initial in (0, 1, 2):
        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        cpu.mem_map(0x10000, 0x54000); cpu.mem_write(0x10000, image[0x10000:0x64000])
        cpu.mem_map(0x200000, 0x10000)
        cpu.mem_write(RESULT, struct.pack("<I", initial))
        # 共享epilogue addesp4/pop ebp/edi/esi/ebx/ret；不执行文件/分配前段。
        cpu.mem_write(STACK+20, struct.pack("<I", STOP))
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        calls = []
        def hook(machine, address, size, data):
            if address in (0x11CAC, 0x3790A, 0x4E381):
                calls.append(hex(address)); sp = cpu.reg_read(UC_X86_REG_ESP)
                target = struct.unpack("<I", cpu.mem_read(sp, 4))[0]
                cpu.reg_write(UC_X86_REG_EIP, target); cpu.reg_write(UC_X86_REG_ESP, sp+4)
                return
            if not any(a <= address < b for a, b in ranges):
                raise ValueError(f"undeclared Load tail instruction {address:#x}")
        cpu.hook_add(unicorn.UC_HOOK_CODE, hook)
        cpu.emu_start(0x105EB, STOP, count=1000)
        actual = struct.unpack("<I", cpu.mem_read(RESULT, 4))[0]
        if cpu.reg_read(UC_X86_REG_EIP) != STOP or actual != initial:
            raise ValueError("Load tail changed latch or failed return")
        cases.append({"initial_result": initial, "final_result": actual, "calls": calls})
    return cases


def controller_cases(image, functions):
    cases = []
    # 原actor选择取消由18890返回1；没有actor的Enter菜单返回1不进入1197b。
    for stage, key, actor, initial in itertools.product((0, 1), (44, 72, 59, 60, 28), (0, -1), (0, 1, 2)):
        records = records_for_case(0, 1, 0, 0, True)
        row = execute(image, functions, stage, records, key=key, actor=actor, initial_result=initial)
        queried = key == 28 and actor == 0
        if bool(row["result_writes"]) != queried:
            raise ValueError("original controller result timing differs")
        if not queried and row["result_raw"] != initial:
            raise ValueError("early-return controller changed result latch")
        row.update(key=key, actor=actor, initial_result=initial, result_queried=queried)
        cases.append(row)
    return cases


def validate_c_rows(cases, rows):
    if len(rows) != len(cases): raise ValueError("C vector count differs")
    for index, (expected, actual) in enumerate(zip(cases, rows)):
        if actual.split() != [str(expected["stage"]), str(expected["result_sdl"])]:
            raise ValueError(f"C result differs at vector {index}: {actual}")
    return len(rows)


def compare_c(executable, cases):
    executable = executable.resolve()
    before = executable.read_bytes()
    payload = "".join(f"{r['stage']} {r['count']} {r['records_hex']}\n" for r in cases)
    result = subprocess.run([str(executable), "--s0-vectors"], input=payload, text=True,
                            capture_output=True, check=False)
    if result.returncode:
        raise ValueError(f"C probe failed: {result.stderr[-1000:]}")
    count = validate_c_rows(cases, result.stdout.splitlines())
    if executable.read_bytes() != before:
        raise ValueError("C probe changed during comparison")
    return {"vectors": count, "path": str(executable), "sha256": hashlib.sha256(before).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--readset", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--c-probe", type=Path)
    args = parser.parse_args()
    if args.output.exists(): raise ValueError("output already exists")
    image = args.image.read_bytes()
    readset_raw = args.readset.read_bytes()
    functions = validate_source(image, json.loads(readset_raw))
    cases = [execute(image, functions, stage, records) for stage, records in vector_inputs()]
    controller = controller_cases(image, functions)
    main_cases = []
    for stage, key in itertools.product((0, 1), (44, 72, 59, 60, 28)):
        row = execute(image, functions, stage, records_for_case(0, 1, 0, 0, True),
                      key=key, actor=0, main_dispatch=True)
        expected = "ongoing" if key != 28 else "victory" if stage == 0 else "defeat"
        if row["main_dispatch"][-1] != expected: raise ValueError("main result exit differs")
        row["key"] = key; main_cases.append(row)
    c_probe = compare_c(args.c_probe, cases) if args.c_probe else None
    result = {"schema": "fd2-s0-result-original-cpu-v1", "ok": True,
              "image_sha256": RELOCATED_SHA256,
              "readset_sha256": hashlib.sha256(readset_raw).hexdigest(),
              "result_vectors": cases, "controller_vectors": controller,
              "load_tail_vectors": load_tail_cases(image, json.loads(readset_raw)),
              "main_dispatch_vectors": main_cases,
              "C_vectors_compared": c_probe["vectors"] if c_probe else 0, "C_probe": c_probe,
              "original_machine_code_executed": True, "original_Load_file_executed": False,
              "native_WGC_acceptance": False,
              "substitutions": ["Watcom RET4", "controller keyboard scan and actor lookup",
                  "18890 returns cancellation1", "refresh/reward/phase/input-clear/focus/detail/overview/sfx leaves"],
              "scope": "complete original205b4+shared2067d/206c5 and117e7; not Load IO, pixels, native input, full phase or GUI"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"ok": True, "result_vectors": len(cases), "controller_vectors": len(controller),
                      "C_vectors_compared": result["C_vectors_compared"]}))


if __name__ == "__main__": main()
