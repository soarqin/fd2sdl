"""SD1 独立来源校验的损坏输入回归，不需要原版资源。"""
import copy
import hashlib
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "static_data"))
from verify_unit_tables import EXE_SHA256, check_table, source_range


class StaticUnitTablesTest(unittest.TestCase):
    def setUp(self):
        self.spec = ("fixture", "uint8_t", (1, 3), 0x619FD)
        self.expected = bytes([0, 255, 23])
        self.table = {
            "name": "fixture", "c_type": "uint8_t", "dimensions": [1, 3],
            "values": [[0, 255, 23]],
            "source": {
                "file": "FD2.EXE", "file_sha256": EXE_SHA256,
                "object": 3, "canonical_va": "0x619fd", "length": 3,
                "sha256": hashlib.sha256(self.expected).hexdigest(),
                "view": "raw-unrelocated", "evidence": "synthetic fixture",
            },
        }

    def test_complete_bytes_and_identity(self):
        check_table(self.table, self.spec, self.expected)

    def test_mutated_unknown_byte_cannot_change_expected(self):
        table = copy.deepcopy(self.table)
        table["values"][0][1] = 0
        table["source"]["sha256"] = hashlib.sha256(bytes([0, 0, 23])).hexdigest()
        with self.assertRaisesRegex(ValueError, "未知字节"):
            check_table(table, self.spec, self.expected)

    def test_types_shapes_ranges_and_source_rejected(self):
        mutations = (
            ("c_type", "uint32_t"), ("dimensions", [3]),
            ("values", [[0, 255]]), ("values", [[0, True, 23]]),
            ("values", [[0, 256, 23]]), ("values", [[0, -1, 23]]),
            ("source", {}),
        )
        for key, value in mutations:
            with self.subTest(key=key, value=value):
                table = copy.deepcopy(self.table)
                table[key] = value
                with self.assertRaises(ValueError):
                    check_table(table, self.spec, self.expected)

    def test_u32_uses_little_endian_source_bytes(self):
        table = copy.deepcopy(self.table)
        table.update(c_type="uint32_t", dimensions=[1], values=[0x12345678])
        expected = bytes.fromhex("78 56 34 12")
        table["source"].update(length=4, sha256=hashlib.sha256(expected).hexdigest())
        check_table(table, ("fixture", "uint32_t", (1,), 0x619FD), expected)

    def test_different_executable_is_rejected_before_parsing(self):
        with self.assertRaisesRegex(ValueError, "身份不符"):
            source_range(b"LE damaged or different version", 0x619FD, 3)


if __name__ == "__main__":
    unittest.main()
