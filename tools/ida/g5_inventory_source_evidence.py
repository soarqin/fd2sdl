#!/usr/bin/env python3
"""G5 库存来源闭包的只读 Domain 读集；不保存数据库或输出猜测 ABI。

读取两个原记录 owner 的全部 xref／完整函数和五个库存写原语的 caller，
每个 chunk 独立核 strict relocated 镜像；这只是候选来源，不自动证明保持性。
由 ida-domain-scripting 执行器提供 db，调用 collect(db, image_path, output)。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from g1_preparation_evidence import function_record, RELOCATED_SHA256

OWNERS = (0x53A45, 0x53BF7)
WRITERS = (0x10C50, 0x112A5, 0x1B8E7, 0x1BB8C, 0x1C142)
COPY_BOUNDARIES = (0x10010, 0x1088D, 0x11506, 0x2B777)
COUNTS = (0x53BEB, 0x53BFB)


def collect(db, image_path, output):
    image_path, output = Path(image_path), Path(output)
    image = image_path.read_bytes()
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    targets = set(WRITERS + COPY_BOUNDARIES)
    owner_refs = []
    for owner in OWNERS:
        refs = []
        for ref in db.xrefs.to_ea(owner):
            function = db.functions.get_at(ref.from_ea)
            function_va = function.start_ea if function else None
            refs.append({"from": ref.from_ea, "to": ref.to_ea,
                         "type": ref.type.name, "function": function_va})
            if function_va is not None:
                targets.add(function_va)
        owner_refs.append({"va": owner, "refs": refs})
    writer_callers = []
    for writer in WRITERS:
        function = db.functions.get_at(writer)
        if function is None or function.start_ea != writer:
            raise ValueError(f"missing exact inventory writer: {writer:#x}")
        callers = sorted({item.start_ea for item in db.functions.get_callers(function)})
        writer_callers.append({"va": writer, "callers": callers})
        targets.update(callers)
    count_writes = []
    for owner in COUNTS:
        refs = []
        for address in db.xrefs.writes_to_ea(owner):
            function = db.functions.get_at(address)
            if function is None:
                raise ValueError(f"unowned unit count write: {address:#x}")
            refs.append({"from": address, "function": function.start_ea})
            targets.add(function.start_ea)
        count_writes.append({"va": owner, "writes": refs})
    records = [function_record(db, va) for va in sorted(targets)]
    total = 0
    for function in records:
        for chunk in function["chunks"]:
            raw = bytes.fromhex(chunk["raw_hex"])
            if raw != image[chunk["start"]:chunk["end"]]:
                raise ValueError(f"inventory Domain/strict source mismatch: {chunk['start']:#x}")
            total += len(raw)
    report = {"schema": "fd2-g5-inventory-domain-v1",
              "scope": "owner xrefs plus original writer callers; manual alias/write review required",
              "image_sha256": RELOCATED_SHA256, "owner_refs": owner_refs,
              "writer_callers": writer_callers, "count_writes": count_writes,
              "copy_boundaries": list(COPY_BOUNDARIES),
              "functions": records, "strict_byte_match": True,
              "function_bytes": total, "semantic_closure_proven": False,
              "original_runtime_executed": False}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    return {"output": str(output), "functions": len(records), "function_bytes": total,
            "owner_xrefs": [len(item["refs"]) for item in owner_refs],
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
