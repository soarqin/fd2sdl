"""B1 资源 ROI／取消导航判据；不启动游戏或图形桌面。"""
import sys
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_battle_menu as menu


def assets():
    palette = [value for i in range(256) for value in (i, (i * 3) % 256, (i * 7) % 256)]
    frames = []
    for i in range(78):
        frame = Image.new("P", (24, 16 if i in (48, 49, 51, 52) else 20), 74)
        frame.putpalette(palette)
        frame.putpixel((0, 0), 0)
        # 每个帧有不同的可见图案，不能靠平面颜色或任意矩形通过。
        frame.putpixel((2 + i % 20, 2 + i // 20), 197)
        frame.putpixel((2 + i % 20, 3 + i // 20), 199)
        frames.append(frame)
    cursor = Image.new("P", (24, 24), 0)
    cursor.putpalette(palette)
    for point in ((1, 1), (22, 1), (1, 22), (22, 22)):
        cursor.putpixel(point, 197)
    border = Image.new("P", (16, 5), 73)
    border.putpalette(palette)
    return {"commands": frames, "cursor": cursor, "notice": cursor,
            "restoration_border": border}


def paste(frame, indexed, point):
    mask = indexed.point(lambda value: 255 if value else 0).convert("L")
    frame.paste(indexed.convert("RGB"), point, mask)


def buttons(oracle, selected):
    frame = Image.new("RGB", menu.SIZE, (9, 8, 7))
    for item, command in enumerate((16, 17)):
        paste(frame, oracle.assets["commands"][command * 3 + (item == selected)],
              menu.BUTTON_POSITIONS[item])
    return frame


def parent(oracle, selected, march_disabled=False):
    frame = Image.new("RGB", menu.SIZE, (9, 8, 7))
    for item, command in enumerate(menu.PARENT_IDS):
        state = 2 if item == 1 and march_disabled else int(item == selected)
        position = tuple(a + b for a, b in zip(oracle.focus, menu.MENU_OFFSETS[item]))
        paste(frame, oracle.assets["commands"][command * 3 + state], position)
    return frame


def browse(oracle):
    frame = Image.new("RGB", menu.SIZE, (9, 8, 7))
    paste(frame, oracle.assets["cursor"], oracle.focus)
    frame.paste(oracle.assets["restoration_border"].convert("RGB"),
                menu.RESTORED_BORDER_BOX[:2])
    return frame


class BattleMenuOracleTest(unittest.TestCase):
    def setUp(self):
        self.oracle = menu.Oracle(assets(), (148, 124))

    def test_exact_buttons_distinguish_yes_no_and_reject_missing(self):
        yes = buttons(self.oracle, 0)
        no = buttons(self.oracle, 1)
        self.assertIsNotNone(self.oracle.buttons(yes, 0))
        self.assertIsNone(self.oracle.buttons(yes, 1))
        self.assertIsNotNone(self.oracle.buttons(no, 1))
        self.assertIsNone(self.oracle.buttons(no, 0))
        self.assertIsNone(self.oracle.buttons(Image.new("RGB", menu.SIZE)))
        yes.putpixel((235, 172), (255, 255, 255))
        self.assertIsNone(self.oracle.buttons(yes, 0))

    def test_transparency_is_ignored_but_one_opaque_error_fails(self):
        indexed = self.oracle.assets["commands"][48]
        frame = Image.new("RGB", menu.SIZE)
        paste(frame, indexed, (232, 168))
        frame.putpixel((232, 168), (255, 255, 255))  # 资源透明角。
        self.assertTrue(menu.match_indexed(frame, indexed, (232, 168))["ok"])
        frame.putpixel((233, 168), (255, 255, 255))
        self.assertFalse(menu.match_indexed(frame, indexed, (232, 168))["ok"])
        self.assertFalse(menu.match_indexed(frame, Image.new("P", (1, 1)), (1, 1))["ok"])

    def test_menu_highlight_requires_enabled_march(self):
        original = parent(self.oracle, 3)
        sdl = parent(self.oracle, 3, march_disabled=True)
        self.assertIsNotNone(self.oracle.menu(original, 3))
        self.assertIsNone(self.oracle.menu(original, 0))
        self.assertIsNone(self.oracle.menu(sdl, 3))
        self.assertIsNone(self.oracle.menu(sdl))
        highlighted = parent(self.oracle, 1)
        self.assertIsNotNone(self.oracle.menu(highlighted, 1))
        self.assertIsNotNone(self.oracle.menu(highlighted))

    def test_resource_comparison_includes_march_opaque_pixels(self):
        original = parent(self.oracle, 3)
        detail = self.oracle.menu(original, 3)
        sdl = original.copy()
        self.assertTrue(menu.compare_resources(original, sdl, detail, self.oracle.assets)["ok"])
        x, y = (a + b for a, b in zip(self.oracle.focus, menu.MENU_OFFSETS[1]))
        sdl.putpixel((x + 1, y), (255, 255, 255))
        self.assertFalse(menu.compare_resources(original, sdl, detail, self.oracle.assets)["ok"])

    def test_fixture_must_have_empty_supported_visible_focus(self):
        plain = bytearray(0x59cb)
        plain[0x30c3:0x30ca] = bytes((1, 1, 0, 0, 15, 6, 20))
        with patch.object(menu, "decode_save", return_value=plain):
            self.assertEqual(menu.validate_fixture(Path("fixture"), 0)[0], (148, 124))
            plain[0] = 29
            with self.assertRaisesRegex(ValueError, "shape"):
                menu.validate_fixture(Path("fixture"), 0)
            plain[0] = 0
            plain[0x12a3:0x12a5] = bytes((6, 20))
            with self.assertRaisesRegex(ValueError, "存在可见 actor"):
                menu.validate_fixture(Path("fixture"), 0)
            plain[0x12a3 + 5] = 1
            self.assertEqual(menu.validate_fixture(Path("fixture"), 0)[0], (148, 124))
            plain[0x30c5] = 1
            with self.assertRaisesRegex(ValueError, "stage 0"):
                menu.validate_fixture(Path("fixture"), 0)

    def test_cancel_wait_observes_notice_without_pressing_a_key(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Image.new("RGB", menu.SIZE, (1, 2, 3))
            second = Image.new("RGB", menu.SIZE, (3, 2, 1))
            capture = Mock(artifacts=Path(directory), process=Mock(pid=123))
            capture.frame.side_effect = [first, second]
            report = []
            with patch.object(menu, "require_content"), patch.object(menu.time, "sleep"):
                _, result = menu.wait_state(capture, "cancel", lambda frame:
                                            {"kind": "browse"} if frame is second else None,
                                            report, transition=lambda frame: frame is first)
            self.assertTrue(result["cancellation_notice_observed"])
            capture.key.assert_not_called()
            self.assertTrue((Path(directory) / "cancel-fragment412.png").is_file())

    def test_timeout_retains_frame_and_does_not_blindly_confirm(self):
        with tempfile.TemporaryDirectory() as directory:
            frame = Image.new("RGB", menu.SIZE, (1, 2, 3))
            capture = Mock(artifacts=Path(directory), process=Mock(pid=123), last=frame)
            capture.frame.return_value = frame
            report = []
            with patch.object(menu.time, "monotonic", side_effect=[0, 0, 2, 2]), \
                    patch.object(menu.time, "sleep"), self.assertRaises(TimeoutError):
                menu.wait_state(capture, "missing", lambda _: None, report, timeout=1)
            capture.key.assert_not_called()
            self.assertTrue((Path(directory) / "missing-timeout.png").is_file())

    def test_real_input_sequence_never_affirms_end(self):
        image = Image.new("RGB", menu.SIZE)
        capture = Mock(last=image)
        titles = {3: [image, image, image]}
        with patch.object(menu, "save_checkpoint"), \
                patch.object(menu, "wait_state", return_value=(image, {"kind": "browse"})):
            menu.exercise(capture, "test", self.oracle, titles, [])
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                         ["UP", "ENTER", "ENTER", "DOWN", "ENTER", "ESC",
                          "ENTER", "DOWN", "ENTER", "RIGHT", "ENTER", "ENTER", "ESC"])

    def test_startup_geometry_can_settle_without_capture_fallback(self):
        capture = Mock()
        image = Image.new("RGB", menu.SIZE)
        capture.wait.side_effect = [ValueError("截图尺寸不是 320x200 的整数倍率"), image]
        with patch.object(menu.time, "sleep"):
            self.assertIs(menu.wait_title(capture, image), image)
        self.assertEqual(capture.wait.call_count, 2)
        capture.key.assert_not_called()

    def test_cancel_notice_with_visible_cursor_is_not_restored_browse(self):
        initial = browse(self.oracle)
        dialog = initial.copy()
        # 即使面板底边和光标均正确，也必须显式拒绝412文字残留。
        paste(dialog, self.oracle.assets["notice"], (98, 146))
        self.assertIsNotNone(self.oracle.browse(dialog))
        self.assertIsNone(menu.restored_browse(dialog, initial, self.oracle))
        self.assertIsNotNone(menu.restored_browse(initial, initial, self.oracle))

    def test_restore_checks_resource_baseline_cursor_and_missing_menus(self):
        initial = browse(self.oracle)
        bad = initial.copy()
        bad.putpixel(menu.RESTORED_BORDER_BOX[:2], (255, 255, 255))
        # 同一错误同时存在于baseline也不能代替正确资源边框。
        self.assertIsNone(menu.restored_browse(bad, bad, self.oracle))
        self.assertIsNone(menu.restored_browse(initial, bad, self.oracle))
        no_cursor = initial.copy()
        no_cursor.paste((9, 8, 7), (*self.oracle.focus,
                         self.oracle.focus[0] + 24, self.oracle.focus[1] + 24))
        self.assertIsNone(menu.restored_browse(no_cursor, initial, self.oracle))
        for overlay in (buttons(self.oracle, 0), parent(self.oracle, 0)):
            paste(overlay, self.oracle.assets["cursor"], self.oracle.focus)
            overlay.paste(self.oracle.assets["restoration_border"].convert("RGB"),
                          menu.RESTORED_BORDER_BOX[:2])
            self.assertIsNone(menu.restored_browse(overlay, initial, self.oracle))

    def test_known_wgc_scene_transients_retry_and_keep_last_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            image = Image.new("RGB", menu.SIZE, (1, 2, 3))
            capture = Mock(artifacts=folder, process=Mock(pid=123), last=image)
            (folder / "window-123-current.bmp").write_bytes(b"last raw capture")
            capture.frame.side_effect = [
                RuntimeError("Windows.Graphics.Capture failed: blank or nearly uniform captured client area"),
                RuntimeError("window geometry changed during capture; retry at a stable size"),
                RuntimeError("no visible window for PID 123"),
                ValueError("截图尺寸不是 320x200 的整数倍率：(640, 480)"), image]
            report = []
            with patch.object(menu, "require_content"), patch.object(menu.time, "sleep"):
                _, detail = menu.wait_state(capture, "battle", lambda _: {"kind": "browse"}, report)
            self.assertEqual(detail["wgc_transient_retries"], 4)
            self.assertEqual(len([e for e in report if e["kind"] == "wgc_transient"]), 4)
            self.assertTrue((folder / "battle-wgc-retry-1-last-valid.png").is_file())
            self.assertEqual((folder / "battle-wgc-retry-1-last-raw.bmp").read_bytes(), b"last raw capture")
            capture.key.assert_not_called()

    def test_persistent_wgc_transient_times_out_without_changing_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            image = Image.new("RGB", menu.SIZE, (1, 2, 3))
            capture = Mock(artifacts=folder, process=Mock(pid=123), last=image)
            capture.frame.side_effect = RuntimeError("blank or nearly uniform captured client area")
            report = []
            with patch.object(menu.time, "monotonic", side_effect=[0, 0, 0.2, 2, 2]), \
                    patch.object(menu.time, "sleep"), self.assertRaises(TimeoutError):
                menu.wait_state(capture, "battle", lambda _: None, report, timeout=1)
            self.assertTrue((folder / "battle-timeout.png").is_file())
            self.assertTrue(report[0]["last_frame_may_precede_error"])
            capture.key.assert_not_called()

    def test_unrelated_capture_error_fails_immediately(self):
        capture = Mock()
        capture.frame.side_effect = RuntimeError("Windows.Graphics.Capture failed: access denied")
        with patch.object(menu.time, "sleep") as sleep, self.assertRaisesRegex(RuntimeError, "access denied"):
            menu.wait_state(capture, "battle", lambda _: None, [])
        sleep.assert_not_called()
        capture.key.assert_not_called()


class RealRestorationBorderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        original = Path(__file__).resolve().parents[2] / "original_game"
        cls.assets = menu.resource_templates(original / "FDOTHER.DAT", original / "FDTXT.DAT")
        cls.oracle = menu.Oracle(cls.assets, (148, 124))
        sheet = menu.archive_entry((original / "FDOTHER.DAT").read_bytes(), 5)
        start, end = struct.unpack_from("<II", sheet, 6 + 130 * 4)
        cls.panel = menu.decode_image(sheet[start:end])

    def test_all_80_pixels_are_static_frame130_border(self):
        self.assertEqual(menu.RESTORED_BORDER_BOX, (16, 190, 32, 195))
        self.assertEqual(self.panel.size, (69, 34))
        border = self.assets["restoration_border"]
        self.assertEqual(border.size, (16, 5))
        self.assertEqual(border.tobytes(), self.panel.crop((11, 29, 27, 34)).tobytes())
        self.assertEqual(list(border.getdata()),
                         [index for index in (207, 73, 254, 189, 99) for _ in range(16)])
        self.assertFalse(any(224 <= index < 240 for index in border.getdata()))
        initial = browse(self.oracle)
        result = menu.restored_browse(initial, initial, self.oracle)
        self.assertIsNotNone(result)
        self.assertEqual(result["restoration_resource"]["bad_pixels"], 0)
        for y in range(190, 195):
            for x in range(16, 32):
                damaged = initial.copy()
                damaged.putpixel((x, y), (255, 255, 255))
                self.assertIsNone(menu.restored_browse(damaged, initial, self.oracle))

    def test_old_roi_false_negative_from_observed_water_colors(self):
        initial = browse(self.oracle)
        # 原失败实帧y195出现的合法DAC颜色；它已在69×34面板下方。
        initial.paste((44, 73, 142), (16, 195, 32, 196))
        changed = initial.copy()
        changed.paste((56, 85, 154), (16, 195, 32, 196))
        expected, actual = Image.new("RGB", menu.SIZE), Image.new("RGB", menu.SIZE)
        old_box = (16, 184, 32, 196)
        expected.paste(initial.crop(old_box), old_box[:2])
        actual.paste(changed.crop(old_box), old_box[:2])
        self.assertEqual(menu.compare(expected, actual)["bad_pixels"], 16)
        self.assertIsNotNone(menu.restored_browse(changed, initial, self.oracle))
        paste(changed, self.assets["notice"], (98, 146))
        self.assertTrue(self.oracle.cancellation_notice(changed))
        self.assertIsNone(menu.restored_browse(changed, initial, self.oracle))

    def test_panel_size_and_dynamic_border_are_rejected(self):
        original = Path(__file__).resolve().parents[2] / "original_game"
        invalid = [Image.new("P", (69, 35)), self.panel.copy()]
        invalid[1].putpixel((11, 29), 224)
        for panel, message in zip(invalid, ("69×34", "动态水色")):
            with self.subTest(message=message), \
                    patch.object(menu, "decode_image", side_effect=[self.assets["cursor"].copy(), panel]), \
                    self.assertRaisesRegex(ValueError, message):
                menu.resource_templates(original / "FDOTHER.DAT", original / "FDTXT.DAT")


if __name__ == "__main__":
    unittest.main()
