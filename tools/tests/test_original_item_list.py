"""原版ITEM只读校准：真实资源与纯协议回归，不启动窗口。"""
from contextlib import contextmanager
import hashlib
import json
import os
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

    def menu_frame(self, selected=0, *, give_enabled=False, actor_overlay=True):
        image = Image.new("RGB", (320, 200))
        for item, command in enumerate((8, 9, 10, 11)):
            index = command * 3 + (0 if give_enabled else 2) if item == 1 else command * 3 + int(item == selected)
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


class RunTest(Fixture, unittest.TestCase):
    def model(self, output, *, capture_error=False, cleanup_error=False, final_save_change=False, source_change=False, fixture_change=None, scenario="controls", complete_herb=False):
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
            events.append("started")
            receipt = {"pid": 123, "cleaned": False, "forced_cleanup": False}
            receipts.append(receipt)
            try:
                yield Mock(pid=123)
            finally:
                receipt.update(cleaned=True, exit_code=0)
                events.append("closed")
            if cleanup_error:
                raise RuntimeError("cleanup error")
        def copytree(source, target):
            target.mkdir()
        calls = 0
        def hashes(path):
            nonlocal calls, tamper
            calls += 1
            if calls == 2:
                tamper = True
            if calls == 2 and final_save_change:
                save = output / "game/FD2.SAV"
                stamp = save.stat()
                os.utime(save, ns=(stamp.st_atime_ns, stamp.st_mtime_ns + 2_000_000_000))
            return {"source": "changed" if calls == 2 and source_change else "stable"}
        args = SimpleNamespace(root=ROOT, artifacts=output, driver=ROOT / "tools/run_original_item_list.py", dosbox=ROOT / "tools/run_original_item_list.py", scenario=scenario)
        prepared = (self.source, self.focus, self.assets, self.actor, self.selection, self.templates, self.fields)
        def exercise(*args, **kwargs):
            if capture_error:
                raise RuntimeError("first resource error")
            if complete_herb:
                for key in ("herb_outcome", "herb_measurement", "herb_detail_closed"):
                    args[-1][key] = {"model": True}
        with patch.object(probe, "prepare", return_value=prepared), patch.object(probe, "hashes", side_effect=hashes), \
                patch.object(probe.shutil, "copytree", side_effect=copytree), patch.object(probe, "running", running), \
                patch.object(probe, "Capture"), patch.object(probe, "title_templates", return_value={}), \
                patch.object(Path, "read_bytes", read), patch.object(Path, "stat", file_stat), \
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


if __name__ == "__main__":
    unittest.main()
