"""总览资源合成、人数门、只读关闭与测量Save导航；不启动窗口或子进程。"""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_overview as overview


def source_snapshot():
    data = bytearray(0x59cb)
    data[overview.META:overview.META + 18] = bytes((1, 12, 0, 5, 11, 8, 15, 3, 4, 4, 0, 0, 0, 0, 0, 1, 1, 1))
    for index in range(12):
        base = overview.UNITS + index * overview.STRIDE
        data[base + 6] = 2 if index < 4 else 0
        data[base + 7] = index
    data[overview.UNITS + 9 * overview.STRIDE + 5] = 1
    return data


def condition_text(first=(1, 2, 3, -1), second=(4, 5, 6, 7, -1)):
    a = struct.pack("<" + "h" * len(first), *first)
    b = struct.pack("<" + "h" * len(second), *second)
    text = bytearray(1200)
    struct.pack_into("<HHH", text, 597 * 2, 1200, 1200 + len(a), 1200 + len(a) + len(b))
    return bytes(text) + a + b


def literal_image(width, height, color):
    data = bytearray(struct.pack("<HH", width, height))
    for _ in range(height):
        remaining = width
        while remaining:
            count = min(remaining, 64)
            data.append(0x80 + count - 1)
            data.extend(bytes([color]) * count)
            remaining -= count
    return bytes(data)


class OverviewResourceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "FDOTHER.DAT").write_bytes(b"images")
        (self.root / "FDTXT.DAT").write_bytes(b"text")
        frames = [literal_image(1, 1, 1)] * 138
        for base, color in ((31, 20), (42, 40)):
            for digit in range(10):
                frames[base + digit] = literal_image(6, 8, color + digit)
        for index, _, _, width, height in overview.RECTANGLES:
            frames[index] = literal_image(width, height, index - 132)
        offsets, offset = [], 6 + 138 * 4
        for frame in frames:
            offsets.append(offset)
            offset += len(frame)
        self.sheet = b"LMI1" + struct.pack("<H138I", 138, *offsets) + b"".join(frames)
        self.font = bytearray(32 * 12)
        for token in range(1, 8):
            self.font[token * 32] = 0x80 >> (token % 7)
        self.text = condition_text()
        self.palette = [channel for value in range(256) for channel in (value, value, value)]
        self.source = source_snapshot()
        patcher = patch.object(overview, "archive_entry", side_effect=self.entry)
        patcher.start()
        self.addCleanup(patcher.stop)

    def entry(self, data, index):
        return self.text if data == b"text" else bytes(self.font) if index == 4 else self.sheet

    def templates(self, source=None):
        return overview.overview_templates(self.root, self.source if source is None else source, self.palette)

    def test_all_four_opaque_rectangles_numbers_and_conditions_are_checked(self):
        templates = self.templates()
        self.assertEqual(templates["fields"], {"stage": 1, "turn": 1, "resource": 0,
                                               "side_counts_left_to_right": [7, 4, 0]})
        self.assertEqual([item["fragment"] for item in templates["conditions"]], [597, 598])
        frame = templates["canvas"].convert("RGB")
        matched = overview.match_overview(frame, templates)
        self.assertEqual(sum(item["checked_pixels"] for item in matched["rectangles"]), 25289)
        for point in ((109, 19), (143, 24), (120, 159), (140, 176), (81, 61), (84, 116)):
            with self.subTest(point=point):
                corrupted = frame.copy()
                corrupted.putpixel(point, (255, 0, 255))
                self.assertIsNone(overview.match_overview(corrupted, templates))
        outside = frame.copy()
        outside.putpixel((0, 0), (255, 0, 255))
        self.assertTrue(overview.compare_overview(frame, outside)["ok"])

    def test_count_gate_excludes_only_hidden_unit121_and_race10(self):
        data = self.source.copy()
        data[overview.UNITS + 5] = 0x84  # acted、AI-ineligible且HP0仍计数。
        data[overview.UNITS + overview.STRIDE + 7] = 121
        data[overview.UNITS + 2 * overview.STRIDE + 0x1f] = 10
        self.assertEqual(overview.overview_counts(data), {0: 7, 1: 0, 2: 2})

    def test_eight_digit_resource_uses_leading_eight_characters_and_signed_clamp(self):
        for value, expected in ((0, "00000000"), (12_345_678, "12345678"),
                                (100_000_000, "10000000"), (0xffffffff, "00000000")):
            data = self.source.copy()
            data[overview.META + 10:overview.META + 14] = value.to_bytes(4, "little")
            canvas = self.templates(data)["canvas"]
            self.assertEqual([canvas.getpixel((140 + digit * 6, 176)) for digit in range(8)],
                             [20 + int(digit) for digit in expected])

    def test_font_per_glyph_order_can_shadow_the_previous_glyph_foreground(self):
        font = bytearray(32 * 3)
        font[32 + 2:32 + 4] = b"\0\1"  # glyph1: row1 col15。
        font[64:66] = b"\x80\0"  # glyph2: row0 col0，其左下影覆盖前一个字。
        canvas = Image.new("P", (320, 200), 2)
        overview.draw_condition(canvas, condition_text((1, 2, -1)), font, 597, 80, 61)
        self.assertEqual(canvas.getpixel((95, 62)), 76)
        self.assertEqual(canvas.getpixel((96, 61)), 205)
        self.assertEqual(canvas.getpixel((80, 61)), 2)  # native bg0透明，不预清。

    def test_newline_is_19px_and_unknown_controls_or_truncated_glyphs_fail(self):
        canvas = Image.new("P", (320, 200), 2)
        tokens = overview.draw_condition(canvas, condition_text((1, -2, 2, -1)), self.font, 597, 80, 61)
        self.assertIn(-2, tokens["tokens"])
        self.assertEqual(canvas.getpixel((82, 80)), 205)
        for text, font in ((condition_text((1, -3, -1)), self.font),
                           (condition_text((100, -1)), self.font),
                           (condition_text((1, -1, 2, -1)), self.font)):
            with self.assertRaises(ValueError):
                overview.draw_condition(canvas, text, font, 597, 80, 61)


