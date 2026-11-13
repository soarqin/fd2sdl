"""S3发布闭包：核原DAT/S1/S2来源、唯一正式绑定和声明域，不运行或开放关卡。

依据1088d/10b4e、1a813/13a44/190ac/1b6b7原caller及已冻结S1/S2读集。
完整输入与结果只保本地build；来源图/资源结构通过不等于正式或原版运行通过。
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
import audit_stage_data as audit
import s2_source_disposition as disposition
import stage_source_ledger as ledger_tool

S1_SHA256 = "1a47357ba1f179db74bd0d99bf2096748596e6fc46c1ab1aabf30950eb3e6202"
S2_SHA256 = "59fd72f29377e12249a7bb077338ba05d651484517e39cb2c1172f4ee6fab120"


def read(path):
    return json.loads(Path(path).read_bytes(), object_pairs_hook=audit.unique_keys)


def stages(value):
    result = set()
    for part in value.split(","):
        limits = part.split(":")
        if len(limits) not in (1, 2) or any(not x.isdecimal() for x in limits):
            raise ValueError("stage list requires comma-separated IDs or inclusive first:last")
        first, last = int(limits[0]), int(limits[-1])
        if not 0 <= first <= last < 30:
            raise ValueError("battle tables contain0..29; opening31/32 is separate")
        result.update(range(first, last + 1))
    return sorted(result)


def entry_counts(flow, metadata, flows, aliases, depth=0):
    """仅核1088d/10b4e人数写集；枚举冷图成功边，不制造Game或执行玩法。"""
    if depth > 3:
        raise ValueError("entry shared call depth exceeded")
    groups = Counter(t[0x15] for t in metadata["templates"][:metadata["unknown_02"]])
    nodes = {n["id"]: n for n in flow["nodes"]}
    pending, seen, ends = [(flow["entry"], 0)], set(), set()
    while pending:
        pc, count = pending.pop()
        if (pc, count) in seen:
            continue
        seen.add((pc, count))
        if len(seen) > 65536:
            raise ValueError("entry count graph budget exceeded")
        node = nodes[pc]; op, args = node["op"], node["args"]
        if op == "fail":
            continue
        if op == "end":
            ends.add(count); continue
        if op == "stage_setup_source":
            counts = {metadata["unknown_01"]}
        elif op == "append_group":
            counts = {count + groups[args[0]]}
        elif op == "call_init":
            helper = audit.resolve("stage.source.initialize", flows, aliases)
            if helper is None:
                raise ValueError("entry lacks shared initialization")
            counts = entry_counts(helper, metadata, flows, aliases, depth + 1)
        else:
            counts = {count}
        if any(not 0 <= n <= 96 for n in counts):
            raise ValueError("entry append exceeds original active capacity96")
        edges = {target for port, target in node["on"].items() if port != "error"}
        for n in counts:
            pending.extend((target, n) for target in edges)
    if not ends:
        raise ValueError("entry has no successful terminal")
    return ends


def callback_requirements(stage):
    """保原槽和WORD，不把引用次数当机制数；模板尾与collector255不提权。"""
    required = []
    for row in stage["metadata"]["turn_slots"]:
        if not row["default_fill"]:
            required.append(("turn", row["slot"], row["action"]))
    for row in stage["metadata"]["cell_lookup_slots"]:
        if row["action"] != 255:
            required.append(("lookup", row["slot"], row["action"]))
    for row in stage["metadata"]["cell_action_slots"]:
        if row["referenced_by_map"] and row["callback_u16"] is not None:
            required.append(("cell_action", row["slot"], row["callback_u16"]))
    for row in stage["metadata"]["templates"]:
        drop = row["drop"]
        if row["effective_template"] and drop["callback_u16"] is not None:
            required.append(("drop", row["index"], drop["callback_u16"]))
    if any(type(action) is not int or not 0 <= action < 90 for _, _, action in required):
        raise ValueError("required callback WORD outside original90-slot table")
    return required


def check_binding(runtime_id, source_id, dispatch, flows, aliases):
    source = dispatch[source_id]
    if source["disposition"] in ("blocked", "unresolved"):
        raise ValueError(f"{runtime_id}: required source remains quarantined: {source_id}")
    actual = audit.resolve(runtime_id, flows, aliases)
    expected = audit.resolve(source["definition_id"], flows, aliases)
    if actual is None or expected is None or actual["id"] != expected["id"]:
        raise ValueError(f"{runtime_id}: exact original caller/target binding differs: {source_id}")
    return {"runtime_id": runtime_id, "source_id": source_id,
            "original_target": source["target_va"], "original_caller": source["original_caller"],
            "source": source["source"], "definition_domain": source["definition_domain"],
            "definition_id": actual["id"],
            "definition_sha256": audit.digest(disposition.encode(actual).encode()),
            "maximum_source_read_prefix": source.get("minimum_raw_prefix"),
            "prefix_scope": "source full-readset maximum; actual conditional node validates when reached"}


def refine_callbacks(dispatch, callbacks, proof):
    """S2旧归属不改签；S3仅凭完整原caller/实际C门纠正41共享尾参数。"""
    rows={r["source_id"]:dict(r) for r in dispatch}
    decisions=callbacks.get("decisions",[])
    if len(decisions)!=90 or len({r["source_id"] for r in decisions})!=90:
        raise ValueError("callback90 original source decisions missing/duplicate")
    refinements=[]
    for candidate in decisions:
        identity=candidate["source_id"];old=rows[identity]
        if old["target_va"]!=candidate["target_va"]:
            raise ValueError("callback original target changed")
        new=candidate.get("definition")
        if old.get("definition_id")==new:continue
        if (identity!="dispatch:event_callback:041" or old.get("definition_id")!="common.loot.child.1" or
                new!="common.callback.041" or candidate["disposition"]!="converted_candidate"):
            raise ValueError("new callback refinement lacks reviewed complete source")
        if (not proof or proof.get("schema")!="fd2-s3-callback-binding-source-v1" or
                proof.get("original_CPU_executed") is not True or proof.get("actual_C",{}).get("verified") is not True):
            raise ValueError("callback41 needs independent complete caller/actualC evidence")
        if proof.get("S2_refinement",{}).get("current_definition")!=new:
            raise ValueError("callback41 actualC evidence refers to another graph")
        old["S2_frozen_definition_id"]=old["definition_id"]
        old["definition_id"]=new;old["disposition"]="converted_candidate"
        refinements.append(proof["S2_refinement"])
    return rows,refinements


def next_stage_targets(flow):
    nodes={n["id"]:n for n in flow["nodes"]};pending=[(flow["entry"],None)];seen=set();targets=set()
    while pending:
        pc,target=pending.pop()
        if (pc,target) in seen:continue
        seen.add((pc,target));node=nodes[pc]
        if node["op"]=="fail":continue
        if node["op"]=="next_stage":target=node["args"][0]
        if node["op"]=="end":
            if target is None:raise ValueError("victory success path lacks original next_stage write")
            targets.add(target);continue
        pending.extend((dst,target) for port,dst in node["on"].items() if port!="error")
    return targets


def resource_references(stage, bindings, flows, scripts):
    """结构/字面引用门；动态身份/地图/说话人由实际有限C预检另验。"""
    result = []; seen = set()
    for binding in bindings:
        flow = flows[binding["definition_id"]]
        for node in flow["nodes"]:
            op, args = node["op"], node["args"]
            key = (flow["id"], node["id"])
            if key in seen: continue
            seen.add(key)
            if op in ("text", "story"):
                if not 0 <= args[0] < stage["text_reference"]["fragment_count"]:
                    raise ValueError(f"stage{stage['stage']} {key}: source text fragment out of range")
                result.append({"flow": key[0], "node": key[1], "kind": "text_fragment",
                    "source_id": f"text:{stage['stage']+1:03d}:{args[0]:05d}"})
            elif op == "move":
                script = scripts.get(str(args[0]))
                if script is None: raise ValueError(f"{key}: source movement script missing")
                structure = audit.scan_movement_structure(bytes.fromhex(script["raw_hex"]))
                if not structure["complete"] or structure["consumed_length"] != script["length"]:
                    raise ValueError(f"{key}: incomplete counted movement source")
                actors = {a for group in structure["groups"] for a, _ in group["pairs"]}
                result.append({"flow": key[0], "node": key[1], "kind": "movement",
                    "script": args[0], "source": {k: script[k] for k in ("canonical_va", "length", "sha256")},
                    "required_raw_prefix": max(actors, default=-1)+1,
                    "gate": "actual projected count/identity/cache; not initial count alone"})
            elif op in ("story_source", "append_group_source", "map_replace", "raw_animation"):
                result.append({"flow": key[0], "node": key[1], "kind": op, "args": args,
                    "gate": "actual C bounded future-resource projection and leaf revalidation"})
    return result


def build(original, ledger_directory, overlay_path, callbacks_path, transitions_path,
          battle_stages, entry_stages, binding_source=None):
    original, ledger_directory = Path(original), Path(ledger_directory)
    manifest = read(ledger_directory / "manifest.json")
    if manifest.get("schema") != ledger_tool.SCHEMA or set(manifest["files"]) != {
            "inventory.json", "common-scope.json", "ledger.json", "fixtures.json"}:
        raise ValueError("incomplete original S1 source package")
    inputs = {ledger_directory / "manifest.json": (ledger_directory / "manifest.json").read_bytes()}
    for path in [Path(__file__), ROOT / "tools/static_data/s2_source_disposition.py",
                 ROOT / "tools/static_data/audit_stage_data.py", ROOT / "tools/static_data/stage_source_ledger.py",
                 ROOT / "tools/static_data/generate.py", *sorted((ROOT / "data/static").rglob("*.json"))]:
        inputs[path] = path.read_bytes()
    for name, digest in manifest["files"].items():
        path = ledger_directory / name; inputs[path] = path.read_bytes()
        if audit.digest(inputs[path]) != digest: raise ValueError("S1 package file changed")
    if manifest["files"]["ledger.json"] != S1_SHA256:
        raise ValueError("S1 frozen source identity differs")
    ledger = read(ledger_directory / "ledger.json")
    sources = ledger_tool.Sources(original); ledger_tool.validate_ledger(ledger, sources)
    paths = [Path(overlay_path), Path(callbacks_path), Path(transitions_path)]
    for path in paths: inputs[path] = path.read_bytes()
    if audit.digest(inputs[paths[0]]) != S2_SHA256:
        raise ValueError("S2 frozen disposition identity differs; never rewrite its previous receipt")
    overlay, callbacks, transitions = map(read, paths)
    if overlay.get("schema") != "fd2-s2-source-disposition-v1" or overlay["S1_ledger_sha256"] != S1_SHA256:
        raise ValueError("S2 source overlay schema/ledger identity differs")
    for path in paths[2:]:
        matching = [value for name, value in overlay["input_sha256"].items()
                    if name.replace("\\", "/").endswith("/"+path.parent.name+"/"+path.name)]
        if matching != [audit.digest(inputs[path])]: raise ValueError("S2 candidate source identity differs")
    proof=None
    if binding_source:
        path=Path(binding_source);inputs[path]=path.read_bytes();proof=read(path)
        for name,digest in proof.get("inputs_sha256",{}).items():
            source=Path(name);inputs[source]=source.read_bytes()
            if audit.digest(inputs[source])!=digest:raise ValueError("S3 callback original/C source evidence inputs changed")
        actual_c=proof.get("actual_C",{})
        if not actual_c.get("probe") or audit.digest(Path(actual_c["probe"]).read_bytes())!=actual_c.get("probe_sha256"):
            raise ValueError("S3 callback actualC executable evidence identity differs")
    candidate_manifest=read(paths[1].parent/"manifest.json")
    inputs[paths[1].parent/"manifest.json"]=(paths[1].parent/"manifest.json").read_bytes()
    if candidate_manifest.get("files",{}).get(paths[1].name)!=audit.digest(inputs[paths[1]]):
        raise ValueError("callback candidate package incomplete/changed")
    for name,digest in candidate_manifest["files"].items():
        path=paths[1].parent/name;inputs[path]=path.read_bytes()
        if audit.digest(inputs[path])!=digest:raise ValueError("callback candidate receipt changed")
    inventory = audit.audit(original)
    flows, aliases, _ = audit.load_definitions(ROOT / "data/static")
    catalog = {(document["domain"], f["id"]): f for path in (ROOT / "data/static/flows").glob("*.json")
               for document in [read(path)] for f in document["flows"]}
    stage_document = read(ROOT / "data/static/flows/stage.json")
    disposition.validate_definition_contents(callbacks, transitions, catalog, stage_document.get("bindings", {}))
    disposition.validate_dispatch_mapping(ledger, overlay["dispatch"])
    dispatch,refinements=refine_callbacks(overlay["dispatch"],callbacks,proof)
    source_rows = {r["source_id"]: r for r in ledger["records"]}
    for identity, row in dispatch.items():
        if row["source"] != source_rows[identity]["source"]:
            raise ValueError("S2 dispatch original source locator/raw/hash changed")
    for identity, row in dispatch.items():
        if row["disposition"] not in ("blocked", "unresolved"): continue
        _, table, slot = identity.split(":")
        if table != "event_callback":
            kind = {"stage_entry":"entry", "stage_victory":"victory"}[table]
            if audit.resolve(f"stage.{kind}.{int(slot):03d}", flows, aliases):
                raise ValueError("quarantined source has a formal shipping binding")
        elif any(re.fullmatch(r"stage\.\d+\.action\."+slot, key) for key in (*flows, *aliases)):
            raise ValueError("quarantined callback has a formal shipping binding")
    if set(battle_stages) & set(entry_stages): raise ValueError("battle/entry-only stage sets overlap")
    selected = set(battle_stages) | set(entry_stages)
    if not selected: raise ValueError("empty release scope")
    for stage in inventory["stages"]:
        if stage["stage"] >= 2 and stage["battle_enabled"] and stage["stage"] not in battle_stages:
            raise ValueError("enabled stage missing from declared release scope")
    report_rows = []
    prepare_table=next(t for t in read(ROOT/"data/static/tables/stage.json")["tables"] if t["name"]=="k_stage_prepare")
    for stage in inventory["stages"]:
        st = stage["stage"]
        if st not in selected: continue
        battle = st in battle_stages; cap = stage["capability"]
        if not cap or bool(cap[0]) != battle: raise ValueError(f"stage{st}: capability/scope differs")
        if stage["reference_errors"]: raise ValueError(f"stage{st}: original resource reference errors")
        bindings = [check_binding(f"stage.entry.{st:03d}", f"dispatch:stage_entry:{st:03d}", dispatch, flows, aliases)]
        requirements = callback_requirements(stage)
        if battle:
            for kind, table in [("result", "stage_runtime_result"), ("victory", "stage_victory")]:
                source_id=f"dispatch:{table}:{st:03d}"
                runtime_id=f"stage.{st:03d}.result" if kind=="result" and dispatch[source_id]["definition_domain"]=="common_event" else f"stage.{kind}.{st:03d}"
                bindings.append(check_binding(runtime_id, source_id, dispatch, flows, aliases))
            for action in sorted({a for _, _, a in requirements}):
                bindings.append(check_binding(f"stage.{st:03d}.action.{action:03d}", f"dispatch:event_callback:{action:03d}", dispatch, flows, aliases))
        handoffs=[]
        if battle:
            victory=next(b for b in bindings if b["source_id"]==f"dispatch:stage_victory:{st:03d}")
            for target in sorted(next_stage_targets(flows[victory["definition_id"]])):
                if target not in selected:raise ValueError("victory next_stage leaves declared entry/owner closure")
                entry=check_binding(f"stage.entry.{target:03d}",f"dispatch:stage_entry:{target:03d}",dispatch,flows,aliases)
                if not audit.capability(target,flows,aliases) or not 1<=target<=26:
                    raise ValueError("victory handoff lacks capability/original31B preparation record")
                raw=audit.source_range(sources.data["FD2.EXE"],0x6238d+(target-1)*31,31)
                if bytes(prepare_table["values"][target-1])!=raw:raise ValueError("next_stage preparation source differs")
                handoffs.append({"next_stage":target,"entry_definition":entry["definition_id"],
                    "preparation_source":{"file":"FD2.EXE","canonical_va":hex(0x6238d+(target-1)*31),"length":31,"sha256":audit.digest(raw)},
                    "entry_only":target in entry_stages,"owner_gate":"export preparation then close old Game; reopen entry; never run new controller with old resources"})
        raw = sources.entries["FDFIELD.DAT"][st*3+1][1]
        counts = entry_counts(flows[bindings[0]["definition_id"]], audit.parse_metadata(raw), flows, aliases)
        if cap[1] != min(counts) or cap[2] != 0:
            raise ValueError(f"stage{st}: snapshot count gate differs from complete entry count; growth must remain possible")
        defaults = [source_rows[f"stage:{st:02d}:turn:{x['slot']:05d}"] for x in stage["metadata"]["turn_slots"] if x["default_fill"]]
        if any(x["disposition"] != "rejected" or x["source"]["raw_hex"] != "ffff00" for x in defaults):
            raise ValueError("default255 source refusal was masked or removed")
        kinds = Counter(r["kind"] for r in ledger["records"] if r.get("stage") == st)
        callback_sources=[]
        for kind,slot,action in requirements:
            identity=f"stage:{st:02d}:{kind}:{slot:05d}";source=source_rows[identity]
            parent=source_rows.get(source["parent_id"],{})
            callback_sources.append({"source_id":identity,"action_u16":action,
                "caller":source["caller"],"source":source["source"],
                "parent_source_id":source["parent_id"],
                "template_group":parent.get("decoded",{}).get("group") if kind=="drop" else None,
                "required_for_battle_scope":battle})
        report_rows.append({"stage": st, "battle_enabled": battle, "entry_only": not battle,
            "capability": cap, "entry_count": sorted(counts), "source_record_counts": dict(kinds),
            "map": {k:stage["map"][k] for k in ("entry","file_offset","width","height","length","sha256")},
            "metadata": {k:stage["metadata"][k] for k in ("entry","file_offset","length","sha256","shape_index","effective_template_count","retained_template_count")},
            "placements": {k:stage["placements"][k] for k in ("entry","file_offset","length","sha256")},
            "bindings": bindings, "victory_handoffs":handoffs,
            "callback_source_references": callback_sources,
            "resource_references": resource_references(stage,bindings,flows,inventory["movement_sources"]),
            "default255_refusals": [{"source_id":r["source_id"],"source":r["source"],"gate":"load preserves mutable metadata; actual turn255/phase0 dispatch explicitly refuses action255"} for r in defaults],
            "strict_supported": stage["selected_semantics_supported"],
            "structural_reference_errors": stage["reference_errors"]})
    sources.unchanged()
    if any(p.read_bytes() != raw for p,raw in inputs.items()): raise ValueError("release audit inputs changed")
    return {"schema":"fd2-stage-release-scope-v1", "structural_binding_scope_closed":True,
        "input_sha256":{str(p):audit.digest(raw) for p,raw in inputs.items()},
        "battle_stages":sorted(battle_stages), "entry_only_stages":sorted(entry_stages), "stages":report_rows,
        "S2_history_retained":True,"S3_callback_refinements":refinements,
        "summary":{"bindings":sum(len(r["bindings"]) for r in report_rows),
            "different_targets":len({b["original_target"] for r in report_rows for b in r["bindings"]}),
            "callback_source_references":sum(len(r["callback_source_references"]) for r in report_rows),
            "default255_refusals":sum(len(r["default255_refusals"]) for r in report_rows),
            "declared_scope_rejected_source_ids":sum(len(r["default255_refusals"]) for r in report_rows),
            "S1_rejected_by_resource_domain":dict(Counter("battle_table_0_29" if source_rows[r["source_id"]]["stage"]<30 else
                "resource_groups_30_32" for r in overlay["S1_rejected"])),
            "S2_quarantined_slots":sum(r["disposition"] in ("blocked","unresolved") for r in dispatch.values()),
            "S1_rejected_sources":len(overlay["S1_rejected"])},
        "claims":{"runtime_paths_validated":False,"new_native_runtime_executed":False,
            "all_text_glyph_portrait_semantics_validated":False,"RNG_or_hot_state_changed":False,
            "strict_turn255_was_ignored":False,"new_shipping_bindings_written":False},
        "next_gates":["actual finite C count/identity/map/roster projection and each leaf resource preflight",
            "conditional result reads incl stage12/18 after prior reinforcement; no global max-prefix gate",
            "formal entry/controller/END/settlement/preparation and activity/manual restore",
            "few original/native representatives; source-derived fixtures are not independent runtime oracle"]}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original",type=Path,required=True)
    parser.add_argument("--ledger",type=Path,required=True)
    parser.add_argument("--s2-disposition",type=Path,required=True)
    parser.add_argument("--callbacks",type=Path,required=True)
    parser.add_argument("--transitions",type=Path,required=True)
    parser.add_argument("--binding-source",type=Path)
    parser.add_argument("--battle-stages",type=stages,required=True)
    parser.add_argument("--entry-stages",type=stages,default=[])
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    try:
        output=ledger_tool.validate_output(args.output,ROOT,args.original)
        report=build(args.original,args.ledger,args.s2_disposition,args.callbacks,args.transitions,
                     args.battle_stages,args.entry_stages,args.binding_source)
        output.mkdir(parents=True)
        raw=disposition.encode(report).encode("utf8");(output/"scope.json").write_bytes(raw)
        print(json.dumps({"output":str(output),"sha256":audit.digest(raw),**report["summary"]},ensure_ascii=False))
    except (OSError,ValueError,KeyError,TypeError) as error:
        parser.exit(1,f"发布来源闭包校验失败：{error}\n")


if __name__=="__main__": main()
