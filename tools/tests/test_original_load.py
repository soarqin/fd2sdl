"""Load恢复的字段allowlist、观察边界和真实导航序列模型；无窗口无子进程。"""
from pathlib import Path
import os
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_load as load


def checksum(data):
    data[-4:] = sum(data[:-4]).to_bytes(4, "little")
    return bytes(data)


def source_snapshot():
    data = bytearray((index * 31 + 7) % 256 for index in range(0x59cb))
    data[load.META:load.META + 18] = bytes((1, 12, 0, 5, 11, 8, 15, 3, 4, 4, 0, 0, 0, 0, 0, 1, 1, 1))
    return checksum(data)


class LoadMeasurementTest(unittest.TestCase):
    def setUp(self):
        self.source = source_snapshot()
        self.steps = (("LEFT", (52, 100)),)
        data = bytearray(self.source)
        data[load.META + 5:load.META + 9] = bytes((7, 15, 2, 4))
        data[load.UNITS + 2] ^= 7
        self.measured = checksum(data)

    def test_only_declared_measurement_focus_and_active_cache_class_may_differ(self):
        report = load.check_load_measurement(self.source, self.measured, self.steps)
        self.assertEqual((report["turn"], report["stage"], report["active_count"], report["roster_count"]),
                         (1, 0, 12, 4))
        self.assertEqual(report["source_camera_focus"], [5, 11, 8, 15, 3, 4])
        self.assertEqual(report["measurement_camera_focus"], [5, 11, 7, 15, 2, 4])
        self.assertEqual(len(report["cache_class_changes_excluded"]), 1)
        self.assertTrue(report["source_equal_except_declared_cache_and_measuring_focus"])

    def test_no_unconfirmed_masks_for_roster_active_or_other_container_bytes(self):
        for offset in (0, 0x8a3, 0x8a3 + 4 * 80, load.UNITS, load.UNITS + 3,
                       load.UNITS + 4, load.UNITS + 5, load.UNITS + 0x0a,
                       load.UNITS + 0x1a, load.UNITS + 0x25, load.UNITS + 0x40,
                       load.UNITS + 0x44, load.UNITS + 12 * 80 + 2,
                       0x30a3, load.META, load.META + 3, load.META + 9,
                       load.META + 10, load.META + 16, load.META + 18, 0x312b):
            with self.subTest(offset=offset):
                bad = bytearray(self.measured)
                bad[offset] ^= 1
                with self.assertRaisesRegex(AssertionError, "outside the declared"):
                    load.check_load_measurement(self.source, checksum(bad), self.steps)

    def test_source_focus_is_kept_without_measuring_navigation(self):
        report = load.check_load_measurement(self.source, self.source, ())
        self.assertEqual(report["measurement_camera_focus"], [5, 11, 8, 15, 3, 4])
        with self.assertRaises(ValueError):
            load.measuring_focus(self.source, (("LEFT", (53, 100)),))

    def test_invalid_checksum_or_size_cannot_be_masked_as_runtime_difference(self):
        bad = bytearray(self.measured)
        bad[load.UNITS + 2] ^= 1
        for data in (bad, self.measured[:-1]):
            with self.subTest(length=len(data)), self.assertRaisesRegex(AssertionError, "checksum"):
                load.check_load_measurement(self.source, data, self.steps)

    def test_measurement_save_may_rewrite_identical_bytes_but_must_have_new_mtime(self):
        save = Mock()
        save.read_bytes.return_value = b"same encrypted bytes"
        save.stat.return_value = SimpleNamespace(st_mtime_ns=2)
        with patch.object(load, "decode_save", return_value=self.source) as decode:
            self.assertEqual(load.wait_measured_save(save, b"same encrypted bytes", 1), self.source)
        decode.assert_called_once_with(save)

    def test_load_source_gate_rejects_any_write_before_measurement(self):
        save = Mock()
        save.read_bytes.return_value = b"written during Load"
        with self.assertRaisesRegex(AssertionError, "before measurement"):
            load.require_source_unchanged(save, b"source A", 1)

    def test_identical_source_bytes_with_new_mtime_are_rejected_before_measurement(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"identical source A")
            original_stat = save.stat()
            os.utime(save, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns + 1_000_000_000))
            self.assertEqual(save.read_bytes(), b"identical source A")
            self.assertNotEqual(save.stat().st_mtime_ns, original_stat.st_mtime_ns)
            with self.assertRaisesRegex(AssertionError, "rewrote source save mtime"):
                load.require_source_unchanged(save, b"identical source A", original_stat.st_mtime_ns)


