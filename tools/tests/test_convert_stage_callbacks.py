"""S2转换信任边界；真实原码批量轨迹另由带Unicorn的CLI复跑。"""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
import convert_stage_callbacks as converter


class CallbackConversionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.programs = converter.programs()
        cls.document = json.loads((ROOT / "data/static/flows/common_event.json").read_bytes())
        cls.ops = {**cls.document["operations"], **converter.NEW_OPERATIONS}

    def test_ninety_original_slots_have_explicit_destinations(self):
        self.assertEqual(set(self.programs) | set(converter.EXISTING) | {82}, set(range(90)))
        self.assertFalse(set(self.programs) & set(converter.EXISTING))
        self.assertNotIn(82, self.programs)
        # S2旧17→S3纠正41后16；S5再独立纠正51，共15，旧收据不改签。
        self.assertEqual(len(converter.EXISTING), 15)
        self.assertEqual(self.programs[51], [('loot_child',[2,-1]),('story',[3])])

    def test_existing_reference_requires_full_graph_digest(self):
        flows = {flow["id"]: flow for flow in self.document["flows"]}
        for slot, identity in converter.EXISTING.items():
            self.assertEqual(converter.digest(converter.encode(flows[identity]).encode()), converter.EXISTING_FLOW_SHA[slot])
        changed = copy.deepcopy(flows[converter.EXISTING[12]])
        changed["nodes"][1]["args"] = [99]
        self.assertNotEqual(converter.digest(converter.encode(changed).encode()), converter.EXISTING_FLOW_SHA[12])

    def test_shared_tail41_keeps_own_four_and_legacy_mechanism_unchanged(self):
        flow=converter.cold_flow("common.callback.041",self.programs[41])
        actual=next(f for f in self.document["flows"] if f["id"]==flow["id"])
        self.assertEqual(actual,flow)
        self.assertEqual(self.programs[41],[("story",[3]),("loot_child",[1,-1]),("story",[4])])
        old=next(f for f in self.document["flows"] if f["id"]=="common.loot.child.1")
        self.assertEqual(converter.digest(converter.encode(old).encode()),"6fa7527cb03af747d24ea77e659aa11ec75d69241a5db63bdf88eeb7be205644")

    def test_real_shared_entries_and_noops_deduplicate_without_new_state(self):
        self.assertEqual(self.programs[5], self.programs[32])
        self.assertEqual(self.programs[24], self.programs[34])
        for slot in range(85, 90):
            self.assertEqual(converter.cold_flow("noop", self.programs[slot]), {"id": "noop", "entry": "done", "nodes": [{"id": "done", "op": "end", "args": [], "on": {}}]})

    def test_all_generated_edges_and_arg_widths_validate(self):
        flows = [converter.cold_flow(f"test.{slot}", program) for slot, program in self.programs.items()]
        converter.validate_candidates(flows, self.ops)
        self.assertEqual(len(flows), 74)  # S3 73 + S5完整355f0参数纠正51。
        self.assertEqual(len(converter.cold_flow("pulse", self.programs[76])["nodes"]), 1546)

    def test_nested_branches_have_unique_nodes_and_both_paths(self):
        flow = converter.cold_flow("nested", self.programs[75])
        self.assertEqual(len(flow["nodes"]), len({n["id"] for n in flow["nodes"]}))
        meta = bytes(131)
        cases = converter.mechanism_cases(meta)
        observed = {}
        for case in cases:
            for label, port in converter.graph_trace(flow, case, {})["branches"]:
                observed.setdefault(label, set()).add(port)
        self.assertTrue(observed)
        self.assertTrue(all(ports == {"yes", "no"} for ports in observed.values()))

    def test_wait_error_missing_label_unknown_op_and_bool_arg_refuse(self):
        flow = converter.cold_flow("wait", [converter.action("story", 2)])
        for mutation in ("wrong_port", "missing_label", "unknown_op", "bool_arg"):
            changed = copy.deepcopy(flow)
            node = next(n for n in changed["nodes"] if n["op"] == "story")
            if mutation == "wrong_port": node["on"]["next"] = node["on"].pop("ack")
            elif mutation == "missing_label": node["on"]["ack"] = "missing"
            elif mutation == "unknown_op": node["op"] = "execute_native_callback"
            else: node["args"][0] = True
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                converter.validate_candidates([changed], self.ops)

    def test_signed_half_and_byte_sources_keep_different_widths(self):
        state = bytes([0] * 16 + [255] + [0] * 15)
        self.assertEqual(converter.source_value(0, 0xffffffff, state), 0xffffffff)
        self.assertEqual(converter.source_value(2, 0xffffffff, state), 0)
        self.assertEqual(converter.source_value(2, 0x80000000, state), 0xc0000000)
        self.assertEqual(converter.source_value(3, 0xffffffff, state), 226)
        self.assertEqual(converter.source_value(5, 0, state), 255)

    def test_all_typed_fields_match_original80b_abi_and_text_predicate(self):
        self.assertEqual(converter.FIELD_OFFSETS, (0, 1, 3, 4, 5, 6, 7, 8, 34, 35, 36, 37, 38, 39, 49, 52, 53, 54, 64, 68))
        exe = (ROOT / "original_game/FD2.EXE").read_bytes()
        self.assertEqual(converter.source_range(exe, 0x36008, 4), bytes.fromhex("0fb65008"))
        flow = converter.cold_flow("text", self.programs[75])
        predicate = next(n for n in flow["nodes"] if n["op"] == "branch_actor" and n["args"][-1] == 9)
        self.assertEqual(predicate["args"], [-1, 2, 0, 9])

    def test_original83_tail_pushes_twenty_before_explicit_shared_call(self):
        exe = (ROOT / "original_game/FD2.EXE").read_bytes()
        raw = converter.source_range(exe, 0x3640f, 7)
        self.assertEqual(raw[:2], bytes.fromhex("6a14"))
        self.assertEqual(raw[2], 0xe9)
        self.assertEqual(0x36416 + int.from_bytes(raw[3:], "little", signed=True), 0x356aa)
        self.assertEqual(converter.source_range(exe, 0x356a8, 2), bytes.fromhex("6a12"))
        self.assertIn(("unit_hp_zero_tail", [20]), self.programs[83])
        self.assertNotIn(("unit_hp_zero_tail", [18]), self.programs[83])

    def test_indirect_call_and_missing_explicit_tail_block(self):
        for raw in ("ffd0", "e900010000"):
            domain = {"slots": [{"slot": 0, "target": 0x10000}], "functions": [{"instructions": [{"ea": 0x10000, "raw_hex": raw}]}]}
            with self.subTest(raw=raw), self.assertRaises(ValueError): converter.source_cfg_branches(domain, 0)

    def test_caller_once_suffix_is_after_story_ack(self):
        flow = converter.cold_flow("once", self.programs[10])
        case = converter.mechanism_cases(bytes(131))[0]
        full = converter.graph_trace(flow, case, {})["trace"]
        self.assertEqual(full, [("unit_ai_write", [16, 71, 0]), ("story", [1]), ("state_write", [16, 1, 1])])
        prefix = converter.graph_trace(flow, case, {}, stop_after=2)["trace"]
        self.assertEqual(prefix, full[:2])
        self.assertNotIn(("state_write", [16, 1, 1]), prefix)


if __name__ == "__main__":
    unittest.main()
