#!/usr/bin/env python3
"""用 IDA 的独立分析结果验证项目自研 LE parser 的 fixup 与镜像输出。

本脚本不依赖 idalib，只读取两侧已生成的产物，因此可以在没有 IDA 的机器上复核：

  项目侧  tools/fd2_le_raw.bin
          tools/fd2_le_relocated_relbase.bin
          tools/fd2_le_relocation_manifest.tsv
  IDA 侧  tools/ida-out/ida-program.json
          tools/ida-out/ida-fixups.tsv
          tools/ida-out/ida-image-<seg>-<base>.bin
          tools/ida-out/ida-functions.tsv

检查项：

  1. object relbase / vsize / 32 位属性一致；
  2. relocation source 集合完全相同；
  3. 每个 source 的 target linear 地址完全相同；
  4. IDA 载入后的字节 == 项目 relocated oracle（在 LE vsize 与已初始化范围内）；
  5. 把 IDA 镜像按 manifest 的 raw 前值反写后 == 项目 raw oracle；
  6. object2 未初始化区（BSS）边界一致；
  7. object2 / object3 内没有函数，函数最大结束地址不超过 object1 vsize。

IDA 用 `0xFF` 表示未初始化字节，因此 BSS 区按边界比对而不是按字节比对。

用法：
    python3 tools/ida/verify_ida_vs_project.py
    python3 tools/ida/verify_ida_vs_project.py --json tools/ida-out/ida-verify.json
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
IDA_OUT = TOOLS / "ida-out"

# LE object table 的权威值，由 tools/rebuild_fd2_analysis.py 从 FD2.EXE 解析得到。
EXPECTED_OBJECTS = [
    {"index": 1, "relbase": 0x10000, "vsize": 0x3EF29, "pages": 63,
     "ida_segment": "cseg01", "initialized_end": 0x4F000},
    {"index": 2, "relbase": 0x50000, "vsize": 0x56B0, "pages": 4,
     "ida_segment": "dseg02", "initialized_end": 0x54000},
    {"index": 3, "relbase": 0x60000, "vsize": 0x34D2, "pages": 4,
     "ida_segment": "dseg03", "initialized_end": 0x64000},
]

RAW_SHA256 = "37bceab29c9ee9089b5c5f9c4abefdee26a0bb34ae47bb917783dcc23ee30fc2"
RELOCATED_SHA256 = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
EXPECTED_UNIQUE_SOURCES = 7948
EXPECTED_RECORDS = 7959


def load_manifest(path: Path):
    records, targets, prevalues = [], {}, {}
    with path.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            src = int(row["source_relbase"], 16)
            tgt = int(row["target_linear"], 16)
            records.append((src, tgt))
            targets[src] = tgt
            prevalues[src] = int(row["raw_u32"], 16)
    return records, targets, prevalues


def load_ida_fixups(path: Path):
    out = {}
    with path.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            src = int(row["source_ea"], 16)
            out[src] = {
                "target": int(row["off"], 16),
                "value": int(row["value"], 16),
                "type": row["type"],
                "size": int(row["size"]),
                "stored": bytes.fromhex(row["stored_bytes"]),
            }
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()

    checks: list[dict] = []

    def check(name: str, ok: bool, detail: str = "") -> None:
        checks.append({"check": name, "ok": bool(ok), "detail": detail})
        print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail else ""))

    program = json.loads((IDA_OUT / "ida-program.json").read_text(encoding="utf-8"))
    segs = {s["name"]: s for s in program["segments"]}
    raw = (TOOLS / "fd2_le_raw.bin").read_bytes()
    rel = (TOOLS / "fd2_le_relocated_relbase.bin").read_bytes()

    # 0. oracle 身份
    check("项目 raw oracle SHA-256",
          hashlib.sha256(raw).hexdigest() == RAW_SHA256,
          hashlib.sha256(raw).hexdigest())
    check("项目 relocated oracle SHA-256",
          hashlib.sha256(rel).hexdigest() == RELOCATED_SHA256,
          hashlib.sha256(rel).hexdigest())
    check("IDA loader 识别为 Linear Executable (LE)",
          program["file_type_name"] == "Linear Executable (LE)",
          program["file_type_name"])
    check("IDA 识别 compiler 为 Watcom",
          "Watcom" in program["compiler_name"], program["compiler_name"])

    # 1. object 布局
    for obj in EXPECTED_OBJECTS:
        s = segs.get(obj["ida_segment"])
        if s is None:
            check(f"object{obj['index']} 存在对应 IDA segment", False,
                  obj["ida_segment"])
            continue
        page_end = obj["relbase"] + obj["pages"] * 0x1000
        ok = (s["start_ea"] == obj["relbase"] and s["bits"] == 32
              and s["end_ea"] in (obj["relbase"] + obj["vsize"], page_end))
        check(f"object{obj['index']} relbase/位宽/长度",
              ok, f"{s['start_ea']:#x}..{s['end_ea']:#x} bits={s['bits']} "
                  f"(LE vsize 末尾 {obj['relbase'] + obj['vsize']:#x}, "
                  f"页对齐末尾 {page_end:#x})")

    # 2/3. relocation
    records, targets, prevalues = load_manifest(TOOLS / "fd2_le_relocation_manifest.tsv")
    ida_fx = load_ida_fixups(IDA_OUT / "ida-fixups.tsv")
    check("项目 relocation 记录数 7959 / 唯一 source 7948",
          len(records) == EXPECTED_RECORDS and len(targets) == EXPECTED_UNIQUE_SOURCES,
          f"records={len(records)} unique={len(targets)}")
    check("IDA fixup 数量与项目唯一 source 数一致",
          len(ida_fx) == len(targets), f"ida={len(ida_fx)} project={len(targets)}")
    check("relocation source 集合完全相同",
          set(ida_fx) == set(targets),
          f"only_project={len(set(targets) - set(ida_fx))} "
          f"only_ida={len(set(ida_fx) - set(targets))}")
    tgt_mismatch = [s for s in sorted(set(ida_fx) & set(targets))
                    if ida_fx[s]["target"] != targets[s]]
    check("每个 source 的 target linear 地址一致",
          not tgt_mismatch, f"mismatches={len(tgt_mismatch)}")
    bad_type = [s for s, v in ida_fx.items()
                if v["type"] != "FIXUP_OFF32" or v["size"] != 4]
    check("IDA 全部 fixup 为 FIXUP_OFF32 / 4 字节",
          not bad_type, f"exceptions={len(bad_type)}")
    bad_store = [s for s, v in ida_fx.items()
                 if v["stored"] != struct.pack("<I", v["target"])]
    check("IDA 已把 target linear 地址写入 source",
          not bad_store, f"exceptions={len(bad_store)}")

    # 4/5/6. 镜像字节
    images = {}
    for obj in EXPECTED_OBJECTS:
        s = segs[obj["ida_segment"]]
        path = IDA_OUT / program["segment_dumps"][obj["ida_segment"]]["file"]
        images[obj["index"]] = path.read_bytes()

    diff_total = 0
    for obj in EXPECTED_OBJECTS:
        base, vsize = obj["relbase"], obj["vsize"]
        init_len = min(vsize, obj["initialized_end"] - base)
        a = rel[base:base + init_len]
        b = images[obj["index"]][:init_len]
        d = sum(1 for x, y in zip(a, b) if x != y)
        diff_total += d
        check(f"object{obj['index']} 已初始化区与 relocated oracle 逐字节一致",
              d == 0, f"compared={init_len:#x} diffs={d}")

    # object2 BSS 边界：IDA 用 0xFF 填充未初始化字节
    o2 = images[2]
    bss = o2[0x4000:]
    check("object2 BSS 区在 IDA 中标记为未初始化（0xFF 填充）",
          len(bss) == 0x16B0 and all(x == 0xFF for x in bss),
          f"len={len(bss):#x} all_ff={all(x == 0xFF for x in bss)}")
    check("项目 oracle 的 object2 BSS 区为零填充",
          all(x == 0 for x in rel[0x54000:0x556B0]))

    # 反写 raw 前值后应与 raw oracle 一致
    restored = bytearray(len(raw))
    for obj in EXPECTED_OBJECTS:
        base, vsize = obj["relbase"], obj["vsize"]
        init_len = min(vsize, obj["initialized_end"] - base)
        restored[base:base + init_len] = images[obj["index"]][:init_len]
    for src, v in prevalues.items():
        struct.pack_into("<I", restored, src, v)
    holes = [i for i in range(len(raw)) if raw[i] != restored[i]]
    # 只有 BSS 与 object 间空隙允许不同（两侧都应为零/未初始化）
    real = [i for i in holes if not (0x54000 <= i < 0x556B0)]
    check("反写 relocation 前值后与 raw oracle 一致（BSS 除外）",
          not real, f"diffs={len(real)} bss_only={len(holes) - len(real)}")

    # 7. 函数分布
    fpath = IDA_OUT / "ida-functions.tsv"
    if fpath.exists():
        starts, ends = [], []
        with fpath.open(encoding="utf-8") as f:
            for row in csv.DictReader(f, delimiter="\t"):
                starts.append(int(row["start_ea"], 16))
                ends.append(int(row["end_ea"], 16))
        outside = [s for s in starts if not (0x10000 <= s < 0x4EF29)]
        check("全部 IDA 函数位于 object1 范围内",
              not outside, f"outside={len(outside)} total={len(starts)}")
        check("函数最大结束地址不超过 object1 vsize 末尾",
              max(ends) <= 0x4EF29, f"max_end={max(ends):#x} limit=0x4ef29")

    failed = [c for c in checks if not c["ok"]]
    print()
    print(f"checks={len(checks)} passed={len(checks) - len(failed)} failed={len(failed)}")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({
            "schema": "fd2-ida-verify-v1",
            "checks": checks,
            "passed": len(checks) - len(failed),
            "failed": len(failed),
        }, indent=1, ensure_ascii=False), encoding="utf-8", newline="\n")
        print(f"wrote {args.json}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
