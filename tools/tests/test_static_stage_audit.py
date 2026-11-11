"""批量关卡审计的无资源负例；不作为原版运行或完整语义等价证据。"""
import contextlib
import copy
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "static_data"))
import audit_stage_data as audit


def archive(entries):
    first = 6 + (len(entries) + 1) * 4
    offsets = [first]
    for entry in entries:
        offsets.append(offsets[-1] + len(entry))
    return b"LLLLLL" + struct.pack(f"<{len(offsets)}I", *offsets) + b"".join(entries)


def flow(identity, op="end", args=None):
    return {"id": identity, "entry": "start", "nodes": [
        {"id": "start", "op": op, "args": [] if args is None else args, "on": {}}]}


def write_definitions(directory, document):
    (directory / "flows").mkdir()
    path = directory / "flows/scene.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    manifest = {"schema_version": 1, "flow_files": ["scene.json"],
                "sources": {"FD2.EXE": {"sha256": audit.EXE_SHA256}},
                "flow_contracts": {"scene.json": {
                    "operation_abi_sha256": audit.digest(json.dumps(document["operations"], sort_keys=True).encode()),
                    "ports": {"end": [[]]}, "required_ids": ["event.action.006"]}}}
    (directory / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    return path


def definition_document():
    return {"schema_version": 1, "domain": "scene",
            "source": {"file": "FD2.EXE", "file_sha256": audit.EXE_SHA256, "evidence": "synthetic source"},
            "operations": {"end": {"args": []}}, "flows": [flow("event.action.006")],
            "bindings": {"stage.001.action.006": "event.action.006", "stage.007.action.006": "event.action.006"}}


def stage_fixture():
    metadata = bytearray(0x83 + 26)
    for slot in range(16):
        metadata[3 + slot * 3:6 + slot * 3] = bytes((255, 255, 0))
        metadata[0x33 + slot * 2] = 255
        metadata[0x53 + slot * 3] = 255
    # 此行没有placement，也不属于当前绑定的append_group；必须保留而不能猜fallback。
    metadata[0x83 + 1], metadata[0x83 + 21], metadata[0x83 + 22] = 76, 10, 255
    field = [(10, struct.pack("<HHI", 1, 1, 0)), (20, bytes(metadata)), (200, b"\0\0")]
    shape, text = [(0, b"unused"), (20, bytes(4))], [(0, b"unused"), (20, b"\2\0\0\0")]
    flows = {identity: flow(identity) for identity in ("stage.entry.000", "stage.result.000", "stage.victory.000", "scene.new_game", "loot")}
    flows["stage.capability.000"] = flow("stage.capability.000", "stage_capability", [1, 0, 0, 0])
    dispatch = {name: {"count": count, "records": [{"target_va": "0x10000"} for _ in range(count)]}
                for name, _, count, _ in audit.DISPATCH}
    dispatch["stage_entry"]["records"][0]["target_va"] = "0x3231b"
    return field, shape, text, flows, {}, dispatch


class StaticStageAuditTest(unittest.TestCase):
    def test_archive_requires_full_directory_and_eof(self):
        entries = [b"abc", b"defg", b"unknown\xff"]
        raw = archive(entries)
        self.assertEqual([item for _, item in audit.archive_entries(raw)], entries)
        for malformed in (raw[:-1], raw + b"extra", b"bad" + raw[3:], raw[:6] + struct.pack("<I", 15) + raw[10:]):
            with self.subTest(raw=malformed[:20]), self.assertRaises(ValueError):
                audit.archive_entries(malformed)
        duplicate = bytearray(raw)
        duplicate[10:14] = duplicate[6:10]
        with self.assertRaises(ValueError):
            audit.archive_entries(duplicate)

    def test_payloads_reject_truncation_trailing_data_and_bad_offsets(self):
        field, _, _, _, _, _ = stage_fixture()
        for parser, raw in ((audit.parse_grid, field[0][1]), (audit.parse_metadata, field[1][1]),
                            (audit.parse_placements, field[2][1]), (audit.text_fragment_count, b"\2\0\0\0")):
            for bad in (raw[:-1], raw + b"x"):
                with self.subTest(parser=parser.__name__, length=len(bad)), self.assertRaises(ValueError):
                    parser(bad)
        for raw in (b"\0\0", b"\3\0\0\0", b"\4\0\2\0\0\0"):
            with self.assertRaises(ValueError):
                audit.text_fragment_count(raw)

    def test_all_slots_unknown_bytes_and_nondefault_turn255_retained(self):
        fixture = list(stage_fixture())
        metadata = bytearray(fixture[0][1][1])
        metadata[1:3] = b"\x91\x92"
        metadata[6:9] = bytes((255, 6, 1))  # 位于默认填充之后，不能遇255就截断。
        fixture[0][1] = (20, bytes(metadata))
        row = audit.audit_stage(0, *fixture)
        self.assertEqual([len(row["metadata"][key]) for key in ("turn_slots", "cell_lookup_slots", "cell_action_slots")], [16, 16, 16])
        self.assertEqual(row["metadata"]["unknown_01"], 0x91)
        self.assertEqual(bytes.fromhex(row["metadata"]["raw_hex"]), metadata)
        self.assertTrue(row["metadata"]["turn_slots"][0]["default_fill"])
        self.assertEqual(row["metadata"]["turn_slots"][1]["classification"], audit.UNKNOWN)
        self.assertFalse(row["selected_semantics_supported"])
        self.assertFalse(row["semantic_complete"])

    def test_unbound_template_kept_but_bound_missing_placement_fails(self):
        fixture = list(stage_fixture())
        row = audit.audit_stage(0, *fixture)
        self.assertTrue(row["bound_definitions_closed"])
        self.assertFalse(row["selected_semantics_supported"])
        template = row["metadata"]["templates"][0]
        self.assertIsNone(template["placement_index"])
        self.assertEqual(template["placement_relation"], "resource_tail_not_consumed")
        self.assertFalse(template["effective_template"])
        fixture[3]["stage.entry.000"] = flow("stage.entry.000", "append_group", [10, 0, 0])
        metadata=bytearray(fixture[0][1][1]);metadata[2]=1
        fixture[0][1]=(20,bytes(metadata))
        row = audit.audit_stage(0, *fixture)
        self.assertFalse(row["bound_definitions_closed"])
        self.assertFalse(row["selected_semantics_supported"])
        self.assertIn("missing same-index", row["reference_errors"][0])

    def test_effective_count_same_index_low_byte_and_retained_legacy_observation(self):
        fixture=list(stage_fixture());metadata=bytearray(fixture[0][1][1]);metadata[2]=1
        fixture[0][1]=(20,bytes(metadata));fixture[0][2]=(200,struct.pack("<HHHH",1,0x102,0x203,99))
        row=audit.audit_stage(0,*fixture);template=row["metadata"]["templates"][0]
        self.assertEqual(template["placement_index"],0)
        self.assertEqual(template["placement_coordinates_low_byte"],[2,3])
        self.assertEqual(template["placement_relation"],"same_index_low_byte")
        self.assertIsNone(template["legacy_id_ordinal_placement_index"])
        self.assertTrue(row["bound_definitions_closed"])
        metadata[2]=2;fixture[0][1]=(20,bytes(metadata))
        self.assertIn("count exceeds",audit.audit_stage(0,*fixture)["reference_errors"][0])

    def test_default_turn255_is_a_strict_blocker_even_when_bound_definitions_close(self):
        row = audit.audit_stage(0, *stage_fixture())
        self.assertTrue(row["bound_definitions_closed"])
        self.assertFalse(row["selected_semantics_supported"])
        self.assertFalse(row["semantic_complete"])
        self.assertEqual(len(row["unsupported_active_requirements"]), 16)
        self.assertEqual(row["active_classification_counts"][audit.UNKNOWN], 16)
        for candidate in row["metadata"]["turn_slots"]:
            self.assertEqual(candidate["raw_hex"], "ffff00")
            self.assertTrue(candidate["turn255_potential_match"])
            self.assertFalse(candidate["ignore_semantics_verified"])
            self.assertIsNone(candidate["original_callback"])
            self.assertEqual(candidate["classification"], audit.UNKNOWN)
        with self.assertRaisesRegex(ValueError, "turn255"):
            audit.require_supported({"stages": [row]})

    def test_supported_preparation_entry_does_not_require_battle_enabled(self):
        fixture = list(stage_fixture())
        fixture[0] *= 3
        fixture[2] += fixture[2]
        fixture[3] = {identity.replace(".000", ".002"): {**definition, "id": identity.replace(".000", ".002")}
                      for identity, definition in fixture[3].items()}
        fixture[3]["stage.capability.002"]["nodes"][0]["args"] = [0, 0, 15, 6]
        row = audit.audit_stage(2, *fixture)
        self.assertTrue(row["entry_supported"])
        self.assertEqual(row["dispatch"]["entry"]["classification"], audit.SUPPORTED)
        self.assertFalse(row["battle_enabled"])
        self.assertEqual(row["dispatch"]["result"]["classification"], audit.UNOPENED)
        self.assertEqual(row["dispatch"]["victory"]["classification"], audit.UNOPENED)
        self.assertFalse(row["selected_semantics_supported"])
        with self.assertRaises(ValueError):
            audit.require_supported({"stages": [row]})
        del fixture[3]["stage.result.002"]
        del fixture[3]["stage.victory.002"]
        row = audit.audit_stage(2, *fixture)
        self.assertEqual(row["dispatch"]["result"]["classification"], audit.UNKNOWN)
        self.assertEqual(row["dispatch"]["victory"]["classification"], audit.UNKNOWN)
        del fixture[3]["stage.capability.002"]
        self.assertFalse(audit.audit_stage(2, *fixture)["entry_supported"])

    def test_start_entry_evidence_is_separate_from_battle_gate(self):
        fixture = list(stage_fixture())
        fixture[3]["stage.capability.000"]["nodes"][0]["args"] = [0, 0, 0, 0]
        row = audit.audit_stage(0, *fixture)
        self.assertTrue(row["entry_supported"])
        self.assertEqual(row["dispatch"]["entry"]["definition"], "scene.new_game")
        self.assertFalse(row["battle_enabled"])
        self.assertFalse(row["selected_semantics_supported"])

    def test_same_unit_id_keeps_distinct_index_and_legacy_observation(self):
        fixture = list(stage_fixture())
        raw = fixture[0][1][1]
        raw=bytearray(raw + raw[0x83:]);raw[2]=2
        fixture[0][1] = (20, bytes(raw))
        fixture[0][2] = (200, struct.pack("<HHHHHHH", 2, 2, 3, 76, 4, 5, 76))
        row = audit.audit_stage(0, *fixture)
        self.assertEqual([item["placement_index"] for item in row["metadata"]["templates"]], [0, 1])
        self.assertEqual([item["legacy_id_ordinal_placement_index"] for item in row["metadata"]["templates"]], [0,1])

    def test_aliases_reuse_one_graph_without_implicit_cross_stage_support(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            document = definition_document()
            path = write_definitions(directory, document)
            flows, aliases, _ = audit.load_definitions(directory)
            self.assertIs(audit.resolve("stage.001.action.006", flows, aliases), audit.resolve("stage.007.action.006", flows, aliases))
            self.assertEqual(len(flows), 1)
            self.assertEqual(audit.classify("stage.008.action.006", True, flows, aliases)["classification"], audit.UNKNOWN)
            self.assertEqual(audit.classify("stage.007.action.006", False, flows, aliases)["classification"], audit.UNOPENED)
            for target in ("missing", "stage.001.action.006"):
                document["bindings"]["stage.007.action.006"] = target
                path.write_text(json.dumps(document), encoding="utf-8")
                with self.assertRaises(ValueError):
                    audit.load_definitions(directory)

    def test_known_flow_id_with_unknown_op_bad_schema_or_forged_abi_fails(self):
        for change in ("unknown_op", "registered_unknown_op", "bad_args", "missing_edge", "empty_nodes", "bad_ports"):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                document = definition_document()
                path = write_definitions(directory, document)
                node = document["flows"][0]["nodes"][0]
                if change in ("unknown_op", "registered_unknown_op"):
                    node["op"] = "undecoded"
                    if change == "registered_unknown_op":
                        document["operations"]["undecoded"] = {"args": []}
                elif change == "bad_args":
                    node["args"] = [999]
                elif change == "missing_edge":
                    node["on"] = {"next": "absent"}
                elif change == "empty_nodes":
                    document["flows"][0]["nodes"] = []
                else:
                    # 结构上有合法出口，但end原语不接受next端口。
                    node["on"] = {"next": "other"}
                    document["flows"][0]["nodes"].append({"id": "other", "op": "end", "args": [], "on": {}})
                path.write_text(json.dumps(document), encoding="utf-8")
                with self.assertRaises(ValueError):
                    audit.load_definitions(directory)

    def test_cell_uses_zero_based_actions_and_one_based_normal_lookup(self):
        fixture = list(stage_fixture())
        fixture[0][0] = (10, struct.pack("<HHII", 2, 1, 1 << 16, 1))
        fixture[1][1] = (20, bytes(4) + bytes((0x20, 0, 0, 0)))
        metadata = bytearray(fixture[0][1][1])
        metadata[0x53:0x56] = b"\0\0\0"  # special index0 -> known item0
        fixture[0][1] = (20, bytes(metadata))
        row = audit.audit_stage(0, *fixture)
        self.assertEqual([cell["index_base"] for cell in row["map"]["event_cells"]], [1, 0])
        self.assertTrue(row["metadata"]["cell_action_slots"][0]["referenced_by_map"])
        self.assertEqual(row["reference_errors"], [])
        fixture[0][0] = (10, struct.pack("<HHI", 1, 1, (16 << 16) | 1))
        self.assertIn("outside 16", audit.audit_stage(0, *fixture)["reference_errors"][0])

    def test_dispatch_bounds_use_fixed_source_slots_not_flow_count(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "FD2.EXE"
            path.write_bytes(b"fixture")
            raw = bytearray(0x51E61 - 0x51B19)
            raw[0x51CF9 - 0x51B19:0x51D01 - 0x51B19] = struct.pack("<II", 18, 18)
            fixups = {base + slot * 4: (1, 0, 0x10000) for _, base, count, _ in audit.DISPATCH for slot in range(count)}
            owner = SimpleNamespace(index=1, relbase=0x10000, vsize=4)
            functions = [{"canonical_va": "0x10000", "chunks": [{"canonical_va": "0x10000", "length": 4}]}]
            with mock.patch.multiple(audit, EXE_SHA256=audit.digest(b"fixture")), \
                    mock.patch.object(audit, "parse_le", return_value=None), \
                    mock.patch.object(audit, "parse_objects", return_value=[owner]), \
                    mock.patch.object(audit, "source_range", side_effect=lambda _, va, size: b"code" if va == 0x10000 else bytes(raw)), \
                    mock.patch.object(audit, "collect_internal_offset_fixups", return_value=fixups), \
                    mock.patch.object(audit, "verify_ida_evidence", return_value={}), \
                    mock.patch.object(audit, "function_evidence", return_value=functions):
                tables, gap, _, _ = audit.read_dispatch(path, Path(temporary))
                self.assertEqual(tables["event_callback"]["count"], 90)
                self.assertEqual(tables["event_callback"]["records"][-1]["source_va"], "0x51cf5")
                self.assertEqual(tables["adjacent_magic"]["canonical_va"], "0x51d01")
                self.assertEqual(gap["raw_hex"], "1200000012000000")
                del fixups[0x51CF5]
                with self.assertRaisesRegex(ValueError, r"event_callback\[89\]"):
                    audit.read_dispatch(path, Path(temporary))

    def test_ida_checksums_and_original_identity_cannot_be_self_reported(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            files = {"fd2-functions.tsv": b"x", "fd2-ida-function-registry.json": b"{}",
                     "fd2-ida-decomp-all.c": b"x", "fd2-program.json": json.dumps({"input_sha256": audit.EXE_SHA256}).encode()}
            for name, raw in files.items():
                (directory / name).write_bytes(raw)
            def manifest():
                (directory / "SHA256SUMS").write_text("".join(f"{audit.digest(raw)}  {name}\n" for name, raw in files.items()), encoding="utf-8")
            manifest()
            audit.verify_ida_evidence(directory)
            (directory / "fd2-functions.tsv").write_bytes(b"forged")
            with self.assertRaisesRegex(ValueError, "checksum"):
                audit.verify_ida_evidence(directory)
            (directory / "fd2-functions.tsv").write_bytes(files["fd2-functions.tsv"])
            files["fd2-program.json"] = b'{"input_sha256":"forged"}'
            (directory / "fd2-program.json").write_bytes(files["fd2-program.json"])
            manifest()
            with self.assertRaisesRegex(ValueError, "different EXE"):
                audit.verify_ida_evidence(directory)

    def test_baseline_source_checks_actual_git_blob(self):
        record = {"kind": "existing_confirmed_definition", "file": "src/field_game.c", "baseline_commit": "a" * 40,
                  "baseline_source_sha256": audit.digest(b"historical C"), "evidence": "confirmed baseline"}
        with mock.patch.object(audit.subprocess, "run", return_value=SimpleNamespace(returncode=0, stdout=b"historical C")) as command:
            audit.validate_definition_sources([{"source": record}])
            self.assertEqual(command.call_args.args[0], ["git", "show", "a" * 40 + ":src/field_game.c"])
            record["baseline_source_sha256"] = "0" * 64
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                audit.validate_definition_sources([{"source": record}])

    def test_nested_source_blobs_are_verified_and_duplicate_leaf_uses_cache(self):
        commit = "a" * 40
        blobs = {"src/field_game.c": b"root", "src/scene.c": b"child", "src/field.c": b"leaf"}
        records = {file: {"kind": "existing_confirmed_definition", "file": file,
                         "baseline_commit": commit, "baseline_source_sha256": audit.digest(blob),
                         "evidence": "synthetic nested source"} for file, blob in blobs.items()}
        leaf = records["src/field.c"]
        records["src/scene.c"]["related_sources"] = [leaf, copy.deepcopy(leaf)]
        records["src/field_game.c"]["related_sources"] = [records["src/scene.c"]]
        provenance = [{"source": records["src/field_game.c"]}]
        manifest_sources = {file + "@" + commit: {"sha256": audit.digest(blob)} for file, blob in blobs.items()}
        audit.flow_generator.validate_source(provenance[0]["source"], manifest_sources, "nested fixture")
        with mock.patch.object(audit.subprocess, "run", side_effect=lambda command, **kwargs:
                SimpleNamespace(returncode=0, stdout=blobs[command[-1].split(":", 1)[1]])) as read:
            verified = audit.validate_definition_sources(provenance)
            self.assertEqual(set(verified), {commit + ":" + file for file in blobs})
            self.assertEqual(read.call_count, 3)
            leaf["baseline_source_sha256"] = "0" * 64
            manifest_sources["src/field.c@" + commit]["sha256"] = "0" * 64
            records["src/scene.c"]["related_sources"] = [leaf]
            audit.flow_generator.validate_source(provenance[0]["source"], manifest_sources, "forged nested fixture")
            with self.assertRaisesRegex(ValueError, r"hash mismatch: .*src/field\.c"):
                audit.validate_definition_sources(provenance)

    def test_executable_definition_kinds_require_actual_original_identity(self):
        kinds=("canonical_disassembly","decoded_control_flow",
               "decoded_domain_control_flow","original_executable")
        with mock.patch.object(audit,"EXE_SHA256",audit.digest(b"fixture")):
            for kind in kinds:
                record={"kind":kind,"file":"FD2.EXE","file_sha256":audit.digest(b"fixture"),
                        "evidence":"synthetic original instruction evidence"}
                with self.subTest(kind=kind),mock.patch.object(audit.subprocess,"run") as command:
                    self.assertEqual(audit.validate_definition_sources([{"source":record}],source_files={"FD2.EXE":b"fixture"}),
                                     {"FD2.EXE":audit.digest(b"fixture")})
                    command.assert_not_called()
                    for payload in (None,{},[],["FD2.EXE"],"fixture",{"other.exe":b"fixture"},
                                    {"FD2.EXE":None},{"FD2.EXE":"fixture"},
                                    {"FD2.EXE":bytearray(b"fixture")},{"FD2.EXE":b"changed"}):
                        with self.subTest(payload=repr(payload)),self.assertRaisesRegex(ValueError,"matching original"):
                            audit.validate_definition_sources([{"source":record}],source_files=payload)
                    for key,value in (("file","other.exe"),("file","original_game/FD2.EXE"),
                                      ("file_sha256","0"*64),("file_sha256",None),
                                      ("evidence",""),("evidence","  \t"),
                                      ("evidence",None),("evidence",["claimed evidence"])):
                        changed=dict(record);changed[key]=value
                        with self.subTest(key=key,value=value),self.assertRaises(ValueError):
                            audit.validate_definition_sources([{"source":changed}],source_files={"FD2.EXE":b"fixture"})
                    command.assert_not_called()

    def test_unknown_source_kind_cannot_borrow_exe_or_git_identity(self):
        for kind in (None,"","decoded_control_flow_v2","canonical_disassembly_typo",["original_executable"]):
            for historical in (False,True):
                record={"kind":kind,"file":"src/field_game.c" if historical else "FD2.EXE",
                        "file_sha256":audit.digest(b"fixture"),"evidence":"synthetic exact identity",
                        "baseline_commit":"a"*40,"baseline_source_sha256":audit.digest(b"historical C")}
                with self.subTest(kind=kind,historical=historical),mock.patch.object(audit,"EXE_SHA256",audit.digest(b"fixture")), \
                        mock.patch.object(audit.subprocess,"run") as command,self.assertRaises(ValueError):
                    audit.validate_definition_sources([{"source":record}],source_files={"FD2.EXE":b"fixture"})
                command.assert_not_called()

    def test_source_tree_rejects_malformed_records_and_related_sources(self):
        valid={"kind":"canonical_disassembly","file":"FD2.EXE","file_sha256":audit.digest(b"fixture"),"evidence":"synthetic source"}
        records=[None,[],"FD2.EXE"]
        records.extend(dict(valid,related_sources=related) for related in (None,{},"claimed child",[None],["claimed child"]))
        with mock.patch.object(audit,"EXE_SHA256",audit.digest(b"fixture")):
            for record in records:
                with self.subTest(record=record),self.assertRaises(ValueError):
                    audit.validate_definition_sources([{"source":record}],source_files={"FD2.EXE":b"fixture"})

    def test_executable_and_historical_nested_sources_keep_both_independent_checks(self):
        original={"kind":"original_executable","file":"FD2.EXE","file_sha256":audit.digest(b"fixture"),"evidence":"synthetic source"}
        historical={"kind":"existing_confirmed_definition","file":"src/field_game.c","baseline_commit":"a"*40,
                    "baseline_source_sha256":audit.digest(b"historical C"),"evidence":"synthetic Git identity"}
        with mock.patch.object(audit,"EXE_SHA256",audit.digest(b"fixture")):
            for exe_root in (False,True):
                record=copy.deepcopy(original if exe_root else historical)
                record["related_sources"]=[copy.deepcopy(historical if exe_root else original)]
                with self.subTest(exe_root=exe_root),mock.patch.object(audit.subprocess,"run",return_value=SimpleNamespace(returncode=0,stdout=b"historical C")) as command:
                    self.assertEqual(audit.validate_definition_sources([{"source":record}],source_files={"FD2.EXE":b"fixture"}),
                                     {"FD2.EXE":audit.digest(b"fixture"),"a"*40+":src/field_game.c":audit.digest(b"historical C")})
                    self.assertEqual(command.call_count,1)
            for key,value in (("kind","decoded_unknown"),("file_sha256","0"*64)):
                child=dict(original);child[key]=value
                record=dict(original,related_sources=[child])
                with self.subTest(key=key),self.assertRaises(ValueError):
                    audit.validate_definition_sources([{"source":record}],source_files={"FD2.EXE":b"fixture"})

    def test_historical_source_bad_types_remain_closed_before_git(self):
        valid={"kind":"existing_confirmed_definition","file":"src/field_game.c","baseline_commit":"a"*40,
               "baseline_source_sha256":audit.digest(b"historical C"),"evidence":"synthetic Git identity"}
        for key,value in (("file",None),("file",["src/field_game.c"]),("baseline_commit",None),
                          ("baseline_commit",["a"*40]),("evidence","  "),("evidence",["claim"])):
            record=dict(valid);record[key]=value
            with self.subTest(key=key,value=value),mock.patch.object(audit.subprocess,"run") as command,self.assertRaises(ValueError):
                audit.validate_definition_sources([{"source":record}])
            command.assert_not_called()

    def test_require_supported_fails_closed_and_keeps_written_report(self):
        report = {"structural_success": True, "semantic_complete": False,
                  "stages": [{"stage": 7, "selected_semantics_supported": False}]}
        with self.assertRaises(ValueError):
            audit.require_supported(report)
        passing = copy.deepcopy(report)
        passing["stages"][0]["selected_semantics_supported"] = True
        audit.require_supported(passing)
        self.assertFalse(passing["semantic_complete"])
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "report.json"
            argv = ["audit_stage_data.py", "--output", str(output), "--require-supported"]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(audit, "audit", return_value=report), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                audit.main()
            self.assertEqual(error.exception.code, 1)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), report)


class FullMovementSourceTest(unittest.TestCase):
    def fixture(self):
        raw = bytearray(0x34D2)
        start = audit.MOVEMENT_PAYLOAD_VA - 0x60000
        fixups, runtime = {}, {}
        for identity in range(audit.MOVEMENT_POINTER_COUNT):
            offset = start + identity * 5
            raw[offset:offset + 5] = bytes((1, 1, 1, 0, 0))
            struct.pack_into("<I", raw, audit.POINTER_TABLE_VA - 0x60000 + identity * 4, offset)
            fixups[audit.POINTER_TABLE_VA + identity * 4] = (3, offset, 0x60000 + offset)
            if identity in audit.SCRIPT_IDS:
                runtime[identity] = SimpleNamespace(canonical_va=0x60000 + offset, data=bytes(raw[offset:offset + 5]))
        return raw, fixups, runtime

    def read(self, raw, fixups, runtime):
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / "FD2.EXE"
            executable.write_bytes(b"fixture")
            owner = SimpleNamespace(index=3, relbase=0x60000, vsize=len(raw))
            with mock.patch.object(audit, "EXE_SHA256", audit.digest(b"fixture")), \
                    mock.patch.object(audit, "parse_le", return_value=None), \
                    mock.patch.object(audit, "parse_objects", return_value=[owner]), \
                    mock.patch.object(audit, "source_range", return_value=bytes(raw)), \
                    mock.patch.object(audit, "collect_internal_offset_fixups", return_value=fixups):
                return audit.read_movement_table(executable, runtime)

    def test_all_106_pointers_and_raw_payload_retained_runtime_subset_separate(self):
        raw, fixups, runtime = self.fixture()
        report = self.read(raw, fixups, runtime)
        self.assertTrue(report["pointer_complete"])
        self.assertEqual(report["count"], 106)
        self.assertEqual(report["length"], 424)
        self.assertEqual(report["records"][-1]["pointer_va"], "0x6297c")
        self.assertEqual(report["payload"]["length"], 2898)
        self.assertEqual(bytes.fromhex(report["payload"]["raw_hex"]), raw[0x2980:])
        self.assertEqual(report["summary"]["runtime_strict_streams"], 106)
        self.assertEqual(report["summary"]["runtime_unknown_or_unopened"], 0)
        self.assertFalse(report["semantic_complete"])
        for row in report["records"][20:90]:
            self.assertTrue(row["runtime_supported"])
            self.assertTrue(row["semantic_decoded"])
            self.assertEqual(row["stream_syntax_status"], "runtime_strict_verified")

    def test_missing_last_fixup_wrong_owner_and_outside_initialized_source_fail(self):
        raw, fixups, runtime = self.fixture()
        last = audit.POINTER_TABLE_VA + 105 * 4
        for value in (None, (2, 0x2980, 0x62980), (3, len(raw), 0x60000 + len(raw)), (3, 0x27D8, 0x627D8)):
            changed = dict(fixups)
            if value is None:
                del changed[last]
            else:
                changed[last] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.read(raw, changed, runtime)
        changed = dict(fixups)
        changed[audit.MOVEMENT_PAYLOAD_VA] = (3, 0x2980, 0x62980)
        with self.assertRaisesRegex(ValueError, "boundary"):
            self.read(raw, changed, runtime)

    def test_duplicate_targets_keep_both_pointer_records_and_candidate_is_not_boundary(self):
        raw, fixups, runtime = self.fixture()
        fixups[audit.POINTER_TABLE_VA + 21 * 4] = fixups[audit.POINTER_TABLE_VA + 20 * 4]
        runtime[21]=runtime[20]
        report = self.read(raw, fixups, runtime)
        self.assertEqual(report["count"], 106)
        first, second = report["records"][20:22]
        self.assertEqual(first["target_va"], second["target_va"])
        self.assertEqual(first["candidate_span"], second["candidate_span"])
        self.assertFalse(first["candidate_span"]["boundary_confirmed"])
        self.assertEqual(first["candidate_span"]["length"], 10)
        self.assertEqual(first["candidate_relation"], "candidate_tail_retained")

    def test_cross_target_tail_actor83_and_zero_raw5_retain_candidate_bytes(self):
        raw, fixups, runtime = self.fixture()
        start = audit.MOVEMENT_PAYLOAD_VA - 0x60000
        raw[start + 24 * 5:start + 24 * 5 + 5] = bytes((1, 0x80, 2, 48, 0))
        raw[start + 25 * 5 + 1] = 5  # ID24按原计数读入下一目标头部的(1,5)。
        raw[start + 60 * 5 + 3] = 83
        raw[start + 84 * 5] = 0  # 仅消费group_count，候选余4 byte仍须保存。
        for identity,size in ((24,7),(25,5),(60,5),(84,1)):
            offset=start+identity*5
            runtime[identity]=SimpleNamespace(canonical_va=0x60000+offset,data=bytes(raw[offset:offset+size]))
        report = self.read(raw, fixups, runtime)
        row24, row60, row84 = (report["records"][i] for i in (24, 60, 84))
        self.assertEqual(row24["candidate_relation"], "crosses_next_target")
        self.assertEqual(row24["candidate_span"]["length"], 5)
        self.assertEqual(row24["counted_structure_observation"]["consumed_length"], 7)
        self.assertEqual(row24["counted_structure_observation"]["unconfirmed_directions"], [])
        self.assertEqual(row60["counted_structure_observation"]["actors_outside_current_capacity"], [])
        self.assertEqual(row84["candidate_relation"], "candidate_tail_retained")
        self.assertEqual(row84["candidate_span"]["raw_hex"], raw[start + 84 * 5:start + 85 * 5].hex())
        for row in (row24, row60, row84):
            self.assertTrue(row["runtime_supported"])
            self.assertTrue(row["semantic_decoded"])
        self.assertEqual(report["payload"]["raw_hex"], raw[0x2980:].hex())

    def test_unresolved_counted_structure_keeps_raw_source_without_runtime_success(self):
        raw, fixups, runtime = self.fixture()
        fixups[audit.POINTER_TABLE_VA + 84 * 4] = (3, len(raw) - 1, 0x60000 + len(raw) - 1)
        raw[-1] = 1
        self.assertFalse(audit.scan_movement_structure(bytes(raw[-1:]))["complete"])
        with self.assertRaisesRegex(ValueError,"runtime strict"):
            self.read(raw,fixups,runtime)

    def test_current_runtime_subset_cannot_bypass_existing_strict_constraints(self):
        raw, fixups, runtime = self.fixture()
        start = audit.MOVEMENT_PAYLOAD_VA - 0x60000
        for actor, direction in ((96, 0), (0, 5)):
            changed = bytearray(raw)
            changed[start + 3:start + 5] = bytes((actor, direction))
            forged = dict(runtime)
            forged[0] = SimpleNamespace(canonical_va=audit.MOVEMENT_PAYLOAD_VA, data=bytes(changed[start:start + 5]))
            with self.subTest(actor=actor, direction=direction), self.assertRaisesRegex(ValueError, "runtime strict"):
                self.read(changed, fixups, forged)


if __name__ == "__main__":
    unittest.main()
