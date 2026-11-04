"""移动表oracle的纯数据回归；不启动进程、窗口或IDA，不写原版。"""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
ROOT = next(parent for parent in Path(__file__).resolve().parents if (parent / "tools/rebuild_fd2_analysis.py").is_file())
sys.path[:0] = [str(TOOLS), str(ROOT / "tools")]
from fd2_movement_oracle import (COST_CLASS_COUNT, MOVEMENT_VA, PROFILE_COUNT, TABLE_SHA256,
                                 TABLE_SIZE, movement_profile, movement_table)
from rebuild_fd2_analysis import parse_le, parse_objects


class MovementOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = ROOT / "original_game/FD2.EXE"
        cls.raw = cls.exe.read_bytes()
        cls.le = parse_le(cls.raw)
        cls.owner = parse_objects(cls.raw, cls.le)[2]
        cls.obj_record = cls.le.le_off + cls.le.objtab_off + 2 * 24
        cls.map_entry = cls.le.le_off + cls.le.objmap_off + (cls.owner.page_map_index - 1 + 1) * 4

    def rejects(self, data):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "FD2.EXE"
            executable.write_bytes(data)
            with self.assertRaises(ValueError):
                movement_table(executable)

    def test_real_le_canonical_mapping_and_full_table_hash_are_exact(self):
        table, receipt = movement_table(self.exe)
        self.assertEqual((len(table), receipt["canonical_va"], receipt["object"]), (580, 0x61646, 3))
        self.assertEqual(receipt["sha256"], TABLE_SHA256)
        self.assertEqual(receipt["pages"], [{"object_page": 1, "page_number": 69, "file_offset": 0x7a65a}])
        self.assertEqual(table, self.raw[receipt["pages"][0]["file_offset"]:receipt["pages"][0]["file_offset"] + 580])
        self.assertNotEqual(hashlib.sha256(self.raw[0x7a64a:0x7a64a + 580]).hexdigest(), TABLE_SHA256)

    def test_real_attack_profile_all_twenty_classes_cost_one_and_movement_class_one_is_not_flat(self):
        table, _ = movement_table(self.exe)
        self.assertEqual(table[:20], bytes([1] * 20))
        self.assertEqual((min(table), max(table)), (1, 20))
        profile, receipt = movement_profile(self.exe, 1)
        self.assertEqual((profile[0], profile[1]), (1, 20))
        self.assertEqual(receipt["selected_profile"], 1)
        self.assertEqual(len(profile), COST_CLASS_COUNT)
        profile5, _ = movement_profile(self.exe, 5)
        self.assertEqual(profile5[:6], bytes((1, 20, 1, 2, 2, 20)))

    def test_every_supported_profile_is_sliced_from_validated_580_bytes(self):
        table, _ = movement_table(self.exe)
        for index in range(PROFILE_COUNT):
            profile, receipt = movement_profile(self.exe, index)
            self.assertEqual(profile, table[index * 20:(index + 1) * 20])
            self.assertEqual(receipt["sha256"], TABLE_SHA256)

    def test_invalid_profile_fails_before_reading_executable(self):
        for index in (-1, 29, "1", None):
            with self.subTest(index=index), self.assertRaises(ValueError):
                movement_profile(Path("not-present"), index)

    def test_one_changed_table_byte_rejects_wrong_source(self):
        data = bytearray(self.raw)
        data[0x7a65a + TABLE_SIZE - 1] ^= 1
        self.rejects(data)

    def test_partial_table_file_and_truncated_le_are_rejected(self):
        self.rejects(self.raw[:0x7a65a + TABLE_SIZE - 1])
        self.rejects(self.raw[:self.le.le_off + 0x40])

    def test_table_cannot_be_beyond_virtual_size_or_file_backed_page_count(self):
        for offset, value in ((0, MOVEMENT_VA - self.owner.relbase + TABLE_SIZE - 1), (16, 1)):
            data = bytearray(self.raw)
            struct.pack_into("<I", data, self.obj_record + offset, value)
            self.rejects(data)

    def test_malformed_page_sizes_and_objects_fail_before_large_allocation(self):
        for offset, value in ((self.le.le_off + 0x28, 0), (self.le.le_off + 0x28, 0xffffffff),
                              (self.le.le_off + self.le.objtab_off + 4, 0xffff0000),
                              (self.obj_record + 16, 0xffffffff), (self.le.le_off + 0x14, 0xffffffff)):
            data = bytearray(self.raw)
            struct.pack_into("<I", data, offset, value)
            with patch("fd2_movement_oracle.build_relbase_image", side_effect=AssertionError("invalid LE reached allocation")):
                self.rejects(data)

    def test_invalid_uninitialized_or_unsupported_page_does_not_read_other_file_bytes(self):
        data = bytearray(self.raw)
        data[self.map_entry + 3] = 1
        self.rejects(data)
        for page in (0, self.le.num_pages + 1):
            data = bytearray(self.raw)
            data[self.map_entry:self.map_entry + 3] = page.to_bytes(3, "big")
            self.rejects(data)


if __name__ == "__main__":
    unittest.main()
