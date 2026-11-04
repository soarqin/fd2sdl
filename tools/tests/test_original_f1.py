import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
from contextlib import contextmanager
from types import SimpleNamespace
import os

from PIL import Image

HERE = Path(__file__).resolve().parent
RUNNER = HERE / "run_original_f1.py"
if not RUNNER.is_file():
    RUNNER = HERE.parent / "run_original_f1.py"
spec = importlib.util.spec_from_file_location("f1_pair", RUNNER)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class F1OracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = probe.prepare(probe.ROOT / "original_game", probe.ROOT / "original_game/START.SAV")

    def test_resource_preflight_uses_real_shape_and_rle_audit(self):
        source, focus, assets, composites, base, safe, actor, plan = self.prepared
        self.assertEqual(plan["map"], [24, 24])
        self.assertEqual(plan["shape"], source[0])
        self.assertEqual(plan["shape_frames"], 288)
        self.assertEqual(plan["map_used_tiles"], 138)
        self.assertFalse(plan["map_used_tiles_with_rle_skip"])
        self.assertEqual(plan["marker_color_bases"], [32, 80, 72])
        self.assertEqual(len(plan["safe_markers"]["actors"]), 11)
        self.assertNotIn(9, [actor["actor"] for actor in plan["safe_markers"]["actors"]])

    def test_nearest_neighbor_samples_and_black_bounds_are_explicit(self):
        palette = [channel for index in range(256) for channel in (index, index, index)]
        tile = bytes((y * 24 + x) % 256 for y in range(24) for x in range(24))
        image = probe.scaled_base(24, 24, [0] * 576, {0: tile}, palette)
        for at, expected in (((112, 52), 0), ((113, 52), 6), ((115, 52), 18),
                             ((112, 53), 144), ((116, 52), 0), ((111, 52), 0),
                             ((208, 52), 0), ((112, 148), 0), ((319, 199), 0)):
            self.assertEqual(image.getpixel(at), expected)

    def test_eight_full_frame_phases_match_uniquely(self):
        frames = self.prepared[5]
        oracle = probe.FullFrameOracle(frames)
        for phase, frame in enumerate(frames):
            match = oracle.match(frame)
            self.assertIsNotNone(match)
            self.assertEqual(match["pulse_phase"], phase)
            self.assertEqual(match["checked_pixels"], 64000)
            self.assertIsNone(oracle.match(frame, (phase,)))

    def test_black_and_opaque_pixels_are_never_ignored(self):
        frames = self.prepared[5]
        oracle = probe.FullFrameOracle(frames)
        for position in ((0, 0), (319, 199), (160, 100)):
            frame = frames[3].convert("RGB")
            old = frame.getpixel(position)
            frame.putpixel(position, tuple(255 - value for value in old))
            self.assertIsNone(oracle.match(frame), f"all pixels must be checked: {position}")

    def test_actor_focus_gray_block_overlays_player_block(self):
        source, _, assets, _, base, safe, actor_frames, plan = self.prepared
        phase = 4
        position = tuple(plan["actor_focus_markers"]["focus_screen"])
        palette = assets["palette"]
        expected = tuple(palette[(24 + phase) * 3:(25 + phase) * 3])
        for y in range(3):
            for x in range(3):
                self.assertEqual(actor_frames[phase].convert("RGB").getpixel((position[0] + x, position[1] + y)), expected)
        mutable = bytearray(source)
        mutable[probe.UNITS + 5] = 0x84  # acted/AI-ineligible do not hide marker.
        mutable[probe.UNITS + 0x40:probe.UNITS + 0x42] = b"\0\0"
        _, detail = probe.marker_frames(base, mutable, tuple(plan["safe_world"]), (32, 80, 72))
        self.assertIn(0, [actor["actor"] for actor in detail["actors"]])

    def test_full_frame_resource_dac_calibration_is_locked_per_page(self):
        assets, frames = self.prepared[2], self.prepared[5]
        actual = frames[0].copy()
        actual.putpalette(assets["f1_palette_cycles"][14]["palette"])
        self.assertIsNone(probe.FullFrameOracle(frames).match(actual))
        oracle = probe.FullFrameOracle(frames, assets["f1_palette_cycles"])
        detail = oracle.match(actual)
        self.assertIsNotNone(detail)
        self.assertEqual(detail["pulse_phase"], 0)
        self.assertEqual(detail["water_palette_cycle_candidates"], [14])
        self.assertEqual(detail["checked_pixels"], 64000)
        self.assertEqual(detail["bad_pixels"], 0)
        self.assertEqual(detail["channel_tolerance"], 3)
        changed = frames[2].copy()
        changed.putpalette(assets["f1_palette_cycles"][0]["palette"])
        with self.assertRaisesRegex(AssertionError, "水DAC漂移"):
            oracle.match(changed.convert("RGB"))

    def test_wait_cannot_swallow_legal_water_drift_and_accept_returned_cycle(self):
        assets, frames = self.prepared[2], self.prepared[5]
        def actual(cycle, phase):
            image = frames[phase].copy()
            image.putpalette(assets["f1_palette_cycles"][cycle]["palette"])
            return image.convert("RGB")
        capture = Mock()
        capture.frame.side_effect = [actual(14, 0), actual(0, 1), actual(14, 2)]
        oracle = probe.FullFrameOracle(frames, assets["f1_palette_cycles"])
        with patch("run_original_battle_menu.save_checkpoint"):
            _, first = probe.wait_state(capture, "first", oracle.match, [])
            self.assertEqual(first["pulse_phase"], 0)
            samples = []
            with self.assertRaisesRegex(AssertionError, "水DAC漂移"):
                probe.wait_state(capture, "distinct", probe.phase_sampler(
                    oracle, samples, "distinct", excluded=(0,)), [])
        self.assertEqual(capture.frame.call_count, 2, "must fail on drift before consuming the returned cycle14 frame")
        self.assertEqual(oracle.water_candidates, [14])
        self.assertEqual(samples[0]["delay_ms"], 0)
        self.assertIn("水DAC漂移", samples[0]["error"])

    def test_timeout_at_full_map_gate_never_sends_closing_or_recovery_keys(self):
        source, focus, assets, composites, base, safe, actor, plan = self.prepared
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"unchanged")
            capture = Mock(artifacts=Path(directory))
            def wait(capture, name, predicate, records, **kwargs):
                if name == "f1-escape-pulse-first":
                    raise TimeoutError("no full map ACK")
                return Image.new("RGB", (320, 200)), {}
            with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait), \
                    patch.object(probe, "driver_call") as close:
                with self.assertRaisesRegex(TimeoutError, "no full map ACK"):
                    probe.exercise(capture, source, focus, assets, composites, safe, actor, plan,
                                   {3: [None] * 3}, save, b"unchanged", save.stat().st_mtime_ns,
                                   {"checkpoints": [], "observed_pulses": [], "inputs": []})
            self.assertEqual(capture.key.call_args_list[-1].args, ("F1",))
            self.assertNotIn("ESC", [call.args[0] for call in capture.key.call_args_list])
            close.assert_not_called()

    def test_sequence_preserves_disk_and_requests_host_close_only_after_page_proof(self):
        source, focus, assets, composites, base, safe, actor, plan = self.prepared
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"unchanged")
            stamp = save.stat().st_mtime_ns
            capture = Mock(artifacts=Path(directory))
            capture.process.pid = 123
            capture.process.wait.return_value = capture.process.poll.return_value = 0
            names = []
            def wait(capture, name, predicate, records, **kwargs):
                names.append(name)
                return Image.new("RGB", (320, 200)), {"pulse_phase": 1 if name.endswith("distinct") else 0}
            report = {"checkpoints": [], "observed_pulses": [], "inputs": []}
            with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait), \
                    patch.object(probe, "driver_call") as close:
                probe.exercise(capture, source, focus, assets, composites, safe, actor, plan,
                               {3: [None] * 3}, save, b"unchanged", stamp, report)
            keys = [call.args[0] for call in capture.key.call_args_list]
            self.assertEqual([key for key in keys if key in ("F1", "PAGEUP")], ["F1", "PAGEUP", "F1", "F1"])
            self.assertIn("pageup-right-same-focus-browse", names)
            self.assertEqual(names[-1], "actor0-focus-overlays-side-marker-pulse-distinct")
            self.assertEqual(len(report["observed_pulses"]), 4)
            close.assert_called_once_with(capture.driver, "close", capture.process)
            self.assertTrue(report["host_close"]["ended_before_finally_cleanup"])
            self.assertEqual(save.read_bytes(), b"unchanged")
            self.assertEqual(save.stat().st_mtime_ns, stamp)

    def test_original_evidence_paths_are_rejected_before_mkdir(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "original_game"
            original.mkdir()
            args = Mock(root=root, artifacts=original / "new", save_fixture=None)
            with self.assertRaises(ValueError):
                probe.run(args)
            self.assertFalse(args.artifacts.exists())

    def test_unknown_exe_is_rejected_before_le_image_allocation(self):
        read_bytes = Path.read_bytes
        def changed(path):
            data = read_bytes(path)
            return bytes([data[0] ^ 1]) + data[1:] if path.name == "FD2.EXE" else data
        with patch.object(Path, "read_bytes", changed), patch.object(probe, "build_relbase_image") as build:
            with self.assertRaisesRegex(ValueError, "不是已验证的LE布局"):
                probe.prepare(probe.ROOT / "original_game", probe.ROOT / "original_game/START.SAV")
        build.assert_not_called()

    def test_sdl_waits_same_native_phases_without_extra_keys(self):
        source, focus, assets, composites, base, safe, actor, plan = self.prepared
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"unchanged")
            capture = Mock(artifacts=Path(directory))
            capture.process.pid = 123
            capture.process.wait.return_value = capture.process.poll.return_value = 0
            names = []
            def actual(frames, phase):
                image = frames[phase].copy()
                image.putpalette(assets["f1_palette_cycles"][14]["palette"])
                return image.convert("RGB")
            def wait(capture, name, predicate, records, **kwargs):
                names.append(name)
                frames = actor if name.startswith("actor0-focus-") else safe
                if name.endswith("-pulse-first"):
                    image = actual(frames, 1)
                    detail = predicate(image)
                    self.assertIsNotNone(detail)
                    return image, detail
                if "-native-phase" in name:
                    phase = int(name.rsplit("phase", 1)[1])
                    self.assertIsNone(predicate(actual(frames, (phase + 1) % 8)))
                    image = actual(frames, phase)
                    detail = predicate(image)
                    self.assertIsNotNone(detail)
                    self.assertEqual(detail["pulse_phase"], phase)
                    return image, detail
                return Image.new("RGB", (320, 200)), {}
            report = {"checkpoints": [], "observed_pulses": [], "inputs": []}
            with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait), \
                    patch.object(probe, "driver_call"):
                observed = probe.exercise(capture, source, focus, assets, composites, safe, actor, plan,
                                          {3: [None] * 3}, save, b"unchanged", save.stat().st_mtime_ns,
                                          report, observations())
            self.assertTrue(all(list(page["frames"]) == [3, 4] for page in observed.values()))
            self.assertTrue(all(page["water_palette_cycle_candidates"] == [14] for page in observed.values()))
            self.assertEqual(len([name for name in names if "-native-phase" in name]), 8)
            self.assertFalse(any(name.endswith("distinct") for name in names))
            self.assertEqual(len(report["phase_samples"]), 20)
            target_samples = [sample for sample in report["phase_samples"] if sample["target"] is not None]
            self.assertEqual(sum(sample["matched"] for sample in target_samples), 8)
            self.assertTrue(all(sample["observed_phase"] is not None for sample in target_samples))
            keys = [call.args[0] for call in capture.key.call_args_list]
            self.assertEqual([key for key in keys if key in ("F1", "PAGEUP")], ["F1", "PAGEUP", "F1", "F1"])

    def test_black_water_or_marker_error_after_calibration_is_failure(self):
        assets, frames = self.prepared[2], self.prepared[5]
        cycle = assets["f1_palette_cycles"][14]["palette"]
        first = frames[0].copy()
        first.putpalette(cycle)
        oracle = probe.FullFrameOracle(frames, assets["f1_palette_cycles"])
        self.assertIsNotNone(oracle.match(first))
        # 真实水像素，黑框、普通不透明像素和focus标记都不可被排除。
        water = next((x, y) for y in range(200) for x in range(320)
                     if 224 <= frames[2].getpixel((x, y)) < 240)
        for point in ((0, 0), water, (144, 112), (160, 100)):
            image = frames[2].copy()
            image.putpalette(cycle)
            rgb = image.convert("RGB")
            color = rgb.getpixel(point)
            rgb.putpixel(point, tuple(255 - channel for channel in color))
            self.assertIsNone(oracle.match(rgb), point)

    def test_sdl_reuses_valid_first_phase_and_keeps_same_page_water_lock(self):
        source, focus, assets, composites, base, safe, actor, plan = self.prepared
        for drift in (False, True):
            with self.subTest(drift=drift), tempfile.TemporaryDirectory() as directory:
                save = Path(directory) / "FD2.SAV"
                save.write_bytes(b"unchanged")
                stamp = save.stat().st_mtime_ns
                capture = Mock(artifacts=Path(directory))
                capture.process.pid = 123
                capture.process.wait.return_value = capture.process.poll.return_value = 0
                names = []
                first_images = {}

                def actual(frames, phase, cycle=15):
                    image = frames[phase].copy()
                    image.putpalette(assets["f1_palette_cycles"][cycle]["palette"])
                    return image.convert("RGB")

                def wait(capture, name, predicate, records, **kwargs):
                    names.append(name)
                    frames = actor if name.startswith("actor0-focus-") else safe
                    if name.endswith("-pulse-first"):
                        image = actual(frames, 2)
                        invalid = image.copy()
                        invalid.putpixel((0, 0), (255, 255, 255))
                        self.assertIsNone(predicate(invalid), "坏首图不能校准或收作phase2")
                        detail = predicate(image)
                        self.assertIsNotNone(detail)
                        self.assertEqual(detail["water_palette_cycle_candidates"], [15])
                        first_images[name.removesuffix("-pulse-first")] = image
                        return image, detail
                    if "-native-phase" in name:
                        self.assertTrue(name.endswith("phase4"), "首图phase2不得再次等待")
                        image = actual(frames, 4, cycle=0 if drift else 15)
                        detail = predicate(image)
                        self.assertIsNotNone(detail)
                        return image, detail
                    return Image.new("RGB", (320, 200)), {}

                report = {"checkpoints": [], "observed_pulses": [], "inputs": []}
                with patch.object(probe, "wait_title"), patch.object(probe, "wait_state", side_effect=wait), \
                        patch.object(probe.time, "sleep"), patch.object(probe, "driver_call") as close:
                    if drift:
                        with self.assertRaisesRegex(AssertionError, "水DAC漂移"):
                            probe.exercise(capture, source, focus, assets, composites, safe, actor, plan,
                                           {3: [None] * 3}, save, b"unchanged", stamp,
                                           report, observations(phases=(4, 2)))
                    else:
                        observed = probe.exercise(capture, source, focus, assets, composites, safe, actor, plan,
                                                  {3: [None] * 3}, save, b"unchanged", stamp,
                                                  report, observations(phases=(4, 2)))
                if drift:
                    close.assert_not_called()
                    self.assertFalse(report["observed_pulses"])
                    self.assertEqual(capture.key.call_args_list[-1].args, ("F1",))
                    self.assertIn("水DAC漂移", report["phase_samples"][-1]["error"])
                else:
                    self.assertEqual(len([name for name in names if "-native-phase" in name]), 4)
                    for name, page in observed.items():
                        self.assertEqual(set(page["frames"]), {4, 2})
                        self.assertIs(page["frames"][2], first_images[name])
                        self.assertEqual(page["first_frame_reused_for_native_phase"], 2)
                        self.assertEqual(page["water_palette_cycle_candidates"], [15])
                    close.assert_called_once_with(capture.driver, "close", capture.process)
                    self.assertEqual([call.args[0] for call in capture.key.call_args_list
                                      if call.args[0] in ("F1", "PAGEUP")], ["F1", "PAGEUP", "F1", "F1"])
                self.assertEqual(save.read_bytes(), b"unchanged")
                self.assertEqual(save.stat().st_mtime_ns, stamp)