class LoadObservationTest(unittest.TestCase):
    def setUp(self):
        self.image = Image.new("RGB", (320, 200), (30, 60, 90))

    def test_source_initial_cursor_alone_is_not_enough_while_dialog_still_covers_battle(self):
        oracle = Mock()
        oracle.initial_browse.return_value = {"kind": "initial_browse", "visible_cursor_rows": 21}
        border = Image.new("P", (310, 2), 80)
        border.putpalette([channel for value in range(256) for channel in (value, value, value)])
        oracle.assets = {"dialog_top": border}
        covered = self.image.copy()
        covered.paste((80, 80, 80), (5, 112, 315, 114))
        self.assertIsNone(load.restored_source(covered, oracle))
        result = load.restored_source(self.image, oracle)
        self.assertEqual(result["visible_cursor_rows"], 21)
        self.assertTrue(result["dialog_top_resource_absent"])
        self.assertEqual(oracle.initial_browse.call_count, 2)
        oracle.browse.assert_not_called()

    def test_round2_cancel_cancel_load_then_explicit_measurement_save(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"source A")
            capture = Mock()
            capture.process.poll.return_value = None
            observation = {}
            source_mtime = save.stat().st_mtime_ns
            with patch.object(load, "wait_title") as startup, \
                    patch.object(load, "wait_state", return_value=(self.image, {"focus_screen": [148, 28]})) as state, \
                    patch.object(load, "wait_measured_save", return_value=b"measured plaintext") as measure, \
                    patch.object(load, "require_source_unchanged", wraps=load.require_source_unchanged) as guard:
                measured = load.exercise(capture, "original", {}, (76, 100), (("LEFT", (52, 100)),),
                                         {3: [self.image] * 3}, self.image, save, b"source A", Mock(), [], observation,
                                         source_mtime=source_mtime)
            self.assertEqual(measured, b"measured plaintext")
            self.assertTrue(observation["round2_reached_without_save"])
            self.assertTrue(observation["load_disk_unchanged_before_measurement"])
            self.assertTrue(observation["explicit_measurement_save_completed"])
            self.assertEqual(observation["source_mtime_ns_before_measurement"], source_mtime)
            self.assertEqual(guard.call_count, 5)
            self.assertTrue(all(call.args[2] == source_mtime for call in guard.call_args_list))
            self.assertEqual([entry["branch"] for entry in observation["cancellations"]], ["escape", "no"])
            self.assertTrue(all(not entry["full_runtime_fields_measured"] for entry in observation["cancellations"]))
            open_load = ["ENTER", "UP", "ENTER", "RIGHT", "ENTER"]
            expected = (["UP", "ENTER", "LEFT", "ENTER", "DOWN", "ENTER", "ENTER", "LEFT"] +
                        open_load + ["ESC"] + open_load + ["RIGHT", "ENTER"] + open_load +
                        ["ENTER", "LEFT", "ENTER", "UP", "ENTER", "LEFT", "ENTER", "ENTER"])
            self.assertEqual([call.args[0] for call in capture.key.call_args_list], expected)
            names = [call.args[1] for call in state.call_args_list]
            self.assertLess(names.index("original-loaded-source-initial-browse"),
                            names.index("original-measurement-empty-step-1"))
            leader_gate = next(call.args[2] for call in state.call_args_list if call.args[1] == "original-round2-leader")
            with patch.object(load, "browse_anywhere", return_value=None) as browse:
                leader_gate(self.image)
            browse.assert_called_once_with(self.image, {}, leader=True)
            measure.assert_called_once()
            startup.assert_called_once()
            capture.wait.assert_called_once()  # Load之后不会再调用startup wait或补Esc。

    def test_missing_original_round2_banner_stops_before_load_or_save(self):
        capture = Mock()
        def wait(capture, name, predicate, checkpoints, **kwargs):
            if name.endswith("round-002"):
                raise TimeoutError("round 2 not observed")
            return self.image, {}
        with patch.object(load, "wait_title"), patch.object(load, "wait_state", side_effect=wait), \
                patch.object(load, "wait_measured_save") as measure:
            with self.assertRaisesRegex(TimeoutError, "not observed"):
                load.exercise(capture, "original", {}, (76, 100), (), {3: [self.image] * 3},
                              self.image, Mock(), b"source A", Mock(), [], {}, source_mtime=1)
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                         ["UP", "ENTER", "ENTER", "DOWN", "ENTER", "ENTER"])
        measure.assert_not_called()


if __name__ == "__main__":
    unittest.main()
