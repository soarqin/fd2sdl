#!/usr/bin/env python3
"""SD3只读批量提取：原结构、fixup/IDA跨度、当前定义覆盖分别报告。

依据field_turn_event_check @0x1a813、field_cell_event_lookup @0x13a44、
当前IDA发布物和已确认FDFIELD布局。不翻译未知控制流，不自动开放任何stage。
报告含原始资源字节，只能保存为本地证据，不得提交或随发布包分发。
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from dump_fd2_fixup_table import collect_internal_offset_fixups
from rebuild_fd2_analysis import parse_le, parse_objects
from verify_unit_tables import EXE_SHA256, source_range
from verify_movement_scripts import ACTOR_LIMIT, POINTER_TABLE_VA, SCRIPT_IDS, source_scripts
import generate as flow_generator

SOURCE_HASHES = {
    "FD2.EXE": EXE_SHA256,
    "FDFIELD.DAT": "81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d",
    "FDSHAP.DAT": "901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c",
    "FDTXT.DAT": "b47e39ee727482528da061a8705f5cdee8a9cf759447aeda298afe17d9dd8bcc",
}
# 不是以相邻stage表反推120项action：90项之后有两个无fixup的DWORD，
# 0x51d01为IDA独立符号funcs_1541F的magic dispatch，不是action90..119。
DISPATCH = (
    ("stage_runtime_result", 0x51B19, 30, "funcs_1197B"),
    ("event_callback", 0x51B91, 90, "funcs_1199C"),
    ("adjacent_magic", 0x51D01, 28, "funcs_1541F"),
    ("stage_entry", 0x51D71, 30, "funcs_25E3A"),
    ("stage_victory", 0x51DE9, 30, "funcs_25E23"),
)
SUPPORTED = "supported_definition"
UNOPENED = "decoded_unopened"
UNKNOWN = "unmapped_semantics"
MOVEMENT_POINTER_COUNT = 106
MOVEMENT_PAYLOAD_VA = 0x62980
DESCRIPTOR_SOURCE_SPANS = (
    # G1完整Domain caller读集与G2/G4已审原义；摘要为raw-unrelocated原页。
    ("cell_execute", 0x190AC, 0x1956B, "4e78883e3c83d7043ab94084479b15ceb07b7732793066428d577873aa12a70a"),
    ("drop_execute", 0x1AA1D, 0x1ACF3, "01f0b93310d7441ab6e1ede05c93bde7a32e0bd15b3b24bf283a8f1f04099267"),
    ("ai_story_collector", 0x1B653, 0x1B6B7, "eac8dc89737cdcd6b8aad22ef15caba0737e96a41f32f0b57a720fc99baf4b29"),
    ("defeated_collector", 0x1B6B7, 0x1B722, "24d4f68717d397e47be71887e91b36a2b8baa69e54f66fd3f853da4fadea799e"),
    ("collector_return_tail", 0x1B64D, 0x1B653, "c6d7835030e05f1ba551d25d1b51f942eae5c9ca1193fb2e579317f7a572b7db"),
    ("drop_return_tail", 0x22BBE, 0x22BC6, "b389950002ba37dbf8ee72e175c575064b3a60d86d656fbc1a0aebbdf240f405"),
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def archive_entries(data):
    """严格解析本作三份已核顶层DAT的完整目录及EOF哨兵，不扫描payload猜目录。"""
    if len(data) < 14 or data[:6] != b"LLLLLL":
        raise ValueError("invalid top-level DAT header")
    first = struct.unpack_from("<I", data, 6)[0]
    if first < 14 or first >= len(data) or (first - 6) % 4:
        raise ValueError("invalid DAT directory extent")
    offsets = struct.unpack_from(f"<{(first - 6) // 4}I", data, 6)
    if offsets[0] != first or offsets[-1] != len(data) or any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise ValueError("DAT directory is not complete/increasing with an EOF sentinel")
    return [(at, data[at:end]) for at, end in zip(offsets, offsets[1:])]


def parse_metadata(data):
    if len(data) < 0x83 or (len(data) - 0x83) % 26:
        raise ValueError("metadata prefix/template extent invalid")
    return {
        "shape": data[0], "unknown_01": data[1], "unknown_02": data[2],
        "turn": [tuple(data[3 + i * 3:6 + i * 3]) for i in range(16)],
        "lookup": [tuple(data[0x33 + i * 2:0x35 + i * 2]) for i in range(16)],
        "actions": [(data[0x53 + i * 3], struct.unpack_from("<H", data, 0x54 + i * 3)[0]) for i in range(16)],
        "templates": [data[at:at + 26] for at in range(0x83, len(data), 26)],
    }


def parse_grid(data):
    if len(data) < 8:
        raise ValueError("map header truncated")
    width, height = struct.unpack_from("<HH", data)
    if not 1 <= width <= 256 or not 1 <= height <= 256 or len(data) != 4 + width * height * 4:
        raise ValueError("map dimensions/payload mismatch")
    return width, height, struct.unpack_from(f"<{width * height}I", data, 4)


def parse_placements(data):
    if len(data) < 2:
        raise ValueError("placement count truncated")
    count = struct.unpack_from("<H", data)[0]
    if len(data) != 2 + count * 6:
        raise ValueError("placement count/payload mismatch")
    return [struct.unpack_from("<HHH", data, 2 + i * 6) for i in range(count)]


def text_fragment_count(data):
    if len(data) < 2:
        raise ValueError("text offset directory truncated")
    first = struct.unpack_from("<H", data)[0]
    if not first or first % 2 or first > len(data):
        raise ValueError("text directory extent invalid")
    offsets = struct.unpack_from(f"<{first // 2}H", data)
    ends = (*offsets[1:], len(data))
    if offsets[0] != first or any(not first <= at < end <= len(data) or (end - at) % 2 for at, end in zip(offsets, ends)):
        raise ValueError("text fragment offsets invalid")
    return len(offsets)


def load_definitions(directory):
    manifest_path = directory / "manifest.json"
    manifest_raw = manifest_path.read_bytes()
    manifest = flow_generator.decode_json(manifest_raw)
    paths = sorted((directory / "flows").glob("*.json"))
    files = manifest.get("flow_files")
    contracts = manifest.get("flow_contracts", {})
    if (type(manifest.get("schema_version")) is not int or manifest["schema_version"] != 1
            or not isinstance(files, list) or len(files) != len(set(files))
            or set(files) != {path.name for path in paths} or set(contracts) != set(files)):
        raise ValueError("flow manifest/consumer contract coverage mismatch")
    flows, aliases, provenance, documents = {}, {}, [], {}
    inputs = {manifest_path: manifest_raw}
    for path in paths:
        raw = path.read_bytes()
        inputs[path] = raw
        document = flow_generator.validate_flows(flow_generator.decode_json(raw))
        if document["domain"] != path.stem:
            raise ValueError("flow domain/file mismatch")
        flow_generator.validate_source(document["source"], manifest["sources"], path.name)
        documents[path.name] = document
        provenance.append({"file": str(path.relative_to(directory)), "sha256": digest(raw), "source": document["source"],
                           "consumer_manifest_sha256": digest(manifest_raw)})
        local = {flow["id"]: flow for flow in document["flows"]}
        if flows.keys() & local.keys():
            raise ValueError("duplicate flow ID")
        bindings = document.get("bindings", {})
        if aliases.keys() & bindings.keys():
            raise ValueError("duplicate flow aliases")
        flows.update(local)
        aliases.update(bindings)
    if flows.keys() & aliases.keys():
        raise ValueError("alias shadows a real flow")
    all_flows = {**flows, **{alias: flows[target] for alias, target in aliases.items()}}
    for name, document in documents.items():
        flow_generator.validate_flow_contract(document, contracts[name], SCRIPT_IDS, all_flows)
    if any(path.read_bytes() != raw for path, raw in inputs.items()):
        raise ValueError("static definitions changed while auditing")
    return flows, aliases, provenance


def validate_definition_sources(provenance, repository=ROOT, source_files=None):
    """历史定义校验Git blob；新逆向定义必须有实读的同身份原EXE。"""
    verified = {}
    for document in provenance:
        pending = [document["source"]]
        while pending:
            record = pending.pop()
            if (not isinstance(record, dict) or not isinstance(record.get("evidence"), str)
                    or not record["evidence"].strip()):
                raise ValueError("definition source lacks nonempty evidence")
            related = record.get("related_sources", [])
            if not isinstance(related, list):
                raise ValueError("definition related_sources must be a list")
            pending.extend(related)
            # 四种已登记标签只区分来源描述，均须实读同一原EXE；
            # 不按decoded前缀放行，更不把身份通过当作语义/运行已验。
            if record.get("kind") in ("canonical_disassembly", "decoded_control_flow",
                                      "decoded_domain_control_flow", "original_executable"):
                payload = source_files.get("FD2.EXE") if isinstance(source_files, dict) else None
                actual = digest(payload) if isinstance(payload, bytes) else None
                if (record.get("file") != "FD2.EXE" or record.get("file_sha256") != EXE_SHA256 or
                        actual != EXE_SHA256):
                    raise ValueError("executable definition source lacks matching original executable bytes")
                verified["FD2.EXE"] = actual
                continue
            commit, file = record.get("baseline_commit", ""), record.get("file", "")
            if (record.get("kind") != "existing_confirmed_definition"
                    or not isinstance(commit, str) or not re.fullmatch(r"[a-f0-9]{40}", commit)
                    or not isinstance(file, str) or not file.startswith("src/") or ".." in Path(file).parts or ":" in file):
                raise ValueError("definition source lacks an exact historical source identity/evidence")
            key = f"{commit}:{file}"
            if key not in verified:
                result = subprocess.run(["git", "show", key], cwd=repository, capture_output=True, check=False)
                if result.returncode:
                    raise ValueError(f"definition baseline blob is unavailable: {key}")
                verified[key] = digest(result.stdout)
            if verified[key] != record.get("baseline_source_sha256"):
                raise ValueError(f"definition baseline source hash mismatch: {key}")
    return verified


def verify_ida_evidence(directory):
    """清单只证明发布物未变；原始EXE身份仍独立核实，不提升语义可信度。"""
    manifest = (directory / "SHA256SUMS").read_bytes()
    expected = {}
    for line in manifest.decode("utf-8").splitlines():
        match = re.fullmatch(r"([a-f0-9]{64})  ([\w.-]+)", line)
        if not match or match[2] in expected:
            raise ValueError("invalid/duplicate IDA SHA256SUMS entry")
        expected[match[2]] = match[1]
    verified = {"SHA256SUMS": digest(manifest)}
    for name in ("fd2-functions.tsv", "fd2-ida-function-registry.json", "fd2-ida-decomp-all.c", "fd2-program.json"):
        actual = digest((directory / name).read_bytes())
        if actual != expected.get(name):
            raise ValueError(f"published IDA evidence checksum mismatch: {name}")
        verified[name] = actual
    program = json.loads((directory / "fd2-program.json").read_bytes(), object_pairs_hook=unique_keys)
    if program.get("input_sha256") != EXE_SHA256:
        raise ValueError("IDA evidence belongs to a different EXE")
    return verified


def resolve(flow_id, flows, aliases):
    target = aliases.get(flow_id, flow_id)
    return flows.get(target)


def capability(stage, flows, aliases):
    flow = resolve(f"stage.capability.{stage:03d}", flows, aliases)
    if not flow:
        return None
    if len(flow["nodes"]) != 1 or flow["nodes"][0]["op"] != "stage_capability":
        raise ValueError("invalid capability declaration")
    args = flow["nodes"][0]["args"]
    if (len(args) != 4 or any(type(value) is not int for value in args)
            or args[0] not in (0, 1) or any(not 0 <= value <= 96 for value in args[1:3])
            or not 0 <= args[3] <= 20):
        raise ValueError("invalid capability arguments")
    return args


def classify(flow_id, enabled, flows, aliases):
    flow = resolve(flow_id, flows, aliases)
    return {"runtime_id": flow_id, "definition": flow["id"] if flow else None,
            "classification": SUPPORTED if flow and enabled else UNOPENED if flow else UNKNOWN}


def function_evidence(directory, code, code_base):
    """复用已发布IDA的原函数/chunk边界；不从字节相邻猜函数、不改命名。"""
    names = json.loads((directory / "fd2-ida-function-registry.json").read_text(encoding="utf-8"))
    canonical = {row["va"]: row["ida_name"] for row in names["entries"]}
    functions = []
    for row in csv.DictReader(io.StringIO((directory / "fd2-functions.tsv").read_text(encoding="utf-8")), delimiter="\t"):
        start = int(row["start_ea"], 16)
        chunks = []
        for value in re.split(r"[;,]", row["chunks"]):
            first, end = (int(part, 16) for part in value.split("-"))
            if not code_base <= first < end <= code_base + len(code):
                raise ValueError("IDA function chunk outside raw object1")
            data = code[first - code_base:end - code_base]
            chunks.append({"canonical_va": hex(first), "length": len(data), "sha256": digest(data), "raw_hex": data.hex()})
        functions.append({"canonical_va": hex(start), "name": canonical.get(start, row["name"]), "chunks": chunks})
    return functions


def read_dispatch(executable, ida_directory):
    data = executable.read_bytes()
    if digest(data) != EXE_SHA256:
        raise ValueError("FD2.EXE identity mismatch")
    objects = parse_objects(data, parse_le(data))
    code_owner = next(obj for obj in objects if obj.index == 1)
    code = source_range(data, code_owner.relbase, code_owner.vsize)
    raw = source_range(data, 0x51B19, 0x51E61 - 0x51B19)
    fixups = collect_internal_offset_fixups(executable)
    if executable.read_bytes() != data:
        raise ValueError("EXE changed while auditing")
    evidence = verify_ida_evidence(ida_directory)
    functions = function_evidence(ida_directory, code, code_owner.relbase)
    used_functions, tables = {}, {}
    for name, base, count, symbol in DISPATCH:
        rows = []
        for slot in range(count):
            va = base + slot * 4
            target = fixups.get(va)
            if not target or target[0] != 1 or target[2] != code_owner.relbase + target[1]:
                raise ValueError(f"{name}[{slot}] missing verified code fixup")
            owners = []
            for function in functions:
                if any(int(chunk["canonical_va"], 16) <= target[2] < int(chunk["canonical_va"], 16) + chunk["length"] for chunk in function["chunks"]):
                    owners.append(function["canonical_va"])
                    used_functions[function["canonical_va"]] = function
            if not owners:
                raise ValueError(f"{name}[{slot}] has no published IDA chunk owner")
            at = va - 0x51B19
            rows.append({"slot": slot, "source_va": hex(va), "raw_hex": raw[at:at + 4].hex(),
                         "target_object": target[0], "target_offset": hex(target[1]), "target_va": hex(target[2]),
                         "ida_owners": owners, "semantic_decoded": False})
        table = raw[base - 0x51B19:base - 0x51B19 + count * 4]
        tables[name] = {"canonical_va": hex(base), "count": count, "length": len(table), "sha256": digest(table),
                        "ida_symbol": symbol, "records": rows}
    gap = raw[0x51CF9 - 0x51B19:0x51D01 - 0x51B19]
    if gap != struct.pack("<II", 18, 18) or any(va in fixups for va in (0x51CF9, 0x51CFD)):
        raise ValueError("unexpected pointer fixup in the callback/magic boundary gap")
    return tables, {"canonical_va": "0x51cf9", "length": 8, "raw_hex": gap.hex(), "classification": "adjacent_nonpointer_bytes"}, used_functions, evidence


def scan_movement_structure(data):
    """只按原计数观察初始化字节；不以运行时容量或候选相邻跨度裁掉未知原数据。"""
    result = {"complete": False, "consumed_length": None, "groups": [],
              "actors_outside_current_capacity": [], "unconfirmed_directions": []}
    if not data:
        return result
    result["group_count"] = data[0]
    at, actors, directions = 1, set(), set()
    for _ in range(data[0]):
        if at + 2 > len(data):
            return result
        mode, count = data[at:at + 2]
        at += 2
        if at + count * 2 > len(data):
            return result
        pairs = [list(data[offset:offset + 2]) for offset in range(at, at + count * 2, 2)]
        actors.update(actor for actor, _ in pairs if actor >= ACTOR_LIMIT)
        directions.update(direction for _, direction in pairs
                          if direction > 3 and not (mode == 0x80 and direction == 5))
        result["groups"].append({"mode": mode, "pair_count": count, "pairs": pairs})
        at += count * 2
    result.update(complete=True, consumed_length=at, actors_outside_current_capacity=sorted(actors),
                  unconfirmed_directions=sorted(directions))
    return result


def read_movement_table(executable, runtime_sources):
    """完整源指针和payload；36项运行时严格验证与70项未知来源结构分别登记。"""
    data = executable.read_bytes()
    if digest(data) != EXE_SHA256 or set(runtime_sources) != set(SCRIPT_IDS):
        raise ValueError("movement source identity/runtime subset mismatch")
    objects = parse_objects(data, parse_le(data))
    owner = next(obj for obj in objects if obj.index == 3)
    if owner.relbase != 0x60000:
        raise ValueError("movement table has no confirmed object3 owner")
    raw = source_range(data, owner.relbase, owner.vsize)
    end = owner.relbase + owner.vsize
    fixups = collect_internal_offset_fixups(executable)
    if executable.read_bytes() != data:
        raise ValueError("EXE changed while reading the movement table")
    if POINTER_TABLE_VA + MOVEMENT_POINTER_COUNT * 4 != MOVEMENT_PAYLOAD_VA or any(
            va in fixups for va in (POINTER_TABLE_VA - 4, MOVEMENT_PAYLOAD_VA)):
        raise ValueError("movement pointer table boundary mismatch")
    targets = []
    for identity in range(MOVEMENT_POINTER_COUNT):
        target = fixups.get(POINTER_TABLE_VA + identity * 4)
        if (not target or target[0] != 3 or target[2] != owner.relbase + target[1]
                or not MOVEMENT_PAYLOAD_VA <= target[2] < end):
            raise ValueError(f"movement[{identity}]: missing/out-of-range object3 fixup")
        targets.append(target[2])
    if min(targets) != MOVEMENT_PAYLOAD_VA:
        raise ValueError("movement payload start differs from the confirmed source boundary")
    starts = sorted(set(targets))
    records = []
    for identity, target in enumerate(targets):
        candidate_end = next((value for value in starts if value > target), end)
        offset = target - owner.relbase
        candidate = raw[offset:candidate_end - owner.relbase]
        scan = scan_movement_structure(raw[offset:])
        used = scan["consumed_length"]
        relation = ("unresolved_counted_structure" if used is None else
                    "crosses_next_target" if target + used > candidate_end else
                    "candidate_tail_retained" if target + used < candidate_end else "same_extent")
        expected = runtime_sources.get(identity)
        if expected and (expected.canonical_va != target or used != len(expected.data)
                or raw[offset:offset + used] != expected.data
                or scan["actors_outside_current_capacity"] or scan["unconfirmed_directions"]):
            raise ValueError(f"movement[{identity}]: runtime strict source does not match full-table evidence")
        pointer_offset = POINTER_TABLE_VA - owner.relbase + identity * 4
        records.append({"id": identity, "pointer_va": hex(POINTER_TABLE_VA + identity * 4),
                        "pointer_raw_hex": raw[pointer_offset:pointer_offset + 4].hex(),
                        "target_object": 3, "target_offset": hex(offset), "target_va": hex(target),
                        "candidate_span": {"canonical_va": hex(target), "length": len(candidate),
                            "sha256": digest(candidate), "raw_hex": candidate.hex(), "boundary_confirmed": False,
                            "basis": "next different target or initialized object end; candidate allocation only"},
                        "counted_structure_observation": scan, "candidate_relation": relation,
                        "stream_syntax_status": "runtime_strict_verified" if expected else "source_structure_only",
                        "runtime_supported": bool(expected), "semantic_decoded": bool(expected)})
    pointer_raw = raw[POINTER_TABLE_VA - owner.relbase:MOVEMENT_PAYLOAD_VA - owner.relbase]
    payload = raw[MOVEMENT_PAYLOAD_VA - owner.relbase:]
    return {"canonical_va": hex(POINTER_TABLE_VA), "count": MOVEMENT_POINTER_COUNT, "length": len(pointer_raw),
            "sha256": digest(pointer_raw), "raw_hex": pointer_raw.hex(), "pointer_complete": True,
            "evidence": "field_movement_script_ptr @0x4eb48; 106 object3 fixups to payload ending at initialized object3 size; getter has no count guard",
            "payload": {"canonical_va": hex(MOVEMENT_PAYLOAD_VA), "length": len(payload), "sha256": digest(payload), "raw_hex": payload.hex()},
            "records": records, "semantic_complete": False,
            "summary": {"pointer_slots": len(records), "runtime_strict_streams": len(runtime_sources),
                "runtime_unknown_or_unopened": len(records) - len(runtime_sources),
                "counted_structure_observations": sum(row["counted_structure_observation"]["complete"] for row in records),
                "cross_next_target_ids": [row["id"] for row in records if row["candidate_relation"] == "crosses_next_target"],
                "retained_candidate_tail_ids": [row["id"] for row in records if row["candidate_relation"] == "candidate_tail_retained"],
                "actors_outside_current_capacity_ids": [row["id"] for row in records if row["counted_structure_observation"]["actors_outside_current_capacity"]],
                "unconfirmed_direction_ids": [row["id"] for row in records if row["counted_structure_observation"]["unconfirmed_directions"]]}}


def descriptor_source_evidence(executable):
    """独立原页核完整caller／共享返回尾，不从冷图或当前C取expected。"""
    raw = Path(executable).read_bytes()
    if digest(raw) != EXE_SHA256:
        raise ValueError("descriptor original EXE identity mismatch")
    spans = []
    for caller, start, end, expected in DESCRIPTOR_SOURCE_SPANS:
        actual = source_range(raw, start, end-start)
        if digest(actual) != expected:
            raise ValueError(f"descriptor original caller bytes differ: {caller}")
        spans.append({"caller": caller, "canonical_va": hex(start), "end": hex(end),
                      "length": end-start, "raw_unrelocated_sha256": expected})
    return {"file": "FD2.EXE", "sha256": EXE_SHA256, "address_view": "canonical relbase VA; raw-unrelocated",
            "spans": spans, "strict_source_bytes_checked": True,
            "semantic_basis": ["docs/reverse-engineering/common-battle-semantics.md#64-descriptor默认分支是有依据的无效果",
                               "docs/reverse-engineering/common-runtime-semantics.md"],
            "independent_original_probes": ["tools/ida/verify_fd2_descriptor_default.py", "tools/ida/verify_fd2_special_cell.py"],
            "original_machine_code_executed_this_audit": False, "new_gameplay_enabled": False}


def descriptor_semantics(mode, param, *, caller="drop_execute"):
    """原mode／WORD按真实caller解释；collector只核mode过滤，不声称可达。"""
    if type(mode) is not int or not 0 <= mode <= 255 or type(param) is not int or not 0 <= param <= 65535:
        raise ValueError("descriptor requires raw BYTE mode and WORD param")
    if caller not in ("drop_execute", "defeated_collector", "ai_story_collector", "cell_execute"):
        raise ValueError("unknown descriptor caller")
    result = {"caller": caller, "param_u16": param, "callback_u16": None, "callback_in_table": None}
    sources = [hex(next(a for name, a, _, _ in DESCRIPTOR_SOURCE_SPANS if name == caller))]
    if caller == "defeated_collector" and mode == 255:
        kind, reason = "collector_sentinel", "1b6b7 excludes mode255 before queue execution; not the direct1aa1d default"
    elif caller == "ai_story_collector" and mode != 3:
        kind, reason = "collector_filtered", "1b653 only collects mode3; runtime nonhidden/HP0 eligibility is separate"
    else:
        if caller in ("defeated_collector", "ai_story_collector"):
            sources.append("0x1aa1d")
        kind = "callback" if caller == "cell_execute" and mode >= 2 else {
            0: "item", 1: "money", 2: "callback", 3: "story"}.get(mode, "default_noop")
        if kind == "callback":
            result.update(callback_u16=param, callback_in_table=param < 90)
            reason = ("190ac non0/1 mode" if caller == "cell_execute" else "1aa1d mode2") + \
                " dispatches full WORD into the90-slot callback table; " + \
                ("exact stage binding remains required" if param < 90 else "out-of-table target is explicitly refused; no low-BYTE alias")
        elif kind == "default_noop":
            reason = "1aa1d modes4..255 clear input per descriptor then continue; no business/RNG effect"
        else:
            reason = f"{'190ac' if caller == 'cell_execute' else '1aa1d'} confirmed {kind} branch; raw WORD retained"
    result.update(semantic_kind=kind, classification_reason=reason, source_canonical_vas=sources)
    return result


def legacy_descriptor(mode, param, stage, enabled, fragments, flows, aliases, *, cell=False):
    """保留SD3旧读集的分类快照，仅供差异账；不作当前原义判据。"""
    result = {"mode": mode, "param_u16": param, "classification": UNKNOWN, "reference_errors": []}
    if mode == 255 and not cell:
        result["classification"] = "unused_descriptor"
        return result
    if mode not in ((0, 1) if cell else (0, 1, 2, 3)):
        return result
    if mode == 0 and param >= 215:
        result["reference_errors"].append("item ID outside the confirmed table")
    elif mode == 2:
        binding = classify(f"stage.{stage:03d}.action.{param & 255:03d}", enabled, flows, aliases)
        result.update(binding)
    elif mode == 3 and param >= fragments:
        result["reference_errors"].append("story fragment outside stage text entry")
    if mode != 2:
        result["classification"] = SUPPORTED if enabled and "loot" in flows else UNOPENED if "loot" in flows else UNKNOWN
    if result["reference_errors"]:
        result["classification"] = UNKNOWN
    return result


def descriptor(mode, param, stage, enabled, fragments, flows, aliases, *, cell=False, caller=None):
    if caller is None:
        caller = "cell_execute" if cell else "defeated_collector"
    if cell and caller != "cell_execute":
        raise ValueError("cell descriptor caller mismatch")
    semantic = descriptor_semantics(mode, param, caller=caller)
    legacy = legacy_descriptor(mode, param, stage, enabled, fragments, flows, aliases, cell=caller == "cell_execute")
    result = {"mode": mode, "classification": UNKNOWN, "reference_errors": [], **semantic,
              "legacy_classification": legacy["classification"],
              "legacy_reference_errors": legacy["reference_errors"],
              "legacy_reason": "SD3 audit mode0..3/cell0..1 and mode2 low-BYTE classification; historical comparison only"}
    if "runtime_id" in legacy:
        result["legacy_runtime_id"] = legacy["runtime_id"]
    kind = semantic["semantic_kind"]
    if kind in ("collector_sentinel", "collector_filtered"):
        result["classification"] = "unused_descriptor"
        return result
    if kind == "callback":
        # 先核原完整WORD域，再查精确stage/action绑定；同target不授予支持。
        if semantic["callback_in_table"]:
            result.update(classify(f"stage.{stage:03d}.action.{param:03d}", enabled, flows, aliases))
        return result
    if kind == "item" and param >= 215:
        result["reference_errors"].append("item ID outside the confirmed table")
    elif kind == "story" and param >= fragments:
        result["reference_errors"].append("story fragment outside stage text entry")
    if not result["reference_errors"]:
        result["classification"] = SUPPORTED if enabled and "loot" in flows else UNOPENED if "loot" in flows else UNKNOWN
    return result


def bound_stage_flows(stage, flows, aliases):
    for key in sorted(set(flows) | set(aliases)):
        match = re.fullmatch(r"(?:stage\.(?:entry|result|victory|settlement)|scene\.(?:init|opening))\.(\d+)|stage\.(\d+)\.(?:action\.\d+|result)", key)
        if match and int(match.group(1) or match.group(2)) == stage:
            yield key, resolve(key, flows, aliases)


def audit_stage(stage, field, shape, text, flows, aliases, dispatch):
    map_offset, grid = field[stage * 3]
    meta_offset, raw_meta = field[stage * 3 + 1]
    placement_offset, raw_placement = field[stage * 3 + 2]
    width, height, cells = parse_grid(grid)
    metadata = parse_metadata(raw_meta)
    placements = parse_placements(raw_placement)
    attr_index = metadata["shape"] * 2 + 1
    if attr_index >= len(shape):
        raise ValueError(f"stage{stage}: FDSHAP shape reference outside archive")
    attr_offset, attrs = shape[attr_index]
    if not attrs or len(attrs) % 4:
        raise ValueError("terrain attributes are not complete 4-byte records")
    if stage + 1 >= len(text):
        raise ValueError("stage text entry is missing")
    fragments = text_fragment_count(text[stage + 1][1])
    cap = capability(stage, flows, aliases)
    enabled = bool(cap and cap[0])
    requirements, errors = [], []

    def action_binding(action):
        binding = classify(f"stage.{stage:03d}.action.{action:03d}", enabled, flows, aliases)
        binding["original_callback"] = dispatch["event_callback"]["records"][action] if action < 90 else None
        if action >= 90:
            binding["classification"] = UNKNOWN
        # 候选仅供复用研究，绝不因同action/target自动变成已支持。
        binding["reuse_candidates"] = sorted({flow["id"] for key in (*flows, *aliases)
            if re.fullmatch(r"stage\.\d+\.action\." + f"{action:03d}", key)
            and (flow := resolve(key, flows, aliases)) and key != binding["runtime_id"]})
        return binding

    turns = []
    for slot, (turn, action, phase) in enumerate(metadata["turn"]):
        default = (turn, action, phase) == (255, 255, 0)
        row = {"slot": slot, "entry_offset": 3 + slot * 3, "raw_hex": raw_meta[3 + slot * 3:6 + slot * 3].hex(),
               "turn": turn, "action": action, "phase": phase, "default_fill": default}
        row.update(action_binding(action))
        if default:
            # 查询按turn/phase直接匹配，未证这些填充在turn255可忽略；不推成无效事件。
            row.update(turn255_potential_match=True, ignore_semantics_verified=False,
                       unresolved_reason="default (255,255,0) can match turn255/phase0; callback255 is not decoded")
        requirements.append(row)
        turns.append(row)
    lookups = []
    for slot, (action, match) in enumerate(metadata["lookup"]):
        row = {"slot": slot, "lookup_id": slot + 1, "entry_offset": 0x33 + slot * 2,
               "raw_hex": raw_meta[0x33 + slot * 2:0x35 + slot * 2].hex(), "action": action, "match_arg": match}
        if action != 255:
            row.update(action_binding(action)); requirements.append(row)
        lookups.append(row)
    event_cells, active_actions = [], set()
    for index, cell in enumerate(cells):
        terrain, reference = cell & 0x3FF, (cell >> 16) & 31
        if terrain >= len(attrs) // 4:
            errors.append(f"map cell {index}: terrain attribute reference outside table"); continue
        special = attrs[terrain * 4] & 0x60
        if special or reference:
            row = {"x": index % width, "y": index // width, "raw_u32": cell, "terrain": terrain,
                   "terrain_flags60": special, "index": reference, "index_base": 0 if special else 1}
            event_cells.append(row)
            if special:
                if reference >= 16: errors.append(f"map cell {index}: special action outside 16 slots")
                else: active_actions.add(reference)
            elif reference > 16:
                errors.append(f"map cell {index}: normal lookup outside 16 slots")
    actions = []
    for slot, (mode, param) in enumerate(metadata["actions"]):
        row = {"slot": slot, "entry_offset": 0x53 + slot * 3,
               "raw_hex": raw_meta[0x53 + slot * 3:0x56 + slot * 3].hex(), "referenced_by_map": slot in active_actions}
        row.update(descriptor(mode, param, stage, enabled, fragments, flows, aliases, cell=True))
        if slot in active_actions: requirements.append(row)
        actions.append(row)
    by_id = defaultdict(list)
    for index, (_, _, unit) in enumerate(placements): by_id[unit].append(index)
    ordinal, templates = Counter(), []
    effective_count = metadata["unknown_02"]
    if effective_count > len(metadata["templates"]):
        errors.append("metadata byte2 effective template count exceeds retained template resource")
    required_groups = {node["args"][0] for _, flow in bound_stage_flows(stage, flows, aliases)
                       for node in flow["nodes"] if node["op"] == "append_group"}
    for index, template in enumerate(metadata["templates"]):
        unit = template[1]
        candidates = by_id[unit]
        legacy_placement = candidates[ordinal[unit]] if ordinal[unit] < len(candidates) else None
        ordinal[unit] += 1
        consumed = index < effective_count
        placement = index if consumed and index < len(placements) else None
        required = consumed and template[21] in required_groups
        if consumed and placement is None:
            errors.append(f"template {index}: effective prefix needs a missing same-index placement")
        drop = descriptor(template[22], int.from_bytes(template[23:25], "little"), stage, enabled, fragments, flows, aliases)
        if drop["classification"] != "unused_descriptor": requirements.append(drop)
        templates.append({"index": index, "entry_offset": 0x83 + index * 26, "raw_hex": template.hex(),
                          "unit_id": unit, "group": template[21], "placement_index": placement,
                          "required_by_bound_append_group": required,
                          "effective_template": consumed,
                          "placement_relation": "same_index_low_byte" if placement is not None else
                              "missing_effective_index" if consumed else "resource_tail_not_consumed",
                          "placement_coordinates_low_byte": [placements[index][0] & 255,placements[index][1] & 255] if placement is not None else None,
                          "legacy_id_ordinal_placement_index": legacy_placement,
                          "drop": drop})
    # 只做当前已有冷定义的引用检查；不根据IDA命名推导新事件语义。
    for key, flow in bound_stage_flows(stage, flows, aliases):
        for node in flow["nodes"]:
            op, args = node["op"], node["args"]
            if op == "move" and args[0] not in SCRIPT_IDS: errors.append(f"{key}/{node['id']}: unsupported movement script")
            # 原10b4e没有匹配group时正常返回；保留空group，而非编造资源依赖。
            if op in ("text", "story") and not 0 <= args[0] < fragments: errors.append(f"{key}/{node['id']}: text fragment out of range")
    stages = {}
    for table, kind in (("stage_runtime_result", "result"), ("stage_entry", "entry"), ("stage_victory", "victory")):
        original = dispatch[table]["records"][stage] if stage < dispatch[table]["count"] else None
        start_entry = bool(original and original["target_va"] == "0x3231b" and "scene.new_game" in flows)
        # 入场/准备不要求开放战斗；stage0使用已证START图，其余取cap的准备槽声明。
        source_entry = resolve(f"stage.entry.{stage:03d}", flows, aliases)
        source_entry_bound = bool(source_entry and source_entry["id"].startswith("stage.source.entry."))
        entry_enabled = bool(cap and (cap[3] or start_entry or source_entry_bound))
        runtime_id=f"stage.{kind}.{stage:03d}"
        runtime_domain="stage"
        if kind=="result" and resolve(f"stage.{stage:03d}.result",flows,aliases):
            if resolve(runtime_id,flows,aliases):raise ValueError("duplicate pure/effect result runtime binding")
            runtime_id=f"stage.{stage:03d}.result";runtime_domain="common_event"
        binding = classify(runtime_id, entry_enabled if kind == "entry" else enabled, flows, aliases)
        binding["runtime_domain"]=runtime_domain
        # 已证new_game_opening_play入口是显式新游戏图，不伪造stage.entry.000运行别名。
        if kind == "entry" and start_entry:
            binding = {"runtime_id": None, "definition": "scene.new_game", "classification": SUPPORTED if entry_enabled else UNOPENED,
                       "evidence": "new_game_opening_play @0x3231b; explicit START path, not a new runtime alias"}
        stages[kind] = {"original": original, **binding}
        if original: requirements.append(binding)
    unknown = [row for row in requirements if row.get("classification") != SUPPORTED]
    bound_unknown = [row for row in unknown if not row.get("default_fill", False)]
    victory_flow=resolve(f"stage.victory.{stage:03d}",flows,aliases)
    source_victory=bool(victory_flow and victory_flow["id"].startswith("stage.source.victory."))
    settlement_flow=resolve(f"stage.settlement.{stage:03d}",flows,aliases)
    return {"stage": stage, "battle_enabled": enabled, "entry_supported": stages["entry"]["classification"] == SUPPORTED, "capability": cap,
            "map": {"entry": stage * 3, "file_offset": map_offset, "width": width, "height": height,
                    "length": len(grid), "sha256": digest(grid), "raw_hex": grid.hex(), "event_cells": event_cells},
            "metadata": {"entry": stage * 3 + 1, "file_offset": meta_offset, "length": len(raw_meta), "sha256": digest(raw_meta),
                         "raw_hex": raw_meta.hex(), "shape_index": metadata["shape"], "unknown_01": metadata["unknown_01"],
                         "unknown_02": metadata["unknown_02"], "effective_template_count": effective_count,
                         "retained_template_count": len(templates), "turn_slots": turns, "cell_lookup_slots": lookups,
                         "cell_action_slots": actions, "templates": templates},
            "placements": {"entry": stage * 3 + 2, "file_offset": placement_offset, "length": len(raw_placement),
                           "sha256": digest(raw_placement), "raw_hex": raw_placement.hex(), "records": [list(p) for p in placements]},
            "terrain_attributes": {"entry": attr_index, "file_offset": attr_offset, "length": len(attrs), "sha256": digest(attrs)},
            "text_reference": {"entry": stage + 1, "fragment_count": fragments}, "dispatch": stages,
            "handoff_contract":{"kind":"source_victory_once_then_export_preparation" if source_victory else
                "legacy_settlement" if settlement_flow else "unbound",
                "legacy_settlement_required":bool(settlement_flow and not source_victory),
                "settlement_definition":settlement_flow["id"] if settlement_flow else None,
                "source_victory_definition":victory_flow["id"] if source_victory else None,
                "owner":"Scene consumes source once; fd2_field_game_export_preparation only copies; App closes old resources before next entry",
                "old_resources_can_run_next_stage_controller":False},
            "reference_errors": errors, "active_classification_counts": dict(Counter(row["classification"] for row in requirements)),
            "legacy_active_classification_counts": dict(Counter(row.get("legacy_classification", row["classification"]) for row in requirements)),
            "unsupported_active_requirements": unknown,
            "bound_definitions_closed": bool(enabled and not bound_unknown and not errors),
            "selected_semantics_supported": bool(enabled and not unknown and not errors), "semantic_complete": False}


def require_supported(report):
    failed = [stage["stage"] for stage in report["stages"] if not stage["selected_semantics_supported"]]
    if failed:
        raise ValueError(f"unknown/unopened semantics, potential turn255 matches or unresolved references in stages {failed}; structural success is not support")


def audit(original, definitions=ROOT / "data/static", ida_directory=ROOT / "docs/generated/ida-canonical", stages=None):
    sources = {name: (original / name).read_bytes() for name in SOURCE_HASHES}
    for name, raw in sources.items():
        if digest(raw) != SOURCE_HASHES[name]: raise ValueError(f"{name}: source identity mismatch")
    field, shape, text = (archive_entries(sources[name]) for name in ("FDFIELD.DAT", "FDSHAP.DAT", "FDTXT.DAT"))
    if len(field) != 99: raise ValueError("confirmed FDFIELD must contain exactly99 entries/33 stages")
    selected = list(range(33)) if stages is None else sorted(set(stages))
    if any(type(stage) is not int or not 0 <= stage < 33 for stage in selected) or not selected:
        raise ValueError("stage selection must be in0..32")
    flows, aliases, definition_sources = load_definitions(definitions)
    baseline_sources = validate_definition_sources(definition_sources, source_files=sources)
    tables, gap, functions, ida_sources = read_dispatch(original / "FD2.EXE", ida_directory)
    scripts = source_scripts(original / "FD2.EXE")
    movement_table = read_movement_table(original / "FD2.EXE", scripts)
    rows = [audit_stage(stage, field, shape, text, flows, aliases, tables) for stage in selected]
    return {"schema_version": 1, "structural_success": True, "semantic_complete": False,
            "claims": {"original_runtime_executed": False, "all_game_equivalent": False, "new_gameplay_enabled": False,
                       "default_turn_fill_retained": True, "default_fill_does_not_prove_turn255_safe": True},
            "sources": {name: {"sha256": digest(raw), "length": len(raw)} for name, raw in sources.items()},
            "definition_sources": definition_sources, "verified_baseline_blobs": baseline_sources,
            "flow_bindings": aliases, "ida_evidence_sources": ida_sources,
            "descriptor_source_evidence": descriptor_source_evidence(original / "FD2.EXE"),
            "dispatch_tables": tables, "adjacent_nonpointer_bytes": gap, "function_catalog": functions,
            "movement_sources": {str(i): {"canonical_va": hex(s.canonical_va), "length": len(s.data), "sha256": digest(s.data), "raw_hex": s.data.hex()} for i, s in scripts.items()},
            "movement_source_scope": "movement_sources contains all 106 reviewed counted streams; movement_pointer_table separately preserves all source slots and the complete 2898-byte raw candidate payload (counted stream sum 2896, overlapping/remaining candidates retained). Runtime grammar availability does not enable stage bindings",
            "movement_pointer_table": movement_table,
            "stages": rows, "summary": {"resource_entries": len(field), "resource_stage_groups": 33,
                "selected_stage_groups": len(rows), "primary_dispatch_slots": sum(tables[name]["count"] for name in tables if name != "adjacent_magic"),
                "adjacent_magic_slots": tables["adjacent_magic"]["count"], "preserved_turn_slots": len(rows) * 16,
                "movement_pointer_slots": movement_table["count"], "movement_payload_bytes": movement_table["payload"]["length"],
                "movement_runtime_strict_streams": len(scripts), "movement_runtime_unknown_or_unopened": MOVEMENT_POINTER_COUNT - len(scripts),
                "preserved_lookup_slots": len(rows) * 16, "preserved_action_slots": len(rows) * 16,
                "templates": sum(len(r["metadata"]["templates"]) for r in rows),
                "placements": sum(len(r["placements"]["records"]) for r in rows),
                "unresolved_template_placements": sum(t["placement_index"] is None for r in rows for t in r["metadata"]["templates"]),
                "placement_count_compatibility_note": "unresolved_template_placements retains the historical missing-consumption count; use the effective/tail counts to distinguish current meaning",
                "unresolved_effective_template_placements": sum(t["effective_template"] and t["placement_index"] is None for r in rows for t in r["metadata"]["templates"]),
                "unconsumed_resource_template_tails": sum(not t["effective_template"] for r in rows for t in r["metadata"]["templates"]),
                "default_turn255_candidates": sum(t["default_fill"] for r in rows for t in r["metadata"]["turn_slots"]),
                "active_classification_counts": dict(sum((Counter(r["active_classification_counts"]) for r in rows), Counter())),
                "legacy_active_classification_counts": dict(sum((Counter(r["legacy_active_classification_counts"]) for r in rows), Counter())),
                "bound_definitions_closed_stages": [r["stage"] for r in rows if r["bound_definitions_closed"]],
                "supported_selected_stages": [r["stage"] for r in rows if r["selected_semantics_supported"]],
                "reference_error_count": sum(len(r["reference_errors"]) for r in rows)}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--definitions", type=Path, default=ROOT / "data/static")
    parser.add_argument("--ida-evidence", type=Path, default=ROOT / "docs/generated/ida-canonical")
    parser.add_argument("--stage", type=int, action="append")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-supported", action="store_true")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        if output.exists():
            raise ValueError("audit output already exists; keep historical report and choose a new path")
        if any(output.is_relative_to(path.resolve()) for path in (args.original, args.definitions, args.ida_evidence, ROOT / "docs")):
            raise ValueError("report must not overwrite original resources, definitions or published evidence/docs")
        report = audit(args.original, args.definitions, args.ida_evidence, args.stage)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if args.require_supported: require_supported(report)
        print(json.dumps({"structural_success": True, "semantic_complete": False, "output": str(output), **report["summary"]}, ensure_ascii=False))
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        parser.exit(1, f"关卡批量审计失败：{error}\n")


if __name__ == "__main__":
    main()