class F1SamplingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.frames = probe.prepare(probe.ROOT / "original_game", probe.ROOT / "original_game/START.SAV")[5]

    def test_fixed_sampling_aliases_110ms_pingpong_but_stagger_reaches_target_without_input(self):
        sequence = (7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7)
        def attempt(stagger):
            clock = [0]
            capture = Mock(last=None)
            oracle = probe.FullFrameOracle(self.frames)
            seen, samples = [], []
            def sleep(seconds):
                clock[0] += round(seconds * 1000)
            def frame():
                # 固定观察成本800ms + 既有wait80ms，恰隔8个110ms pulse。
                clock[0] += 800
                image = self.frames[sequence[(clock[0] // 110) % len(sequence)]]
                capture.last = image
                return image
            capture.frame.side_effect = frame
            def fixed(image):
                detail = oracle.match(image)
                self.assertIsNotNone(detail)
                seen.append(detail["pulse_phase"])
                return detail if detail["pulse_phase"] == 4 else None
            predicate = probe.phase_sampler(oracle, samples, "target4", target=4) if stagger else fixed
            with patch.object(probe.time, "monotonic", side_effect=lambda: clock[0] / 1000), \
                    patch.object(probe.time, "sleep", side_effect=sleep), \
                    patch("run_original_battle_menu.save_checkpoint"):
                if stagger:
                    _, detail = probe.wait_state(capture, "target4", predicate, [])
                    self.assertEqual((detail["pulse_phase"], detail["checked_pixels"]), (4, 64000))
                else:
                    with self.assertRaises(TimeoutError):
                        probe.wait_state(capture, "target4", predicate, [])
            capture.key.assert_not_called()
            return clock[0], seen, samples
        old_time, old_seen, _ = attempt(False)
        self.assertGreaterEqual(old_time, 15000)
        self.assertEqual(set(old_seen), {0, 7})
        new_time, _, samples = attempt(True)
        self.assertLess(new_time, 15000)
        self.assertEqual([sample["delay_ms"] for sample in samples[:8]], [17, 43, 79, 131] * 2)
        self.assertTrue(samples[-1]["matched"])
        self.assertEqual((samples[-1]["observed_phase"], samples[-1]["delay_ms"]), (4, 0))

    def test_distinct_sampling_records_excluded_phase_then_full_frame_change(self):
        clock, samples, delays = [0], [], []
        oracle = probe.FullFrameOracle(self.frames)
        sample = probe.phase_sampler(oracle, samples, "native-distinct", excluded=(3,))
        def sleep(seconds):
            delays.append(round(seconds * 1000))
            clock[0] += round(seconds * 1000)
        with patch.object(probe.time, "monotonic", side_effect=lambda: clock[0] / 1000), \
                patch.object(probe.time, "sleep", side_effect=sleep):
            self.assertIsNone(sample(self.frames[3]))
            self.assertIsNone(sample(self.frames[3]))
            result = sample(self.frames[2])
        self.assertEqual(result["checked_pixels"], 64000)
        self.assertEqual([entry["observed_phase"] for entry in samples], [3, 3, 2])
        self.assertEqual([entry["matched"] for entry in samples], [False, False, True])
        self.assertTrue(all(entry["excluded"] == [3] and entry["target"] is None for entry in samples))
        self.assertEqual(delays, [17, 43])
        self.assertEqual([entry["time"] for entry in samples], [0, 0.017, 0.060])

    def test_corrupt_full_frame_is_recorded_none_and_cannot_satisfy_target(self):
        samples = []
        oracle = probe.FullFrameOracle(self.frames)
        sample = probe.phase_sampler(oracle, samples, "target2", target=2)
        with patch.object(probe.time, "sleep") as sleep:
            for point in ((0, 0), (319, 199), (160, 100)):
                image = self.frames[2].convert("RGB")
                old = image.getpixel(point)
                image.putpixel(point, tuple(255 - channel for channel in old))
                self.assertIsNone(sample(image))
        self.assertEqual([entry["observed_phase"] for entry in samples], [None] * 3)
        self.assertFalse(any(entry["matched"] for entry in samples))
        self.assertEqual([entry["delay_ms"] for entry in samples], [17, 43, 79])
        self.assertEqual(sleep.call_count, 3)


def observations(color=20, cycle=0, phases=(3, 4)):
    return {name: {"frames": {phase: Image.new("RGB", (320, 200), (color + phase,) * 3)
                              for phase in phases},
                   "water_palette_cycle_candidates": [cycle]}
            for name in ("f1-escape", "pageup-right", "f1-letter", "actor0-focus-overlays-side-marker")}


class F1PairComparisonTest(unittest.TestCase):
    def test_cycles_may_differ_without_color_normalization_or_false_raw_equivalence(self):
        result = probe.compare_observations(observations(20, 14), observations(30, 0))
        self.assertEqual(len(result), 4)
        for page in result:
            self.assertFalse(page["full_raw_rgb_equivalence"])
            self.assertTrue(page["independent_preopen_cycles_differ"])
            self.assertFalse(page["color_normalization_applied"])
            self.assertFalse(page["raw_rgb_equality_is_acceptance_gate"])
            self.assertEqual([item["bad_pixels"] for item in page["raw_frame_differences"]], [64000, 64000])
            self.assertTrue(page["resource_full_frames_passed_on_each_side"])

    def test_same_cycle_and_same_phase_records_direct_equal_frames(self):
        result = probe.compare_observations(observations(), observations())
        self.assertTrue(all(page["full_raw_rgb_equivalence"] for page in result))
        self.assertTrue(all(item["checked_pixels"] == 64000 and item["exact_rgb_different_pixels"] == 0
                            for page in result for item in page["raw_frame_differences"]))

    def test_nonzero_exact_rgb_difference_is_not_called_raw_equivalence(self):
        result = probe.compare_observations(observations(20, 0), observations(21, 0))
        for page in result:
            self.assertFalse(page["full_raw_rgb_equivalence"])
            self.assertTrue(page["within_vga_tolerance"])
            self.assertTrue(all(item["exact_rgb_different_pixels"] == 64000 and
                                item["max_channel_delta"] == 1 and item["ok"]
                                for item in page["raw_frame_differences"]))
            self.assertFalse(page["raw_rgb_equality_is_acceptance_gate"])
            self.assertTrue(page["resource_full_frames_passed_on_each_side"])

    def test_partial_case_phase_or_palette_never_counts_as_pair(self):
        for mode in ("page", "phase", "cycles"):
            actual = observations()
            if mode == "page": actual.pop("f1-letter")
            if mode == "phase": actual["f1-letter"]["frames"].pop(3)
            if mode == "cycles": actual["f1-letter"]["water_palette_cycle_candidates"] = None
            with self.assertRaises(AssertionError, msg=mode):
                probe.compare_observations(observations(), actual)


class F1PairRunTest(unittest.TestCase):
    def run_model(self, *, sdl_failure=False, same_bytes_rewrite=False, missing_host_close=False):
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
                receipts.append({"kind": kind, "forced_cleanup": False, "cleaned": True})
                try:
                    yield Mock(pid=123)
                finally:
                    live.pop()
                    events.append((kind, "close"))
            def exercise(capture, source, focus, assets, composites, safe, actor, plan, titles,
                         save, data, stamp, report, native):
                self.assertEqual(save.stat().st_mtime_ns, stamp)
                if live == ["sdl"]:
                    self.assertIsNotNone(native)
                else:
                    self.assertIsNone(native)
                if same_bytes_rewrite:
                    os.utime(save, ns=(stamp + 1000000, stamp + 1000000))
                if sdl_failure and live == ["sdl"]:
                    raise TimeoutError("SDL full frame checkpoint missing")
                if not missing_host_close:
                    report["host_close"] = {"ended_before_finally_cleanup": True, "exit_code": 0}
                return observations(color=20 if live == ["original"] else 30,
                                    cycle=14 if live == ["original"] else 0)
            image = Image.new("P", (320, 200))
            prepared = (b"source", (1, 2), {}, [], image, [image] * 8, [image] * 8, {})
            with patch.object(probe, "prepare", return_value=prepared), \
                    patch.object(probe, "title_templates", return_value={}), \
                    patch.object(probe, "Capture"), patch.object(probe, "running", side_effect=running), \
                    patch.object(probe, "exercise", side_effect=exercise), \
                    patch("subprocess.Popen", side_effect=AssertionError("no GUI in model tests")):
                report = probe.run(args)
            self.assertTrue((args.artifacts / "comparison.json").is_file())
            self.assertTrue(report["fixture_unchanged"])
            self.assertTrue(report["original_unchanged"])
            self.assertFalse(live)
            return report, events

    def test_original_then_sdl_pass_independently_frozen_legal_cycles(self):
        report, events = self.run_model()
        self.assertTrue(report["ok"])
        self.assertEqual(events, [("original", "open"), ("original", "close"), ("sdl", "open"), ("sdl", "close")])
        self.assertEqual([case["status"] for case in report["cases"]], ["passed", "passed"])
        self.assertEqual(len(report["paired_frames"]), 4)
        self.assertTrue(all(not page["full_raw_rgb_equivalence"] for page in report["paired_frames"]))
        self.assertTrue(all(not case["full_runtime_fields_measured"] for case in report["cases"]))

    def test_sdl_failure_preserves_original_and_cleans_before_return(self):
        report, events = self.run_model(sdl_failure=True)
        self.assertFalse(report["ok"])
        self.assertEqual([case["status"] for case in report["cases"]], ["passed", "failed"])
        self.assertEqual(events[-1], ("sdl", "close"))
        self.assertIn("SDL full frame checkpoint missing", report["error"])

    def test_identical_bytes_new_mtime_prevents_sdl_start(self):
        report, events = self.run_model(same_bytes_rewrite=True)
        self.assertFalse(report["ok"])
        self.assertEqual(events, [("original", "open"), ("original", "close")])

    def test_cleanup_exit_cannot_substitute_missing_page_host_close(self):
        report, events = self.run_model(missing_host_close=True)
        self.assertFalse(report["ok"])
        self.assertEqual(events, [("original", "open"), ("original", "close")])
        self.assertIn("finally清理前", report["error"])


if __name__ == "__main__":
    unittest.main()
