"""B7原资源前置/受控派生纯回归；合成入口不是原版运行证据。"""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_stage1_complete_oracle as o
from fd2_stage1_oracle import stage_tables
from fd2_transition_oracle import derive_prepare_fixture, entry_geometry, entry_initialized_records

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = ROOT / "original_game"


def synthetic_entry():
    _, source, state, _ = derive_prepare_fixture(ORIGINAL)
    result = bytearray(source)
    geometry = entry_geometry(ORIGINAL, state)
    records = entry_initialized_records(ORIGINAL, state, geometry)
    metadata = o.archive_entry((ORIGINAL / "FDFIELD.DAT").read_bytes(), 4)
    result[:len(metadata)] = metadata
    result[0x8a3:0x12a3] = state["roster"]
    for raw, (record, mask) in enumerate(records):
        record = bytearray(record)
        for at, known in enumerate(mask):
            if not known:
                record[at] = (0xa0 + raw + at) & 255
        result[o.UNITS + raw * 80:o.UNITS + (raw + 1) * 80] = record
    result[0x30a3:0x30c3] = bytes(32)
    result[o.META:o.META + 10] = bytes((1, 21, 1, 9, 13, 19, 14, 10, 1, 5))
    struct.pack_into("<I", result, o.META + 10, state["resource"])
    for at, value in zip((15, 14, 16, 17), state["options"]):
        result[o.META + at] = value
    return o.checksum(result)


def synthetic_post_turn3():
    result = bytearray(synthetic_entry())
    for index, record in enumerate(stage_tables(ORIGINAL)["records"]):
        result[o.UNITS + (21 + index) * 80:o.UNITS + (22 + index) * 80] = record
    result[o.META:o.META + 2] = bytes((4, 27))
    return o.checksum(result)


