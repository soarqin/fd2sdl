"""S2叠加去向：覆盖完整分母、异常传播、禁止同target提权及原魔法alias合同。"""
import copy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
import s2_source_disposition as disposition


class DispositionTest(unittest.TestCase):
    def setUp(self):
        records, candidates = [], []
        for table, count in (("stage_runtime_result", 30), ("event_callback", 90), ("adjacent_magic", 28), ("stage_entry", 30), ("stage_victory", 30)):
            for slot in range(count):
                identity = f"dispatch:{table}:{slot:03d}"
                records.append({"source_id": identity, "kind": "dispatch", "disposition": "unresolved",
                    "source": {"raw_hex": "01000000"}, "caller": "explicit original caller", "references": [], "definition": None,
                    "decoded": {"table": table, "slot": slot, "target_va": "0x10001"}})
                candidates.append({"source_id": identity, "target_va": "0x10001", "definition_domain": "stage", "definition_id": "candidate",
                    "disposition": "converted_candidate", "blocker": None, "new_runtime_binding": False})
        for i in range(107):
            records.append({"source_id": f"pending:{i}", "kind": "turn", "disposition": "unresolved", "source": {},
                            "caller": "turn", "decoded": {}, "references": ["dispatch:event_callback:082"]})
        for i in range(450):
            records.append({"source_id": f"refusal:{i}", "kind": "turn", "disposition": "rejected", "source": {"raw_hex": "ffff00"},
                            "caller": "turn", "decoded": {"action": 255}})
        self.ledger, self.candidates = {"records": records}, candidates

    def test_all_208_315_450_preserve_overlap_and_binding_gate(self):
        report = disposition.overlay(self.ledger, self.candidates, {("stage", "candidate")})
        self.assertEqual((len(report["dispatch"]), len(report["S1_unresolved"]), len(report["S1_rejected"])), (208, 315, 450))
        self.assertEqual(report["summary"]["unique_source_ID_union"], 765)
        self.assertEqual(len({r["source_id"] for r in report["dispatch"]}), 208)
        self.assertFalse(report["claims"]["source_candidate_is_shipping_binding"])
        self.assertTrue(all(not r["new_runtime_binding"] for r in report["dispatch"]))
        self.assertTrue(all(r["definition_present_in_static_input"] for r in report["dispatch"]))

    def test_single_unknown_cfg_blocks_all_its_source_parameters(self):
        row = next(r for r in self.candidates if r["source_id"] == "dispatch:event_callback:082")
        row.update(disposition="blocked", definition_id=None, blocker={"family": "teleport", "reason": "unknown exact ABI"})
        report = disposition.overlay(self.ledger, self.candidates, {("stage", "candidate")})
        blocked = [r for r in report["S1_unresolved"] if r["S2_disposition"] == "quarantined_source_dependency"]
        self.assertEqual(len(blocked), 108)
        self.assertTrue(all(r["blocked_dependencies"] == [row["source_id"]] for r in blocked))

    def test_missing_duplicate_target_change_approval_and_empty_graph_ref_fail(self):
        for what in ("missing", "duplicate", "target", "approval", "empty_definition"):
            rows = copy.deepcopy(self.candidates)
            if what == "missing": rows.pop()
            elif what == "duplicate": rows[-1] = copy.deepcopy(rows[0])
            elif what == "target": rows[0]["target_va"] = "0x10002"
            elif what == "approval": rows[0]["new_runtime_binding"] = True
            else: rows[0]["definition_id"] = None
            with self.subTest(what=what), self.assertRaises(ValueError): disposition.validate_dispatch_mapping(self.ledger, rows)

    def test_rejected_original_domain_cannot_be_reclassified_or_silently_changed(self):
        row = next(r for r in self.ledger["records"] if r["disposition"] == "rejected")
        row["source"]["raw_hex"] = "ff0100"
        with self.assertRaisesRegex(ValueError, "拒绝域"): disposition.overlay(self.ledger, self.candidates, {("stage", "candidate")})

    def test_missing_actual_definition_and_same_id_changed_graph_refuse(self):
        with self.assertRaisesRegex(ValueError, "实际唯一"): disposition.overlay(self.ledger, self.candidates, set())
        flow = {"id": "common.callback.test", "entry": "end", "nodes": [{"id": "end", "op": "end", "args": [], "on": {}}]}
        callbacks = {"flows": [flow], "decisions": [{"disposition": "converted_candidate", "definition": flow["id"], "definition_sha256": disposition.digest(disposition.encode(flow).encode())}]}
        transitions = {"flows": [], "rows": []}
        disposition.validate_definition_contents(callbacks, transitions, {("common_event", flow["id"]): flow})
        changed = copy.deepcopy(flow); changed["nodes"][0]["op"] = "fail"
        with self.assertRaisesRegex(ValueError, "内容改变"): disposition.validate_definition_contents(callbacks, transitions, {("common_event", flow["id"]): changed})
        with self.assertRaisesRegex(ValueError, "缺图"): disposition.validate_definition_contents(callbacks, transitions, {})

    def test_transition_shared_body_change_and_missing_flow_refuse(self):
        helper = {"id": "stage.source.fade.out", "entry": "end", "nodes": [{"id": "end", "op": "end", "args": [], "on": {}}]}
        callbacks = {"flows": [], "decisions": []}
        transitions = {"flows": [helper], "rows": [{"disposition": "converted_candidate", "flow_id": helper["id"]}]}
        disposition.validate_definition_contents(callbacks, transitions, {("stage", helper["id"]): helper})
        bad = copy.deepcopy(helper); bad["nodes"][0]["args"] = [1]
        with self.assertRaisesRegex(ValueError, "内容改变"): disposition.validate_definition_contents(callbacks, transitions, {("stage", helper["id"]): bad})
        transitions["rows"][0]["flow_id"] = "missing"
        with self.assertRaisesRegex(ValueError, "完整候选"): disposition.validate_definition_contents(callbacks, transitions, {("stage", helper["id"]): helper})

    def test_transition_source_only_alias_requires_exact_static_binding_and_canonical_body(self):
        flow = {"id": "stage.source.entry.012", "entry": "end", "nodes": [{"id": "end", "op": "end", "args": [], "on": {}}]}
        aliases = {"stage.source.entry.015": flow["id"]}
        callbacks = {"flows": [], "decisions": []}
        transitions = {"flows": [flow], "source_bindings": aliases, "rows": [{"disposition": "converted_candidate",
            "flow_id": "stage.source.entry.015", "canonical_definition": flow["id"], "source_only_alias": True}]}
        catalog = {("stage", flow["id"]): flow}
        disposition.validate_definition_contents(callbacks, transitions, catalog, aliases)
        # S2只签source别名；S3正式绑定仍由独立发布闭包门核caller/资源。
        disposition.validate_definition_contents(callbacks, transitions, catalog,
            {**aliases, "stage.entry.015": flow["id"]})
        with self.assertRaisesRegex(ValueError, "bindings"): disposition.validate_definition_contents(callbacks, transitions, catalog, {})
        transitions["rows"][0]["canonical_definition"] = "stage.source.entry.020"
        with self.assertRaisesRegex(ValueError, "规范终点"): disposition.validate_definition_contents(callbacks, transitions, catalog, aliases)
        transitions["rows"][0]["canonical_definition"] = flow["id"]
        changed = copy.deepcopy(flow); changed["nodes"][0]["op"] = "fail"
        with self.assertRaisesRegex(ValueError, "内容改变"): disposition.validate_definition_contents(callbacks, transitions, {("stage", flow["id"]): changed}, aliases)
        bad = {"stage.source.entry.015": "stage.source.entry.015"}; transitions["source_bindings"] = bad
        with self.assertRaisesRegex(ValueError, "有链"): disposition.validate_definition_contents(callbacks, transitions, catalog, bad)

    def test_g2_map_magic_targets_alias_and_caller_scope_are_explicit(self):
        targets = (0x21206,0x2134b,0x21364,0x2137d,0x21396,0x21449,0x21462,0x2147b,0x21494,0x214ad,0x21527,0x2185f,0x21a9e,0x21ad9,0x21b99,0x2211c,0x22153,0x226ea,0x2282f,0x22960,0x22a85,0x22bc6,0x22be1,0x2218a,0x22153,0x22c04,0x22cbf,0x22e41)
        records = [{"kind": "dispatch", "source_id": f"dispatch:adjacent_magic:{i:03d}", "decoded": {"table": "adjacent_magic", "slot": i, "target_va": hex(va)}} for i, va in enumerate(targets)]
        rows, receipt = disposition.magic_destinations({"records": records}, ROOT / "original_game")
        self.assertEqual(len(rows), 28); self.assertTrue(receipt["ok"])
        self.assertEqual(rows[24]["target_va"], rows[16]["target_va"])
        self.assertEqual(rows[24]["projection"]["MAGIC_MAP_EFFECT_IDS"], 16)
        self.assertEqual(rows[17]["projection"]["MAGIC_MAP_COST_IDS"], 18)
        self.assertEqual(rows[9]["projection"]["MAGIC_MAP_TARGET_LIMITS"], 1)
        self.assertEqual(rows[23]["definition_id"], "magic.effect.023")
        self.assertEqual(rows[24]["caller_scope"]["player"], "FULL before table")
        self.assertFalse(any(r["new_runtime_binding"] for r in rows))
        records[24]["decoded"]["target_va"] = "0x22154"
        with self.assertRaisesRegex(ValueError, "目标"): disposition.magic_destinations({"records": records}, ROOT / "original_game")


if __name__ == "__main__": unittest.main()
