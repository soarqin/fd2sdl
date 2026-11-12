"""S0：结果与活动 Load 的完整 Domain 读集；只读，不保存数据库。

依据 canonical 205b4/206c5、10010、117e7 及原 caller。由技能执行器提供
db，复用现有 function_record；原字节/chunk 与严格 relocated 镜像独立核对。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from g1_preparation_evidence import function_record, RELOCATED_SHA256

TARGETS = (0x10010, 0x117E7, 0x12D7B, 0x16F55, 0x19DF7,
           0x205B4, 0x206C5, 0x25BF4, 0x25EBB)


def collect(db, image_path, output):
    image = Path(image_path).read_bytes()
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    records = [function_record(db, va) for va in TARGETS]
    total = 0
    for row in records:
        for chunk in row["chunks"]:
            raw = bytes.fromhex(chunk["raw_hex"])
            if raw != image[chunk["start"]:chunk["end"]]:
                raise ValueError(f"Domain/strict source mismatch: {chunk['start']:#x}")
            total += len(raw)
    refs = [{"from": ref.from_ea, "to": ref.to_ea, "type": ref.type.name}
            for ref in db.xrefs.to_ea(0x53ECC)]
    tail = db.bytes.get_bytes_at(0x22BBE, size=8)
    if tail != image[0x22BBE:0x22BC6]:
        raise ValueError("Load shared return tail differs")
    shared_tail = {"start": 0x22BBE, "end": 0x22BC6, "raw_hex": tail.hex(),
                   "sha256": hashlib.sha256(tail).hexdigest(),
                   "instructions": [{"ea": i.ea, "disasm": db.instructions.get_disassembly(i)}
                                    for i in db.instructions.get_between(0x22BBE, 0x22BC6)]}
    report = {"schema": "fd2-s0-result-domain-v1", "database_saved": False,
              "address_view": "canonical relbase VA; loader-relocated bytes",
              "image_sha256": RELOCATED_SHA256, "functions": records,
              "result_global_xrefs": refs, "strict_byte_match": True,
              "function_bytes": total, "load_return_shared_tail": shared_tail,
              "original_runtime_executed": False}
    output = Path(output)
    if output.exists():
        raise ValueError("output already exists")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"output": str(output), "functions": len(records), "bytes": total,
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
