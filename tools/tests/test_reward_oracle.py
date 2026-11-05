import sys
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fd2_reward_oracle as oracle
from run_graphics_tests import decode_save
from run_original_end_turn import UNITS, STRIDE, round_assets

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = (ROOT / "original_game").resolve()


class RewardOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tables, cls.evidence = oracle.reward_tables(ORIGINAL / "FD2.EXE")
        cls.source = decode_save(ORIGINAL / "START.SAV")
        cls.actor = cls.source[UNITS + STRIDE:UNITS + 2 * STRIDE]
        cls.assets = round_assets(ORIGINAL)

    def test_full_strict_le_readsets_and_real_learning(self):
        self.assertEqual(len(self.tables["growth"]), 748)
        self.assertEqual(len(self.tables["learning"]), 240)
        self.assertEqual(self.evidence["growth"]["canonical_va"], 0x620a1)
        self.assertEqual(self.evidence["learning"]["sha256"], oracle.LEARNING_SHA256)
        row = self.tables["growth"][9 * 11:10 * 11]
        self.assertEqual(bytes(row), bytes.fromhex("0306020501030509040800"))
        self.assertEqual(list(self.tables["learning"][:10]), [5, 17, 9, 1, 15, 26, 21, 2, 26, 27])

    def test_nonlevel_does_not_recompute_stale_values_or_touch_hp_mp(self):
        actor = bytearray(self.actor)
        actor[0x48:0x50] = b"\xa5" * 8
        before = bytes(actor)
        result = oracle.reward_plan(actor, 20, self.tables)
        expected = bytearray(before); expected[0x3c] = 20
        self.assertEqual(result["final_record"], expected)
        self.assertEqual(bytes(actor), before)
        self.assertEqual([e["fragment"] for e in result["events"]], [488])
        self.assertTrue(result["raw_contribution_cleared"])

    def test_single_level_learning_and_current_hp_mp_retained(self):
        actor = bytearray(self.actor)
        actor[0x21], actor[0x3c] = 4, 99
        actor[0x1b] |= 0x20  # 明列的恢复术13夹具能力，不冒称自然学习。
        actor[0x44] = 5  # 已完成一次费用后的奖励入口。
        before = bytes(actor)
        result = oracle.reward_plan(actor, 5, self.tables)
        final = result["final_record"]
        self.assertEqual((final[0x21], final[0x3c]), (5, 4))
        self.assertEqual(final[0x40:0x42], before[0x40:0x42])
        self.assertEqual(final[0x44:0x46], before[0x44:0x46])
        self.assertEqual(int.from_bytes(final[0x42:0x44], "little"), 37)
        self.assertEqual(int.from_bytes(final[0x46:0x48], "little"), 16)
        self.assertTrue(final[0x1c] & 2)
        self.assertEqual([e["fragment"] for e in result["events"]], [488, 489, 490, 491, 492, 493, 494, 587])
        for event in result["events"]:
            if event["kind"] == "growth":
                offset = event["offset"]
                self.assertNotEqual(event["record_before"][offset:offset + 2], event["record_after"][offset:offset + 2])
        allowed = set(range(0x1a, 0x1f)) | {0x21, 0x3c} | set(range(0x48, 0x50))
        for offset, _ in oracle.GROWTH_FIELDS:
            allowed.update((offset, offset + 1))
        self.assertTrue(all(a == b or i in allowed for i, (a, b) in enumerate(zip(before, final))))
        self.assertEqual(actor, before)

    def test_uncapped_six_level_model_and_precise_caps(self):
        hero = self.source[UNITS:UNITS + STRIDE]
        result = oracle.reward_plan(hero, 630, self.tables)
        self.assertEqual((result["levels_gained"], result["final_record"][0x21], result["final_record"][0x3c]), (6, 7, 30))
        self.assertEqual(int.from_bytes(result["final_record"][0x42:0x44], "little"), 114)
        for identity, level in ((0, 40), (30, 99), (31, 99)):
            actor = bytearray(hero); actor[7] = identity; actor[0x21] = level
            early = oracle.reward_plan(actor, 630, self.tables)
            self.assertFalse(early["entered"])
            self.assertFalse(early["raw_contribution_cleared"])
            self.assertEqual(early["final_record"], actor)
        actor = bytearray(hero); actor[0x21] = 39
        # 原版没有循环>=40门；AI630可跨40，不能用玩家cap99规则替代。
        self.assertEqual(oracle.reward_plan(actor, 630, self.tables)["final_record"][0x21], 45)
        actor[0x21] = 29
        self.assertEqual((oracle.reward_plan(actor, 630, self.tables)["final_record"][0x21],
                          oracle.reward_plan(actor, 630, self.tables)["final_record"][0x3c]), (30, 0))

    def test_duplicate_learning_still_prompts_and_preserves_unknown_bits(self):
        actor = bytearray(self.actor)
        actor[7], actor[0x21] = 28, 22
        actor[0x1b] |= 8  # 已经会ID11。
        actor[0x1e] = 0xa5
        result = oracle.reward_plan(actor, 100, self.tables)
        self.assertEqual([e["magic_id"] for e in result["events"] if e["kind"] == "learning"], [11])
        self.assertEqual(result["final_record"][0x1e], 0xa5)

    def test_invalid_input_never_mutates_record(self):
        for patch, gain in (({7: 68}, 5), ({0x22: 1}, 5), ({}, -1), ({}, 0x80000000)):
            actor = bytearray(self.actor)
            for offset, value in patch.items(): actor[offset] = value
            before = bytes(actor)
            with self.assertRaises(ValueError): oracle.reward_plan(actor, gain, self.tables)
            self.assertEqual(actor, before)

    def test_native_stat_scale_is_binary64_truncation_not_rational_or_nearest(self):
        self.assertEqual([oracle._stat_scale(value) for value in (12, 16, 20, 40, 60, 100, -12, -100)],
                         [13, 18, 23, 46, 69, 114, -13, -114])
        with self.assertRaises(ValueError): oracle._stat_scale(65536)

    def test_resource_pages_reject_missing_arrow_and_wrong_growth_number(self):
        actor = bytearray(self.actor); actor[0x21], actor[0x3c] = 4, 99
        plan = oracle.reward_plan(actor, 5, self.tables)
        pages = oracle.reward_pages(ORIGINAL, self.assets, 9, plan["events"])
        self.assertEqual([page["requires_key"] for page in pages], [False, True, True, True, True, True, True, False])
        for page in pages:
            canvas = Image.new("RGB", (320, 200))
            def paste(image, at): canvas.paste(image.convert("RGB"), at)
            paste(self.assets["dialog_top"], (5, 112))
            paste(page["portraits"][0], page["portrait_position"])
            known = page["text"].point(lambda p: 255 if p else 0, mode="L")
            canvas.paste(page["text"].convert("RGB"), (0, 0), known)
            if page["requires_key"]:
                without = canvas.copy()
                self.assertIsNone(oracle.page_match(without, page, self.assets))
                paste(page["arrows"][0], page["arrow_position"])
            self.assertIsNotNone(oracle.page_match(canvas, page, self.assets))
        growth = next(page for page in pages if page["fragment"] == 490)
        self.assertEqual(growth["value"], 6)
        self.assertEqual(growth["portrait_position"], (8, 115))
        self.assertEqual(pages[0]["portrait_frames"], (0,))
        self.assertEqual(pages[1]["portrait_frames"], (0, 3))
        self.assertEqual(pages[-1]["portrait_frames"], (0, 1, 2))

    def test_exact_six_fixtures_and_full_container_boundaries(self):
        for scenario in oracle.SCENARIOS:
            with self.subTest(scenario=scenario):
                encoded, source, manifest = oracle.derive_fixture(ORIGINAL, scenario)
                result = oracle.expected_state(ORIGINAL, source, scenario, self.tables)
                self.assertEqual(sum(source[:-4]), int.from_bytes(source[-4:], "little"))
                self.assertEqual(source[:UNITS], self.source[:UNITS])
                self.assertEqual(source[UNITS + 12 * STRIDE:-4], self.source[UNITS + 12 * STRIDE:-4])
                final = result["final"]
                self.assertEqual(final[0x30c3], 2)
                self.assertTrue(all(not final[UNITS + raw * STRIDE + 5] & 0x80 for raw in range(12)))
                self.assertEqual(len(manifest["plaintext_patches"]), sum(a != b for a, b in zip(source, self.source)))
                self.assertEqual(len(encoded), len(source))
                if scenario == "status":
                    self.assertEqual([e["fragment"] for e in result["status_events"]], [487, 481, 482, 483, 484, 485, 486])
                    self.assertEqual(int.from_bytes(final[UNITS + 0x40:UNITS + 0x42], "little"), 38)
                    self.assertEqual(final[UNITS + 0x22:UNITS + 0x28], b"\0" * 6)
                    self.assertEqual(list(__import__("struct").unpack_from("<4H", final, UNITS + 0x48)), [16, 12, 97, 2])
                    status_pages = oracle.reward_pages(ORIGINAL, self.assets, 0, result["status_events"])
                    self.assertEqual(len(status_pages), 7)
                    self.assertTrue(all(page["portrait_frames"] == (0, 1, 2) for page in status_pages))
                elif scenario == "counter-multi":
                    self.assertEqual(final[UNITS + 0x21], 7)
                    self.assertEqual(final[UNITS + 0x3c], 30)
                    self.assertEqual(int.from_bytes(final[UNITS + 0x40:UNITS + 0x42], "little"), 38)
                    self.assertEqual(int.from_bytes(final[UNITS + 0x42:UNITS + 0x44], "little"), 114)
                    self.assertEqual(final[UNITS + 11 * STRIDE + 5], 1)
                elif scenario == "learn":
                    actor = final[UNITS + STRIDE:UNITS + 2 * STRIDE]
                    (_, page), data = oracle.magic_page(ORIGINAL, actor, self.assets)
                    self.assertEqual(data["magic_ids"], [0, 13, 17])
                    self.assertEqual(page.size, (310, 99))
                elif scenario == "physical-overwrite":
                    self.assertEqual(final[UNITS + 0x3c], 3)
                    self.assertEqual(int.from_bytes(final[UNITS + 11 * STRIDE + 0x40:UNITS + 11 * STRIDE + 0x42], "little"), 24)
                    self.assertEqual(final[UNITS + 11 * STRIDE + 5], 4)
                    self.assertEqual(final[UNITS + 0x48:UNITS + 0x4a], b"\x03\0")


if __name__ == "__main__":
    unittest.main()
