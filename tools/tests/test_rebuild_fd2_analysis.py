#!/usr/bin/env python3
"""Regression tests for FD2 bound LE page and fixup handling."""

import struct
import sys
from dataclasses import replace
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from rebuild_fd2_analysis import (  # noqa: E402
    LEHeader,
    ObjectRecord,
    FixupRecord,
    apply_fixups,
    build_code0_image,
    build_dual_clean_image,
    build_relbase_image,
    extract_page,
    parse_fixups,
    parse_le,
    parse_objects,
)
from validate_le_fixups import validate  # noqa: E402


class FixupValidationTest(unittest.TestCase):
    def test_real_fd2_fixup_table_and_bound_pages(self) -> None:
        exe = Path(__file__).resolve().parents[2] / "original_game" / "FD2.EXE"
        if not exe.exists():
            self.skipTest("original_game/FD2.EXE is developer-local")
        data = exe.read_bytes()
        le = parse_le(data)
        objects = parse_objects(data, le)
        self.assertEqual(le.le_off, 0x27ACC)
        self.assertEqual(le.module_file_off, 0x25214)
        self.assertEqual(le.module_file_off + le.data_pages_off, 0x36014)
        raw = bytes(build_relbase_image(data, le, objects))
        self.assertEqual(raw[objects[0].relbase:objects[0].relbase + 4], bytes.fromhex("ccebfd90"))
        expected_magic = (
            10, 10, 10, 10, 7, 7, 10, 10, 10, 10, 9, 10, 5, 5,
            8, 10, 6, 8, 10, 9, 5, 5, 10, 8, 8, 4, 10, 7)
        self.assertEqual(
            struct.unpack_from("<28I", raw, objects[1].relbase + 0x1F96),
            expected_magic)
        fixups = parse_fixups(data, le, objects)
        self.assertEqual(len(fixups), 7959)
        self.assertEqual(len({r.source_addr_relbase for r in fixups}), 7948)
        relocated = apply_fixups(raw, fixups)
        for record in fixups:
            self.assertEqual(
                struct.unpack_from("<I", relocated, record.source_addr_relbase)[0],
                record.target_linear)

    def test_independent_decoder_matches_primary(self) -> None:
        exe = Path(__file__).resolve().parents[2] / "original_game" / "FD2.EXE"
        if not exe.exists():
            self.skipTest("original_game/FD2.EXE is developer-local")
        total, errors, counts = validate(exe)
        self.assertEqual(total, 7959)
        self.assertEqual(errors, [])
        self.assertEqual(counts[(0x07, 0x00)], 7067)
        self.assertEqual(counts[(0x07, 0x10)], 892)
        self.assertEqual(validate.last_details["unique_sources"], 7948)
        self.assertEqual(validate.last_details["duplicate_records"], 11)


class MalformedFixupTest(unittest.TestCase):
    @staticmethod
    def fixture(records: bytes):
        data = struct.pack("<2I", 0, len(records)) + records
        header = LEHeader(
            le_off=0, num_pages=1, page_size=16, last_page_size=0,
            objtab_off=0, objcnt=1, objmap_off=0,
            fixup_page_table_off=0, fixup_record_table_off=8,
            data_pages_off=0, entry_object=1, entry_eip=0,
            module_file_off=0,
        )
        return data, header, [ObjectRecord(1, 32, 0, 0, 1, 1)]

    def test_truncated_record_is_not_silently_dropped(self):
        record = bytes.fromhex("07000000010400")
        for length in range(1, len(record)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                parse_fixups(*self.fixture(record[:length]))
        with self.assertRaises(ValueError):
            parse_fixups(*self.fixture(record + b"\x07"))

    def test_invalid_page_boundaries_are_rejected(self):
        record = bytes.fromhex("07000000010400")
        data, header, objects = self.fixture(record)
        for start, end in ((1, len(record)), (len(record), 0), (0, len(record) + 1)):
            with self.subTest(start=start, end=end), self.assertRaises(ValueError):
                parse_fixups(struct.pack("<2I", start, end) + record, header, objects)
        with self.assertRaises(ValueError):
            parse_fixups(data[:4], header, objects)

    def test_overlapping_and_out_of_bounds_writes_are_rejected(self):
        record = FixupRecord(0, 0, 0, 0, 7, 0, 1, 4, 4)
        raw = bytes(8)
        self.assertEqual(apply_fixups(raw, [record, record])[:4], b"\x04\x00\x00\x00")
        for other in (replace(record, target_linear=5),
                      replace(record, source_addr_relbase=1),
                      replace(record, source_addr_relbase=-1),
                      replace(record, source_addr_relbase=6)):
            with self.subTest(other=other), self.assertRaises(ValueError):
                apply_fixups(raw, [record, other])


class DataPageOffsetTest(unittest.TestCase):
    def test_data_page_offset_is_module_relative(self) -> None:
        data = bytearray(0x50)
        data[0x10:0x14] = b"BAD!"
        data[0x30:0x34] = b"PAGE"
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
            module_file_off=0x20,
        )
        self.assertEqual(extract_page(bytes(data), header, 1, True), b"PAGE")

    def test_truncated_data_page_is_rejected(self) -> None:
        _, header, _ = MalformedFixupTest.fixture(b"")
        with self.assertRaises(ValueError):
            extract_page(b"short", header, 1, False)

    def test_code0_contains_only_object1_pages(self) -> None:
        data = bytearray(range(0x40))
        data[0x08:0x0c] = bytes.fromhex("00000100")
        data[0x0c:0x10] = bytes.fromhex("00000200")
        header = LEHeader(
            le_off=0, num_pages=2, page_size=4, last_page_size=0,
            objtab_off=0, objcnt=2, objmap_off=8,
            fixup_page_table_off=0, fixup_record_table_off=0,
            data_pages_off=0x10, entry_object=1, entry_eip=0,
            module_file_off=0,
        )
        objects = [
            ObjectRecord(1, 4, 4, 0, 1, 1),
            ObjectRecord(2, 4, 8, 0, 2, 1),
        ]
        code0 = build_code0_image(bytes(data), header, objects)
        self.assertEqual(code0, data[0x10:0x14])
        dual = build_dual_clean_image(code0)
        self.assertEqual(dual[0x10000:0x10000 + len(code0)], code0)
        self.assertEqual(dual[:len(code0)], code0)


if __name__ == "__main__":
    unittest.main()
