"""原版ITEM只读校准：真实资源与纯协议回归，不启动窗口。"""
from contextlib import contextmanager
import hashlib
import json
import os
import re
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import fd2_item_list_oracle as oracle
import run_original_item_list as probe


class Fixture:
    @classmethod
    def setUpClass(cls):
        (cls.source, cls.focus, cls.assets, cls.actor, cls.selection,
         cls.templates, cls.fields) = probe.prepare(ROOT / "original_game")

    def list_frame(self, selected=0):
        image = Image.new("RGB", (320, 200))
        for name in (f"selected_{selected}" if selected >= 0 else "unselected", "actor_name", "current_max_hp", "current_max_mp"):
            at, part = self.templates[name]
            image.paste(part.convert("RGB"), at)
        return image

    def menu_frame(self, selected=0, *, give_enabled=False, actor_overlay=True, role="original"):
        image = Image.new("RGB", (320, 200))
        for item, command in enumerate((8, 9, 10, 11)):
            disabled = item == 1 and not give_enabled
            index = command * 3 + (2 if disabled else int(item == selected))
            part = self.assets["upper_commands"][index][0] if item == 0 and actor_overlay else self.assets["commands"][index]
            at = tuple(a + b for a, b in zip((52, 76), probe.MENU_OFFSETS[item]))
            image.paste(part.convert("RGB"), at)
        return image

    def command_frame(self):
        image = Image.new("RGB", (320, 200))
        for item, index in enumerate((2, 5, 7, 9)):
            part = self.assets["upper_commands"][index][0] if item == 0 else self.assets["commands"][index]
            at = tuple(a + b for a, b in zip((52, 76), probe.MENU_OFFSETS[item]))
            image.paste(part.convert("RGB"), at)
        return image


class ResourceTest(Fixture, unittest.TestCase):
    def test_native_signed_word_numbers_and_complete_positive_negative_mp_bars(self):
        sheet = probe.archive_entry((ROOT / "original_game/FDOTHER.DAT").read_bytes(), 5)
        for current, maximum, length in ((0, 40, 0), (1, 40, 3), (20, 40, 51), (40, 40, 102),
                (65516, 40, -49), (65517, 40, -46), (0, 0, None)):
            canvas = Image.new("P", (120, 12), 205); canvas.putpalette(self.assets["palette"]); expected = canvas.copy()
            result = oracle.word_bar(canvas, sheet, current, maximum, 2, 3, 26)
            def paste(frame, offset): expected.paste(oracle.raw_icon(sheet, frame, (1, 5), self.assets["palette"]), (2 + offset, 3))
            if length is not None:
                if length:
                    paste(26, 0)
                    for x in range(1, length): paste(27, x)
                    paste(28, max(1, length))
                else:
                    for x in range(1, 102): paste(29, x)
                    paste(30, 102)
                self.assertEqual(result["native_length"], length)
            else: self.assertFalse(result["drawn"])
            self.assertEqual(canvas.tobytes(), expected.tobytes(), (current, maximum))
        for current in (0, 1):
            actor = bytearray(self.source[probe.UNITS:probe.UNITS + probe.STRIDE])
            struct.pack_into("<HH", actor, 0x44, (current - 20) & 65535, 40)
            _, templates, fields = oracle.inventory_templates(ROOT / "original_game", actor, self.assets)
            self.assertEqual(fields["mp"], 65516 + current)
            self.assertEqual(fields["signed_word_display"]["mp"]["signed_input"], current - 20)
            self.assertEqual(fields["signed_word_display"]["mp"]["nonnegative_number_input"], 0)
            self.assertEqual(templates["mp_bar"][1].size, (103, 5))
            top = probe.sheet_frame(sheet, 20, lmi=True); top.putpalette(self.assets["palette"])
            oracle.number(top, sheet, 0, 175, 52, 42, 3); oracle.number(top, sheet, 40, 201, 52, 31, 3)
            self.assertEqual(templates["current_max_mp"][1].tobytes(), top.crop((175, 52, 219, 60)).tobytes())

    def test_native_fixed_width_numbers_use_original_overflow_resources_at_boundaries(self):
        from fd2_magic_list_oracle import digit as digit_resource
        sheet = probe.archive_entry((ROOT / "original_game/FDOTHER.DAT").read_bytes(), 5)
        for bank in (31, 42, 119):
            for width, values in ((2, (-1, 0, 99, 100, 255)), (3, (-1, 0, 999, 1000, 65535))):
                for value in values:
                    canvas = Image.new("P", (40, 18), 205); canvas.putpalette(self.assets["palette"])
                    expected = canvas.copy()
                    oracle.number(canvas, sheet, value, 2, 3, bank, width)
                    if value > (99 if width == 2 else 999):
                        frame = 93 if width == 2 else bank + 10
                        image = oracle.rle_icon(sheet, frame, (12 if width == 2 else 17 if bank == 119 else 18, 8), self.assets["palette"])
                        expected.paste(image, (2, 3), image.point(lambda pixel: 255 if pixel else 0, mode="L"))
                    else:
                        for index, digit in enumerate(f"{max(value, 0):0{width}d}"):
                            image = digit_resource(sheet, bank + int(digit))
                            image.putpalette(self.assets["palette"])
                            expected.paste(image, (2 + index * 6, 3), image.point(lambda pixel: 255 if pixel else 0, mode="L"))
                    self.assertEqual(canvas.tobytes(), expected.tobytes(), (bank, width, value))
        for experience in (99, 100, 255):
            actor = bytearray(self.source[probe.UNITS:probe.UNITS + probe.STRIDE]); actor[0x3c] = experience
            _, templates, fields = oracle.inventory_templates(ROOT / "original_game", actor, self.assets)
            top = probe.sheet_frame(sheet, 20, lmi=True); top.putpalette(self.assets["palette"])
            oracle.number(top, sheet, experience, 65, 37, 42, 2)
            self.assertEqual(templates["experience"][1].tobytes(), top.crop((65, 37, 77, 45)).tobytes())
            self.assertEqual(fields["experience"], experience)  # 原始字节不裁剪/按身份隐藏。

    def test_all_29_equipment_profile_rows_match_strict_le_and_actual_c_table(self):
        rows = oracle.equipment_rows(ROOT / "original_game/FD2.EXE")
        self.assertEqual(len(rows), 203)
        self.assertEqual(hashlib.sha256(rows).hexdigest(), oracle.EQUIPMENT_SHA256)
        implementation = (ROOT / "src/field_item.c").read_text(encoding="utf-8")
        match = re.search(r"profiles\[29\]\[7\]\s*=\s*\{(.*?)\n\s*\};", implementation, re.S)
        self.assertIsNotNone(match)
        stored = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})\b", match.group(1)))
        self.assertEqual(stored, rows)
        self.assertEqual(rows[:7], bytes.fromhex("ffffffffffff01"))
        self.assertNotIn(1, rows[:6])  # 第7字节01不是许可类别。

    def test_eight_raw_slots_two_columns_mp_and_no_primary_attribute_resources(self):
        actor = bytearray(self.source[probe.UNITS:probe.UNITS + probe.STRIDE])
        ids = (0, 132, 192, 206, 196, 1, 193, 198)
        for slot, item in enumerate(ids):
            actor[10 + slot * 2:12 + slot * 2] = bytes((0, item))
        _, templates, fields = oracle.inventory_templates(ROOT / "original_game", actor, self.assets)
        self.assertEqual([row["item_id"] for row in fields["slots"]], list(ids))
        self.assertEqual(fields["slots"][3]["effect_frame"], 67)
        self.assertEqual(fields["slots"][4]["effect_frame"], 41)
        image = templates["selected_7"][1]
        self.assertEqual(image.size, (310, 99))
        self.assertNotEqual(image.tobytes(), templates["selected_3"][1].tobytes())
        # 右列第五槽的category图必须完整复制到x=163，而不是继续画到第5行。
        sheet = probe.archive_entry((ROOT / "original_game/FDOTHER.DAT").read_bytes(), 5)
        icon = oracle.raw_icon(sheet, fields["slots"][4]["category_frame"], (24, 20), self.assets["palette"])
        self.assertEqual(image.crop((158, 7, 182, 27)).tobytes(), icon.tobytes())

    def test_strict_le_table_and_real_start_three_slots(self):
        table, evidence = oracle.item_table(ROOT / "original_game/FD2.EXE")
        self.assertEqual((len(table), evidence["canonical_va"], evidence["object_offset"]), (4945, 0x602ad, 0x2ad))
        self.assertEqual(hashlib.sha256(table).hexdigest(), oracle.TABLE_SHA256)
        self.assertEqual([(item["flags"], item["item_id"], item["use_code"], item["value"])
                          for item in self.fields["slots"]], [(64, 0, 0, 10), (64, 132, 0, 8), (0, 192, 5, 40)])
        self.assertEqual([item["name_tokens"] for item in self.fields["slots"]], [[182, 176], [283, 107], [308, 309]])
        self.assertEqual(self.fields["actor_tokens"], [36, 37])

    def test_each_selected_full_rectangles_and_wrong_states_rejected(self):
        for selected in range(3):
            image = self.list_frame(selected)
            detail = probe.list_page(image, self.templates, self.fields, selected)
            self.assertEqual(detail["checked_pixels"], 31955)
            self.assertEqual([part["checked_pixels"] for part in detail["rectangles"]], [30690, 561, 352, 352])
            self.assertEqual((detail["hp"], detail["max_hp"], detail["mp"], detail["max_mp"]), (42, 42, 0, 0))
            self.assertEqual(detail["selected_raw_slot"], selected)
            for other in range(3):
                if selected != other:
                    self.assertIsNone(probe.list_page(image, self.templates, self.fields, other))
            for point in ((5, 94), (98, 13), (267, 41), (267, 59), (13, 101), (42, 103), (110, 107), (135, 107)):
                bad = image.copy()
                bad.putpixel(point, (255, 0, 0))
                self.assertIsNone(probe.list_page(bad, self.templates, self.fields, selected))
        self.assertIsNone(probe.list_page(self.list_frame(-1), self.templates, self.fields))
        self.assertIsNone(probe.list_page(Image.new("RGB", (640, 400)), self.templates, self.fields))
        self.assertIsNone(probe.list_page(Image.new("RGB", (320, 200)), self.templates, self.fields))

    def test_selected_only_recolors_actual_name_foreground(self):
        unselected = self.templates["unselected"][1]
        for index in range(3):
            selected = self.templates[f"selected_{index}"][1]
            differences = [(a, b) for a, b in zip(unselected.getdata(), selected.getdata()) if a != b]
            self.assertTrue(differences)
            self.assertEqual(set(differences), {(205, 201)})

    def test_submenu_all_four_resources_and_actor_overlap(self):
        for selected, expected in ((0, [25, 29, 30, 33]), (2, [24, 29, 31, 33]), (3, [24, 29, 30, 34])):
            image = self.menu_frame(selected)
            detail = probe.submenu_page(image, self.assets, (52, 76), self.templates, self.fields, selected)
            self.assertEqual([match["frame"] for match in detail["matches"]], expected)
            self.assertTrue(detail["inventory_list_absent"])
            self.assertIsNone(probe.submenu_page(self.menu_frame(selected, give_enabled=True), self.assets, (52, 76), self.templates, self.fields, selected))
            self.assertIsNone(probe.submenu_page(self.menu_frame(selected, actor_overlay=False), self.assets, (52, 76), self.templates, self.fields, selected))
        with self.assertRaises(ValueError):
            probe.submenu_page(self.menu_frame(), self.assets, (52, 76), self.templates, self.fields, 1)

    def test_sdl_equip_and_drop_use_full_enabled_frames_and_reject_old_gray_equip(self):
        frame = self.menu_frame(role="sdl")
        result = probe.submenu_page(frame, self.assets, (52, 76), self.templates, self.fields, role="sdl")
        self.assertEqual([item["frame"] for item in result["matches"]], [25, 29, 30, 33])
        self.assertEqual(result["disabled"], [0, 1, 0, 0])
        self.assertFalse(result["capability_equivalence_claimed"])
        self.assertIsNotNone(probe.submenu_page(frame, self.assets, (52, 76), self.templates, self.fields))
        self.assertIsNone(probe.submenu_page(frame, self.assets, (52, 76), self.templates, self.fields, 2, role="sdl"))
        gray = frame.copy()
        gray.paste(self.assets["commands"][32].convert("RGB"), (76, 78))
        self.assertIsNone(probe.submenu_page(gray, self.assets, (52, 76), self.templates, self.fields, role="sdl"))
        selected = probe.submenu_page(self.menu_frame(3, role="sdl"), self.assets, (52, 76), self.templates, self.fields, 3, role="sdl")
        self.assertEqual([item["frame"] for item in selected["matches"]], [24, 29, 30, 34])
        disabled_drop = frame.copy()
        at = tuple(a + b for a, b in zip((52, 76), probe.MENU_OFFSETS[3]))
        disabled_drop.paste(self.assets["commands"][35].convert("RGB"), at)
        self.assertIsNone(probe.submenu_page(disabled_drop, self.assets, (52, 76), self.templates, self.fields, role="sdl"))

    def test_command_first_enabled_and_cancelled_browse_requires_plain_terrain(self):
        command = probe.PlayerOracle(self.source, self.assets, [], {})
        self.assertTrue(probe.command_page(self.command_frame(), command, (52, 76), self.templates, self.fields)["native_first_enabled_item"])
        self.assertIsNone(probe.command_page(self.menu_frame(), command, (52, 76), self.templates, self.fields))
        image = Image.new("RGB", (320, 200))
        part = self.actor["templates"][0]
        image.paste(part.convert("RGB"), (52, 70))
        image.paste(self.selection["base"].convert("RGB"), self.selection["position"])
        self.assertIsNotNone(probe.cancelled_browse(image, self.assets, self.actor, self.selection, self.templates, self.fields))
        image.paste(self.selection["ranges"][0][1].convert("RGB"), self.selection["position"])
        self.assertIsNone(probe.cancelled_browse(image, self.assets, self.actor, self.selection, self.templates, self.fields))

    def test_unknown_source_or_executable_rejected_before_le_allocation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad"
            path.write_bytes(b"not the verified source")
            with patch.object(oracle, "build_relbase_image") as build, self.assertRaises(ValueError):
                oracle.item_table(path)
            build.assert_not_called()
            with patch.object(oracle, "validate_initial") as validate, self.assertRaises(ValueError):
                oracle.prepare(ROOT / "original_game", path)
            validate.assert_not_called()

    def test_raw_icon_dimension_or_directory_error_rejected(self):
        sheet = bytearray(b"LMI1" + struct.pack("<HIII", 3, 18, 22, 26) + b"\0" * 12)
        for broken in (bytes(sheet[:5]), bytes(sheet), bytes(sheet[:6] + struct.pack("<III", 99, 100, 101) + sheet[18:])):
            with self.subTest(length=len(broken)), self.assertRaises(ValueError):
                oracle.raw_icon(broken, 0, (24, 20), self.assets["palette"])


