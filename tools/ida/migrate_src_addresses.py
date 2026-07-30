#!/usr/bin/env python3
"""把 `src/` 注释里的旧坐标函数地址迁移到 canonical relbase VA。

只改注释里形如 `@0x…` 的函数地址 token，不改任何代码语义、不改裸十六进制常量
（那些可能是数据偏移、文件偏移、掩码或记录字段）。

判定用 IDA 导出的函数表做 oracle，且只在下列两种情形之一成立时改写：

  A. `addr` 本身完全不落在任何 object1 函数内，而 `addr - 0x25214`
     （旧 dual）或 `addr + 0x28b8`（旧 Ghidra 窗口）正好是某个函数入口；
  B. `addr` 形式上落在某个无关函数体内部，但注释自身提供了消歧证据，具体为
     以下任一：
       - 邻近注释行（±1 行）写出的语义名等于候选函数的 IDA 名字；
       - 邻近注释行写出候选函数的历史登记别名
         （来自 `fd2-ida-function-registry.json`）；
       - 邻近注释行写出旧 Ghidra 名 `FUN_xxxxxxxx`，且 `FUN_addr + 0x28b8`
         正好等于候选函数入口 —— 两套 legacy 坐标互相印证。

  C. 注释按项目约定写成 `<语义名> @0x…`，且该语义名在规范登记表
     （`fd2-ida-function-registry.json`）里唯一对应一个 canonical VA。
     这条不依赖任何固定位移，因此能处理旧地址落在 `__chkstk` body 上、
     或两套 legacy 位移都不成立的情形。

其余情形（`canonical`、`canonical-inside`、`unknown`）一律不动，交由
`tools/ida/check_src_addresses.py` 报告后人工处理。

全部改写记录写入 `docs/generated/ida-canonical/fd2-src-address-migration.json`，
保留 legacy 地址、canonical VA、函数名和判定规则，满足可追溯要求。

用法：
    python3 tools/ida/migrate_src_addresses.py --dry-run
    python3 tools/ida/migrate_src_addresses.py
"""

from __future__ import annotations

import argparse
import bisect
import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FUNCS_TSV = ROOT / "docs" / "generated" / "ida-canonical" / "fd2-functions.tsv"
REPORT = (ROOT / "docs" / "generated" / "ida-canonical"
          / "fd2-src-address-migration.json")
REGISTRY = (ROOT / "docs" / "generated" / "ida-canonical"
            / "fd2-ida-function-registry.json")

OBJ1_LO, OBJ1_HI = 0x10000, 0x4EF29
LEGACY_DUAL_SHIFT = 0x25214
LEGACY_GHIDRA_SHIFT = 0x28B8

TOKEN = re.compile(r"@(0x[0-9a-fA-F]{4,6})\b")
COMMENTISH = re.compile(r"(/\*|\*|//)")
FUN_NAME = re.compile(r"FUN_([0-9a-fA-F]{6,8})")


NAME_AT = re.compile(r"([A-Za-z_][A-Za-z_0-9]{3,})\s*@(0x[0-9a-fA-F]{4,6})\b")


def load_aliases() -> dict[int, set[str]]:
    """canonical VA -> 全部历史登记语义名（用于注释消歧）。"""
    if not REGISTRY.exists():
        return {}
    data = json.loads(REGISTRY.read_text(encoding="utf-8"))
    return {e["va"]: set(e.get("historical_names", [])) | {e.get("ida_name", "")}
            for e in data["entries"]}


def build_name_index(aliases: dict[int, set[str]]) -> dict[str, int]:
    """语义名 -> canonical VA。只保留唯一映射的名字。"""
    idx: dict[str, set[int]] = {}
    for va, names in aliases.items():
        for n in names:
            if n:
                idx.setdefault(n, set()).add(va)
    return {n: next(iter(v)) for n, v in idx.items() if len(v) == 1}


