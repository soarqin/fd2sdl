"""B7 三箱/六村民败北的原资源与输入合同纯测试；不启动窗口。"""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_stage1_branches as b
import fd2_stage1_complete_oracle as o
import run_original_stage1_complete as complete
from run_original_reward import actor_view
from test_stage1_complete_oracle import synthetic_entry, synthetic_post_turn3

ORIGINAL = Path(__file__).resolve().parents[2] / "original_game"


def encode(plain):
    source = o.decode_save(ORIGINAL / "START.SAV")
    encrypted = (ORIGINAL / "START.SAV").read_bytes()
    return bytes(a ^ c ^ d for a, c, d in zip(encrypted, source, plain))


def hero_frame(prepared, source, phase=0):
    result = Image.new("RGB", (320, 200), (17, 11, 9))
    key = (tuple(o.actor(source, 0)[:2]), bool(o.actor(source, 0)[5] & 0x80), bool(source[b.COMPLETED + prepared["chest"]["map_cell_index"]]))
    x, y = b.screen(source, o.actor(source, 0)[:2])
    result.paste(prepared["hero_templates"][key][phase].convert("RGB"), (x, y - 6))
    return result


def moved(prepared, source, ordinal):
    destination = prepared["actions"][ordinal]["destination"]
    steps = b.natural.navigation(source, destination)
    result = bytearray(steps[-1][1] if steps else source)
    result[o.UNITS:o.UNITS + 2] = bytes(destination)
    return result


