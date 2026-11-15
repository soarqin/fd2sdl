"""Load恢复的字段allowlist、观察边界和真实导航序列模型；无窗口无子进程。"""
from pathlib import Path
import json
import os
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_load as load
from run_graphics_tests import decode_save


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


class ActiveLoadCacheMeasurementTest(unittest.TestCase):
    """完整原10010/11019的独立literal向量；不是自然游戏前提证明。"""
    def setUp(self):
        # 身份在+7；+4移动相位特设为不同值，防止字段错位自证。
        identities = (7, 0, 7, 68, 68, 5, 0, 121, 121, 3, 8, 4)
        self.expected_classes = (0, 1, 0, 2, 2, 3, 1, 4, 4, 5, 6, 7)
        data = bytearray(source_snapshot())
        for raw, identity in enumerate(identities):
            base = load.UNITS + raw * load.STRIDE
            data[base + 7] = identity
            data[base + 2] = (raw + 3) % 8
            data[base + 4] = (raw + 1) % 6
        data[load.UNITS + 5] = 1  # 首个hidden身份仍占cache class0。
        self.source = checksum(data)
        for raw, expected in enumerate(self.expected_classes):
            data[load.UNITS + raw * load.STRIDE + 2] = expected
        self.measured = checksum(data)

    def test_literal_first_identity_order_including_hidden_is_verified(self):
        report = load.check_active_load_measurement(self.source, self.measured, ())
        self.assertTrue(report['cache_classes_original_10010_verified'])
        self.assertNotIn('cache_class_changes_excluded', report)
        self.assertTrue(report['cache_class_rewrites_verified'])
        self.assertEqual(report['measurement_camera_focus'], [5, 11, 8, 15, 3, 4])

    def test_retaining_saved_classes_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, 'original10010'):
            load.check_active_load_measurement(self.source, self.source, ())

    def test_arbitrary_measured_class_cannot_be_an_oracle(self):
        for raw in range(12):
            bad = bytearray(self.measured)
            bad[load.UNITS + raw * load.STRIDE + 2] ^= 1
            with self.subTest(raw=raw), self.assertRaisesRegex(AssertionError, 'original10010'):
                load.check_active_load_measurement(self.source, checksum(bad), ())

    def test_measured_identity_or_inactive_tail_change_still_fails(self):
        for position in (load.UNITS + 7, load.UNITS + 12 * load.STRIDE + 2):
            bad = bytearray(self.measured)
            bad[position] ^= 1
            with self.subTest(position=position), self.assertRaisesRegex(AssertionError, 'outside the declared'):
                load.check_active_load_measurement(self.source, checksum(bad), ())

    def test_zero_and_full_disk_capacity_use_same_original_loop(self):
        for count in (0, 96):
            source = bytearray(self.source)
            source[load.META + 1] = count
            for raw in range(count):
                source[load.UNITS + raw * load.STRIDE + 7] = raw % 7
                source[load.UNITS + raw * load.STRIDE + 2] = 6
            measured = bytearray(source)
            for raw in range(count):
                measured[load.UNITS + raw * load.STRIDE + 2] = raw % 7
            with self.subTest(count=count):
                report = load.check_active_load_measurement(checksum(source), checksum(measured), ())
                self.assertTrue(report['cache_classes_original_10010_verified'])

    def test_outside_disk_capacity_is_not_parsed_as_actor_records(self):
        source = bytearray(self.source)
        source[load.META + 1] = 97
        with self.assertRaisesRegex(AssertionError, '96-record'):
            load.check_active_load_measurement(checksum(source), self.measured, ())


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
            cycle = {"actors": [{"raw_actor": i} for i in range(4)],
                     "returning": [("RIGHT", (76, 76)), ("DOWN", (76, 100))]}
            with patch.object(load, "wait_title") as startup, \
                    patch.object(load, "wait_state", return_value=(self.image, {"focus_screen": [148, 28]})) as state, \
                    patch.object(load, "wait_measured_save", return_value=b"measured plaintext") as measure, \
                    patch.object(load, "require_source_unchanged", wraps=load.require_source_unchanged) as guard:
                measured = load.exercise(capture, "original", {}, (76, 100), (("LEFT", (52, 100)),),
                                         {3: [self.image] * 3}, self.image, save, b"source A", Mock(), [], observation,
                                         source_mtime=source_mtime, focus_cycle=cycle)
            self.assertEqual(measured, b"measured plaintext")
            self.assertTrue(observation["round2_reached_without_save"])
            self.assertTrue(observation["load_disk_unchanged_before_measurement"])
            self.assertTrue(observation["explicit_measurement_save_completed"])
            self.assertEqual(observation["source_mtime_ns_before_measurement"], source_mtime)
            self.assertEqual(guard.call_count, 13)
            self.assertTrue(all(call.args[2] == source_mtime for call in guard.call_args_list))
            self.assertEqual([entry["branch"] for entry in observation["cancellations"]], ["escape", "no"])
            self.assertTrue(all(not entry["full_runtime_fields_measured"] for entry in observation["cancellations"]))
            open_load = ["ENTER", "UP", "ENTER", "RIGHT", "ENTER"]
            expected = (["UP", "ENTER", "LEFT", "ENTER", "DOWN", "ENTER", "ENTER"] + ["Z"] * 5 + ["LEFT"] +
                        open_load + ["ESC"] + open_load + ["RIGHT", "ENTER"] + open_load +
                        ["ENTER", "Z", "RIGHT", "DOWN", "LEFT", "ENTER", "UP", "ENTER", "LEFT", "ENTER", "ENTER"])
            self.assertEqual([call.args[0] for call in capture.key.call_args_list], expected)
            names = [call.args[1] for call in state.call_args_list]
            self.assertLess(names.index("original-loaded-source-initial-browse"),
                            names.index("original-measurement-empty-step-1"))
            cycle_names = [f"original-round2-focus-cycle-{i}-raw-{actor}"
                           for i, actor in enumerate((0, 1, 2, 3, 0), 1)]
            for name, raw in zip(cycle_names + ["original-loaded-focus-cycle-raw-0"], (0, 1, 2, 3, 0, 0)):
                gate = next(call.args[2] for call in state.call_args_list if call.args[1] == name)
                with patch.object(load, "focused_actor", return_value=None) as focused:
                    gate(self.image)
                focused.assert_called_once_with(self.image, {}, cycle["actors"][raw])
            self.assertLess(names.index(cycle_names[-1]), names.index("original-round2-empty-cell"))
            self.assertLess(names.index("original-loaded-focus-cycle-raw-0"),
                            names.index("original-loaded-source-focus-restored"))
            self.assertLess(names.index("original-loaded-source-focus-restored"),
                            names.index("original-measurement-empty-step-1"))
            leader_gate = next(call.args[2] for call in state.call_args_list if call.args[1] == "original-round2-leader")
            with patch.object(load, "browse_anywhere", return_value=None) as browse:
                leader_gate(self.image)
            browse.assert_called_once_with(self.image, {}, leader=True)
            measure.assert_called_once()
            startup.assert_called_once()
            capture.wait.assert_called_once()  # Load之后不会再调用startup wait或补Esc。

    def test_load_write_set_scope_has_no_z_or_unverified_return_navigation(self):
        with tempfile.TemporaryDirectory() as directory:
            save=Path(directory)/'FD2.SAV';save.write_bytes(b'source A');stamp=save.stat().st_mtime_ns
            capture=Mock();capture.process.poll.return_value=None;observation={}
            with patch.object(load,'wait_title'),\
                    patch.object(load,'wait_state',return_value=(self.image,{'focus_screen':[148,28]})) as state,\
                    patch.object(load,'wait_measured_save',return_value=b'measured plaintext') as measure,\
                    patch.object(load,'require_source_unchanged',wraps=load.require_source_unchanged) as guard:
                result=load.exercise(capture,'original',{},(76,100),(("LEFT",(52,100)),),
                    {3:[self.image]*3},self.image,save,b'source A',Mock(),[],observation,
                    source_mtime=stamp,focus_cycle=None)
            self.assertEqual(result,b'measured plaintext');self.assertFalse(observation['focus_cycle_executed'])
            self.assertEqual(observation['focus_cycle_before_load'],[])
            self.assertNotIn('focus_cycle_after_load',observation)
            self.assertEqual([x['branch'] for x in observation['cancellations']],['escape','no'])
            open_load=['ENTER','UP','ENTER','RIGHT','ENTER']
            expected=(['UP','ENTER','LEFT','ENTER','DOWN','ENTER','ENTER','LEFT']+
                open_load+['ESC']+open_load+['RIGHT','ENTER']+open_load+
                ['ENTER','LEFT','ENTER','UP','ENTER','LEFT','ENTER','ENTER'])
            self.assertEqual([c.args[0] for c in capture.key.call_args_list],expected)
            self.assertEqual(guard.call_count,5);measure.assert_called_once()
            names=[c.args[1] for c in state.call_args_list]
            self.assertFalse(any('focus-cycle' in name for name in names))
            self.assertLess(names.index('original-loaded-source-focus-restored'),names.index('original-measurement-empty-step-1'))

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
                              self.image, Mock(), b"source A", Mock(), [], {}, source_mtime=1,
                              focus_cycle={"actors": [], "returning": []})
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                         ["UP", "ENTER", "ENTER", "DOWN", "ENTER", "ENTER"])
        measure.assert_not_called()

    def test_missing_actor_ack_stops_before_next_z_or_measurement(self):
        for failed in ("round2-focus-cycle-2-raw-1", "loaded-focus-cycle-raw-0"):
            with self.subTest(failed=failed):
                capture = Mock()
                capture.process.poll.return_value = None
                save = Mock()
                save.read_bytes.return_value = b"source A"
                save.stat.return_value = SimpleNamespace(st_mtime_ns=1)
                cycle = {"actors": [{"raw_actor": i} for i in range(4)], "returning": []}
                def wait(capture, name, predicate, checkpoints, **kwargs):
                    if name.endswith(failed):
                        raise TimeoutError("expected actor not observed")
                    return self.image, {"focus_screen": [148, 28]}
                with patch.object(load, "wait_title"), patch.object(load, "wait_state", side_effect=wait), \
                        patch.object(load, "wait_measured_save") as measure:
                    with self.assertRaisesRegex(TimeoutError, "actor not observed"):
                        load.exercise(capture, "original", {}, (76, 100), (), {3: [self.image] * 3},
                                      self.image, save, b"source A", Mock(), [], {}, source_mtime=1,
                                      focus_cycle=cycle)
                keys = [call.args[0] for call in capture.key.call_args_list]
                self.assertEqual(keys[-1], "Z")
                self.assertEqual(keys.count("Z"), 2 if failed.startswith("round2") else 6)
                measure.assert_not_called()


