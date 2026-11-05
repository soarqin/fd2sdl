#!/usr/bin/env python3
"""以 IDA 9.4 独立分析为 oracle，逐行审计历史命名档案。

输入固定为 `docs/reverse-engineering/function-names-legacy.md`（只读历史档案）。
输出的规范登记表由 `tools/ida/build_fd2_name_registry.py` 生成到
`docs/reverse-engineering/function-names.md`，两者不得互为输入。

背景：该历史档案的地址列混有三套坐标系。

1. canonical relbase VA：`code0 + 0x10000`，object1 有效范围 `0x10000..0x4ef29`；
2. 旧 dual 地址：旧流程错把 `LE+0x80 = 0x10e00` 当作 **文件** 偏移，
   真实 page 1 在 `0x25214 + 0x10e00 = 0x36014`，因此
   `canonical_VA = legacy_dual_VA - 0x25214`；
3. 纯 code0 offset（有的行在文字里显式写了 `code0 0x…`）。

本脚本对每一行枚举候选 VA，并用 IDA 导出的函数表判定：

    exact              候选正好是 IDA 函数入口
    interior           候选落在某个 IDA 函数体内部（可能是 wrapper/body 或旧边界错误）
    unmapped           候选不在 object1 `0x10000..0x4ef29` 内
    ambiguous          多个候选都命中函数入口

用法：
    python3 tools/ida/fd2_name_audit.py                    # 打印摘要
    python3 tools/ida/fd2_name_audit.py --json out.json    # 机器可读账本
    python3 tools/ida/fd2_name_audit.py --markdown out.md  # 人类可读对照表
"""

from __future__ import annotations

import argparse
import bisect
import csv
import json
import re
from dataclasses import dataclass, field, asdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NAMES_MD = ROOT / "docs" / "reverse-engineering" / "function-names-legacy.md"
IDA_FUNCS = ROOT / "tools" / "ida-out" / "ida-functions.tsv"

# object1 canonical VA 范围（LE relbase 0x10000 + vsize 0x3ef29）
OBJ1_LO, OBJ1_HI = 0x10000, 0x4EF29
# 旧 dual 镜像的错误 page base 造成的常量偏移：
# 旧流程把 `LE+0x80 = 0x10e00` 当成文件偏移，真实 page 1 在 0x25214+0x10e00=0x36014。
LEGACY_DUAL_SHIFT = 0x25214
# 旧 Ghidra 窗口（`FUN_xxxxxxxx` 名字里的地址）另用一套基址：
# canonical_VA = FUN_addr + 0x28b8（0x28b8 是 embedded MZ 的 e_lfanew）。
LEGACY_GHIDRA_SHIFT = 0x28B8
CODE0_BASE = 0x10000
# Watcom `push imm32; call __chkstk` 前缀长度
CHKSTK_BODY_OFFSET = 0xA


@dataclass
class Candidate:
    va: int
    origin: str
    status: str = ""
    func_start: int | None = None
    func_end: int | None = None
    ida_name: str = ""
    offset_in_func: int | None = None
    is_lib: bool = False


@dataclass
class Row:
    line_no: int
    address_cell: str
    ghidra_cell: str
    semantic_name: str
    purpose: str
    evidence: str
    candidates: list[Candidate] = field(default_factory=list)
    resolution: str = ""
    canonical_va: int | None = None
    legacy_body_va: int | None = None
    ida_name: str = ""
    note: str = ""


def load_ida_functions() -> tuple[list[tuple[int, int]], dict[int, dict]]:
    ranges: list[tuple[int, int]] = []
    info: dict[int, dict] = {}
    with IDA_FUNCS.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            s = int(row["start_ea"], 16)
            e = int(row["end_ea"], 16)
            ranges.append((s, e))
            info[s] = row
    ranges.sort()
    return ranges, info


def parse_rows(path: Path = NAMES_MD) -> list[Row]:
    rows: list[Row] = []
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 5:
            continue
        if cells[0] in ("地址",) or set(cells[0]) <= set("-: "):
            continue
        rows.append(Row(i, cells[0], cells[1], cells[2], cells[3],
                        " | ".join(cells[4:])))
    return rows


