"""后台校准工具纯回归；WinAPI、游戏及哨兵均不实际启动。"""
import ast
import ctypes
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path[:0] = [str(TOOLS), str(ROOT / "src/tests")]
import run_background_input_check as check
from run_graphics_tests import decode_save


class BackgroundInputCheckTest(unittest.TestCase):
    @staticmethod
    def window_api():
        class Rect(ctypes.Structure):
            _fields_ = [(name, ctypes.c_long) for name in ("left", "top", "right", "bottom")]
        owner = {"pid": 99, "exists": True}
        def get_pid(hwnd, pointer):
            pointer._obj.value = owner["pid"]
            return 1
        u = SimpleNamespace(IsWindow=Mock(side_effect=lambda hwnd: owner["exists"]),
            GetWindowThreadProcessId=Mock(side_effect=get_pid),
            PostMessageW=Mock(return_value=True), SetWindowPos=Mock(return_value=True))
        w = SimpleNamespace(DWORD=ctypes.c_uint32, RECT=Rect, HWND=lambda value: value)
        return u, w, owner

    def test_background_foreground_rejected(self):
        with self.assertRaises(AssertionError):
            check.assert_background({"pid": 42}, 42)

    def test_other_app_focus_allowed(self):
        check.assert_background({"pid": 77}, 42)
        check.assert_background({"pid": 0}, 42)

    def test_exact_start_fixture(self):
        source = ROOT / "original_game/START.SAV"
        source_bytes, source_mtime = source.read_bytes(), source.stat().st_mtime_ns
        before = decode_save(source)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "fixture.SAV"
            manifest = check.fixture_from_start(ROOT, target)
            after = decode_save(target)
            self.assertEqual(after[0x30c8:0x30cc], bytes((6, 14, 1, 3)))
            self.assertEqual(after[0x30ca], after[0x30c8] - after[0x30c6])
            self.assertEqual(after[0x30cb], after[0x30c9] - after[0x30c7])
            self.assertEqual(after[0x30c6:0x30c8], bytes((5, 11)))
            self.assertEqual(after[:0x30c8], before[:0x30c8])
            self.assertEqual(after[0x30cc:-4], before[0x30cc:-4])
            self.assertEqual(int.from_bytes(after[-4:], "little"), sum(after[:-4]))
            self.assertTrue(manifest["controlled_not_natural"])
            self.assertTrue(manifest["stored_relative_fields_verified"])
            self.assertEqual(manifest["focus"], [6, 14])
            self.assertEqual(manifest["relative"], [1, 3])
            self.assertEqual(manifest["differences"], [
                {"offset": i, "before": a, "after": b}
                for i, (a, b) in enumerate(zip(before, after)) if a != b])
            from run_original_battle_menu import validate_fixture
            from run_original_comparison import archive_entry
            validate_fixture(target, archive_entry((ROOT / "original_game/FDFIELD.DAT").read_bytes(), 1)[0])
        self.assertEqual((source.read_bytes(), source.stat().st_mtime_ns),
                         (source_bytes, source_mtime))

    def test_output_readonly_evidence_rejected(self):
        for out in ((ROOT / "original_game").resolve(),
                    (ROOT / "original_game/new-artifacts").resolve()):
            with self.subTest(out=out), self.assertRaises(ValueError):
                check.check_output(ROOT, out)

    def test_sentinel_never_activates_any_window(self):
        tree = ast.parse(Path(check.__file__).read_text(encoding="utf-8"))
        calls = [n for n in ast.walk(tree) if isinstance(n, ast.Call)
                 and isinstance(n.func, ast.Attribute)]
        self.assertFalse(any(n.func.attr in (
            "SetForegroundWindow", "SetFocus", "SetActiveWindow") for n in calls))
        creates = [n for n in calls if n.func.attr == "CreateWindowExW"]
        self.assertEqual(len(creates), 1)
        self.assertEqual(ast.literal_eval(creates[0].args[0]) & 0x08000080, 0x08000080)
        show = next(n for n in calls if n.func.attr == "ShowWindow")
        self.assertEqual(ast.literal_eval(show.args[1]), 4)
        for call in (n for n in calls if n.func.attr == "SetWindowPos"):
            self.assertTrue(ast.literal_eval(call.args[-1]) & 0x10)  # SWP_NOACTIVATE

    def test_evidence_detects_samebytes_mtime(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "one"
            file.write_bytes(b"unchanged")
            before = check.evidence(Path(directory))
            stat = file.stat()
            os.utime(file, ns=(stat.st_atime_ns, stat.st_mtime_ns + 1_000_000_000))
            after = check.evidence(Path(directory))
            self.assertEqual(before["one"]["sha256"], after["one"]["sha256"])
            self.assertNotEqual(before, after)

    def test_original_directory_is_rejected_before_artifacts_or_processes(self):
        for out in (ROOT / "original_game", ROOT / "original_game/new-artifacts"):
            args = SimpleNamespace(root=ROOT, artifacts=out)
            with self.subTest(out=out), patch.object(Path, "mkdir") as mkdir, \
                    patch.object(check.subprocess, "Popen") as popen:
                with self.assertRaises(ValueError):
                    check.run(args)
                mkdir.assert_not_called()
                popen.assert_not_called()

    def test_foreground_record_keeps_target_and_observed_pid_separate(self):
        audit = check.Audit.__new__(check.Audit)
        audit.pid, audit.u, audit.w = 42, Mock(), Mock()
        audit.events, audit.violations = [], []
        with patch.object(check, "foreground", return_value={"hwnd": 770, "pid": 77}):
            row = audit.point("key_before:ENTER")
        self.assertEqual(row["target_pid"], 42)
        self.assertEqual((row["pid"], row["hwnd"]), (77, 770))
        self.assertEqual(audit.violations, [])

    def test_foreground_violation_is_sticky_and_missing_sentinel_fails(self):
        audit = check.Audit.__new__(check.Audit)
        audit.pid, audit.u, audit.w = 42, Mock(), Mock()
        audit.sentinel_hwnd = 100
        audit.sentinel_pid = 99
        audit.events, audit.violations = [], []
        with patch.object(check, "foreground", return_value={"hwnd": 420, "pid": 42}):
            with self.assertRaisesRegex(AssertionError, "activated"):
                audit.check("capture_before")
        with patch.object(check, "foreground", return_value={"hwnd": 770, "pid": 77}):
            with self.assertRaisesRegex(AssertionError, "activated"):
                audit.check("capture_after")
            audit.violations.clear()
            audit.u.IsWindow.return_value = False
            with self.assertRaisesRegex(AssertionError, "Sentinel closed"):
                audit.check("capture_after")

    def test_owned_window_requires_actual_pid_and_live_handle(self):
        u, w, owner = self.window_api()
        check.require_owned_window(u, w, 100, 99)
        for hwnd in (None, 0, -1, True, "100"):
            with self.subTest(hwnd=hwnd), self.assertRaisesRegex(AssertionError, "invalid HWND"):
                check.require_owned_window(u, w, hwnd, 99)
        owner["exists"] = False
        with self.assertRaisesRegex(AssertionError, "closed"):
            check.require_owned_window(u, w, 100, 99)
        owner.update(exists=True, pid=77)
        with self.assertRaisesRegex(AssertionError, "actual PID 77"):
            check.require_owned_window(u, w, 100, 99)
        owner["pid"] = 99
        u.GetWindowThreadProcessId.return_value = 0
        u.GetWindowThreadProcessId.side_effect = None
        with self.assertRaisesRegex(AssertionError, "not owned"):
            check.require_owned_window(u, w, 100, 99)
        u.PostMessageW.assert_not_called()
        u.SetWindowPos.assert_not_called()

    def test_audit_rejects_reused_sentinel_hwnd(self):
        audit = check.Audit.__new__(check.Audit)
        audit.pid, audit.sentinel_pid, audit.sentinel_hwnd = 42, 99, 100
        audit.u, audit.w, owner = self.window_api()
        audit.events, audit.violations = [], []
        owner["pid"] = 77
        with patch.object(check, "foreground", return_value={"hwnd": 770, "pid": 77}), \
                self.assertRaisesRegex(AssertionError, "not owned by PID 99"):
            audit.check("capture_before")

    def test_required_resource_error_cleans_only_owned_sentinel_and_keeps_first_error(self):
        import run_original_comparison as title

        for forced, mismatch in ((False, None), (True, None), (False, "registration"), (True, "cleanup")):
            with self.subTest(forced=forced, mismatch=mismatch), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "original_game").mkdir()
                (root / "original_game/FDOTHER.DAT").write_bytes(b"fault model")
                binary = root / "unused.exe"
                binary.write_bytes(b"never executable in this model")
                output = root / "artifacts"
                args = SimpleNamespace(root=root, artifacts=output,
                    game=binary, driver=binary, dosbox=binary)
                state = output / "sentinel.json"
                process = Mock(pid=99, returncode=None)
                process.stdout = io.StringIO(json.dumps({"pid": 99, "hwnd": 100}) + "\n")
                process.poll.side_effect = lambda: process.returncode
                closed = {"pid": 99, "hwnd": 100, "closed": True, "keys": []}

                def spawn(*unused, **kwargs):
                    check.write(state, {"pid": 99, "hwnd": 100, "keys": []})
                    kwargs["stderr"].close()  # fake child has no writer
                    return process

                def close_owned(hwnd, message, wp, lp):
                    self.assertEqual((hwnd, message, wp, lp), (100, 0x10, 0, 0))
                    check.write(state, closed)
                    return True

                attempts = []
                def wait(**kwargs):
                    attempts.append(kwargs)
                    if forced and len(attempts) == 1:
                        raise subprocess.TimeoutExpired("owned sentinel model", 5)
                    process.returncode = 0
                    return 0

                process.wait.side_effect = wait
                u, w, owner = self.window_api()
                if mismatch == "registration":
                    owner["pid"] = 77
                u.PostMessageW.side_effect = close_owned
                def bad_resource(*unused):
                    if mismatch == "cleanup":
                        owner["pid"] = 77
                    raise ValueError("bad required title resource")
                audit = SimpleNamespace(finish=Mock(), violations=[])
                with patch.object(check.subprocess, "Popen", side_effect=spawn) as popen, \
                        patch.object(sys, "path", list(sys.path)), \
                        patch.object(check, "Audit", return_value=audit) as make_audit, \
                        patch.object(check, "winapi", return_value=(u, w)), \
                        patch.object(title, "title_templates", side_effect=bad_resource) as templates, \
                        patch.object(title, "run") as run_game:
                    report = check.run(args)
                self.assertFalse(report["ok"])
                if mismatch == "registration":
                    self.assertIn("actual PID 77", report["error"])
                    make_audit.assert_not_called()
                    templates.assert_not_called()
                else:
                    self.assertEqual(report["error"], "bad required title resource")
                    audit.finish.assert_called_once()
                self.assertEqual(report["sentinel_exit_code"], 0)
                if mismatch:
                    u.PostMessageW.assert_not_called()
                    self.assertNotIn("closed", report["sentinel"])
                    self.assertTrue(report["sentinel_window_errors"])
                else:
                    self.assertTrue(report["sentinel"]["closed"])
                    u.PostMessageW.assert_called_once_with(100, 0x10, 0, 0)
                u.SetWindowPos.assert_not_called()
                self.assertEqual(report.get("sentinel_forced_cleanup", False), forced)
                self.assertEqual(process.terminate.call_count, int(forced))
                self.assertEqual(popen.call_count, 1)
                run_game.assert_not_called()
                self.assertEqual(json.loads((output / "comparison.json").read_text(encoding="utf-8"))["error"],
                                 report["error"])

    def test_occlusion_rechecks_ownership_before_move_restore_and_cleanup(self):
        import run_original_comparison as title
        import run_original_battle_menu as battle

        for lost_at in ("before_move", "before_restore", "body_error"):
            with self.subTest(lost_at=lost_at), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "original_game").mkdir()
                (root / "original_game/FDOTHER.DAT").write_bytes(b"model")
                (root / "original_game/FDFIELD.DAT").write_bytes(b"model")
                binary = root / "unused.exe"
                binary.write_bytes(b"never executable in this model")
                output = root / "artifacts"
                args = SimpleNamespace(root=root, artifacts=output,
                    game=binary, driver=binary, dosbox=binary)
                state = output / "sentinel.json"
                process = Mock(pid=99, returncode=None)
                process.stdout = io.StringIO(json.dumps({"pid": 99, "hwnd": 100}) + "\n")
                process.poll.side_effect = lambda: process.returncode
                def spawn(*unused, **kwargs):
                    check.write(state, {"pid": 99, "hwnd": 100, "keys": []})
                    kwargs["stderr"].close()
                    return process
                def wait(**kwargs):
                    process.returncode = 0
                    return 0
                process.wait.side_effect = wait
                u, w, owner = self.window_api()
                def rect(hwnd, pointer):
                    for name, value in (("left", 10), ("top", 20), ("right", 300), ("bottom", 200)):
                        setattr(pointer._obj, name, value)
                    return True
                u.GetWindowRect = Mock(side_effect=rect)
                def position(*unused):
                    self.assertEqual(owner["pid"], 99)
                    return True
                u.SetWindowPos.side_effect = position
                audit = SimpleNamespace(u=u, w=w, sentinel_hwnd=100,
                    finish=Mock(), violations=[])
                image = Mock()
                image.copy.return_value = image
                calls = []
                def frame():
                    calls.append(True)
                    if lost_at == "before_move" or len(calls) == 2:
                        owner["pid"] = 77
                    if lost_at == "body_error" and len(calls) == 2:
                        raise ValueError("bad required captured frame")
                    return image
                capture = SimpleNamespace(trace=[{"delivery": {"hwnd": "0xc8"}}], frame=frame)
                def scenario(*unused):
                    title.verify_occlusion(capture, output)
                    self.fail("lost sentinel owner must stop the scenario")
                with patch.object(check.subprocess, "Popen", side_effect=spawn), \
                        patch.object(sys, "path", list(sys.path)), \
                        patch.object(check, "Audit", return_value=audit), \
                        patch.object(check, "winapi", return_value=(u, w)), \
                        patch.object(check.time, "sleep"), \
                        patch.object(check, "fixture_from_start", return_value={}), \
                        patch.object(title, "archive_entry", return_value=(b"", None)), \
                        patch.object(battle, "validate_fixture"), \
                        patch.object(title, "title_templates", return_value={}), \
                        patch.object(title, "compare", return_value={"ok": True}), \
                        patch.object(title, "run", side_effect=scenario), \
                        patch.object(title, "Capture", title.Capture), \
                        patch.object(battle, "Capture", battle.Capture), \
                        patch.object(title, "running", title.running), \
                        patch.object(battle, "running", battle.running), \
                        patch.object(title, "verify_occlusion", create=True):
                    report = check.run(args)
                self.assertFalse(report["ok"])
                self.assertEqual(u.SetWindowPos.call_count, int(lost_at != "before_move"))
                u.PostMessageW.assert_not_called()
                self.assertTrue(report["sentinel_window_errors"])
                self.assertEqual(process.wait.call_count, 1)
                process.terminate.assert_not_called()
                if lost_at == "body_error":
                    self.assertEqual(report["error"], "bad required captured frame")
                else:
                    self.assertIn("actual PID 77", report["error"])


if __name__ == "__main__":
    unittest.main()
