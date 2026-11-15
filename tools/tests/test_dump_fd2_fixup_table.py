#!/usr/bin/env python3
"""Regression tests for FD2 LE fixup targets mapped into code0."""

import sys
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from analyze_fd2_stage_code import is_dual_code_view, stage_targets  # noqa: E402
from dump_fd2_fixup_table import (  # noqa: E402
    collect_internal_offset_fixups,
    fd2_analysis_offset,
    object1_code0_offset,
)


class Object1Code0MappingTest(unittest.TestCase):
    def collect_pages(self, first, second):
        # 两页边界上的同一DWORD：page0+0xfff与page1-1。
        data = struct.pack('<3I', 0, len(first), len(first)+len(second)) + first + second
        header = SimpleNamespace(le_off=0, num_pages=2, page_size=0x1000,
            fixup_page_table_off=0, fixup_record_table_off=12)
        obj = SimpleNamespace(index=1, relbase=0x10000, vsize=0x2000,
            page_map_index=1, page_count=2)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'synthetic.exe';path.write_bytes(data)
            with patch('dump_fd2_fixup_table.parse_le',return_value=header),\
                    patch('dump_fd2_fixup_table.parse_objects',return_value=[obj]):
                return collect_internal_offset_fixups(path)

    def test_signed_cross_page_source_is_deduplicated_without_ghost(self):
        result=self.collect_pages(bytes.fromhex('0700ff0f011000'),bytes.fromhex('0700ffff011000'))
        self.assertEqual(result,{0x10fff:(1,0x10,0x10010)})

    def test_conflicting_and_overlapping_sources_are_rejected(self):
        for second in ('0700ffff012000','0700feff011000'):
            with self.subTest(second=second),self.assertRaises(ValueError):
                self.collect_pages(bytes.fromhex('0700ff0f011000'),bytes.fromhex(second))

    def test_unknown_or_truncated_records_are_not_silently_skipped(self):
        for second in ('0600ffff011000','0700ffff01'):
            with self.subTest(second=second),self.assertRaises(ValueError):
                self.collect_pages(bytes.fromhex('0700ff0f011000'),bytes.fromhex(second))

    def test_stage_dispatch_target_is_object_offset(self) -> None:
        self.assertEqual(object1_code0_offset(0x2231B), 0x2231B)

    def test_field_event_targets_are_object_offsets(self) -> None:
        self.assertEqual(object1_code0_offset(0x24531), 0x24531)
        self.assertEqual(object1_code0_offset(0x2460B), 0x2460B)
        self.assertEqual(object1_code0_offset(0x24673), 0x24673)
        self.assertEqual(object1_code0_offset(0x246CD), 0x246CD)

    def test_object3_script_target_is_object_offset(self) -> None:
        self.assertEqual(fd2_analysis_offset(0x29D1), 0x29D1)

    def test_negative_target_rejected(self) -> None:
        with self.assertRaises(ValueError):
            fd2_analysis_offset(-1)

    def test_view_detection_ignores_filename(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            renamed_code0 = Path(directory) / "analysis.bin"
            payload = b"\x11" * 0x10000 + b"\x22" * 0x10000
            renamed_code0.write_bytes(payload)
            self.assertFalse(is_dual_code_view(renamed_code0))

            renamed_dual = Path(directory) / "another.bin"
            renamed_dual.write_bytes(payload[:0x10000] + payload)
            self.assertTrue(is_dual_code_view(renamed_dual))

    @patch("analyze_fd2_stage_code.collect_internal_offset_fixups")
    def test_stage_target_consumers(self, collect) -> None:
        collect.return_value = {
            0x51D71: (1, 0x24531, 0x34531),
            0x51D75: (2, 0x1234, 0x51234),
        }
        exe = Path("unused.exe")
        self.assertEqual(
            stage_targets(exe, 0x1D71, 2, Path("fd2_le_code0.bin")),
            [0x24531, 0x51234],
        )
        self.assertEqual(
            stage_targets(exe, 0x1D71, 2, Path("fd2_le_dual_clean.bin")),
            [0x34531, 0x51234],
        )


if __name__ == "__main__":
    unittest.main()
