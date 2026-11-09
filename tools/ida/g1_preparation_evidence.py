#!/usr/bin/env python3
"""G1 准备服务的只读 Domain 取证；VA 来自 canonical 导出及真实 caller。

由 ida-domain-scripting/run.py 提供 db，调用 collect(db, output)。
输出含原机器码／伪 C，只能留本地 ignored 证据目录；不保存数据库。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path


TARGETS = (
    0x26152, 0x2670E, 0x26996, 0x26A0D, 0x26A7A, 0x26B91,
    0x26CE4, 0x26E38, 0x26EDA, 0x275E6, 0x279BC, 0x27D33,
    0x27F4A, 0x28632, 0x2872B, 0x28B41, 0x28CBD, 0x28EFE,
    0x28F65, 0x29DAA, 0x2A07A, 0x2A0C2, 0x2A29D, 0x2A43E,
    0x2A694, 0x2A857, 0x2AA00, 0x2AC7D, 0x2AE0E, 0x2AEDB,
    0x2AF28, 0x2B439, 0x2B4FB, 0x2B67F, 0x2B749, 0x2B777,
    0x2B843, 0x2FB2C, 0x2FE14, 0x4E7DD,
)
RELOCATED_SHA256 = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
INITIALIZATION_TARGETS = (0x1088D, 0x10B4E, 0x10C50)


def verify_bytes(report, image, expected_targets=None):
    """独立验证原有严格 relocated 镜像，拒绝部分读集和伪造成功。"""
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    functions = report["functions"]
    if [row["va"] for row in functions] != list(TARGETS if expected_targets is None else expected_targets):
        raise ValueError("preparation function coverage mismatch")
    byte_count = 0
    for function in functions:
        if not function["chunks"] or not function["instructions"] or not function["blocks"]:
            raise ValueError("incomplete function evidence")
        for chunk in function["chunks"]:
            start, end = chunk["start"], chunk["end"]
            raw = bytes.fromhex(chunk["raw_hex"])
            if not 0x10000 <= start < end <= 0x4EF29 or len(raw) != end - start:
                raise ValueError("invalid canonical code range")
            if image[start:end] != raw or hashlib.sha256(raw).hexdigest() != chunk["sha256"]:
                raise ValueError(f"Domain/strict LE function bytes mismatch at {start:#x}")
            byte_count += len(raw)
    for table in report["tables"]:
        raw = bytes.fromhex(table["raw_hex"])
        if len(raw) != table["size"] or raw != image[table["va"]:table["va"] + table["size"]]:
            raise ValueError("Domain/strict LE table bytes mismatch")
    return {"functions": len(functions), "function_bytes": byte_count,
            "tables": len(report["tables"]), "strict_byte_match": True}


def function_record(db, ea):
    function = db.functions.get_at(ea)
    if function is None or function.start_ea != ea:
        raise ValueError(f"not an exact canonical function entry: {ea:#x}")
    chunks = []
    for chunk in db.functions.get_chunks(function):
        start, end = chunk.start_ea, chunk.end_ea
        raw = db.bytes.get_bytes_at(start, size=end - start)
        if not raw or len(raw) != end - start:
            raise ValueError(f"incomplete function chunk: {start:#x}..{end:#x}")
        chunks.append({"start": start, "end": end, "raw_hex": raw.hex(),
                       "sha256": hashlib.sha256(raw).hexdigest()})
    if not chunks:
        raise ValueError(f"function has no chunks: {ea:#x}")
    blocks = [{"start": block.start_ea, "end": block.end_ea,
               "successors": [successor.start_ea for successor in block.get_successors()]}
              for block in db.functions.get_flowchart(function)]
    instructions = [{"ea": insn.ea,
                     "disasm": db.instructions.get_disassembly(insn)}
                    for insn in db.functions.get_instructions(function)]
    try:
        pseudocode, decompilation_error = str(db.functions.get_pseudocode(function)), None
    except Exception as error:
        pseudocode, decompilation_error = None, str(error)
    return {"va": ea, "end": function.end_ea,
            "name": db.functions.get_name(function), "chunks": chunks,
            "callers": [caller.start_ea for caller in db.functions.get_callers(function)],
            "callees": [callee.start_ea for callee in db.functions.get_callees(function)],
            "blocks": blocks, "instructions": instructions,
            "pseudocode": pseudocode, "decompilation_error": decompilation_error}


def collect(db, output):
    records = [function_record(db, ea) for ea in TARGETS]
    report = {"schema": "fd2-g1-preparation-domain-v1",
              "scope": "static Domain read; not original execution or verified ABI",
              "functions": records,
              "tables": [{"va": va, "size": size, "scope": scope,
                          "raw_hex": db.bytes.get_bytes_at(va, size=size).hex()}
                         for va, size, scope in (
                             (0x52397, 62, "bounded source span before special-item map, not 31 verified fee records"),
                             (0x6238D, 806, "confirmed 26x31 preparation records, including secret-shop selector/scan"),
                             (0x523E7, 30, "stage indexed NoTown flags before next independent data at 0x52405"),
                             (0x52387, 6, "six town-selector portrait IDs, including secret shop"))]}
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"output": str(output), "functions": len(records),
            "blocks": sum(len(row["blocks"]) for row in records),
            "instructions": sum(len(row["instructions"]) for row in records),
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}


def collect_initialization(db, output):
    """跟踪原模板数量／索引读法；完整保留未被原循环消费的资源尾。"""
    report = {"schema": "fd2-g1-initialization-domain-v1",
              "functions": [function_record(db, ea) for ea in INITIALIZATION_TARGETS],
              "tables": [],
              "template_count_xrefs": [{"from": ref.from_ea, "to": ref.to_ea,
                                         "type": ref.type.name}
                                        for ref in db.xrefs.to_ea(0x53BE3)]}
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"output": str(output), "functions": len(report["functions"]),
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", required=True, type=Path)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--initialization", action="store_true")
    args = parser.parse_args()
    result = verify_bytes(json.loads(args.verify.read_text(encoding="utf-8")), args.image.read_bytes(),
                          INITIALIZATION_TARGETS if args.initialization else None)
    result["evidence_sha256"] = hashlib.sha256(args.verify.read_bytes()).hexdigest()
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.json:
        args.json.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
