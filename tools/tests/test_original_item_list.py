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


class RunTest(Fixture, unittest.TestCase):
    def model(self, output, *, capture_error=False, cleanup_error=False, final_save_change=False, source_change=False, fixture_change=None):
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
        args = SimpleNamespace(root=ROOT, artifacts=output, driver=ROOT / "tools/run_original_item_list.py", dosbox=ROOT / "tools/run_original_item_list.py")
        prepared = (self.source, self.focus, self.assets, self.actor, self.selection, self.templates, self.fields)
        with patch.object(probe, "prepare", return_value=prepared), patch.object(probe, "hashes", side_effect=hashes), \
                patch.object(probe.shutil, "copytree", side_effect=copytree), patch.object(probe, "running", running), \
                patch.object(probe, "Capture"), patch.object(probe, "title_templates", return_value={}), \
                patch.object(Path, "read_bytes", read), patch.object(Path, "stat", file_stat), \
                patch.object(probe, "exercise", side_effect=RuntimeError("first resource error") if capture_error else None):
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


if __name__ == "__main__":
    unittest.main()
