"""图形测试调度器的无窗口回归；不启动 SDL、DOSBox 或截图程序。"""

from pathlib import Path
import hashlib
import subprocess
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, call, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_graphics_tests as runner


def encrypted_fixture(plain):
    """按 src/save.c 的对称 XOR 生成合成输入；预期明文由调用者提供。"""
    encrypted = bytearray(plain)
    state = 0xa5
    for index in range(len(encrypted)):
        added = (state + 0x9014) % 0x10000
        state = (added * 8 % 0x10000) + added // 0x2000
        encrypted[index] ^= state % 0x100
    return bytes(encrypted)


class SaveEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "FD2.SAV"
        # 非零且有跨字节进位的 checksum，避免全零输入掩盖错误。
        payload = bytes((index * 37 + 11) % 256 for index in range(0x59c7))
        self.plain = payload + sum(payload).to_bytes(4, "little")
        self.encrypted = encrypted_fixture(self.plain)

    def test_decode_preserves_full_plaintext_and_encrypted_evidence(self):
        self.path.write_bytes(self.encrypted)
        self.assertEqual(runner.decode_save(self.path), self.plain)
        self.assertEqual(self.path.read_bytes(), self.encrypted)

    def test_corrupt_payload_or_checksum_is_rejected(self):
        for index in (0, 0x30c3, 0x59ca):
            with self.subTest(index=index):
                corrupt = bytearray(self.encrypted)
                corrupt[index] ^= 1
                self.path.write_bytes(corrupt)
                with self.assertRaisesRegex(AssertionError, "checksum"):
                    runner.decode_save(self.path)
                self.assertEqual(self.path.read_bytes(), bytes(corrupt))

    def test_truncated_empty_or_extra_data_is_rejected(self):
        for data in (b"", self.encrypted[:-1], self.encrypted + b"\0"):
            with self.subTest(size=len(data)):
                self.path.write_bytes(data)
                with self.assertRaisesRegex(AssertionError, "size"):
                    runner.decode_save(self.path)

    def test_roster_count_is_independent_of_active_actor_count(self):
        for active, roster in ((12, 4), (27, 31), (12, 0), (12, 32)):
            data = bytearray(self.plain)
            data[0x30c4] = active
            data[0x30cc] = roster
            self.assertEqual(runner.check_roster_count(data), roster)
        data[0x30cc] = 33
        with self.assertRaisesRegex(AssertionError, "roster count"):
            runner.check_roster_count(data)

    def test_fresh_session_owns_full_roster_while_target_metadata_tail_and_slots_remain(self):
        after = bytearray(self.plain)
        after[0x30cc] = 4
        after[0x8a3:0x12a3] = b"\x42" * 0xa00
        self.assertEqual(runner.check_storage_preservation(self.plain, after, 937), 4)
        for offset in (937, 0x8a2, 0x312b, 0x59c6):
            with self.subTest(offset=offset):
                bad = after.copy()
                bad[offset] ^= 1
                with self.assertRaisesRegex(AssertionError, "unrelated container"):
                    runner.check_storage_preservation(self.plain, bad, 937)


class EvidencePathTest(unittest.TestCase):
    def test_readonly_evidence_paths_are_rejected_before_directory_creation(self):
        for path in (runner.ROOT / "original_game", runner.ROOT / "original_game" / "nested"):
            with self.subTest(path=path), patch.object(Path, "mkdir") as mkdir:
                with self.assertRaisesRegex(ValueError, "original_game"):
                    runner.Suite(SimpleNamespace(build_dir=path))
                mkdir.assert_not_called()


class TerminalFixtureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "terminal.SAV"
        data = bytearray((index * 29 + 7) % 256 for index in range(0x59cb))
        data[0x30c4] = 3
        data[0x30c5] = 0
        for index, side in enumerate((2, 0, 0)):
            base = 0x12a3 + index * 0x50
            data[base + 5] = 0x80
            data[base + 6] = side
        data[-4:] = sum(data[:-4]).to_bytes(4, "little")
        self.plain = bytes(data)

    def test_terminal_fixtures_only_change_known_hidden_flags_and_checksum(self):
        for outcome, targets in (("victory", (1, 2)), ("defeat", (0,))):
            with self.subTest(outcome=outcome):
                self.path.write_bytes(runner.terminal_save_fixture(self.plain, outcome))
                actual = runner.decode_save(self.path)
                expected = bytearray(self.plain)
                for index in targets:
                    expected[0x12a3 + index * 0x50 + 5] |= 1
                expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
                self.assertEqual(actual, bytes(expected))
        self.assertEqual(self.plain[0x12a3 + 5], 0x80)

    def test_invalid_or_unsupported_terminal_fixture_is_rejected(self):
        for offset, value in ((0x30c5, 1), (0x30c4, 0), (0x30c4, 65)):
            data = bytearray(self.plain)
            data[offset] = value
            data[-4:] = sum(data[:-4]).to_bytes(4, "little")
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                runner.terminal_save_fixture(data, "victory")
        for data, outcome in ((self.plain[:-1], "victory"), (self.plain, "unknown")):
            with self.subTest(outcome=outcome), self.assertRaises(ValueError):
                runner.terminal_save_fixture(data, outcome)


