"""G1来源分类的错源、漏审、表外及旧支持状态不提升回归。"""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

DIRECTORY = Path(__file__).resolve().parents[1] / "static_data"
sys.path.insert(0, str(DIRECTORY))
SPEC = importlib.util.spec_from_file_location("audit_common_scope", DIRECTORY / "audit_common_scope.py")
scope = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scope)


def fixture():
    chunk = {"canonical_va": "0x10000", "length": 1, "sha256": "code"}
    inventory = {"structural_success": True, "sources": {"FD2.EXE": {"sha256": "exe"}},
        "function_catalog": {"0x10000": {"chunks": [chunk]}},
        "dispatch_tables": {"event_callback": {"sha256": "table", "records": [
            {"slot": 0, "source_va": "0x51b91", "target_va": "0x10000", "ida_owners": ["0x10000"]}]}},
        "stages": [], "movement_pointer_table": {"records": [{"id": 0,
            "pointer_va": "0x627d8", "target_va": "0x62980",
            "counted_structure_observation": {"consumed_length": 1}}]}}
    review = {"schema": "fd2-common-scope-review-v1", "exe_sha256": "exe",
        "source_sha256": {"FD2.EXE": "exe"}, "function_chunks": {"0x10000": [chunk.copy()]},
        "dispatch_sha256": {"event_callback": "table"},
        "families": {"scene_binding": {"state": "known_rule", "next": "S"},
                     "movement_known": {"state": "known_rule", "next": "S"},
                     "callback_guard": {"state": "common_gap", "next": "G4"},
                     "descriptor_default": {"state": "common_gap", "next": "G2"}},
        "target_groups": [{"table": "event_callback", "targets": ["0x10000"],
                           "families": ["scene_binding"], "basis": "synthetic reviewed source"}],
        "placement_tails": {}, "movement": [{"id": 0, "source_va": "0x62980", "counted_length": 1,
                                               "state": "existing_supported"}]}
    return inventory, review


