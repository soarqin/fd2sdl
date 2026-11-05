#!/usr/bin/env python3
"""局部执行原1b750的AP/DP缩放；不代表原版运行验收。

field_unit_combat_stats_recompute @0x1b750 的指令VA1b7ef/1b80a，
原__CHP @0x37af4；启动控制字127f/PC53，乘binary64 1.15后向0截断。
全部u16及-65536..-1两块共262144向量；原指令/常量直接读已验镜像，
无patch/handler。示例：uv run --no-project --with unicorn python
tools/ida/verify_fd2_x87_stat_scale.py --json C:/Temp/stat-scale.json

--write-c-model可生成独立C11模型，编译后将其输出经--c-results逐项比对；
仅带volatile double中间值的实际C输出才记为C映射验证。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

from verify_fd2_x87_boost import IMAGE_SHA256

ROOT = Path(__file__).resolve().parents[2]
FIRST, LAST = -65536, 65535
VECTOR_SHA256 = "55dafb59f3c3576c68a43d3f241cc735c5160a4b3eb48e362fb54315d0ea32c2"
BLOCKS = ((0x1b7ef, 0x1b802, 8), (0x1b80a, 0x1b81d, 12))
VECTORS = (-65536, -32768, -100, -60, -40, -20, -16, -12, -10, -1,
           0, 1, 10, 12, 16, 20, 40, 60, 100, 32767, 65535)
C_MODEL = """#include <stdint.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *out = fopen(argv[1], "wb");
    if (!out) return 3;
    for (int32_t value = -65536; value < 65536; value++) {
        volatile double factor = 1.15;
        volatile double product = (double)value * factor;
        int32_t scaled = (int32_t)product;
        if (fwrite(&scaled, sizeof(scaled), 1, out) != 1) return 4;
    }
    return fclose(out) != 0;
}
"""


def validate_image(image: bytes) -> None:
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError("不是已验证FD2 relocated镜像：SHA256不符")
    if image[0x5391c:0x5391e] != bytes.fromhex("7f12"):
        raise ValueError("原始控制字不是127f")
    if image[0x5018d:0x50195] != bytes.fromhex("666666666666f23f"):
        raise ValueError("原始binary64倍率不是1.15")


def verify(image: bytes, c_results: bytes | None = None) -> dict:
    validate_image(image)  # 不可信输入先拒绝，不导入/执行模拟器。
    count = LAST - FIRST + 1
    if c_results is not None and len(c_results) != count * 4:
        raise ValueError("C映射结果不是131072个little-endian int32")
    try:
        import unicorn
        from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPCW
    except ImportError as error:
        raise RuntimeError("缺少Unicorn；按文件头uv命令运行，不能记为通过") from error
    factor = struct.unpack_from("<d", image, 0x5018d)[0]
    report = {"engine": "Unicorn " + unicorn.__version__,
              "native_runtime_acceptance": False, "image_sha256": IMAGE_SHA256,
              "control_word": "0x127f", "factor_hex": "666666666666f23f",
              "input_range": [FIRST, LAST], "blocks": [],
              "c_output_compared": c_results is not None}
    for start, end, slot in BLOCKS:
        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        cpu.mem_map(0x10000, 0x54000)
        cpu.mem_write(0x10000, image[0x10000:0x64000])
        cpu.mem_map(0x100000, 0x10000)
        stack = 0x108000
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_FPCW, 0x127f)
        digest, c_digest = hashlib.sha256(), hashlib.sha256()
        vectors, integer_bad, nearest_bad = [], 0, 0
        for index, old in enumerate(range(FIRST, LAST + 1)):
            cpu.mem_write(stack + slot, struct.pack("<i", old))
            cpu.emu_start(start, end, count=100)
            actual = struct.unpack("<i", cpu.mem_read(stack + slot, 4))[0]
            binary64 = int(float(old) * factor)
            integer = (abs(old) * 115 // 100) * (-1 if old < 0 else 1)
            q, r = divmod(abs(old) * 115, 100)
            nearest = (q + (r > 50 or r == 50 and q % 2)) * (-1 if old < 0 else 1)
            if actual != binary64:
                raise RuntimeError(f"binary64模型不符: {start:#x} input={old} native={actual} model={binary64}")
            integer_bad += actual != integer
            nearest_bad += actual != nearest
            if old in VECTORS:
                vectors.append({"input": old, "native": actual,
                                "binary64_then_truncate": binary64,
                                "integer115_100": integer, "nearest_even": nearest})
            digest.update(struct.pack("<ii", old, actual))
            if c_results is not None:
                value = struct.unpack_from("<i", c_results, index * 4)[0]
                if value != actual:
                    raise RuntimeError(f"实际C映射不符: input={old} native={actual} c={value}")
                c_digest.update(struct.pack("<ii", old, value))
        if cpu.reg_read(UC_X86_REG_ESP) != stack or cpu.reg_read(UC_X86_REG_FPCW) != 0x127f:
            raise RuntimeError("原__CHP未恢复stack/control word")
        if digest.hexdigest() != VECTOR_SHA256:
            raise RuntimeError("全域原码结果SHA不符")
        block = {"start": hex(start), "end": hex(end), "inputs": count,
                 "vectors_sha256": digest.hexdigest(), "vectors": vectors,
                 "integer_model_mismatches": integer_bad,
                 "nearest_even_model_mismatches": nearest_bad}
        if c_results is not None:
            block["actual_c_vectors_sha256"] = c_digest.hexdigest()
        report["blocks"].append(block)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--c-results", type=Path)
    parser.add_argument("--write-c-model", type=Path)
    args = parser.parse_args()
    try:
        if args.write_c_model:
            args.write_c_model.parent.mkdir(parents=True, exist_ok=True)
            args.write_c_model.write_text(C_MODEL, encoding="utf-8", newline="\n")
        report = verify(args.image.read_bytes(), args.c_results.read_bytes() if args.c_results else None)
    except (OSError, ValueError, RuntimeError) as error:
        parser.error(str(error))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8", newline="\n")
    print("x87 AP/DP1.15: 2 x 131072 matches; native runtime acceptance=false; C compared=" + str(report["c_output_compared"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
