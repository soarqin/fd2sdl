#!/usr/bin/env python3
"""S2全来源去向叠加：原S1不改写，候选冷图不等于正式关卡绑定。

208槽由result/entry/victory/callback候选及G2地图法术合同分别对齐；
315未决与450拒绝保留原ID/locator/hash、参数与后续门，不把同target自动签运行。
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/static_data"), str(ROOT / "tools/ida")]
import audit_stage_data as audit
from stage_source_ledger import Sources, validate_ledger, validate_output
import s2_result_source as result_source
import verify_magic_contracts


def digest(raw): return hashlib.sha256(raw).hexdigest()
def encode(value): return json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
def read(path): return json.loads(Path(path).read_bytes(), object_pairs_hook=audit.unique_keys)


def magic_destinations(ledger, original):
    """继承G2完整wrapper/有限consumer域；仅复核源表与实际别名/费用合同。"""
    tables = read(ROOT / "data/static/tables/magic.json")
    receipt = verify_magic_contracts.check(tables, Path(original) / "FD2.EXE")
    values = {row["name"]: row["values"] for row in tables["tables"]}
    targets = (0x21206, 0x2134b, 0x21364, 0x2137d, 0x21396, 0x21449, 0x21462,
               0x2147b, 0x21494, 0x214ad, 0x21527, 0x2185f, 0x21a9e, 0x21ad9,
               0x21b99, 0x2211c, 0x22153, 0x226ea, 0x2282f, 0x22960, 0x22a85,
               0x22bc6, 0x22be1, 0x2218a, 0x22153, 0x22c04, 0x22cbf, 0x22e41)
    rows = {row["decoded"]["slot"]: row for row in ledger["records"]
            if row["kind"] == "dispatch" and row["decoded"].get("table") == "adjacent_magic"}
    if set(rows) != set(range(28)): raise ValueError("原28地图法术槽缺失")
    output = []
    for slot in range(28):
        row = rows[slot]
        if int(row["decoded"]["target_va"], 16) != targets[slot]: raise ValueError("G2原地图法术目标不同")
        family = ("damage_map" if slot <= 12 else "heal_map" if slot <= 16 or slot == 24 else
                  "status_buff" if slot <= 19 else "status_heal" if slot <= 21 else
                  "teleport_first_target" if slot == 23 else "acted_clear" if slot == 25 else "status_damage")
        output.append({"source_id": row["source_id"], "target_va": row["decoded"]["target_va"],
            "disposition": "existing_primitive_caller_domain", "definition_domain": "magic",
            "definition_id": "magic.effect.023" if slot == 23 else None, "semantic_family": family,
            "projection": {name: values[name][slot] for name in verify_magic_contracts.NAMES},
            "consumer": {"profile": "fd2_field_magic_map_profile_get", "effect": "magic.effect.023" if slot == 23 else "fd2_field_magic_effect_apply",
                         "map_owner": "execute_map_magic", "teleport_owner": "execute_teleport_magic" if slot == 23 else None},
            "caller_scope": {"player": "FULL before table" if slot < 9 or slot == 24 else "dedicated teleport selection/023 cold graph" if slot == 23 else "map wrapper",
                             "AI": "FULL iff id<10 and SHORT option0; otherwise map, after existing normal-source eligibility gates",
                             "slot24": "table24 aliases16(effect16/cost16); player24 never consumes this alias" if slot == 24 else None},
            "evidence": ["docs/reverse-engineering/common-battle-semantics.md#2-28-槽地图法术分派与-36-条法术记录",
                         "tools/static_data/verify_magic_contracts.py", "G2 existing primitive/consumer/original representative receipts"],
            "new_runtime_binding": False, "blocker": None})
    return output, receipt


def validate_dispatch_mapping(ledger, candidates):
    source = {row["source_id"]: row for row in ledger["records"] if row["kind"] == "dispatch"}
    if len(source) != 208 or len(candidates) != 208: raise ValueError("208分派源/去向口径不完整")
    mapped = {}
    for row in candidates:
        identity = row["source_id"]
        if identity in mapped or identity not in source: raise ValueError("分派源重复/不存在")
        original = source[identity]
        if row["target_va"] != original["decoded"]["target_va"]: raise ValueError("分派目标不等于原S1目标")
        if row.get("new_runtime_binding") is not False: raise ValueError("来源叠加不能批准新运行绑定")
        if row["disposition"] in ("blocked", "unresolved"):
            if not row.get("blocker"): raise ValueError("未决分派没有真实blocker")
        elif row["disposition"] not in ("existing_definition_reference", "converted_candidate", "existing_primitive_caller_domain"):
            raise ValueError("未知S2来源去向")
        elif row["disposition"] != "existing_primitive_caller_domain" and not row.get("definition_id"):
            raise ValueError("已转换/已有图去向缺唯一冷定义引用")
        if row["disposition"] == "existing_primitive_caller_domain" and not identity.startswith("dispatch:adjacent_magic:"):
            raise ValueError("已有primitive域不能冒充callback或stage冷图")
        mapped[identity] = row
    if set(mapped) != set(source): raise ValueError("分派去向遗漏")
    return mapped


def overlay(ledger, candidates, definition_ids):
    mapped = validate_dispatch_mapping(ledger, candidates)
    dispatch = []
    for original in (r for r in ledger["records"] if r["kind"] == "dispatch"):
        identity = original["source_id"]; candidate = mapped[identity]
        definition = candidate.get("definition_id")
        if candidate["disposition"] not in ("blocked", "unresolved") and definition and (candidate.get("definition_domain"), definition) not in definition_ids:
            raise ValueError("已转换/已有定义缺实际唯一冷图，不能标available")
        dispatch.append({**candidate, "S1_disposition": original["disposition"], "source": original["source"],
            "S1_definition": original["definition"], "original_caller": original["caller"],
            "S1_function_sources": original["references"],
            "definition_present_in_static_input": (candidate.get("definition_domain"), definition) in definition_ids if definition else None,
            "binding_gate": "preserve existing shipping domain; every new exact stage/caller/parameter/resource binding remains S3"})
    unresolved, rejected = [], []
    for original in ledger["records"]:
        identity = original["source_id"]
        if original["disposition"] == "unresolved":
            dependencies = [identity] if original["kind"] == "dispatch" else [ref for ref in original["references"] if ref in mapped]
            if not dependencies: raise ValueError("未决原来源未归属分派/能力")
            blockers = [ref for ref in dependencies if mapped[ref]["disposition"] in ("blocked", "unresolved")]
            unresolved.append({"source_id": identity, "S1_disposition": "unresolved", "source": original["source"],
                "caller": original["caller"], "decoded": original["decoded"], "dependencies": dependencies,
                "S2_disposition": "quarantined_source_dependency" if blockers else "source_graph_or_existing_primitive_available",
                "blocked_dependencies": blockers, "source_semantics_candidates": [{"source_id": ref,
                    "domain": mapped[ref].get("definition_domain"), "definition": mapped[ref].get("definition_id")} for ref in dependencies],
                "next_gate": "S2 exception remains quarantined; do not enable its dependency batch" if blockers else "S3 exact caller ABI/parameter/resource/state closure; source graph is not binding approval",
                "new_runtime_binding": False})
        elif original["disposition"] == "rejected":
            if original["kind"] != "turn" or original["source"]["raw_hex"] != "ffff00" or original["decoded"].get("action") != 255:
                raise ValueError("拒绝域变化，需要重新核原caller而非套turn255")
            rejected.append({"source_id": identity, "S1_disposition": "rejected", "source": original["source"],
                "caller": original["caller"], "decoded": original["decoded"], "S2_disposition": "known_out_of_table_refusal",
                "reason": "original turn255/phase0 can match; callback255 has no code fixup in the confirmed90-slot table",
                "required_behavior": "preserve source and explicit diagnostic refusal; never mask to BYTE, default victory or skip silently",
                "new_runtime_binding": False})
    if len(unresolved) != 315 or len(rejected) != 450: raise ValueError("S1未决315/拒绝450来源分母变化")
    by_table = defaultdict(Counter)
    for row in dispatch: by_table[row["source_id"].split(':')[1]][row["disposition"]] += 1
    all_ids = {row["source_id"] for row in dispatch + unresolved + rejected}
    return {"schema": "fd2-s2-source-disposition-v1", "dispatch": dispatch, "S1_unresolved": unresolved,
        "S1_rejected": rejected, "summary": {"dispatch_slots": len(dispatch),
            "different_dispatch_targets": len({r["target_va"] for r in dispatch}),
            "dispatch_by_table_and_destination": {key: dict(value) for key, value in by_table.items()},
            "S1_unresolved_sources": len(unresolved), "S1_unresolved_destinations": dict(Counter(r["S2_disposition"] for r in unresolved)),
            "S1_rejected_sources": len(rejected), "unique_source_ID_union": len(all_ids),
            "scope_warning": "208 slots and315 unresolved overlap; not independent mechanism counts or runtime coverage"},
        "claims": {"S1_history_rewritten": False, "new_runtime_bindings": 0, "all_game_semantic_complete": False,
            "source_candidate_is_shipping_binding": False, "new_original_native_runtime_executed": False}}


def validate_definition_contents(callbacks, transitions, catalog, static_bindings=None):
    """既有C门只对应当时图内容；同ID换图不复用旧签核。"""
    proposals = {flow["id"]: flow for flow in callbacks["flows"]}
    if len(proposals) != len(callbacks["flows"]): raise ValueError("callback候选图重复")
    for row in callbacks["decisions"]:
        if row["disposition"] in ("blocked", "unresolved"): continue
        identity = row["definition"]; actual = catalog.get(("common_event", identity))
        if actual is None or digest(encode(actual).encode()) != row["definition_sha256"]:
            raise ValueError("callback同ID缺图/内容改变，不能复用旧C门")
        if row["disposition"] == "converted_candidate" and encode(proposals.get(identity)) != encode(actual):
            raise ValueError("callback候选图与实际唯一输入内容不同")
    proposed = {flow["id"]: flow for flow in transitions["flows"]}
    if len(proposed) != len(transitions["flows"]): raise ValueError("transition候选/共享图重复")
    aliases = transitions.get("source_bindings", {})
    actual_aliases = {key: value for key, value in (static_bindings or {}).items()
                      if key.startswith("stage.source.")}
    if type(aliases) is not dict or aliases != actual_aliases:
        raise ValueError("transition source-only别名不等于唯一静态输入bindings")
    for alias, canonical in aliases.items():
        if (not alias.startswith("stage.source.") or not canonical.startswith("stage.source.") or
                alias in proposed or ("stage", alias) in catalog or canonical not in proposed or canonical in aliases):
            raise ValueError("transition source-only别名缺唯一终点/有链或混入正式绑定")
    for identity, flow in proposed.items():
        if ("stage", identity) not in catalog or encode(catalog["stage", identity]) != encode(flow):
            raise ValueError("transition候选/共享图缺失或内容改变")
    for row in transitions["rows"]:
        if row["disposition"] == "converted_candidate":
            logical = row.get("flow_id"); canonical = aliases.get(logical, logical)
            if canonical not in proposed: raise ValueError("transition已转换源缺对应完整候选图")
            if (row.get("canonical_definition", canonical) != canonical or
                    row.get("source_only_alias", logical in aliases) is not (logical in aliases)):
                raise ValueError("transition来源行与source-only别名规范终点不等")


def apply_source_extension(dispatch, extension, catalog):
    """S5只叠加原13隔离来源，不重写S2原件；源/图门不提升成运行证明。"""
    import s5_stage_transitions as s5
    decisions, expected, snapshots = s5.validate_candidate(extension,catalog)
    rows={r['source_id']:dict(r) for r in dispatch}
    if len(rows)!=208 or len(dispatch)!=208:raise ValueError('S5 needs complete original208 dispatch')
    fixed={'call_teleport':'stage.source.teleport',
           'call_cinematic_ending':'stage.cinematic.ending','call_cinematic_credits':'stage.cinematic.credits',
           'call_cinematic_card_first':'stage.cinematic.card_first',
           'call_cinematic_card_normal':'stage.cinematic.card_normal','call_cinematic_card_last':'stage.cinematic.card_last',
           'call_cinematic_montage_round':'stage.cinematic.montage_round'}
    def installed_closure(domain,identity,seen):
        key=(domain,identity)
        if key in seen:return
        seen.add(key)
        if key not in catalog or catalog[key]!=expected[key]:
            raise ValueError('S5 converted source lacks exact installed unique cold definition/dependency: '+identity)
        for node in expected[key]['nodes']:
            if node['op'] in fixed:installed_closure('stage',fixed[node['op']],seen)
    changes=[]
    for candidate in decisions:
        identity=candidate['source_id'];old=rows.get(identity)
        if (old is None or old['target_va']!=candidate['target_va'] or
                old['disposition'] not in ('blocked','unresolved')):
            raise ValueError('S5 extension may only close exact frozen quarantined caller/target')
        old['S2_frozen_disposition']=old['disposition'];old['S2_frozen_blocker']=old.get('blocker')
        if candidate['disposition']=='converted_candidate':
            installed_closure(candidate['definition_domain'],candidate['definition_id'],set())
            old.update(disposition='converted_candidate',definition_domain=candidate['definition_domain'],
                       definition_id=candidate['definition_id'],blocker=None)
        else:
            old.update(disposition='blocked',blocker={'family':'source_conversion_incomplete',
                'reason':'partial source graph is not runnable conversion',
                'source_id':identity,'next_evidence':'complete original source/cold graph plus independent C/required native gates'})
        old['S5_source_candidate']={'definition_id':candidate['definition_id'],
            'definition_sha256':candidate['definition_sha256'],'source_CFG_complete':True,
            'original_CPU_executed':False,'actual_C_compared':False,'native_GUI_executed':False,
            'source_graph_is_runtime_approval':False}
        changes.append({'source_id':identity,'target_va':old['target_va'],'disposition':old['disposition'],
                        'S2_history_preserved':True,'new_runtime_binding':False})
    return rows,changes,snapshots


def build(original, ledger_directory, callbacks_path, transitions_path, result_domain, image_path,s5_extension=None):
    ledger_directory = Path(ledger_directory)
    paths = [Path(__file__), ledger_directory / "manifest.json", Path(callbacks_path), Path(transitions_path), Path(result_domain),
             Path(image_path), ROOT / "tools/ida/s2_result_source.py", ROOT / "tools/static_data/verify_magic_contracts.py",
             ROOT / "data/static/tables/magic.json", ROOT / "src/field_magic.c", ROOT / "src/field_game.c",
             *sorted((ROOT / "data/static/flows").glob("*.json"))]
    snapshots = {path: path.read_bytes() for path in paths}
    manifest = read(ledger_directory / "manifest.json")
    if manifest.get("schema") != "fd2-stage-source-ledger-v1" or set(manifest["files"]) != {"inventory.json", "common-scope.json", "ledger.json", "fixtures.json"}:
        raise ValueError("S1正式来源包manifest不同")
    for name, expected in manifest["files"].items():
        path = ledger_directory / name; raw = path.read_bytes(); snapshots[path] = raw
        if digest(raw) != expected: raise ValueError("S1来源包文件摘要不同")
    ledger = read(ledger_directory / "ledger.json"); sources = Sources(Path(original)); validate_ledger(ledger, sources)
    callbacks, transitions = read(callbacks_path), read(transitions_path)
    if callbacks.get("schema") != "fd2-s2-callback-candidates-v1" or transitions.get("schema") != "fd2-s2-transition-conversion-v1":
        raise ValueError("callback/transition候选schema不同")
    callback_manifest_path = Path(callbacks_path).parent / "manifest.json"
    callback_manifest = read(callback_manifest_path); snapshots[callback_manifest_path] = callback_manifest_path.read_bytes()
    if callback_manifest.get("schema") != callbacks["schema"] or not {"candidates.json", "original-traces.json", "C-original-comparison.json"} <= set(callback_manifest.get("files", {})):
        raise ValueError("callback候选缺完整原码/真实C门收据，不能沿用旧falsepositive")
    for name, expected in callback_manifest["files"].items():
        path = Path(callbacks_path).parent / name; raw = path.read_bytes(); snapshots[path] = raw
        if digest(raw) != expected: raise ValueError("callback最终门收据摘要不等")
    result = read(result_domain); result_source.validate_source(snapshots[Path(image_path)], result)
    result_source.dispatch_source(Path(original) / "FD2.EXE", ROOT / "docs/generated/ida-canonical")
    catalog = {}; stage_bindings = {}
    for path in (ROOT / "data/static/flows").glob("*.json"):
        doc = read(path)
        if doc["domain"] == "stage":
            if stage_bindings: raise ValueError("重复stage静态输入bindings")
            stage_bindings = doc.get("bindings", {})
        for flow in doc["flows"]:
            key = (doc["domain"], flow["id"])
            if key in catalog: raise ValueError("当前静态输入定义重名")
            catalog[key] = flow
    definitions = set(catalog)
    validate_definition_contents(callbacks, transitions, catalog, stage_bindings)
    candidates = []
    for row in result_source.closure_mapping():
        candidates.append({**row, "disposition": "existing_definition_reference" if row["definition_id"] in ("stage.result.000", "stage.result.001") else "converted_candidate", "blocker": None})
    for row in transitions["rows"]:
        identity = row.get("canonical_definition", row.get("flow_id")) if row["disposition"] == "converted_candidate" else row.get("existing_definition") if row["disposition"] == "existing_definition_reference" else None
        domains = [domain for domain, name in definitions if name == identity]
        domain = domains[0] if len(domains) == 1 else "stage" if row["disposition"] == "converted_candidate" else None
        if row["disposition"] == "existing_definition_reference" and domain is None: raise ValueError("transition既有定义缺失/跨域歧义")
        candidates.append({**row, "definition_domain": domain, "definition_id": identity})
    for row in callbacks["decisions"]:
        candidates.append({**row, "definition_domain": "common_event", "definition_id": row.get("definition")})
    magic, receipt = magic_destinations(ledger, original); candidates.extend(magic)
    changes=[]
    if s5_extension:
        path=Path(s5_extension);snapshots[path]=path.read_bytes()
        updated,changes,extension_inputs=apply_source_extension(candidates,read(path),catalog)
        candidates=list(updated.values());snapshots.update(extension_inputs)
        import s5_stage_transitions
        tool=Path(s5_stage_transitions.__file__);snapshots[tool]=tool.read_bytes()
    report = overlay(ledger, candidates, definitions)
    if changes:
        report['S5_source_refinements']=changes
        report['S5_source_only_scope']='fresh overlay; S1/S2/S3/S4 old receipts unchanged; original/C/native gates remain independent'
        report['S5_actual_C_excluded_from_older_callback_receipt']=True
    report["S1_ledger_sha256"] = digest(snapshots[ledger_directory / "ledger.json"])
    report["input_sha256"] = {str(path): digest(raw) for path, raw in snapshots.items()}
    report["existing_magic_contract_receipt"] = receipt
    comparison = read(Path(callbacks_path).parent / "C-original-comparison.json")
    report["callback_actual_C_receipt"] = {key: comparison[key] for key in ("schema", "probe_sha256", "normal_cases", "wait_guard_cases", "capacity_refusal_cases")}
    if comparison["schema"] != "fd2-s2-callback-C-original-comparison-v1" or any(row.get("passed") is not True for row in comparison["cases"]):
        raise ValueError("callback实际C门来源schema或case状态不同")
    report["callback_actual_C_receipt"]["wait_guard_cases"] = sum(row["mode"] in (1,2,3) for row in comparison["cases"])
    report["transition_source_conversion"] = {key: transitions["summary"][key] for key in
        ("converted_source_bindings", "candidate_flows", "shared_helper_flows", "source_only_aliases")
        if key in transitions["summary"]}
    report["source_only_transition_aliases"] = transitions.get("source_bindings", {})
    report["source_only_alias_scope"] = "full-equal source graphs share one canonical definition; these are not shipping stage/controller aliases"
    report["external_result_C_gate"] = "result original/C matrix is independently signed in its own final receipt; this overlay checks original result source and definition presence only"
    report["magic_scope"] = "G2 complete reviewed wrappers/projection and existing finite C consumer reused; no new magic conversion or native run, player24/FULL and AI SHORT caller domains stay distinct"
    report["quarantined_families"] = dict(Counter(row["blocker"]["family"] for row in report["dispatch"] if row.get("blocker")))
    sources.unchanged()
    if any(path.read_bytes() != raw for path, raw in snapshots.items()): raise ValueError("来源或候选输入在叠加期间改变")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--ledger-dir", type=Path, required=True)
    parser.add_argument("--callbacks", type=Path, required=True)
    parser.add_argument("--transitions", type=Path, required=True)
    parser.add_argument("--result-domain", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument('--s5-extension',type=Path)
    args = parser.parse_args()
    try:
        if args.s5_extension:
            from s5_output import validate_output as s5_output
            output=s5_output(args.output_dir,args.original)
        else:output = validate_output(args.output_dir, original=args.original)
        report = build(args.original, args.ledger_dir, args.callbacks, args.transitions, args.result_domain, args.image,args.s5_extension)
        output.mkdir(parents=True); raw = encode(report).encode(); (output / "disposition.json").write_bytes(raw)
        (output / "manifest.json").write_text(encode({"schema": report["schema"], "files": {"disposition.json": digest(raw)}, "summary": report["summary"]}), encoding="utf8")
        print(encode(report["summary"]), end="")
    except (OSError, KeyError, ValueError) as error: parser.error(str(error))


if __name__ == "__main__": main()
