"""leave配对的导航、自然退出和失败证据判据；不启动窗口或子进程。"""
from contextlib import contextmanager
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_leave as leave


class LeaveExitTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.image = Image.new("RGB", (320, 200), (30, 60, 90))
        self.capture = Mock(artifacts=self.root, process=Mock(pid=123), last=self.image)

    def test_only_observed_natural_zero_exit_passes_without_capture_or_close(self):
        self.capture.process.wait.return_value = 0
        receipt = {}
        leave.wait_natural_exit(self.capture, "original", receipt, [])
        self.assertTrue(receipt["natural_exit"])
        self.assertTrue(receipt["observed_before_cleanup"])
        self.assertEqual(receipt["exit_code"], 0)
        self.capture.process.wait.assert_called_once_with(timeout=15)
        self.capture.frame.assert_not_called()
        self.capture.key.assert_not_called()

    def test_nonzero_natural_exit_is_failure(self):
        self.capture.process.wait.return_value = 2
        receipt = {}
        with self.assertRaisesRegex(AssertionError, "naturally exited 2"):
            leave.wait_natural_exit(self.capture, "original", receipt, [])
        self.assertTrue(receipt["natural_exit"])
        self.assertEqual(receipt["exit_code"], 2)
        self.capture.key.assert_not_called()

    def test_title_remaining_alive_cannot_pass_even_if_later_cleanup_exits_zero(self):
        self.capture.process.wait.side_effect = subprocess.TimeoutExpired("SDL", 15)
        self.capture.frame.return_value = self.image
        receipt, checkpoints = {}, []
        with self.assertRaisesRegex(TimeoutError, "did not naturally exit"):
            leave.wait_natural_exit(self.capture, "sdl", receipt, checkpoints)
        self.capture.process.returncode = 0  # 模拟finally随后WM_CLOSE正常退出。
        self.assertFalse(receipt["natural_exit"])
        self.assertTrue(receipt["timed_out"])
        self.assertNotIn("exit_code", receipt)
        self.assertTrue((self.root / "sdl-leave-not-exited.png").is_file())
        self.assertEqual(checkpoints[0]["kind"], "natural_exit_timeout")
        self.assertFalse(checkpoints[0]["last_frame_may_precede_error"])
        self.capture.key.assert_not_called()

    def test_timeout_capture_failure_keeps_last_valid_frame_without_another_backend(self):
        self.capture.process.wait.side_effect = subprocess.TimeoutExpired("SDL", 15)
        self.capture.frame.side_effect = RuntimeError("WGC access denied")
        receipt, checkpoints = {}, []
        with self.assertRaises(TimeoutError):
            leave.wait_natural_exit(self.capture, "sdl", receipt, checkpoints)
        self.assertTrue(checkpoints[0]["last_frame_may_precede_error"])
        self.assertEqual(checkpoints[0]["capture_error"], "WGC access denied")
        self.capture.frame.assert_called_once_with()
        self.capture.key.assert_not_called()


class LeaveNavigationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.image = Image.new("RGB", (320, 200), (30, 60, 90))
        self.save = self.root / "FD2.SAV"
        self.save.write_bytes(b"immutable fixture")
        self.capture = Mock(artifacts=self.root, last=self.image)
        self.capture.process.poll.return_value = None
        self.titles = {3: [self.image] * 3}

    def exercise(self, initial_steps=()):
        return leave.exercise(self.capture, "original", {}, (76, 100), initial_steps,
                              self.titles, self.save, b"immutable fixture", [], {})

    def test_escape_no_then_one_affirmative_with_wgc_gates_and_no_extra_key(self):
        with patch.object(leave, "wait_title"), patch.object(leave, "save_checkpoint"), \
                patch.object(leave, "wait_state", return_value=(self.image, {})) as state, \
                patch.object(leave, "wait_natural_exit") as exit_wait:
            self.exercise(initial_steps=(("RIGHT", (100, 100)),))
        open_path = ["ENTER", "UP", "ENTER", "DOWN", "ENTER"]
        expected = (["UP", "ENTER", "RIGHT"] + open_path + ["ESC"] +
                    open_path + ["RIGHT", "ENTER"] + open_path + ["ENTER"])
        self.assertEqual([call.args[0] for call in self.capture.key.call_args_list], expected)
        self.assertIn("original-empty-step-1", [call.args[1] for call in state.call_args_list])
        self.assertIn("original-no-selected", [call.args[1] for call in state.call_args_list])
        exit_wait.assert_called_once()
        self.assertEqual(self.save.read_bytes(), b"immutable fixture")

    def test_missing_selected_leave_checkpoint_stops_without_confirmation(self):
        def wait(capture, name, predicate, checkpoints, **kwargs):
            if name.endswith("leave-selected"):
                raise TimeoutError("leave not selected")
            return self.image, {}
        with patch.object(leave, "wait_title"), patch.object(leave, "save_checkpoint"), \
                patch.object(leave, "wait_state", side_effect=wait), patch.object(leave, "wait_natural_exit") as exit_wait:
            with self.assertRaisesRegex(TimeoutError, "not selected"):
                self.exercise()
        self.assertEqual([call.args[0] for call in self.capture.key.call_args_list],
                         ["UP", "ENTER", "ENTER", "UP", "ENTER", "DOWN"])
        exit_wait.assert_not_called()

    def test_cancel_that_writes_save_stops_before_next_menu(self):
        def key(name):
            if name == "ESC":
                self.save.write_bytes(b"unexpected write")
        self.capture.key.side_effect = key
        with patch.object(leave, "wait_title"), patch.object(leave, "save_checkpoint"), \
                patch.object(leave, "wait_state", return_value=(self.image, {})), \
                patch.object(leave, "wait_natural_exit") as exit_wait:
            with self.assertRaisesRegex(AssertionError, "changed its isolated save"):
                self.exercise()
        self.assertEqual(self.capture.key.call_args_list[-1].args, ("ESC",))
        exit_wait.assert_not_called()


class LeaveRunTest(unittest.TestCase):
    def test_original_evidence_is_kept_when_later_sdl_fails_and_processes_are_serial(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "original_game"
            original.mkdir()
            (original / "START.SAV").write_bytes(b"source fixture")
            (original / "FDFIELD.DAT").write_bytes(b"metadata")
            binary = root / "test.exe"
            binary.write_bytes(b"test binary")
            args = SimpleNamespace(root=root, artifacts=root / "result", game=binary,
                                   driver=binary, dosbox=binary, save_fixture=None)
            plain = bytearray(0x59cb)
            plain[leave.META + 1] = 12
            plain[leave.META + 9] = 4
            events = []
            @contextmanager
            def running(command, cwd, env, log, driver, receipts):
                events.append("start")
                if events.count("start") != events.count("end") + 1:
                    raise AssertionError("overlapping test processes")
                receipt = {"pid": len(events), "cleaned": False, "forced_cleanup": False}
                receipts.append(receipt)
                try:
                    yield Mock(pid=len(events))
                finally:
                    receipt.update(cleaned=True, exit_code=0)
                    events.append("end")
            def exercise(capture, kind, assets, focus, steps, titles, save, source, checkpoints, exit_receipt):
                if kind == "sdl":
                    raise TimeoutError("SDL stayed at title")
                exit_receipt.update(natural_exit=True, observed_before_cleanup=True, exit_code=0)
            with patch.object(leave, "archive_entry", return_value=b"\1"), \
                    patch.object(leave, "validate_initial", return_value=(plain, (76, 100))), \
                    patch.object(leave, "empty_focus_navigation", return_value=[]), \
                    patch.object(leave, "round_assets", return_value={}), \
                    patch.object(leave, "title_templates", return_value={}), \
                    patch.object(leave, "running", side_effect=running), patch.object(leave, "Capture"), \
                    patch.object(leave, "exercise", side_effect=exercise):
                report = leave.run(args)
            self.assertFalse(report["ok"])
            self.assertEqual(events, ["start", "end", "start", "end"])
            self.assertEqual([case["status"] for case in report["cases"]], ["passed", "failed"])
            self.assertTrue(report["cases"][0]["exit_observation"]["natural_exit"])
            self.assertTrue(report["cases"][0]["save_unchanged"])
            self.assertTrue(report["fixture_unchanged"])
            self.assertTrue(report["original_unchanged"])
            self.assertTrue((args.artifacts / "comparison.json").is_file())


if __name__ == "__main__":
    unittest.main()
