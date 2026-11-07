"""后台窗口投递与错误收据；只用隔离子进程和stub，不启动GUI。"""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_comparison as probe


class DriverProtocolTest(unittest.TestCase):
    def test_native_quiet_uses_last_override_without_changing_host_inputs(self):
        for binary in ("C:/Tools/DOSBox-X.EXE", "/usr/bin/dosbox-x"):
            command = [binary, "-conf", "owned.conf", "-noconsole", "-set", "mixer nosound=false"]
            environment = {"DOSBOX_MIXER_nosound": "false", "DOSBOX_CPU_cycles": "20000", "kept": "value"}
            before_command, before_env = list(command), dict(environment)
            actual, child_env, receipt = probe.quiet_game_command(command, environment)
            self.assertEqual(actual, command + ["-set", "mixer nosound=true"])
            self.assertEqual(child_env, {"DOSBOX_CPU_cycles": "20000", "kept": "value"})
            self.assertTrue(receipt["native_quiet_requested"])
            self.assertEqual(receipt["ignored_native_quiet_overrides"], ["DOSBOX_MIXER_nosound"])
            self.assertEqual(command, before_command)
            self.assertEqual(environment, before_env)

    def test_native_quiet_does_not_change_other_programs_or_user_audio_settings(self):
        command, environment = ["python.exe"], {"DOSBOX_MIXER_nosound": "false"}
        actual, child_env, receipt = probe.quiet_game_command(command, environment)
        self.assertEqual((actual, child_env), (command, environment))
        self.assertIsNot(actual, command)
        self.assertIsNot(child_env, environment)
        self.assertFalse(receipt["native_quiet_requested"])

    def test_owned_sdl_test_is_quiet_without_observer_and_explicit_output_remains_distinct(self):
        command, environment = ["C:/candidate/fd2sdl.exe"], {"kept": "value"}
        actual, child_env, receipt = probe.quiet_game_command(command, environment)
        self.assertEqual(actual, command)
        self.assertEqual(child_env, {"kept": "value", "FD2SDL_TEST_AUDIO_QUIET": "1"})
        self.assertTrue(receipt["application_quiet_requested"])
        self.assertFalse(any(key.startswith("FD2SDL_APP_PCM_") for key in child_env))
        self.assertEqual(environment, {"kept": "value"})
        _, explicit, receipt = probe.quiet_game_command(command, {"FD2SDL_TEST_AUDIO_QUIET": "0"})
        self.assertEqual(explicit["FD2SDL_TEST_AUDIO_QUIET"], "0")
        self.assertFalse(receipt["application_quiet_requested"])
        with self.assertRaises(ValueError):
            probe.quiet_game_command(command, {"FD2SDL_TEST_AUDIO_QUIET": "invalid"})

    def test_utf8_and_unexpected_ansi_stderr_survive(self):
        native_run = subprocess.run
        process = Mock(pid=12345)
        process.poll.return_value = None
        for expression, expected in (("'窗口失焦'.encode('utf-8')", "窗口失焦"),
                                     ("bytes([0xd0])", "\\xd0")):
            def child(args, **kwargs):
                self.assertEqual(args, ["driver.exe", "key", "12345", "ENTER"])
                return native_run([sys.executable, "-c",
                    "import sys;sys.stderr.buffer.write(" + expression + ");sys.exit(1)"], **kwargs)
            with self.subTest(expected=expected), patch.object(probe.subprocess, "run", side_effect=child):
                with self.assertRaisesRegex(RuntimeError, expected.replace("\\", "\\\\")):
                    probe.driver_call(Path("driver.exe"), "key", process, "ENTER")

    def test_failed_background_delivery_never_falls_back_to_focus(self):
        process = Mock(pid=12345)
        with tempfile.TemporaryDirectory() as directory:
            capture = probe.Capture(Path("driver.exe"), process, Path(directory))
            with patch.object(probe, "driver_call", side_effect=RuntimeError("delivery failed")) as driver:
                with self.assertRaisesRegex(RuntimeError, "delivery failed"):
                    capture.key("ESC")
                self.assertEqual(driver.call_count, 1)
                self.assertEqual(driver.call_args.args[1:], ("key", process, "ESC"))

    @staticmethod
    def receipt(**changes):
        result = {"backend":"PostMessageW HWND", "pid":12345, "hwnd":"0x1234",
                  "key_down_sent":1, "key_up_sent":1, "down_tick_ms":100,
                  "up_tick_ms":250, "foreground_before":"0xabcd", "foreground_after":"0xef12",
                  "ok":True,"error":0,"kind":"key","keys":1,"sent":1,"release":1}
        result.update(changes)
        return "fd2 input: " + json.dumps(result) + "\n"

    def test_background_key_retains_changed_foreground_and_actual_delivery(self):
        process = Mock(pid=12345)
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            capture = probe.Capture(Path("driver.exe"), process, work)
            with patch.object(probe, "driver_call", return_value=self.receipt()) as driver:
                capture.key("DOWN")
            driver.assert_called_once_with(Path("driver.exe"), "key", process, "DOWN")
            attempt = json.loads((work/"input-trace.jsonl").read_text())
            delivered = json.loads((work/"input-delivery.jsonl").read_text())
            self.assertEqual(attempt["key"], "DOWN")
            self.assertNotIn("delivery", attempt)
            self.assertTrue(delivered["accepted"])
            self.assertEqual(delivered["delivery"], capture.trace[0]["delivery"])
            self.assertEqual(delivered["delivery"]["foreground_after"], "0xef12")

    def test_invalid_background_receipt_is_not_accepted_or_retried(self):
        for diagnostic in ("", "fd2 input: {broken", "fd2 input: []", self.receipt(pid=99), self.receipt(key_up_sent=0),
                           self.receipt(backend="SendInput"), self.receipt(up_tick_ms=99),
                           self.receipt()+self.receipt(), self.receipt(ok=False), self.receipt(error=5),
                           self.receipt(kind="wave",keys=2), self.receipt(sent=0), self.receipt(release=0)):
            with self.subTest(diagnostic=diagnostic), tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                capture = probe.Capture(Path("driver.exe"), Mock(pid=12345), work)
                with patch.object(probe, "driver_call", return_value=diagnostic) as driver:
                    with self.assertRaisesRegex(RuntimeError, "后台投递收据"):
                        capture.key("ENTER")
                self.assertEqual(driver.call_count, 1)
                self.assertFalse(json.loads((work/"input-delivery.jsonl").read_text())["accepted"])
                self.assertEqual(json.loads((work/"input-trace.jsonl").read_text())["key"], "ENTER")

    def test_exit_after_down_requires_owned_process_exit_before_omitting_up(self):
        diagnostic = self.receipt(key_up_sent=0, release=0, target_exited_before_release=True,
                                  release_not_needed=True, target_exit_code=0)
        for actual_exit in (None, 0, 1):
            with self.subTest(actual_exit=actual_exit), tempfile.TemporaryDirectory() as directory:
                process = Mock(pid=12345)
                process.poll.return_value = actual_exit
                capture = probe.Capture(Path("driver.exe"), process, Path(directory))
                with patch.object(probe, "driver_call", return_value=diagnostic):
                    if actual_exit == 0:
                        capture.key("ENTER")
                    else:
                        with self.assertRaisesRegex(RuntimeError, "后台投递收据无效"):
                            capture.key("ENTER")

    def test_foreground_guard_failure_preserves_release_and_stops_without_retry(self):
        for before, sent in (("0x1234", 0), ("0xabcd", 1)):
            with self.subTest(sent=sent), tempfile.TemporaryDirectory() as directory:
                diagnostic = self.receipt(ok=False, error=5023, sent=sent, release=sent,
                    key_down_sent=bool(sent), key_up_sent=bool(sent),
                    foreground_before=before, foreground_after="0x1234",
                    noactivate_style_before="0xc0000", noactivate_style_after="0x80c0000",
                    noactivate_verified=bool(sent), foreground_guard_ok=False)
                result = subprocess.CompletedProcess([], 1, stderr=diagnostic, stdout="")
                process = Mock(pid=12345)
                process.poll.return_value = None
                capture = probe.Capture(Path("driver.exe"), process, Path(directory))
                with patch.object(probe.subprocess, "run", return_value=result) as run:
                    with self.assertRaisesRegex(probe.DriverCallError, "exit=1"):
                        capture.key("ENTER")
                self.assertEqual(run.call_count, 1)
                delivered = json.loads((Path(directory)/"input-delivery.jsonl").read_text())
                self.assertFalse(delivered["accepted"])
                self.assertFalse(delivered["transport_ok"])
                self.assertEqual(delivered["receipts"][0]["release"], sent)
                self.assertFalse(delivered["receipts"][0]["foreground_guard_ok"])

    def test_wave_requires_both_keys_and_failed_partial_receipt_is_preserved(self):
        process = Mock(pid=12345)
        process.poll.return_value = None
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            capture = probe.Capture(Path("driver.exe"), process, work)
            valid = self.receipt(kind="wave",keys=2,sent=2,release=2)
            with patch.object(probe,"driver_call",return_value=valid):
                capture.key("HOST_W")
            with patch.object(probe,"driver_call",return_value=self.receipt()):
                with self.assertRaisesRegex(RuntimeError,"后台投递收据无效"):
                    capture.key("HOST_W")
            failed = self.receipt(ok=False,error=5,kind="wave",keys=2,sent=1,release=1,key_down_sent=0,key_up_sent=0)
            result = subprocess.CompletedProcess([],1,stderr=failed,stdout="")
            with patch.object(probe.subprocess,"run",return_value=result) as run:
                with self.assertRaisesRegex(probe.DriverCallError,"exit=1"):
                    capture.key("HOST_W")
            self.assertEqual(run.call_count,1)
            records = [json.loads(line) for line in (work/"input-delivery.jsonl").read_text().splitlines()]
            self.assertEqual([row["accepted"] for row in records],[True,False,False])
            self.assertFalse(records[-1]["transport_ok"])
            self.assertEqual(records[-1]["receipts"][0]["sent"],1)
            self.assertEqual(records[-1]["receipts"][0]["error"],5)

    @unittest.skipUnless(sys.platform == "win32", "Windows 子进程显示参数")
    def test_background_launch_does_not_mutate_parent_environment(self):
        process = Mock(pid=12345)
        process.poll.return_value = 0
        environment = {"kept":"original", "SDL_WINDOWS_RAW_KEYBOARD":"1"}
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(probe.subprocess, "Popen", return_value=process) as launch:
                with probe.running(["game.exe"], Path(directory), environment,
                                   Path(directory)/"game.log", Path("driver.exe"), []):
                    pass
            options = launch.call_args.kwargs
            self.assertEqual(options["env"]["SDL_WINDOWS_RAW_KEYBOARD"], "0")
            self.assertEqual(options["env"]["SDL_WINDOW_ACTIVATE_WHEN_SHOWN"], "0")
            self.assertEqual(options["env"]["SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN"], "1")
            self.assertEqual(options["startupinfo"].wShowWindow, 4)
        self.assertEqual(environment, {"kept":"original", "SDL_WINDOWS_RAW_KEYBOARD":"1"})

    def test_natural_startup_never_sends_skip_keys(self):
        expected=Image.new("RGB",(320,200))
        expected.putdata([((x*17)%256,(y*31)%256,((x+y)*7)%256)
                          for y in range(200) for x in range(320)])
        with tempfile.TemporaryDirectory() as directory:
            capture=probe.Capture(Path("driver.exe"),Mock(pid=12345),Path(directory),startup_skip=False)
            with patch.object(capture,"frame",side_effect=[Image.new("RGB",(320,200)),expected]), \
                 patch.object(capture,"key") as key, \
                 patch.object(probe.time,"monotonic",side_effect=[0,0,2,3]), \
                 patch.object(probe.time,"sleep"):
                self.assertEqual(capture.wait(expected,startup=True).tobytes(),expected.tobytes())
                key.assert_not_called()


if __name__ == "__main__":
    unittest.main()
