from contextlib import contextmanager
import importlib.util
import os
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import Mock, patch

from PIL import Image

HERE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("original_focus", HERE / "run_original_focus.py")
focus = importlib.util.module_from_spec(spec)
spec.loader.exec_module(focus)


class FocusFixtureAndOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = focus.ROOT / "original_game"
        cls.fixture = cls.original / "START.SAV"
        metadata = focus.archive_entry((cls.original / "FDFIELD.DAT").read_bytes(), 1)
        cls.source, _ = focus.validate_initial(cls.fixture, metadata[0])
        cls.encrypted = cls.fixture.read_bytes()
        cls.prepared = []
        for name, target in focus.TARGETS:
            encoded, plain, manifest = focus.derive_fixture(cls.source, cls.encrypted, target)
            cls.prepared.append((name, encoded, plain, manifest, *focus.prepare(cls.original, plain, target)))

    def test_coordinate_derivation_only_changes_two_known_fields_and_checksum(self):
        for name, encoded, plain, manifest, *_ in self.prepared:
            allowed = {focus.UNITS, focus.UNITS + 1, *range(len(plain) - 4, len(plain))}
            actual = {i for i, (old, new) in enumerate(zip(self.source, plain)) if old != new}
            self.assertTrue(actual <= allowed)
            self.assertEqual(actual, {item["offset"] for item in manifest["changed_plain_bytes"]})
            self.assertEqual(actual, {item["offset"] for item in manifest["changed_encrypted_bytes"]})
            self.assertEqual(plain[focus.META:focus.META + 18], self.source[focus.META:focus.META + 18])
            self.assertEqual(sum(plain[:-4]), int.from_bytes(plain[-4:], "little"))
            self.assertTrue(manifest["synthetic_coordinate_fixture"])
            self.assertFalse(manifest["natural_opening_covered"])
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / (name + ".SAV")
                path.write_bytes(encoded)
                self.assertEqual(focus.decode_save(path), plain)

    def test_source_camera_or_target_occupancy_is_rejected_before_fixture_use(self):
        source = bytearray(self.source)
        source[focus.META + 3] += 1
        source[-4:] = sum(source[:-4]).to_bytes(4, "little")
        with self.assertRaisesRegex(ValueError, "camera/focus"):
            focus.derive_fixture(bytes(source), self.encrypted, (2, 5))
        occupied = tuple(self.source[focus.UNITS + focus.STRIDE:focus.UNITS + focus.STRIDE + 2])
        with self.assertRaisesRegex(ValueError, "占用"):
            focus.derive_fixture(self.source, self.encrypted, occupied)
        with self.assertRaisesRegex(ValueError, "超出"):
            focus.derive_fixture(self.source, self.encrypted, (24, 5))

    def test_invalid_profile_or_flags_cannot_become_a_supported_fixture(self):
        for offset, value in ((5, 0x80), (6, 0), (7, 9), (0x20, 0), (0x3b, 0)):
            source = bytearray(self.source)
            source[focus.UNITS + offset] = value
            source[-4:] = sum(source[:-4]).to_bytes(4, "little")
            with self.assertRaisesRegex(ValueError, "actor0"):
                focus.derive_fixture(bytes(source), self.encrypted, (2, 5))

    def test_reference_uses_pre_move_thresholds_and_x_then_y(self):
        left = focus.native_focus_reference((5, 11), (8, 15), (2, 5))
        right = focus.native_focus_reference((5, 11), (8, 15), (19, 21))
        self.assertEqual((left["camera"], left["screen"]), ([1, 4], [28, 28]))
        self.assertEqual((right["camera"], right["screen"]), ([8, 15], [268, 148]))
        self.assertEqual(left["steps"][0], {"axis": "x", "world": [7, 15], "camera": [5, 11]})
        self.assertEqual(left["steps"][1], {"axis": "x", "world": [6, 15], "camera": [5, 11]})
        self.assertEqual([step["axis"] for step in left["steps"]], ["x"] * 6 + ["y"] * 10)
        self.assertEqual(focus.native_focus_reference((5, 11), (8, 15), (8, 15))["steps"], [])
        self.assertEqual(focus.native_focus_reference((5, 11), (8, 15), (0, 0))["camera"], [0, 0])
        self.assertEqual(focus.native_focus_reference((5, 11), (8, 15), (23, 23))["camera"], [11, 16])

    def test_full_dry_range_and_independent_terrain_camera_have_no_added_masks(self):
        for _, _, _, _, assets, ranges, camera_tiles, plan in self.prepared:
            self.assertEqual(plan["range_probe"]["pixels"], 576)
            self.assertEqual(plan["range_probe"]["rows"], 24)
            self.assertFalse(plan["range_probe"]["water_mask"])
            self.assertGreaterEqual(len(camera_tiles), 1)
            self.assertEqual(plan["camera_rois"][-1]["compatible_camera_count"], 1)
            self.assertTrue(plan["camera_uniquely_identified"])
            for image in [assets["probe_base_tile"], *(image for _, image in ranges), *(image for image, _ in camera_tiles)]:
                self.assertEqual(image.size, (24, 24))
                self.assertFalse(any(224 <= value < 240 for value in image.getdata()))
            for image, _ in camera_tiles:
                self.assertNotIn(0, image.getdata())

    def test_original_water_target_is_rejected_without_a_range_mask(self):
        _, plain, _ = focus.derive_fixture(self.source, self.encrypted, (20, 22))
        with self.assertRaisesRegex(ValueError, "可站立"):
            focus.prepare(self.original, plain, (20, 22))

    def frame(self, prepared, *, selected=False, phase=0):
        _, _, _, _, assets, ranges, camera_tiles, plan = prepared
        frame = Image.new("RGB", (320, 200))
        for image, at in camera_tiles:
            frame.paste(image.convert("RGB"), at)
        image = ranges[0][1] if selected else assets["probe_base_tile"]
        frame.paste(image.convert("RGB"), tuple(plan["range_probe"]["screen"]))
        leader = assets["leader_cursors"][phase]
        x, y = plan["leader_screen"]
        frame.paste(leader.convert("RGB"), (x, y - 6), leader.point(lambda value: 255 if value else 0, mode="L"))
        return frame

    def oracle(self, prepared):
        _, _, plain, _, assets, ranges, camera_tiles, plan = prepared
        return focus.FocusOracle(plain, assets, ranges, camera_tiles, plan)

    def test_browse_requires_all_twelve_actor_frames_full_cursor_and_untranslated_tile(self):
        for prepared in self.prepared:
            oracle = self.oracle(prepared)
            for phase in range(12):
                detail = oracle.browse(self.frame(prepared, phase=phase))
                self.assertIsNotNone(detail)
                self.assertEqual(detail["expected_screen"], detail["actual_screen"])
                self.assertEqual(detail["terrain_proven_camera"], prepared[-1]["expected_camera"])
            self.assertIsNone(oracle.browse(self.frame(prepared, selected=True)))
            frame = self.frame(prepared)
            x, y = prepared[-1]["leader_screen"]
            frame.paste((251, 1, 127), (x, y - 6, x + 24, y + 24))
            self.assertIsNone(oracle.browse(frame))

    def test_each_full_camera_roi_is_required_independently_of_actor_location(self):
        for prepared in self.prepared:
            oracle = self.oracle(prepared)
            for image, at in prepared[-2]:
                frame = self.frame(prepared)
                frame.putpixel(at, (251, 1, 127))
                self.assertIsNone(oracle.browse(frame))

    def test_selection_requires_full_576_pixel_range_tile_and_no_command(self):
        for prepared in self.prepared:
            oracle = self.oracle(prepared)
            self.assertIsNone(oracle.selection(self.frame(prepared)))
            frame = self.frame(prepared, selected=True)
            self.assertEqual(oracle.selection(frame)["checked_pixels"], 576)
            x, y = prepared[-1]["range_probe"]["screen"]
            frame.putpixel((x + 23, y + 23), (251, 1, 127))
            self.assertIsNone(oracle.selection(frame))

    def command_frame(self, prepared, phase):
        _, _, _, _, assets, _, _, plan = prepared
        frame = self.frame(prepared, selected=True, phase=phase)
        x, y = plan["leader_screen"]
        for item, index in enumerate((2, 5, 7, 9)):
            image = assets["commands"][index]
            dx, dy = focus.MENU_OFFSETS[item]
            frame.paste(image.convert("RGB"), (x + dx, y + dy), image.point(lambda value: 255 if value else 0, mode="L"))
        # sprite 独立原图，不能把 cursor 的像素带入上图标覆盖区。
        icons = (self.original / "FDICON.B24").read_bytes()
        sprite = focus.sheet_frame(icons, phase)
        sprite.putpalette(assets["palette"])
        frame.paste(sprite.convert("RGB"), (x, y - 6), sprite.point(lambda value: 255 if value else 0, mode="L"))
        return frame

    def test_all_four_command_icons_are_fully_visible_with_real_actor_occlusion(self):
        for prepared in self.prepared:
            oracle = self.oracle(prepared)
            for phase in range(12):
                detail = oracle.command(self.command_frame(prepared, phase))
                self.assertIsNotNone(detail)
                self.assertEqual([item["resource_rows"] for item in detail["matches"]], [20] * 4)
                self.assertEqual([item["checked_pixels"] for item in detail["matches"]], [476] * 4)
                self.assertFalse(detail["capability_gates_compared"])
                self.assertFalse(detail["default_selection_compared"])
            frame = self.command_frame(prepared, 0)
            x, y = prepared[-1]["leader_screen"]
            frame.paste((251, 1, 127), (x + 24, y + 2, x + 48, y + 22))
            self.assertIsNone(oracle.command(frame))

    def test_exercise_ack_failure_stops_keys_without_retry_or_recovery(self):
        _, encoded, plain, _, assets, ranges, camera_tiles, plan = self.prepared[0]
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(encoded)
            capture = Mock()
            checkpoints = []
            def ack(capture_arg, name, predicate, report, **kwargs):
                checkpoints.append(name)
                if name == "one-z-edge-focus":
                    raise TimeoutError("no edge resource ACK")
                return None, {"ok": True}
            with patch.object(focus, "wait_title"), patch.object(focus, "wait_state", side_effect=ack):
                with self.assertRaisesRegex(TimeoutError, "ACK"):
                    focus.exercise(capture, plain, assets, ranges, camera_tiles, plan,
                                   {3: [None, None, None]}, save, encoded, save.stat().st_mtime_ns, {"checkpoints": []})
            self.assertEqual([item.args[0] for item in capture.key.call_args_list], ["UP", "ENTER", "Z"])
            self.assertEqual(checkpoints, ["source-browse", "one-z-edge-focus"])

    def test_exercise_has_single_z_and_each_later_key_waits_for_state_ack(self):
        _, encoded, plain, _, assets, ranges, camera_tiles, plan = self.prepared[0]
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(encoded)
            events = []
            capture = Mock()
            capture.key.side_effect = lambda key: events.append(key)
            def ack(capture_arg, name, predicate, report, **kwargs):
                events.append(name)
                return None, {"ok": True}
            with patch.object(focus, "wait_title"), patch.object(focus, "wait_state", side_effect=ack):
                focus.exercise(capture, plain, assets, ranges, camera_tiles, plan,
                               {3: [None, None, None]}, save, encoded, save.stat().st_mtime_ns, {"checkpoints": []})
            self.assertEqual(events, ["UP", "ENTER", "source-browse", "Z", "one-z-edge-focus", "ENTER",
                "edge-unit-selection", "ENTER", "edge-zero-step-command", "ESC", "command-cancel-selection",
                "ESC", "selection-cancel-browse"])

    def test_identical_bytes_rewrite_after_ack_is_rejected(self):
        _, encoded, plain, _, assets, ranges, camera_tiles, plan = self.prepared[0]
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(encoded)
            stamp = save.stat().st_mtime_ns
            capture = Mock()
            def ack(*args, **kwargs):
                os.utime(save, ns=(stamp + 1_000_000_000, stamp + 1_000_000_000))
                return None, {"ok": True}
            with patch.object(focus, "wait_title"), patch.object(focus, "wait_state", side_effect=ack):
                with self.assertRaisesRegex(AssertionError, "mtime"):
                    focus.exercise(capture, plain, assets, ranges, camera_tiles, plan,
                                   {3: [None, None, None]}, save, encoded, stamp, {"checkpoints": []})
            self.assertEqual([item.args[0] for item in capture.key.call_args_list], ["UP", "ENTER"])


