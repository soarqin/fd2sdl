#!/usr/bin/env python3
"""Regression tests for FD2 LE page extraction."""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from rebuild_fd2_analysis import (  # noqa: E402
    LEHeader,
    ObjectRecord,
    build_code0_image,
    build_dual_clean_image,
    extract_page,
)


class DataPageOffsetTest(unittest.TestCase):
    def test_data_page_offset_is_file_relative(self) -> None:
        # LE +0x80 points to file offset 0x10 directly. The decoy at
        # le_off+data_pages_off must not be selected.
        data = bytearray(0x40)
        data[0x10:0x14] = b"PAGE"
        data[0x30:0x34] = b"BAD!"
        header = LEHeader(
            le_off=0x20,
            num_pages=1,
            page_size=4,
            last_page_size=0,
            objtab_off=0,
            objcnt=0,
            objmap_off=0,
            fixup_page_table_off=0,
            fixup_record_table_off=0,
            data_pages_off=0x10,
            entry_object=1,
            entry_eip=0,
        )
        self.assertEqual(extract_page(bytes(data), header, 1, True), b"PAGE")

    def test_bound_code_tail_is_kept_separate_from_ds(self) -> None:
        data = bytes(range(0x40))
        header = LEHeader(
            le_off=0x20, num_pages=1, page_size=4, last_page_size=0,
            objtab_off=0, objcnt=2, objmap_off=0,
            fixup_page_table_off=0, fixup_record_table_off=0,
            data_pages_off=0x10, entry_object=1, entry_eip=0,
        )
        objects = [
            ObjectRecord(1, 4, 4, 0, 1, 1),
            ObjectRecord(2, 4, 8, 0, 2, 1),
        ]
        code0 = build_code0_image(data, header, objects)
        self.assertEqual(code0, data[0x10:])
        dual = build_dual_clean_image(code0)
        self.assertEqual(dual[0x10000:0x10000 + len(code0)], code0)
        self.assertEqual(dual[:len(code0)], code0)


if __name__ == "__main__":
    unittest.main()