class InputTest(Fixture, unittest.TestCase):
    def test_only_code0_confirmations_and_exact_layered_cancel(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"readonly copy")
            image = Image.new("RGB", (320, 200))
            capture = Mock(last=image)
            report = {"checkpoints": []}
            with patch.object(probe, "save_checkpoint"), patch.object(probe, "wait_title"), \
                    patch.object(probe, "wait_state", return_value=(image, {"focus_screen": [52, 76]})) as wait:
                probe.exercise(capture, self.source, self.focus, self.assets, self.actor, self.selection,
                               self.templates, self.fields, {3: [image] * 3}, save, b"readonly copy", save.stat().st_mtime_ns, report)
            self.assertEqual([call.args[0] for call in capture.key.call_args_list],
                ["UP", "ENTER", "Z", "ENTER", "ENTER", "ENTER", "RIGHT", "DOWN", "UP", "LEFT", "ENTER",
                 "ENTER", "SPACE", "DOWN", "ENTER", "SPACE", "RIGHT", "LEFT", "DOWN", "DOWN", "UP",
                 "ESC", "ENTER", "ESC", "ESC", "ESC", "ESC"])
            labels = [call.args[1] for call in wait.call_args_list]
            self.assertEqual(labels[-6:], ["list-cancel-item-submenu", "use-list-reopened-zero", "list-cancel-item-submenu-again",
                                          "submenu-cancel-command", "command-cancel-selection", "selection-cancel-browse"])
            self.assertEqual(report["input_scope"]["valid_use_confirmations"], 0)

    def test_failed_checkpoint_stops_before_sending_next_key(self):
        capture = Mock(last=Image.new("RGB", (320, 200)))
        with patch.object(probe, "save_checkpoint"), patch.object(probe, "wait_title"), \
                patch.object(probe, "wait_state", side_effect=TimeoutError("resource gate")), \
                self.assertRaises(TimeoutError):
            probe.exercise(capture, self.source, self.focus, self.assets, self.actor, self.selection,
                           self.templates, self.fields, {3: [capture.last] * 3}, Path("unused"), b"", 0, {"checkpoints": []})
        self.assertEqual([call.args[0] for call in capture.key.call_args_list], ["UP", "ENTER"])

    def test_sdl_down_selects_drop_and_other_directions_keep_exact_supported_icons(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"
            save.write_bytes(b"copy")
            frame = self.menu_frame(role="sdl")
            capture = Mock(last=frame)
            checked = []
            def wait(capture, name, predicate, checkpoints, **kwargs):
                if name.startswith("submenu-") and name != "submenu-cancel-command":
                    selected = {"submenu-right": 2, "submenu-down": 3}.get(name, 0)
                    detail = predicate(self.menu_frame(selected, role="sdl"))
                    self.assertIsNotNone(detail)
                    self.assertEqual(detail["selected"], selected)
                    self.assertEqual([item["frame"] for item in detail["matches"]],
                                     [24, 29, 30, 34] if selected == 3 else [24, 29, 31, 33] if selected == 2 else [25, 29, 30, 33])
                    checked.append(name)
                return frame, {"focus_screen": [52, 76]}
            with patch.object(probe, "wait_title"), patch.object(probe, "save_checkpoint"), patch.object(probe, "wait_state", side_effect=wait):
                probe.exercise(capture, self.source, self.focus, self.assets, self.actor, self.selection, self.templates,
                    self.fields, {3: [frame] * 3}, save, b"copy", save.stat().st_mtime_ns, {"kind": "sdl", "checkpoints": []})
            self.assertEqual(checked, ["submenu-right", "submenu-down", "submenu-up", "submenu-left"])
            self.assertEqual(len(capture.key.call_args_list), 27)


class HerbTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        original = ROOT / "original_game"
        cls.base = probe.decode_save(original / "START.SAV")
        cls.encrypted = (original / "START.SAV").read_bytes()
        cls.derived, cls.injured, cls.manifest = oracle.derive_injured(cls.base, cls.encrypted)
        cls.cases = {"herb-full": probe.prepare(original, herb=True),
                     "herb-injured": probe.prepare(original, source=cls.injured, herb=True)}

    @staticmethod
    def detail_frame(assets):
        image = Image.new("RGB", (320, 200))
        templates = assets["herb"]["after_templates"]
        for name in ("unselected", "actor_name", "current_max_hp", "current_max_mp", "level", "experience"):
            at, part = templates[name]
            image.paste(part.convert("RGB"), at)
        return image

    @staticmethod
    def target_frame(assets, actor, selection, *, ordinary_cursor=False, range_on=True):
        image = Image.new("RGB", (320, 200))
        target = actor if ordinary_cursor else assets["herb"]["target_actor"]
        part = target["templates"][0]
        mask = part.point(lambda value: 255 if value else 0, mode="L")
        image.paste(part.convert("RGB"), (52, 70), mask)
        part = selection["ranges"][0][1] if range_on else selection["base"]
        image.paste(part.convert("RGB"), selection["position"])
        return image

    @staticmethod
    def acted_frame(assets, selection):
        image = Image.new("RGB", (320, 200))
        image.paste(assets["herb"]["acted_composites"][0].convert("RGB"), (52, 70))
        image.paste(selection["base"].convert("RGB"), selection["position"])
        terrain = assets["herb"]["detail_close_terrain"]
        image.paste(terrain["image"].convert("RGB"), terrain["position"])
        return image

    def test_injured_manifest_only_hp_ex_checksum_and_xor_roundtrip(self):
        changed = {index for index, (a, b) in enumerate(zip(self.base, self.injured)) if a != b}
        self.assertEqual(changed, {probe.UNITS + 0x3c, probe.UNITS + 0x40, len(self.base) - 4})
        self.assertEqual(self.injured[probe.UNITS:probe.UNITS + 2], b"\x07\x0e")
        self.assertEqual((self.injured[probe.UNITS + 0x3c], struct.unpack_from("<H", self.injured, probe.UNITS + 0x40)[0]), (99, 20))
        self.assertEqual(self.injured[0x8a3:probe.UNITS], self.base[0x8a3:probe.UNITS])
        self.assertEqual(self.manifest["derived_sha256"], "15cdaa8ce74038f2031d94dcf1983c0597b506e776a2b6028c87ffacb45ec17d")
        self.assertEqual(self.manifest["checksum_after"], "a5672300")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "derived.SAV"
            path.write_bytes(self.derived)
            self.assertEqual(probe.decode_save(path), self.injured)
        with self.assertRaises(ValueError):
            oracle.derive_injured(self.injured, self.encrypted)

    def test_target_frame1_and_separate_range_lut_both_required(self):
        for name, case in self.cases.items():
            source, focus, assets, actor, selection, templates, fields = case
            with self.subTest(case=name):
                frame = self.target_frame(assets, actor, selection)
                detail = probe.herb_target(frame, assets, selection, templates, fields)
                self.assertEqual((detail["selection_frame"], detail["selection_mode"]), (1, 2))
                self.assertEqual(detail["range"]["checked_pixels"], 576)
                self.assertEqual(fields["herb_contract"]["target_candidates"], [0])
                self.assertEqual(fields["herb_contract"]["raw_heal_static_range"], [36, 39])
                for kwargs in ({"ordinary_cursor": True}, {"range_on": False}):
                    self.assertIsNone(probe.herb_target(self.target_frame(assets, actor, selection, **kwargs), assets, selection, templates, fields))

    def test_after_use_whole_two_slot_page_hp_mp_level_ex_and_static_tail_scope(self):
        for name, case in self.cases.items():
            source, focus, assets, actor, selection, templates, fields = case
            with self.subTest(case=name):
                frame = self.detail_frame(assets)
                detail = probe.herb_detail(frame, assets)
                self.assertEqual(detail["checked_pixels"], 32147)
                self.assertEqual((detail["hp"], detail["max_hp"], detail["mp"], detail["max_mp"], detail["level"]), (42, 42, 0, 0, 1))
                self.assertEqual(detail["experience"], 99 if name == "herb-injured" else 0)
                self.assertEqual(detail["visible_inventory_ids"], [0, 132])
                self.assertEqual(detail["expected_inventory_hex_static_only"], "40004084800080008000800080c980c9")
                self.assertFalse(detail["runtime_raw_inventory_measured"])
                old = frame.copy()
                old.paste(templates["unselected"][1].convert("RGB"), (5, 94))
                self.assertIsNone(probe.herb_detail(old, assets))
                for point in ((5, 94), (267, 41), (267, 59), (157, 33), (157, 44)):
                    bad = frame.copy()
                    bad.putpixel(point, (255, 0, 255))
                    self.assertIsNone(probe.herb_detail(bad, assets))
        after = self.detail_frame(self.cases["herb-injured"][2])
        part = self.cases["herb-full"][2]["herb"]["after_templates"]["experience"]
        after.paste(part[1].convert("RGB"), part[0])
        self.assertIsNone(probe.herb_detail(after, self.cases["herb-injured"][2]))

    def test_acted_and_range_off_and_full_detail_close_terrain_required(self):
        for name, case in self.cases.items():
            source, focus, assets, actor, selection, templates, fields = case
            with self.subTest(case=name):
                frame = self.acted_frame(assets, selection)
                self.assertIsNotNone(probe.herb_browse(frame, source, assets, selection, templates, fields))
                wrong = frame.copy()
                wrong.paste(actor["templates"][0].convert("RGB"), (52, 70))
                self.assertIsNone(probe.herb_browse(wrong, source, assets, selection, templates, fields))
                wrong = frame.copy()
                wrong.paste(selection["ranges"][0][1].convert("RGB"), selection["position"])
                self.assertIsNone(probe.herb_browse(wrong, source, assets, selection, templates, fields))
                terrain = assets["herb"]["detail_close_terrain"]
                wrong = frame.copy()
                wrong.putpixel(terrain["position"], (255, 0, 255))
                self.assertIsNone(probe.herb_browse(wrong, source, assets, selection, templates, fields))
                self.assertIsNone(probe.herb_browse(self.detail_frame(assets), source, assets, selection, templates, fields))

    def test_herb_protocol_cancel_reopens_zero_one_commit_then_only_f2_and_close(self):
        expected = ["UP", "ENTER", "Z", "ENTER", "ENTER", "ENTER", "ENTER", "UP", "ENTER", "ESC", "UP", "ENTER", "ENTER", "F2", "ESC"]
        for name, case in self.cases.items():
            with self.subTest(case=name), tempfile.TemporaryDirectory() as directory:
                save = Path(directory) / "FD2.SAV"
                data = self.encrypted if name == "herb-full" else self.derived
                save.write_bytes(data)
                source, focus, assets, actor, selection, templates, fields = case
                image = Image.new("RGB", (320, 200))
                capture, report = Mock(last=image), {"checkpoints": []}
                with patch.object(probe, "wait_title"), patch.object(probe, "save_checkpoint"), \
                        patch.object(probe, "wait_state", return_value=(image, {"focus_screen": [52, 76]})) as wait:
                    probe.exercise(capture, *case, {3: [image] * 3}, save, data, save.stat().st_mtime_ns, report, scenario=name)
                self.assertEqual([call.args[0] for call in capture.key.call_args_list], expected)
                self.assertIn("herb-target-cancel-list-zero", [call.args[1] for call in wait.call_args_list])
                self.assertEqual(report["input_scope"]["valid_use_confirmations"], 1)
                self.assertFalse(report["input_scope"]["save_or_end_sent"])
                self.assertEqual(report["input_scope"]["post_use_skip_keys"], [])

    def test_target_or_effect_timeout_never_sends_retry_or_skip_key(self):
        case = self.cases["herb-full"]
        expected = ["UP", "ENTER", "Z", "ENTER", "ENTER", "ENTER", "ENTER", "UP", "ENTER", "ESC", "UP", "ENTER", "ENTER"]
        for stop, count in (("herb-target-first", 9), ("herb-once-used-acted-browse", 13)):
            with self.subTest(checkpoint=stop), tempfile.TemporaryDirectory() as directory:
                save = Path(directory) / "FD2.SAV"
                save.write_bytes(self.encrypted)
                image = Image.new("RGB", (320, 200))
                capture = Mock(last=image)
                def wait(capture, name, predicate, report, **kwargs):
                    if name == stop:
                        raise TimeoutError("resource gate")
                    return image, {"focus_screen": [52, 76]}
                with patch.object(probe, "wait_title"), patch.object(probe, "save_checkpoint"), \
                        patch.object(probe, "wait_state", side_effect=wait), self.assertRaises(TimeoutError):
                    probe.exercise(capture, *case, {3: [image] * 3}, save, self.encrypted, save.stat().st_mtime_ns,
                                   {"checkpoints": []}, scenario="herb-full")
                self.assertEqual([call.args[0] for call in capture.key.call_args_list], expected[:count])


class DropTest(Fixture, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        probe.prepare_drop(ROOT / "original_game", cls.source, cls.assets, cls.actor, cls.fields)
        cls.plan = cls.assets["drop"]
        cls.encrypted = (ROOT / "original_game/START.SAV").read_bytes()
        cls.boundary_cases = {}

    def boundary_case(self, scenario):
        if scenario not in self.boundary_cases:
            source, focus, assets, actor, selection, templates, fields = probe.prepare(ROOT / "original_game")
            if scenario == "drop-last":
                table, _ = oracle.item_table(ROOT / "original_game/FD2.EXE")
                _, source, _ = probe.derive_drop_last(source, self.encrypted, table)
                _, templates, updated = oracle.inventory_templates(ROOT / "original_game", source[probe.UNITS:probe.UNITS + probe.STRIDE], assets)
                fields = dict(updated, submenu=fields["submenu"])
            probe.prepare_drop(ROOT / "original_game", source, assets, actor, fields, scenario)
            self.boundary_cases[scenario] = (source, focus, assets, actor, selection, templates, fields)
        return self.boundary_cases[scenario]

    def measured(self):
        plain = bytearray(self.plan["expected_source"])
        plain[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3))
        plain[-4:] = sum(plain[:-4]).to_bytes(4, "little")
        encoded = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.source, plain))
        return bytes(plain), encoded

    def test_static_model_is_native_remove_and_recompute_not_heal_or_acted(self):
        expected = self.plan["expected_source"]
        self.assertEqual(expected[probe.UNITS + 0x0a:probe.UNITS + 0x1a].hex(), probe.AFTER_USE_INVENTORY)
        self.assertEqual([i for i, (a, b) in enumerate(zip(self.source[:-4], expected[:-4])) if a != b],
                         [probe.UNITS + 0x0e, probe.UNITS + 0x0f, probe.UNITS + 0x17])
        self.assertEqual(struct.unpack_from("<4H", expected, probe.UNITS + 0x48), (16, 12, 97, 2))
        self.assertEqual(expected[0x8a3:probe.UNITS], self.source[0x8a3:probe.UNITS])
        self.assertEqual(expected[probe.META:], self.source[probe.META:-4] + expected[-4:])
        self.assertEqual(expected[probe.UNITS + 5], 0)
        self.assertEqual(self.fields["drop_contract"]["measurement_navigation"]["to_world"], [6, 14])
        self.assertFalse(self.fields["drop_contract"]["runtime_raw_inventory_measured"])
        table, _ = oracle.item_table(ROOT / "original_game/FD2.EXE")
        broken = bytearray(table); broken[1] += 1
        with self.assertRaises(ValueError):
            probe.drop_expected_source(self.source, broken)
        with self.assertRaises(ValueError):
            probe.drop_expected_source(expected, table)

    def test_full_container_measurement_only_allows_original_cache_and_explicit_focus(self):
        measured, _ = self.measured()
        measured = bytearray(measured)
        measured[probe.UNITS + 2] ^= 1
        measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
        result = probe.check_load_measurement(self.plan["expected_source"], measured, self.plan["steps"])
        self.assertTrue(result["source_equal_except_declared_cache_and_measuring_focus"])
        for offset in (0x8a3, probe.UNITS - 1, 0x800, probe.UNITS + 5, probe.UNITS + 0x19,
                       probe.UNITS + 0x3c, probe.UNITS + 0x40, probe.UNITS + 0x48, probe.META + 3):
            with self.subTest(offset=offset):
                bad = bytearray(measured); bad[offset] ^= 1
                bad[-4:] = sum(bad[:-4]).to_bytes(4, "little")
                with self.assertRaises(AssertionError):
                    probe.check_load_measurement(self.plan["expected_source"], bad, self.plan["steps"])

    def test_two_slot_page_is_complete_and_normal_actor_is_required_after_cancel(self):
        before_frame = self.list_frame()
        for name in ("level","experience"):
            at, image = self.templates[name]; before_frame.paste(image.convert("RGB"),at)
        detail = probe.drop_list_page(before_frame, self.templates, self.fields)
        self.assertEqual((detail["kind"],detail["operation"],detail["list_mode"]),("drop_inventory_list","drop",0))
        self.assertEqual(probe.list_page(before_frame,self.templates,self.fields)["kind"],"use_inventory_list")
        after = self.plan["after_templates"]
        frame = Image.new("RGB", (320, 200))
        for name in ("selected_0", "actor_name", "current_max_hp", "current_max_mp", "level", "experience"):
            at, image = after[name]; frame.paste(image.convert("RGB"), at)
        self.assertEqual(probe.list_page(frame, after, self.plan["after_fields"])["checked_pixels"], 32147)
        self.assertIsNone(probe.list_page(before_frame, after, self.plan["after_fields"]))
        frame = Image.new("RGB", (320, 200))
        frame.paste(self.actor["templates"][0].convert("RGB"), (52, 70))
        frame.paste(self.selection["base"].convert("RGB"), self.selection["position"])
        self.assertIsNotNone(probe.drop_browse(frame, self.source, self.assets, self.actor,
            self.selection, after, self.plan["after_fields"]))
        herb = probe.prepare(ROOT / "original_game", herb=True)[2]["herb"]["acted_composites"][0]
        frame.paste(herb.convert("RGB"), (52, 70))
        self.assertIsNone(probe.drop_browse(frame, self.source, self.assets, self.actor,
            self.selection, after, self.plan["after_fields"]))

    drop_keys = ["UP", "ENTER", "Z", "ENTER", "ENTER", "ENTER",
        "DOWN", "ENTER", "DOWN", "DOWN", "ESC", "DOWN", "ENTER", "DOWN", "DOWN", "ENTER",
        "ENTER", "ESC", "ESC", "ESC", "ESC", "LEFT", "ENTER", "ENTER", "LEFT", "ENTER", "ENTER"]

    def protocol(self, directory, *, stop=None, bad_measurement=False, late_mtime=False,
                 scenario="drop-herb", case=None, role="original"):
        case = case or (self.source, self.focus, self.assets, self.actor, self.selection, self.templates, self.fields)
        source, focus, assets, actor, selection, templates, fields = case
        data = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.source, source))
        save = directory / "FD2.SAV"; save.write_bytes(data)
        measured = bytearray(assets["drop"]["expected_source"])
        measured[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3))
        measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
        encoded = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.source, measured))
        if bad_measurement:
            measured = bytearray(measured); measured[0x8a3] ^= 1
            measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
            encoded = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.source, measured))
        image = Image.new("RGB", (320, 200))
        capture = Mock(artifacts=directory, last=image)
        self.protocol_capture = capture
        armed = [False]
        self.protocol_menu_checks = []
        def key(name):
            if armed[0] and name == "ENTER":
                save.write_bytes(encoded); armed[0] = False
        capture.key.side_effect = key
        def wait(capture, name, predicate, checkpoints, **kwargs):
            if name == stop:
                raise TimeoutError("resource gate")
            if name == "drop-save-default-yes": armed[0] = True
            if name == "drop-saved-browse" and late_mtime:
                stamp = save.stat(); os.utime(save, ns=(stamp.st_atime_ns, stamp.st_mtime_ns + 2_000_000_000))
            if role == "sdl" and (name in ("item-submenu-default-use", "drop-cancel-selected", "drop-commit-selected") or
                    name.endswith("-back-item-zero") or name == "drop-readonly-list-cancel-item"):
                selected = 3 if name in ("drop-cancel-selected", "drop-commit-selected") else 0
                detail = predicate(self.menu_frame(selected, role="sdl"))
                self.assertIsNotNone(detail)
                self.protocol_menu_checks.append((name, detail["disabled"], detail["selected"]))
            return image, {"focus_screen": [52, 76]}
        report = {"checkpoints": [], "kind": role}
        with patch.object(probe, "wait_title"), patch.object(probe, "save_checkpoint"), \
                patch.object(probe, "wait_state", side_effect=wait):
            probe.exercise(capture, source, focus, assets, actor, selection,
                templates, fields, {3: [image] * 3}, save, data,
                save.stat().st_mtime_ns, report, scenario=scenario)
        return capture, report, save

    def test_one_drop_after_cancel_and_only_explicit_ui_save_measures_raw(self):
        with tempfile.TemporaryDirectory() as directory:
            capture, report, save = self.protocol(Path(directory))
            self.assertEqual([call.args[0] for call in capture.key.call_args_list], self.drop_keys)
            self.assertEqual(report["input_scope"]["drop_confirmation_count"], 1)
            self.assertEqual(report["input_scope"]["valid_use_confirmations"], 0)
            self.assertTrue(report["drop_measurement"]["runtime_raw_inventory_measured"])
            self.assertEqual(report["drop_measurement"]["measured_derived_values"], [16, 12, 97, 2])
            self.assertEqual(report["drop_measurement"]["raw_flags"], 0)
            self.assertEqual(save.read_bytes(), Path(report["completed_save"]["snapshot"]).read_bytes())

    def test_failed_resource_or_unknown_measured_bytes_never_retry_and_keep_artifact(self):
        for fault in ("resource", "unknown", "mtime"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(TimeoutError if fault == "resource" else AssertionError):
                    self.protocol(Path(directory), stop="drop-cancel-list-zero" if fault == "resource" else None,
                                  bad_measurement=fault == "unknown", late_mtime=fault == "mtime")
                if fault == "resource":
                    self.assertEqual([call.args[0] for call in self.protocol_capture.key.call_args_list], self.drop_keys[:8])
                    self.assertEqual((Path(directory) / "FD2.SAV").read_bytes(), self.encrypted)
                    self.assertFalse((Path(directory) / "drop-measured.SAV").exists())
                else:
                    self.assertEqual([call.args[0] for call in self.protocol_capture.key.call_args_list], self.drop_keys)
                    self.assertTrue((Path(directory) / "drop-measured.SAV").is_file())

    def test_sdl_drop_bad_executable_rejected_before_mkdir_read_or_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(Path, "mkdir") as mkdir, patch.object(probe, "hashes") as hashes, \
                    patch.object(Path, "read_bytes") as reads, patch("subprocess.Popen") as popen, \
                    self.assertRaisesRegex(ValueError, "--sdl"):
                probe.run(SimpleNamespace(root=ROOT, artifacts=Path(directory) / "not-created",
                    scenario="drop-herb", sdl=Path("missing.exe")))
            mkdir.assert_not_called(); hashes.assert_not_called(); reads.assert_not_called(); popen.assert_not_called()

    def test_original_only_run_uses_completed_save_guard_and_revokes_late_tampering(self):
        for fault in (None, "save_mtime", "save_bytes", "snapshot"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "new"
                args = SimpleNamespace(root=ROOT, artifacts=output, scenario="drop-herb",
                    driver=Path(__file__), dosbox=Path(__file__))
                events = []
                @contextmanager
                def running(command, cwd, env, log, driver, receipts):
                    events.append("started")
                    receipt = {"pid":123,"cleaned":False,"forced_cleanup":False}
                    receipts.append(receipt)
                    try:
                        yield Mock(pid=123)
                    finally:
                        receipt.update(cleaned=True,exit_code=0)
                        events.append("closed")
                        save = output / "game/FD2.SAV"
                        if fault == "save_mtime":
                            stamp = save.stat(); os.utime(save,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+2_000_000_000))
                        elif fault == "save_bytes": save.write_bytes(b"late change")
                        elif fault == "snapshot": (output / "drop-measured.SAV").write_bytes(b"late snapshot change")
                def exercise(*args, **kwargs):
                    self.assertEqual(kwargs["scenario"],"drop-herb")
                    save, record = args[-4], args[-1]
                    measured, encoded = self.measured()
                    snapshot = output / "drop-measured.SAV"
                    save.write_bytes(encoded); snapshot.write_bytes(encoded)
                    record["drop_measurement"] = {**probe.check_load_measurement(
                        self.plan["expected_source"],measured,self.plan["steps"]),"runtime_raw_inventory_measured":True}
                    record["completed_save"] = {"snapshot":str(snapshot),"sha256":hashlib.sha256(encoded).hexdigest(),
                                                "mtime_ns":save.stat().st_mtime_ns}
                prepared = (self.source,self.focus,self.assets,self.actor,self.selection,self.templates,self.fields)
                with patch.object(probe,"prepare",return_value=prepared),patch.object(probe,"hashes",return_value={"source":"stable"}), \
                        patch.object(probe.shutil,"copytree",side_effect=lambda source,dest:dest.mkdir()), \
                        patch.object(probe,"running",side_effect=running),patch.object(probe,"Capture"), \
                        patch.object(probe,"title_templates",return_value={}),patch.object(probe,"exercise",side_effect=exercise), \
                        patch("subprocess.Popen",side_effect=AssertionError("pure test must not launch")):
                    report = probe.run(args)
                self.assertEqual(events,["started","closed"])
                self.assertTrue(report["probe_only"] and not report["ok"])
                self.assertEqual(report["probe_cases_passed"],fault is None)
                self.assertEqual(report["copy_save_matches_completed_state_after_cleanup"],fault is None)
                self.assertNotIn("copy_save_unchanged_after_cleanup",report)

    def test_weapon_armor_and_last_models_preserve_every_unrelated_byte(self):
        expected_values = {
            "drop-weapon": ("408400c0800080008000800080c980c9", [6, 12, 2, 2]),
            "drop-armor": ("400000c0800080008000800080c980c9", [16, 4, 97, 2]),
            "drop-last": ("808480c0800080008000800080c980c9", [6, 4, 2, 2])}
        for scenario, (inventory, values) in expected_values.items():
            source, _, assets, _, _, _, fields = self.boundary_case(scenario)
            expected = assets["drop"]["expected_source"]
            contract = fields["drop_contract"]
            with self.subTest(scenario=scenario):
                self.assertEqual(expected[probe.UNITS + 10:probe.UNITS + 26].hex(), inventory)
                self.assertEqual(contract["derived_values"], values)
                self.assertEqual(list(struct.unpack_from("<4H", expected, probe.UNITS + 0x48)), values)
                allowed = {probe.UNITS + i for i in range(10, 26)} | {probe.UNITS + i for i in range(0x48, 0x50)}
                self.assertTrue(all(i in allowed for i, (a, b) in enumerate(zip(source[:-4], expected[:-4])) if a != b))
                self.assertEqual(expected[probe.UNITS + 0x19], 0xc9)
                self.assertEqual(expected[0x8a3:probe.UNITS], self.source[0x8a3:probe.UNITS])
                self.assertEqual(struct.unpack_from("<4H", expected, probe.UNITS + 0x40), (42, 42, 0, 0))
                self.assertEqual(expected[probe.UNITS + 5], 0)
                self.assertEqual(expected[probe.UNITS + 0x3c], 0)
                measured = bytearray(expected)
                measured[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3))
                measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
                self.assertTrue(probe.check_load_measurement(expected, measured, assets["drop"]["steps"]))
                for offset in (probe.META + 3, probe.UNITS + 0x19, 0x8a3, probe.UNITS + 0x40, probe.UNITS + 5):
                    bad = bytearray(measured); bad[offset] ^= 1
                    bad[-4:] = sum(bad[:-4]).to_bytes(4, "little")
                    with self.assertRaises(AssertionError):
                        probe.check_load_measurement(expected, bad, assets["drop"]["steps"])

    def test_last_fixture_exact_manifest_and_source_whitelist_remains_strict(self):
        table, _ = oracle.item_table(ROOT / "original_game/FD2.EXE")
        encoded, source, manifest = probe.derive_drop_last(self.source, self.encrypted, table)
        self.assertEqual(hashlib.sha256(source).hexdigest(), probe.DROP_LAST_PLAIN_SHA256)
        self.assertEqual(source[probe.UNITS + 10:probe.UNITS + 26].hex(), "00c0808480c0800080008000800080c9")
        self.assertEqual(manifest["derived_values"], [6, 4, 2, 2])
        self.assertFalse(manifest["cancel_recompute_dynamic_write_observed"])
        offsets = {p["offset"] for p in manifest["plaintext_patches"]}
        self.assertEqual(offsets, {probe.UNITS + i for i in (10, 11, 12, 14, 0x48, 0x4a, 0x4c)} | {len(source) - 4, len(source) - 3})
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "last.SAV"; save.write_bytes(encoded)
            self.assertEqual(probe.decode_save(save), source)
        # 共享构图不会把新派生SAV加入原prepare的来源白名单。
        with self.assertRaises(ValueError):
            oracle.prepare(ROOT / "original_game", ROOT / "original_game/START.SAV", source=source)
        for before, encrypted in ((source, self.encrypted), (self.source, encoded)):
            with self.assertRaises(ValueError): probe.derive_drop_last(before, encrypted, table)
        bad = bytearray(source); bad[0x8a3] ^= 1
        with self.assertRaises(ValueError): probe.drop_expected_source(bad, table, "drop-last")

    def test_reordered_resources_and_only_herb_menu_have_exact_native_gates(self):
        for scenario in ("drop-weapon", "drop-armor", "drop-last"):
            source, _, assets, actor, _, templates, fields = self.boundary_case(scenario)
            plan = assets["drop"]
            with self.subTest(scenario=scenario):
                for t, f in ((templates, fields), (plan["after_templates"], plan["after_fields"])):
                    for selected in range(len(f["slots"])):
                        frame = Image.new("RGB", (320, 200))
                        for name in (f"selected_{selected}", "actor_name", "current_max_hp", "current_max_mp", "level", "experience",
                                     "attack", "defense", "accuracy", "evasion"):
                            at, part = t[name]; frame.paste(part.convert("RGB"), at)
                        detail = probe.drop_list_page(frame, t, f, selected)
                        self.assertEqual(detail["checked_pixels"], 32723)
                        self.assertEqual(detail["visible_derived_values"], f["derived_values"])
                        for wrong in range(len(f["slots"])):
                            if selected != wrong: self.assertIsNone(probe.list_page(frame, t, f, wrong))
                        broken = frame.copy(); broken.putpixel((110, 107), (255, 0, 255))
                        self.assertIsNone(probe.list_page(broken, t, f, selected))
                        for name in ("attack", "defense", "accuracy", "evasion"):
                            bad = frame.copy()
                            at, _ = t[name]; bad.putpixel(at, (255, 0, 255))
                            self.assertIsNone(probe.list_page(bad, t, f, selected))
                # only-herb仍有EQT普通图30和Drop普通图33；不是[0,1,1,1]。
                menu = self.menu_frame()
                self.assertEqual(probe.submenu_page(menu, assets, (52, 76), templates, fields)["disabled"], [0, 1, 0, 0])
                self.assertIsNotNone(probe.submenu_page(self.menu_frame(role="sdl"), assets, (52, 76), templates, fields))
                if scenario == "drop-last":
                    self.assertEqual(plan["after_fields"]["slots"], [])
                    command = Image.new("RGB", (320, 200))
                    for i, index in enumerate((2, 5, 8, 10)):
                        part = assets["upper_commands"][index][0] if i == 0 else assets["commands"][index]
                        at = tuple(a + b for a, b in zip((52, 76), probe.MENU_OFFSETS[i]))
                        command.paste(part.convert("RGB"), at)
                    detail = probe.command_page(command, probe.PlayerOracle(source, assets, [], {}), (52, 76), plan["after_templates"], plan["after_fields"])
                    self.assertTrue(detail["native_first_enabled_wait_after_empty_drop"])
                    self.assertIsNone(probe.command_page(self.command_frame(), probe.PlayerOracle(source, assets, [], {}), (52, 76), plan["after_templates"], plan["after_fields"]))

    def test_new_drop_protocols_one_cancel_one_commit_no_empty_use_page(self):
        for scenario, slot, count in (("drop-weapon", 0, 23), ("drop-armor", 1, 25), ("drop-last", 0, 20)):
            case = self.boundary_case(scenario)
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                capture, report, save = self.protocol(Path(directory), scenario=scenario, case=case)
                navigate = ["DOWN"] * slot
                expected = ["UP", "ENTER", "Z", "ENTER", "ENTER", "ENTER", "DOWN", "ENTER", *navigate,
                            "ESC", "DOWN", "ENTER", *navigate, "ENTER"]
                if scenario != "drop-last": expected += ["ENTER", "ESC", "ESC"]
                expected += ["ESC", "ESC", "LEFT", "ENTER", "ENTER", "LEFT", "ENTER", "ENTER"]
                self.assertEqual([call.args[0] for call in capture.key.call_args_list], expected)
                self.assertEqual(len(expected), count)
                self.assertEqual("drop_empty_command" in report, scenario == "drop-last")
                self.assertEqual("drop_two_slot_page" in report, scenario != "drop-last")
                self.assertEqual(report["input_scope"]["drop_confirmation_count"], 1)
                self.assertEqual(report["input_scope"]["valid_use_confirmations"], 0)
                self.assertEqual(report["drop_measurement"]["measured_derived_values"], case[-1]["drop_contract"]["derived_values"])
                self.assertEqual(save.read_bytes(), (Path(directory) / "drop-measured.SAV").read_bytes())
        for scenario in ("drop-weapon", "drop-armor", "drop-last"):
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(AssertionError):
                    self.protocol(Path(directory), scenario=scenario, case=self.boundary_case(scenario), bad_measurement=True)
                self.assertTrue((Path(directory) / "drop-measured.SAV").is_file())

    def test_sdl_drop_protocol_uses_sdl_resources_at_every_menu_return(self):
        for scenario in probe.DROP_SCENARIOS:
            case = None if scenario == "drop-herb" else self.boundary_case(scenario)
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                self.protocol(Path(directory), scenario=scenario, case=case, role="sdl")
                self.assertEqual(len(self.protocol_menu_checks), 4 if scenario == "drop-last" else 6)
                self.assertTrue(all(disabled == [0, 1, 0, 0] for _, disabled, _ in self.protocol_menu_checks))
                self.assertEqual([selected for _, _, selected in self.protocol_menu_checks].count(3), 2)

    def test_all_new_drop_bad_sdl_paths_are_rejected_before_any_io(self):
        for scenario in ("drop-weapon", "drop-armor", "drop-last"):
            with self.subTest(scenario=scenario), patch.object(Path, "mkdir") as mkdir, \
                    patch.object(Path, "read_bytes") as reads, patch.object(probe, "hashes") as hashes, \
                    patch("subprocess.Popen") as popen, self.assertRaisesRegex(ValueError, "--sdl"):
                probe.run(SimpleNamespace(root=ROOT, artifacts=Path("unused"), scenario=scenario, sdl=Path("unused.exe")))
            mkdir.assert_not_called(); reads.assert_not_called(); hashes.assert_not_called(); popen.assert_not_called()

    def test_new_scenarios_keep_probe_only_and_guard_the_actual_completed_save(self):
        for scenario, tamper in (("drop-weapon", False), ("drop-armor", False),
                                 ("drop-last", False), ("drop-last", True)):
            with self.subTest(scenario=scenario, tamper=tamper), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "new"
                args = SimpleNamespace(root=ROOT, artifacts=output, scenario=scenario,
                    driver=Path(__file__), dosbox=Path(__file__))
                @contextmanager
                def running(command, cwd, env, log, driver, receipts):
                    receipt = {"pid": 123, "cleaned": False, "forced_cleanup": False}
                    receipts.append(receipt)
                    try:
                        yield Mock(pid=123)
                    finally:
                        receipt.update(cleaned=True, exit_code=0)
                        if tamper:
                            save = output / "game/FD2.SAV"; info = save.stat()
                            os.utime(save, ns=(info.st_atime_ns, info.st_mtime_ns + 2_000_000_000))
                def exercise(*args, **kwargs):
                    self.assertEqual(kwargs["scenario"], scenario)
                    source, assets, save, data, record = args[1], args[3], args[-4], args[-3], args[-1]
                    self.assertEqual(save.read_bytes(), data)
                    self.assertEqual(probe.decode_save(save), source)
                    self.assertEqual(hashlib.sha256(data).hexdigest(), record["runtime_fixture_sha256"])
                    if scenario == "drop-last":
                        self.assertEqual(hashlib.sha256(source).hexdigest(), probe.DROP_LAST_PLAIN_SHA256)
                    else:
                        self.assertEqual(data, self.encrypted)
                    measured = bytearray(assets["drop"]["expected_source"])
                    measured[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3))
                    measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
                    encoded = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.source, measured))
                    snapshot = output / "drop-measured.SAV"; snapshot.write_bytes(encoded); save.write_bytes(encoded)
                    record["drop_measurement"] = {"runtime_raw_inventory_measured": True}
                    record["completed_save"] = {"snapshot": str(snapshot), "sha256": hashlib.sha256(encoded).hexdigest(),
                                                "mtime_ns": save.stat().st_mtime_ns}
                with patch.object(probe, "hashes", return_value={"source": "stable"}), \
                        patch.object(probe.shutil, "copytree", side_effect=lambda source, dest: dest.mkdir()), \
                        patch.object(probe, "running", side_effect=running), patch.object(probe, "Capture"), \
                        patch.object(probe, "title_templates", return_value={}), patch.object(probe, "exercise", side_effect=exercise), \
                        patch("subprocess.Popen", side_effect=AssertionError("pure test must not launch")):
                    report = probe.run(args)
                self.assertTrue(report["probe_only"] and not report["ok"])
                self.assertEqual(report["probe_cases_passed"], not tamper)
                self.assertEqual(report["copy_save_matches_completed_state_after_cleanup"], not tamper)
                self.assertEqual("derived_fixture" in report, scenario == "drop-last")
                self.assertNotIn("copy_save_unchanged_after_cleanup", report)
                self.assertTrue(report["fixture_unchanged"])

    def test_drop_full_rectangles_are_cross_compared_including_all_four_new_numbers(self):
        for scenario in ("drop-weapon", "drop-armor", "drop-last"):
            case = self.boundary_case(scenario)
            source, _, assets, _, selection, templates, fields = case
            if scenario != "drop-last":
                templates, fields = assets["drop"]["after_templates"], assets["drop"]["after_fields"]
            image = Image.new("RGB", (320, 200))
            for name in ("selected_0", "actor_name", "current_max_hp", "current_max_mp", "level", "experience",
                         "attack", "defense", "accuracy", "evasion"):
                at, part = templates[name]; image.paste(part.convert("RGB"), at)
            detail = probe.drop_list_page(image, templates, fields)
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                cases = PairRGBTest.cases(folder, image, image, detail, detail)
                compared = probe.compare_paired_checkpoints(cases, folder, assets, selection)
                self.assertTrue(compared["ok"])
                self.assertEqual(compared["checked_pixels"], 32723)
                self.assertEqual([r["name"] for r in compared["checkpoints"][0]["static_regions"]][-4:],
                                 ["attack", "defense", "accuracy", "evasion"])
            point = next((x, y) for y in range(67, 75) for x in range(157, 175)
                         if min(image.getpixel((x, y))) >= 2 and max(image.getpixel((x, y))) <= 253)
            value = image.getpixel(point); first, second = image.copy(), image.copy()
            first.putpixel(point, tuple(v - 2 for v in value)); second.putpixel(point, tuple(v + 2 for v in value))
            a, b = (probe.drop_list_page(frame, templates, fields) for frame in (first, second))
            self.assertIsNotNone(a); self.assertIsNotNone(b)
            with tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                compared = probe.compare_paired_checkpoints(PairRGBTest.cases(folder, first, second, a, b), folder, assets, selection)
                self.assertFalse(compared["ok"])
                self.assertEqual(sum(r["bad_pixels"] for row in compared["checkpoints"] for r in row["static_regions"]), 1)