class FocusRunnerOwnershipTest(unittest.TestCase):
    def test_all_original_cases_finish_and_close_before_first_sdl_and_failure_is_retained(self):
        original = focus.ROOT / "original_game"
        starts, exits, active = [], [], []
        @contextmanager
        def owned(command, cwd, env, log, driver, receipts):
            self.assertFalse(active)
            active.append(command)
            try:
                yield SimpleNamespace(pid=123)
            finally:
                active.pop()
                exits.append(command)
        def fake_exercise(capture, plain, assets, ranges, camera_tiles, plan, titles, save, data, stamp, report):
            starts.append(report["name"])
            self.assertEqual(len(exits), len(starts) - 1)
            if report["kind"] == "sdl":
                raise TimeoutError("SDL missing edge ACK")
        def fake_copy(source, target):
            Path(target).mkdir()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            binary = base / "binary.exe"
            binary.write_bytes(b"not executed")
            args = SimpleNamespace(root=focus.ROOT, artifacts=base / "artifacts", save_fixture=original / "START.SAV",
                                   game=binary, driver=binary, dosbox=binary)
            with patch.object(focus, "hashes", return_value={}), patch.object(focus.shutil, "copytree", side_effect=fake_copy), \
                    patch.object(focus, "running", side_effect=owned), patch.object(focus, "Capture"), \
                    patch.object(focus, "title_templates", return_value={}), patch.object(focus, "exercise", side_effect=fake_exercise):
                result = focus.run(args)
            self.assertEqual(starts, ["original-upper-left", "original-lower-right", "sdl-upper-left"])
            self.assertEqual(len(exits), 3)
            self.assertFalse(active)
            self.assertFalse(result["ok"])
            self.assertEqual([case["status"] for case in result["cases"]], ["passed", "passed", "failed"])
            self.assertTrue(result["original_unchanged"])
            self.assertTrue(result["fixture_unchanged"])
            self.assertIn("SDL missing edge ACK", result["cases"][-1]["error"])
            self.assertTrue((args.artifacts / "comparison.json").is_file())


if __name__ == "__main__":
    unittest.main()
