#!/usr/bin/env python3
"""Regression tests for evidence-tiered FD2 function seeds."""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from build_fd2_function_seeds import build  # noqa: E402


class FunctionSeedTest(unittest.TestCase):
    def test_current_object1_anchor_and_chkstk_evidence(self) -> None:
        code = TOOLS / "fd2_le_code0.bin"
        relocations = TOOLS / "fd2_le_relocation_manifest.tsv"
        if not code.exists() or not relocations.exists():
            self.skipTest("generated LE analysis artifacts are developer-local")
        result = build(code, relocations)
        self.assertEqual(result["schema"], "fd2-function-seeds-v1")
        self.assertEqual(result["chkstk"]["helper_va"], 0x3702F)
        self.assertEqual(result["chkstk"]["direct_caller_count"], 541)
        self.assertEqual(len(result["relocation_code_candidates"]), 634)
        self.assertFalse(result["policy"]["relocation_candidates_auto_create"])
        self.assertFalse(result["policy"]["legacy_markdown_is_seed_source"])

        by_name = {row["name"]: row for row in result["seeds"] if "name" in row}
        expected = {
            "fd2_le_entry": 0x3CCB4,
            "title_action_menu": 0x1F894,
            "animation_play": 0x20421,
            "music_track_play": 0x25977,
            "sfx_play": 0x25A96,
            "new_game_opening_play": 0x3231B,
            "__chkstk": 0x3702F,
        }
        self.assertEqual({name: row["va"] for name, row in by_name.items()}, expected)
        self.assertEqual(by_name["new_game_opening_play"]["entry_kind"],
                         "chkstk-wrapper")
        self.assertEqual(by_name["new_game_opening_play"]["body_va"], 0x32325)
        self.assertEqual(by_name["new_game_opening_play"]["frame_size"], 0x2C)


if __name__ == "__main__":
    unittest.main()