class GiveTest(Fixture, unittest.TestCase):
    def test_runtime_resources_and_exact_cancel_or_sticky_protocol(self):
        original = ROOT / "original_game"
        for scenario in probe.GIVE_SCENARIOS:
            _, source, _ = probe.derive_give(self.source, (original / "START.SAV").read_bytes(), scenario)
            _, _, assets, actor, selection, templates, fields = probe.prepare_controlled(original, source)
            probe.prepare_give(original, source, assets, actor, selection, fields, scenario)
            plan = assets["give"]
            capture, report, labels = Mock(), {"kind": "original", "scenario": scenario}, []
            def checkpoint(name, predicate, **kwargs):
                labels.append(name)
                return None, {}
            with patch.object(probe, "measure_inventory_save") as measure, patch.object(probe, "execute_end") as end:
                probe.exercise_give(capture, source, assets, actor, selection, templates, fields,
                    None, b"", 0, report, checkpoint)
            measure.assert_called_once()
            self.assertEqual(end.call_count, int(scenario != "give-cancel"))
            self.assertEqual(labels.count("give-command-cancel-selection"), int(scenario == "give-cancel"))
            self.assertEqual(labels.count("give-recipient-immediate-detail"), int(scenario != "give-cancel"))
            self.assertEqual(plan["steps"], [("DOWN", (52, 124))] if scenario == "give-moved" else [("LEFT", (28, 76))])
            self.assertEqual(fields["submenu"]["disabled"], [0, 0, 0, 0])
            if scenario != "give-cancel":
                self.assertEqual(report["input_scope"]["give_success_count"], 1)
            capture = Mock()
            with self.assertRaises(TimeoutError):
                probe.exercise_give(capture, source, assets, actor, selection, templates, fields,
                    None, b"", 0, report, lambda *a, **k: (_ for _ in ()).throw(TimeoutError("gate")))
            self.assertEqual([call.args[0] for call in capture.key.call_args_list], ["LEFT"])

    def test_shared_cursor_distinguishes_acted_without_changing_unacted(self):
        from run_original_player import cursor_assets, acted_sprite
        source = bytearray(self.source)
        source[probe.META + 1] = 1
        normal, composites = cursor_assets(ROOT / "original_game/FDICON.B24", source, self.assets)
        source[probe.UNITS + 5] |= 0x80
        gray, gray_composites = cursor_assets(ROOT / "original_game/FDICON.B24", source, self.assets)
        self.assertEqual(len(normal), 12)
        self.assertEqual(len(gray), 12)
        self.assertTrue(any(a.tobytes() != b.tobytes() for a, b in zip(normal, gray)))
        for a, b in zip(normal, gray):
            self.assertEqual(acted_sprite(a).tobytes(), b.tobytes())
        again, again_composites = cursor_assets(ROOT / "original_game/FDICON.B24", self.source[:probe.META + 1] + bytes((1,)) + self.source[probe.META + 2:], self.assets)
        self.assertEqual([x.tobytes() for x in normal], [x.tobytes() for x in again])
        self.assertNotEqual(composites[0][0].tobytes(), gray_composites[0][0].tobytes())

    def test_exact_fixtures_keep_roster_and_nonplayer_ai_visible(self):
        base = self.source
        encrypted = (ROOT / "original_game/START.SAV").read_bytes()
        for scenario in probe.GIVE_SCENARIOS:
            encoded, source, manifest = probe.derive_give(base, encrypted, scenario)
            self.assertTrue(manifest["controlled_fixture"])
            self.assertEqual(source[0x8a3:probe.UNITS], base[0x8a3:probe.UNITS])
            self.assertEqual(source[probe.META:probe.META + 9], base[probe.META:probe.META + 9])
            self.assertEqual(sum(source[:-4]), int.from_bytes(source[-4:], "little"))
            for i in range(source[probe.META + 1]):
                at = probe.UNITS + i * probe.STRIDE
                if base[at + 6] != 2:
                    self.assertEqual(source[at + 5], base[at + 5] | 4)
                    self.assertEqual(source[at + 5] & 1, base[at + 5] & 1)
            self.assertEqual(source[probe.UNITS + 9 * probe.STRIDE + 5], 5)
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "derived.SAV"; path.write_bytes(encoded)
                self.assertEqual(probe.decode_save(path), source)
            broken = bytearray(source); broken[0x8a3] ^= 1
            with self.assertRaises(ValueError): probe.give_expected_source(ROOT / "original_game", broken, scenario)

    def test_give_fifo_flags_sender_only_stats_and_sticky_after_end(self):
        encrypted = (ROOT / "original_game/START.SAV").read_bytes()
        for scenario in probe.GIVE_SCENARIOS:
            _, source, _ = probe.derive_give(self.source, encrypted, scenario)
            expected, fields = probe.give_expected_source(ROOT / "original_game", source, scenario)
            sender = expected[probe.UNITS:probe.UNITS + probe.STRIDE]
            recipient = expected[probe.UNITS + probe.STRIDE:probe.UNITS + 2 * probe.STRIDE]
            if scenario == "give-cancel":
                self.assertEqual(expected, source)
            else:
                self.assertEqual(expected[probe.META], 2)
                self.assertEqual(sender[10:26].hex(), "408400c0003480008000800080c980c9" if scenario == "give-swap" else "408400c0800080008000800080c980c9")
                self.assertEqual(recipient[10:26].hex(), "40a400c000c000c000c000c000c00000" if scenario == "give-swap" else "403440a4000080cc80cc80cc80cc80c1")
                self.assertEqual(struct.unpack_from("<4H", sender, 0x48), (6, 12, 2, 2))
            self.assertEqual(struct.unpack_from("<4H", recipient, 0x48), (11, 7, 86, 1))
            self.assertEqual((sender[0x3c], recipient[0x3c]), (99, 99))
            self.assertEqual(sender[:2], bytes((7, 15) if scenario == "give-moved" else (7, 14)))
            self.assertFalse(fields["recipient_recomputed"])
            self.assertEqual(fields["success_sticky_53c53"], scenario != "give-cancel")
            self.assertEqual(expected[0x8a3:probe.UNITS], source[0x8a3:probe.UNITS])
            self.assertEqual(expected[0x312b:-4], source[0x312b:-4])


class UseMatrixTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = ROOT / "original_game"
        cls.base = probe.decode_save(cls.original / "START.SAV")
        cls.encrypted = (cls.original / "START.SAV").read_bytes()
        cls.table, _ = oracle.item_table(cls.original / "FD2.EXE")
        cls.cases = {}
        for scenario in probe.USE_SCENARIOS:
            encoded, source, manifest = probe.derive_use(cls.base, cls.encrypted, scenario, cls.table)
            prepared = probe.prepare_controlled(cls.original, source)
            _, _, assets, actor, _, _, fields = prepared
            probe.prepare_use(cls.original, source, assets, actor, fields, scenario)
            cls.cases[scenario] = (encoded, source, manifest, prepared)

    def test_all_twenty_exact_manifests_native_tail_and_no_unknown_writes(self):
        self.assertEqual(list(self.cases), [f"use-{code:02d}" for code in range(5, 25)] + ["use-area2", "use-23-mp0"])
        for scenario, (encoded, source, manifest, prepared) in self.cases.items():
            plan = prepared[2]["use"]; code = manifest["use_code"]
            self.assertEqual(source[0x8a3:probe.UNITS], self.base[0x8a3:probe.UNITS])
            self.assertEqual(source[probe.UNITS + 9 * probe.STRIDE + 5], 5)
            self.assertEqual(manifest["item_id"], 102 if scenario == "use-area2" else probe.USE_ITEMS[code])
            self.assertEqual(source[probe.UNITS + 15], manifest["item_id"])
            self.assertEqual(bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.base, source)), encoded)
            self.assertEqual(sum(source[:-4]), int.from_bytes(source[-4:], "little"))
            for variant in plan["variants"]:
                immediate, final = variant["source"], variant["final"]
                self.assertEqual(immediate[probe.UNITS + 5] & 0x80, 0x80)
                self.assertEqual(immediate[probe.UNITS + 0x3c], 99)
                self.assertEqual(final[probe.META], 2)
                self.assertEqual(final[0x8a3:probe.UNITS], source[0x8a3:probe.UNITS])
                self.assertEqual(final[probe.UNITS + 12 * probe.STRIDE:probe.META], source[probe.UNITS + 12 * probe.STRIDE:probe.META])
                self.assertEqual(final[probe.META + 1:probe.META + 18], source[probe.META + 1:probe.META + 18])
                self.assertEqual(final[probe.META + 18:-4], source[probe.META + 18:-4])
                self.assertEqual(sum(final[:-4]), int.from_bytes(final[-4:], "little"))
            damaged = bytearray(source); damaged[0x312b] ^= 1
            with self.assertRaises(ValueError): probe.use_outcomes(self.original, damaged, scenario)

    def test_cure_restores_nine_before_clear_and_end_recipient_idle_heal(self):
        for code, raw in ((6, 0), (7, 1)):
            plan = self.cases[f"use-{code:02d}"][3][2]["use"]
            immediate, final = plan["variants"][0]["source"], plan["variants"][0]["final"]
            at = probe.UNITS + raw * probe.STRIDE
            self.assertEqual(struct.unpack_from("<H", immediate, at + 0x40)[0], 29)
            self.assertEqual(immediate[at + (0x25 if code == 6 else 0x26)], 0)
            maximum = struct.unpack_from("<H", immediate, at + 0x42)[0]
            self.assertEqual(struct.unpack_from("<H", final, at + 0x40)[0], 29 if raw == 0 else min(maximum, 29 + maximum // 5))
            self.assertEqual(immediate[at + 0x44:at + 0x48], self.base[at + 0x44:at + 0x48])

    def test_true_area2_two_targets_and_mp_underflow_keep_raw_owner_and_visible_metadata(self):
        _, source, manifest, prepared = self.cases["use-area2"]
        self.assertEqual((manifest["item_id"], manifest["use_code"], manifest["player_cast"], manifest["player_area"]), (102, 5, 4, 2))
        self.assertEqual(manifest["final_target_raws"], [0, 1])
        plan = prepared[2]["use"]
        self.assertEqual([item["frame"] for item in plan["area2_markers"]], list(range(2, 14)))
        self.assertEqual(len(plan["variants"]), 1)
        for data in (plan["variants"][0]["source"], plan["variants"][0]["final"]):
            self.assertEqual(struct.unpack_from("<HH", data, probe.UNITS + 0x40), (42, 42))
            self.assertEqual(struct.unpack_from("<HH", data, probe.UNITS + probe.STRIDE + 0x40), (28, 28))
            self.assertEqual(data[probe.UNITS + 0x3c], 99)
        self.assertEqual([item["raw_actor"] for item in plan["extra_details"]], [1])
        self.assertEqual(plan["extra_details"][0]["fields"]["hp"], 28)
        for scenario, raw_mp, length in (("use-23", 20, 51), ("use-23-mp0", 65516, -49)):
            plan = self.cases[scenario][3][2]["use"]
            self.assertEqual(struct.unpack_from("<HH", plan["variants"][0]["final"], probe.UNITS + 0x44), (raw_mp, 40))
            self.assertEqual([item["raw_actor"] for item in plan["extra_details"]], [0])
            extra = plan["extra_details"][0]
            self.assertTrue(extra["fields"]["require_word_bars"])
            self.assertEqual(extra["fields"]["mp_bar"]["native_length"], length)
            capture = Mock(); labels = []; report = {}
            def checkpoint(name, predicate, **kwargs):
                labels.append(name); return None, {}
            probe.exercise_use_additional_details(capture, self.cases[scenario][1], self.cases[scenario][3][2], report, checkpoint)
            self.assertIn("use-extra-0-f2-detail", labels)
            self.assertIn("use-extra-0-detail-closed", labels)
            self.assertEqual([call.args[0] for call in capture.key.call_args_list], ["LEFT", "LEFT", "F2", "ESC"])

    def test_random_native_records_are_finite_not_rerolled_or_whole_save_allowlisted(self):
        counts = {12: 4, 14: 5, 15: 4, 16: 4, 20: 26, 21: 13, 22: 5, 24: 51}
        for code, expected in counts.items():
            plan = self.cases[f"use-{code:02d}"][3][2]["use"]
            self.assertEqual(len(plan["variants"]), expected)
            self.assertFalse(plan["contract"]["rng_rerolled"])
            raw = plan["contract"]["target_raw"]; at = probe.UNITS + raw * probe.STRIDE
            if code in (14, 22):
                self.assertEqual({struct.unpack_from("<H", variant["source"], at + 0x40)[0] for variant in plan["variants"]}, {11, 20})
            if code in (12, 15, 16):
                status = {12: 0x24, 15: 0x23, 16: 0x22}[code]
                self.assertEqual({variant["source"][at + status] for variant in plan["variants"]}, {2, 3, 4, 5})
                self.assertEqual({variant["final"][at + status] for variant in plan["variants"]}, {1, 2, 3, 4})

    def test_all_twenty_protocols_have_one_use_confirmation_and_never_skip(self):
        for scenario, (_, source, _, prepared) in self.cases.items():
            _, _, assets, actor, selection, templates, fields = prepared
            capture = Mock(); report = {"scenario": scenario, "kind": "original", "original_path": str(self.original), "checkpoints": []}
            labels = []
            def checkpoint(name, predicate, **kwargs):
                labels.append(name)
                if name == "use-immediate-f2-detail":
                    return None, {"finite_native_outcome_indices": list(range(len(assets["use"]["variants"])))}
                return None, {}
            with patch.object(probe, "execute_end") as end, patch.object(probe, "measure_inventory_save") as save, \
                    patch.object(probe, "exercise_use_additional_details") as extra:
                probe.exercise_use(capture, source, assets, actor, selection, templates, fields, None, b"", 0, report, checkpoint)
            self.assertEqual(labels.count("use-cancel-target"), 1)
            self.assertEqual(labels.count("use-commit-target"), 1)
            self.assertIn("use-target-cancel-list-zero", labels)
            self.assertEqual(report["input_scope"]["valid_use_confirmations"], 1)
            self.assertEqual(report["input_scope"]["post_use_skip_keys"], [])
            end.assert_called_once(); save.assert_called_once()
            extra.assert_called_once()
            keys = [call.args[0] for call in capture.key.call_args_list]
            self.assertNotIn("SPACE", keys)
            if scenario in ("use-23", "use-23-mp0"):
                self.assertIn("use23-destination-right", labels)
                self.assertIn("use23-destination-cancel-list-zero", labels)
                self.assertIn("use23-destination-cancel-item-zero", labels)
                self.assertEqual(report["input_scope"]["destination_cancel_count"], 1)
                self.assertEqual(keys.count("ENTER"), 9)  # 三次list/target＋两次首目标肯定/一次目的地肯定。

    def test_all_target_modes_full_resources_and_negative_lut_cursor_detail(self):
        for scenario, (_, source, _, prepared) in self.cases.items():
            _, _, assets, actor, selection, templates, fields = prepared
            plan = assets["use"]
            frame = Image.new("RGB", (320, 200))
            at = probe.screen(source, plan["center"])
            if plan.get("area2_markers"):
                sheet = probe.archive_entry((self.original / "FDOTHER.DAT").read_bytes(), 1)
                for item in plan["area2_markers"]:
                    # 先全部选择图，再按raw顺序绘actor；完整保留自然遮挡。
                    image = probe.sheet_frame(sheet, item["frame"]); image.putpalette(assets["palette"])
                    position = item["position"]
                    if item["actor_composited"] and item["images"][0].height == 30: position = (position[0], position[1] + 6)
                    frame.paste(image.convert("RGB"), position, image.point(lambda c: 255 if c else 0, mode="L"))
            part = plan["target"]["templates"][0]
            frame.paste(part.convert("RGB"), (at[0], at[1] - 6), part.point(lambda c: 255 if c else 0, mode="L"))
            marker = dict(plan["center_ranges"])[plan["range"]["ranges"][0][0]] if plan["cursor_frame"] is None else plan["cursor"].crop((0, 18, 24, 24))
            frame.paste(marker.convert("RGB"), (at[0], at[1] + 18))
            if plan["range"]:
                frame.paste(plan["range"]["ranges"][0][1].convert("RGB"), plan["range"]["position"])
            if plan.get("area2_markers"):
                sprites = (self.original / "FDICON.B24").read_bytes()
                for raw in range(source[probe.META + 1]):
                    record = source[probe.UNITS + raw * probe.STRIDE:probe.UNITS + (raw + 1) * probe.STRIDE]
                    if record[5] & 1: continue
                    image = probe.sheet_frame(sprites, record[7] * 12); image.putpalette(assets["palette"])
                    position = probe.screen(source, tuple(record[:2]))
                    frame.paste(image.convert("RGB"), (position[0], position[1] - 6), image.point(lambda c: 255 if c else 0, mode="L"))
            detail = probe.use_target(frame, source, assets, templates, fields)
            self.assertIsNotNone(detail, scenario)
            if plan.get("area2_markers"):
                self.assertEqual([row["frame"] for row in detail["selection_markers"]], list(range(2, 14)))
                for item in plan["area2_markers"]:
                    common = next(i for i, value in enumerate(item["static"].getdata()) if value)
                    bad = frame.copy()
                    bad.putpixel((item["position"][0] + common % item["static"].width,
                                  item["position"][1] + common // item["static"].width), (255, 0, 255))
                    self.assertIsNone(probe.use_target(bad, source, assets, templates, fields), item["frame"])
            self.assertEqual(detail["selection_frame"], None if scenario == "use-24" else 14 if plan["contract"]["player_area"] == 1 else 1)
            if plan["range"]:
                bad = frame.copy(); bad.paste(plan["range"]["base"].convert("RGB"), plan["range"]["position"])
                self.assertIsNone(probe.use_target(bad, source, assets, templates, fields), scenario)
            bad = frame.copy(); bad.paste(Image.new("RGB", (24, 6), (255, 0, 255)), (at[0], at[1] + 18))
            self.assertIsNone(probe.use_target(bad, source, assets, templates, fields), scenario)
            variant = plan["variants"][0]
            image = Image.new("RGB", (320, 200))
            t, f = variant["detail_templates"], variant["detail_fields"]
            for name in ("unselected", "actor_name", "current_max_hp", "current_max_mp", "level", "experience", "attack", "defense", "accuracy", "evasion", "dexterity", "movement", "status_25", "status_26", "status_27"):
                position, resource = t[name]; image.paste(resource.convert("RGB"), position)
            if f.get("require_word_bars"):
                for name in ("hp_bar", "mp_bar"):
                    position, resource = t[name]; image.paste(resource.convert("RGB"), position)
            detail = probe.use_detail(image, assets)
            self.assertIsNotNone(detail, scenario)
            self.assertIn(0, detail["finite_native_outcome_indices"])
            image.paste(Image.new("RGB", (44, 8), (255, 0, 255)), t["current_max_hp"][0])
            self.assertIsNone(probe.use_detail(image, assets), scenario)

    def test_post_use_navigation_checks_gray_occupant_and_lower_actor_without_mutating_initial_assets(self):
        for scenario, world in (("use-07", (7, 14)), ("use-14", (7, 13)), ("use-23", (7, 14)), ("use-24", (7, 14))):
            _, source, _, prepared = self.cases[scenario]
            assets = prepared[2]; plan = assets["use"]
            before_cursor = assets["cursor"].tobytes()
            resources = list(plan["end_navigation"])
            for extra in plan["extra_details"]: resources += extra["navigation"] + extra["return_navigation"]
            resource = next(resource for _, resource in resources if resource["world"] == world)
            image = Image.new("RGB", (320, 200))
            at = probe.screen(source, world)
            if "actor" in resource:
                part = resource["actor"]["templates"][0]; position = (at[0], at[1] - 6)
                normal = probe.actor_focus_resources(self.original, source, assets, resource["actor"]["raw_actor"], assets["cursor"])["templates"][0]
            else:
                self.assertEqual(resource["lower_raw_actor"], 0)
                part = resource["composites"][0]; position = at
                normal = probe.post_use_navigation_resources(self.original, source, assets, world)["composites"][0]
            image.paste(part.convert("RGB"), position)
            self.assertIsNotNone(probe.post_use_navigation_state(image, source, assets, resource), scenario)
            self.assertIsNone(probe.RoundOracle(assets, at).closed_browse(image), scenario)
            wrong = image.copy(); wrong.paste(normal.convert("RGB"), position)
            self.assertIsNone(probe.post_use_navigation_state(wrong, source, assets, resource), scenario)
            wrong = Image.new("RGB", (320, 200)); wrong.paste(assets["cursor"].convert("RGB"), at)
            self.assertIsNone(probe.post_use_navigation_state(wrong, source, assets, resource), scenario)
            self.assertEqual(assets["cursor"].tobytes(), before_cursor)

    def test_finite_full_saves_allow_only_native_random_positions_between_ends(self):
        from run_original_load import measuring_focus
        for scenario in ("use-06", "use-12", "use-14", "use-20", "use-24"):
            _, source, _, prepared = self.cases[scenario]
            assets = prepared[2]; plan = assets["use"]
            with tempfile.TemporaryDirectory() as directory:
                folder = Path(directory); cases = []
                for role, variant in (("original", plan["variants"][0]), ("sdl", plan["variants"][-1])):
                    measured = bytearray(variant["final"])
                    measured[probe.META + 5:probe.META + 9] = measuring_focus(measured, plan["steps"])
                    measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
                    encoded = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.base, measured))
                    path = folder / (role + ".SAV"); path.write_bytes(encoded); digest = hashlib.sha256(encoded).hexdigest()
                    cases.append({"kind": role, "completed_save": {"snapshot": str(path), "sha256": digest}, "use_measurement": {"snapshot_sha256": digest},
                        "use_immediate_detail": {"finite_native_outcome_indices": [0 if role == "original" else len(plan["variants"]) - 1]}})
                result = probe.compare_use_saves(cases, assets)
                self.assertTrue(result["deterministic_container_bytes_equal"])
                self.assertEqual(result["full_bytes_equal"], scenario == "use-06")
                if scenario != "use-06":
                    # 两个native结果各自合法，也不能把F2读A、Save写B当连续一致。
                    cases[-1]["use_immediate_detail"]["finite_native_outcome_indices"] = [0]
                    with self.assertRaises(AssertionError): probe.compare_use_saves(cases, assets)
                    cases[-1]["use_immediate_detail"]["finite_native_outcome_indices"] = [len(plan["variants"]) - 1]
                measured[0x312b] ^= 1  # 手工槽未知字节不可借随机HP/duration放宽。
                measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
                encoded = bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.base, measured)); path.write_bytes(encoded)
                digest = hashlib.sha256(encoded).hexdigest()
                cases[-1]["completed_save"]["sha256"] = digest; cases[-1]["use_measurement"]["snapshot_sha256"] = digest
                with self.assertRaises(AssertionError): probe.compare_use_saves(cases, assets)


