from contextlib import contextmanager
import importlib.util
from types import SimpleNamespace
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from PIL import Image

HERE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("original_player", HERE / "run_original_player.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class PlayerOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = probe.ROOT / "original_game"
        cls.source, cls.focus, cls.assets, cls.composites, cls.ranges, cls.plan = probe.prepare(
            cls.original, cls.original / "START.SAV")

    def test_real_start_plan_derives_leader_and_legal_tile_without_sample_coordinates(self):
        self.assertEqual(self.plan["leader_world"], list(self.source[probe.UNITS:probe.UNITS + 2]))
        leader, destination = self.plan["leader_world"], self.plan["destination_world"]
        self.assertEqual(sum(abs(a - b) for a, b in zip(leader, destination)), 1)
        self.assertGreater(self.plan["cost"], 0)
        self.assertLessEqual(self.plan["cost"], self.source[probe.UNITS + 0x3b])
        x, y = self.source[probe.META + 5:probe.META + 7]
        for key, cell in self.plan["navigation"]:
            dx, dy = next((dx, dy) for name, dx, dy in probe.DIRECTIONS if name == key)
            self.assertEqual(cell, (x + dx, y + dy))
            x, y = cell
        self.assertEqual([x, y], leader)

    def test_navigation_uses_supplied_source_and_rejects_unreachable_scroll(self):
        source = bytearray(self.source)
        source[probe.META + 3:probe.META + 7] = bytes((2, 7, 7, 11))
        path = probe.navigation(source, (8, 11))
        self.assertEqual(path, [("RIGHT", (8, 11))])
        with self.assertRaises(ValueError):
            probe.navigation(source, (23, 23))

    def frame(self, *, range_tile=False, moved_focus=False):
        frame = Image.new("RGB", (320, 200))
        if not moved_focus:
            leader = self.assets["leader_cursors"][0]
            x, y = self.plan["leader_screen"]
            frame.paste(leader.convert("RGB"), (x, y - 6), leader.point(lambda value: 255 if value else 0, mode="L"))
        if range_tile:
            tile = self.ranges[0][1].copy()
            if moved_focus:
                cursor = self.assets["cursor"]
                tile.paste(cursor, (0, 0), cursor.point(lambda value: 255 if value else 0, mode="L"))
            frame.paste(tile.convert("RGB"), tuple(self.plan["destination_screen"]))
        return frame

    def test_selection_requires_real_range_translation_and_expected_cursor(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        self.assertIsNone(oracle.selection(self.frame()))
        self.assertIsNotNone(oracle.selection(self.frame(range_tile=True)))
        self.assertIsNone(oracle.selection(self.frame(range_tile=True), moved_focus=True))
        self.assertIsNotNone(oracle.selection(self.frame(range_tile=True, moved_focus=True), moved_focus=True))

    def test_browse_requires_untranslated_tile_not_only_visible_leader(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        self.assertIsNone(oracle.browse(self.frame(range_tile=True)))
        frame = self.frame()
        frame.paste(self.assets["probe_base_tile"].convert("RGB"), tuple(self.plan["destination_screen"]))
        self.assertIsNotNone(oracle.browse(frame))

    def test_command_requires_nonempty_resource_pixels_in_all_four_positions(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        frame = Image.new("RGB", (320, 200))
        self.assertIsNone(oracle.command(frame, tuple(self.plan["leader_screen"])))
        for item in range(4):
            image = self.assets["commands"][item * 3 + (1 if item == 3 else 0)]
            at = tuple(a + b for a, b in zip(self.plan["leader_screen"], probe.MENU_OFFSETS[item]))
            frame.paste(image.convert("RGB"), at, image.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNotNone(oracle.command(frame, tuple(self.plan["leader_screen"]), 3))

    def test_command_top_actor_overlap_does_not_relax_other_resource_pixels(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        position = tuple(self.plan["leader_screen"])
        frame = Image.new("RGB", (320, 200))
        for item, index in enumerate((2, 5, 7, 9)):
            image = self.assets["commands"][index]
            at = tuple(a + b for a, b in zip(position, probe.MENU_OFFSETS[item]))
            frame.paste(image.convert("RGB"), at, image.point(lambda value: 255 if value else 0, mode="L"))
        x, y = position[0], position[1] - 20
        frame.paste((251, 1, 127), (x, y + 14, x + 24, y + 20))
        detail = oracle.command(frame, position)
        self.assertIsNotNone(detail)
        self.assertEqual([m["frame"] for m in detail["matches"]], [2, 5, 7, 9])
        self.assertEqual([m["checked_pixels"] for m in detail["matches"]], [334, 476, 476, 476])
        # 已证实无遮挡的上缘任一非零像素仍必须严格匹配。
        for row in range(14):
            for column in range(24):
                if self.assets["commands"][2].getpixel((column, row)):
                    frame.putpixel((x + column, y + row), (251, 1, 127))
                    self.assertIsNone(oracle.command(frame, position))
                    return
        self.fail("top icon unexpectedly has no positive pixels")

    def test_missing_selection_ack_stops_before_direction_or_confirm_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            source_bytes = b"source"
            save.write_bytes(source_bytes)
            stamp = save.stat().st_mtime_ns
            capture, calls = Mock(), []
            capture.key.side_effect = calls.append
            def wait(capture, name, predicate, checkpoints, **kwargs):
                if name == "actor0-selection":
                    raise TimeoutError("range not proven")
                return Image.new("RGB", (320, 200)), {}
            plan = dict(self.plan, navigation=[])
            with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait):
                with self.assertRaisesRegex(TimeoutError, "range not proven"):
                    probe.exercise(capture, self.source, self.focus, self.assets, self.composites,
                                   self.ranges, plan, {3: [None] * 3}, save, source_bytes, stamp,
                                   {"checkpoints": []})
            self.assertEqual(calls, ["UP", "ENTER", "ENTER"])

    def test_identical_bytes_with_changed_mtime_are_not_readonly_success(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "FD2.SAV"
            path.write_bytes(b"same")
            stamp = path.stat().st_mtime_ns
            import os
            os.utime(path, ns=(stamp + 1000000, stamp + 1000000))
            with self.assertRaises(AssertionError):
                probe.require_source_unchanged(path, b"same", stamp)

    def test_cancelled_move_reenters_command_before_final_browse(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"source")
            capture, checkpoints = Mock(), []
            def wait(capture, name, predicate, records, **kwargs):
                checkpoints.append(name)
                return Image.new("RGB", (320, 200)), {}
            plan = dict(self.plan, navigation=[])
            with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait):
                probe.exercise(capture, self.source, self.focus, self.assets, self.composites,
                               self.ranges, plan, {3: [None] * 3}, save, b"source", save.stat().st_mtime_ns,
                               {"checkpoints": []})
            self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                             ["UP", "ENTER", "ENTER", plan["step_key"], "ENTER", "ESC", "ENTER", "ESC", "ESC"])
            self.assertEqual(checkpoints[-5:], ["one-step-command", "cancel-restored-selection",
                              "restored-zero-step-direct-command", "second-command-cancel-selection", "selection-cancel-browse"])


class PlayerPairRunTest(unittest.TestCase):
    def run_model(self, *, sdl_failure=False, same_bytes_rewrite=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "original_game"
            original.mkdir()
            (original / "START.SAV").write_bytes(b"source fixture")
            for name in ("game.exe", "driver.exe", "dosbox.exe"):
                (root / name).write_bytes(name.encode())
            args = SimpleNamespace(root=root, artifacts=root / "artifacts", save_fixture=None,
                                   game=root / "game.exe", driver=root / "driver.exe", dosbox=root / "dosbox.exe")
            events, live = [], []
            @contextmanager
            def running(command, cwd, env, log, driver, receipts):
                kind = "sdl" if "FD2SDL_SAVE_PATH" in env else "original"
                self.assertFalse(live, "pair must close original before opening SDL")
                live.append(kind)
                events.append((kind, "open"))
                receipts.append({"kind": kind, "forced_cleanup": False})
                try:
                    yield Mock(pid=123)
                finally:
                    live.pop()
                    events.append((kind, "close"))
            def exercise(capture, source, focus, assets, composites, ranges, plan, titles, save, data, mtime, report):
                self.assertEqual(save.stat().st_mtime_ns, mtime)
                if same_bytes_rewrite:
                    import os
                    os.utime(save, ns=(mtime + 1000000, mtime + 1000000))
                if sdl_failure and live == ["sdl"]:
                    raise TimeoutError("SDL checkpoint missing")
            environment = dict(probe.os.environ)
            environment.pop("FD2SDL_SAVE_PATH", None)
            with patch.dict(probe.os.environ, environment, clear=True), \
                    patch.object(probe, "prepare", return_value=(b"source", (1, 2), {}, [], [], {})), \
                    patch.object(probe, "title_templates", return_value={}), \
                    patch.object(probe, "Capture"), patch.object(probe, "running", side_effect=running), \
                    patch.object(probe, "exercise", side_effect=exercise), \
                    patch("subprocess.Popen", side_effect=AssertionError("no GUI in model tests")):
                report = probe.run(args)
            self.assertTrue((args.artifacts / "comparison.json").is_file())
            self.assertTrue(report["fixture_unchanged"])
            self.assertTrue(report["original_unchanged"])
            return report, events

    def test_original_then_sdl_are_sequential_and_scope_remains_partial(self):
        report, events = self.run_model()
        self.assertTrue(report["ok"])
        self.assertEqual(events, [("original", "open"), ("original", "close"), ("sdl", "open"), ("sdl", "close")])
        self.assertEqual([case["status"] for case in report["cases"]], ["passed", "passed"])
        self.assertFalse(report["command_icon_scope"]["full_icon_visual_equivalence"])
        self.assertTrue(all(not case["full_runtime_fields_measured"] for case in report["cases"]))

    def test_sdl_failure_keeps_completed_original_case_and_cleanup(self):
        report, events = self.run_model(sdl_failure=True)
        self.assertFalse(report["ok"])
        self.assertEqual([case["status"] for case in report["cases"]], ["passed", "failed"])
        self.assertEqual(events[-1], ("sdl", "close"))
        self.assertIn("SDL checkpoint missing", report["error"])

    def test_same_bytes_rewrite_is_failure_and_prevents_sdl_start(self):
        report, events = self.run_model(same_bytes_rewrite=True)
        self.assertFalse(report["ok"])
        self.assertEqual(events, [("original", "open"), ("original", "close")])
        self.assertEqual(len(report["cases"]), 1)


if __name__ == "__main__":
    unittest.main()
