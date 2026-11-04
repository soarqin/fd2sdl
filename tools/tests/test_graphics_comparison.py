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


if __name__ == "__main__":
    unittest.main()