def load_funcs():
    ranges, names = [], {}
    with FUNCS_TSV.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            s, e = int(row["start_ea"], 16), int(row["end_ea"], 16)
            ranges.append((s, e))
            names[s] = row["name"]
    ranges.sort()
    return ranges, names, [a for a, _ in ranges]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", type=Path, default=ROOT / "src")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    ranges, names, starts = load_funcs()
    aliases = load_aliases()
    name_index = build_name_index(aliases)

    def at(ea: int):
        if not (OBJ1_LO <= ea < OBJ1_HI):
            return None
        i = bisect.bisect_right(starts, ea) - 1
        if i >= 0 and ranges[i][0] <= ea < ranges[i][1]:
            return ranges[i][0], names[ranges[i][0]], ea - ranges[i][0]
        return None

    changes, skipped = [], []
    for path in sorted(list(args.src.rglob("*.c")) + list(args.src.rglob("*.h"))):
        if "third_party" in path.parts or "build" in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        raw_lines = text.splitlines(keepends=True)
        out_lines = []
        touched = False
        for lineno, line in enumerate(raw_lines, 1):
            if not COMMENTISH.search(line):
                out_lines.append(line)
                continue
            # 消歧上下文：当前行加相邻各一行（同一注释块常跨行）
            ctx = "".join(raw_lines[max(0, lineno - 2):lineno + 1])

            # 规则 C 的锚点：同一行 `<name> @0x…` 的名字归属
            anchored = {mm.group(2).lower(): mm.group(1)
                        for mm in NAME_AT.finditer(line)}

            def repl(m: re.Match) -> str:
                ea = int(m.group(1), 16)
                here = at(ea)
                if here and here[2] == 0:
                    return m.group(0)          # 已是 canonical 入口

                anchor = anchored.get(m.group(1).lower())
                target = name_index.get(anchor) if anchor else None
                if target is not None and target != ea:
                    tname = names.get(target, "")
                    changes.append({
                        "file": str(path.relative_to(ROOT)), "line": lineno,
                        "from": m.group(1), "to": f"{target:#x}",
                        "ida_name": tname, "rule": "registry-name-anchored",
                        "disambiguation": f"comment writes `{anchor} @…`",
                        "text": line.strip()[:160],
                    })
                    return f"@{target:#x}"
                for shift, rule in ((-LEGACY_DUAL_SHIFT, "legacy-dual -0x25214"),
                                    (LEGACY_GHIDRA_SHIFT, "legacy-ghidra +0x28b8")):
                    cand = at(ea + shift)
                    if not cand or cand[2] != 0:
                        continue
                    if cand[1] in ctx:
                        dis = "comment-name-match"
                    elif aliases.get(cand[0], set()) & set(
                            re.findall(r"[A-Za-z_][A-Za-z_0-9]{3,}", ctx)):
                        dis = "comment-alias-match"
                    elif any(int(g, 16) + LEGACY_GHIDRA_SHIFT == cand[0]
                             for g in FUN_NAME.findall(ctx)):
                        dis = "legacy-ghidra-name-cross-check"
                    else:
                        dis = None
                    if here is not None and dis is None:
                        continue               # 歧义且无佐证 -> 不动
                    new = f"@{cand[0]:#x}"
                    changes.append({
                        "file": str(path.relative_to(ROOT)), "line": lineno,
                        "from": m.group(1), "to": f"{cand[0]:#x}",
                        "ida_name": cand[1], "rule": rule,
                        "disambiguation": dis or "no-as-is-function",
                        "text": line.strip()[:160],
                    })
                    return new
                if here is not None or ea >= OBJ1_LO:
                    skipped.append({
                        "file": str(path.relative_to(ROOT)), "line": lineno,
                        "token": m.group(1),
                        "reason": ("落在函数体内部且无候选" if here
                                   else "无法归约到任何函数入口"),
                        "inside": f"{here[1]}+{here[2]:#x}" if here else None,
                        "text": line.strip()[:160],
                    })
                return m.group(0)

            new_line = TOKEN.sub(repl, line)
            touched = touched or new_line != line
            out_lines.append(new_line)
        if touched and not args.dry_run:
            path.write_text("".join(out_lines), encoding="utf-8")

    print(f"改写 {len(changes)} 处 `@0x…` 函数地址"
          f"（涉及 {len({c['file'] for c in changes})} 个文件）")
    by_rule: dict[str, int] = {}
    by_dis: dict[str, int] = {}
    for c in changes:
        by_rule[c["rule"]] = by_rule.get(c["rule"], 0) + 1
        by_dis[c["disambiguation"]] = by_dis.get(c["disambiguation"], 0) + 1
    for k, v in sorted(by_rule.items()):
        print(f"  规则 {k}: {v}")
    for k, v in sorted(by_dis.items()):
        print(f"  消歧 {k}: {v}")
    print(f"保留待人工处理: {len(skipped)}")

    if not args.dry_run:
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps({
            "schema": "fd2-src-address-migration-v1",
            "oracle": str(FUNCS_TSV.relative_to(ROOT)),
            "rules": {
                "legacy-dual": "canonical_VA = legacy - 0x25214",
                "legacy-ghidra": "canonical_VA = FUN_addr + 0x28b8",
            },
            "changed": len(changes), "skipped": len(skipped),
            "changes": changes, "skipped_tokens": skipped,
        }, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"wrote {REPORT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
