"""研究规范化只保留来源与已知分类；同目标不自动提升支持状态。"""
import copy
import hashlib
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("research_stage_batch", Path(__file__).resolve().parents[1] / "static_data/research_stage_batch.py")
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)


class NormalizeSlotTest(unittest.TestCase):
    def test_output_preserves_input_and_stays_in_research_directory(self):
        root = Path(__file__).resolve().parents[2]
        source = root / "build/research-stage-batch/full.json"
        MOD.validate_output(source, source.with_name("normalized.json"), root)
        for output in (source, root / "data/static/flows/stage.json"):
            with self.subTest(output=output), self.assertRaises(ValueError):
                MOD.validate_output(source, output, root)

    def setUp(self):
        self.stage = {"stage": 2, "metadata": {"entry": 7, "raw_hex": "000000ffff00", "sha256": hashlib.sha256(bytes.fromhex("000000ffff00")).hexdigest(), "file_offset": 1000}}
        self.slot = {"slot": 0, "entry_offset": 3, "raw_hex": "ffff00", "turn": 255, "action": 255, "phase": 0,
                     "default_fill": True, "classification": "unmapped_semantics", "unresolved_reason": "callback255 unknown"}

    def test_unknown_and_turn255_are_preserved(self):
        row = MOD.normalize_slot(self.stage, "turn", self.slot, "file-hash")
        self.assertEqual(row["classification"], "unmapped_semantics")
        self.assertTrue(row["default_turn255_candidate"])
        self.assertEqual(row["unresolved_reason"], "callback255 unknown")
        self.assertEqual(row["source"]["file_offset"], 1003)
        self.assertFalse(row["runtime_enablement_changed"])

    def test_inconsistent_source_is_rejected(self):
        for key, value in (("entry_offset", 4), ("raw_hex", "030001"), ("slot", 1)):
            slot = copy.deepcopy(self.slot)
            slot[key] = value
            with self.assertRaises(ValueError):
                MOD.normalize_slot(self.stage, "turn", slot, "file-hash")

    def test_known_target_does_not_approve_unbound_stage(self):
        self.slot["original_callback"] = {"target_va": "0x34531"}
        row = MOD.normalize_slot(self.stage, "turn", self.slot, "file-hash")
        self.assertEqual(row["callback_target_va"], "0x34531")
        self.assertIsNone(row["definition"])
        self.assertEqual(row["classification"], "unmapped_semantics")

    def test_result_candidate_keeps_owner_and_enablement_barrier(self):
        source = {"target_va": "0x205b4", "ida_owners": ["0x205b4"], "slot": 2, "source_va": "0x51b21", "raw_hex": "b4050100"}
        report = {"sources": {"FD2.EXE": {"sha256": "file-hash"}}, "dispatch_tables": {
            "stage_runtime_result": {"canonical_va": "0x51b19", "sha256": "table-hash", "records": [source]}},
            "function_catalog": {"0x205b4": {"chunks": [{"canonical_va": "0x205b4", "length": 38, "sha256": "chunk-hash"}]}}}
        definitions = {"flows": [{"id": "stage.result.000"}]}
        rows = MOD.result_reuse_candidates(report, definitions)
        self.assertEqual(rows[0]["runtime_id"], "stage.result.002")
        self.assertEqual(rows[0]["status"], "candidate")
        self.assertTrue(rows[0]["enablement_blocked_by"])
        self.assertFalse(rows[0]["shipping_definition_changed"])
        self.assertFalse(rows[0]["semantic_conversion_approved"])
        source["ida_owners"] = []
        with self.assertRaises(ValueError):
            MOD.result_reuse_candidates(report, definitions)


if __name__ == "__main__":
    unittest.main()
