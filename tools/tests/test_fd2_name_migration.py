#!/usr/bin/env python3
"""Regression tests for the legacy function-name migration ledger."""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from build_fd2_name_migration import build  # noqa: E402


class FunctionNameMigrationTest(unittest.TestCase):
    def test_all_legacy_rows_are_accounted_for_without_address_guessing(self) -> None:
        ledger, confirmed = build(
            ROOT / "docs" / "reverse-engineering" / "function-names.md")
        self.assertEqual(ledger["row_count"], 210)
        self.assertEqual(ledger["confirmed_row_count"], 6)
        self.assertEqual(ledger["unresolved_row_count"], 204)
        self.assertEqual(ledger["duplicate_alias_count"], 0)
        self.assertEqual(confirmed["count"], 8)
        expected = {
            "fd2_le_entry": 0x3CCB4,
            "title_action_menu": 0x1F894,
            "animation_play": 0x20421,
            "music_track_play": 0x25977,
            "sfx_play": 0x25A96,
            "new_game_opening_play": 0x3231B,
            "dialog_text_scroll_up": 0x16E24,
            "__chkstk": 0x3702F,
        }
        self.assertEqual(
            {row["name"]: row["va"] for row in confirmed["functions"]}, expected)
        self.assertTrue(all(
            row["transformation"] == "none-address-space-not-proven"
            for row in ledger["rows"]
            if row["disposition"] == "legacy-unresolved"))


if __name__ == "__main__":
    unittest.main()
