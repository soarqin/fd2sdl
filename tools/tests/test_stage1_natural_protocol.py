"""B7复用自然协议的stage显式参数；纯查询/资源测试，无进程与窗口。"""
import copy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_natural_oracle as n
import fd2_stage1_complete_oracle as o
import run_original_natural as runner
from run_original_focus import native_focus_reference
from test_stage1_complete_oracle import ORIGINAL, synthetic_entry


def wait_plan(source):
    raw = 0
    return {"planning_only": True, "original_acceptance": False, "query_side_effects": False,
        "source_turn": source[o.META], "source_active_count": source[o.META + 1],
        "source_roster_count": source[o.META + 9], "rest_tail": [],
        "next_action": {"kind": "wait", "actor_raw": raw, "target_raw": -1,
            "actor_record_hex": n.actor(source, raw).hex(), "destination": list(n.actor(source, raw)[:2]),
            "reachable_path_directions": [], "command_disabled": [1, 1, 0, 0], "command_first_enabled": 2}}


class Stage1NaturalProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stage0 = o.decode_save(ORIGINAL / "START.SAV")
        source = bytearray(synthetic_entry())
        # 旧prepare-controlled第五人51不是自然stage0剧情加入的unit1；
        # 此处只为Reader资源协议造自然身份，不声称此合成SAV有原版来源。
        source[o.UNITS + 4 * 80 + 7:o.UNITS + 4 * 80 + 9] = bytes((1, 1))
        source[0x8a3 + 4 * 80 + 7:0x8a3 + 4 * 80 + 9] = bytes((1, 1))
        cls.stage1 = o.checksum(source)
        cls.assets = runner.round_assets(ORIGINAL)

    def test_default_stage0_same_actions_and_focus_reference(self):
        plan = wait_plan(self.stage0)
        self.assertEqual(n.validate_plan(plan, self.stage0), n.validate_plan(plan, self.stage0, stage=0))
        self.assertEqual(n._stage_map(ORIGINAL, self.stage0)[2:], (24, 24))
        route = n.navigation(self.stage0, (23, 23))
        old = native_focus_reference(tuple(self.stage0[o.META + 3:o.META + 5]),
            tuple(self.stage0[o.META + 5:o.META + 7]), (23, 23), (24, 24))
        self.assertEqual([list(state[o.META + 3:o.META + 5]) for _, state in route],
            [step["camera"] for step in old["steps"]])
        self.assertEqual([list(state[o.META + 5:o.META + 7]) for _, state in route],
            [step["world"] for step in old["steps"]])

    def test_explicit_stage1_true27x21_and_private_handler_lower_bound(self):
        source = bytearray(self.stage1)
        source[o.UNITS:o.UNITS + 2] = bytes((26, 20))
        source = o.checksum(source)
        plan = wait_plan(source)
        self.assertEqual(n._stage_map(ORIGINAL, source)[2:], (27, 21))
        self.assertEqual(n.validate_plan(plan, source, stage=1), [plan["next_action"]])
        with self.assertRaises(ValueError): n.validate_plan(plan, source)
        for destination in ([27, 20], [26, 21], [True, 20]):
            bad = copy.deepcopy(plan); bad["next_action"]["destination"] = destination
            with self.subTest(destination=destination), self.assertRaises(ValueError):
                n.validate_plan(bad, source, stage=1)
        bad = bytearray(source); bad[o.META + 1] = 10; bad = o.checksum(bad)
        with self.assertRaises(ValueError): n.validate_plan(wait_plan(bad), bad, stage=1)
        with self.assertRaises(ValueError): n.validate_plan(plan, source, stage=2)
        route = n.navigation(source, (26, 20))
        self.assertTrue(route)
        self.assertEqual(list(route[-1][1][o.META + 5:o.META + 7]), [26, 20])
        self.assertTrue(all(state[o.META + 3] <= 14 and state[o.META + 4] <= 13 for _, state in route))

    def test_stage1_range_uses_original_stage1_tiles_no_source_mutation(self):
        source = self.stage1
        proof = n.range_probe(ORIGINAL, source, self.assets, 0)
        self.assertTrue(proof)
        self.assertTrue(all(0 <= p["world"][0] < 27 and 0 <= p["world"][1] < 21 for p in proof))
        self.assertEqual(source, self.stage1)
        unsupported = bytearray(source); unsupported[o.META + 2] = 3
        with self.assertRaises(ValueError): n.range_probe(ORIGINAL, unsupported, self.assets, 0)

    def test_reader_explicit_story_branch_and_victory10_defaults_unchanged(self):
        # 真实stage1词元页，由B7调用端提供；不能默默加载stage0首关故事。
        pages = o.settlement_pages(ORIGINAL, self.assets, self.stage1)
        report = {"resource_acknowledgements": []}
        with patch.object(runner, "stage0_pages", side_effect=AssertionError("must use provided stage1 stories")):
            reader = runner.ProgressReader(ORIGINAL, self.stage1, self.assets, report,
                stories=pages, victory_fragment=10, nonphase_story_fragments=(6, 7, 8, 9, 10))
        self.assertEqual(reader.victory_fragment, 10)
        self.assertEqual(reader.nonphase_story_fragments, (6, 7, 8, 9, 10))
        self.assertEqual(set(reader.fragments), {6, 8, 9, 10})
        page = reader.fragments[10][-1]
        reader.story = (reader.fragments[10], len(reader.fragments[10]) - 1)
        capture = type("Capture", (), {"key": lambda self, key: None})()
        reader.acknowledge(capture, {"page": page, "signature": ["story", 10, page["token_offset"]]}, None)
        self.assertTrue(reader.victory_text_complete)
        # default仍调用原stage0页，胜利为9，而不是由新stage1覆盖。
        with patch.object(runner, "stage0_pages", return_value=[] ) as default:
            old = runner.ProgressReader(ORIGINAL, self.stage0, self.assets, report)
        default.assert_called_once()
        self.assertEqual((old.victory_fragment, old.nonphase_story_fragments), (9, (8, 9)))


if __name__ == "__main__": unittest.main()
