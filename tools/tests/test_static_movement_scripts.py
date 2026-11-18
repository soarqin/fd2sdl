"""移动脚本独立验证器的无资源负例；不把合成数据记作原版运行证据。"""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "static_data"))
from verify_movement_scripts import (
    SCRIPT_IDS, ScriptSource, check_document, check_script, parse_stream, source_scripts, unique_keys,
)


class StaticMovementScriptsTest(unittest.TestCase):
    def setUp(self):
        self.stream = bytes([2, 3, 1, 0, 2, 0x80, 1, 63, 3])
        self.expected = ScriptSource(0x62980, self.stream)
        self.values = list(self.stream)

    def test_modes_bounds_and_complete_consumption(self):
        self.assertEqual(parse_stream(self.stream), self.stream)
        self.assertEqual(parse_stream(self.stream + b"other", complete=False), self.stream)
        with self.assertRaisesRegex(ValueError, "尾随"):
            parse_stream(self.stream + b"other")
        check_script(self.values, 0, self.expected)

    def test_truncated_group_and_actor_pairs(self):
        for stop in range(len(self.stream)):
            with self.subTest(stop=stop), self.assertRaises(ValueError):
                parse_stream(self.stream[:stop])

    def test_bad_direction_and_actor_rejected(self):
        for actor, direction in ((0, 4), (96, 2)):
            values = self.values.copy()
            values[3:5] = [actor, direction]
            with self.subTest(actor=actor, direction=direction), self.assertRaisesRegex(ValueError, "越界"):
                check_script(values, 0, self.expected)

    def test_well_formed_modification_differs_from_original(self):
        values = self.values.copy()
        values[1] = 4
        with self.assertRaisesRegex(ValueError, "独立fixup源"):
            check_script(values, 0, self.expected)

    def test_type_shape_and_unaccepted_script_ids(self):
        for values in (self.stream, [True, *self.stream[1:]], [], [self.values]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                check_script(values, 0, self.expected)
        for identity in (-1, 0x6A, True):
            with self.subTest(identity=identity), self.assertRaises(ValueError):
                check_script(self.values, identity, self.expected)

    def test_fixed_id_coverage_and_duplicates(self):
        expected = {identity: ScriptSource(0x62980 + index * 16, self.stream)
                    for index, identity in enumerate(SCRIPT_IDS)}
        document = {"schema_version": 2, "scripts": {str(identity): self.values for identity in SCRIPT_IDS}}
        check_document(document, expected)
        for mutate in (lambda d: d["scripts"].pop("0"),
                       lambda d: d["scripts"].update({"106": self.values}),
                       lambda d: d["scripts"].update({"00": d["scripts"].pop("0")}),
                       lambda d: d.update(schema_version=True)):
            bad = copy.deepcopy(document)
            mutate(bad)
            with self.assertRaises(ValueError):
                check_document(bad, expected)
        with self.assertRaisesRegex(ValueError, "重复JSON键"):
            json.loads('{"scripts":{"0":[0],"0":[0]}}', object_pairs_hook=unique_keys)

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
