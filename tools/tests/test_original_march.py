"""March runner的纯资源/状态/生命周期判据；不创建窗口或子进程。"""
from contextlib import contextmanager
import importlib.util
import json
import os
from pathlib import Path
import struct
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import Mock, patch

from PIL import Image

HERE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("original_march", HERE / "run_original_march.py")
march = importlib.util.module_from_spec(spec)
spec.loader.exec_module(march)
from run_graphics_tests import decode_save

PHASES = "field march: anchor=(7,15) turn=1 phase=2\nfield phase: turn=1 phase=1 remaining=0\nfield phase: turn=1 phase=0 remaining=7\nfield phase: turn=2 phase=2 remaining=4\n"


def checksum(data):
    data = bytearray(data)
    data[-4:] = sum(data[:-4]).to_bytes(4, "little")
    return bytes(data)


class MeasurementTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = march.ROOT / "original_game"
        cls.source, cls.focus, cls.steps, cls.precondition = march.prepare(cls.original, cls.original / "START.SAV")

    def measured(self):
        data = bytearray(self.source)
        data[march.META] = 2
        data[march.UNITS:march.UNITS + 2] = bytes((9, 12))
        data[march.UNITS + 2 * 80:march.UNITS + 2 * 80 + 2] = bytes((6, 15))
        data[march.UNITS + 11 * 80:march.UNITS + 11 * 80 + 2] = bytes((5, 15))
        data[march.META + 3:march.META + 9] = bytes((2, 11, 4, 16, 2, 5))
        return checksum(data)

    def measure(self, data):
        return march.measurement(self.source, data, (7, 15), (7, 1), (2, 5))

    def test_prepare_uses_source_identity_anchor_navigation_and_strict_le_oracle(self):
        self.assertEqual([unit["unit_id"] for unit in self.precondition["eligible_players"]], [0, 9, 4, 30])
        self.assertEqual(self.precondition["movement_table_oracle"]["canonical_va"], 0x61646)
        self.assertEqual(self.precondition["movement_table_oracle"]["size"], 580)
        x, y = self.steps[-1][1] if self.steps else self.focus
        world = (self.source[march.META + 3] + (x - 4) // 24, self.source[march.META + 4] + (y - 4) // 24)
        occupied = {tuple(self.source[march.UNITS + i * 80:march.UNITS + i * 80 + 2])
                    for i in range(self.source[march.META + 1]) if not self.source[march.UNITS + i * 80 + 5] & 1}
        self.assertNotIn(world, occupied)
        self.assertNotIn((world[0], world[1] + 1), occupied)

    def test_measurement_derives_camera_and_focus_and_proves_whole_roster(self):
        result = self.measure(self.measured())
        self.assertEqual(result["observed_camera"], [2, 11])
        self.assertEqual(result["measurement_focus"], [4, 16])
        self.assertEqual(len(result["players"]), 4)
        self.assertTrue(result["source_equal_except_declared_runtime_fields_and_measurement_navigation"])
        self.assertIn("whole raw roster0xa00", result["preserved_source_regions"])

    def test_every_unrelated_owner_and_unknown_region_remains_strict(self):
        offsets = (0, 936, 0x8a2, 0x8a3, march.UNITS - 1, march.UNITS + 12 * 80,
                   0x30a3, march.META + 10, march.META + 17, march.META + 18, 0x59c6,
                   march.UNITS + 7, march.UNITS + 0x0b, march.UNITS + 0x20, march.UNITS + 0x39, march.UNITS + 0x44)
        for offset in offsets:
            data = bytearray(self.measured())
            data[offset] ^= 1
            with self.subTest(offset=hex(offset)), self.assertRaises(AssertionError):
                self.measure(checksum(data))

    def test_declared_cache_hp_and_ai_positions_can_differ_without_comparing_rng(self):
        first = self.measure(self.measured())
        data = bytearray(self.measured())
        data[march.UNITS + 2 * 80 + 2] = 7
        data[march.UNITS + 2 * 80 + 0x40] -= 17
        data[march.UNITS + 4 * 80] += 1
        second = self.measure(checksum(data))
        self.assertTrue(march.compare_player_geometry(first, second))
        self.assertEqual([item["field"] for item in second["excluded_active_changes"]], ["cache_class", "hp_low"])
        data[march.UNITS] += 1
        data[march.META + 3] += 1
        data[march.META + 5] += 1
        self.assertFalse(march.compare_player_geometry(first, self.measure(checksum(data))))

    def test_bad_round_counts_acted_direction_phase_hp_or_navigation_are_rejected(self):
        for offset, value in ((march.META, 3), (march.META + 1, 11), (march.META + 9, 5),
                              (march.UNITS + 5, 0x80), (march.UNITS + 3, 1), (march.UNITS + 4, 1),
                              (march.UNITS + 0x40, 255), (march.META + 3, 3)):
            data = bytearray(self.measured())
            data[offset] = value
            with self.subTest(offset=hex(offset)), self.assertRaises(AssertionError):
                self.measure(checksum(data))

    def test_hp0_flags_are_only_allowed_as_known_hidden_finalization(self):
        data = bytearray(self.measured())
        offset = march.UNITS + 11 * 80
        data[offset + 0x40:offset + 0x42] = b"\0\0"
        data[march.UNITS + 2 * 80 + 0x3c] = 42
        with self.assertRaises(AssertionError):
            self.measure(checksum(data))
        data[offset + 5] = 1
        self.measure(checksum(data))
        data[offset + 5] = 3
        with self.assertRaises(AssertionError):
            self.measure(checksum(data))

    def counter_case(self, loss, experience, incoming_hp=31):
        data = bytearray(self.measured())
        player, enemy = march.UNITS + 2 * 80, march.UNITS + 11 * 80
        data[player + 0x3c] = experience
        struct.pack_into("<H", data, player + 0x40, incoming_hp)
        struct.pack_into("<H", data, enemy + 0x40, 28 - loss)
        data[enemy + 5] = int(loss == 28)
        return checksum(data)

    def test_counter_each_exact_single_double_last_miss_pair_and_no_level_up(self):
        legal = [(0, 0), (20, 0), (20, 30), (21, 0), (21, 31),
                 (22, 0), (22, 33), (23, 0), (23, 34), (28, 42)]
        for loss, experience in legal:
            with self.subTest(loss=loss, experience=experience):
                result = self.measure(self.counter_case(loss, experience))["counter_reward"]
                self.assertEqual(result["legal_loss_experience_pairs"], [list(pair) for pair in legal])
                self.assertEqual((result["enemy_hp_loss"], result["experience"]), (loss, experience))
                self.assertFalse(result["level_up_possible"])
                self.assertTrue(result["last_miss_overwrites_contribution_with_zero"])
        for hp in (14, 31, 48):
            self.measure(self.counter_case(20, 30, hp))

    def test_counter_rejects_wrong_reward_actor_level_equipment_and_all_growth_fields(self):
        for loss, experience in ((20, 29), (20, 31), (20, 41), (20, 42), (0, 30), (28, 41), (28, 99)):
            with self.subTest(loss=loss, experience=experience), self.assertRaises(AssertionError):
                self.measure(self.counter_case(loss, experience))
        base = march.UNITS + 2 * 80
        for offset in (march.UNITS + 0x3c, march.UNITS + 80 + 0x3c,
                       base + 0x21, base + 0x0b, base + 0x22, base + 0x26,
                       base + 0x37, base + 0x39, base + 0x3e, base + 0x42,
                       base + 0x44, base + 0x46, base + 0x48, base + 0x1a):
            data = bytearray(self.counter_case(20, 30))
            data[offset] ^= 1
            with self.subTest(offset=hex(offset)), self.assertRaises(AssertionError):
                self.measure(checksum(data))

    def test_counter_rejects_other_damage_sources_nonoriginal_hp_and_changed_pair_geometry(self):
        for offset, value in ((march.UNITS + 10 * 80 + 0x40, 27),
                              (march.UNITS + 80 + 0x40, 27),
                              (march.UNITS + 2 * 80 + 0x40, 30),
                              (march.UNITS + 11 * 80 + 0x40, 27),
                              (march.UNITS + 2 * 80, 7),
                              (march.UNITS + 11 * 80, 4)):
            data = bytearray(self.counter_case(20, 30))
            data[offset] = value
            with self.subTest(offset=hex(offset)), self.assertRaises(AssertionError):
                self.measure(checksum(data))

    def test_counter_terrain_and_source_contract_cannot_be_replaced_by_arbitrary_counter_data(self):
        for relative in (0x21, 0x0b, 0x26):
            source = bytearray(self.source)
            source[march.UNITS + 2 * 80 + relative] ^= 1
            with self.subTest(relative=relative), self.assertRaisesRegex(AssertionError, "exact real START"):
                march.counter_reward(checksum(source), self.counter_case(20, 30), self.original)
        actual = march.archive_entry
        shape = (self.original / "FDSHAP.DAT").read_bytes()
        shape_id = actual((self.original / "FDFIELD.DAT").read_bytes(), 1)[0]

        def changed_terrain(data, index):
            result = actual(data, index)
            if data == shape and index == shape_id * 2 + 1:
                result = bytearray(result)
                result[53 * 4 + 1] = 1
            return result
        with patch.object(march, "archive_entry", side_effect=changed_terrain), self.assertRaisesRegex(AssertionError, "terrain"):
            self.measure(self.counter_case(20, 30))

    def test_legal_counter_does_not_relax_unknown_roster_metadata_or_manual_slot_bytes(self):
        for offset in (0, 936, 0x8a2, 0x8a3, march.UNITS - 1,
                       march.UNITS + 2 * 80 + 0x28, march.UNITS + 12 * 80,
                       0x30a3, march.META + 10, march.META + 17, march.META + 18, 0x312b, 0x59c6):
            data = bytearray(self.counter_case(20, 30))
            data[offset] ^= 1
            with self.subTest(offset=hex(offset)), self.assertRaises(AssertionError):
                self.measure(checksum(data))


class PhaseAndNavigationTest(unittest.TestCase):
    def test_acceptance_and_one_whole_phase_cycle_must_match_anchor(self):
        march.check_march_phases(PHASES, (7, 15))
        lines = PHASES.splitlines()
        for text in ("\n".join(lines[1:]), PHASES + lines[0], "\n".join(reversed(lines)),
                     PHASES.replace("anchor=(7,15)", "anchor=(8,15)"), PHASES + lines[1],
                     PHASES + "field end turn: turn=1 phase=2 remaining=4\n",
                     PHASES + "field phase: turn=3 phase=2 remaining=4\n"):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                march.check_march_phases(text, (7, 15))

    def test_save_navigation_is_bounded_and_avoids_known_actor_candidates(self):
        for leader in ((7, 1), (3, 5), (10, 2)):
            occupied = {leader, (6, 2), (8, 2)}
            target, path = march.save_path(leader, occupied)
            self.assertNotIn(target, occupied)
            self.assertNotIn((target[0], target[1] + 1), occupied)
            self.assertLessEqual(len(path), 20)
            previous = leader
            for key, cell in path:
                self.assertEqual(abs(previous[0] - cell[0]) + abs(previous[1] - cell[1]), 1)
                self.assertNotIn(cell, occupied)
                previous = cell
        with self.assertRaises(ValueError):
            march.save_path((7, 1), {(x, y) for x in range(1, 12) for y in range(1, 7)})

    def test_march_parent_cannot_accept_disabled_or_unselected_march(self):
        assets = march.round_assets(march.ROOT / "original_game")
        oracle = march.RoundOracle(assets, (148, 124))
        frame = Image.new("RGB", (320, 200))
        for item, command in enumerate(march.PARENT_IDS):
            image = assets["commands"][command * 3 + int(item == 1)]
            at = tuple(a + b for a, b in zip(oracle.focus, march.MENU_OFFSETS[item]))
            frame.paste(image.convert("RGB"), at, image.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNotNone(march.parent_menu(frame, oracle, 1))
        disabled = assets["commands"][17]
        frame.paste(disabled.convert("RGB"), (124, 126), disabled.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNone(march.parent_menu(frame, oracle, 1))

    def test_closed_browse_rejects_highlighted_march_and_secondary_icons(self):
        assets = march.round_assets(march.ROOT / "original_game")
        oracle = march.MarchOracle(assets, (148, 124))
        def frame_with_cursor():
            frame = Image.new("RGB", (320, 200))
            image = assets["cursor"]
            frame.paste(image.convert("RGB"), oracle.focus, image.point(lambda value: 255 if value else 0, mode="L"))
            return frame
        for commands, selected in ((march.PARENT_IDS, 1), ((12, 13, 14, 15), 1)):
            frame = frame_with_cursor()
            for item, command in enumerate(commands):
                image = assets["commands"][command * 3 + int(item == selected)]
                at = tuple(a + b for a, b in zip(oracle.focus, march.MENU_OFFSETS[item]))
                frame.paste(image.convert("RGB"), at, image.point(lambda value: 255 if value else 0, mode="L"))
            self.assertIsNone(oracle.closed_browse(frame))
        self.assertIsNotNone(oracle.closed_browse(frame_with_cursor()))

    def test_final_actor0_must_wait_until_round_banner_has_disappeared(self):
        assets = march.round_assets(march.ROOT / "original_game")
        banner = march.round_two_template(march.ROOT / "original_game/FDOTHER.DAT", assets["palette"])
        frame = Image.new("RGB", (320, 200))
        actor = assets["leader_cursors"][0]
        frame.paste(actor.convert("RGB"), (172, 22), actor.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNotNone(march.leader_ready(frame, assets, banner))
        frame.paste(banner.convert("RGB"), (120, 84), banner.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNone(march.leader_ready(frame, assets, banner))


class ExerciseAndCloseTest(unittest.TestCase):
    def exercise_model(self, kind, failing_gate=None, rewrite_gate=None):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"source")
            stamp = save.stat().st_mtime_ns
            capture = Mock(artifacts=Path(directory))
            source = bytearray(march.META + 20)
            source[march.META + 3:march.META + 5] = bytes((5, 11))
            checkpoints, observation = [], {}
            def wait(capture, name, predicate, records, **kwargs):
                checkpoints.append(name)
                if name == rewrite_gate:
                    os.utime(save, ns=(stamp + 1000000, stamp + 1000000))
                if name == failing_gate:
                    raise TimeoutError("missing " + name)
                return Image.new("RGB", (320, 200)), {"focus_screen": [172, 28]}
            with patch.object(march, "wait_title"), patch.object(march, "wait_state", side_effect=wait), \
                    patch.object(march, "visible_actors", return_value=set()), \
                    patch.object(march, "save_path", return_value=((6, 2), [("DOWN", (7, 2)), ("LEFT", (6, 2))])), \
                    patch.object(march, "wait_log", return_value=PHASES) as wait_log, \
                    patch.object(march, "wait_measured_save", return_value=b"measured") as measure, \
                    patch.object(march, "measurement", return_value={"geometry": True}):
                error = None
                try:
                    march.exercise(capture, kind, Path(directory) / "game.log", {}, [], [], source,
                                   (52, 100), (), {3: [None] * 3}, None, save, b"source", stamp, [], observation)
                except (TimeoutError, AssertionError) as failure:
                    error = str(failure)
                return checkpoints, observation, capture.key.call_args_list, wait_log.call_count, measure.call_count, error

    def test_original_banner_and_sdl_ack_phase_are_followed_by_strict_leader_and_one_save(self):
        for kind in ("original", "sdl"):
            checkpoints, observation, keys, log_calls, saves, error = self.exercise_model(kind)
            self.assertIsNone(error)
            self.assertEqual("round-002" in checkpoints, kind == "original")
            self.assertEqual(log_calls, int(kind == "sdl"))
            self.assertEqual(saves, 1)
            self.assertEqual(observation["fixed_world_anchor"], [7, 15])
            self.assertFalse(observation["cancel_full_runtime_fields_measured"])
            self.assertTrue(observation["no_automatic_save"])
            self.assertEqual([call.args[0] for call in keys].count("ESC"), 1)
            self.assertEqual(checkpoints.count("affirmative-default-yes"), 1)
            self.assertIn("save-empty-proven-by-parent", checkpoints)

    def test_missing_empty_parent_ack_does_not_continue_or_blindly_recover(self):
        _, _, keys, _, saves, error = self.exercise_model("sdl", failing_gate="save-empty-proven-by-parent")
        self.assertIn("missing", error)
        self.assertEqual(keys[-1].args, ("ENTER",))
        self.assertEqual([call.args[0] for call in keys].count("ESC"), 1)
        self.assertEqual(saves, 0)

    def test_identical_bytes_rewritten_on_cancel_are_failure_before_affirmative(self):
        checkpoints, _, _, _, saves, error = self.exercise_model("original", rewrite_gate="escape-closed-browse")
        self.assertIsNotNone(error)
        self.assertNotIn("affirmative-default-yes", checkpoints)
        self.assertEqual(saves, 0)

    def test_host_close_must_really_exit0_and_not_rewrite_save(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"measured")
            capture = Mock()
            for code in (1, 0):
                capture.process.wait.return_value = code
                observation = {}
                with patch.object(march, "driver_call"):
                    if code:
                        with self.assertRaises(AssertionError):
                            march.close_host(capture, save, observation)
                        self.assertFalse(observation["normal_host_close"])
                    else:
                        march.close_host(capture, save, observation)
                        self.assertTrue(observation["normal_host_close"])
            stamp = save.stat().st_mtime_ns
            with patch.object(march, "driver_call", side_effect=lambda *args: os.utime(save, ns=(stamp + 1000000, stamp + 1000000))):
                with self.assertRaises(AssertionError):
                    march.close_host(capture, save, {})


class PairRunTest(unittest.TestCase):
    def run_model(self, *, mismatch=False, sdl_failure=False, cleanup_failure=False, late_round=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            resources = root / "original_game"
            resources.mkdir()
            (resources / "START.SAV").write_bytes(b"source")
            for name in ("game.exe", "driver.exe", "dosbox.exe"):
                (root / name).write_bytes(b"binary")
            args = SimpleNamespace(root=root, artifacts=root / "out", save_fixture=None,
                                   game=root / "game.exe", driver=root / "driver.exe", dosbox=root / "dosbox.exe")
            events, live = [], []
            @contextmanager
            def running(command, cwd, env, log, driver, receipts):
                kind = "sdl" if "FD2SDL_SAVE_PATH" in env else "original"
                self.assertFalse(live)
                if kind == "sdl":
                    saved_receipt = json.loads((args.artifacts / "comparison.json").read_text(encoding="utf-8"))
                    self.assertFalse(saved_receipt["ok"])
                    self.assertEqual(saved_receipt["cases"][0]["status"], "passed")
                live.append(kind)
                events.append((kind, "start"))
                log.write_text(PHASES if kind == "sdl" else "", encoding="utf-8")
                try:
                    yield Mock(pid=42)
                finally:
                    live.pop()
                    events.append((kind, "closed"))
                    if kind == "sdl" and late_round:
                        log.write_text(PHASES + "field phase: turn=3 phase=2 remaining=4\n", encoding="utf-8")
                if cleanup_failure:
                    raise RuntimeError("forced cleanup cannot pass")
            geometry = {"anchor": [7, 15], "turn": 2, "stage": 0, "all_acted_cleared": True,
                        "active_count": 12, "roster_count": 4, "players": [{"index": 0, "unit_id": 0, "x": 7, "y": 15}]}
            def exercise(capture, kind, *args):
                if kind == "sdl" and sdl_failure:
                    raise TimeoutError("no enabled March")
                if kind == "sdl" and mismatch:
                    return dict(geometry, players=[dict(geometry["players"][0], x=8)])
                return geometry
            environment = dict(march.os.environ)
            environment.pop("FD2SDL_SAVE_PATH", None)
            with patch.dict(march.os.environ, environment, clear=True), \
                    patch.object(march, "prepare", return_value=(b"source", (52, 100), (), {})), \
                    patch.object(march, "round_assets", return_value={"palette": []}), \
                    patch.object(march, "cursor_assets", return_value=([], [])), \
                    patch.object(march, "round_two_template"), patch.object(march, "title_templates"), \
                    patch.object(march, "Capture"), patch.object(march, "running", side_effect=running), \
                    patch.object(march, "exercise", side_effect=exercise), patch.object(march, "close_host"), \
                    patch("subprocess.Popen", side_effect=AssertionError("model cannot create processes")):
                report = march.run(args)
            self.assertTrue((args.artifacts / "comparison.json").is_file())
            self.assertTrue(report["fixture_unchanged"])
            self.assertTrue(report["original_unchanged"])
            return report, events

    def test_original_then_sdl_are_serial_and_require_matching_measured_geometry(self):
        report, events = self.run_model()
        self.assertTrue(report["ok"])
        self.assertEqual(events, [("original", "start"), ("original", "closed"), ("sdl", "start"), ("sdl", "closed")])
        report, _ = self.run_model(mismatch=True)
        self.assertFalse(report["ok"])
        self.assertEqual(report["cases"][-1]["status"], "failed")

    def test_original_measurement_survives_sdl_failure_and_still_closes(self):
        report, events = self.run_model(sdl_failure=True)
        self.assertFalse(report["ok"])
        self.assertEqual(events[-1], ("sdl", "closed"))
        self.assertEqual([case["status"] for case in report["cases"]], ["passed", "failed"])

    def test_extra_round_after_save_is_rejected_by_final_complete_log(self):
        report, _ = self.run_model(late_round=True)
        self.assertFalse(report["ok"])
        self.assertEqual(report["cases"][-1]["status"], "failed")

    def test_forced_cleanup_is_not_success_and_prevents_second_start(self):
        report, events = self.run_model(cleanup_failure=True)
        self.assertFalse(report["ok"])
        self.assertEqual(events, [("original", "start"), ("original", "closed")])


if __name__ == "__main__":
    unittest.main()
