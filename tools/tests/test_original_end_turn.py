"""B1.3a纯数据与导航门回归，不启动SDL、DOSBox或宿主驱动。"""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import Mock, patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_end_turn as end


def checksum(data):
    data[-4:] = sum(data[:-4]).to_bytes(4, "little")
    return bytes(data)


def snapshot():
    data = bytearray((index * 31 + 7) % 256 for index in range(0x59cb))
    data[0] = 1
    data[end.META:end.META + 18] = bytes((1, 12, 0, 0, 15, 6, 20, 6, 5, 4, 0, 0, 0, 0, 0, 1, 1, 1))
    for index in range(12):
        offset = end.UNITS + index * end.STRIDE
        data[offset:offset + end.STRIDE] = bytes(end.STRIDE)
        data[offset:offset + 2] = bytes((7 + index % 3, 20 + index % 3))
        data[offset + 6:offset + 9] = bytes((2 if index < 4 else 0, index, index + 1))
        struct.pack_into("<HH", data, offset + 0x40, 30, 30)
    return checksum(data)


class EndRoundEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.before = snapshot()
        data = bytearray(self.before)
        data[end.META] = 2
        self.after = checksum(data)

    def test_valid_next_round_save_and_unaligned_random_fields(self):
        data = bytearray(self.after)
        struct.pack_into("<H", data, end.UNITS + 0x40, 19)
        data[end.UNITS + 4 * end.STRIDE] = 11  # AI随机相关位置只登记，不作一致性门。
        data[end.META + 3:end.META + 5] = bytes((1, 16))
        data[end.META + 7:end.META + 9] = bytes((5, 4))
        report = end.check_saved_round(self.before, checksum(data), 937)
        self.assertEqual(report["turn"], 2)
        self.assertEqual(report["unit_count"], 12)
        self.assertEqual(report["roster_count"], 4)
        self.assertEqual(report["focus"], [6, 20])
        self.assertEqual(report["observed_camera"], [1, 16])
        self.assertTrue(report["all_acted_cleared"])

    def test_wrong_round_focus_acted_identity_or_unrelated_changes_fail(self):
        for offset in (end.META, end.META + 1, end.META + 5, end.META + 7,
                       end.META + 9, end.META + 10, end.META + 18,
                       end.UNITS + 5, end.UNITS + 7, 0, 940, 0x8a3 + 4 * 80, 0x30a3, 0x312b):
            with self.subTest(offset=offset):
                data = bytearray(self.after)
                data[offset] ^= 0x80 if offset == end.UNITS + 5 else 1
                with self.assertRaises(AssertionError):
                    end.check_saved_round(self.before, checksum(data), 937)

    def test_initial_fixture_rejects_noninitial_or_ineligible_actor_state(self):
        with patch.object(end, "decode_save", return_value=self.before):
            plain, focus = end.validate_initial("fixture", 1)
            self.assertEqual(plain, self.before)
            self.assertEqual(focus, (148, 124))
        for offset, value in ((end.META, 2), (end.META + 1, 11),
                              (end.META + 9, 33),
                              (end.UNITS + 5, 0x80), (end.UNITS + 0x25, 1),
                              (end.UNITS + 0x40, 29)):
            data = bytearray(self.before)
            data[offset] = value
            with self.subTest(offset=offset), patch.object(end, "decode_save", return_value=checksum(data)), \
                    self.assertRaises((ValueError, AssertionError)):
                end.validate_initial("fixture", 1)

    def test_independent_roster_hidden_actor_and_nonzero_magic_bitmap_are_allowed(self):
        data = bytearray(self.before)
        data[end.UNITS + 9 * end.STRIDE + 5] = 1
        data[end.UNITS + end.STRIDE + 0x1a] = 1
        for roster_count in (0, 4, 31, 32):
            data[end.META + 9] = roster_count
            with self.subTest(roster_count=roster_count), patch.object(end, "decode_save", return_value=checksum(data)):
                plain, _ = end.validate_initial("fixture", 1)
                self.assertEqual(plain[end.META + 9], roster_count)

    def test_occupied_initial_focus_has_bounded_steps_to_empty_cell(self):
        data = bytearray(self.before)
        data[end.META + 5:end.META + 9] = bytes((7, 20, 7, 5))
        steps = end.empty_focus_navigation(checksum(data))
        self.assertEqual(steps, [("LEFT", (148, 124))])
        self.assertEqual(end.empty_focus_navigation(self.before), [])

    def test_round_phase_gate_accepts_original_visible_count_and_rejects_duplicate_or_reordered_phases(self):
        lines = ("field end turn: turn=1 phase=2 remaining=4",
                 "field phase: turn=1 phase=1 remaining=0",
                 "field phase: turn=1 phase=0 remaining=7",
                 "field phase: turn=2 phase=2 remaining=4")
        end.check_round_phase_log("\n".join(lines))
        for bad in (lines[:-1], lines + lines[:1], (lines[0], lines[2], lines[1], lines[3]),
                    lines + ("field phase: turn=3 phase=2 remaining=4",)):
            with self.subTest(bad=bad), self.assertRaises(AssertionError):
                end.check_round_phase_log("\n".join(bad))

    def test_missing_round_banner_sends_no_more_inputs(self):
        image = Image.new("RGB", (320, 200))
        capture = Mock(last=image)
        def wait(capture, name, predicate, report, **kwargs):
            if name.endswith("round-002"):
                raise TimeoutError("round 2 not observed")
            return image, {"kind": "browse"}
        with patch.object(end, "wait_title"), patch.object(end, "wait_state", side_effect=wait):
            with self.assertRaisesRegex(TimeoutError, "not observed"):
                end.exercise(capture, "original", {}, (148, 124), {3: [image] * 3},
                             image, Mock(), Mock(), [])
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                         ["UP", "ENTER", "ENTER", "DOWN", "ENTER", "ENTER"])


