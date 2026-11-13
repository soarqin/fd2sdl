"""S2：30个原结果槽／18个完整目标及两个只读helper的Domain来源合同。

由ida-domain执行器提供db；原EXE只读，完整chunk逐字节核严格relocated镜像。
此表是独立原来源读集，不从冷图或C实现产生expected。无数据库保存或GUI。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

from g1_preparation_evidence import function_record, RELOCATED_SHA256

TABLE_VA = 0x51B19
TABLE_RAW_SHA256 = "9d1f34bf7f946a48abab6a104a7e043f58b1779d7a3c29d10d286d55ab8d02eb"
SLOTS = (0x205B4, 0x206C5, *([0x205B4] * 7), 0x20707, 0x205B4,
         0x2073D, 0x20765, 0x205B4, 0x20822, 0x2084A, 0x20872,
         0x208CF, 0x20926, 0x20957, 0x20A51, 0x20A87, 0x20AAF,
         0x205B4, 0x20B14, 0x20B3C, 0x20A87, 0x20A87, 0x20B72, 0x20BF5)
TARGETS = tuple(sorted(set(SLOTS)))
HELPERS = (0x34894, 0x33499)
CHUNKS = {
    0x205B4: ((0x205B4, 0x205DA), (0x2067D, 0x206C5)),
    **{a: ((a, b),) for a, b in zip(TARGETS[1:], TARGETS[2:] + (0x20C6F,))},
    0x34894: ((0x34894, 0x348BB),), 0x33499: ((0x33499, 0x334D9),),
}
# 正常原raw前缀；缺失记录不由current C的安全扩展反推原版语义。
MINIMUM = dict(zip(TARGETS, (1, 11, 52, 15, 60, 65, 66, 53, 53, 65,
                            83, 18, 2, 19, 17, 3, 2, 21)))
EFFECTFUL = (0x20765, 0x20872, 0x20957, 0x20B72, 0x20BF5)
PRESERVING = (0x208CF, 0x20AAF, 0x20B72, 0x20BF5)
FAMILIES = {
    0x205B4: "ordinary_side_raw0", 0x206C5: "ordinary_group_all_hidden",
    **{va: "ordinary_protected_actor_hidden" for va in
       (0x20707, 0x2073D, 0x20822, 0x2084A, 0x20A51, 0x20A87, 0x20B14, 0x20B3C)},
    0x20765: "ordinary_multi_story_override", 0x20872: "ordinary_roster_hidden_story",
    0x20926: "ordinary_signed_turn_hidden", 0x20957: "ordinary_group_victory_override",
    0x208CF: "latched_hidden_victory_override", 0x20AAF: "latched_hidden_victory_override",
    0x20B72: "latched_objective_story_defeat", 0x20BF5: "latched_objective_story_defeat",
}


def source_flow(va):
    if va == 0x205B4:
        return "stage", "stage.result.000"
    if va == 0x206C5:
        return "stage", "stage.result.001"
    domain = "common_event" if va in EFFECTFUL else "stage"
    return domain, f"{'common' if va in EFFECTFUL else 'stage'}.result.source_{va:06x}"


def dispatch_source(executable, ida_directory):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "static_data"))
    from audit_stage_data import read_dispatch
    table = read_dispatch(Path(executable), Path(ida_directory))[0]["stage_runtime_result"]
    if (table["canonical_va"] != hex(TABLE_VA) or table["count"] != 30
            or table["sha256"] != TABLE_RAW_SHA256
            or tuple(int(r["target_va"], 16) for r in table["records"]) != SLOTS):
        raise ValueError("original result fixup table differs")
    return table


def validate_source(image, readset):
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated image identity mismatch")
    if (readset.get("schema") != "fd2-s2-result-domain-v1"
            or readset.get("image_sha256") != RELOCATED_SHA256
            or [f["va"] for f in readset.get("functions", [])] != list(TARGETS + HELPERS)):
        raise ValueError("incomplete or wrong result Domain readset")
    for row in readset["functions"]:
        if not row.get("instructions") or not row.get("blocks") or "callers" not in row or "callees" not in row:
            raise ValueError("missing complete CFG/xref evidence")
        if tuple((c["start"], c["end"]) for c in row.get("chunks", [])) != CHUNKS[row["va"]]:
            raise ValueError("missing original full function/shared chunk")
        for chunk in row["chunks"]:
            a, b = chunk["start"], chunk["end"]
            raw = bytes.fromhex(chunk["raw_hex"])
            if len(raw) != b-a or raw != image[a:b] or hashlib.sha256(raw).hexdigest() != chunk["sha256"]:
                raise ValueError("Domain/strict chunk mismatch")
    return {row["va"]: row for row in readset["functions"]}


def closure_mapping():
    return [{"source_id": f"dispatch:stage_runtime_result:{stage:03d}",
             "stage": stage, "canonical_va": hex(TABLE_VA + 4*stage),
             "target_va": hex(va), "definition_domain": source_flow(va)[0],
             "definition_id": source_flow(va)[1], "minimum_raw_prefix": MINIMUM[va],
             "semantic_family": FAMILIES[va],
             "preserves_latch": va in PRESERVING, "ordered_text": va in EFFECTFUL,
             "new_runtime_binding": False, "binding_gate": "S3 caller/resource closure"}
            for stage, va in enumerate(SLOTS)]


def collect(db, image_path, executable, ida_directory, output):
    image = Path(image_path).read_bytes()
    rows = [function_record(db, va) for va in TARGETS + HELPERS]
    report = {"schema": "fd2-s2-result-domain-v1", "image_sha256": RELOCATED_SHA256,
              "database_saved": False, "functions": rows,
              "result_global_xrefs": [{"from": r.from_ea, "to": r.to_ea, "type": r.type.name}
                                      for r in db.xrefs.to_ea(0x53ECC)],
              "dispatch": dispatch_source(executable, ida_directory),
              "candidate_mapping": closure_mapping(), "original_runtime_executed": False}
    validate_source(image, report)
    if Path(image_path).read_bytes() != image: raise ValueError("strict image changed while collecting")
    output = Path(output)
    if output.exists():
        raise ValueError("output already exists")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"output": str(output), "functions": len(rows), "targets": len(TARGETS),
            "slots": len(SLOTS), "bytes": sum(b-a for spans in CHUNKS.values() for a, b in spans),
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
