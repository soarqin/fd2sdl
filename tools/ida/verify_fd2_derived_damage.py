#!/usr/bin/env python3
"""G2：原1c81f完整伤害core与真实SDL C API批量差分。

--prepare输出输入流与调用生产API的C harness；编译后以--c-results验结果。
原码不patch，仅Watcom栈探测和指定15bit RNG为显式替身；不执行FIGANI
marker／wrapper，也不冒称DOSBox／WGC。派生power采用G1已证signed公式。
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
CORE_SHA = "49c9ba6730675ae147a49f9ed219fcf9cb519de2dbf8c63c1c21aeee0b905e04"
INPUT = struct.Struct("<BBBBHHHHII")
OUTPUT_SIZE = 92
C_HARNESS = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "field_magic.h"
static uint32_t next_value, calls;
static uint32_t fixed_rng(void *context) { (void)context; calls++; return next_value; }
static uint16_t u16(const unsigned char *p) { return (uint16_t)(p[0] | (uint16_t)p[1]<<8); }
static uint32_t u32(const unsigned char *p) { return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int put32(FILE *f, uint32_t v) {
    unsigned char p[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};
    return fwrite(p,1,4,f)==4;
}
int main(int argc, char **argv) {
    if(argc!=3) return 2;
    FILE *in=fopen(argv[1],"rb"), *out=fopen(argv[2],"wb");
    if(!in || !out) return 3;
    unsigned char p[20]; size_t got;
    while((got=fread(p,1,sizeof(p),in))==sizeof(p)) {
        fd2_field_unit caster={0}, target={0}; fd2_field_magic_basic_result result={0};
        target.unit_id=p[1]; target.level=p[2]; caster.attack=u16(p+4); target.defense=u16(p+6);
        target.hp=u16(p+8); target.hp_max=u16(p+10); next_value=u32(p+12); calls=0;
        uint32_t owner=u32(p+16);
        if(fd2_field_magic_apply_derived_result(p[0],&caster,&target,fixed_rng,NULL,&owner,&result)!=1) return 4;
        if(fwrite(&target,1,80,out)!=80 || !put32(out,owner) || !put32(out,result.raw_damage) || !put32(out,calls)) return 5;
    }
    int bad=got || ferror(in); if(fclose(in) || fclose(out)) bad=1;
    return bad ? 6 : 0;
}
'''


def vectors():
    for mid, ap, dp, identity, level, hp, maximum, roll in itertools.product(
            (24, 28, 29, 30), (0, 50, 32767, 32768, 65535), (0, 80, 32768, 65535),
            (0, 68, 135), (1, 99), (0, 1, 50, 32767, 32768, 65535), (200, 65535), (0, 37, 99, 32767)):
        yield mid, identity, level, 0, ap, dp, hp, maximum, roll, 0xff123456


def trunc_div(a, b):
    return (-1 if a < 0 else 1) * (abs(a) // b)


def signed16(value):
    return value if value < 32768 else value - 65536


def verify(image, c_results=None):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA or hashlib.sha256(image[0x1c81f:0x1c8ed]).hexdigest() != CORE_SHA:
        raise ValueError("strict image or full core identity mismatch")
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000); cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(0x100000, 0x10000); cpu.mem_map(0x200000, 0x10000)
    target, stack, stop = 0x100000, 0x208000, 0x300000
    cpu.mem_write(0x53a45, struct.pack("<I", target))
    state = {}

    def trace(machine, address, size, userdata):
        if address in (0x3702f, 0x4ebe3):
            pointer = machine.reg_read(UC_X86_REG_ESP)
            return_address = struct.unpack("<I", machine.mem_read(pointer, 4))[0]
            machine.reg_write(UC_X86_REG_ESP, pointer + (8 if address == 0x3702f else 4))
            machine.reg_write(UC_X86_REG_EIP, return_address)
            if address == 0x4ebe3:
                state["rng_calls"] += 1; machine.reg_write(UC_X86_REG_EAX, state["roll"])
        elif address == 0x1c897:
            state["damage"] = machine.reg_read(UC_X86_REG_EBP)
        elif not (0x1c81f <= address < 0x1c8ed or 0x14230 <= address < 0x14237 or
                  0x10b46 <= address < 0x10b4e or 0x4e84f <= address < 0x4e866):
            raise RuntimeError(f"core escaped into unexpected code {address:#x}")

    def writes(machine, access, address, size, value, userdata):
        if 0x200000 <= address < 0x210000: return  # original stack/saved registers
        if (address, size) == (target + 0x40, 2): state["writes"].append("HP")
        elif (address, size) == (0x53ec8, 4): state["writes"].append("contribution")
        else: raise RuntimeError(f"unexpected business write {address:#x}/{size}")

    cpu.hook_add(unicorn.UC_HOOK_CODE, trace)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, writes)
    digest, count = hashlib.sha256(), 0
    for case in vectors():
        mid, identity, level, _, ap, dp, hp, maximum, roll, owner = case
        raw = bytearray(80); raw[7] = identity; raw[0x21] = level
        struct.pack_into("<HH", raw, 0x40, hp, maximum); struct.pack_into("<H", raw, 0x4a, dp)
        # Write fixture before enabling write collection in emulation (mem_write never runs hooks).
        cpu.mem_write(target, bytes(raw)); cpu.mem_write(0x53ec8, struct.pack("<I", owner))
        ratio = 15 if mid == 24 else 20 if mid == 28 else 12 if mid == 29 else 18
        power = trunc_div(signed16(ap) * ratio, 10) - signed16(dp)
        cpu.mem_write(stack, struct.pack("<IIi", stop, 0, power)); cpu.reg_write(UC_X86_REG_ESP, stack)
        state.clear(); state.update(roll=roll, rng_calls=0, writes=[])
        cpu.emu_start(0x1c81f, stop, count=1000)
        expected_writes = ["HP", "contribution"] if identity >= 68 else ["HP"]
        if (cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_ESP) != stack + 4 or
                state["rng_calls"] != 1 or state["writes"] != expected_writes):
            raise RuntimeError(f"core return/RNG/write trace differs: {case} {state}")
        actual = bytes(cpu.mem_read(target, 80)) + bytes(cpu.mem_read(0x53ec8, 4)) + struct.pack("<II", state["damage"], 1)
        if c_results is not None and c_results[count * OUTPUT_SIZE:(count + 1) * OUTPUT_SIZE] != actual:
            raise RuntimeError(f"real SDL C/core mismatch at vector {count}: {case}")
        digest.update(INPUT.pack(*case) + actual); count += 1
    if c_results is not None and len(c_results) != count * OUTPUT_SIZE:
        raise ValueError("real C output extent differs from declared vector stream")
    return {"ok": True, "vectors": count, "image_sha256": IMAGE_SHA, "core_sha256": CORE_SHA,
            "engine": "Unicorn " + unicorn.__version__, "vectors_sha256": digest.hexdigest(),
            "real_C_compared": c_results is not None, "native_runtime_acceptance": False,
            "original_intermediate_write_trace_observed": True, "real_C_intermediate_writes_traced": False,
            "scope": "1c81f core, original enemy getter/shared tails, all 80 target bytes, contribution/RNG/write order",
            "stubs": ["Watcom stack probe RET4", "one declared 15bit RNG result"],
            "wrapper_and_FIGANI_markers_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare", type=Path)
    parser.add_argument("--image", type=Path, default=ROOT / "tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--c-results", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    if args.prepare:
        args.prepare.mkdir(parents=True, exist_ok=True)
        (args.prepare / "derived-input.bin").write_bytes(b"".join(INPUT.pack(*case) for case in vectors()))
        (args.prepare / "derived-harness.c").write_text(C_HARNESS, encoding="utf-8", newline="\n")
    if args.json:
        report = verify(args.image.read_bytes(), args.c_results.read_bytes() if args.c_results else None)
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