class OverviewNavigationTest(unittest.TestCase):
    def setUp(self):
        self.image = Image.new("RGB", (320, 200), (30, 60, 90))

    def test_default_overview_any_key_close_explicit_left_save_and_host_close(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"source A")
            stamp = save.stat().st_mtime_ns
            capture = Mock()
            capture.process.wait.return_value = 0
            observation = {}
            with patch.object(overview, "wait_title"), \
                    patch.object(overview, "wait_state", return_value=(self.image, {})) as state, \
                    patch.object(overview, "wait_measured_save", return_value=b"measured") as measure, \
                    patch.object(overview, "driver_call") as driver:
                measured, pages = overview.exercise(capture, "original", {}, {}, (76, 100),
                                                    (("LEFT", (52, 100)),), {3: [self.image] * 3},
                                                    save, b"source A", stamp, [], observation)
            open_overview = ["ENTER", "UP", "ENTER", "ENTER"]
            expected = ["UP", "ENTER", "LEFT"]
            for key in overview.CLOSE_KEYS:
                expected += open_overview + [key]
            expected += ["ENTER", "UP", "ENTER", "LEFT", "ENTER", "ENTER"] + open_overview
            self.assertEqual([call.args[0] for call in capture.key.call_args_list], expected)
            self.assertEqual([entry["key"] for entry in observation["close_keys"]], list(overview.CLOSE_KEYS))
            self.assertEqual(set(pages), {"enter", "esc", "right", "a", "host-close"})
            self.assertEqual(measured, b"measured")
            self.assertTrue(observation["host_close_while_overview"])
            self.assertEqual(sum("secondary-zero" in call.args[1] for call in state.call_args_list), 6)
            driver.assert_called_once_with(capture.driver, "close", capture.process)
            measure.assert_called_once()

    def test_missing_stable_overview_stops_before_closing_or_saving(self):
        capture = Mock()
        def wait(capture, name, predicate, checkpoints, **kwargs):
            if name.endswith("enter-overview"):
                raise TimeoutError("stable overview not reached")
            return self.image, {}
        with patch.object(overview, "wait_title"), patch.object(overview, "wait_state", side_effect=wait), \
                patch.object(overview, "wait_measured_save") as measure:
            with self.assertRaises(TimeoutError):
                overview.exercise(capture, "original", {}, {}, (76, 100), (),
                                  {3: [self.image] * 3}, Mock(), b"source A", 1, [], {})
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                         ["UP", "ENTER", "ENTER", "UP", "ENTER", "ENTER"])
        measure.assert_not_called()


if __name__ == "__main__":
    unittest.main()