class EquipmentTest(Fixture, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.encrypted = (ROOT / "original_game/START.SAV").read_bytes()
        cls.encoded, cls.changed, cls.manifest = probe.derive_second_weapon(cls.source, cls.encrypted)
        cls.cases = {}
        for scenario in probe.EQUIPMENT_SCENARIOS:
            source, focus, assets, actor, selection, templates, fields = probe.prepare(ROOT / "original_game")
            if scenario == "eqt-weapon":
                source = cls.changed
                _, templates, updated = oracle.inventory_templates(ROOT / "original_game", source[probe.UNITS:probe.UNITS + probe.STRIDE], assets)
                fields = dict(updated, submenu=fields["submenu"])
            probe.prepare_equipment(ROOT / "original_game", source, assets, actor, fields, scenario)
            cls.cases[scenario] = (source, focus, assets, actor, selection, templates, fields)

    @staticmethod
    def page(templates, selected):
        frame = Image.new("RGB", (320, 200))
        for name in (f"selected_{selected}", "actor_name", "current_max_hp", "current_max_mp", "level", "experience",
                     "attack", "defense", "accuracy", "evasion"):
            at, part = templates[name]; frame.paste(part.convert("RGB"), at)
        return frame

    def test_exact_fixture_and_permission_row_first_six(self):
        self.assertEqual(hashlib.sha256(self.encoded).hexdigest(), "ec01712ee1c8055eeba4071554af0284f3b89a8fbac24e56d28666ac58e99b4c")
        self.assertEqual([p["offset"] for p in self.manifest["plaintext_patches"]], [0x12b2, 0x59c7, 0x59c8])
        for scenario, case in self.cases.items():
            contract = case[-1]["equipment_contract"]
            self.assertEqual(contract["profile"], 1)
            self.assertEqual(contract["row_hex"], "011516ffffff01")
            self.assertEqual(contract["allowed_categories"], [1, 21, 22, 255, 255, 255])
            self.assertFalse(contract["seventh_byte_used"])
            self.assertEqual(contract["compatible"], scenario == "eqt-weapon")
        with self.assertRaises(ValueError):
            oracle.prepare(ROOT / "original_game", ROOT / "original_game/START.SAV", source=self.changed)
        for source, encoded in ((self.changed, self.encrypted), (self.source, self.encoded)):
            with self.assertRaises(ValueError): probe.derive_second_weapon(source, encoded)
        broken = bytearray(self.changed); broken[0x8a3] ^= 1
        with self.assertRaises(ValueError): probe.equipment_expected_source(ROOT / "original_game", broken, "eqt-weapon")

    def test_success_changes_only_flags_and_attack_incompatibility_changes_nothing(self):
        for scenario, case in self.cases.items():
            source, _, assets, _, _, _, _ = case
            expected = assets["equipment"]["expected_source"]
            diff = [i for i, (a, b) in enumerate(zip(source[:-4], expected[:-4])) if a != b]
            self.assertEqual(diff, [0x12ad, 0x12b1, 0x12eb] if scenario == "eqt-weapon" else [])
            self.assertEqual(expected[probe.UNITS + 10:probe.UNITS + 26].hex(),
                "000040844001800080008000800080c9" if scenario == "eqt-weapon" else "4000408400c0800080008000800080c9")
            self.assertEqual(list(struct.unpack_from("<4H", expected, probe.UNITS + 0x48)),
                [26, 12, 97, 2] if scenario == "eqt-weapon" else [16, 12, 97, 2])
            self.assertEqual(struct.unpack_from("<4H", expected, probe.UNITS + 0x40), (42, 42, 0, 0))
            self.assertEqual(expected[probe.UNITS + 5], 0)
            self.assertEqual(expected[probe.UNITS + 0x3c], 0)
            self.assertEqual(expected[0x8a3:probe.UNITS], self.source[0x8a3:probe.UNITS])
            measured = bytearray(expected)
            measured[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3)); measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
            probe.check_load_measurement(expected, measured, assets["equipment"]["steps"])
            for offset in (probe.META + 3, 0x8a3, probe.UNITS + 0x19, probe.UNITS + 5, probe.UNITS + 0x48):
                bad = bytearray(measured); bad[offset] ^= 1; bad[-4:] = sum(bad[:-4]).to_bytes(4, "little")
                with self.assertRaises(AssertionError): probe.check_load_measurement(expected, bad, assets["equipment"]["steps"])

    def test_full_resource_flags_stats_selected_and_stale_page_rejection(self):
        for scenario, case in self.cases.items():
            plan = case[2]["equipment"]
            for templates, fields in ((case[-2], case[-1]), (plan["after_templates"], plan["after_fields"])):
                for selected in (0, 2):
                    frame = self.page(templates, selected)
                    detail = probe.equipment_page(frame, templates, fields, selected)
                    self.assertEqual(detail["checked_pixels"], 32723)
                    self.assertEqual(detail["list_mode"], 0)
                    self.assertIsNone(probe.equipment_page(frame, templates, fields, 2 if selected == 0 else 0))
                    for point in ((13, 101), (13, 145), (157, 67), (117, 67), (117, 79), (157, 79)):
                        bad = frame.copy(); bad.putpixel(point, (255, 0, 255))
                        self.assertIsNone(probe.equipment_page(bad, templates, fields, selected))
            if scenario == "eqt-weapon":
                stale = self.page(case[-2], 2)
                self.assertIsNone(probe.equipment_page(stale, plan["after_templates"], plan["after_fields"], 2))

    def protocol(self, folder, scenario, *, stop=None, unknown_save=False):
        case = self.cases[scenario]
        source, focus, assets, actor, selection, templates, fields = case
        encoded_source = self.encoded if scenario == "eqt-weapon" else self.encrypted
        save = folder / "FD2.SAV"; save.write_bytes(encoded_source)
        measured = bytearray(assets["equipment"]["expected_source"])
        measured[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3))
        if unknown_save: measured[0x8a3] ^= 1
        measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
        encoded = bytes(a ^ b ^ c for a, b, c in zip(encoded_source, source, measured))
        capture = Mock(last=Image.new("RGB", (320, 200)), artifacts=folder)
        self.last_capture = capture
        armed = [False]
        def key(name):
            if armed[0] and name == "ENTER": save.write_bytes(encoded); armed[0] = False
        capture.key.side_effect = key
        def wait(capture, name, predicate, checkpoints, **kwargs):
            if name == stop: raise TimeoutError("equipment resource gate")
            if name == "eqt-save-default-yes": armed[0] = True
            if name in ("eqt-list-zero", "eqt-selected-slot1", "eqt-selected-slot2", "eqt-reopened-list-zero") or name.startswith("eqt-confirm-"):
                after = name.startswith("eqt-confirm-") or name == "eqt-reopened-list-zero"
                t = assets["equipment"]["after_templates"] if after else templates
                selected = 0 if name.endswith("zero") else 1 if name == "eqt-selected-slot1" else 2
                detail = predicate(self.page(t, selected)); self.assertIsNotNone(detail)
                return capture.last, detail
            return capture.last, {"focus_screen": [52, 76]}
        report = {"kind": "original", "checkpoints": []}
        with patch.object(probe, "wait_title"), patch.object(probe, "save_checkpoint"), patch.object(probe, "wait_state", side_effect=wait):
            probe.exercise(capture, *case, {3: [capture.last] * 3}, save, encoded_source, save.stat().st_mtime_ns, report, scenario=scenario)
        return capture, report

    def test_protocol_stays_after_confirmation_then_reopens_zero_and_saves_once(self):
        for scenario in self.cases:
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                capture, report = self.protocol(Path(directory), scenario)
                expected = ["UP", "ENTER", "Z", "ENTER", "ENTER", "ENTER", "RIGHT", "ENTER", "DOWN", "DOWN", "ENTER"]
                if scenario == "eqt-incompatible": expected += ["SPACE"]
                expected += ["ESC", "RIGHT", "ENTER", "ESC", "ESC", "ESC", "ESC", "LEFT", "ENTER", "ENTER", "LEFT", "ENTER", "ENTER"]
                self.assertEqual([c.args[0] for c in capture.key.call_args_list], expected)
                self.assertEqual(len(expected), 24 if scenario == "eqt-weapon" else 25)
                self.assertTrue(report["equipment_measurement"]["runtime_raw_inventory_measured"])
                self.assertEqual(report["input_scope"]["compatible_equipment_confirmations"], int(scenario == "eqt-weapon"))

    def test_failed_resource_stops_keys_and_unknown_saved_bytes_are_preserved_then_rejected(self):
        for scenario in self.cases:
            with tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(TimeoutError): self.protocol(Path(directory), scenario, stop="eqt-confirm-1-stays-slot2")
                self.assertEqual(len(self.last_capture.key.call_args_list), 11)
                self.assertFalse((Path(directory) / "eqt-measured.SAV").exists())
            with tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(AssertionError): self.protocol(Path(directory), scenario, unknown_save=True)
                self.assertTrue((Path(directory) / "eqt-measured.SAV").is_file())

    def test_eqt_original_only_keeps_completed_save_guards(self):
        for scenario in self.cases:
            for tamper in (False, True):
                with tempfile.TemporaryDirectory() as directory:
                    result, _ = RunTest.model(self, Path(directory) / "new", scenario=scenario, final_save_change=tamper)
                    self.assertTrue(result["probe_only"] and not result["ok"])
                    self.assertEqual(result["probe_cases_passed"], not tamper)
                    self.assertEqual(result["copy_save_matches_completed_state_after_cleanup"], not tamper)
                    self.assertNotIn("copy_save_unchanged_after_cleanup", result)


