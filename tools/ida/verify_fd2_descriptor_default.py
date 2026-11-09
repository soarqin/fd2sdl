#!/usr/bin/env python3
"""G2局部执行原1aa1d的default分支；不替代DOSBox／WGC验收。

完整原代码、共享epilogue22bbe和BIOS清键4e381直接取严格relocated镜像；
仅Watcom栈探测用已确认RET4宿主替身，不patch任何原字节。
uv run --no-project --with unicorn python tools/ida/verify_fd2_descriptor_default.py --image PATH --json OUT
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
IMAGE_SHA256 = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTION_SHA256 = "27d6043310a9806fed48393b299d7d71cddcc8a5f50e0a0f2340f2e8b6701d39"


def verify(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    if hashlib.sha256(image[0x1aa1d:0x1acf3]).hexdigest() != FUNCTION_SHA256:
        raise ValueError("descriptor full-function identity mismatch")
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0, 0x1000)  # BIOS ring: original helper executes, no keyboard emulation.
    cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(0x100000, 0x10000)
    cpu.mem_map(0x200000, 0x10000)
    stop, stack, units, descriptors = 0x300000, 0x208000, 0x100000, 0x101000
    cpu.mem_write(0x53a45, struct.pack("<I", units))
    calls = {"stack_probe_stub": 0, "original_bios_clear": 0}

    def guard(machine, address, size, userdata):
        if address == 0x3702f:
            # Verified Watcom ABI: pop return address and its pushed size parameter.
            pointer = machine.reg_read(UC_X86_REG_ESP)
            return_address = struct.unpack("<I", machine.mem_read(pointer, 4))[0]
            machine.reg_write(UC_X86_REG_ESP, pointer + 8)
            machine.reg_write(UC_X86_REG_EIP, return_address)
            calls["stack_probe_stub"] += 1
        elif address == 0x4e381:
            calls["original_bios_clear"] += 1
        elif not (0x1aa1d <= address < 0x1acf3 or 0x22bbe <= address < 0x22bc6 or
                  0x4e381 <= address < 0x4e390):
            raise RuntimeError(f"default escaped into unexpected code {address:#x}")

    cpu.hook_add(unicorn.UC_HOOK_CODE, guard)
    results = []
    cases = [(side, [mode]) for side in range(3) for mode in range(4, 256)]
    cases += [(side, [4, 203, 254, 255, 5]) for side in range(3)]
    cases += [(2, [])]
    for side, modes in cases:
        raw_unit = bytearray((i * 17 + 3) & 255 for i in range(80))
        raw_unit[6] = side
        cpu.mem_write(units, bytes(raw_unit))
        cpu.mem_write(descriptors, b"".join(bytes([mode, 255, 255]) for mode in modes))
        cpu.mem_write(0x41a, struct.pack("<HH", 0x1e, 0x3e))
        before = bytes(cpu.mem_read(0x10000, 0x54000))
        old_calls = dict(calls)
        cpu.mem_write(stack, struct.pack("<IIII", stop, 0, len(modes), descriptors))
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(0x1aa1d, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_ESP) != stack + 4:
            raise RuntimeError("descriptor did not return through the original shared epilogue")
        if bytes(cpu.mem_read(units, 80)) != bytes(raw_unit) or bytes(cpu.mem_read(0x10000, 0x54000)) != before:
            raise RuntimeError("default changed business memory")
        if calls["original_bios_clear"] - old_calls["original_bios_clear"] != len(modes):
            raise RuntimeError("default did not clear BIOS input exactly once per descriptor")
        expected_tail = 0x1e if modes else 0x3e
        if bytes(cpu.mem_read(0x41a, 4)) != struct.pack("<HH", 0x1e, expected_tail):
            raise RuntimeError("original BIOS clear result differs")
        if calls["stack_probe_stub"] != old_calls["stack_probe_stub"] + 1:
            raise RuntimeError("unexpected stack probe count")
        results.append({"side": side, "modes": modes, "business_unchanged": True})
    return {"scope": "original descriptor default and BIOS clear; stack probe stubbed",
            "image_sha256": IMAGE_SHA256, "function_sha256": FUNCTION_SHA256,
            "engine": "Unicorn " + unicorn.__version__, "cases": len(results),
            "modes": "4..255, all three sides, mixed default queue and empty queue",
            "calls": calls, "ok": True, "native_runtime_acceptance": False,
            "collector_255_sentinel_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--json", type=Path, required=True)
    args = parser.parse_args()
    report = verify(args.image.read_bytes())
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
