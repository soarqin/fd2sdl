#!/usr/bin/env python3
"""历史研究原型；当前全量来源合同使用 stage_source_ledger.py，不生成玩法。

依据 audit_stage_data.py 已确认的 FDFIELD 读集与当前冷定义分类；未知项
逐项保留，不根据相同 callback 目标或邻近 CALL 推导控制流。输出仅作本地证据。
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path


def normalize_slot(stage, kind, slot, source_sha256):
    """只转换来源坐标／已有解码字段，不修改原有支持状态。"""
    metadata = stage["metadata"]
    raw = bytes.fromhex(metadata["raw_hex"])
    size = 2 if kind == "lookup" else 3
    at = slot["entry_offset"]
    expected_at = {"turn": 3, "lookup": 0x33, "cell_action": 0x53}[kind] + slot["slot"] * size
    if (hashlib.sha256(raw).hexdigest() != metadata["sha256"]
            or at != expected_at or not 0 <= slot["slot"] < 16 or len(raw[at:at + size]) != size
            or raw[at:at + size].hex() != slot["raw_hex"]):
        raise ValueError("原槽坐标或字节与 metadata 不一致")
    identity = f"FDFIELD.DAT:entry={metadata['entry']}:offset={at}:length={size}"
    classification = slot.get("classification", "unused_lookup")
    if classification not in ("supported_definition", "decoded_unopened", "unmapped_semantics", "unused_lookup"):
        raise ValueError("未知来源分类")
    target = slot.get("original_callback")
    decoded = {key: slot[key] for key in ("turn", "action", "phase", "lookup_id", "match_arg", "mode", "param_u16") if key in slot}
    return {"id": identity, "stage": stage["stage"], "kind": kind, "slot": slot["slot"],
            "source": {"file": "FDFIELD.DAT", "file_sha256": source_sha256,
                       "entry": metadata["entry"], "entry_sha256": metadata["sha256"],
                       "entry_offset": at, "file_offset": metadata["file_offset"] + at, "length": size,
                       "read_sha256": hashlib.sha256(raw[at:at + size]).hexdigest()},
            "decoded": decoded, "classification": classification,
            "definition": slot.get("definition"), "runtime_id": slot.get("runtime_id"),
            "callback_target_va": target["target_va"] if target else None,
            "default_turn255_candidate": slot.get("default_fill", False),
            "referenced_by_map": slot.get("referenced_by_map"),
            "unresolved_reason": slot.get("unresolved_reason"),
            "runtime_enablement_changed": False}


def summarize(report):
    if (report.get("structural_success") is not True or report.get("semantic_complete") is not False
            or report["claims"]["original_runtime_executed"] or report["claims"]["new_gameplay_enabled"]):
        raise ValueError("需要独立批量审计的结构报告；不能把研究转换标为玩法通过")
    records = []
    for stage in report["stages"]:
        meta = stage["metadata"]
        for kind, key in (("turn", "turn_slots"), ("lookup", "cell_lookup_slots"), ("cell_action", "cell_action_slots")):
            if len(meta[key]) != 16:
                raise ValueError("原16槽记录不完整")
            records.extend(normalize_slot(stage, kind, row, report["sources"]["FDFIELD.DAT"]["sha256"]) for row in meta[key])
    if len({row["id"] for row in records}) != len(records):
        raise ValueError("重复原槽身份")
    callback_refs = [row for row in records if row["callback_target_va"]]
    callback_groups = defaultdict(list)
    for row in callback_refs:
        callback_groups[row["callback_target_va"]].append(row["id"])
    cross_stage = []
    for table, value in report["dispatch_tables"].items():
        if table not in ("stage_runtime_result", "stage_entry", "stage_victory"):
            continue
        grouped = defaultdict(list)
        for row in value["records"]:
            grouped[row["target_va"]].append(row["slot"])
        cross_stage.extend({"table": table, "target_va": target, "original_slots": slots,
                            "semantic_conversion_approved": False}
                           for target, slots in grouped.items() if len(slots) > 1)
    return {"schema_version": 1, "research_only": True, "semantic_complete": False,
            "sources": report["sources"],
            "summary": {"resource_groups": len(report["stages"]),
                        "normalized_source_slots": len(records),
                        "classification_by_record": dict(Counter(row["classification"] for row in records)),
                        "record_kind_counts": dict(Counter(row["kind"] for row in records)),
                        "callback_slot_references": len(callback_refs),
                        "different_referenced_callback_targets": len(callback_groups),
                        "map_cells": sum(s["map"]["width"] * s["map"]["height"] for s in report["stages"]),
                        "event_marked_map_cells": sum(len(s["map"]["event_cells"]) for s in report["stages"]),
                        "stage_text_fragments_directory_checked": sum(s["text_reference"]["fragment_count"] for s in report["stages"]),
                        "dispatch_different_targets": {name: len({r["target_va"] for r in value["records"]}) for name, value in report["dispatch_tables"].items()},
                        "existing_audit_potential_requirements": report["summary"]["active_classification_counts"],
                        "new_semantic_conversions": 0, "new_runtime_bindings": 0},
            "cross_stage_dispatch_reuse_candidates": cross_stage,
            "normalized_source_records": records,
            "not_checked": ["text token/portrait/glyph validity for all fragments", "terrain sheet RLE/frame decoding for all stages",
                            "new callback CFG/control-flow equivalence", "dynamic runtime behavior", "all resource lifetime/presentation paths"]}


def result_reuse_candidates(report, definitions):
    """仅投影已审查205b4完整函数的同目标槽，不从目标附近指令猜规则。"""
    flow_id = "stage.result.000"
    flow = next((flow for flow in definitions["flows"] if flow["id"] == flow_id), None)
    if flow is None:
        raise ValueError("缺少既有结果流程")
    flow_hash = hashlib.sha256(json.dumps(flow, sort_keys=True).encode()).hexdigest()
    table = report["dispatch_tables"]["stage_runtime_result"]
    function = report["function_catalog"]["0x205b4"]
    chunks = [{key: row[key] for key in ("canonical_va", "length", "sha256")} for row in function["chunks"]]
    result = []
    for row in table["records"]:
        if row["target_va"] != "0x205b4":
            continue
        if row["ida_owners"] != ["0x205b4"] or not chunks:
            raise ValueError("结果流程候选缺少完整函数owner")
        result.append({"runtime_id": f"stage.result.{row['slot']:03d}", "definition": flow_id,
                       "definition_sha256": flow_hash, "semantic_conversion_approved": False,
                       "status": "existing_definition" if row["slot"] == 0 else "candidate",
                       "source": {"file": "FD2.EXE", "file_sha256": report["sources"]["FD2.EXE"]["sha256"],
                                  "table_canonical_va": table["canonical_va"], "table_sha256": table["sha256"],
                                  "original_slot": row["slot"], "source_canonical_va": row["source_va"], "length": 4,
                                  "read_sha256": hashlib.sha256(bytes.fromhex(row["raw_hex"])).hexdigest(),
                                  "target_va": row["target_va"], "ida_owners": row["ida_owners"], "function_chunks": chunks},
                       "enablement_blocked_by": ["stage-specific resources and entry/callback/victory closure",
                                                "existing S0 result contract does not prove a new exact caller/parameter/resource binding",
                                                "result effective-domain fixtures and source trajectory checks"],
                       "shipping_definition_changed": False})
    return result


def validate_output(audit_path, output_path, root):
    output = output_path.resolve()
    if output == audit_path.resolve():
        raise ValueError("研究输出不能覆盖输入审计证据")
    if not output.is_relative_to((root / "build/research-stage-batch").resolve()):
        raise ValueError("研究报告只允许写入 build/research-stage-batch/")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audit", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    # 固定研究输出范围，避免误覆盖原资源、正式定义或文档。
    try:
        validate_output(args.audit, args.output, root)
    except ValueError as error:
        parser.error(str(error))
    raw = args.audit.read_bytes()
    report = json.loads(raw)
    result = summarize(report)
    definition_path = root / "data/static/flows/stage.json"
    definition_raw = definition_path.read_bytes()
    result["definition_input"] = {"file": "data/static/flows/stage.json", "sha256": hashlib.sha256(definition_raw).hexdigest()}
    result["result_binding_proposals"] = result_reuse_candidates(report, json.loads(definition_raw))
    result["summary"]["result_binding_proposals"] = len(result["result_binding_proposals"])
    result["summary"]["result_new_binding_candidates"] = sum(row["status"] == "candidate" for row in result["result_binding_proposals"])
    result["audit_input_sha256"] = hashlib.sha256(raw).hexdigest()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result["summary"], ensure_ascii=False))


if __name__ == "__main__":
    main()
