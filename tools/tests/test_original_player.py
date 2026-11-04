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


class PlayerVisualOracleTest(unittest.TestCase):
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
        sprite = self.assets["leader_sprites"][0]
        x, y = self.plan["leader_screen"]
        frame.paste(sprite.convert("RGB"), (x, y - 6), sprite.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNotNone(oracle.command(frame, tuple(self.plan["leader_screen"]), 3))

    def command_frame(self, phase=1, states=(2, 5, 7, 9)):
        position = tuple(self.plan["leader_screen"])
        frame = Image.new("RGB", (320, 200))
        for item, index in enumerate(states):
            image = self.assets["commands"][index]
            at = tuple(a + b for a, b in zip(position, probe.MENU_OFFSETS[item]))
            frame.paste(image.convert("RGB"), at, image.point(lambda value: 255 if value else 0, mode="L"))
        sprite = self.assets["leader_sprites"][phase]
        frame.paste(sprite.convert("RGB"), (position[0], position[1] - 6),
                    sprite.point(lambda value: 255 if value else 0, mode="L"))
        return frame

    def test_top_command_all_twenty_rows_include_current_actor_and_reject_corrupt_occlusion(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        position = tuple(self.plan["leader_screen"])
        for phase in range(12):
            detail = oracle.command(self.command_frame(phase), position)
            self.assertIsNotNone(detail)
            self.assertEqual([match["resource_rows"] for match in detail["matches"]], [20] * 4)
            self.assertEqual([match["checked_pixels"] for match in detail["matches"]], [476] * 4)
            self.assertTrue(detail["full_resource_height_checked"])
            self.assertFalse(detail["capability_gates_compared"])
            self.assertFalse(detail["default_selection_compared"])
        frame = self.command_frame()
        sprite = self.assets["leader_sprites"][1]
        for row in range(6):
            for column in range(24):
                if sprite.getpixel((column, row)):
                    frame.putpixel((position[0] + column, position[1] - 6 + row), (251, 1, 127))
                    self.assertIsNone(oracle.command(frame, position))
                    return
        self.fail("expected actual actor overlap pixels")

    def test_full_icon_resource_matching_does_not_require_same_abilities_or_default_selection(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        for states in ((2, 5, 7, 9), (2, 4, 6, 9)):
            detail = oracle.command(self.command_frame(states=states), tuple(self.plan["leader_screen"]))
            self.assertIsNotNone(detail)
            self.assertEqual([match["frame"] for match in detail["matches"]], list(states))
        self.assertIsNone(oracle.command(self.command_frame(), tuple(self.plan["leader_screen"]), 3))
        self.assertIsNotNone(oracle.command(self.command_frame(states=(2, 5, 6, 10)), tuple(self.plan["leader_screen"]), 3))

    def test_acted_sprite_maps_only_opaque_nonzero_and_preserves_cursor_colors(self):
        sprite = Image.new("P", (4, 1))
        sprite.putpalette(self.assets["palette"])
        sprite.putdata((0, 1, 8, 255))
        self.assertEqual(list(probe.acted_sprite(sprite).getdata()), [0, 25, 24, 31])
        cursor, source = self.assets["cursor"], self.assets["leader_sprites"][0]
        acted = self.assets["acted_leader_cursors"][0]
        visible_cursor = 0
        for y in range(24):
            for x in range(24):
                if cursor.getpixel((x, y)) and (y + 6 >= 24 or not source.getpixel((x, y + 6))):
                    self.assertEqual(acted.getpixel((x, y + 6)), cursor.getpixel((x, y)))
                    visible_cursor += 1
        self.assertGreater(visible_cursor, 0)

    def acted_frame(self):
        frame = Image.new("RGB", (320, 200))
        leader = self.assets["acted_leader_cursors"][0]
        x, y = self.plan["leader_screen"]
        frame.paste(leader.convert("RGB"), (x, y - 6), leader.point(lambda value: 255 if value else 0, mode="L"))
        frame.paste(self.assets["probe_base_tile"].convert("RGB"), tuple(self.plan["destination_screen"]))
        return frame

    def test_wait_browse_requires_acted_leader_not_ordinary_actor_or_only_cursor(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        self.assertIsNotNone(oracle.browse(self.acted_frame(), acted=True))
        self.assertIsNone(oracle.browse(self.acted_frame()))
        ordinary = self.frame()
        ordinary.paste(self.assets["probe_base_tile"].convert("RGB"), tuple(self.plan["destination_screen"]))
        self.assertIsNone(oracle.browse(ordinary, acted=True))

    def test_native_detail_requires_both_real_edge_resources(self):
        oracle = probe.PlayerOracle(self.source, self.assets, self.ranges, self.plan)
        frame = Image.new("RGB", (320, 200))
        self.assertIsNone(oracle.detail(frame))
        for index, (image, at) in enumerate(self.assets["detail_edges"]):
            frame.paste(image.convert("RGB"), at, image.point(lambda value: 255 if value else 0, mode="L"))
            if index == 0:
                self.assertIsNone(oracle.detail(frame))
        detail = oracle.detail(frame)
        self.assertIsNotNone(detail)
        self.assertFalse(detail["full_page_visual_equivalence"])

    def test_wait_source_must_be_full_nonzero_hp_before_sending_any_input(self):
        for hp, maximum in ((0, 0), (1, 2)):
            source = bytearray(self.source)
            import struct
            struct.pack_into("<HH", source, probe.UNITS + 0x40, hp, maximum)
            capture = Mock()
            with self.assertRaises(ValueError):
                probe.exercise(capture, source, self.focus, self.assets, self.composites, self.ranges,
                               self.plan, {}, None, None, None, {}, "wait-full-hp")
            capture.key.assert_not_called()
        self.assertFalse(probe.require_full_hp(self.source)["injured_recovery_branch_covered"])

    def test_wait_checks_acted_before_entering_native_detail_and_stops_on_missing_ack(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"source")
            for fail_acted in (False, True):
                capture, checkpoints = Mock(), []
                def wait(capture, name, predicate, records, **kwargs):
                    checkpoints.append(name)
                    if fail_acted and name == "wait-acted-closed-browse":
                        raise TimeoutError("acted not proven")
                    return Image.new("RGB", (320, 200)), {}
                with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait):
                    try:
                        probe.exercise(capture, self.source, self.focus, self.assets, self.composites,
                                       self.ranges, dict(self.plan, navigation=[]), {3: [None] * 3},
                                       save, b"source", save.stat().st_mtime_ns, {"checkpoints": []}, "wait-full-hp")
                    except TimeoutError:
                        self.assertTrue(fail_acted)
                keys = [call.args[0] for call in capture.key.call_args_list]
                self.assertEqual(keys, ["UP", "ENTER", "ENTER", "ENTER", "DOWN", "ENTER"] + ([] if fail_acted else ["ENTER"]))
                self.assertEqual("acted-enter-opens-detail" in checkpoints, not fail_acted)

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
                                   game=root / "game.exe", driver=root / "driver.exe", dosbox=root / "dosbox.exe", scenario="move-cancel")
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
            def exercise(capture, source, focus, assets, composites, ranges, plan, titles, save, data, mtime, report, scenario):
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
        self.assertTrue(report["command_icon_scope"]["resource_overlay_checked"])
        self.assertFalse(report["command_icon_scope"]["full_menu_visual_equivalence"])
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