class RunTest(Fixture, unittest.TestCase):
    def model(self, output, *, capture_error=False, cleanup_error=False, final_save_change=False, source_change=False, fixture_change=None,
              scenario="controls", complete_herb=False, paired=False, failure_role="original", late_save_change=False, rgb_ok=True,
              drop_cache_difference=False):
        events = []
        fixture = (ROOT / "original_game").resolve() / "START.SAV"
        read_bytes, stat = Path.read_bytes, Path.stat
        tamper = False
        def read(path):
            result = read_bytes(path)
            return result + b"changed" if tamper and path == fixture and fixture_change == "bytes" else result
        def file_stat(path, *args, **kwargs):
            result = stat(path, *args, **kwargs)
            if tamper and path == fixture and fixture_change == "mtime":
                return SimpleNamespace(st_mode=result.st_mode, st_mtime_ns=result.st_mtime_ns + 2_000_000_000)
            return result
        @contextmanager
        def running(command, cwd, env, log, driver, receipts):
            role = "original" if "-conf" in command else "sdl"
            events.append("started-" + role if paired else "started")
            if role == "sdl":
                self.assertEqual(Path(env["FD2SDL_SAVE_PATH"]), output / "sdl/FD2.SAV")
                self.assertEqual(cwd, ROOT.resolve())
            receipt = {"pid": 123 if role == "original" else 456, "cleaned": False, "forced_cleanup": False}
            receipts.append(receipt)
            try:
                yield Mock(pid=123)
            finally:
                receipt.update(cleaned=True, exit_code=0, forced_cleanup=cleanup_error and role == failure_role)
                events.append("closed-" + role if paired else "closed")
            if cleanup_error and role == failure_role:
                raise RuntimeError("cleanup error")
        def copytree(source, target):
            target.mkdir()
        calls = 0
        def hashes(path):
            nonlocal calls, tamper
            calls += 1
            fail_at = 3 if paired and failure_role == "sdl" else 2
            if calls == fail_at:
                tamper = True
            if calls == fail_at and final_save_change:
                save = output / ("sdl/FD2.SAV" if failure_role == "sdl" else "original/game/FD2.SAV" if paired else "game/FD2.SAV")
                stamp = save.stat()
                os.utime(save, ns=(stamp.st_atime_ns, stamp.st_mtime_ns + 2_000_000_000))
            return {"source": "changed" if calls == fail_at and source_change else "stable"}
        args = SimpleNamespace(root=ROOT, artifacts=output, driver=ROOT / "tools/run_original_item_list.py", dosbox=ROOT / "tools/run_original_item_list.py", scenario=scenario)
        if paired:
            args.sdl = ROOT / "tools/run_original_magic_list.py"
        prepared = (self.source, self.focus, self.assets, self.actor, self.selection, self.templates, self.fields)
        if scenario != "controls" and scenario not in probe.SAVED_SCENARIOS:
            source = oracle.derive_injured(probe.decode_save(fixture), fixture.read_bytes())[1] if scenario == "herb-injured" else None
            prepared = probe.prepare(ROOT / "original_game", source=source, herb=True)
        def exercise(*args, **kwargs):
            record = args[-1]
            if capture_error and record["kind"] == failure_role:
                raise RuntimeError("first resource error")
            if scenario in probe.SAVED_SCENARIOS:
                source, assets, save, data = args[1], args[3], args[-4], args[-3]
                self.assertEqual(probe.decode_save(save), source)
                equipment = scenario in probe.EQUIPMENT_SCENARIOS
                plan = assets["equipment" if equipment else "drop"]
                measured = bytearray(plan["expected_source"])
                measured[probe.META + 5:probe.META + 9] = bytes((6, 14, 1, 3))
                if drop_cache_difference and record["kind"] == "sdl":
                    measured[probe.UNITS + 2] ^= 1
                measured[-4:] = sum(measured[:-4]).to_bytes(4, "little")
                encoded = bytes(a ^ b ^ c for a, b, c in zip(data, source, measured))
                work = output / record["kind"] if paired else output
                snapshot = work / ("eqt-measured.SAV" if equipment else "drop-measured.SAV")
                save.write_bytes(encoded); snapshot.write_bytes(encoded)
                record["equipment_measurement" if equipment else "drop_measurement"] = {**probe.check_load_measurement(plan["expected_source"], measured, plan["steps"]),
                    "runtime_raw_inventory_measured": True, "snapshot_sha256": hashlib.sha256(encoded).hexdigest(),
                    "measured_inventory_hex": measured[probe.UNITS + 10:probe.UNITS + 26].hex(),
                    "measured_derived_values": list(struct.unpack_from("<4H", measured, probe.UNITS + 0x48)),
                    "raw_flags": measured[probe.UNITS + 5], "unit_experience": measured[probe.UNITS + 0x3c]}
                record["completed_save"] = {"snapshot": str(snapshot), "sha256": hashlib.sha256(encoded).hexdigest(),
                                            "mtime_ns": save.stat().st_mtime_ns}
                return
            record["input_scope"] = {"valid_use_confirmations": 0, "code0_confirmations": 4, "equip_drop_give_confirmations": 0,
                "save_or_end_sent": False, "cancel_route": ["Use list", "ITEM submenu", "COMMAND", "selection", "browse"]}
            if complete_herb:
                for key in ("herb_outcome", "herb_measurement", "herb_detail_closed"):
                    record[key] = {"hp": 42, "max_hp": 42, "mp": 0, "max_mp": 0, "level": 1,
                                   "experience": 99 if scenario == "herb-injured" else 0, "visible_inventory_ids": [0, 132]}
        def paired_rgb(*args):
            if late_save_change:
                save = output / "original/game/FD2.SAV"
                stamp = save.stat()
                os.utime(save, ns=(stamp.st_atime_ns, stamp.st_mtime_ns + 2_000_000_000))
            return {"ok": rgb_ok, "checked_pixels": 31955}
        with patch.object(probe, "prepare", return_value=prepared), patch.object(probe, "hashes", side_effect=hashes), \
                patch.object(probe.shutil, "copytree", side_effect=copytree), patch.object(probe, "running", running), \
                patch.object(probe, "Capture"), patch.object(probe, "title_templates", return_value={}), \
                patch.object(Path, "read_bytes", read), patch.object(Path, "stat", file_stat), \
                patch.object(probe, "compare_paired_checkpoints", side_effect=paired_rgb), \
                patch.object(probe, "exercise", side_effect=exercise):
            return probe.run(args), events

    def test_original_only_success_keeps_ok_false_and_cleanup_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            result, events = self.model(Path(directory) / "new")
        self.assertFalse(result["ok"])
        self.assertTrue(result["probe_only"] and result["probe_cases_passed"])
        self.assertEqual((result["status"], events), ("passed", ["started", "closed"]))
        self.assertTrue(result["copy_save_unchanged_after_cleanup"])

    def test_final_save_mtime_source_and_cleanup_failure_revoke_pass(self):
        for guard in ("final_save_change", "source_change", "cleanup_error"):
            with self.subTest(guard=guard), tempfile.TemporaryDirectory() as directory:
                result, events = self.model(Path(directory) / "new", **{guard: True})
                self.assertFalse(result["probe_cases_passed"] or result["ok"])
                self.assertEqual(result["status"], "failed")
                self.assertEqual(events, ["started", "closed"])

    def test_first_resource_failure_preserved_and_process_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            result, events = self.model(Path(directory) / "new", capture_error=True, final_save_change=True)
        self.assertEqual(result["error"], "first resource error")
        self.assertFalse(result["probe_cases_passed"])
        self.assertEqual(events[-1], "closed")

    def test_final_fixture_bytes_and_mtime_revoke_pass(self):
        for guard in ("bytes", "mtime"):
            with self.subTest(guard=guard), tempfile.TemporaryDirectory() as directory:
                result, events = self.model(Path(directory) / "new", fixture_change=guard)
                self.assertFalse(result["fixture_unchanged"])
                self.assertFalse(result["probe_cases_passed"] or result["ok"])
                self.assertEqual(events, ["started", "closed"])

    def test_junction_target_rejected_before_mkdir_or_source_read(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory).resolve()
            root, original = base / "candidate", base / "actual-evidence"
            actual_resolve = Path.resolve
            def resolve(path, *args, **kwargs):
                return original if path == root / "original_game" else actual_resolve(path, *args, **kwargs)
            with patch.object(Path, "resolve", resolve), patch.object(Path, "mkdir") as mkdir, \
                    patch.object(probe, "hashes") as hashes, self.assertRaisesRegex(ValueError, "original_game"):
                probe.run(SimpleNamespace(root=root, artifacts=original / "leak"))
            mkdir.assert_not_called()
            hashes.assert_not_called()

    def test_herb_endpoints_require_all_checkpoints_and_keep_original_only(self):
        for scenario in ("herb-full", "herb-injured"):
            for complete in (False, True):
                with self.subTest(case=scenario, complete=complete), tempfile.TemporaryDirectory() as directory:
                    result, events = self.model(Path(directory) / "new", scenario=scenario, complete_herb=complete)
                    self.assertEqual(result["probe_cases_passed"], complete)
                    self.assertFalse(result["ok"])
                    self.assertTrue(result["probe_only"])
                    self.assertEqual(events, ["started", "closed"])
                    self.assertTrue(result["fixture_unchanged"] and result["copy_save_unchanged_after_cleanup"])
                    if scenario == "herb-injured":
                        self.assertEqual(result["derived_fixture"]["derived_sha256"], "15cdaa8ce74038f2031d94dcf1983c0597b506e776a2b6028c87ffacb45ec17d")