class ConfirmationButtonTest(unittest.TestCase):
    def setUp(self):
        from PIL import Image

        palette = bytearray(768)
        self.colors = ((40, 80, 120), (120, 80, 40), (80, 120, 40), (20, 60, 100))
        for index, color in enumerate(self.colors, 1):
            palette[index * 3:index * 3 + 3] = bytes(channel // 4 for channel in color)
        payloads = []
        for frame in range(54):
            color = {48: 1, 49: 2, 51: 3, 52: 4}.get(frame, 0)
            payloads.append(struct.pack("<HH", 24, 16) + b"\0\0" + bytes([color]) * 382)
        offsets, offset = [], 54 * 4
        for payload in payloads:
            offsets.append(offset)
            offset += len(payload)
        self.commands = struct.pack("<54I", *offsets) + b"".join(payloads)
        self.resource = Mock()
        self.resource.read_bytes.return_value = b"synthetic resource"
        patcher = patch("run_original_comparison.archive_entry",
                        side_effect=lambda data, entry: bytes(palette) if entry == 0 else self.commands)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.image = Image.new("RGB", (320, 200), "green")
        self.image.paste(self.colors[0], (232, 168, 256, 184))
        self.image.paste(self.colors[3], (264, 168, 288, 184))
        for x in (232, 233, 264, 265):
            self.image.putpixel((x, 168), (0, 255, 0))

    def test_fixed_positions_accept_normal_or_highlight_and_ignore_transparency(self):
        buttons = runner.check_confirmation_buttons(self.image, self.resource)
        self.assertEqual(buttons["yes"], {"position": [232, 168], "frame": 48, "checked_pixels": 382})
        self.assertEqual(buttons["no"]["frame"], 52)

    def test_missing_or_swapped_buttons_and_truncated_resource_are_rejected(self):
        absent = self.image.copy()
        absent.paste("green", (232, 168, 256, 184))
        with self.assertRaisesRegex(AssertionError, "YES button"):
            runner.check_confirmation_buttons(absent, self.resource)
        swapped = self.image.copy()
        swapped.paste(self.image.crop((264, 168, 288, 184)), (232, 168))
        with self.assertRaisesRegex(AssertionError, "YES button"):
            runner.check_confirmation_buttons(swapped, self.resource)
        self.commands = self.commands[:216 + 52 * (4 + 24 * 16) + 5]
        with self.assertRaises(ValueError):
            runner.check_confirmation_buttons(self.image, self.resource)


class ResourceHashTest(unittest.TestCase):
    def test_hashes_detect_content_addition_and_deletion(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "subdir").mkdir()
            evidence = root / "subdir" / "resource.dat"
            evidence.write_bytes(b"original")
            before = runner.hashes(root)
            self.assertEqual(before, {
                str(Path("subdir") / "resource.dat"):
                    hashlib.sha256(b"original").hexdigest()})
            self.assertEqual(runner.hashes(root), before)
            evidence.write_bytes(b"modified")
            self.assertNotEqual(runner.hashes(root), before)
            evidence.write_bytes(b"original")
            added = root / "new.dat"
            added.write_bytes(b"")
            self.assertNotEqual(runner.hashes(root), before)
            added.unlink()
            self.assertEqual(runner.hashes(root), before)
            evidence.unlink()
            self.assertEqual(runner.hashes(root), {})


class BrowseRestoreTest(unittest.TestCase):
    def test_idle_animation_outside_menu_regions_is_ignored_at_integer_scale(self):
        from PIL import Image

        browse = Image.new("RGB", (320, 200), (20, 40, 60))
        animated = browse.copy()
        animated.paste((80, 100, 120), (148, 100, 172, 124))
        animated.paste((120, 80, 40), (172, 124, 196, 148))
        runner.check_browse_restored(
            browse.resize((960, 600), Image.Resampling.NEAREST), animated)

    def test_retained_radial_menu_or_confirmation_pixels_are_rejected(self):
        from PIL import Image

        browse = Image.new("RGB", (320, 200), (20, 40, 60))
        for position in ((148, 80), (171, 95), (264, 168), (287, 183)):
            with self.subTest(position=position):
                retained = browse.copy()
                retained.putpixel(position, (21, 40, 60))
                with self.assertRaisesRegex(AssertionError, "restore browse pixels"):
                    runner.check_browse_restored(browse, retained)


class EndTurnEvidenceTest(unittest.TestCase):
    checkpoints = (
        "field end turn: turn=1 phase=2 remaining=4",
        "field phase: turn=1 phase=1 remaining=0",
        "field phase: turn=1 phase=0 remaining=8",
        "field phase: turn=2 phase=2 remaining=4",
    )

    def test_single_confirmation_and_ordered_phases_are_accepted(self):
        runner.check_end_turn_log("\n".join(self.checkpoints))

    def test_missing_duplicate_or_out_of_order_phase_is_rejected(self):
        for lines in (self.checkpoints[:-1], self.checkpoints + self.checkpoints[:1],
                      (self.checkpoints[0], self.checkpoints[2], self.checkpoints[1], self.checkpoints[3])):
            with self.subTest(lines=lines), self.assertRaises(AssertionError):
                runner.check_end_turn_log("\n".join(lines))

    def test_extra_player_round_is_rejected(self):
        text = "\n".join(self.checkpoints + ("field phase: turn=3 phase=2 remaining=4",))
        with self.assertRaisesRegex(AssertionError, "second round"):
            runner.check_end_turn_log(text)


class CommandTreeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.suite = runner.Suite(SimpleNamespace(
            build_dir=root, driver=root / "input driver.exe"))

    def test_windows_command_has_suspended_job_owner_and_cleanup_deadline(self):
        with patch.object(runner.sys, "platform", "win32"), patch.object(
                runner.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as execute:
            self.suite.command("original", ["python", "runner with spaces.py", "a\"b\\"], timeout=12)
        self.assertEqual(execute.call_args.args[0], [
            str(self.suite.args.driver), "run-job", "12000", "python", "runner with spaces.py", "a\"b\\"])
        self.assertEqual(execute.call_args.kwargs["timeout"], 27)
        self.assertTrue(execute.call_args.kwargs["stdout"].closed)
        receipt = self.suite.report["cases"][0]
        self.assertEqual(receipt["status"], "passed")
        self.assertIn("Windows Job", receipt["process_tree_owner"])

    def test_timeout_residual_children_or_guard_failure_cannot_pass(self):
        for exit_code in (124, 125, 126):
            with self.subTest(exit_code=exit_code), patch.object(runner.sys, "platform", "win32"), \
                    patch.object(runner.subprocess, "run", return_value=SimpleNamespace(returncode=exit_code)):
                with self.assertRaisesRegex(RuntimeError, f"exited {exit_code}"):
                    self.suite.command(f"failed-{exit_code}", ["python", "runner.py"])
                self.assertEqual(self.suite.report["cases"][-1]["status"], "failed")

    def test_outer_guard_timeout_still_records_failure_and_closes_log(self):
        with patch.object(runner.sys, "platform", "win32"), patch.object(
                runner.subprocess, "run", side_effect=subprocess.TimeoutExpired("driver", 255)) as execute:
            with self.assertRaises(subprocess.TimeoutExpired):
                self.suite.command("hung-guard", ["python", "runner.py"])
        self.assertTrue(execute.call_args.kwargs["stdout"].closed)
        self.assertEqual(self.suite.report["cases"][-1]["status"], "failed")

    def test_other_platform_keeps_existing_direct_command(self):
        with patch.object(runner.sys, "platform", "linux"), patch.object(
                runner.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as execute:
            self.suite.command("input", ["ctest", "--test-dir", Path("build")], timeout=42)
        self.assertEqual(execute.call_args.args[0], ["ctest", "--test-dir", "build"])
        self.assertEqual(execute.call_args.kwargs["timeout"], 42)
        self.assertNotIn("process_tree_owner", self.suite.report["cases"][0])


class WindowCleanupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        resources = self.root / "original_game"
        resources.mkdir()
        self.source_save = resources / "FD2.SAV"
        self.source_save.write_bytes(b"read-only source evidence")
        self.suite = runner.Suite(SimpleNamespace(
            build_dir=self.root / "build", game=self.root / "game.exe",
            driver=self.root / "driver.exe"))
        self.suite.driver = Mock()
        self.process = Mock(pid=1234, returncode=None)
        self.process.poll.side_effect = lambda: self.process.returncode
        self.wait_results = [0]
        self.process.wait.side_effect = self.wait
        self.popen = self.enter_patch(patch.object(
            runner.subprocess, "Popen", return_value=self.process))
        self.enter_patch(patch.object(runner, "ROOT", self.root))

    def enter_patch(self, patcher):
        self.addCleanup(patcher.stop)
        return patcher.start()

    def wait(self, **kwargs):
        self.assertEqual(kwargs, {"timeout": 5})
        result = self.wait_results.pop(0)
        if isinstance(result, Exception):
            raise result
        self.process.returncode = result
        return result

    def assert_cleaned(self, exit_code, forced=False, body_error=None, expected_exit=0):
        expected = {
            "name": "case", "pid": 1234, "cleaned": True,
            "exit_code": exit_code, "forced_cleanup": forced, "expected_exit": expected_exit}
        if body_error is not None:
            expected["body_error"] = body_error
        self.assertEqual(self.suite.report["processes"], [expected])
        self.assertTrue(self.popen.call_args.kwargs["stdout"].closed)

    def test_success_closes_waits_and_uses_isolated_save(self):
        with self.suite.window("case", ["--field-play", "0"], cwd=self.root) as result:
            process, work, _, save = result
            self.assertIs(process, self.process)
            self.assertEqual(save, work / "FD2.SAV")
            self.assertEqual(save.read_bytes(), self.source_save.read_bytes())
            save.write_bytes(b"test write")
            self.assertEqual(self.source_save.read_bytes(), b"read-only source evidence")
            command = self.popen.call_args.args[0]
            self.assertEqual(command, [str(self.root / "game.exe"), "--field-play", "0"])
            self.assertEqual(self.popen.call_args.kwargs["env"]["FD2SDL_SAVE_PATH"], str(save))
        self.suite.driver.assert_called_once_with(self.process, "close")
        self.process.wait.assert_called_once_with(timeout=5)
        self.process.terminate.assert_not_called()
        self.process.kill.assert_not_called()
        self.assert_cleaned(0)

    def test_body_exception_still_closes_and_preserves_exception(self):
        with self.assertRaisesRegex(RuntimeError, "body failed"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                raise RuntimeError("body failed")
        self.suite.driver.assert_called_once_with(self.process, "close")
        self.process.wait.assert_called_once_with(timeout=5)
        self.assert_cleaned(0, body_error="body failed")

    def test_close_failure_terminates_and_reaps_process(self):
        self.suite.driver.side_effect = OSError("window unavailable")
        self.wait_results = [-15]
        with self.assertRaisesRegex(RuntimeError, "forced cleanup"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                pass
        self.process.terminate.assert_called_once_with()
        self.process.wait.assert_called_once_with(timeout=5)
        self.process.kill.assert_not_called()
        self.assert_cleaned(-15, forced=True)

    def test_close_and_terminate_timeout_escalate_to_kill_and_reap(self):
        self.wait_results = [subprocess.TimeoutExpired("game", 5),
                             subprocess.TimeoutExpired("game", 5), -9]
        with self.assertRaisesRegex(RuntimeError, "forced cleanup"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                pass
        self.suite.driver.assert_called_once_with(self.process, "close")
        self.assertEqual(self.process.method_calls, [
            call.poll(), call.wait(timeout=5), call.terminate(),
            call.wait(timeout=5), call.kill(), call.wait(timeout=5), call.poll()])
        self.assert_cleaned(-9, forced=True)

    def test_forced_cleanup_is_failure_even_with_zero_exit_code(self):
        self.suite.driver.side_effect = OSError("window unavailable")
        with self.assertRaisesRegex(RuntimeError, "forced cleanup"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                pass
        self.assert_cleaned(0, forced=True)

    def test_already_exited_normally_is_not_signalled(self):
        self.process.returncode = 0
        with self.suite.window("case", [], cwd=self.root, save=False):
            pass
        self.suite.driver.assert_not_called()
        self.process.wait.assert_not_called()
        self.process.terminate.assert_not_called()
        self.process.kill.assert_not_called()
        self.assert_cleaned(0)

    def test_nonzero_exit_is_rejected_even_if_no_cleanup_is_needed(self):
        self.process.returncode = 7
        with self.assertRaisesRegex(RuntimeError, "exited 7"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                pass
        self.suite.driver.assert_not_called()
        self.assert_cleaned(7)

    def test_explicit_unsupported_terminal_exit_is_accepted_without_signalling(self):
        self.process.returncode = 2
        with self.suite.window("case", [], cwd=self.root, save=False, expected_exit=2):
            pass
        self.suite.driver.assert_not_called()
        self.assert_cleaned(2, expected_exit=2)

    def test_expected_terminal_exit_cannot_accept_a_normal_exit(self):
        self.process.returncode = 0
        with self.assertRaisesRegex(RuntimeError, "expected 2"):
            with self.suite.window("case", [], cwd=self.root, save=False, expected_exit=2):
                pass
        self.assert_cleaned(0, expected_exit=2)

    def test_title_save_fixture_is_copied_before_process_start(self):
        fixture = self.root / "supported-stage0.SAV"
        fixture.write_bytes(b"isolated stage 0 fixture")
        with self.suite.window("case", [], cwd=self.root, save_fixture=fixture) as (_, _, _, save):
            self.assertEqual(save.read_bytes(), fixture.read_bytes())
            self.assertNotEqual(save.read_bytes(), self.source_save.read_bytes())
        self.assertEqual(fixture.read_bytes(), b"isolated stage 0 fixture")
        self.assert_cleaned(0)

    def test_forced_cleanup_does_not_replace_body_exception(self):
        self.suite.driver.side_effect = OSError("window unavailable")
        self.wait_results = [-15]
        with self.assertRaisesRegex(ValueError, "body failed"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                raise ValueError("body failed")
        self.assert_cleaned(-15, forced=True, body_error="body failed")

    def test_cleanup_error_does_not_replace_body_exception(self):
        self.wait_results = [subprocess.TimeoutExpired("game", 5)] * 3
        with self.assertRaisesRegex(ValueError, "body failed"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                raise ValueError("body failed")
        receipt = self.suite.report["processes"][0]
        self.assertFalse(receipt["cleaned"])
        self.assertTrue(receipt["forced_cleanup"])
        self.assertEqual(receipt["body_error"], "body failed")
        self.assertIn("timed out", receipt["cleanup_error"])

    def test_process_still_alive_after_kill_is_failure(self):
        self.wait_results = [subprocess.TimeoutExpired("game", 5)] * 3
        with self.assertRaisesRegex(RuntimeError, "not cleaned"):
            with self.suite.window("case", [], cwd=self.root, save=False):
                pass
        self.assertFalse(self.suite.report["processes"][0]["cleaned"])
        self.assertIn("cleanup_error", self.suite.report["processes"][0])


if __name__ == "__main__":
    unittest.main()
