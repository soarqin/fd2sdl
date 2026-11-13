"""S2 additive DAC／BIOS tick等待的完整Domain来源与共享返回尾。"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from g1_preparation_evidence import function_record, RELOCATED_SHA256

CHUNKS = {
    0x11DF2: ((0x11DF2, 0x11EB0, "60a949e31a64bec1dc8eed0c8a16fe7a152b208f65de037bb4407a58c8ee0759"),),
    0x17AA9: ((0x17AA9, 0x17AED, "4c699109f96bc0134c6f7039bc12325289b5f1b0dea02c60c46cd35d61091e71"),),
    0x35B78: ((0x35B78, 0x35BEE, "e07aeed51654a8659008a4d7c1e1fac48315c6908cc11454e111937a8629cdad"),
              (0x35722, 0x35730, "155dcc8dd8bb68171c183c698adec974e8e8d7fbf1b1e57ae9b6eca65fa46da3")),
    0x361B0: ((0x361B0, 0x36214, "153dd784492e0c360924fd844796121f283c31a5fdcb9c63411bba7f0a6645cb"),),
}
TAIL = (0x1951F, 0x19523)
TAIL_RAW = bytes.fromhex("5f5e5bc3")


def validate_source(image, readset):
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    if (readset.get("schema") != "fd2-s2-visual-domain-v1"
            or readset.get("image_sha256") != RELOCATED_SHA256
            or [f["va"] for f in readset.get("functions", [])] != list(CHUNKS)):
        raise ValueError("wrong visual Domain readset")
    for row in readset["functions"]:
        spans = CHUNKS[row["va"]]
        if tuple((c["start"], c["end"], c["sha256"]) for c in row.get("chunks", [])) != spans:
            raise ValueError("missing full visual function/shared chunk")
        if not row.get("instructions") or not row.get("blocks") or "callers" not in row:
            raise ValueError("missing complete visual CFG/xrefs")
        for chunk in row["chunks"]:
            a, b = chunk["start"], chunk["end"]
            raw = bytes.fromhex(chunk["raw_hex"])
            if raw != image[a:b] or hashlib.sha256(raw).hexdigest() != chunk["sha256"]:
                raise ValueError("visual Domain/strict source mismatch")
    tail = readset.get("palette_shared_return", {})
    if ((tail.get("start"), tail.get("end")) != TAIL or tail.get("raw_hex") != TAIL_RAW.hex()
            or image[TAIL[0]:TAIL[1]] != TAIL_RAW or not tail.get("instructions")):
        raise ValueError("missing palette shared return tail")
    xrefs = readset.get("tick_scratch_xrefs", [])
    if [(r["from"], r["type"]) for r in xrefs] != [(0x17ABF, "WRITE"), (0x17ACC, "READ"), (0x17AE7, "WRITE")]:
        raise ValueError("tick scratch has an unaccounted external owner")
    return {row["va"]: row for row in readset["functions"]}


def collect(db, image_path, output):
    image = Path(image_path).read_bytes()
    tail = db.bytes.get_bytes_at(TAIL[0], size=TAIL[1]-TAIL[0])
    report = {"schema": "fd2-s2-visual-domain-v1", "database_saved": False,
              "image_sha256": RELOCATED_SHA256,
              "functions": [function_record(db, va) for va in CHUNKS],
              "palette_shared_return": {"start": TAIL[0], "end": TAIL[1], "raw_hex": tail.hex(),
                  "owner_va": 0x190AC, "instructions": [{"ea": i.ea, "disasm": db.instructions.get_disassembly(i)}
                  for i in db.instructions.get_between(*TAIL)]},
              "tick_scratch_xrefs": [{"from": r.from_ea, "type": r.type.name} for r in db.xrefs.to_ea(0x53A2C)],
              "original_runtime_executed": False}
    validate_source(image, report)
    if Path(image_path).read_bytes() != image: raise ValueError("strict image changed while collecting")
    output = Path(output)
    if output.exists(): raise ValueError("output already exists")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    return {"output": str(output), "functions": len(CHUNKS), "bytes": 494,
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