def enumerate_candidates(row: Row) -> list[Candidate]:
    out: list[Candidate] = []
    seen: set[tuple[int, str]] = set()

    def add(va: int, origin: str) -> None:
        if (va, origin) in seen:
            return
        seen.add((va, origin))
        out.append(Candidate(va=va, origin=origin))

    literals = [int(m, 16) for m in re.findall(r"0x[0-9a-fA-F]+", row.address_cell)]

    # 地址列里显式写出的 code0
    for m in re.finditer(r"code0\s*(0x[0-9a-fA-F]+)", row.address_cell):
        add(int(m.group(1), 16) + CODE0_BASE, "address-cell code0+0x10000")
    for m in re.finditer(r"VA\s*(0x[0-9a-fA-F]+)", row.address_cell):
        add(int(m.group(1), 16), "address-cell VA")

    for lit in literals:
        add(lit, "address-cell literal")
        add(lit - LEGACY_DUAL_SHIFT, "address-cell literal - 0x25214")

    # 证据文字里的 corrected code0 / 权威 code0
    for m in re.finditer(
            r"(?:corrected|当前权威|权威|修正后的)\s*code0\s*[`\s]*(0x[0-9a-fA-F]+)",
            row.evidence):
        add(int(m.group(1), 16) + CODE0_BASE, "evidence code0+0x10000")

    # 旧 Ghidra 名字里的窗口地址：FUN_xxxxxxxx + 0x28b8。
    # 两个限制：
    #  - 当 `FUN_xxxx` 与地址列字面相同时，该行只有一套坐标（旧 dual），
    #    不得再套用 +0x28b8 规则，否则会凭巧合落进无关函数；
    #  - 当该单元格写着 `merged in/into/before`、`legacy`、`错记为`、`错分为`
    #    等限定语时，`FUN_xxxx` 指的是**别的**（外层或错判的）函数，不是本行
    #    自己的名字，同样不能用作本行的坐标来源。实测这正是
    #    `field_selection_overlay_draw` / `field_selection_sprite_blit`
    #    两行冲突的原因。
    disowned = any(k in row.ghidra_cell for k in
                   ("merged", "legacy", "错记", "错分", "未识别"))
    for m in ([] if disowned else re.finditer(r"FUN_([0-9a-fA-F]{6,8})",
                                              row.ghidra_cell)):
        fun_addr = int(m.group(1), 16)
        if fun_addr in literals:
            continue
        add(fun_addr + LEGACY_GHIDRA_SHIFT, "ghidra-name FUN_+0x28b8")
    return out


def classify(cands: list[Candidate], ranges, info) -> None:
    starts = [a for a, _ in ranges]
    for c in cands:
        if not (OBJ1_LO <= c.va < OBJ1_HI):
            c.status = "unmapped"
            continue
        i = bisect.bisect_right(starts, c.va) - 1
        if i >= 0 and ranges[i][0] <= c.va < ranges[i][1]:
            s, e = ranges[i]
            c.func_start, c.func_end = s, e
            c.ida_name = info[s]["name"]
            c.is_lib = info[s]["is_lib"] == "1"
            c.offset_in_func = c.va - s
            c.status = "exact" if c.va == s else "interior"
        else:
            c.status = "in-object-no-func"


def resolve(row: Row) -> None:
    """把一行历史登记归约到唯一 canonical VA。

    Watcom `__chkstk` wrapper 的机器码固定为 `push imm32`(5) + `call rel32`(5)，
    因此 body 恰好在入口 `+0xa`。旧 Ghidra 流程把 wrapper 与 body 拆成两个函数，
    历史档案登记的往往是 body 地址；IDA 把两者合成同一个函数，
    所以 `+0xa` 命中要归一化到函数入口，而不是记为边界错误。
    """
    # 优先级：显式已修正的 code0/VA > 两套 legacy 规则 > 原字面值
    priority = {
        "address-cell code0+0x10000": 0,
        "address-cell VA": 1,
        "evidence code0+0x10000": 2,
        "ghidra-name FUN_+0x28b8": 3,
        "address-cell literal - 0x25214": 4,
        "address-cell literal": 5,
    }

    def sort_key(c: Candidate):
        return (priority.get(c.origin, 9), c.va)

    exact = sorted((c for c in row.candidates if c.status == "exact"), key=sort_key)
    # 若存在非 FLIRT-runtime 的候选，就排除 runtime 候选：历史档案登记的是游戏
    # 自身的函数，落在 Watcom runtime（如 `strncpy`）上的候选必为坐标巧合。
    non_lib = [c for c in exact if not c.is_lib]
    if non_lib and len(non_lib) < len(exact):
        exact = non_lib
    if exact:
        best = exact[0]
        row.canonical_va = best.va
        row.ida_name = best.ida_name
        distinct = {c.va for c in exact}
        row.resolution = "exact" if len(distinct) == 1 else "exact-conflict"
        row.note = f"via {best.origin} -> {best.ida_name}"
        if len(distinct) > 1:
            row.note += " ; 其他 exact 候选: " + ", ".join(
                f"{c.va:#x}({c.origin})" for c in exact[1:])
        return

    body = sorted((c for c in row.candidates
                   if c.status == "interior" and c.offset_in_func == CHKSTK_BODY_OFFSET),
                  key=sort_key)
    if body:
        best = body[0]
        row.canonical_va = best.func_start
        row.legacy_body_va = best.va
        row.ida_name = best.ida_name
        row.resolution = "chkstk-body"
        row.note = (f"via {best.origin}: 历史地址 {best.va:#x} 是 __chkstk body，"
                    f"canonical 入口为 {best.func_start:#x} ({best.ida_name})")
        return

    interior = sorted((c for c in row.candidates if c.status == "interior"), key=sort_key)
    if interior:
        best = interior[0]
        row.canonical_va = best.va
        row.ida_name = best.ida_name
        row.resolution = "interior-mismatch"
        row.note = (f"via {best.origin} -> 落在 {best.ida_name} "
                    f"@{best.func_start:#x}+{best.offset_in_func:#x}，需人工复核")
        return

    row.resolution = "unresolved"
    row.note = "无候选命中 object1 函数"