class CommonScopeTest(unittest.TestCase):
    def setUp(self):
        self.inventory, self.review = fixture()
        context = patch.object(scope, "EXPECTED_COUNTS", {"dispatch": 1, "movement": 1})
        context.start()
        self.addCleanup(context.stop)

    def test_classification_is_not_semantic_or_runtime_acceptance(self):
        result = scope.build_ledger(self.inventory, self.review)
        self.assertTrue(result["classification_complete"])
        self.assertFalse(result["all_game_semantic_complete"])
        self.assertFalse(result["original_runtime_executed"])
        self.assertFalse(result["new_gameplay_enabled"])
        self.assertEqual(result["rows"][0]["source_id"], "dispatch:event_callback:000")

    def test_unreviewed_target_or_script_remains_unresolved(self):
        for target in ("dispatch", "movement"):
            review = copy.deepcopy(self.review)
            if target == "dispatch":
                review["target_groups"].clear()
            else:
                review["movement"][0]["counted_length"] = 2
            result = scope.build_ledger(self.inventory, review)
            self.assertFalse(result["classification_complete"])
            self.assertEqual(len(result["unresolved_source_ids"]), 1)

    def test_wrong_source_table_and_code_fail(self):
        for mutate in (lambda r: r.update(exe_sha256="wrong"),
                       lambda r: r["dispatch_sha256"].update(event_callback="wrong"),
                       lambda r: r["function_chunks"]["0x10000"][0].update(sha256="wrong")):
            review = copy.deepcopy(self.review)
            mutate(review)
            with self.assertRaises(ValueError):
                scope.build_ledger(self.inventory, review)

    def test_duplicate_review_and_duplicate_source_fail(self):
        review = copy.deepcopy(self.review)
        review["target_groups"] *= 2
        with self.assertRaisesRegex(ValueError, "duplicate reviewed"):
            scope.build_ledger(self.inventory, review)
        self.inventory["dispatch_tables"]["event_callback"]["records"] *= 2
        with self.assertRaisesRegex(ValueError, "duplicate source"):
            scope.build_ledger(self.inventory, self.review)

    def test_table_outside_is_not_callback_or_unknown_magic_alias(self):
        dispatch = [{"source_id": "callback0", "semantic": {"classification": "binding_only"}}]
        self.assertEqual(scope.classify_callback(255, dispatch, self.review)["classification"], "unresolved")
        self.review["callback_out_of_table"] = {"reviewed_actions": [255], "basis": "verified non-code pointer"}
        self.assertEqual(scope.classify_callback(255, dispatch, self.review)["classification"], "original_out_of_table")
        self.assertEqual(scope.classify_callback(90, dispatch, self.review)["classification"], "unresolved")

    def test_mode203_default_does_not_dispatch_event203(self):
        self.review["descriptor_default"] = {"reviewed_modes": list(range(4, 255)), "basis": "verified default"}
        descriptor = {"mode": 203, "param_u16": 0}
        result = scope.classify_descriptor(descriptor, [], self.review)
        self.assertEqual(result["classification"], "original_default_noop")
        self.assertNotIn("dispatch_source_id", result)
        self.assertEqual(scope.classify_descriptor(descriptor, [], self.review, cell=True)["classification"], "unresolved")
        descriptor["mode"] = 255
        self.assertEqual(scope.classify_descriptor(descriptor, [], self.review)["classification"], "unresolved")

    def test_shared_tail_can_gain_owner_without_changing_known_bytes(self):
        tail = {"canonical_va": "0x10010", "length": 1, "sha256": "tail"}
        self.review["function_chunks"]["0x10010"] = [tail]
        self.inventory["function_catalog"]["0x10010"] = {"chunks": [tail]}
        self.inventory["function_catalog"]["0x10000"]["chunks"].append(tail)
        self.assertTrue(scope.build_ledger(self.inventory, self.review)["classification_complete"])
        self.inventory["function_catalog"]["0x10000"]["chunks"][-1] = {**tail, "sha256": "changed"}
        with self.assertRaises(ValueError):
            scope.build_ledger(self.inventory, self.review)

    def test_unpaired_placement_requires_original_count_boundary(self):
        self.inventory["stages"] = [{"stage": 0, "metadata": {
            "entry": 1, "unknown_02": 1, "turn_slots": [], "cell_lookup_slots": [],
            "cell_action_slots": [], "templates": [{"index": 1, "entry_offset": 157,
                "placement_index": None, "drop": {"classification": "unused_descriptor"}}]}, "dispatch": {}}]
        self.review["placement_tails"] = {"stage:00:template:001": {
            "effective_template_count": 1, "basis": "synthetic original bounded loop"}}
        self.review["families"]["indexed_initialization"] = {"state": "common_gap", "next": "G2"}
        with patch.object(scope, "EXPECTED_COUNTS", {"dispatch": 1, "movement": 1, "placement_tails": 1}):
            result = scope.build_ledger(self.inventory, self.review)
            self.assertTrue(result["classification_complete"])
            tail = next(row for row in result["rows"] if row["kind"] == "placement_tails")
            self.assertEqual(tail["semantic"]["next"], ["G2"])
            self.inventory["stages"][0]["metadata"]["unknown_02"] = 2
            self.assertFalse(scope.build_ledger(self.inventory, self.review)["classification_complete"])

    def test_common_dependency_cannot_be_renamed_binding(self):
        self.review["families"]["condition"] = {"state": "common_gap", "next": "G4"}
        self.review["target_groups"][0]["families"].append("condition")
        semantic = scope.build_ledger(self.inventory, self.review)["rows"][0]["semantic"]
        self.assertEqual(semantic["classification"], "common_gap")
        self.assertEqual(semantic["next"], ["G4"])

    def test_cleared_chunk_review_or_wrong_owner_blocks_target(self):
        review = copy.deepcopy(self.review)
        review["function_chunks"].clear()
        self.assertFalse(scope.build_ledger(self.inventory, review)["classification_complete"])
        review["function_chunks"]["0x10000"] = []
        with self.assertRaises(ValueError):
            scope.build_ledger(self.inventory, review)

    def test_unresolved_family_propagates_and_invalid_state_fails(self):
        self.review["families"]["scene_binding"]["state"] = "unresolved"
        self.assertFalse(scope.build_ledger(self.inventory, self.review)["classification_complete"])
        self.review["families"]["scene_binding"]["state"] = "anything"
        with self.assertRaisesRegex(ValueError, "family state"):
            scope.build_ledger(self.inventory, self.review)

    def test_shared_tail_target_is_reviewed_via_containing_owner(self):
        chunk = self.review["function_chunks"]["0x10000"][0]
        chunk.update(length=2)
        self.inventory["function_catalog"]["0x10000"]["chunks"][0].update(length=2)
        self.inventory["dispatch_tables"]["event_callback"]["records"][0]["target_va"] = "0x10001"
        self.review["target_groups"][0]["targets"] = ["0x10001"]
        self.assertTrue(scope.build_ledger(self.inventory, self.review)["classification_complete"])


if __name__ == "__main__":
    unittest.main()
