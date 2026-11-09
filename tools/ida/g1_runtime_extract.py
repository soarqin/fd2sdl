"""G1 共享控制／视觉原义的只读 Domain API 证据提取。

由 ida-domain-scripting/run.py 包装后提供 db；输出路径由临时 driver 传入。
只导出指令、函数块及真实数据 xref，不修改命名、类型或数据库。
地址均为 canonical relbase VA，object1 的 code0 = VA - 0x10000。
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


FUNCTIONS = (
    0x10010, 0x10620, 0x10652, 0x1088D, 0x117E7, 0x11AA8, 0x11CAC, 0x11EEE,
    0x127A9, 0x127E0, 0x1297D, 0x15F84, 0x164E8, 0x16559,
    0x165AC, 0x16C57, 0x1728C, 0x1A30B, 0x1ACF3, 0x205DA, 0x25BF4,
    0x12EAA, 0x1300D, 0x13185, 0x135DD, 0x1366A,
    0x3231B, 0x32999, 0x4E22A, 0x4E310, 0x4E31C,
)

GLOBALS = (
    0x51AAB, 0x51AAC, 0x53AF9, 0x53AE9, 0x53A04, 0x53A08,
    0x53A10, 0x53A14, 0x53A49, 0x53C07, 0x53C0B, 0x53C0F,
    0x60000, 0x60002,
)


def function_evidence(db, ea):
    function = db.functions.get_at(ea)
    if function is None or function.start_ea != ea:
        raise ValueError(f"not an exact function entry: {ea:#x}")
    chunks = [
        {"start": chunk.start_ea, "end": chunk.end_ea}
        for chunk in db.functions.get_chunks(function)
    ]
    instructions = []
    for insn in db.functions.get_instructions(function):
        raw = db.bytes.get_bytes_at(insn.ea, size=insn.size)
        if raw is None or len(raw) != insn.size:
            raise ValueError(f"unreadable instruction: {insn.ea:#x}")
        instructions.append({
            "va": insn.ea,
            "bytes": bytes(raw).hex(),
            "assembly": db.instructions.get_disassembly(insn),
        })
    return {
        "va": ea,
        "name": db.functions.get_name(function),
        "end": function.end_ea,
        "chunks": chunks,
        "sha256_instruction_bytes": hashlib.sha256(
            b"".join(bytes.fromhex(item["bytes"]) for item in instructions)
        ).hexdigest(),
        "callers": sorted(f.start_ea for f in db.functions.get_callers(function)),
        "callees": sorted(f.start_ea for f in db.functions.get_callees(function)),
        "instructions": instructions,
    }


def global_evidence(db, ea):
    references = []
    for reference in db.xrefs.to_ea(ea):
        if reference.is_flow:
            continue
        function = db.functions.get_at(reference.from_ea)
        instruction = db.instructions.get_at(reference.from_ea)
        references.append({
            "from": reference.from_ea,
            "function": function.start_ea if function else None,
            "type": reference.type.name,
            "read": reference.is_read,
            "write": reference.is_write,
            "assembly": db.instructions.get_disassembly(instruction) if instruction else None,
        })
    return {
        "va": ea,
        "initial_bytes": bytes(db.bytes.get_bytes_at(ea, size=4)).hex(),
        "references": sorted(references, key=lambda item: item["from"]),
    }


def verify_report(report, executable):
    """以既有严格 LE 解析器和合法 fixup 在内存中逐指令独立对照。"""
    import sys
    root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(root / "tools"))
    from rebuild_fd2_analysis import (
        parse_le, parse_objects, build_relbase_image, parse_fixups, apply_fixups,
    )
    from static_data.verify_unit_tables import EXE_SHA256

    source = Path(executable).read_bytes()
    if hashlib.sha256(source).hexdigest() != EXE_SHA256:
        raise ValueError("FD2.EXE identity mismatch")
    le = parse_le(source)
    objects = parse_objects(source, le)
    raw = build_relbase_image(source, le, objects)
    fixups = parse_fixups(source, le, objects)
    relocated = apply_fixups(raw, fixups)
    result = verify_image(report, relocated)
    return {
        **result, "executable_sha256": EXE_SHA256,
        "kind": "static-bytes-not-machine-execution",
    }


def verify_image(report, relocated):
    """拒绝缺项、错字节、越界或错误摘要；不把报告自报摘要当 oracle。"""
    if [item["va"] for item in report["functions"]] != list(FUNCTIONS):
        raise ValueError("function set/order mismatch")
    if [item["va"] for item in report["globals"]] != list(GLOBALS):
        raise ValueError("global set/order mismatch")
    total = 0
    for function in report["functions"]:
        if not function["instructions"]:
            raise ValueError("empty function instructions")
        data = bytearray()
        previous_end = -1
        for instruction in function["instructions"]:
            va = instruction["va"]
            value = bytes.fromhex(instruction["bytes"])
            if not value or va < previous_end or not any(
                chunk["start"] <= va < va + len(value) <= chunk["end"]
                for chunk in function["chunks"]
            ):
                raise ValueError(f"instruction extent/order invalid: {va:#x}")
            if value != relocated[va:va + len(value)]:
                raise ValueError(f"LE/IDA instruction bytes mismatch: {va:#x}")
            previous_end = va + len(value)
            data.extend(value)
        if hashlib.sha256(data).hexdigest() != function["sha256_instruction_bytes"]:
            raise ValueError("function byte digest mismatch")
        if len(data) != sum(chunk["end"] - chunk["start"] for chunk in function["chunks"]):
            raise ValueError("function chunks not completely consumed")
        total += len(data)
    for item in report["globals"]:
        va = item["va"]
        if bytes.fromhex(item["initial_bytes"]) != relocated[va:va + 4]:
            raise ValueError(f"global initial bytes mismatch: {va:#x}")
    return {"functions": len(FUNCTIONS), "instruction_bytes": total, "globals": len(GLOBALS)}


def extract(db, output):
    """对独立数据库取证；输出失败不会留下可冒称成功的 JSON。"""
    report = {
        "schema": "fd2-g1-runtime-domain-v1",
        "address_view": "canonical-relbase-va",
        "database_saved": False,
        "functions": [function_evidence(db, ea) for ea in FUNCTIONS],
        "globals": [global_evidence(db, ea) for ea in GLOBALS],
    }
    path = Path(output)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report
