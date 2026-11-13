#!/usr/bin/env python3
"""保留G1来源族投影，另列S1当前caller语义；结构成功不提升为玩法完整。

原字节、fixup、资源和发布证据校验复用 audit_stage_data；本层只消费
显式review的VA／chunk摘要和来源ID；当前descriptor路由复用已证caller规则。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import audit_stage_data as source_audit

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REVIEW = ROOT / "docs/reverse-engineering/common-scope-review.json"
EXPECTED_COUNTS = {"dispatch": 208, "references": 1171, "movement": 106, "placement_tails": 12}


def review_index(review):
    if review.get("schema") != "fd2-common-scope-review-v1":
        raise ValueError("unknown semantic review schema")
    for family in review["families"].values():
        state, next_stage = family.get("state"), family.get("next")
        if state not in ("known_rule", "common_gap", "unresolved") or next_stage not in (None, "S", "G2", "G3", "G4"):
            raise ValueError("invalid semantic family state/next")
        if state == "known_rule" and next_stage != "S" or state == "common_gap" and next_stage not in ("G2", "G3", "G4"):
            raise ValueError("inconsistent semantic family state/next")
    index = {}
    for group in review["target_groups"]:
        if not group.get("basis") or not group.get("families"):
            raise ValueError("semantic group lacks review basis or family")
        for family in group["families"]:
            if family not in review["families"]:
                raise ValueError("unknown semantic family")
        for target in group["targets"]:
            key = (group["table"], target)
            if key in index:
                raise ValueError("duplicate reviewed target")
            index[key] = group
    return index


def family_result(families, review):
    families = sorted(set(families))
    if not families or any(family not in review["families"] for family in families):
        return {"classification": "unresolved", "families": families, "next": [],
                "reason": "no reviewed semantic family"}
    if any(review["families"][family]["state"] == "unresolved" for family in families):
        return {"classification": "unresolved", "families": families, "next": [],
                "reason": "semantic family is explicitly unresolved"}
    gaps = sorted({entry["next"] for family in families
                   if (entry := review["families"][family])["state"] == "common_gap"})
    return {"classification": "common_gap" if gaps else "binding_only",
            "families": families, "next": gaps or ["S"]}


def classify_callback(action, dispatch, review):
    # turn/lookup是BYTE，descriptor callback为完整WORD，不能在此截断alias。
    if type(action) is not int or not 0 <= action <= 65535:
        return {"classification": "unresolved", "families": [], "next": [], "reason": "invalid action domain"}
    if action >= 90:
        fact = review.get("callback_out_of_table")
        if not fact or action not in fact["reviewed_actions"]:
            return {"classification": "unresolved", "families": [], "next": [],
                    "reason": "callback index outside the reviewed table"}
        semantic = family_result(["callback_guard"], review)
        if semantic["classification"] == "unresolved":
            return semantic
        return {"classification": "original_out_of_table", "families": ["callback_guard"],
                "next": ["G4"], "reason": fact["basis"]}
    if action >= len(dispatch):
        return {"classification": "unresolved", "families": [], "next": [],
                "reason": "callback slot lacks a verified source table record"}
    return {**dispatch[action]["semantic"], "dispatch_source_id": dispatch[action]["source_id"]}


def classify_descriptor(descriptor, dispatch, review, *, cell=False):
    """冻结G1投影；旧低BYTE/cell归属仅用于保持历史报告可比。"""
    mode = descriptor["mode"]
    if cell and mode not in (0, 1):
        return {"classification": "unresolved", "families": [], "next": [],
                "reason": "drop descriptor semantics do not prove a special-cell mode"}
    if mode == 2:
        return classify_callback(descriptor["param_u16"] & 255, dispatch, review)
    if mode in (0, 1, 3):
        if descriptor.get("reference_errors"):
            return {"classification": "unresolved", "families": [], "next": [],
                    "reason": "descriptor has unresolved source reference"}
        return family_result(["loot_known"], review)
    fact = review.get("descriptor_default")
    if fact and mode in fact["reviewed_modes"]:
        semantic = family_result(["descriptor_default"], review)
        if semantic["classification"] == "unresolved":
            return semantic
        return {"classification": "original_default_noop", "families": ["descriptor_default"],
                "next": ["G2"], "reason": fact["basis"]}
    return {"classification": "unresolved", "families": [], "next": [], "reason": "unreviewed descriptor mode"}


def current_descriptor_semantic(descriptor, dispatch, review, *, cell=False):
    """S1按已证caller分流；family/next仍是G1审查标签，不是当前实施状态。"""
    caller = descriptor.get("caller", "cell_execute" if cell else "defeated_collector")
    route = source_audit.descriptor_semantics(descriptor["mode"], descriptor["param_u16"], caller=caller)
    kind = route["semantic_kind"]
    if kind == "callback":
        semantic = classify_callback(route["callback_u16"], dispatch, review)
    elif descriptor.get("reference_errors"):
        semantic = {"classification": "unresolved", "families": [], "next": [],
                    "reason": "descriptor has unresolved source reference"}
    elif kind in ("item", "money", "story"):
        semantic = family_result(["loot_known"], review)
    elif kind == "default_noop":
        semantic = family_result(["descriptor_default"], review)
        if semantic["classification"] != "unresolved":
            semantic = {**semantic, "classification": "original_default_noop"}
    elif kind in ("collector_sentinel", "collector_filtered"):
        semantic = family_result(["loot_known"], review)
        if semantic["classification"] != "unresolved":
            semantic = {**semantic, "classification": "original_collector_excluded"}
    else:
        semantic = {"classification": "unresolved", "families": [], "next": [],
                    "reason": "descriptor route lacks reviewed primitive-family mapping"}
    return {**semantic, **route, "family_review_epoch": "G1_not_current_implementation",
            "runtime_classification": descriptor.get("classification")}


def build_ledger(inventory, review):
    if inventory.get("sources", {}).get("FD2.EXE", {}).get("sha256") != review["exe_sha256"]:
        raise ValueError("semantic review and EXE identity differ")
    for name, expected in review["source_sha256"].items():
        if inventory["sources"].get(name, {}).get("sha256") != expected:
            raise ValueError("semantic review and resource identity differ")
    reviewed_chunks = {(row["canonical_va"], row["length"], row["sha256"])
                       for chunks in review["function_chunks"].values() for row in chunks}
    for va, chunks in review["function_chunks"].items():
        actual = inventory["function_catalog"].get(va, {}).get("chunks")
        projection = lambda rows: [(row["canonical_va"], row["length"], row["sha256"]) for row in rows]
        # 已核字节的共享尾可补给另一个owner；命名／合法共享归属变更不改语义。
        if (actual is None or not set(projection(chunks)).issubset(projection(actual))
                or not set(projection(actual)).issubset(reviewed_chunks)):
            raise ValueError(f"reviewed function bytes/chunks changed: {va}")
    targets = review_index(review)
    rows, dispatch_by_table = [], {}
    for table, data in inventory["dispatch_tables"].items():
        if data["sha256"] != review["dispatch_sha256"].get(table):
            raise ValueError(f"reviewed dispatch table changed: {table}")
        dispatch_by_table[table] = []
        for source in data["records"]:
            group = targets.get((table, source["target_va"]))
            target_va = int(source["target_va"], 16)
            # 目标可位于另一个函数的shared tail，不要求target本身是函数入口。
            backed = any(int(chunk["canonical_va"], 16) <= target_va < int(chunk["canonical_va"], 16) + chunk["length"]
                         for owner in source.get("ida_owners", [])
                         for chunk in review["function_chunks"].get(owner, []))
            semantic = (family_result(group["families"], review) if group and backed else
                        {"classification": "unresolved", "families": [], "next": [],
                         "reason": "target lacks explicit review or verified containing owner chunk"})
            row = {"source_id": f"dispatch:{table}:{source['slot']:03d}", "kind": "dispatch",
                   "source_va": source["source_va"], "target_va": source["target_va"],
                   "semantic": semantic}
            rows.append(row)
            dispatch_by_table[table].append(row)
    callbacks = dispatch_by_table["event_callback"]
    for stage in inventory["stages"]:
        identity = stage["stage"]
        metadata = stage["metadata"]
        for turn in metadata["turn_slots"]:
            rows.append({"source_id": f"stage:{identity:02d}:turn:{turn['slot']:02d}", "kind": "references",
                         "source_entry": metadata["entry"], "source_offset": turn["entry_offset"],
                         "legacy_classification": turn["classification"],
                         "semantic": classify_callback(turn["action"], callbacks, review)})
        for lookup in metadata["cell_lookup_slots"]:
            if lookup["action"] != 255:
                rows.append({"source_id": f"stage:{identity:02d}:lookup:{lookup['slot']:02d}", "kind": "references",
                             "source_entry": metadata["entry"], "source_offset": lookup["entry_offset"],
                             "legacy_classification": lookup["classification"],
                             "semantic": classify_callback(lookup["action"], callbacks, review)})
        for action in metadata["cell_action_slots"]:
            if action["referenced_by_map"]:
                rows.append({"source_id": f"stage:{identity:02d}:cell:{action['slot']:02d}", "kind": "references",
                             "source_entry": metadata["entry"], "source_offset": action["entry_offset"],
                             "legacy_classification": action.get("legacy_classification", action["classification"]),
                             "semantic": classify_descriptor(action, callbacks, review, cell=True),
                             "current_semantic": current_descriptor_semantic(action, callbacks, review, cell=True)})
        for template in metadata["templates"]:
            index, drop = template["index"], template["drop"]
            if drop["classification"] != "unused_descriptor":
                rows.append({"source_id": f"stage:{identity:02d}:drop:{index:03d}", "kind": "references",
                             "source_entry": metadata["entry"], "source_offset": template["entry_offset"] + 22,
                             "legacy_classification": drop.get("legacy_classification", drop["classification"]),
                             "semantic": classify_descriptor(drop, callbacks, review),
                             "current_semantic": current_descriptor_semantic(drop, callbacks, review)})
            if template["placement_index"] is None:
                key = f"stage:{identity:02d}:template:{index:03d}"
                fact = review["placement_tails"].get(key)
                confirmed = fact and metadata["unknown_02"] == fact["effective_template_count"] and index >= metadata["unknown_02"]
                rows.append({"source_id": key, "kind": "placement_tails", "source_entry": metadata["entry"],
                             "source_offset": template["entry_offset"],
                             "semantic": ({**family_result(["indexed_initialization"], review),
                                            "original_consumption": "outside_effective_group_prefix",
                                            "reason": fact["basis"]} if confirmed else
                                           {"classification": "unresolved", "families": [], "next": [],
                                            "reason": "unpaired template lacks reviewed count boundary"})})
        for kind, source in stage["dispatch"].items():
            if source["original"]:
                table = {"entry": "stage_entry", "result": "stage_runtime_result", "victory": "stage_victory"}[kind]
                target = dispatch_by_table[table][identity]
                rows.append({"source_id": f"stage:{identity:02d}:{kind}", "kind": "references",
                             "legacy_classification": source["classification"],
                             "semantic": {**target["semantic"], "dispatch_source_id": target["source_id"]}})
    movement = {row["id"]: row for row in review["movement"]}
    if len(movement) != len(review["movement"]):
        raise ValueError("duplicate movement review")
    for source in inventory["movement_pointer_table"]["records"]:
        fact = movement.get(source["id"])
        observed = source["counted_structure_observation"]
        confirmed = fact and fact["source_va"] == source["target_va"] and fact["counted_length"] == observed["consumed_length"]
        if confirmed:
            if fact["state"] == "common_operand_gap":
                semantic = family_result(["movement_operand"], review)
            elif fact["state"] == "existing_supported":
                semantic = family_result(["movement_known"], review)
                if semantic["classification"] != "unresolved":
                    semantic = {"classification": "existing_supported", "families": ["movement_known"], "next": []}
            elif fact["state"] == "known_rule_new_binding":
                semantic = family_result(["movement_known"], review)
            else:
                confirmed = False
        if not confirmed:
            semantic = {"classification": "unresolved", "families": [], "next": [],
                        "reason": "movement source lacks matching semantic review"}
        rows.append({"source_id": f"movement:{source['id']:03d}", "kind": "movement",
                     "source_va": source["pointer_va"], "target_va": source["target_va"], "semantic": semantic})
    if len({row["source_id"] for row in rows}) != len(rows):
        raise ValueError("duplicate source ID in common ledger")
    counts = dict(Counter(row["kind"] for row in rows))
    if counts != EXPECTED_COUNTS:
        raise ValueError(f"source ledger scope differs: {counts}")
    for row in rows:
        row.setdefault("current_semantic", dict(row["semantic"]))
    unknown = [row["source_id"] for row in rows if row["semantic"]["classification"] == "unresolved"]
    current_unknown = [row["source_id"] for row in rows if row["current_semantic"]["classification"] == "unresolved"]
    return {"schema": "fd2-common-scope-ledger-v1", "structural_success": inventory["structural_success"],
            "classification_complete": not unknown, "all_game_semantic_complete": False,
            "new_gameplay_enabled": False, "original_runtime_executed": False,
            "current_phase": "S1", "historical_fields": "semantic, classification_counts and classification_complete are frozen_G1_projection",
            "projection_scope": "frozen G1 potential-requirement subset; not the complete S1 resource ledger",
            "family_label_boundary": "common_gap, binding_only and next in both projections are G1 review labels, not current gaps, completion or priority",
            "boundary": "reviewed primitive-family classification; implementation, extraction of every flow edge and runtime acceptance remain separate gates",
            "counts": counts, "classification_counts": dict(Counter(row["semantic"]["classification"] for row in rows)),
            "current_classification_complete": not current_unknown,
            "current_classification_counts": dict(Counter(row["current_semantic"]["classification"] for row in rows)),
            "current_unresolved_source_ids": current_unknown,
            "unresolved_source_ids": unknown, "sources": inventory["sources"], "rows": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--review", type=Path, default=DEFAULT_REVIEW)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-classified", action="store_true")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        if output.exists():
            raise ValueError("ledger output already exists; keep historical report and choose a new path")
        if any(output.is_relative_to(path.resolve()) for path in (args.original, ROOT / "data", ROOT / "docs")):
            raise ValueError("ledger output must not overwrite resources, definitions or documents")
        if output == args.review.resolve():
            raise ValueError("ledger output must not overwrite its semantic review input")
        reviewed_bytes = args.review.read_bytes()
        review = json.loads(reviewed_bytes)
        ledger = build_ledger(source_audit.audit(args.original), review)
        if args.review.read_bytes() != reviewed_bytes:
            raise ValueError("semantic review changed while auditing")
        ledger["review_sha256"] = hashlib.sha256(reviewed_bytes).hexdigest()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(ledger, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({key: ledger[key] for key in ("classification_complete", "current_classification_complete", "counts",
                                                     "classification_counts", "current_classification_counts")}, ensure_ascii=False))
        if args.require_classified and not (ledger["classification_complete"] and ledger["current_classification_complete"]):
            raise ValueError("unresolved source IDs block historical or current classification")
    except (ValueError, KeyError, TypeError, OSError) as error:
        parser.exit(1, f"通用来源分类失败：{error}\n")


if __name__ == "__main__":
    main()
