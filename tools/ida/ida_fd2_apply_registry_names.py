#!/usr/bin/env python3
"""在真实IDA数据库同步已确认registry/CORRECTIONS；不设函数类型，不生成替换伪C。"""
from __future__ import annotations
import idapro  # idalib requires this before IDA/Domain imports.
import argparse
import json
import re
from pathlib import Path
from ida_domain import Database
from ida_domain.database import IdaCommandOptions
from build_fd2_name_registry import CORRECTIONS
ROOT = Path(__file__).resolve().parents[2]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idb", type=Path, required=True)
    parser.add_argument("--registry", type=Path, default=ROOT / "docs/generated/ida-canonical/fd2-ida-function-registry.json")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    entries = {row["va"]: row for row in json.loads(args.registry.read_text(encoding="utf-8"))["entries"]}
    wanted = {va: row["ida_name"] for va, row in entries.items()}
    for va, rows in CORRECTIONS.items():
        for row in rows:
            if row["target"] == "semantic_name" and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", row["now"]):
                wanted[va] = row["now"]
    report = {"method": "IDA Domain functions.set_name/set_comment; no prototypes or instruction writes", "verified": [], "renamed": [], "commented": []}
    with Database.open(str(args.idb), IdaCommandOptions(auto_analysis=False), save_on_close=True) as db:
        # All preconditions precede the first write. Unknown boundaries are never forced.
        for va, name in wanted.items():
            function = db.functions.get_at(va)
            if va == 0x3702f or not function or function.start_ea != va:
                raise ValueError(f"不是允许命名的函数入口: {va:#x}")
            old = entries.get(va)
            if old and function.end_ea - function.start_ea != old["size"]:
                raise ValueError(f"函数边界变化，先人工复核: {va:#x}")
            if old and old.get("is_lib") and db.functions.get_name(function) != name:
                raise ValueError(f"FLIRT库符号冲突，保留原符号: {va:#x}")
            report["verified"].append({"va": hex(va), "name": name, "size": function.end_ea - function.start_ea})
        stack_check = db.functions.get_at(0x3702f)
        if db.functions.get_name(stack_check) != "watcom_stack_check" or db.functions.get_signature(stack_check):
            raise ValueError("3702f必须保持中性名且无显式／猜测原型")
        for va, name in wanted.items():
            function = db.functions.get_at(va)
            old_name = db.functions.get_name(function)
            if old_name != name:
                if not db.functions.set_name(function, name, auto_correct=False):
                    raise RuntimeError(f"IDA命名失败: {va:#x} {name}")
                report["renamed"].append({"va": hex(va), "from": old_name, "to": name})
            comment = db.functions.get_comment(function, repeatable=True) or ""
            previous = comment
            for row in CORRECTIONS.get(va, []):
                if row["now"] not in comment:
                    comment += "\n[勘误 " + row["target"] + "] " + row["now"] + "；依据: " + row["why"]
            if comment != previous:
                if not db.functions.set_comment(function, comment.strip(), repeatable=True):
                    raise RuntimeError(f"IDA注释失败: {va:#x}")
                report["commented"].append(hex(va))
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8", newline="\n")
    print(f"verified={len(report['verified'])} renamed={len(report['renamed'])} commented={len(report['commented'])}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