class Stage1BranchesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.path = Path(cls.folder.name)
        cls.fixture = cls.path / "post-turn3.SAV"
        cls.post = synthetic_post_turn3()
        cls.fixture.write_bytes(encode(cls.post))
        cls.entry = cls.path / "entry.SAV"
        cls.entry.write_bytes(encode(synthetic_entry()))
        with patch.object(complete, "source_origin", return_value={"pure_model_only": True}):
            cls.chests = {scenario: b.prepare(ORIGINAL, cls.fixture, scenario, cls.path / "source.json")
                for scenario in b.SCENARIOS[:3]}
        with patch.object(o, "validate_origin_report", return_value={"pure_model_only": True}):
            cls.defeat = b.prepare(ORIGINAL, cls.entry, "villager-defeat", cls.path / "entry.json")

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_exact_three_original_cells_and_manifest_preserve_full_source(self):
        before, stamp = self.fixture.read_bytes(), self.fixture.stat().st_mtime_ns
        for name, prepared in self.chests.items():
            source, manifest = prepared["source"], prepared["manifest"]
            actual = [{"offset": at, "before": a, "after": z} for at, (a, z) in enumerate(zip(self.post, source)) if a != z]
            self.assertEqual(actual, [{key: row[key] for key in ("offset", "before", "after")} for row in manifest["plaintext_patches"]])
            allowed = {o.UNITS, o.UNITS + 1, o.UNITS + 59} | set(range(o.META + 3, o.META + 9)) | set(range(len(source) - 4, len(source)))
            allowed |= {o.UNITS + raw * 80 + 5 for raw in range(27) if o.actor(self.post, raw)[6] != 2}
            self.assertTrue({row["offset"] for row in actual} <= allowed)
            self.assertEqual(source[o.UNITS + 10:o.UNITS + 26], self.post[o.UNITS + 10:o.UNITS + 26])
            self.assertTrue(all(o.actor(source, raw)[5] & 4 for raw in range(5, 27)))
            self.assertEqual(o.decode_save(self.fixture), self.post)
            self.assertEqual(prepared["required_snapshots"], ("completed_save",))
            self.assertFalse(manifest["natural_stage1_battle"])
            self.assertEqual(manifest["action_trigger"], "one-step move then WAIT; exit and re-enter completed cell")
            self.assertEqual(sum(abs(a - z) for a, z in zip(manifest["approach_world"], prepared["chest"]["world"])), 1)
            self.assertEqual(source[o.UNITS + 59], 4)
            self.assertEqual(manifest["movement"]["base_canonical_va"], "0x61da1")
            self.assertEqual(manifest["movement"]["path_budget_limit"], 31)
            self.assertIn("corrected fixture GUI pending", manifest["closed_origin_stationary_path_anomaly"])
        self.assertEqual((self.fixture.read_bytes(), self.fixture.stat().st_mtime_ns), (before, stamp))

    def test_origin_binding_cannot_be_skipped_for_synthetic_or_wrong_report(self):
        with self.assertRaises((FileNotFoundError, ValueError)):
            b.prepare(ORIGINAL, self.fixture, "chest-129", self.path / "missing-report.json")
        with self.assertRaises(ValueError): b.prepare(ORIGINAL, self.fixture, "unknown", self.path / "missing.json")
        with self.assertRaises(ValueError): b._derive_chest(ORIGINAL, self.entry, "chest-129")

    def test_old_accelerated_hero40_is_explicitly_restored_to_original_base4(self):
        source = bytearray(self.post); source[o.UNITS + 59] = 40
        source = o.checksum(source)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "old-accelerated.SAV"; path.write_bytes(encode(source))
            before, stamp = path.read_bytes(), path.stat().st_mtime_ns
            for scenario in b.SCENARIOS[:3]:
                _, derived, manifest = b._derive_chest(ORIGINAL, path, scenario)
                self.assertEqual(o.actor(derived, 0)[59], 4)
                self.assertEqual(o.actor(derived, 0)[10:26], o.actor(source, 0)[10:26])
                self.assertEqual(derived[0x8a3:0x12a3], source[0x8a3:0x12a3])
                change = next(row for row in manifest["plaintext_patches"] if row["offset"] == o.UNITS + 59)
                self.assertEqual((change["before"], change["after"]), (40, 4))
                self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), (before, stamp))

    def test_native_path_budget_preflight_rejects_unsafe_or_insufficient_actor(self):
        prepared = self.chests["chest-129"]
        for budget in (0, 32, 33, 40, 255):
            source = bytearray(prepared["source"]); source[o.UNITS + 59] = budget
            with self.subTest(budget=budget), self.assertRaises(ValueError):
                b._approach(ORIGINAL, source, prepared["chest"])
        with patch.object(b, "movement_profile", return_value=(bytes((5,)) * 20, {})):
            with self.assertRaises(ValueError): b._approach(ORIGINAL, prepared["source"], prepared["chest"])

    def test_movement_restore_is_not_a_default_for_other_profile_or_equipment(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "wrong-identity.SAV"
            for offset, value in ((o.UNITS + 32, 2), (o.UNITS + 11, 1), (o.UNITS + 13, 128)):
                source = bytearray(self.post); source[offset] = value
                path.write_bytes(encode(o.checksum(source)))
                with self.subTest(offset=offset), self.assertRaises(ValueError):
                    b._derive_chest(ORIGINAL, path, "chest-129")

    def test_busy_completed_occupied_and_full_inventory_sources_rejected(self):
        modifications = ((o.UNITS + 34, 1), (o.UNITS + 5, 0x80), (b.COMPLETED + 1, 1))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.SAV"
            for at, value in modifications:
                source = bytearray(self.post); source[at] = value
                path.write_bytes(encode(o.checksum(source)))
                with self.subTest(offset=at), self.assertRaises(ValueError): b._derive_chest(ORIGINAL, path, "chest-129")
            source = bytearray(self.post); source[o.UNITS + 10:o.UNITS + 26] = bytes((0, 192)) * 8
            path.write_bytes(encode(o.checksum(source)))
            with self.assertRaises(ValueError): b._derive_chest(ORIGINAL, path, "chest-129")
            source = bytearray(self.post); source[o.UNITS + 80:o.UNITS + 82] = bytes((10, 13))
            path.write_bytes(encode(o.checksum(source)))
            with self.assertRaises(ValueError): b._derive_chest(ORIGINAL, path, "chest-129")

    def test_one_append_then_repeat_completed_wait_no_second_inventory_write(self):
        for prepared in self.chests.values():
            source = prepared["source"]; item = prepared["chest"]["item_id"]
            first = b._after_action(prepared, moved(prepared, source, 0), 0)
            wanted = bytearray(o.actor(source, 0)); wanted[:2] = bytes(prepared["chest"]["world"])
            free = next(slot for slot in range(8) if wanted[10 + 2 * slot] & 0x80)
            wanted[10 + 2 * free:12 + 2 * free] = bytes((0, item)); wanted[5] |= 0x80
            self.assertEqual(o.actor(first, 0), bytes(wanted))
            ended = b._after_end(first); second = b._after_action(prepared, moved(prepared, ended, 1), 1)
            ended = b._after_end(second); repeated = b._after_action(prepared, moved(prepared, ended, 2), 2)
            final = b._after_end(repeated)
            self.assertEqual(o.actor(second, 0)[10:26], o.actor(first, 0)[10:26])
            self.assertEqual(o.actor(repeated, 0)[10:26], o.actor(first, 0)[10:26])
            self.assertEqual(final[o.META], 7)
            self.assertEqual(final[b.COMPLETED + prepared["chest"]["map_cell_index"]], 1)
            self.assertTrue(all(not o.actor(final, raw)[5] & 0x80 for raw in range(27)))
            self.assertEqual(final[0x8a3:0x12a3], self.post[0x8a3:0x12a3])
            self.assertEqual(final[0x312b:-4], self.post[0x312b:-4])

    def test_entire_save_exact_gate_only_active_cache_byte2_is_derived(self):
        for prepared in self.chests.values():
            state = prepared["source"]
            for ordinal in range(3): state = b._after_end(b._after_action(prepared, moved(prepared, state, ordinal), ordinal))
            self.assertTrue(b.check_chest_save(prepared, prepared["source"], state, state)["raw_equal"])
            changed = bytearray(state); changed[o.UNITS + 26 * 80 + 2] ^= 7; changed = o.checksum(changed)
            check = b.check_chest_save(prepared, prepared["source"], changed, state)
            self.assertFalse(check["raw_equal"]); self.assertTrue(check["mask_equal"])
            for at in (o.UNITS + 1, o.UNITS + 3, 0x8a3 + 2, o.UNITS + 27 * 80 + 2,
                    o.UNITS + 10, b.COMPLETED + prepared["chest"]["map_cell_index"], o.META + 10, 0x312b + 9):
                changed = bytearray(state); changed[at] ^= 1
                with self.subTest(item=prepared["chest"]["item_id"], offset=at), self.assertRaises(AssertionError):
                    b.check_chest_save(prepared, prepared["source"], o.checksum(changed), state)

    def test_full720_pixel_actor_cursor_static_terrain_and_original_fg(self):
        for prepared in self.chests.values():
            for world in (prepared["manifest"]["approach_world"], prepared["chest"]["world"]):
                for acted in (False, True):
                    for completed in (False, True):
                        source = b.natural.view(prepared["source"], world, prepared["source"][o.META + 3:o.META + 5])
                        source[o.UNITS:o.UNITS + 2] = bytes(world)
                        source[o.UNITS + 5] = source[o.UNITS + 5] & 0x7f | (0x80 if acted else 0)
                        source[b.COMPLETED + prepared["chest"]["map_cell_index"]] = int(completed)
                        for phase in range(3):
                            frame = hero_frame(prepared, source, phase)
                            hit = b.hero_focus(frame, prepared, source)
                            self.assertEqual(hit["actor_cursor_foreground"]["checked_pixels"], 720)
                            x, y = hit["focus_screen"]
                            frame.putpixel((x + 5, y + 23), (255, 0, 255))
                            self.assertIsNone(b.hero_focus(frame, prepared, source))
        p = self.chests["chest-193"]; source = moved(p, p["source"], 0)
        frame = hero_frame(p, source)
        # 不含FG的cursor+actor不是可接受替代；真实tile177完整RLE写点必须保留。
        unit = o.actor(source, 0); isolated = bytearray(source); isolated[o.META + 1] = 1
        _, composites = b.cursor_assets(ORIGINAL / "FDICON.B24", isolated, p["assets"])
        x, y = b.screen(source, unit[:2]); raw = next(image for image, offset in composites if offset == -6)
        frame.paste(raw.convert("RGB"), (x, y - 6))
        self.assertIsNone(b.hero_focus(frame, p, source))

    def test_wrong_acted_and_open_dialog_do_not_prove_completion(self):
        for prepared in self.chests.values():
            source = prepared["source"]; acted = b._after_action(prepared, source, 0)
            self.assertIsNone(b.hero_focus(hero_frame(prepared, source), prepared, acted))
            frame = hero_frame(prepared, acted)
            frame.paste(prepared["assets"]["dialog_top"].convert("RGB"), (5, 112))
            self.assertIsNone(b.hero_focus(frame, prepared, acted))

    def test_bottom_wait_request_never_claims_unseen_highlight(self):
        p = self.chests["chest-201"]; source = p["source"]
        where = b.screen(source, p["chest"]["world"]); self.assertEqual(where[1], 172)
        self.assertIsNone(b.natural.visible_resource(p["assets"]["commands"][10], (where[0], where[1] + 24)))
        self.assertEqual([page["fragment"] for page in p["pages"]], [421, 422])
        # 201资源仍是拾取name而非Use效果，完整第二行页保留第一行提问。
        for page in p["pages"]: self.assertEqual(page["wait_key_mode"], 0)

    def run_chest_model(self, prepared, *, reject_save=False):
        events = []; state_box = [prepared["source"]]
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "active.SAV"; save.write_bytes(prepared["encoded"])
            guard = [save.read_bytes(), save.stat().st_mtime_ns]
            capture = SimpleNamespace(artifacts=Path(directory), last=Image.new("RGB", (320, 200)),
                key=lambda key: events.append(("key", key)), wait=lambda *a, **kw: None)
            record = {"kind": "original", "checkpoints": []}
            def checkpoint(cap, name, predicate, records, **kwargs):
                events.append(("ack", name)); records.append({"name": name})
                return capture.last, {}
            def empty(original, state, assets, frame):
                state_box[0] = state
                world = (o.actor(state, 0)[0] - 2, max(0, o.actor(state, 0)[1] - 2))
                return {"world": world, "focused": Image.new("RGB", (24, 24))}
            def navigation(cap, p, state, destination, cp, name):
                steps = b.natural.navigation(state, destination)
                result = steps[-1][1] if steps else state
                state_box[0] = result; events.append(("navigate", name)); return result
            def written(path, *previous):
                wanted = o.checksum(state_box[0])
                if reject_save:
                    wrong = bytearray(wanted); wrong[0x312b + 7] ^= 1; wanted = o.checksum(wrong)
                path.write_bytes(encode(wanted)); return wanted
            with patch.object(b, "wait_title"), patch.object(b, "wait_state", side_effect=checkpoint), \
                    patch.object(b.natural, "empty_tile", side_effect=empty), patch.object(b, "_navigate", side_effect=navigation), \
                    patch.object(b, "wait_measured_save", side_effect=written):
                if reject_save:
                    with self.assertRaises(AssertionError): b.exercise(capture, prepared, save, guard, record)
                    self.assertNotIn("business_sequence_completed", record)
                    self.assertEqual(save.read_bytes(), guard[0])
                    self.assertEqual(b.sha(Path(record["completed_save"]["snapshot"]).read_bytes()), record["completed_save"]["sha256"])
                else:
                    b.exercise(capture, prepared, save, guard, record)
                    self.assertTrue(record["business_sequence_completed"])
                    self.assertTrue(record["completed_save"]["measurement"]["repeat_completed_cell_did_not_append_again"])
                    self.assertEqual(o.decode_save(save)[o.META], 7)
            return events, record

    def test_real_exercise_protocol_one_step_entry_exit_reentry_three_end_and_one_actual_save(self):
        for prepared in self.chests.values():
            events, record = self.run_chest_model(prepared)
            for fragment in ((428, 429) if prepared["scenario"] == "chest-193" else (421, 422)):
                self.assertEqual(sum(name.endswith(f"-message-{fragment}") for kind, name in events if kind == "ack"), 1)
            self.assertFalse(any("herb" in name for kind, name in events if kind == "ack"))
            self.assertEqual([a["append_count"] for a in record["chest_actions"]], [1, 0, 0])
            self.assertEqual([a["at_chest"] for a in record["chest_actions"]], [True, False, True])
            self.assertEqual(sum(name.endswith("-end-yes") for kind, name in events if kind == "ack"), 3)
            for index in range(3):
                destination = events.index(("ack", f"b7-chest-{index}-destination-cursor"))
                command = events.index(("ack", f"b7-chest-{index}-command"))
                self.assertEqual(events[destination + 1:command + 1], [("key", "ENTER"), ("ack", f"b7-chest-{index}-command")])
            self.assertEqual(sum(name == "b7-chest-save-yes" for kind, name in events if kind == "ack"), 1)
            if prepared["scenario"] == "chest-201":
                self.assertEqual(sum(name.endswith("-bottom-wait-request") for kind, name in events if kind == "ack"), 2)
                self.assertEqual(sum(name.endswith("-wait-selected") for kind, name in events if kind == "ack"), 1)

    def test_wrong_destination_cursor_stops_before_move_confirmation_or_command(self):
        p = self.chests["chest-129"]
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "active.SAV"; save.write_bytes(p["encoded"])
            guard = [save.read_bytes(), save.stat().st_mtime_ns]; events = []
            capture = SimpleNamespace(artifacts=Path(directory), key=lambda key: events.append(("key", key)), wait=lambda *a, **kw: None)
            bad = Image.new("RGB", (320, 200), (17, 11, 9))
            at = b.screen(p["source"], o.actor(p["source"], 0)[:2])
            cursor = p["assets"]["cursor"]; bad.paste(cursor.convert("RGB"), at, cursor.point(lambda px: 255 if px else 0, mode="L"))
            good = Image.new("RGB", (320, 200), (17, 11, 9))
            target = b.screen(p["source"], p["actions"][0]["destination"])
            good.paste(cursor.convert("RGB"), target, cursor.point(lambda px: 255 if px else 0, mode="L"))
            def checkpoint(cap, name, predicate, records, **kwargs):
                events.append(("ack", name))
                if "move-destination" in name or name.endswith("-destination-cursor"):
                    self.assertIsNotNone(predicate(good))
                    self.assertIsNone(predicate(bad))
                    raise TimeoutError("wrong destination rejected")
                return good, {}
            record = {"checkpoints": []}
            with patch.object(b, "wait_title"), patch.object(b, "wait_state", side_effect=checkpoint):
                with self.assertRaises(TimeoutError): b.exercise(capture, p, save, guard, record)
            self.assertEqual([key for kind, key in events if kind == "key"].count("ENTER"), 2)
            self.assertFalse(any("-command" in name for kind, name in events if kind == "ack"))
            self.assertNotIn("business_sequence_completed", record)

    def test_late_bad_save_cannot_become_business_pass_or_lose_actual_write_guard(self):
        self.run_chest_model(self.chests["chest-129"], reject_save=True)

    def test_mid_protocol_guard_failure_stops_before_command_or_save(self):
        p = self.chests["chest-129"]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "active.SAV"; path.write_bytes(p["encoded"])
            guard = [path.read_bytes(), path.stat().st_mtime_ns]; events = []
            capture = SimpleNamespace(artifacts=Path(directory), key=events.append, wait=lambda *a, **kw: None)
            record = {"checkpoints": []}
            def corrupt(*args, **kwargs):
                path.write_bytes(b"bad"); return None, {}
            with patch.object(b, "wait_title"), patch.object(b, "wait_state", side_effect=corrupt), \
                    patch.object(b, "wait_measured_save") as saved:
                with self.assertRaises(AssertionError): b.exercise(capture, p, path, guard, record)
            self.assertEqual(events, ["UP", "ENTER"]); saved.assert_not_called()
            self.assertNotIn("business_sequence_completed", record)

    def test_six_villagers_hidden_only_result_full_resource_title_and_no_save(self):
        p = self.defeat
        source = o.decode_save(self.entry)
        changed = {at for at, (a, z) in enumerate(zip(source, p["source"])) if a != z}
        self.assertTrue(changed <= {o.UNITS + raw * 80 + 5 for raw in range(5, 11)} | set(range(len(source) - 4, len(source))))
        self.assertEqual(p["required_snapshots"], ()); self.assertTrue(p["result_capture_required"])
        for expected in p["result"]["defeat"]:
            self.assertIsNotNone(b.result_branch(expected, "defeat", p["assets"], p["result"]))
            wrong = expected.copy(); wrong.putpixel((0, 0), (255, 0, 255))
            self.assertIsNone(b.result_branch(wrong, "defeat", p["assets"], p["result"]))
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "active.SAV"; save.write_bytes(p["encoded"])
            guard = [save.read_bytes(), save.stat().st_mtime_ns]; events = []
            capture = SimpleNamespace(artifacts=Path(directory), key=events.append, wait=lambda *a, **kw: None)
            record = {"checkpoints": []}
            def checkpoint(cap, name, predicate, records, **kwargs):
                records.append({"name": name})
                if name == "b7-defeat-game-over":
                    frame = p["result"]["defeat"][1]
                    frame.save(Path(directory) / (name + ".png"))
                    self.assertEqual(predicate(frame)["frame"], 1)
                    return frame, predicate(frame)
                return None, {"kind": "native_defeat_resource", "frame": 0}
            with patch.object(b, "wait_title"), patch.object(b, "wait_state", side_effect=checkpoint), \
                    patch.object(b, "wait_measured_save") as saving:
                b.exercise(capture, p, save, guard, record)
            self.assertEqual(events, ["UP", "ENTER"]); saving.assert_not_called()
            self.assertEqual((save.read_bytes(), save.stat().st_mtime_ns), tuple(guard))
            self.assertTrue(record["business_sequence_completed"])
            fact = record["result_capture"]; file = Path(fact["path"])
            self.assertEqual(fact, {"path": str(file.resolve()), "sha256": hashlib.sha256(file.read_bytes()).hexdigest(),
                "size": file.stat().st_size, "mtime_ns": file.stat().st_mtime_ns})


if __name__ == "__main__": unittest.main()
