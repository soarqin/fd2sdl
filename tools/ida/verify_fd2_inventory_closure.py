#!/usr/bin/env python3
"""G5 原库存正常来源保持性：完整 owner xref 读集＋原 x86 三写叶。

Domain scope 是人工 alias/写点审查的固定来源，不由“未搜到 offset”自动证明；
本工具拒绝 scope/字节变化，并执行原 append/remove/equip 的有限等价类。
UI、libc memmove、Watcom stack check 边界不冒称原版窗口或完整 ABI。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
from verify_inventory_sources import verify as verify_initial

IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
SCOPE_SHA = "44b0a8c7ed04fb5b143e8769a1daf9e2b0ea1520032f0e8425df0e670cd67b0b"
XREF_SHA = "b35ff9feb450dc93a7b81c6147e27212910774945072169a82a50edcccd4b8cb"
CALLER_SHA = "de6a8f98a77eada12360966261082196b93902d2bddd0cae2450781b6d43176d"
CHUNKS_SHA = "4db2088e389742e818e3e324fd0637bc07bec574743a02634d2e1010c5818b22"
COUNTS_SHA = "753d3b7bd17181a3072cb0045ad93a14cec34d846c713787ab93e4c0ed1736c5"
START_SHA = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"
WRITERS = (0x10C50, 0x112A5, 0x1B8E7, 0x1BB8C, 0x1C142)
# 其余已审205函数不写库存：单位其它offset、只读UI/评分/资源及非记录缓冲。
# 80B/a00复制的源分别回到这两构造器／已满足归纳前提的同owner／正常Save。
RECORD_COPIES = (0x10010, 0x1088D, 0x11506, 0x25EBB, 0x2986F, 0x2B777, 0x2B843)
SAVE_WRITERS = (0x19DF7, 0x2968D)


def digest_json(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def verify_evidence(report, image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA:
        raise ValueError("strict relocated image identity mismatch")
    if report.get("schema") != "fd2-g5-inventory-domain-v1" or report.get("image_sha256") != IMAGE_SHA:
        raise ValueError("inventory evidence identity mismatch")
    functions = report.get("functions", [])
    if digest_json([f["va"] for f in functions]) != SCOPE_SHA:
        raise ValueError("inventory reviewed function scope changed")
    if digest_json(report.get("owner_refs")) != XREF_SHA:
        raise ValueError("inventory owner xref scope changed")
    if digest_json(report.get("writer_callers")) != CALLER_SHA:
        raise ValueError("inventory original writer caller scope changed")
    if digest_json(report.get("count_writes")) != COUNTS_SHA:
        raise ValueError("inventory original count write scope changed")
    chunks = [[f["va"], [[c["start"], c["end"], c["sha256"]] for c in f["chunks"]]] for f in functions]
    if digest_json(chunks) != CHUNKS_SHA:
        raise ValueError("inventory complete function chunk scope changed")
    by_va = {f["va"]: f for f in functions}
    byte_count = 0
    for f in functions:
        if not f["chunks"] or not f["instructions"] or not f["blocks"]:
            raise ValueError("inventory incomplete function evidence")
        for chunk in f["chunks"]:
            raw = bytes.fromhex(chunk["raw_hex"])
            if (len(raw) != chunk["end"]-chunk["start"] or
                    image[chunk["start"]:chunk["end"]] != raw or
                    hashlib.sha256(raw).hexdigest() != chunk["sha256"]):
                raise ValueError("inventory original function bytes changed")
            byte_count += len(raw)
    if [x["va"] for x in report["writer_callers"]] != list(WRITERS):
        raise ValueError("inventory writer caller list incomplete")
    for item in report["writer_callers"]:
        if item["callers"] != sorted(by_va[item["va"]]["callers"]) or any(v not in by_va for v in item["callers"]):
            raise ValueError("inventory original writer callers incomplete")
    return byte_count


def dense_record(record):
    flags = record[10:26:2]
    if len(record) != 80 or any(f not in (0, 0x40, 0x80) for f in flags):
        return False
    occupied = [not f & 0x80 for f in flags]
    return occupied == [i < sum(occupied) for i in range(8)]


def start_records(data):
    if hashlib.sha256(data).hexdigest() != START_SHA or len(data) != 22987:
        raise ValueError("normal START source identity mismatch")
    plain = bytearray(data); state = 0xa5
    for i in range(len(plain)):
        state = (state+0x9014) & 0xffff
        state = ((state << 3) | (state >> 13)) & 0xffff
        plain[i] ^= state & 255
    if sum(plain[:-4]) != struct.unpack_from("<I", plain, len(plain)-4)[0]:
        raise ValueError("normal START checksum mismatch")
    counts = (plain[0x30c4], plain[0x30cc]); result = []
    for base, count, maximum in ((0x12a3, counts[0], 96), (0x8a3, counts[1], 32)):
        if count > maximum:
            raise ValueError("normal START record count outside owner")
        for raw in range(count):
            record = plain[base+80*raw:base+80*(raw+1)]
            if not dense_record(record) or any(record[11+2*i] >= 215 for i in range(8) if not record[10+2*i]&128):
                raise ValueError("normal START inventory is outside declared induction base")
            result.append(bytes(record))
    return counts, result


def original_machine(image):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000); cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(0x100000, 0x10000); cpu.mem_map(0x200000, 0x10000)
    unit, stack, stop = 0x100000, 0x208000, 0x400000
    def word(at): return struct.unpack("<I", cpu.mem_read(at, 4))[0]
    def put(at, value): cpu.mem_write(at, struct.pack("<I", value))
    put(0x53a45, unit)
    allowed = ((0x1b8e7, 0x1b932), (0x1bb8c, 0x1bbdc), (0x1c142, 0x1c1c3), (0x1b722, 0x1b750))
    def code(machine, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if address == 0x3702f:
            cpu.reg_write(UC_X86_REG_EIP, word(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+8)
        elif address == 0x3771c:
            target, source, count = (word(sp+4+4*i) for i in range(3))
            if not unit <= target <= unit+80 or not unit <= source <= unit+80 or count > 14:
                raise ValueError("original remove memmove outside declared record")
            cpu.mem_write(target, bytes(cpu.mem_read(source, count)))
            cpu.reg_write(UC_X86_REG_EAX, target)
            cpu.reg_write(UC_X86_REG_EIP, word(sp)); cpu.reg_write(UC_X86_REG_ESP, sp+4)
        elif not any(a <= address < b for a, b in allowed):
            raise ValueError(f"undeclared inventory original callee {address:#x}")
    cpu.hook_add(unicorn.UC_HOOK_CODE, code)
    def execute(va, record, argument):
        cpu.mem_write(unit, bytes(record)); put(stack, stop); put(stack+4, 0); put(stack+8, argument)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(va, stop, count=2000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise ValueError("original inventory writer failed to return")
        after = bytes(cpu.mem_read(unit, 80))
        if after[:10] != record[:10] or after[26:] != record[26:]:
            raise ValueError("original inventory writer touched other unit fields")
        return after, cpu.reg_read(UC_X86_REG_EAX)
    return execute


def check_original_writers(image):
    execute = original_machine(image); cases = 0
    for count in range(9):
        for mask in range(1 << count):
            for classes in range(3):
                record = bytearray([0xa5]*80)
                for slot in range(8):
                    record[10+2*slot] = (0x40 if mask & (1 << slot) else 0) if slot < count else 0x80
                    record[11+2*slot] = (1 if classes == 0 else 128 if classes == 1 else 1 if slot & 1 else 128) if slot < count else 0xfe
                after, returned = execute(0x1bb8c, record, 214); cases += 1
                if not dense_record(after) or returned != (1 if count < 8 else 0xffffffff):
                    raise ValueError("original append breaks dense/type induction")
                for selected in range(count):
                    for va in (0x1b8e7, 0x1c142):
                        after, _ = execute(va, record, selected); cases += 1
                        if not dense_record(after):
                            raise ValueError("original remove/equip breaks dense/type induction")
    record = bytearray([0]*80)
    for slot in range(1,8): record[10+2*slot] = 0x80
    record[24] = 0xc0
    after, _ = execute(0x1c142, record, 0)
    if dense_record(after) or after[24] != 0:
        raise ValueError("original external C0 counterexample was not preserved")
    return {"vectors": cases, "external_c0_counterexample": True,
            "scope": "all visible flag bit patterns; 0..8 count/all visible selected raw; same/opposite/mixed ID classes",
            "not_claimed": ["libc machine code", "UI selection/RNG", "DOSBox/native", "arbitrary external SAV"]}


def check(original, image, evidence):
    original = Path(original)
    initial = verify_initial((original/"FD2.EXE").read_bytes(), (original/"FDFIELD.DAT").read_bytes())
    byte_count = verify_evidence(evidence, image)
    counts, _ = start_records((original/"START.SAV").read_bytes())
    cpu = check_original_writers(image)
    return {"schema": "fd2-g5-inventory-normal-closure-v1", "ok": True,
            "scope": "normal original resource constructors, START and saves generated from states satisfying this invariant",
            "manual_source_review_required": "build/g5/inventory-source-review.md",
            "source_functions": 205, "source_function_bytes": byte_count, "owner_xrefs": [262,28],
            "source_operations_preservation_proven": True, "normal_source_only": True,
            "evidence_sha256": digest_json(evidence),
            "count_write_sites": evidence["count_writes"],
            "copy_zero_case": "1088d hidden placeholder memset80 gives eight occupied item0 slots; dense/type still holds",
            "direct_writers": [hex(x) for x in WRITERS], "record_copy_boundaries": [hex(x) for x in RECORD_COPIES],
            "save_writers": [hex(x) for x in SAVE_WRITERS], "initial_sources": initial,
            "start_active_roster_counts": counts, "original_cpu": cpu,
            "arbitrary_external_save_domain_proven": False, "original_runtime_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", required=True, type=Path)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    report = check(args.original, args.image.read_bytes(), json.loads(args.evidence.read_text(encoding="utf-8")))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__": main()
