"""SHORT native协议纯测；不启动WGC、游戏或driver。"""
from contextlib import contextmanager
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, call, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_reward as runner
from run_original_item_list import derive_active_fixture
from run_graphics_tests import decode_save

# 候选可在新目录运行，普通提交后默认仍用项目root。
import os
ROOT = Path(os.environ.get("FD2_TEST_ROOT", Path(__file__).resolve().parents[2]))
ORIGINAL = (ROOT / "original_game").resolve()


class ShortCounterNativeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parent = runner.derive_fixture(ORIGINAL, "counter-multi")
        cls.prepared = runner.prepare(ORIGINAL, "counter-multi", short_counter=True)

    def test_narrow_option_and_checksum_manifest_are_complete_at_both_layers(self):
        encoded, source, manifest = self.parent
        actual = self.prepared
        changed = actual["source"]
        self.assertEqual({i for i, (a, b) in enumerate(zip(source, changed)) if a != b}, {12497, 22983})
        self.assertEqual((source[12497], changed[12497]), (0, 1))
        self.assertEqual(int.from_bytes(changed[-4:], "little"), sum(changed[:-4]))
        self.assertEqual(bytes(a ^ b ^ c for a, b, c in zip(encoded, source, changed)), actual["encoded"])
        for old, new, rows in ((source, changed, actual["manifest"]["short_counter"]["plaintext_patches"]),
                (encoded, actual["encoded"], actual["manifest"]["short_counter"]["encrypted_patches"])):
            rebuilt = bytearray(old)
            for row in rows:
                self.assertEqual(rebuilt[row["offset"]], row["before"])
                rebuilt[row["offset"]] = row["after"]
            self.assertEqual(bytes(rebuilt), new)
        start_cipher = (ORIGINAL / "START.SAV").read_bytes()
        start_plain = decode_save(ORIGINAL / "START.SAV")
        for old, new, key in ((start_cipher, actual["encoded"], "encrypted_patches"),
                (start_plain, changed, "plaintext_patches")):
            expected = [{"offset": i, "before": a, "after": b} for i, (a, b) in enumerate(zip(old, new)) if a != b]
            self.assertEqual(actual["manifest"][key], expected)

    def test_shared_active_gate_still_rejects_option_and_bad_flag_has_no_io(self):
        plain, encoded = decode_save(ORIGINAL / "START.SAV"), (ORIGINAL / "START.SAV").read_bytes()
        with self.assertRaises(ValueError):
            derive_active_fixture(plain, encoded, "not-active", [(12497, b"\1")])
        with patch.object(runner, "derive_fixture") as derive:
            for scenario in set(runner.SCENARIOS)-{"counter-multi"}:
                with self.assertRaises(ValueError): runner.prepare(ORIGINAL, scenario, short_counter=True)
            derive.assert_not_called()

    def test_repeated_option_bad_checksum_or_parent_manifest_are_not_accepted(self):
        encoded, source, manifest = self.parent
        for offset, value in ((12497, 1), (22983, source[22983]^1)):
            bad = bytearray(source); bad[offset] = value
            with self.assertRaises(ValueError): runner.short_counter_fixture(encoded, bytes(bad), manifest, "counter-multi")
        bad = dict(manifest, source_plain_sha256="0"*64)
        with self.assertRaises(ValueError): runner.short_counter_fixture(encoded, source, bad, "counter-multi")

    def test_original_layout_and_damage_constants_are_not_old_fixture_coordinates(self):
        proof = self.prepared["short_counter"]
        self.assertEqual(proof["camera"], [5, 11])
        self.assertEqual([(b["raw_actor"], b["direction"], b["position"], b["width"], b["hp"])
                         for b in proof["bars"]], [(0, 3, (87, 100), 63, 38), (11, 1, (111, 60), 3, 1)])
        # 31资源页、30个显式-3 ACK；经验页不多投一键。
        self.assertEqual((len(self.prepared["pages"]), sum(p["requires_key"] for p in self.prepared["pages"])), (31, 30))

    def test_all_355_pixels_per_bar_are_source_raw_and_exact_DAC(self):
        # 独立已知原RAW列：23左帽199、24内列195/197/199、25右帽195，29/30尾37/35。
        # 不调用被测bar模板构造；逐row/col固定资源公式建立完整expected。
        data = (ORIGINAL / "FDOTHER.DAT").read_bytes()
        palette = runner.archive_entry(data, 0)
        frame = Image.new("RGB", (320, 200), (17, 18, 19))
        for at, width in (((87, 100), 63), ((111, 60), 3)):
            for row in range(5):
                for col in range(71):
                    color = 199 if col == 0 else (195 if row == 0 else 199 if row == 4 else 197) if col < width else 195 if col == width else 37 if col == 70 or row == 0 else 35
                    rgb = tuple((n << 2) | (n >> 4) for n in palette[3*color:3*color+3])
                    frame.putpixel((at[0]+col, at[1]+row), rgb)
        found = runner.short_counter_feedback_state(frame, self.prepared)
        self.assertIsNotNone(found)
        self.assertEqual(found["RGB_tolerance"], 0)
        self.assertEqual(found["normalized_full_frame_RGB_sha256"], hashlib.sha256(frame.tobytes()).hexdigest())
        for x, y in ((87, 100), (157, 104), (111, 60), (181, 64), (149, 102), (115, 62)):
            bad = frame.copy(); rgb = bad.getpixel((x, y)); bad.putpixel((x, y), (rgb[0]^1, *rgb[1:]))
            self.assertIsNone(runner.short_counter_feedback_state(bad, self.prepared))
        self.assertIsNone(runner.short_counter_feedback_state(frame.resize((640, 400)), self.prepared))

    def test_source_App_to_VGA_destination_origin_is_part_of_the_affine_map(self):
        # 1e611@1e718..1e734传App+8088h/456/312/192；11d2c再推
        # VGA+A0504h/320。8088h=72*456+72，0504h=4*320+4。
        source_y, source_x = divmod(0x8088, 456)
        destination_y, destination_x = divmod(0x0504, 320)
        self.assertEqual((source_x, source_y, destination_x, destination_y), (72, 72, 4, 4))
        for bar, descriptor in zip(self.prepared["short_counter"]["bars"], ((80, 94), (104, 54))):
            app_x, app_y = descriptor[0]+75, descriptor[1]+74
            expected = (destination_x+app_x-source_x, destination_y+app_y-source_y)
            self.assertEqual(bar["position"], expected)

    def test_foreign_bar_geometry_cannot_pass(self):
        frame = Image.new("RGB", (320, 200))
        for bar in self.prepared["short_counter"]["bars"]:
            x, y = bar["position"]; frame.paste(bar["image"], (x+1, y))
        self.assertIsNone(runner.short_counter_feedback_state(frame, self.prepared))

    def observer(self, failure=None):
        import run_g4_detail_check as continuous
        trace = []
        capture = SimpleNamespace(key=lambda name: trace.append(("key", name)))
        report = {"checkpoints": []}
        @contextmanager
        def series(c, record):
            trace.append("ready")
            try: yield {"scope": True}
            finally: trace.append("stopped")
        def checkpoint(c, scope, name, predicate, points, timeout):
            trace.append(("checkpoint", name, timeout))
            if failure: raise TimeoutError(failure)
            points.append({"name": name, "model_only": True})
        with patch.object(continuous, "detail_series", side_effect=series), \
                patch.object(continuous, "series_checkpoint", side_effect=checkpoint):
            if failure:
                with self.assertRaisesRegex(TimeoutError, failure): runner.observe_short_counter(capture, self.prepared, report)
            else: runner.observe_short_counter(capture, self.prepared, report)
        return trace, report

    def test_only_END_confirmation_is_sent_after_series_ready_and_stop_precedes_return(self):
        trace, report = self.observer()
        self.assertEqual(trace, ["ready", ("key", "ENTER"), ("checkpoint", "b4-short-counter-bars", 40), "stopped"])
        self.assertTrue(report["short_counter_feedback"]["passed"])
        self.assertEqual([p["name"] for p in report["checkpoints"]], ["b4-short-counter-bars"])

    def test_missing_feedback_stops_series_without_extra_key_or_false_pass(self):
        trace, report = self.observer("required bars missing")
        self.assertEqual(trace[-1], "stopped")
        self.assertEqual(sum(isinstance(row, tuple) and row[0] == "key" for row in trace), 1)
        self.assertNotIn("passed", report["short_counter_feedback"])

    def test_short_exercise_has_one_Save_no_WAIT_Load_or_detail_mutation(self):
        capture = SimpleNamespace(key=Mock(), wait=Mock())
        report = {"kind": "original", "checkpoints": []}
        with patch.object(runner, "wait_title"), patch.object(runner, "wait_state", return_value=(None, {})), \
                patch.object(runner, "require_source_unchanged"), patch.object(runner, "navigate"), \
                patch.object(runner, "end_turn") as end, patch.object(runner, "save_measure", return_value=self.prepared["model"]["final"]) as save, \
                patch.object(runner, "check_active_load_measurement", return_value={"cache_classes_original_10010_verified": True}) as strict, \
                patch.object(runner, "show_detail") as detail, patch.object(runner, "observe_active_load") as load:
            runner.exercise(capture, self.prepared, Path("unused"), [b"", 0], report)
        end.assert_called_once()
        save.assert_called_once()
        self.assertEqual(save.call_args.args[-1], "b4-short-counter")
        self.assertEqual(strict.call_args.args[0], self.prepared["model"]["final"])
        strict.assert_called_once()
        detail.assert_not_called(); load.assert_not_called()
        self.assertEqual(report["save_protocol"]["save_count"], 1)
        self.assertEqual((report["save_protocol"]["wait_action_count"], report["save_protocol"]["load_count"]), (0, 0))
        self.assertNotIn("save_load", report)

    def test_default_prepare_has_no_option_or_short_protocol(self):
        ordinary = runner.prepare(ORIGINAL, "counter-multi")
        self.assertEqual(ordinary["source"], self.parent[1])
        self.assertNotIn("short_counter", ordinary)
        self.assertNotIn("short_counter", ordinary["manifest"])

    def test_pair_single_Save_diff_fails_without_old_RGB_tolerance(self):
        scope = "one END, resource ACKs, turn2 and one formal Save; no WAIT/Load claim"
        with tempfile.TemporaryDirectory() as temp:
            paths = [Path(temp)/name for name in ("original.SAV", "sdl.SAV")]
            for p in paths: p.write_bytes(self.prepared["encoded"])
            cases = [{"kind": kind, "probe_cases_passed": True,
                      "short_counter_feedback": {"passed": True},
                      "short_counter_full_save": {"cache_classes_original_10010_verified": True},
                      "reward_acknowledgements": [{"fragment": p["fragment"], "key_sent": "ENTER" if p["requires_key"] else None} for p in self.prepared["pages"]],
                      "save_protocol": {"save_count": 1, "wait_action_count": 0, "load_count": 0,
                                        "whole_container_validated": True, "scope": scope},
                      "completed_save": {"snapshot": str(path)}}
                     for kind, path in zip(("original", "sdl"), paths)]
            with patch.object(runner, "rgb_difference", side_effect=AssertionError("old tolerance path")):
                result = runner.compare_endpoints(cases, Path(temp), self.prepared)
                self.assertEqual(result["RGB_tolerance"], 0)
                self.assertFalse(result["WAIT_or_Load_tested"])
                different = bytearray(paths[1].read_bytes()); different[-1] ^= 1; paths[1].write_bytes(different)
                with self.assertRaisesRegex(AssertionError, "全部byte不同"):
                    runner.compare_endpoints(cases, Path(temp), self.prepared)
            cases[0]["short_counter_feedback"].pop("passed")
            with self.assertRaisesRegex(AssertionError, "反馈或单Save"):
                runner.compare_endpoints(cases, Path(temp), self.prepared)

    def test_complete_single_save_has_source_derived_classes_no_measured_byte_exception(self):
        from run_original_load import active_load_cache_classes, measuring_focus
        source = self.prepared["model"]["final"]
        steps = [("LEFT", runner.screen(source, (6, 14)))]
        measured = bytearray(source)
        measured[runner.META+5:runner.META+9] = measuring_focus(source, steps)
        for raw, value in enumerate(active_load_cache_classes(source)):
            measured[runner.UNITS+raw*runner.STRIDE+2] = value
        measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
        result = runner.check_active_load_measurement(source, bytes(measured), steps)
        self.assertTrue(result["cache_classes_original_10010_verified"])
        self.assertNotIn("cache_class_changes_excluded", result)
        bad = bytearray(measured); bad[runner.UNITS+11*runner.STRIDE+2] ^= 1
        bad[-4:] = sum(bad[:-4]).to_bytes(4, "little")
        with self.assertRaisesRegex(AssertionError, "cache class differs"):
            runner.check_active_load_measurement(source, bytes(bad), steps)


if __name__ == "__main__": unittest.main()