def ida_oracle() -> str:
    paths = (IDA_FUNCS.parent / "ida-program.json", ROOT / "docs/generated/ida-canonical/fd2-program.json")
    info = next((json.loads(path.read_text(encoding="utf-8")) for path in paths if path.exists()), {})
    return (f"IDA Pro {info.get('ida_version', 'unknown')} {info.get('host_platform', 'historical host')} "
            f"idalib ({info.get('loader_module', 'LE loader')} + metapc + Watcom FLIRT)")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--names", type=Path, default=NAMES_MD,
                    help="历史命名档案路径（只读）")
    ap.add_argument("--json", type=Path)
    ap.add_argument("--markdown", type=Path)
    args = ap.parse_args()

    ranges, info = load_ida_functions()
    rows = parse_rows(args.names)
    for r in rows:
        r.candidates = enumerate_candidates(r)
        classify(r.candidates, ranges, info)
        resolve(r)

    import collections
    stats = collections.Counter(r.resolution for r in rows)
    origin_stats = collections.Counter()
    for r in rows:
        if r.resolution.startswith("exact"):
            origin_stats[r.note.split(" -> ")[0]] += 1

    print(f"历史登记行数: {len(rows)}")
    for k, v in stats.most_common():
        print(f"  {k:14} {v}")
    print("\n命中来源分布（exact）:")
    for k, v in origin_stats.most_common():
        print(f"  {k:48} {v}")

    # 语义名 -> canonical VA 冲突检查
    by_name: dict[str, set[int]] = {}
    for r in rows:
        if r.canonical_va is not None and r.semantic_name:
            by_name.setdefault(r.semantic_name, set()).add(r.canonical_va)
    dup = {n: sorted(v) for n, v in by_name.items() if len(v) > 1}
    by_va: dict[int, set[str]] = {}
    for r in rows:
        if r.canonical_va is not None and r.semantic_name:
            by_va.setdefault(r.canonical_va, set()).add(r.semantic_name)
    dupva = {v: sorted(n) for v, n in by_va.items() if len(n) > 1}
    print(f"\n同名多地址: {len(dup)}")
    for n, v in list(dup.items())[:10]:
        print(f"  {n}: {[hex(x) for x in v]}")
    print(f"同地址多名: {len(dupva)}")
    for v, n in list(dupva.items())[:10]:
        print(f"  {hex(v)}: {n}")

    if args.json:
        payload = {
            "schema": "fd2-ida-name-audit-v1",
            "oracle": ida_oracle(),
            "object1_range": [OBJ1_LO, OBJ1_HI],
            "legacy_dual_shift": LEGACY_DUAL_SHIFT,
            "stats": dict(stats),
            "rows": [asdict(r) for r in rows],
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=1, ensure_ascii=False),
                             encoding="utf-8", newline="\n")
        print(f"\nwrote {args.json}")

    if args.markdown:
        lines = ["| 行 | 历史地址列 | canonical VA | OFF | IDA 函数 | 判定 | 语义命名 |",
                 "|----|-----------|--------------|-----|----------|------|---------|"]
        for r in sorted(rows, key=lambda r: (r.canonical_va is None, r.canonical_va or 0)):
            va = f"`{r.canonical_va:#x}`" if r.canonical_va is not None else "—"
            off = f"`{r.canonical_va - CODE0_BASE:#x}`" if r.canonical_va is not None else "—"
            fn = r.ida_name
            lines.append(f"| {r.line_no} | `{r.address_cell}` | {va} | {off} | "
                         f"`{fn}` | {r.resolution} | `{r.semantic_name}` |")
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        print(f"wrote {args.markdown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