class RoundResourceTest(unittest.TestCase):
    def test_acted_save_disabled_requires_complete41_and_cannot_be_selected(self):
        assets = end.round_assets((Path(__file__).resolve().parents[2] / "original_game").resolve())
        oracle = end.RoundOracle(assets, (28, 76))
        def frame(selected, save_frame):
            image = Image.new("RGB", (320, 200))
            for item, index in enumerate((37 if selected == 0 else 36, save_frame, 43 if selected == 2 else 42, 45)):
                at = tuple(a + b for a, b in zip(oracle.focus, end.MENU_OFFSETS[item]))
                image.paste(assets["commands"][index].convert("RGB"), at)
            return image
        for selected in (0, 2):
            actual = frame(selected, 41)
            detail = oracle.secondary(actual, selected, save_available=False)
            self.assertEqual(detail["matches"][1]["checked_pixels"], 476)
            self.assertEqual(detail["matches"][1]["bad_pixels"], 0)
            self.assertIsNone(oracle.secondary(actual, selected))
            self.assertIsNone(oracle.secondary(frame(selected, 39), selected, save_available=False))
            self.assertIsNotNone(oracle.secondary(frame(selected, 39), selected))
            bad = actual.copy()
            px, py = next((x, y) for y in range(20) for x in range(24) if assets["commands"][41].getpixel((x, y)))
            at = (4 + px, 78 + py)
            bad.putpixel(at, tuple((channel + 128) % 256 for channel in bad.getpixel(at)))
            self.assertIsNone(oracle.secondary(bad, selected, save_available=False))
        self.assertIsNone(oracle.secondary(frame(0, 41), 1, save_available=False))

    def test_browse_waits_for_automatic_dialog_border_to_disappear(self):
        palette = [value for value in range(256) for _ in range(3)]
        cursor = Image.new("P", (24, 24), 200)
        command = Image.new("P", (24, 20), 100)
        border = Image.new("P", (310, 2), 75)
        for image in (cursor, command, border):
            image.putpalette(palette)
        oracle = end.RoundOracle({"cursor": cursor, "commands": [command] * 78,
                                  "dialog_top": border}, (148, 76))
        frame = Image.new("RGB", (320, 200))
        frame.paste(cursor.convert("RGB"), (148, 76))
        self.assertTrue(oracle.closed_browse(frame)["dialog_top_resource_absent"])
        frame.paste(border.convert("RGB"), (5, 112))
        self.assertIsNotNone(oracle.browse(frame))
        self.assertIsNone(oracle.closed_browse(frame))

    def test_transparent_disabled_overview_is_excluded_but_other_commands_are_required(self):
        palette = [value for value in range(256) for _ in range(3)]
        commands = []
        for index in range(78):
            command = Image.new("P", (24, 20), 0 if index == 38 else index + 1)
            command.putpalette(palette)
            commands.append(command)
        oracle = end.RoundOracle({"commands": commands}, (148, 124))
        frame = Image.new("RGB", (320, 200))
        for item, index in ((1, 40), (2, 42), (3, 45)):
            position = tuple(a + b for a, b in zip(oracle.focus, end.MENU_OFFSETS[item]))
            frame.paste(commands[index].convert("RGB"), position)
        self.assertIsNone(oracle.secondary(frame, 1))
        result = oracle.secondary(frame, 1, overview_available=False)
        self.assertTrue(result["matches"][0]["excluded"])
        self.assertEqual(len(result["matches"]), 4)
        position = tuple(a + b for a, b in zip(oracle.focus, end.MENU_OFFSETS[0]))
        frame.paste(commands[36].convert("RGB"), position)
        self.assertIsNone(oracle.secondary(frame, 1, overview_available=False))
        self.assertIsNone(oracle.secondary(Image.new("RGB", (320, 200)), 1, overview_available=False))

    def test_enabled_overview_is_default_zero_and_its_resource_is_required(self):
        palette = [value for value in range(256) for _ in range(3)]
        commands = []
        for index in range(78):
            image = Image.new("P", (24, 20), index + 1)
            image.putpalette(palette)
            commands.append(image)
        oracle = end.RoundOracle({"commands": commands}, (148, 124))
        frame = Image.new("RGB", (320, 200))
        for item, index in ((0, 37), (1, 39), (2, 42), (3, 45)):
            position = tuple(a + b for a, b in zip(oracle.focus, end.MENU_OFFSETS[item]))
            frame.paste(commands[index].convert("RGB"), position)
        self.assertIsNotNone(oracle.secondary(frame, 0, overview_available=True))
        frame.paste((0, 0, 0), (148, 104, 172, 124))
        self.assertIsNone(oracle.secondary(frame, 0, overview_available=True))

    def test_leader_composite_proves_final_focus_despite_sprite_occlusion(self):
        palette = [value for value in range(256) for _ in range(3)]
        cursor = Image.new("P", (24, 24), 200)
        cursor.putpalette(palette)
        command = Image.new("P", (24, 20), 100)
        command.putpalette(palette)
        frame_data = bytes((23, 33)) * 24
        directory = struct.pack("<HHH", 24, 24, 1680)
        directory += struct.pack("<1680I", *(6 + 1680 * 4 + i * len(frame_data) for i in range(1680)))
        source = Mock()
        source.read_bytes.return_value = directory + frame_data * 1680
        templates = end.leader_cursor_templates(source, cursor, palette)
        self.assertEqual(len(templates), 12)
        self.assertEqual(templates[0].getpixel((0, 6)), 33)
        self.assertEqual(templates[0].getpixel((0, 29)), 200)
        assets = {"cursor": cursor, "commands": [command] * 78, "leader_cursors": templates}
        frame = Image.new("RGB", (320, 200))
        frame.paste(cursor.convert("RGB"), (148, 124))
        self.assertIsNone(end.browse_anywhere(frame, assets, leader=True))
        frame = Image.new("RGB", (320, 200))
        frame.paste(templates[0].convert("RGB"), (148, 118))
        self.assertIsNone(end.browse_anywhere(frame, assets))
        self.assertEqual(end.browse_anywhere(frame, assets, leader=True)["focus_screen"], [148, 124])
        frame.paste(templates[0].convert("RGB"), (196, 70))
        self.assertIsNone(end.browse_anywhere(frame, assets, leader=True))
        source.read_bytes.return_value = b"invalid"
        with self.assertRaises(ValueError):
            end.leader_cursor_templates(source, cursor, palette)

    def test_initial_cursor_accepts_only_the_declared_visible_rows(self):
        palette = [value for value in range(256) for _ in range(3)]
        cursor = Image.new("P", (24, 24), 200)
        cursor.putpalette(palette)
        command = Image.new("P", (24, 20), 100)
        command.putpalette(palette)
        assets = {"cursor": cursor, "commands": [command] * 78}
        frame = Image.new("RGB", (320, 200))
        frame.paste(cursor.crop((0, 0, 24, 21)).convert("RGB"), (148, 124))
        oracle = end.RoundOracle(assets, (148, 124))
        self.assertIsNone(oracle.browse(frame))
        self.assertEqual(oracle.initial_browse(frame)["visible_cursor_rows"], 21)
        frame.putpixel((148, 124), (0, 0, 0))
        self.assertIsNone(oracle.initial_browse(frame))

    def test_browse_cursor_requires_one_unambiguous_resource_match(self):
        palette = [channel for value in range(256) for channel in (value, value, value)]
        cursor = Image.new("P", (24, 24), 0)
        cursor.putpalette(palette)
        for point in ((0, 0), (23, 0), (0, 23), (23, 23)):
            cursor.putpixel(point, 200)
        command = Image.new("P", (24, 20), 100)
        command.putpalette(palette)
        assets = {"cursor": cursor, "commands": [command] * 78}
        frame = Image.new("RGB", (320, 200), (30, 60, 90))
        mask = cursor.point(lambda value: 255 if value else 0, mode="L")
        frame.paste(cursor.convert("RGB"), (148, 124), mask)
        self.assertEqual(end.browse_anywhere(frame, assets)["focus_screen"], [148, 124])
        frame.paste(cursor.convert("RGB"), (196, 76), mask)
        self.assertIsNone(end.browse_anywhere(frame, assets))

    def test_lmi_literal_run_and_strict_bounds(self):
        data = struct.pack("<HH", 3, 1) + bytes((0xc2, 74, 0xc0))
        self.assertEqual(list(end.decode_lmi(data).getdata()), [74, 74, 192])
        for bad in (b"", struct.pack("<HH", 0, 1), data[:-1], data + b"\0",
                    struct.pack("<HH", 1, 1) + bytes((0xc2, 74))):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                end.decode_lmi(bad)

    def test_banner_composes_zero_padded_002_at_original_number_coordinates(self):
        count = 93
        frames = [b"\0"] * count
        for index, color in ((42, 2), (44, 3)):
            frames[index] = struct.pack("<HH", 6, 8) + bytes((5, color)) * 8
        pixels = 87 * 20
        frames[91] = struct.pack("<HH", 87, 20) + bytes((0xff, 1)) * (pixels // 63)
        if pixels % 63:
            frames[91] += bytes((0xc0 + pixels % 63, 1))
        offsets, offset = [], 6 + count * 4
        for frame in frames:
            offsets.append(offset)
            offset += len(frame)
        sheet = b"LMI1" + struct.pack("<H", count) + struct.pack("<93I", *offsets) + b"".join(frames)
        resource = Mock()
        with patch.object(end, "archive_entry", return_value=sheet):
            banner = end.round_two_template(resource, [i for i in range(256) for _ in range(3)])
        self.assertEqual(banner.size, (87, 20))
        self.assertEqual([banner.getpixel((x, 7)) for x in (51, 57, 63)], [2, 2, 3])
        self.assertEqual(banner.getpixel((50, 7)), 1)
        truncated = sheet[:6 + count * 4 - 1]
        with patch.object(end, "archive_entry", return_value=truncated), self.assertRaises(ValueError):
            end.round_two_template(resource, [0] * 768)


if __name__ == "__main__":
    unittest.main()
