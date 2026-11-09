"""移动脚本独立验证器的无资源负例；不把合成数据记作原版运行证据。"""
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "static_data"))
from verify_movement_scripts import (
    EXE_SHA256, GETTER_EVIDENCE, PLAYER_EVIDENCE, POINTER_TABLE_VA,
    SCRIPT_IDS, ScriptSource, check_document, check_table, parse_stream, source_scripts,
)


def record(identity, expected):
    return {
        "name": f"k_movement_script_{identity:02x}", "c_type": "uint8_t",
        "dimensions": [len(expected.data)], "values": list(expected.data),
        "source": {
            "file": "FD2.EXE", "file_sha256": EXE_SHA256, "object": 3,
            "canonical_va": hex(expected.canonical_va), "pointer_table_va": hex(POINTER_TABLE_VA),
            "script_id": identity, "length": len(expected.data),
            "sha256": hashlib.sha256(expected.data).hexdigest(),
            "evidence": GETTER_EVIDENCE + " / " + PLAYER_EVIDENCE,
        },
    }


class StaticMovementScriptsTest(unittest.TestCase):
    def setUp(self):
        self.stream = bytes([2, 3, 1, 0, 2, 0x80, 1, 63, 3])
        self.expected = ScriptSource(0x62980, self.stream)
        self.table = record(0, self.expected)

    def test_modes_bounds_and_complete_consumption(self):
        self.assertEqual(parse_stream(self.stream), self.stream)
        self.assertEqual(parse_stream(self.stream + b"other", complete=False), self.stream)
        with self.assertRaisesRegex(ValueError, "尾随"):
            parse_stream(self.stream + b"other")
        check_table(self.table, 0, self.expected)

    def test_truncated_group_and_actor_pairs(self):
        for stop in range(len(self.stream)):
            with self.subTest(stop=stop), self.assertRaises(ValueError):
                parse_stream(self.stream[:stop])

    def test_bad_direction_and_actor_rejected_even_if_hash_updated(self):
        for actor, direction in ((0, 4), (96, 2)):
            table = copy.deepcopy(self.table)
            table["values"][3:5] = [actor, direction]
            table["source"]["sha256"] = hashlib.sha256(bytes(table["values"])).hexdigest()
            with self.subTest(actor=actor, direction=direction), self.assertRaisesRegex(ValueError, "越界"):
                check_table(table, 0, self.expected)

    def test_value_and_forged_provenance_cannot_override_source(self):
        for key, value in (("canonical_va", "0x62981"), ("pointer_table_va", "0x627dc"),
                           ("length", 8), ("file_sha256", "0" * 64), ("sha256", "0" * 64),
                           ("object", 2), ("script_id", 1), ("script_id", False),
                           ("evidence", "field_movement_script_ptr @0x73d5c")):
            table = copy.deepcopy(self.table)
            table["source"][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                check_table(table, 0, self.expected)
        table = copy.deepcopy(self.table)
        table["values"][1] = 4
        table["source"]["sha256"] = hashlib.sha256(bytes(table["values"])).hexdigest()
        with self.assertRaisesRegex(ValueError, "独立fixup源"):
            check_table(table, 0, self.expected)

    def test_type_shape_and_unaccepted_script_ids(self):
        for key, value in (("c_type", "uint16_t"), ("dimensions", [1, 9]),
                           ("values", [True, *self.stream[1:]])):
            table = copy.deepcopy(self.table)
            table[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_table(table, 0, self.expected)
        for identity in (-1, 0x6A, True):
            with self.subTest(identity=identity), self.assertRaises(ValueError):
                check_table(self.table, identity, self.expected)

    def test_fixed_id_coverage_and_duplicates(self):
        expected = {identity: ScriptSource(0x62980 + index * 16, self.stream)
                    for index, identity in enumerate(SCRIPT_IDS)}
        document = {"schema_version": 1, "tables": [record(identity, expected[identity]) for identity in SCRIPT_IDS]}
        check_document(document, expected)
        for mutate in (lambda d: d["tables"].pop(),
                       lambda d: d["tables"].append(copy.deepcopy(d["tables"][0])),
                       lambda d: d["tables"][0]["source"].update(script_id=20),
                       lambda d: d.update(schema_version=True)):
            bad = copy.deepcopy(document)
            mutate(bad)
            with self.assertRaises(ValueError):
                check_document(bad, expected)

    def test_unrecognized_executable_rejected_before_fixup_parse(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "FD2.EXE"
            executable.write_bytes(b"bad executable")
            with self.assertRaisesRegex(ValueError, "身份不符"):
                source_scripts(executable)

    def test_original_capacity_and_raw5_only_zero_mode(self):
        self.assertEqual(parse_stream(bytes([1, 0x80, 1, 95, 5])), bytes([1, 0x80, 1, 95, 5]))
        for data in (bytes([1, 0x80, 1, 96, 5]), bytes([1, 0x81, 1, 1, 5]),
                     bytes([1, 1, 1, 1, 5]), bytes([1, 0x80, 1, 1, 4]),
                     bytes([1, 0x80, 33]) + bytes([0, 0]) * 33):
            with self.subTest(data=data), self.assertRaises(ValueError):
                parse_stream(data)


if __name__ == "__main__":
    unittest.main()