class PairingTest(Fixture, unittest.TestCase):
    model = RunTest.model

    def test_equipment_pairs_compare_complete_saves_and_block_on_endpoint_failure(self):
        for scenario in probe.EQUIPMENT_SCENARIOS:
            for failure in ({}, {"final_save_change": True}, {"late_save_change": True}, {"drop_cache_difference": True}):
                with self.subTest(scenario=scenario, failure=failure), tempfile.TemporaryDirectory() as directory:
                    output = Path(directory) / "new"
                    result, events = self.model(output, paired=True, scenario=scenario, **failure)
                    self.assertEqual(result["ok"], not failure)
                    self.assertEqual(result["probe_cases_passed"], not failure)
                    self.assertFalse(result["probe_only"])
                    if failure.get("final_save_change"):
                        self.assertEqual(events, ["started-original", "closed-original"])
                        self.assertFalse((output / "sdl").exists())
                    else:
                        self.assertEqual(events, ["started-original", "closed-original", "started-sdl", "closed-sdl"])
                    if not failure:
                        self.assertTrue(result["paired_equipment_save"]["full_encoded_bytes_equal"])
                        self.assertTrue(result["paired_equipment_save"]["full_plaintext_bytes_equal"])
                        self.assertFalse(result["paired_equipment_save"]["cache_or_navigation_normalized"])
                        self.assertEqual((output / "original/eqt-measured.SAV").read_bytes(), (output / "sdl/eqt-measured.SAV").read_bytes())
                        self.assertEqual(result["paired_terminal_values"]["original"], result["paired_terminal_values"]["sdl"])

    def test_four_drop_cases_use_same_fixture_then_compare_two_complete_real_save_artifacts(self):
        for scenario in probe.DROP_SCENARIOS:
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "new"
                result, events = self.model(output, paired=True, scenario=scenario)
                self.assertEqual(events, ["started-original", "closed-original", "started-sdl", "closed-sdl"])
                self.assertTrue(result["ok"] and result["probe_cases_passed"] and not result["probe_only"])
                self.assertEqual((output / "original/drop-measured.SAV").read_bytes(), (output / "sdl/drop-measured.SAV").read_bytes())
                self.assertTrue(result["paired_drop_save"]["full_encoded_bytes_equal"])
                self.assertTrue(result["paired_drop_save"]["full_plaintext_bytes_equal"])
                self.assertFalse(result["paired_drop_save"]["cache_or_navigation_normalized"])
                self.assertTrue(result["copy_save_matches_completed_state_after_cleanup"])
                self.assertNotIn("copy_save_unchanged_after_cleanup", result)
                self.assertEqual(result["paired_terminal_values"]["original"], result["paired_terminal_values"]["sdl"])
                self.assertEqual([case["runtime_fixture_sha256"] for case in result["cases"]], [result["runtime_fixture_sha256"]] * 2)
                if scenario == "drop-last":
                    self.assertEqual(result["derived_fixture"]["derived_plain_sha256"], probe.DROP_LAST_PLAIN_SHA256)
                    final = probe.decode_save(output / "sdl/drop-measured.SAV")
                    self.assertTrue(all(final[probe.UNITS + 10 + 2 * i] & 128 for i in range(8)))

    def test_drop_original_failure_blocks_sdl_and_late_artifact_or_mtime_cannot_pass(self):
        for kwargs in ({"capture_error": True}, {"cleanup_error": True}, {"fixture_change": "mtime"},
                       {"final_save_change": True}):
            with self.subTest(kwargs=kwargs), tempfile.TemporaryDirectory() as directory:
                result, events = self.model(Path(directory) / "new", paired=True, scenario="drop-last", **kwargs)
                self.assertEqual(events, ["started-original", "closed-original"])
                self.assertFalse(result["ok"] or result["probe_cases_passed"])
        for kwargs in ({"late_save_change": True}, {"rgb_ok": False}, {"drop_cache_difference": True},
                       {"cleanup_error": True, "failure_role": "sdl"}):
            with self.subTest(kwargs=kwargs), tempfile.TemporaryDirectory() as directory:
                result, events = self.model(Path(directory) / "new", paired=True, scenario="drop-armor", **kwargs)
                self.assertEqual(events, ["started-original", "closed-original", "started-sdl", "closed-sdl"])
                self.assertFalse(result["ok"] or result["probe_cases_passed"])
                if kwargs.get("drop_cache_difference"):
                    self.assertIn("完整字节不一致", result["error"])

    def test_drop_save_pair_rejects_tampered_hash_or_invalid_checksum(self):
        fixture = ROOT / "original_game/START.SAV"
        for fault in ("hash", "checksum"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as directory:
                p = Path(directory); cases = []
                for role in ("original", "sdl"):
                    data = fixture.read_bytes()
                    if role == "sdl" and fault == "checksum":
                        changed = bytearray(data); changed[-1] ^= 1; data = bytes(changed)
                    path = p / f"{role}.SAV"; path.write_bytes(data)
                    digest = hashlib.sha256(data).hexdigest()
                    cases.append({"kind": role, "completed_save": {"snapshot": str(path), "sha256": digest},
                                  "drop_measurement": {"snapshot_sha256": digest}})
                if fault == "hash": Path(cases[0]["completed_save"]["snapshot"]).write_bytes(b"changed")
                with self.assertRaises(AssertionError): probe.compare_inventory_saves(cases, "drop_measurement")

    def test_each_scenario_runs_original_cleanup_before_sdl_same_fixture(self):
        for scenario in ("controls", "herb-full", "herb-injured"):
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "new"
                result, events = self.model(output, paired=True, scenario=scenario, complete_herb=scenario != "controls")
                self.assertEqual(events, ["started-original", "closed-original", "started-sdl", "closed-sdl"])
                self.assertTrue(result["ok"] and result["probe_cases_passed"])
                self.assertFalse(result["probe_only"])
                self.assertEqual([case["kind"] for case in result["cases"]], ["original", "sdl"])
                self.assertEqual((output / "original/game/FD2.SAV").read_bytes(), (output / "sdl/FD2.SAV").read_bytes())
                self.assertTrue(all(case["status"] == "passed" and case["cleanup_passed"] and case["copy_save_unchanged_after_cleanup"] for case in result["cases"]))
                self.assertEqual(result["cases"][0]["role_expected_submenu_disabled"], [0, 1, 0, 0])
                self.assertEqual(result["cases"][1]["role_expected_submenu_disabled"], [0, 1, 0, 0])
                self.assertTrue(all(case["application"]["sha256"] and case["tool_sources"] for case in result["cases"]))
                self.assertTrue(result["known_capability_differences"]["cross_end_capability_equivalence_claimed"])
                for role in ("original", "sdl"):
                    self.assertEqual(json.loads((output / role / "comparison.json").read_bytes())["status"], "passed")

    def test_any_original_failure_blocks_sdl_even_after_source_restored(self):
        cases = ({"capture_error": True}, {"cleanup_error": True}, {"final_save_change": True},
                 {"source_change": True}, {"fixture_change": "bytes"}, {"fixture_change": "mtime"})
        for failure in cases:
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "new"
                result, events = self.model(output, paired=True, **failure)
                self.assertEqual(events, ["started-original", "closed-original"])
                self.assertFalse((output / "sdl").exists())
                self.assertFalse(result["ok"] or result["probe_cases_passed"])
                self.assertEqual(len(result["cases"]), 1)
                self.assertEqual(result["cases"][0]["status"], "failed")

    def test_sdl_failure_and_late_original_copy_mutation_cannot_pass(self):
        for failure in ({"capture_error": True}, {"cleanup_error": True}, {"final_save_change": True},
                        {"source_change": True}, {"fixture_change": "mtime"}, {"late_save_change": True}, {"rgb_ok": False}):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "new"
                result, events = self.model(output, paired=True, failure_role="sdl", **failure)
                self.assertEqual(events, ["started-original", "closed-original", "started-sdl", "closed-sdl"])
                self.assertFalse(result["ok"] or result["probe_cases_passed"])
                self.assertEqual(result["status"], "failed")
                if failure.get("late_save_change"):
                    self.assertEqual(result["cases"][0]["status"], "failed")
                    self.assertFalse(json.loads((output / "original/comparison.json").read_bytes())["copy_save_unchanged_after_cleanup"])

    def test_first_business_error_is_not_overwritten_by_final_guard(self):
        with tempfile.TemporaryDirectory() as directory:
            result, _ = self.model(Path(directory) / "new", paired=True, capture_error=True, final_save_change=True)
        self.assertEqual(result["error"], "first resource error")
        self.assertEqual(result["cases"][0]["error"], "first resource error")

    def test_bad_sdl_path_rejected_before_mkdir_or_source_read(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(Path, "mkdir") as mkdir, patch.object(probe, "hashes") as hashes, self.assertRaisesRegex(ValueError, "--sdl"):
                probe.run(SimpleNamespace(root=ROOT, artifacts=root / "new", sdl=root / "absent.exe"))
            mkdir.assert_not_called()
            hashes.assert_not_called()


class PairRGBTest(Fixture, unittest.TestCase):
    @staticmethod
    def cases(folder, first, second, a, b, name="checkpoint"):
        result = []
        for role, frame, detail in (("original", first, a), ("sdl", second, b)):
            target = folder / role
            target.mkdir()
            path = target / (name + ".png")
            frame.save(path)
            result.append({"kind": role, "checkpoints": [{"name": name, **detail}],
                           "capture_hashes": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()}})
        return result

    @staticmethod
    def retries(cases, folder, endpoint, count):
        case = cases[endpoint]
        target = case["checkpoints"][0]
        target.update(pid=123 + endpoint, time=100, wgc_transient_retries=count)
        events = []
        for number in range(1, count + 1):
            name = f"{target['name']}-wgc-retry-{number}"
            filename = name + "-last-valid.png"
            path = folder / case["kind"] / filename
            path.write_bytes((folder / case["kind"] / (target["name"] + ".png")).read_bytes())
            case["capture_hashes"][filename] = hashlib.sha256(path.read_bytes()).hexdigest()
            events.append({"name": name, "kind": "wgc_transient", "pid": target["pid"], "time": 90 + number,
                "capture_backend": "Windows.Graphics.Capture HWND", "last_frame_may_precede_error": True,
                "error": "Windows.Graphics.Capture failed: blank or nearly uniform captured client area",
                "last_valid_frame": filename})
        case["checkpoints"][:0] = events
        return case

    def test_typed_wgc_retries_are_preserved_separately_without_reducing_business_rgb(self):
        for counts in ((1, 0), (1, 3)):
            with self.subTest(counts=counts), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                frame = self.list_frame()
                detail = probe.list_page(frame, self.templates, self.fields)
                cases = self.cases(root, frame, frame, detail, detail)
                for endpoint, count in enumerate(counts):
                    self.retries(cases, root, endpoint, count)
                compared = probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)
                self.assertTrue(compared["ok"])
                self.assertEqual(compared["checked_pixels"], 31955)
                self.assertEqual(compared["business_checkpoint_count"], 1)
                self.assertEqual([row["name"] for row in compared["checkpoints"]], ["checkpoint"])
                for endpoint, role in enumerate(("original", "sdl")):
                    diagnostics = compared["wgc_transient_diagnostics"][role]
                    self.assertEqual(len(diagnostics), counts[endpoint])
                    for number, row in enumerate(diagnostics, 1):
                        self.assertEqual(row, {**cases[endpoint]["checkpoints"][number - 1],
                                               "business_checkpoint": "checkpoint", "retry": number})

    def test_malformed_retry_or_missing_duplicate_business_is_not_ignored(self):
        mutations = {
            "backend": lambda rows: rows[0].update(capture_backend="desktop crop"),
            "unknown_kind": lambda rows: rows[0].update(kind="other_diagnostic"),
            "name": lambda rows: rows[0].update(name="unrelated-wgc-retry-1"),
            "ordinal": lambda rows: rows[0].update(name="checkpoint-wgc-retry-0"),
            "pid": lambda rows: rows[0].update(pid=999),
            "time": lambda rows: rows[0].update(time=101),
            "nonfinite_time": lambda rows: rows[0].update(time=float("nan")),
            "reason": lambda rows: rows[0].update(error="permission denied"),
            "last_frame_claim": lambda rows: rows[0].update(last_frame_may_precede_error=False),
            "attachment_name": lambda rows: rows[0].update(last_valid_frame="unrelated.png"),
            "undeclared": lambda rows: rows[1].pop("wgc_transient_retries"),
            "count": lambda rows: rows[1].update(wgc_transient_retries=2),
            "boolean_count": lambda rows: rows[1].update(wgc_transient_retries=True),
            "missing_business": lambda rows: rows.pop(),
            "duplicate_business": lambda rows: rows.append(dict(rows[1])),
            "duplicate_retry": lambda rows: rows.insert(0, dict(rows[0])),
            "late_retry": lambda rows: rows.reverse(),
        }
        frame = self.list_frame()
        detail = probe.list_page(frame, self.templates, self.fields)
        for name, mutate in mutations.items():
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                cases = self.cases(root, frame, frame, detail, detail)
                case = self.retries(cases, root, 0, 1)
                mutate(case["checkpoints"])
                with self.assertRaises(AssertionError):
                    probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)

    def test_retry_gap_or_changed_diagnostic_or_business_png_still_fails(self):
        frame = self.list_frame()
        detail = probe.list_page(frame, self.templates, self.fields)
        for changed in ("gap", "diagnostic_png", "business_png"):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                cases = self.cases(root, frame, frame, detail, detail)
                case = self.retries(cases, root, 0, 2)
                if changed == "gap":
                    case["checkpoints"].pop(0)
                    case["checkpoints"][-1]["wgc_transient_retries"] = 1
                else:
                    name = case["checkpoints"][0]["last_valid_frame"] if changed == "diagnostic_png" else "checkpoint.png"
                    (root / "original" / name).write_bytes(b"modified")
                with self.assertRaisesRegex(AssertionError, "WGC|PNG"):
                    probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)

    def test_actual_rgb_difference_is_not_replaced_by_each_endpoint_resource_pass(self):
        frame = self.list_frame()
        point = (5, 94)
        value = frame.getpixel(point)
        first, second = frame.copy(), frame.copy()
        first.putpixel(point, tuple(channel - 2 for channel in value))
        second.putpixel(point, tuple(channel + 2 for channel in value))
        a, b = (probe.list_page(image, self.templates, self.fields) for image in (first, second))
        self.assertIsNotNone(a)
        self.assertIsNotNone(b)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cases = self.cases(root, first, second, a, b)
            compared = probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)
        self.assertFalse(compared["ok"])
        self.assertEqual(compared["checked_pixels"], 31955)
        self.assertEqual(sum(region["bad_pixels"] for row in compared["checkpoints"] for region in row["static_regions"]), 1)

    def test_identical_list_passes_and_changed_capture_hash_or_missing_checkpoint_rejected(self):
        frame = self.list_frame()
        detail = probe.list_page(frame, self.templates, self.fields)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cases = self.cases(root, frame, frame, detail, detail)
            self.assertTrue(probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)["ok"])
            cases[1]["checkpoints"][0]["name"] = "missing"
            with self.assertRaisesRegex(AssertionError, "检查点"):
                probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)
            cases[1]["checkpoints"][0]["name"] = "checkpoint"
            (root / "sdl/checkpoint.png").write_bytes(b"modified")
            with self.assertRaisesRegex(AssertionError, "PNG"):
                probe.compare_paired_checkpoints(cases, root, self.assets, self.selection)

    def test_different_selection_is_explicit_and_only_same_frames_compare_rgb(self):
        a, b = self.menu_frame(2), self.menu_frame(role="sdl")
        da = probe.submenu_page(a, self.assets, (52, 76), self.templates, self.fields, 2)
        db = probe.submenu_page(b, self.assets, (52, 76), self.templates, self.fields, role="sdl")
        self.assertIsNotNone(da)
        self.assertIsNotNone(db)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compared = probe.compare_paired_checkpoints(self.cases(root, a, b, da, db), root, self.assets, self.selection)
        self.assertTrue(compared["ok"])
        row = compared["checkpoints"][0]
        self.assertEqual(len(row["capability_or_selection_differences"]), 2)
        self.assertEqual([region["name"] for region in row["static_regions"]], ["command-frame29", "command-frame33"])

    def test_target_static_cursor_compared_but_different_lut_phase_is_diagnostic_only(self):
        source, focus, assets, actor, selection, templates, fields = probe.prepare(ROOT / "original_game", herb=True)
        first = HerbTest.target_frame(assets, actor, selection)
        mask = assets["herb"]["target_cursor"].crop((0, 18, 24, 24))
        first.paste(mask.convert("RGB"), (52, 94), mask.point(lambda index: 255 if index else 0, mode="L"))
        second = first.copy()
        phase, image = next((phase, image) for phase, image in selection["ranges"] if image.tobytes() != selection["ranges"][0][1].tobytes())
        second.paste(image.convert("RGB"), selection["position"])
        da, db = (probe.herb_target(image, assets, selection, templates, fields) for image in (first, second))
        self.assertIsNotNone(da)
        self.assertIsNotNone(db)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compared = probe.compare_paired_checkpoints(self.cases(root, first, second, da, db), root, assets, selection)
        self.assertTrue(compared["ok"])
        row = compared["checkpoints"][0]
        self.assertEqual(row["static_regions"][0]["checked_pixels"], 90)
        self.assertFalse(row["range_rgb_diagnostic"]["used_for_pass"])
        self.assertFalse(row["range_rgb_diagnostic"]["within_tolerance"])


if __name__ == "__main__":
    unittest.main()
