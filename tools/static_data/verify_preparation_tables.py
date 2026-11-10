#!/usr/bin/env python3
"""G3 准备冷表的独立严格 LE 原字节／调用常量核验，不导入生成器。"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct

from verify_unit_tables import EXE_SHA256, ROOT, source_range
from verify_stage_tables import unique_object

# G1准备读集、完整caller及4e7dd getter确定；不取待验source自报地址。
SPECS = (
    ("k_prepare_no_town", "uint8_t", (30,), 0x523e7),
    ("k_prepare_revive_coefficients", "uint16_t", (29,), 0x52397),
    ("k_prepare_promotion_records", "uint8_t", (36, 2), 0x615fe),
    ("k_prepare_special_items", "uint8_t", (18,), 0x523d5),
    ("k_prepare_town_x", "uint8_t", (3, 6), 0x52363),
    ("k_prepare_town_y", "uint8_t", (3, 6), 0x52375),
    ("k_prepare_town_portraits", "uint8_t", (6,), 0x52387),
    ("k_prepare_town_background_entries", "uint8_t", (3,), 0x52405),
    ("k_prepare_service_return_prefix", "uint16_t", (5,), 0x5238d),
    ("k_prepare_menu_slide_offsets", "int32_t", (4,), 0x52408),
    ("k_prepare_menu_cursor_offsets", "int32_t", (4,), 0x52418),
    ("k_prepare_buy_messages", "uint16_t", (4, 6), 0x52428),
    ("k_prepare_sell_messages", "uint16_t", (2, 6), 0x52458),
)
FORMATS = {"uint8_t": ("B", 0, 255), "uint16_t": ("H", 0, 65535),
           "int32_t": ("i", -2147483648, 2147483647)}
SERVICE_RANGES = ["0x29327", "0x279d7", "0x279f9", "0x27a09", "0x29dd1"]
SERVICE_ENTRIES = [13, 12, 255, 29, 14, 63]
SERVICE_ANCHORS = ((0x29327, "6a0d"), (0x279d7, "be0c000000"),
                   (0x279f9, "be1d000000"), (0x27a09, "be3f000000"), (0x29dd1, "6a0e"))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def table_bytes(table, spec):
    name, c_type, dimensions, _ = spec
    if (table.get("name"), table.get("c_type"), table.get("dimensions")) != (name, c_type, list(dimensions)):
        raise ValueError(f"{name}: name/type/extent differs")
    fmt, low, high = FORMATS[c_type]

    def flatten(value, dims):
        if not dims:
            if type(value) is not int or not low <= value <= high:
                raise ValueError(f"{name}: invalid integer")
            return [value]
        if not isinstance(value, list) or len(value) != dims[0]:
            raise ValueError(f"{name}: extent differs")
        return [n for child in value for n in flatten(child, dims[1:])]

    values = flatten(table.get("values"), dimensions)
    return struct.pack(f"<{len(values)}{fmt}", *values)


def check_table(table, spec, expected):
    name, _, _, va = spec
    if table_bytes(table, spec) != expected:
        raise ValueError(f"{name}: original bytes differ")
    source = table.get("source", {})
    identity = {"file": "FD2.EXE", "file_sha256": EXE_SHA256,
                "object": 2 if va < 0x60000 else 3, "canonical_va": hex(va),
                "length": len(expected), "sha256": sha(expected), "view": "raw-unrelocated"}
    if any(source.get(k) != value for k, value in identity.items()) or not source.get("evidence"):
        raise ValueError(f"{name}: source identity differs")


def check(document, executable):
    if sha(executable) != EXE_SHA256:
        raise ValueError("FD2.EXE identity mismatch")
    if type(document.get("schema_version")) is not int or document["schema_version"] != 1:
        raise ValueError("preparation schema differs")
    tables = {t["name"]: t for t in document["tables"]}
    names = {s[0] for s in SPECS} | {"k_prepare_town_service_entries", "k_prepare_deployment_roster_gate",
                                   "k_prepare_notice_portrait_origins"}
    if len(tables) != len(document["tables"]) or set(tables) != names:
        raise ValueError("preparation exact table coverage differs")
    total = 0
    for spec in SPECS:
        count = 1
        for n in spec[2]:
            count *= n
        size = count * struct.calcsize(FORMATS[spec[1]][0])
        check_table(tables[spec[0]], spec, source_range(executable, spec[3], size))
        total += size
    service = tables["k_prepare_town_service_entries"]
    if table_bytes(service, ("k_prepare_town_service_entries", "uint8_t", (6,), 0)) != bytes(SERVICE_ENTRIES):
        raise ValueError("preparation service dispatch differs")
    source = service.get("source", {})
    if (source.get("file") != "FD2.EXE" or source.get("file_sha256") != EXE_SHA256 or
            source.get("kind") != "decoded_call_constants" or
            source.get("canonical_ranges") != SERVICE_RANGES or not source.get("evidence")):
        raise ValueError("preparation service call source differs")
    for va, code in SERVICE_ANCHORS:
        if source_range(executable, va, len(bytes.fromhex(code))) != bytes.fromhex(code):
            raise ValueError("preparation original service argument differs")
    gate = tables["k_prepare_deployment_roster_gate"]
    if table_bytes(gate, ("k_prepare_deployment_roster_gate", "uint8_t", (1, 4), 0)) != bytes((0, 12, 6, 2)):
        raise ValueError("preparation deployment predicate differs")
    source = gate.get("source", {})
    if (source.get("file") != "FD2.EXE" or source.get("file_sha256") != EXE_SHA256 or
            source.get("kind") != "decoded_predicate" or
            source.get("canonical_ranges") != ["0x10ae1", "0x10ae8", "0x10af1"] or
            source.get("columns") != ["first_stage", "last_stage", "active_slot", "required_text_id"] or
            not source.get("evidence")):
        raise ValueError("preparation deployment predicate source differs")
    for va, code in ((0x10ae1, "837c241c0d"), (0x10ae8, "83fd06"), (0x10af1, "83f802")):
        if source_range(executable, va, len(bytes.fromhex(code))) != bytes.fromhex(code):
            raise ValueError("preparation original deployment compare differs")
    origins = [4283, 1707, 3939, 1398, 3644]
    addresses = [0x19604, 0x19618, 0x1962c, 0x19640, 0x19654]
    portrait = tables["k_prepare_notice_portrait_origins"]
    if table_bytes(portrait, ("k_prepare_notice_portrait_origins", "uint16_t", (5,), 0)) != struct.pack("<5H", *origins):
        raise ValueError("preparation special portrait origins differ")
    source = portrait.get("source", {})
    if (source.get("file") != "FD2.EXE" or source.get("file_sha256") != EXE_SHA256 or
            source.get("kind") != "decoded_call_constants" or
            source.get("canonical_ranges") != [hex(v) for v in addresses] or not source.get("evidence")):
        raise ValueError("preparation portrait source differs")
    for va, value in zip(addresses, origins):
        if source_range(executable, va, 10) != bytes.fromhex("c705673c0000") + struct.pack("<I", value):
            raise ValueError("preparation original portrait constant differs")
    # 独立原源投影结果；不把JSON中的费用行0变成有效职业引用。
    base = source_range(executable, 0x61da1, 32 * 24)
    promotion = source_range(executable, 0x615fe, 36 * 2)
    profiles = sorted(set(base[1::24]) | set(promotion[::2]))
    if any(not 1 <= p <= 28 for p in profiles):
        raise ValueError("new original revival profile requires review")
    prefix = struct.unpack("<5H", source_range(executable, 0x5238d, 10))
    neighbor = struct.unpack("<H", source_range(executable, 0x52397, 2))[0]
    return {"ok": True, "exe_sha256": EXE_SHA256, "raw_tables": len(SPECS), "raw_bytes": total,
            "decoded_service_tables": 1, "decoded_service_entries": SERVICE_ENTRIES,
            "decoded_deployment_predicates": [[0, 12, 6, 2]],
            "decoded_portrait_origins": origins,
            "source_anchors": SERVICE_RANGES,
            "expected_projections": {"k_prepare_revive_profile_valid": [int(p in profiles) for p in range(29)],
                                     "k_prepare_service_return_fragments": list(prefix) + [neighbor]},
            "neighbor_profile0_is_fee": False, "stage_prepare_duplicated": False,
            "original_runtime_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--tables", type=Path, default=ROOT / "data/static/tables/preparation.json")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    document = json.loads(args.tables.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    executable = args.executable.read_bytes()
    report = check(document, executable)
    if args.self_test:
        def table(d, name):
            return next(t for t in d["tables"] if t["name"] == name)
        mutations = (lambda d: table(d,"k_prepare_no_town")["values"].__setitem__(0, 255),
                     lambda d: table(d,"k_prepare_revive_coefficients")["source"].update(canonical_va="0x52399"),
                     lambda d: table(d,"k_prepare_town_service_entries")["values"].__setitem__(2, 13),
                     lambda d: table(d,"k_prepare_town_service_entries")["source"].update(kind="raw_unchecked"),
                     lambda d: table(d,"k_prepare_deployment_roster_gate")["values"][0].__setitem__(1, 13),
                     lambda d: table(d,"k_prepare_notice_portrait_origins")["values"].__setitem__(0, 1707),
                     lambda d: d["tables"].append(copy.deepcopy(d["tables"][0])))
        for mutate in mutations:
            bad = copy.deepcopy(document)
            mutate(bad)
            try:
                check(bad, executable)
            except ValueError:
                continue
            raise ValueError("preparation corruption was accepted")
        report["rejected_mutations"] = len(mutations)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
