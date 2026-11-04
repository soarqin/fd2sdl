"""图形比较判据；合成图像不需要原版资源或 Windows 桌面。"""
import struct
import json
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_original_comparison import (Capture, SIZE, compare, decode_image, normalize,
                                     require_content, running)


class GraphicsComparisonTest(unittest.TestCase):
    def setUp(self):
        self.image = Image.new("RGB", SIZE, (24, 60, 100))

    def test_exact_and_vga_expansion_tolerance(self):
        self.assertTrue(compare(self.image, self.image.copy())["ok"])
        self.assertTrue(compare(self.image, Image.new("RGB", SIZE, (27, 63, 103)))["ok"])
        self.assertFalse(compare(self.image, Image.new("RGB", SIZE, (28, 60, 100)))["ok"])

    def test_single_wrong_pixel_is_rejected(self):
        actual = self.image.copy()
        actual.putpixel((319, 199), (24, 64, 100))
        result = compare(self.image, actual)
        self.assertFalse(result["ok"])
        self.assertEqual(result["bad_pixels"], 1)

    def test_integer_scale_only_no_crop_or_aspect_distortion(self):
        enlarged = self.image.resize((960, 600), Image.Resampling.NEAREST)
        self.assertEqual(normalize(enlarged).tobytes(), self.image.tobytes())
        for size in ((320, 240), (640, 200), (319, 200), (160, 100), (640, 401)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                normalize(Image.new("RGB", size))
        with self.assertRaises(ValueError):
            compare(self.image, enlarged)

    def test_black_or_flat_frames_are_not_evidence(self):
        for color in ("black", "white", "red"):
            with self.subTest(color=color), self.assertRaises(ValueError):
                require_content(Image.new("RGB", SIZE, color))
        colorful = Image.new("RGB", SIZE)
        for x in range(64):
            colorful.putpixel((x, 0), (x * 4, 0, 0))
        require_content(colorful)

    def test_fixture_decoder_modes_and_failures(self):
        # RUN、LITERAL、stride-2、SKIP；skip 保留零底。
        data = struct.pack("<HH", 8, 1) + bytes([0, 7, 0x81, 8, 9, 0x40, 10, 0xC2])
        self.assertEqual(list(decode_image(data).getdata()), [7, 8, 9, 0, 10, 0, 0, 0])
        for bad in (b"", data[:-1], data + b"x", struct.pack("<HH", 1, 1) + b"\x01\x07"):
            with self.subTest(data=bad), self.assertRaises(ValueError):
                decode_image(bad)

    def test_process_cleanup_on_failed_checkpoint(self):
        child = Mock(pid=123)
        child.poll.return_value = None
        child.wait.side_effect = lambda **_: child.poll.configure_mock(return_value=0)
        receipts = []
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "process.log"
            with patch("run_original_comparison.subprocess.Popen", return_value=child), \
                    patch("run_original_comparison.driver_call") as drive:
                with self.assertRaisesRegex(RuntimeError, "checkpoint"):
                    with running(["game"], Path(directory), {}, log, Path("driver"), receipts):
                        raise RuntimeError("checkpoint")
                drive.assert_called_once_with(Path("driver"), "close", child)
        self.assertTrue(receipts[0]["cleaned"])
        self.assertFalse(receipts[0]["forced_cleanup"])

    def test_forced_cleanup_cannot_pass(self):
        child = Mock(pid=123)
        child.poll.return_value = None
        child.terminate.side_effect = lambda: child.poll.configure_mock(return_value=1)
        receipts = []
        with tempfile.TemporaryDirectory() as directory:
            with patch("run_original_comparison.subprocess.Popen", return_value=child), \
                    patch("run_original_comparison.driver_call", side_effect=RuntimeError("focus")):
                with self.assertRaisesRegex(RuntimeError, "窗口进程未正常退出"):
                    with running(["game"], Path(directory), {}, Path(directory) / "process.log",
                                 Path("driver"), receipts):
                        pass
        self.assertTrue(receipts[0]["cleaned"])
        self.assertTrue(receipts[0]["forced_cleanup"])

    def test_explicit_exit2_requires_natural_matching_exit_and_defaults_still_reject_it(self):
        for actual, expected, passes in ((2,2,True),(1,2,False),(0,2,False),(2,0,False)):
            with self.subTest(actual=actual,expected=expected), tempfile.TemporaryDirectory() as directory:
                child = Mock(pid=123)
                child.poll.return_value = actual
                receipts = []
                with patch("run_original_comparison.subprocess.Popen",return_value=child), \
                        patch("run_original_comparison.driver_call") as drive:
                    context = running(["game"],Path(directory),{},Path(directory)/"result.log",
                                      Path("driver"),receipts,expected_exit=expected)
                    if passes:
                        with context:
                            pass
                    else:
                        with self.assertRaisesRegex(RuntimeError,"窗口进程未正常退出"):
                            with context:
                                pass
                drive.assert_not_called()
                child.terminate.assert_not_called()
                self.assertTrue(receipts[0]["cleaned"])
                self.assertEqual(receipts[0]["expected_exit"],expected)

    def test_missing_driver_still_cleans_and_preserves_checkpoint_failure(self):
        child = Mock(pid=123)
        child.poll.return_value = None
        child.terminate.side_effect = lambda: child.poll.configure_mock(return_value=1)
        receipts = []
        with tempfile.TemporaryDirectory() as directory:
            with patch("run_original_comparison.subprocess.Popen", return_value=child), \
                    patch("run_original_comparison.driver_call", side_effect=OSError("driver missing")):
                with self.assertRaisesRegex(ValueError, "checkpoint failed"):
                    with running(["game"], Path(directory), {}, Path(directory) / "process.log",
                                 Path("driver"), receipts):
                        raise ValueError("checkpoint failed")
        child.terminate.assert_called_once()
        self.assertTrue(receipts[0]["cleaned"])
        self.assertTrue(receipts[0]["forced_cleanup"])

    def test_input_attempt_is_recorded_even_when_focus_or_delivery_fails(self):
        for failed_call in (0, 1):
            events = []
            with self.subTest(failed_call=failed_call), tempfile.TemporaryDirectory() as directory:
                capture = Capture(Path("driver"), Mock(pid=123), Path(directory), events)
                outcomes = [RuntimeError("input failed")] if failed_call == 0 else [None, RuntimeError("input failed")]
                with patch("run_original_comparison.driver_call", side_effect=outcomes):
                    with self.assertRaisesRegex(RuntimeError, "input failed"):
                        capture.key("DOWN")
                logged = [json.loads(line) for line in (Path(directory) / "input-trace.jsonl").read_text().splitlines()]
                self.assertEqual(logged, events)
                self.assertEqual(len(events), 1)
                self.assertEqual((events[0]["pid"], events[0]["key"]), (123, "DOWN"))


class TitleTransitionTest(unittest.TestCase):
    def test_known_blank_transition_retains_frame_and_waits_without_input(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            raw = work / "window-123-current.bmp"
            raw.write_bytes(b"failed WGC evidence")
            capture = Capture(Path("driver"), Mock(pid=123), work)
            expected = Image.new("RGB", SIZE)
            for x in range(64):
                expected.putpixel((x, 0), (x * 4, 0, 0))
            capture.frame = Mock(side_effect=[RuntimeError("blank or nearly uniform"), expected])
            capture.key = Mock()
            with patch("run_original_comparison.time.sleep"):
                result = capture.wait(expected, blank_transition=True)
            self.assertEqual(result.tobytes(), expected.tobytes())
            capture.key.assert_not_called()
            event = json.loads((work / "capture-errors.jsonl").read_text())
            self.assertEqual(Path(event["frame"]).read_bytes(), b"failed WGC evidence")
            self.assertEqual(capture.frame.call_count, 2)

    def test_only_explicit_blank_transition_is_retried(self):
        for allowed, message in ((False, "blank or nearly uniform"),
                                 (True, "geometry changed"),
                                 (True, "no visible window"),
                                 (True, "WGC device failed")):
            with self.subTest(allowed=allowed, message=message), tempfile.TemporaryDirectory() as directory:
                capture = Capture(Path("driver"), Mock(pid=123), Path(directory))
                capture.frame = Mock(side_effect=RuntimeError(message))
                with self.assertRaisesRegex(RuntimeError, message):
                    capture.wait(Image.new("RGB", SIZE), blank_transition=allowed)
                self.assertEqual(capture.frame.call_count, 1)

    def test_blank_transition_keeps_original_deadline_and_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Capture(Path("driver"), Mock(pid=123), Path(directory))
            capture.frame = Mock(side_effect=RuntimeError("blank or nearly uniform"))
            capture.key = Mock()
            with patch("run_original_comparison.time.monotonic", side_effect=[0, 0, 0, 0, 21]), \
                    patch("run_original_comparison.time.sleep"):
                with self.assertRaisesRegex(RuntimeError, "始终没有可用画面"):
                    capture.wait(Image.new("RGB", SIZE), blank_transition=True, timeout=20)
            self.assertEqual(capture.frame.call_count, 1)
            capture.key.assert_not_called()


class StartupGeometryTest(unittest.TestCase):
    class Clock:
        def __init__(self):
            self.now = 0.0

        def monotonic(self):
            return self.now

        def sleep(self, seconds):
            self.now += seconds

    @staticmethod
    def title():
        image = Image.new("RGB", SIZE)
        for x in range(64):
            image.putpixel((x, 0), (x * 4, 0, 0))
        return image

    def test_captured_logo_geometry_keeps_wait_until_owned_escape_then_exact_title(self):
        clock, expected = self.Clock(), self.title()
        logo = Image.linear_gradient("L").resize((720, 400)).convert("RGB")
        with tempfile.TemporaryDirectory() as directory:
            work, process = Path(directory), Mock(pid=123)
            capture = Capture(Path("driver"), process, work)
            escapes, commands = [], []
            def driver(driver, command, target, argument=None):
                self.assertIs(target, process)
                commands.append(command)
                if command == "capture":
                    frame = expected.resize((640, 400), Image.Resampling.NEAREST) if escapes else logo
                    frame.save(argument)
                elif command == "key":
                    self.assertEqual(argument, "ESC")
                    escapes.append(clock.now)
            with patch("run_original_comparison.driver_call", side_effect=driver), \
                    patch("run_original_comparison.time.monotonic", side_effect=clock.monotonic), \
                    patch("run_original_comparison.time.sleep", side_effect=clock.sleep):
                actual = capture.wait(expected, startup=True, timeout=4)
            self.assertEqual(actual.tobytes(), expected.tobytes())
            self.assertEqual(len(escapes), 1)
            self.assertGreaterEqual(escapes[0], 1)
            self.assertLess(escapes[0], 1.2)
            self.assertEqual(commands[-3:], ["focus", "key", "capture"])
            events = [json.loads(line) for line in (work / "capture-errors.jsonl").read_text().splitlines()]
            self.assertTrue(events)
            for event in events:
                self.assertEqual(event["kind"], "startup_geometry")
                self.assertEqual(event["raw_size"], [720, 400])
                with Image.open(event["frame"]) as image:
                    self.assertEqual(image.size, (720, 400))
                self.assertIn(event, capture.trace)
            keys = [event for event in capture.trace if "key" in event]
            self.assertEqual([event["key"] for event in keys], ["ESC"])

    def test_geometry_timeout_keeps_deadline_and_original_escape_cadence(self):
        clock = self.Clock()
        logo = Image.linear_gradient("L").resize((720, 400)).convert("RGB")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            capture = Capture(Path("driver"), Mock(pid=123), work)
            escapes = []
            def driver(driver, command, target, argument=None):
                if command == "capture":
                    logo.save(argument)
                elif command == "key":
                    escapes.append(clock.now)
            with patch("run_original_comparison.driver_call", side_effect=driver), \
                    patch("run_original_comparison.time.monotonic", side_effect=clock.monotonic), \
                    patch("run_original_comparison.time.sleep", side_effect=clock.sleep):
                with self.assertRaisesRegex(RuntimeError, "始终没有可用画面"):
                    capture.wait(self.title(), startup=True, timeout=3.4)
            self.assertGreaterEqual(clock.now, 3.4)
            self.assertLess(clock.now, 3.6)
            self.assertEqual(len(escapes), 2)
            self.assertGreaterEqual(escapes[0], 1)
            self.assertGreaterEqual(escapes[1] - escapes[0], 2)
            self.assertLess(escapes[1] - escapes[0], 2.2)
            self.assertIsNone(capture.last)
            self.assertTrue(list(work.glob("startup-geometry-*.bmp")))

    def test_only_startup_captured_noninteger_geometry_is_allowed(self):
        for startup, size in ((False, (720, 400)), (True, (160, 100)),
                              (True, (319, 200)), (True, (640, 199))):
            with self.subTest(startup=startup, size=size), tempfile.TemporaryDirectory() as directory:
                capture = Capture(Path("driver"), Mock(pid=123), Path(directory))
                def driver(driver, command, target, argument=None):
                    self.assertEqual(command, "capture")
                    Image.new("RGB", size, "blue").save(argument)
                with patch("run_original_comparison.driver_call", side_effect=driver) as calls:
                    with self.assertRaises(ValueError):
                        capture.wait(self.title(), startup=startup, blank_transition=True)
                self.assertEqual(calls.call_count, 1)
                self.assertFalse(capture.trace)
                self.assertFalse(list(Path(directory).glob("startup-geometry-*.bmp")))

    def test_unknown_errors_and_missing_raw_frame_are_not_retried(self):
        for error, raw in ((ValueError("unrelated decoder error"), "invalid"),
                           (RuntimeError("WGC device failed"), True),
                           (ValueError("截图尺寸不是 320x200 的整数倍率：(640, 401)"), True),
                           (ValueError("截图尺寸不是 320x200 的整数倍率：(720, 400)"), False)):
            with self.subTest(error=error, raw=raw), tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                if raw == "invalid":
                    (work / "window-123-current.bmp").write_bytes(b"invalid raw evidence")
                elif raw:
                    Image.new("RGB", (720, 400)).save(work / "window-123-current.bmp")
                capture = Capture(Path("driver"), Mock(pid=123), work)
                capture.frame = Mock(side_effect=error)
                capture.key = Mock()
                with self.assertRaises(type(error)) as caught:
                    capture.wait(self.title(), startup=True)
                self.assertIs(caught.exception, error)
                self.assertEqual(capture.frame.call_count, 1)
                capture.key.assert_not_called()

    def test_legal_title_does_not_send_startup_escape(self):
        for startup in (False, True):
            with self.subTest(startup=startup), tempfile.TemporaryDirectory() as directory:
                capture = Capture(Path("driver"), Mock(pid=123), Path(directory))
                expected = self.title()
                capture.frame = Mock(return_value=expected)
                capture.key = Mock()
                self.assertEqual(capture.wait(expected, startup=startup).tobytes(), expected.tobytes())
                capture.key.assert_not_called()


if __name__ == "__main__":
    unittest.main()