class LoadLateGuardTest(unittest.TestCase):
    def fixture(self, root):
        original=root/'original_game';original.mkdir()
        (original/'FDFIELD.DAT').write_bytes(b'metadata')
        for name in ('game.exe','driver.exe','dosbox.exe','input.SAV'):
            (root/name).write_bytes(name.encode())
        return SimpleNamespace(root=root,artifacts=root/'out',game=root/'game.exe',
            driver=root/'driver.exe',dosbox=root/'dosbox.exe',save_fixture=root/'input.SAV')

    def test_business_error_survives_both_late_identity_read_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));read=Path.read_bytes;fixture_reads=0
            def observe(path):
                nonlocal fixture_reads
                if path==args.save_fixture:
                    fixture_reads+=1
                    if fixture_reads>1:raise OSError('late fixture guard')
                return read(path)
            with patch.object(load,'hashes',side_effect=[{'EXE':'before'},OSError('late original guard')]),\
                    patch.object(Path,'read_bytes',autospec=True,side_effect=observe),\
                    patch.object(load,'archive_entry',side_effect=AssertionError('business mismatch')),\
                    patch.object(load,'running') as running:
                report=load.run(args)
            self.assertFalse(report['ok']);self.assertEqual(report['error'],'business mismatch')
            self.assertEqual(report['guard_errors'],[
                {'guard':'original','text':'late original guard'},
                {'guard':'fixture','text':'late fixture guard'}])
            self.assertFalse(report['original_unchanged']);self.assertFalse(report['fixture_unchanged'])
            self.assertEqual(json.loads((args.artifacts/'comparison.json').read_text()),report)
            running.assert_not_called()

    def test_report_write_error_preserves_business_error_for_outer_owner(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(load,'hashes',return_value={'EXE':'before'}),\
                    patch.object(load,'archive_entry',side_effect=AssertionError('business mismatch')),\
                    patch.object(Path,'write_text',side_effect=OSError('report disk failure')),\
                    patch.object(load,'running') as running:
                with self.assertRaisesRegex(RuntimeError,'business mismatch; report_write_error=report disk failure'):
                    load.run(args)
            running.assert_not_called()


class LoadFocusCycleOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = next(parent for parent in Path(__file__).resolve().parents
                        if (parent / "original_game/START.SAV").is_file())
        cls.original = cls.root / "original_game"
        cls.source = decode_save(cls.original / "START.SAV")
        cls.assets = load.round_assets(cls.original)
        cls.cycle = load.focus_cycle_assets(cls.original, cls.source, cls.assets)

    def frame(self, actor=0, phase=0, position=(76, 76)):
        frame = Image.new("RGB", (320, 200))
        template = self.cycle["actors"][actor]["templates"][phase]
        frame.paste(template.convert("RGB"), (position[0], position[1] - 6),
                    template.point(lambda value: 255 if value else 0, mode="L"))
        return frame

    def test_real_start_all_four_identities_require_their_own_cursor_sprite(self):
        self.assertEqual([actor["raw_actor"] for actor in self.cycle["actors"]], [0, 1, 2, 3])
        self.assertEqual([actor["unit_id"] for actor in self.cycle["actors"]],
                         [self.source[load.UNITS + raw * load.STRIDE + 7] for raw in range(4)])
        for raw in range(4):
            actor = self.cycle["actors"][raw]
            self.assertTrue(all(not any(224 <= value < 240 for value in image.getdata())
                                for image in actor["templates"]))
            for phase in range(12):
                with self.subTest(raw=raw, phase=phase):
                    result = load.focused_actor(self.frame(raw, phase), self.assets, actor)
                    self.assertEqual((result["raw_actor"], result["unit_id"], result["focus_screen"]),
                                     (raw, actor["unit_id"], [76, 76]))
            self.assertIsNone(load.focused_actor(self.frame((raw + 1) % 4), self.assets, actor))

    def test_bare_cursor_unfocused_actor_or_ambiguous_duplicate_are_rejected(self):
        from run_original_player import cursor_assets
        actor = self.cycle["actors"][0]
        bare = Image.new("RGB", (320, 200))
        cursor = self.assets["cursor"]
        bare.paste(cursor.convert("RGB"), (76, 76), cursor.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNone(load.focused_actor(bare, self.assets, actor))
        isolated = bytearray(self.source)
        isolated[load.META + 1] = 1
        sprites, _ = cursor_assets(self.original / "FDICON.B24", isolated, self.assets)
        for sprite in sprites:
            frame = Image.new("RGB", (320, 200))
            frame.paste(sprite.convert("RGB"), (76, 70), sprite.point(lambda value: 255 if value else 0, mode="L"))
            self.assertIsNone(load.focused_actor(frame, self.assets, actor))
        frame = self.frame()
        second = self.frame(position=(124, 76))
        frame.paste(second.crop((124, 70, 148, 94)), (124, 70))
        self.assertIsNone(load.focused_actor(frame, self.assets, actor))

    def test_dialog_gate_and_lower_actor_occlusion_are_not_relaxed(self):
        actor = self.cycle["actors"][0]
        frame = self.frame()
        frame.paste((255, 0, 0), (76, 94, 100, 118))  # 下格sprite从cursor+18开始。
        self.assertIsNotNone(load.focused_actor(frame, self.assets, actor))
        border = self.assets["dialog_top"]
        frame.paste(border.convert("RGB"), (5, 112), border.point(lambda value: 255 if value else 0, mode="L"))
        self.assertIsNone(load.focused_actor(frame, self.assets, actor))

    def test_return_path_is_bounded_and_derived_from_source_not_captured_coordinates(self):
        x, y = self.cycle["actors"][0]["world"]
        cx, cy, fx, fy = self.source[load.META + 3:load.META + 7]
        for key, (sx, sy) in self.cycle["returning"]:
            dx, dy = {"LEFT": (-1, 0), "RIGHT": (1, 0), "UP": (0, -1), "DOWN": (0, 1)}[key]
            x, y = x + dx, y + dy
            self.assertEqual((sx, sy), (4 + (x-cx)*24, 4 + (y-cy)*24))
        self.assertEqual((x, y), (fx, fy))
        self.assertLessEqual(len(self.cycle["returning"]), 20)
        bad = bytearray(self.source)
        bad[load.META + 5] = cx + 1
        with self.assertRaisesRegex(ValueError, "scrolling"):
            load.focus_cycle_assets(self.original, bad, self.assets)
        bad = bytearray(self.source)
        bad[load.UNITS + 7] = 140
        with self.assertRaisesRegex(ValueError, "outside FDICON"):
            load.focus_cycle_assets(self.original, bad, self.assets)
        bad = bytearray(self.source)
        bad[load.UNITS + load.STRIDE + 7] = bad[load.UNITS + 7]
        with self.assertRaisesRegex(ValueError, "distinct resource"):
            load.focus_cycle_assets(self.original, bad, self.assets)


if __name__ == "__main__":
    unittest.main()
