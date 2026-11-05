#!/usr/bin/env python3
"""局部执行原版AP/DP增益x87块，穷举u16；不代表原版运行或完整音画验收。

依据：sub_22721 @0x22721、sub_22866 @0x22866、FLIRT __CHP @0x37af4；
__init_8087 @0x463f2读取data VA0x5391c的127f并传4cf20，PC=53bit。
原版块和常量均直接从严格校验的relocated relbase image读取，不修指令／挂handler。
示例：uv run --no-project --with unicorn python tools/ida/verify_fd2_x87_boost.py --json C:/Temp/x87.json
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
ROOT = Path(__file__).resolve().parents[2]
IMAGE_SHA256 = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
VECTOR_SHA256 = "ec3e36b8d3d7343861ee5507577463144177794866500990886c183003f83d04"
BLOCKS = ((0x227db, 0x227f1), (0x22920, 0x22936))

def validate_image(image: bytes) -> None:
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError("不是已验证的FD2 relocated relbase镜像：SHA256不符")
    if image[0x5391c:0x5391e] != bytes.fromhex("7f12"):
        raise ValueError("原始控制字不是127f")
    for offset in (0x50210, 0x50218):
        if image[offset:offset+8] != bytes.fromhex("333333333333c33f"):
            raise ValueError("原始binary64增益常量不是0.15")

def verify(image: bytes) -> dict:
    validate_image(image)  # Never import an emulator or execute an unverified image.
    try:
        import unicorn
        from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPCW
    except ImportError as error:
        raise RuntimeError("缺少Unicorn；按文件头uv命令运行，不记为已通过") from error
    report = {"engine": "Unicorn " + unicorn.__version__, "image_sha256": IMAGE_SHA256,
              "control_word": "0x127f", "native_runtime_acceptance": False,
              "method": "原始x86块+原__CHP，无指令patch或handler；每块全部65536个u16输入", "blocks": []}
    for start, end in BLOCKS:
        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        cpu.mem_map(0x10000, 0x54000)
        cpu.mem_write(0x10000, image[0x10000:0x64000])
        cpu.mem_map(0x100000, 0x10000)
        stack = 0x108000
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_FPCW, 0x127f)
        digest = hashlib.sha256()
        for old in range(65536):
            cpu.mem_write(stack + 4, struct.pack("<i", old))
            cpu.emu_start(start, end, count=100)
            actual = struct.unpack("<i", cpu.mem_read(stack, 4))[0]
            expected = (15 * old + 100) // 100
            if actual != expected:
                raise RuntimeError(f"x87结果不符: {start:#x} old={old} actual={actual} expected={expected}")
            digest.update(struct.pack("<II", old, actual))
        if cpu.reg_read(UC_X86_REG_ESP) != stack or cpu.reg_read(UC_X86_REG_FPCW) != 0x127f:
            raise RuntimeError("原__CHP未恢复stack/control word")
        if digest.hexdigest() != VECTOR_SHA256:
            raise RuntimeError("全域结果流SHA256不符")
        report["blocks"].append({"start": hex(start), "end": hex(end), "inputs": 65536,
                                 "matches": 65536, "vectors_sha256": digest.hexdigest()})
    return report

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    try:
        report = verify(args.image.read_bytes())
    except (OSError, ValueError, RuntimeError) as error:
        parser.error(str(error))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8", newline="\n")
    print("x87 AP/DP: 2 × 65536 matches; native runtime acceptance=false")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
