#!/usr/bin/env python3
"""检查 `src/` 注释里的原版函数地址是否已迁移到 canonical relbase VA。

`AGENTS.md` 要求源码注释写明对应反编译函数的地址与名字。历史上这些注释混用了
两套已废弃坐标系（旧 dual 地址、旧 Ghidra 窗口地址），需要按
`docs/reverse-engineering/address-mapping.md` 的规则迁移到 canonical VA。

本脚本以 IDA 导出的函数表为 oracle，对注释里形如 `@0x…`（项目约定的函数地址写法）
的 token 分类：

    canonical         已是某个 IDA 函数入口，无需改动
    canonical-inside  落在某函数体内部（可能是 `__chkstk` body 或有意指向具体指令）
    stale-dual        `addr - 0x25214` 才是函数入口 → 旧 dual 地址，需要迁移
    stale-ghidra      `addr + 0x28b8` 才是函数入口 → 旧 Ghidra 窗口地址，需要迁移
    unknown           三种解释都不落在函数入口（可能是数据/文件偏移，需人工判断）

只读，不改文件。

用法：
    python3 tools/ida/check_src_addresses.py
    python3 tools/ida/check_src_addresses.py --only stale-dual --show
    python3 tools/ida/check_src_addresses.py --json tools/ida-out/src-address-audit.json
"""

from __future__ import annotations

import argparse
import bisect
import collections
import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FUNCS_TSV = ROOT / "docs" / "generated" / "ida-canonical" / "fd2-functions.tsv"

OBJ1_LO, OBJ1_HI = 0x10000, 0x4EF29
LEGACY_DUAL_SHIFT = 0x25214
LEGACY_GHIDRA_SHIFT = 0x28B8

# 项目约定：注释里的函数地址写作 `@0x…`。裸十六进制常量可能是数据偏移、掩码或
# 记录字段，不在本检查范围内。
TOKEN = re.compile(r"@(0x[0-9a-fA-F]{4,6})\b")
COMMENTISH = re.compile(r"(/\*|\*|//)")


def load_funcs():
    ranges, names = [], {}
    with FUNCS_TSV.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            s, e = int(row["start_ea"], 16), int(row["end_ea"], 16)
            ranges.append((s, e))
            names[s] = row["name"]
    ranges.sort()
    return ranges, names, [a for a, _ in ranges]


def source_files(directory: Path):
    for path in sorted([*directory.rglob("*.c"), *directory.rglob("*.h")]):
        parts = path.relative_to(directory).parts[:-1]
        if any(part in ("third_party", "build") or part.startswith("build-") for part in parts):
            continue
        yield path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", type=Path, default=ROOT / "src")
    ap.add_argument("--only", help="只显示指定分类")
    ap.add_argument("--show", action="store_true", help="逐条显示")
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()

    ranges, names, starts = load_funcs()

    def at(ea: int):
        if not (OBJ1_LO <= ea < OBJ1_HI):
            return None
        i = bisect.bisect_right(starts, ea) - 1
        if i >= 0 and ranges[i][0] <= ea < ranges[i][1]:
            return ranges[i][0], names[ranges[i][0]], ea - ranges[i][0]
        return None

    findings = []
    for path in source_files(args.src):
        for lineno, line in enumerate(
                path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if not COMMENTISH.search(line):
                continue
            for m in TOKEN.finditer(line):
                ea = int(m.group(1), 16)
                here = at(ea)
                dual = at(ea - LEGACY_DUAL_SHIFT) if ea > LEGACY_DUAL_SHIFT else None
                ghid = at(ea + LEGACY_GHIDRA_SHIFT)
                if here and here[2] == 0:
                    kind, suggest = "canonical", None
                elif dual and dual[2] == 0:
                    kind, suggest = "stale-dual", dual
                elif ghid and ghid[2] == 0:
                    kind, suggest = "stale-ghidra", ghid
                elif here:
                    kind, suggest = "canonical-inside", here
                else:
                    kind, suggest = "unknown", None
                findings.append({
                    "file": str(path.relative_to(ROOT)), "line": lineno,
                    "token": m.group(1), "kind": kind,
                    "suggest_va": f"{suggest[0]:#x}" if suggest else None,
                    "suggest_name": suggest[1] if suggest else None,
                    "offset_in_func": suggest[2] if suggest else None,
                    "text": line.strip()[:160],
                })

    stat = collections.Counter(f["kind"] for f in findings)
    print(f"`@0x…` 函数地址 token 共 {len(findings)} 个")
    for k, v in stat.most_common():
        print(f"  {k:18} {v}")
    per_file = collections.Counter(
        f["file"] for f in findings if f["kind"].startswith("stale"))
    if per_file:
        print("\n待迁移（stale-*）按文件：")
        for f, n in per_file.most_common():
            print(f"  {n:4}  {f}")
    if args.show:
        print()
        for f in findings:
            if args.only and f["kind"] != args.only:
                continue
            tail = ""
            if f["suggest_va"]:
                tail = f" -> {f['suggest_va']} ({f['suggest_name']}"
                tail += f"+{f['offset_in_func']:#x})" if f["offset_in_func"] else ")"
            print(f"  {f['file']}:{f['line']} {f['token']} [{f['kind']}]{tail}")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({
            "schema": "fd2-src-address-audit-v1",
            "oracle": str(FUNCS_TSV.relative_to(ROOT)),
            "stats": dict(stat), "findings": findings,
        }, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
