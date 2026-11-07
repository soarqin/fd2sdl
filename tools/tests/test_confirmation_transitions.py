"""确认框过渡纯模型；不启动窗口、helper、输入或设备。"""
import ast
import copy
import hashlib
import inspect
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_confirmation_transitions as gate
import run_original_battle_menu as menu
import run_original_battle_animation as animation
import run_graphics_tests as suite


def assets():
    palette6 = bytes(v for i in range(256) for v in (i % 64, (i * 3) % 64, (i * 7) % 64))
    palette = [v << 2 for v in palette6]
    frames = []
    for index in range(78):
        image = Image.new("P", (24, 16 if index in (48, 49, 51, 52) else 20), 74)
        image.putpalette(palette)
        image.putpixel((1 + index % 20, 2), 197)
        frames.append(image)
    return {"palette6": palette6, "commands": frames}


def frame(a, *, selected=0, high=False):
    image = Image.new("RGB", (320, 200), (20, 32, 44))
    for item, normal in enumerate((48, 51)):
        source = a["commands"][normal + (high and selected == item)]
        image.paste(source.convert("RGB"), menu.BUTTON_POSITIONS[item])
    return image


class TransitionGateTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.assets = assets()
        source = self.root / "FDOTHER.DAT"
        source.write_bytes(b"source fixture, not original data")
        self.sources = {str(source): hashlib.sha256(source.read_bytes()).hexdigest()}

    def series(self, role, operation, pictures, scale=1):
        directory = self.root / (role + "-" + operation)
        directory.mkdir()
        rows = []
        for index, picture in enumerate(pictures):
            picture = picture.resize((320 * scale, 200 * scale), Image.Resampling.NEAREST)
            path = directory / f"frame{index:05d}.png"
            picture.save(path)
            rows.append({"index": index, "path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                         "pid": 100, "hwnd": "0x123", "wgc_timestamp_100ns": 1000 + index * 20,
                         "client_width": picture.width, "client_height": picture.height})
        return {"role": role, "operation": operation, "pid": 100, "status": "collected", "cleaned": True,
                "forced_cleanup": False, "exit_code": 0,
                "job": {"worker_exit": 0, "timed_out": 0, "residual": 0, "forced": 0, "result": 0, "cleaned": 1},
                "ready": {"ready": True, "pid": 100, "hwnd": "0x123", "first_index": 0, "wgc_timestamp_100ns": 1000},
                "summary": {"ok": True, "pid": 100, "hwnd": "0x123", "backend": "WGC-HWND-session",
                            "session_started": True, "ready_published": True, "stop_reason": "stop", "hresult": 0,
                            "color_filter": False, "frames": len(rows), "first_wgc_timestamp_100ns": 1000,
                            "last_wgc_timestamp_100ns": rows[-1]["wgc_timestamp_100ns"] if rows else None}, "frames": rows}

    def matrix(self, pictures=None):
        pictures = pictures or [frame(self.assets), frame(self.assets, high=True)]
        return [self.series(role, operation, pictures, 1 if role == "original" else 3)
                for role in ("original", "sdl") for operation in gate.OPERATIONS]

    def test_all_eight_raw_series_normalize_nearest_and_minimum_only(self):
        report = gate.verify(self.matrix(), self.assets, self.sources)
        self.assertTrue(report["ok"])
        self.assertEqual(len(report["series"]), 8)
        self.assertEqual([r["pair"]["checked_pixels"] for r in report["paired_operations"]], [26660] * 4)
        self.assertFalse(report["all_transition_frames_equal"])
        self.assertFalse(report["transition_timing_equal"])

    def test_middle_flash_fails_even_with_good_pairs_on_both_sides(self):
        good = frame(self.assets)
        bad = good.copy()
        color = tuple(v << 2 for v in self.assets["palette6"][0xcd * 3:0xcd * 3 + 3])
        bad.paste(color, gate.PANEL)
        report = gate.verify(self.matrix([good, bad, good]), self.assets, self.sources)
        self.assertFalse(report["ok"])
        self.assertTrue(all(row["ok"] for row in report["paired_operations"]))
        self.assertTrue(all(row["frames"][1]["known_flash"] for row in report["series"]))

    def test_one_bad_pixel_in_every_full_panel_cannot_be_masked(self):
        good = frame(self.assets)
        bad = good.copy()
        bad.putpixel((100, 120), (24, 32, 44))  # 非按钮区，仍必须核整板。
        rows = [self.series(role, operation, [good, good] if role == "original" else [bad, bad])
                for role in ("original", "sdl") for operation in gate.OPERATIONS]
        report = gate.verify(rows, self.assets, self.sources)
        self.assertFalse(report["ok"])
        self.assertTrue(report["known_flash_absent"])
        self.assertTrue(all(row["closest"]["bad_pixels"] == 1 for row in report["paired_operations"]))

    def test_matching_browse_background_is_not_confirmation_pair(self):
        blank = Image.new("RGB", (320, 200), (20, 32, 44))
        report = gate.verify(self.matrix([blank, blank]), self.assets, self.sources)
        self.assertFalse(report["ok"])
        self.assertTrue(all(row["pair"] is None for row in report["paired_operations"]))

    def test_missing_duplicate_empty_and_wrong_owner_do_not_pass(self):
        rows = self.matrix()
        for changed in (rows[:-1], rows[:-1] + [rows[0]]):
            with self.assertRaises(ValueError): gate.verify(changed, self.assets, self.sources)
        for mutation in (lambda r: r.update(frames=[]), lambda r: r["summary"].update(hwnd="wrong"),
                         lambda r: r["frames"][1].update(wgc_timestamp_100ns=1000)):
            bad = copy.deepcopy(rows)
            mutation(bad[0])
            with self.assertRaises(ValueError): gate.verify(bad, self.assets, self.sources)

    def test_changed_png_or_resource_source_sha_rejected(self):
        rows = self.matrix()
        path = Path(rows[0]["frames"][0]["path"])
        path.write_bytes(path.read_bytes() + b"changed")
        with self.assertRaisesRegex(ValueError, "PNG源SHA"): gate.verify(rows, self.assets, self.sources)
        source = Path(next(iter(self.sources)))
        source.write_bytes(b"changed source")
        with self.assertRaisesRegex(ValueError, "资源源SHA"): gate.verify(rows, self.assets, self.sources)

    def test_late_error_timeout_or_forced_cleanup_is_not_collection_success(self):
        rows = self.matrix()
        for mutate in (lambda r: r["summary"].update(hresult=-1),
                       lambda r: r["summary"].update(stop_reason="timeout"),
                       lambda r: r.update(forced_cleanup=True),
                       lambda r: r["job"].update(worker_exit=1)):
            bad = copy.deepcopy(rows); mutate(bad[0])
            with self.assertRaises(ValueError): gate.verify(bad, self.assets, self.sources)


class TransitionCollectionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.events = []; self.rows = []
        self.capture = SimpleNamespace(driver=Path("driver.exe"), artifacts=self.root,
                                       process=Mock(pid=123), trace=[])
        self.capture.process.poll.return_value = None
        def key(name):
            self.events.append("key:" + name)
            self.capture.trace.append({"delivery": {"hwnd": "0x123"}})
        self.capture.key = key
        self.oracle = menu.Oracle(assets(), (0, 0))
        self.helper = Mock()
        def owned(*args):
            self.directory = Path(args[1][3]); self.row = args[-1]
            def start(): self.directory.mkdir(); self.events.append("start"); return Mock()
            def finish():
                self.assertTrue((self.directory / "stop").is_file())
                self.events.append("finish")
                return {"ok": True}
            self.helper.start.side_effect = start; self.helper.finish.side_effect = finish
            return self.helper
        self.patches = [patch.object(animation, "OwnedHelper", side_effect=owned),
                        patch.object(animation, "wait_ready", side_effect=lambda *a: self.ready()),
                        patch.object(animation, "series_index", return_value=[{}, {}]),
                        patch.object(menu.time, "sleep"),
                        patch.object(gate, "check_series", return_value={"known_flash_absent": True})]
        for item in self.patches: item.start(); self.addCleanup(item.stop)

    def ready(self):
        self.events.append("ready")
        return {"pid": 123, "hwnd": "0x123"}

    def test_open_ready_before_unique_key_and_stop_before_finish(self):
        menu.capture_transition(self.capture, "original", "escape-open", "ENTER", self.rows, self.oracle)
        self.assertEqual(self.events, ["start", "ready", "key:ENTER", "finish"])
        self.assertEqual(self.rows[0]["status"], "collected")

    def test_cancel_observes_both_fresh_phases_before_unique_key(self):
        a = self.oracle.assets
        samples = [({"index": i}, frame(a, high=bool(i))) for i in (0, 1)]
        with patch.object(animation, "latest_series_frame", side_effect=samples):
            menu.capture_transition(self.capture, "sdl", "escape-cancel", "ESC", self.rows, self.oracle)
        self.assertTrue(self.rows[0]["pre_cancel_phase_ack"])
        self.assertEqual([r["selected_resource_frame"] for r in self.rows[0]["pre_cancel_phase_observations"]], [48, 49])
        self.assertEqual(self.events.count("key:ESC"), 1)

    def test_cancel_missing_phase_stops_before_sending_key(self):
        with patch.object(animation, "latest_series_frame", return_value=None), \
             patch.object(menu.time, "monotonic", side_effect=[0, 0, 2]):
            with self.assertRaises(TimeoutError):
                menu.capture_transition(self.capture, "sdl", "no-cancel", "ENTER", self.rows, self.oracle)
        self.assertFalse(any(event.startswith("key:") for event in self.events))
        self.helper.finish.assert_called_once()

    def test_cleanup_error_is_sticky_and_does_not_replace_first_input_error(self):
        def bad_key(name):
            self.helper.finish.side_effect = RuntimeError("late cleanup")
            raise RuntimeError("first input failure")
        self.capture.key = bad_key
        with self.assertRaisesRegex(RuntimeError, "first input failure"):
            menu.capture_transition(self.capture, "original", "escape-open", "ENTER", self.rows, self.oracle)
        self.assertEqual(self.rows[0]["cleanup_error"], "late cleanup")
        self.assertEqual(self.rows[0]["status"], "failed")

    def test_successful_key_with_late_finish_error_is_failure(self):
        original_key = self.capture.key
        def key(name):
            original_key(name)
            self.helper.finish.side_effect = RuntimeError("finish failed")
        self.capture.key = key
        with self.assertRaisesRegex(RuntimeError, "finish failed"):
            menu.capture_transition(self.capture, "sdl", "escape-open", "ENTER", self.rows, self.oracle)
        self.assertEqual(self.rows[0]["status"], "failed")
        self.assertEqual(self.events.count("key:ENTER"), 1)

    def test_wrong_ready_owner_is_stopped_without_key(self):
        with patch.object(animation, "wait_ready", return_value={"pid": 999, "hwnd": "0x123"}):
            with self.assertRaisesRegex(AssertionError, "owner"):
                menu.capture_transition(self.capture, "original", "escape-open", "ENTER", self.rows, self.oracle)
        self.assertFalse(any(event.startswith("key:") for event in self.events))
        self.helper.finish.assert_called_once()

    def test_default_exercise_unchanged_and_enabled_four_operations_only(self):
        capture = Mock()
        with patch.object(menu, "wait_title"), patch.object(menu, "save_checkpoint"), \
             patch.object(menu, "wait_state", return_value=(Image.new("RGB", (320, 200)), {})), \
             patch.object(menu, "capture_transition") as collect:
            menu.exercise(capture, "sdl", self.oracle, {3: [None, None, None]}, [])
            collect.assert_not_called()
            menu.exercise(capture, "sdl", self.oracle, {3: [None, None, None]}, [], transitions=[])
            self.assertEqual([(c.args[2], c.args[3]) for c in collect.call_args_list],
                             [("escape-open", "ENTER"), ("escape-cancel", "ESC"),
                              ("no-open", "ENTER"), ("no-cancel", "ENTER")])

    def test_suite_real_battle_menu_command_registers_flag(self):
        tree = ast.parse(inspect.getsource(suite))
        calls = [n for n in ast.walk(tree) if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)
                 and n.func.attr == "command" and n.args and isinstance(n.args[0], ast.Constant)
                 and n.args[0].value == "original-battle-menu"]
        self.assertEqual(len(calls), 1)
        self.assertTrue(any(isinstance(n, ast.Constant) and n.value == "--capture-transitions" for n in ast.walk(calls[0])))


if __name__ == "__main__":
    unittest.main()