class Stage1SourceAndFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.entry = synthetic_entry()
        cls.post = synthetic_post_turn3()

    def fixture(self, folder, plain):
        original = o.decode_save(ORIGINAL / "START.SAV")
        encoded = (ORIGINAL / "START.SAV").read_bytes()
        path = Path(folder) / "controlled.SAV"
        path.write_bytes(bytes(a ^ b ^ c for a, b, c in zip(encoded, original, plain)))
        self.assertEqual(o.decode_save(path), plain)
        return path

    def test_entry_uses_own_complete_persistent_not_controlled_default(self):
        before = hashlib.sha256(self.entry).hexdigest()
        checked = o.validate_entry_source(ORIGINAL, self.entry)
        self.assertEqual(checked["map_size"], [27, 21])
        self.assertFalse(checked["runtime_provenance_proven_by_semantic_validation_alone"])
        self.assertEqual(hashlib.sha256(self.entry).hexdigest(), before)
        changed = bytearray(self.entry)
        # 原记录未知字节由本source保留，不归零或误充新玩法。
        changed[o.UNITS + 5 * 80 + 0x28] ^= 17
        checked = o.validate_entry_source(ORIGINAL, o.checksum(changed))
        self.assertTrue(checked["unknown_source_bytes_pinned_not_normalized"])

    def test_bad_checksum_stage_shape_constructor_and_camera_rejected(self):
        for at in (-1, o.META + 2, 0, o.UNITS + 5 * 80 + 7, o.META + 3):
            changed = bytearray(self.entry)
            changed[at] = 255
            data = bytes(changed) if at == -1 else o.checksum(changed)
            with self.subTest(offset=at), self.assertRaises((ValueError, AssertionError)):
                o.validate_entry_source(ORIGINAL, data)

    def test_postturn3_requires_actual_27_identity_order(self):
        self.assertEqual(o.validate_post_turn3_source(ORIGINAL, self.post)["existing_active_count"], 27)
        with self.assertRaises(ValueError):
            o.validate_post_turn3_source(ORIGINAL, self.entry)
        changed = bytearray(self.post)
        changed[o.UNITS + 26 * 80 + 7] = 97
        with self.assertRaises(ValueError):
            o.validate_post_turn3_source(ORIGINAL, o.checksum(changed))

    def test_natural_identity_and_complete_manifest_keep_every_byte_and_mtime(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.fixture(folder, self.entry)
            before, stamp = path.read_bytes(), path.stat().st_mtime_ns
            encoded, plain, manifest = o.derive_fixture(ORIGINAL, path, "natural-stage1")
            self.assertEqual(encoded, before)
            self.assertEqual(plain, self.entry)
            self.assertEqual(manifest["plaintext_patches"], [])
            self.assertFalse(manifest["controlled_fixture"])
            self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), (before, stamp))

    def test_controlled_victory_branches_only_declared_flags_inventory_checksum(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.fixture(folder, self.post)
            for scenario in ("victory-all-villagers", "victory-villager-loss", "victory-full-inventory"):
                encoded, plain, manifest = o.derive_fixture(ORIGINAL, path, scenario)
                self.assertTrue(manifest["controlled_fixture"])
                self.assertFalse(manifest["natural_stage1_battle"])
                self.assertEqual(plain[:o.UNITS], self.post[:o.UNITS])
                self.assertEqual(plain[o.META:-4], self.post[o.META:-4])
                self.assertTrue(all(o.actor(plain, raw)[5] & 1 for raw in range(11, 27)))
                actual = [{"offset": at, "before": a, "after": z} for at, (a, z) in enumerate(zip(self.post, plain)) if a != z]
                self.assertEqual(actual, manifest["plaintext_patches"])
                if scenario == "victory-full-inventory":
                    self.assertTrue(all(o.actor(plain, raw)[10:26] == bytes((0, 192)) * 8 for raw in range(5)))
            path = self.fixture(folder, self.entry)
            with self.assertRaises(ValueError):
                o.derive_fixture(ORIGINAL, path, "victory-all-villagers")

    def test_reward_first_side2_slot_even_hidden_and_unknown_bytes_unchanged(self):
        changed = bytearray(self.post)
        changed[o.UNITS + 10:o.UNITS + 26] = bytes((0, 192)) * 8
        changed[o.UNITS + 80 + 5] |= 1
        source = o.checksum(changed)
        result, detail = o.reward_inventory_write(source)
        self.assertEqual(detail["fragment"], 6)
        self.assertEqual(detail["item198_recipient_raw"], 1)
        diff = {at for at, (a, z) in enumerate(zip(source, result)) if a != z}
        allowed = set(range(o.UNITS + 80 + 10, o.UNITS + 80 + 26)) | set(range(o.FILE_SIZE - 4, o.FILE_SIZE))
        self.assertTrue(diff <= allowed)

    def test_one_villager_hidden_no_reward_and_full_all_side2_no_replacement(self):
        source = bytearray(self.post)
        source[o.UNITS + 5 * 80 + 5] |= 1
        source = o.checksum(source)
        result, detail = o.reward_inventory_write(source)
        self.assertEqual(result, source)
        self.assertEqual(detail["fragment"], 7)
        source = bytearray(self.post)
        for raw in range(5):
            source[o.UNITS + raw * 80 + 10:o.UNITS + raw * 80 + 26] = bytes((0, 192)) * 8
        source = o.checksum(source)
        result, detail = o.reward_inventory_write(source)
        self.assertEqual(result, source)
        self.assertTrue(detail["full_inventory_silent_no_write"])
        self.assertIsNone(detail["item198_recipient_raw"])

    def test_actual_three_chest_records_not_use_effect_claims(self):
        rows = o.stage1_chests(ORIGINAL)
        self.assertEqual([(r["item_id"], r["world"], r["mode"], r["map_cell_index"]) for r in rows],
            [(129, [10, 13], 0, 1), (201, [6, 20], 0, 2), (193, [21, 17], 0, 3)])


class Stage2ReloadOwnerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from test_stage1_complete_runner import active_fixture
        cls.data = o.tables(ORIGINAL)
        cls.state, _, _ = o.settlement_projection(synthetic_post_turn3(), cls.data)
        cls.projection = o.stage2_projection(cls.state, cls.data)
        manual = o.write_stage2_slot(synthetic_post_turn3(), cls.state, 0)
        preload = bytearray(active_fixture(manual, cls.state, cls.projection, cls.data))
        preload[1171:2211] = bytes((index * 17 + 3) & 255 for index in range(1040))
        for raw in range(15):
            preload[o.UNITS + raw * 80 + 2] = 32 + raw
            if raw >= 6:
                preload[o.UNITS + raw * 80 + 40:o.UNITS + raw * 80 + 49] = bytes((raw * 13 + i) & 255 for i in range(9))
        cls.preload = o.checksum(preload)
        native = bytearray(cls.preload)
        # 当前原模板ID为0/9/4/30/51/8/2及八个76；11019按ID首次出现分class。
        for raw, cache in enumerate(list(range(7)) + [7] * 8):
            native[o.UNITS + raw * 80 + 2] = cache
        cls.native = o.checksum(native)

    def check(self, measured, policy, view=None, before=None):
        return o.check_stage2_reloaded_active_save(self.preload if before is None else before,
            measured, self.state, self.projection, self.preload if view is None else view, self.data,
            cache_policy=policy)

    def test_each_endpoint_cache_policy_and_full_owner_positive_without_input_mutation(self):
        for policy, actual in (("original", self.native), ("sdl", self.preload)):
            before, measured = bytearray(self.preload), bytearray(actual)
            saved = bytes(before), bytes(measured)
            checked = self.check(measured, policy, before=before)
            self.assertTrue(checked["loaded_unknown_owner_preserved"])
            self.assertTrue(checked["full_container_reload_checked"])
            self.assertFalse(checked["fresh_unknown_exclusions_reused"])
            self.assertEqual(checked["cache_policy"], policy)
            self.assertNotIn("native_metadata_heap_tail", checked)
            self.assertNotIn("native_cache_and_exact_ctor_holes", checked)
            self.assertEqual((before, measured), saved)

    def test_loaded_tail_all_npc_nine_bytes_and_other_owner_corruption_rejected(self):
        offsets = [1171, 2210]
        offsets += [o.UNITS + raw * 80 + at for raw in range(6, 15) for at in range(40, 49)]
        offsets += [0x8a3 + 400 + 40, 0x12a2, o.UNITS + 15 * 80 + 40, 0x30a2,
            0x30a3, o.META + 10, o.META + 14, o.META + 18, o.UNITS + 5, o.UNITS + 6 * 80 + 5]
        offsets += [o.SLOT_BASE + slot * o.SLOT_STRIDE + 400 + 40 for slot in range(4)]
        for policy, actual in (("original", self.native), ("sdl", self.preload)):
            for at in offsets:
                changed = bytearray(actual); changed[at] ^= 1
                with self.subTest(policy=policy, offset=at), self.assertRaises(AssertionError):
                    self.check(o.checksum(changed), policy)

    def test_only_original_current_id_rebuild_or_sdl_preload_cache_allowed(self):
        with self.assertRaises(AssertionError): self.check(self.preload, "original")
        with self.assertRaises(AssertionError): self.check(self.native, "sdl")
        for policy, actual in (("original", self.native), ("sdl", self.preload)):
            for raw in (0, 7, 14, 15):
                changed = bytearray(actual); changed[o.UNITS + raw * 80 + 2] ^= 128
                with self.subTest(policy=policy, raw=raw), self.assertRaises(AssertionError):
                    self.check(o.checksum(changed), policy)
        source = bytearray(self.preload); source[o.UNITS + 2] = 255; source = o.checksum(source)
        # SDL按原SAV兼容合同保源raw2，不把源未知值变成允许随意改写的mask。
        self.assertTrue(self.check(source, "sdl", before=source)["loaded_unknown_owner_preserved"])

    def test_independent_expected_view_only_six_meta_bytes_not_measured_view_adoption(self):
        import fd2_natural_oracle as n
        view = n.navigation(self.preload, (6, 21))[-1][1]
        self.assertEqual(list(view[o.META + 5:o.META + 7]), [6, 21])
        for policy, actual in (("original", self.native), ("sdl", self.preload)):
            changed = bytearray(actual)
            changed[o.META + 3:o.META + 9] = view[o.META + 3:o.META + 9]
            changed = o.checksum(changed)
            self.assertTrue(self.check(changed, policy, view)["full_container_reload_checked"])
            with self.assertRaises(AssertionError): self.check(changed, policy)
            changed = bytearray(changed); changed[o.META + 9] ^= 1
            with self.assertRaises(AssertionError): self.check(o.checksum(changed), policy, view)

    def test_fresh_entry_holes_still_legal_but_same_loaded_owner_changes_rejected(self):
        for at in (1171, 2210, *(o.UNITS + 6 * 80 + i for i in range(40, 49))):
            changed = bytearray(self.preload); changed[at] ^= 1; changed = o.checksum(changed)
            fresh = o.check_stage2_active_save(self.preload, changed, self.state, self.projection, self.preload, self.data)
            self.assertEqual(fresh["active_count"], 15)
            with self.subTest(offset=at), self.assertRaises(AssertionError): self.check(changed, "sdl")

    def test_bad_checksum_length_policy_and_source_checksum_rejected(self):
        bad = bytearray(self.preload); bad[-1] ^= 1
        for actual in (bad, self.preload[:-1], self.preload + b"\0"):
            with self.subTest(length=len(actual)), self.assertRaises((AssertionError, ValueError)):
                self.check(actual, "sdl")
        with self.assertRaises(AssertionError): self.check(self.preload, "sdl", before=bad)
        with self.assertRaises(ValueError): self.check(self.preload, "sdl", view=self.preload[:-1])
        for policy in (None, "", "native", "original-or-sdl"):
            with self.subTest(policy=policy), self.assertRaises(ValueError): self.check(self.preload, policy)


if __name__ == "__main__":
    unittest.main()
