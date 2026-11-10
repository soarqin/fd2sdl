import hashlib
import os
from pathlib import Path
import sys
import tempfile
from contextlib import contextmanager
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_reward as runner

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = (ROOT / "original_game").resolve()


class RewardProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = {name: runner.prepare(ORIGINAL, name) for name in runner.SCENARIOS}

    def exercise(self, scenario, fail=None, role="original", phase_text=None):
        capture = SimpleNamespace(key=Mock(), wait=Mock(), process=SimpleNamespace(pid=123))
        report = {"kind": role, "checkpoints": []}
        seen = []
        phase_reads = []
        def wait(capture, name, predicate, records, **kwargs):
            seen.append((name, capture.key.call_count))
            if name == fail: raise TimeoutError("resource missing")
            if name == "b4-native-round002":
                self.assertIsNot(records, report["checkpoints"])
            if name in ("b4-load-secondary", "b4-load-selected"):
                selected = 0 if name == "b4-load-secondary" else 2
                image = Image.new("RGB", (320, 200))
                for item, frame_id in enumerate((37 if selected == 0 else 36, 41, 43 if selected == 2 else 42, 45)):
                    offset = runner.RoundOracle(self.prepared[scenario]["assets"], (28, 76))
                    from run_original_battle_menu import MENU_OFFSETS
                    at = tuple(a + b for a, b in zip(offset.focus, MENU_OFFSETS[item]))
                    image.paste(self.prepared[scenario]["assets"]["commands"][frame_id].convert("RGB"), at)
                self.assertIsNotNone(predicate(image), "WAIT后Save41必须被完整资源验证")
            return Image.new("RGB", (320, 200)), {"kind": "model"}
        source = self.prepared[scenario]["model"]["final"]
        if phase_text is None:
            phase_text = "\n".join(("field end turn: turn=1 phase=2 remaining=3", "field phase: turn=1 phase=1 remaining=0",
                                    "field phase: turn=1 phase=0 remaining=6", "field phase: turn=2 phase=2 remaining=4"))
        def phase_log(*args, **kwargs):
            phase_reads.append([name for name, _ in seen])
            return phase_text
        def active_load(capture, prepared, saved, save, guard, report):
            # 此层只验reward业务的调用顺序；连续WGC/原Round判据另在下类真测。
            capture.key("ENTER")
            wait(capture, "b4-load-round", lambda frame: None, report["checkpoints"])
            wait(capture, "b4-loaded-browse", lambda frame: None, report["checkpoints"])
        with tempfile.TemporaryDirectory() as temp:
            capture.artifacts = Path(temp)
            save = Path(temp) / "FD2.SAV"; save.write_bytes(b"isolated")
            guard = [b"isolated", save.stat().st_mtime_ns]
            with patch.object(runner, "wait_state", side_effect=wait), patch.object(runner, "wait_title"), \
                    patch.object(runner, "require_source_unchanged"), patch.object(runner, "save_measure", return_value=source), \
                    patch.object(runner, "observe_active_load", side_effect=active_load), \
                    patch("field_play_input_test.wait_log", side_effect=phase_log):
                try:
                    runner.exercise(capture, self.prepared[scenario], save, guard, report)
                except TimeoutError:
                    if fail is None: raise
        self.phase_reads = phase_reads
        return capture, report, seen

    def test_all_six_have_one_end_and_actual_wait_load_restore_protocol(self):
        for scenario in runner.SCENARIOS:
            with self.subTest(scenario=scenario):
                capture, report, seen = self.exercise(scenario)
                names = [name for name, _ in seen]
                self.assertEqual(names.count("b4-end-default-yes"), 1)
                self.assertEqual(names.count("b4-load-default-yes"), 1)
                self.assertLess(names.index("b4-before-save-detail"), names.index("b4-wait-mutated"))
                self.assertLess(names.index("b4-wait-mutated"), names.index("b4-load-default-yes"))
                self.assertLess(names.index("b4-load-default-yes"), names.index("b4-load-round"))
                self.assertLess(names.index("b4-load-round"), names.index("b4-loaded-browse"))
                self.assertLess(names.index("b4-load-default-yes"), names.index("b4-after-load-detail"))
                self.assertTrue(report["save_load"]["real_wait_before_load"])
                self.assertTrue(report["save_load"]["whole_plaintext_equal"])
                actual = report["reward_acknowledgements"]
                self.assertEqual(sum(item["key_sent"] is not None for item in actual),
                                 sum(page["requires_key"] for page in self.prepared[scenario]["pages"]))
                if scenario not in ("counter-multi", "status"):
                    self.assertEqual(report["action_confirmations"], 1)
                self.assertNotIn("SPACE", [call.args[0] for call in capture.key.call_args_list])

    def test_first_missing_growth_arrow_stops_without_recovery_or_more_ack(self):
        name = "b4-message-02-490"
        capture, report, seen = self.exercise("learn", fail=name)
        self.assertEqual(seen[-1][0], name)
        self.assertEqual(capture.key.call_count, seen[-1][1])
        self.assertEqual([item["fragment"] for item in report["reward_acknowledgements"]], [488, 489])
        self.assertNotIn("F2", [call.args[0] for call in capture.key.call_args_list])

    def test_physical_cycles_cancel_and_reopen_before_the_only_attack_confirmation(self):
        for scenario in ("physical-cap", "physical-overwrite"):
            for role in ("original", "sdl"):
                with self.subTest(scenario=scenario, role=role):
                    capture, report, seen = self.exercise(scenario, role=role)
                    steps = [(name, count) for name, count in seen if name in (
                        "b4-target", "b4-target-cycle-z", "b4-target-cycle-kp5",
                        "b4-target-cancel-command", "b4-target-reopened", "b4-message-00-488")]
                    self.assertEqual([name for name, _ in steps], ["b4-target", "b4-target-cycle-z",
                        "b4-target-cycle-kp5", "b4-target-cancel-command", "b4-target-reopened", "b4-message-00-488"])
                    first = steps[0][1]
                    self.assertEqual([count - first for _, count in steps], [0, 1, 2, 3, 4, 5])
                    keys = [call.args[0] for call in capture.key.call_args_list]
                    self.assertEqual(keys[first:first + 5], ["Z", "KP5", "ESC", "ENTER", "ENTER"])
                    self.assertEqual(report["action_confirmations"], 1)
                    self.assertEqual(report["physical_target_controls"]["target_cancel_count"], 1)

    def test_missing_cycle_ack_never_sends_keypad_cancel_or_attack(self):
        capture, report, seen = self.exercise("physical-cap", fail="b4-target-cycle-z")
        self.assertEqual(seen[-1][0], "b4-target-cycle-z")
        self.assertEqual(capture.key.call_count, seen[-1][1])
        self.assertNotIn("KP5", [call.args[0] for call in capture.key.call_args_list])
        self.assertNotIn("action_confirmations", report)

    def test_learning_does_not_disable_known_spells_and_only_sdl_refuses17(self):
        for role in ("original", "sdl"):
            with self.subTest(role=role):
                _, report, seen = self.exercise("learn", role=role)
                names = [name for name, _ in seen]
                self.assertIn("b4-learned13-target", names)
                self.assertIn("b4-learned13-cancel-list0", names)
                refused = [name for name in names if name.startswith("b4-unsupported17-refused-")]
                self.assertEqual(refused, ["b4-unsupported17-refused-enter", "b4-unsupported17-refused-space"] if role == "sdl" else [])
                self.assertFalse(report["learned_spell_support"]["native_spell17_confirmation_sent"])
                self.assertFalse(report["learned_spell_support"]["spell17_effect_verified"])

    def test_native_banner_is_endpoint_only_and_sdl_uses_ordered_log_after_messages(self):
        for scenario in runner.SCENARIOS:
            for role in ("original", "sdl"):
                with self.subTest(scenario=scenario, role=role):
                    _, report, seen = self.exercise(scenario, role=role)
                    names = [name for name, _ in seen]
                    self.assertNotIn("b4-round002", names)
                    self.assertEqual("native_round_banner" in report, role == "original" and scenario != "status")
                    if role == "sdl":
                        self.assertNotIn("b4-native-round002", names)
                        self.assertEqual(len(self.phase_reads), 1)
                        messages = [name for name in self.phase_reads[0] if name.startswith("b4-message-")]
                        self.assertEqual(len(messages), len(self.prepared[scenario]["pages"]))
                        self.assertFalse(report["sdl_phase_log"]["banner_image_compared"])
                    else:
                        self.assertFalse(self.phase_reads)

    def test_bad_sdl_phase_log_stops_before_turn2_detail_or_save(self):
        invalid = "\n".join(("field end turn: turn=1 phase=2 remaining=3", "field phase: turn=1 phase=0 remaining=6",
                              "field phase: turn=2 phase=2 remaining=4"))
        with self.assertRaisesRegex(AssertionError, "missing or repeated"):
            self.exercise("physical-cap", role="sdl", phase_text=invalid)
        extra = invalid + "\nfield phase: turn=1 phase=1 remaining=0"
        with self.assertRaisesRegex(AssertionError, "out of order"):
            self.exercise("status", role="sdl", phase_text=extra)

    def test_preflight_all_resources_without_processes_and_exact_new_magic(self):
        self.assertEqual(len(self.prepared["counter-multi"]["pages"]), 31)
        self.assertEqual(sum(p["requires_key"] for p in self.prepared["counter-multi"]["pages"]), 30)
        self.assertEqual(self.prepared["learn"]["final_magic"][1].size, (310, 99))
        self.assertEqual([p["fragment"] for p in self.prepared["status"]["pages"]], [487, 481, 482, 483, 484, 485, 486])

    def test_missing_sdl_and_resolved_original_output_rejected_before_mkdir(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "not-created"
            args = SimpleNamespace(root=ROOT, artifacts=output, sdl=Path(temp) / "missing.exe", scenario="learn")
            with patch.object(runner, "prepare") as prepare:
                with self.assertRaisesRegex(ValueError, "--sdl"):
                    runner.run(args)
                prepare.assert_not_called()
            self.assertFalse(output.exists())
            args.sdl = None; args.artifacts = ORIGINAL / "must-not-be-created"
            with patch.object(runner, "prepare") as prepare:
                with self.assertRaisesRegex(ValueError, "original_game"):
                    runner.run(args)
                prepare.assert_not_called()
            self.assertFalse(args.artifacts.exists())

    def test_junction_alias_is_resolved_before_output_mkdir(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "candidate"
            output = ORIGINAL / "must-not-be-created"
            resolve = Path.resolve
            def alias(path, *args, **kwargs):
                return ORIGINAL if path == root / "original_game" else resolve(path, *args, **kwargs)
            args = SimpleNamespace(root=root, artifacts=output, sdl=None, scenario="learn")
            with patch.object(Path, "resolve", alias), patch.object(runner, "prepare") as prepare:
                with self.assertRaisesRegex(ValueError, "original_game"):
                    runner.run(args)
                prepare.assert_not_called()
            self.assertFalse(output.exists())


class RewardFinalGuardTest(unittest.TestCase):
    def test_late_mtime_failure_is_sticky_and_completed_snapshot_hash_is_required(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            fixture, save, snapshot = [base / name for name in ("fixture", "save", "snapshot")]
            for path in (fixture, save, snapshot): path.write_bytes(b"stable")
            record = {"status": "passed", "probe_cases_passed": True,
                "completed_save": {"snapshot": str(snapshot), "sha256": hashlib.sha256(b"stable").hexdigest()},
                "processes": [{"cleaned": True, "forced_cleanup": False, "exit_code": 0}]}
            stamp, before = save.stat().st_mtime_ns, fixture.stat().st_mtime_ns
            with patch.object(runner, "hashes", return_value={"source": "stable"}):
                runner.endpoint_guard(record, base, {"source": "stable"}, fixture, b"stable", before, save, [b"stable", stamp])
                self.assertTrue(record["probe_cases_passed"])
                os.utime(save, ns=(save.stat().st_atime_ns, stamp + 2_000_000_000))
                runner.endpoint_guard(record, base, {"source": "stable"}, fixture, b"stable", before, save, [b"stable", stamp])
                self.assertFalse(record["probe_cases_passed"])
                first_error = record["error"]
                os.utime(save, ns=(save.stat().st_atime_ns, stamp))
                runner.endpoint_guard(record, base, {"source": "stable"}, fixture, b"stable", before, save, [b"stable", stamp])
                self.assertFalse(record["probe_cases_passed"])
                self.assertEqual(first_error, record["error"])
                record["error"] = "earlier business failure"
                record["probe_cases_passed"] = True; record["copy_save_matches_completed_state"] = True
                snapshot.write_bytes(b"changed")
                runner.endpoint_guard(record, base, {"source": "stable"}, fixture, b"stable", before, save, [b"stable", stamp])
                self.assertFalse(record["probe_cases_passed"])
                self.assertEqual(record["error"], "earlier business failure")

    def run_model(self, temp, *, paired, fail_role=None, kind="pair"):
        output = Path(temp) / "output"
        args = SimpleNamespace(root=ROOT, artifacts=output, sdl=Path(__file__) if paired else None,
            scenario="heal", driver=Path(__file__), dosbox=Path(__file__), kind=kind)
        starts, hashes_calls = [], [0]
        @contextmanager
        def running(command, cwd, env, log, driver, processes):
            role = "original" if "-conf" in command else "sdl"
            starts.append(role)
            receipt = {"cleaned": False, "forced_cleanup": False, "exit_code": None}
            processes.append(receipt)
            try: yield SimpleNamespace(pid=123)
            finally: receipt.update(cleaned=True, exit_code=0)
        def copytree(source, target): target.mkdir()
        def capture(driver, process, work, inputs): return SimpleNamespace(artifacts=work)
        def exercise(capture, prepared, save, guard, report):
            snapshot = capture.artifacts / "measured.SAV"
            snapshot.write_bytes(save.read_bytes())
            report["completed_save"] = {"snapshot": str(snapshot), "sha256": hashlib.sha256(snapshot.read_bytes()).hexdigest(),
                                        "mtime_ns": save.stat().st_mtime_ns}
            report["save_load"] = {"whole_plaintext_equal": True, "real_wait_before_load": True, "both_snapshots_validated": True}
        def hashes(path):
            hashes_calls[0] += 1
            at = 2 if fail_role == "original" else 3
            if fail_role and hashes_calls[0] == at:
                save = output / fail_role / ("game/FD2.SAV" if fail_role == "original" else "FD2.SAV")
                stamp = save.stat()
                os.utime(save, ns=(stamp.st_atime_ns, stamp.st_mtime_ns + 2_000_000_000))
            return {"source": "stable"}
        with patch.object(runner, "running", running), patch.object(runner.shutil, "copytree", copytree), \
                patch.object(runner, "Capture", capture), patch.object(runner, "exercise", exercise), \
                patch.object(runner, "hashes", hashes), patch.object(runner, "compare_endpoints", return_value={"model": True}):
            report = runner.run(args)
        return report, starts, output

    def test_original_only_is_probe_and_serial_pair_only_passes_both(self):
        for paired in (False, True):
            with self.subTest(paired=paired), tempfile.TemporaryDirectory() as temp:
                report, starts, _ = self.run_model(temp, paired=paired)
                self.assertTrue(report["probe_cases_passed"])
                self.assertEqual(report["ok"], paired)
                self.assertEqual(report["probe_only"], not paired)
                self.assertEqual(starts, ["original", "sdl"] if paired else ["original"])

    def test_single_sdl_is_new_endpoint_without_inheriting_original(self):
        with tempfile.TemporaryDirectory() as temp:
            report, starts, output = self.run_model(temp, paired=True, kind="sdl")
            self.assertEqual(starts, ["sdl"])
            self.assertEqual(report["requested_endpoints"], ["sdl"])
            self.assertEqual(report["status"], "endpoint_passed")
            self.assertTrue(report["probe_only"] and report["probe_cases_passed"])
            self.assertFalse(report["ok"] or report["paired_result"])
            self.assertNotIn("paired", report)
            self.assertFalse((output / "original").exists())

    def test_single_sdl_late_save_failure_cannot_pass_endpoint(self):
        with tempfile.TemporaryDirectory() as temp:
            report, starts, _ = self.run_model(temp, paired=True, kind="sdl", fail_role="sdl")
            self.assertEqual(starts, ["sdl"])
            self.assertFalse(report["ok"] or report["paired_result"] or report["probe_cases_passed"])
            self.assertEqual(report["status"], "failed")

    def test_late_original_copy_mtime_blocks_sdl_and_late_sdl_revokes_top_pass(self):
        for failed in ("original", "sdl"):
            with self.subTest(failed=failed), tempfile.TemporaryDirectory() as temp:
                report, starts, output = self.run_model(temp, paired=True, fail_role=failed)
                self.assertFalse(report["ok"])
                self.assertFalse(report["probe_cases_passed"])
                self.assertEqual(starts, ["original"] if failed == "original" else ["original", "sdl"])
                record = next(case for case in report["cases"] if case["kind"] == failed)
                self.assertEqual(record["status"], "failed")
                self.assertFalse(record["copy_save_matches_completed_state"])
                self.assertTrue((output / failed / "comparison.json").is_file())
                if failed == "original": self.assertFalse((output / "sdl").exists())


class RewardLoadSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = runner.prepare(ORIGINAL, "status")

    def test_actual_saved_turn_round_uses_original_full_banner_and_digits(self):
        palette = self.prepared["assets"]["palette"]
        two = runner.load_round_template(ORIGINAL, palette, 2)
        self.assertEqual(two.tobytes(), runner.round_two_template(ORIGINAL / "FDOTHER.DAT", palette).tobytes())
        self.assertEqual(two.size, (87, 20))
        correct = Image.new("RGB", (320, 200)); correct.paste(two.convert("RGB"), (120, 84))
        self.assertIsNotNone(runner.load_round_state(correct, two, 2))
        wrong = Image.new("RGB", (320, 200))
        wrong.paste(runner.load_round_template(ORIGINAL, palette, 3).convert("RGB"), (120, 84))
        self.assertIsNone(runner.load_round_state(wrong, two, 2))
        partial = Image.new("RGB", (320, 200)); partial.paste(two.crop((0, 0, 40, 20)).convert("RGB"), (120, 84))
        self.assertIsNone(runner.load_round_state(partial, two, 2))
        for bad in (0, 256, True):
            with self.assertRaises(ValueError): runner.load_round_template(ORIGINAL, palette, bad)

    def test_every_partial_dialog_close_is_rejected_despite_local_cursor_match(self):
        # 专门保留原事故：旧browse局部predicate已经成功，y164仍不能当就绪。
        with patch.object(runner, "resource_browse", return_value=lambda frame: {"kind": "browse"}):
            predicate = runner.loaded_browse_state(self.prepared, self.prepared["model"]["final"], (6, 14))
        plain = Image.new("RGB", (320, 200), (4, 8, 12))
        self.assertTrue(predicate(plain)["dialog_all_phase_edges_absent"])
        border = self.prepared["assets"]["dialog_top"].convert("RGB")
        for y in (112, 125, 138, 151, 164, 177):
            frame = plain.copy(); frame.paste(border, (5, y))
            with self.subTest(y=y): self.assertIsNone(predicate(frame))
        with patch.object(runner, "resource_browse", return_value=lambda frame: None):
            self.assertIsNone(runner.loaded_browse_state(self.prepared,
                self.prepared["model"]["final"], (6, 14))(plain))

    def run_observer(self, fail=None):
        import run_g4_detail_check as continuous
        seen = []; cleared = []
        capture = SimpleNamespace(key=Mock())
        saved = bytearray(self.prepared["model"]["final"]); saved[runner.META] = 7
        report = {"checkpoints": []}
        @contextmanager
        def series(capture, evidence):
            evidence["detail_series"] = {"model_only": True}
            try: yield {"model_only": True}
            finally: cleared.append(True)
        def checkpoint(capture, scope, name, predicate, points, **kwargs):
            seen.append(name)
            if name == fail: raise TimeoutError("required resource missing")
            if name == "b4-load-round":
                frame = Image.new("RGB", (320, 200))
                image = runner.load_round_template(ORIGINAL, self.prepared["assets"]["palette"], 7)
                frame.paste(image.convert("RGB"), (120, 84))
                result = predicate(frame)
                self.assertIsNotNone(result)
                self.assertEqual(result["turn"], 7)  # 实际Save，不是prepared.model的2。
            else: result = {"kind": "browse"}
            points.append({"name": name, **result})
        with patch.object(continuous, "detail_series", side_effect=series), \
                patch.object(continuous, "series_checkpoint", side_effect=checkpoint), \
                patch.object(runner, "require_source_unchanged") as guard:
            try:
                runner.observe_active_load(capture, self.prepared, saved, Path("Save"), [b"whole", 123], report)
            except TimeoutError:
                if fail is None: raise
        return capture, report, seen, cleared, guard.call_count

    def test_round_is_observed_before_browse_and_only_one_confirmation_is_sent(self):
        capture, report, seen, cleared, guards = self.run_observer()
        self.assertEqual(seen, ["b4-load-round", "b4-loaded-browse"])
        self.assertEqual(capture.key.call_args_list, [unittest.mock.call("ENTER")])
        self.assertEqual([row["name"] for row in report["checkpoints"]], seen)
        self.assertEqual((cleared, guards), ([True], 2))

    def test_missing_or_wrong_round_stops_without_navigation_or_extra_ack(self):
        capture, report, seen, cleared, guards = self.run_observer("b4-load-round")
        self.assertEqual(seen, ["b4-load-round"])
        self.assertEqual(capture.key.call_args_list, [unittest.mock.call("ENTER")])
        self.assertEqual((report["checkpoints"], cleared, guards), ([], [True], 0))

    def test_browse_timeout_keeps_round_proof_and_stops_helper_without_extra_key(self):
        capture, report, seen, cleared, guards = self.run_observer("b4-loaded-browse")
        self.assertEqual(seen, ["b4-load-round", "b4-loaded-browse"])
        self.assertEqual(capture.key.call_args_list, [unittest.mock.call("ENTER")])
        self.assertEqual([row["name"] for row in report["checkpoints"]], ["b4-load-round"])
        self.assertEqual((cleared, guards), ([True], 1))


if __name__ == "__main__": unittest.main()
