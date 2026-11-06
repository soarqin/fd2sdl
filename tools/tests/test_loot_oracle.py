"""B5纯资源/写集/输入协议/最终守卫；不创建Windows窗口。"""
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
from unittest.mock import patch

from PIL import Image

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import fd2_loot_oracle as oracle
import run_original_loot as runner
from run_graphics_tests import decode_save
from run_original_end_turn import META, UNITS, STRIDE
from run_original_load import measuring_focus

ROOT = TOOLS.parent


def paste(canvas, image, position, transparent=False):
    canvas.paste(image, position, image.point(lambda p: 255 if p else 0, mode="L") if transparent else None)


def message_frame(prepared, page, selected=0):
    canvas = Image.new("P", (320, 200), 74); canvas.putpalette(prepared["assets"]["palette"])
    paste(canvas, page["text"], (0, 0), True)
    paste(canvas, prepared["assets"]["dialog_top"], (5, 112))
    paste(canvas, page["portraits"][0], (8, 115))
    if page.get("key"): paste(canvas, page["no_arrow_background"], (191, 181))
    if page.get("choice"):
        for index, at in zip((49 if selected == 0 else 48, 52 if selected == 1 else 51), ((232, 168), (264, 168))):
            paste(canvas, prepared["assets"]["commands"][index], at)
    return canvas.convert("RGB")


def lifecycle_completed(capture, prepared, save, guard, record):
    # 只隔离生命周期：不将此mock标为实际业务／WGC验收。
    snapshot = save.parent.parent / "lifecycle-model.SAV"; snapshot.write_bytes(save.read_bytes())
    record["completed_save"] = {"snapshot": str(snapshot), "sha256": hashlib.sha256(snapshot.read_bytes()).hexdigest(), "mtime_ns": guard[1]}
    record["save_end_load"] = {"complete_remeasured_container": True}


class LootOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = {name: runner.prepare(ROOT / "original_game", name) for name in oracle.SCENARIOS}
        cls.base = decode_save(ROOT / "original_game/START.SAV")
        cls.encrypted = (ROOT / "original_game/START.SAV").read_bytes()

    def test_all_ten_manifest_patches_are_exact_and_xor_roundtrip(self):
        for name, p in self.prepared.items():
            with self.subTest(name=name):
                changed = p["source"]
                self.assertEqual(len(changed), len(self.base))
                self.assertEqual(sum(changed[:-4]), int.from_bytes(changed[-4:], "little"))
                actual = [{"offset": i, "before": a, "after": b} for i, (a, b) in enumerate(zip(self.base, changed)) if a != b]
                self.assertEqual(actual, p["manifest"]["plaintext_patches"])
                self.assertEqual(bytes(a ^ b ^ c for a, b, c in zip(self.encrypted, self.base, p["encoded"])), changed)
                self.assertEqual(changed[0x8a3:UNITS], self.base[0x8a3:UNITS])
                self.assertEqual(changed[META + 18:-4], self.base[META + 18:-4])
                self.assertFalse(p["manifest"]["natural_history_claimed"])

    def test_native_cell_zero_index_and_treasure_40_are_not_rebased(self):
        chest = self.prepared["chest-empty"]["manifest"]["cell"]
        treasure = self.prepared["treasure-money"]["manifest"]["cell"]
        self.assertEqual((chest["world"], chest["index"], chest["parameter"], chest["terrain_flags"] & 0x60), ([7, 13], 0, 192, 0x20))
        self.assertEqual((treasure["world"], treasure["index"], treasure["parameter"], treasure["terrain_flags"] & 0x60), ([0, 10], 2, 5000, 0x40))
        self.assertEqual(self.prepared["treasure-money"]["movement"]["world"], (0, 11))
        self.assertEqual(self.prepared["treasure-money"]["movement"]["position"], (4, 52))

    def test_empty_completion_full_swap_cancel_and_no_have_distinct_whole_states(self):
        for name in ("chest-empty", "chest-swap", "chest-list-cancel", "chest-no", "chest-full-no", "treasure-money"):
            with self.subTest(name=name):
                p = self.prepared[name]; before, state = p["source"], p["model"]["immediate"]
                self.assertEqual(state[UNITS + 0x48:UNITS + 0x50], before[UNITS + 0x48:UNITS + 0x50])
                self.assertTrue(state[UNITS + 5] & 0x80)  # WAIT本身仍提交，不把No当取消WAIT。
                expected = name in ("chest-empty", "treasure-money")
                index = 2 if name == "treasure-money" else 0
                self.assertEqual(state[oracle.CELL_STATE + index], int(expected))
                if name in ("chest-list-cancel", "chest-no", "chest-full-no"):
                    self.assertEqual(state[UNITS + 10:UNITS + 26], before[UNITS + 10:UNITS + 26])
                if name == "chest-swap":
                    self.assertEqual(state[oracle.CELL_ACTIONS:oracle.CELL_ACTIONS + 3], b"\0\0\0")
                    self.assertEqual(list(state[UNITS + 11:UNITS + 26:2]), [132, 192, 1, 2, 3, 4, 5, 192])
                self.assertEqual(state[0x8a3:UNITS], before[0x8a3:UNITS])

    def test_slot_remove_keeps_stale_tail_and_does_not_recompute(self):
        actor = bytearray(self.base[UNITS:UNITS + STRIDE])
        actor[10:26] = bytes((0x40, 0, 0x40, 132, 0, 192, 0x80, 77, 0x80, 88, 0x80, 99, 0x80, 111, 0x80, 123))
        changed = oracle.inventory_swap(actor, 1, 192)
        self.assertEqual(changed[10:26], bytes((0x40, 0, 0, 192, 0, 192, 0x80, 88, 0x80, 99, 0x80, 111, 0x80, 123, 0x80, 123)))
        self.assertEqual(changed[26:], actor[26:])
        with self.assertRaises(ValueError): oracle.inventory_swap(actor, 7, 192)

    def test_physical_drop_reward_death_and_callback_only_declared_fields(self):
        for name in ("loot-item", "loot-money", "loot-callback", "loot-story"):
            p = self.prepared[name]; state = p["model"]["immediate"]
            self.assertEqual(struct.unpack_from("<H", state, UNITS + 11 * STRIDE + 0x40)[0], 0)
            self.assertEqual(state[UNITS + 11 * STRIDE + 5], 1)
            self.assertEqual(state[UNITS + 0x3c], 99)
            self.assertEqual(state[UNITS + 0x48:UNITS + 0x50], p["source"][UNITS + 0x48:UNITS + 0x50])
            self.assertEqual(state[META + 9], self.base[META + 9])
        callback = self.prepared["loot-callback"]
        self.assertEqual(callback["source"][META + 1], 14)
        self.assertEqual(callback["source"][UNITS + 13 * STRIDE + 6], 0)
        self.assertEqual(callback["model"]["immediate"][UNITS + 13 * STRIDE + 6], 1)
        self.assertEqual(list(callback["immediate"][META + 5:META + 7]), [3, 5])
        self.assertEqual([p["portrait_id"] for p in callback["stories"]], [1, 3, 1, 3])
        self.assertEqual(self.prepared["loot-story"]["stories"][0]["portrait_id"], 97)

    def test_real_complete_message_text_portrait_arrow_and_button_resources(self):
        for name, p in self.prepared.items():
            for page in p["pages"]:
                with self.subTest(name=name, fragment=page["fragment"]):
                    frame = message_frame(p, page)
                    self.assertIsNotNone(oracle.message_match(frame, page, p["assets"]))
                    index = next(i for i, pixel in enumerate(page["text"].getdata()) if pixel == 205)
                    broken = frame.copy(); broken.putpixel((index % 320, index // 320), (255, 0, 255))
                    self.assertIsNone(oracle.message_match(broken, page, p["assets"]))
                    broken = frame.copy(); broken.putpixel((8, 115), (255, 0, 255))
                    self.assertIsNone(oracle.message_match(broken, page, p["assets"]))
                    if page.get("choice"):
                        other = message_frame(p, page, 1)
                        self.assertIsNone(oracle.message_match(other, page, p["assets"], 0))
                        self.assertIsNotNone(oracle.message_match(other, page, p["assets"], 1))
                    if page.get("key"):
                        self.assertEqual(page["wait_key_mode"], 0)
                        for image in page["arrows"]:
                            broken = frame.copy(); paste(broken, image, (191, 181))
                            self.assertIsNone(oracle.message_match(broken, page, p["assets"]))

    def test_chest_text_accumulates_421_422_and_only_new_423_box_clears(self):
        p = self.prepared["chest-swap"]
        first, appended, fresh, replaced = p["pages"]
        self.assertEqual([page["fragment"] for page in p["pages"]], [421, 422, 423, 425])
        self.assertEqual(first["text"].crop((98,127,308,144)).tobytes(), appended["text"].crop((98,127,308,144)).tobytes())
        self.assertNotEqual(appended["text"].crop((98,127,308,144)).tobytes(), fresh["text"].crop((98,127,308,144)).tobytes())
        frame = message_frame(p, appended)
        self.assertIsNotNone(oracle.message_match(frame, appended, p["assets"]))
        frame.paste(tuple(p["assets"]["palette"][74*3:74*3+3]), (98,127,308,144))
        self.assertIsNone(oracle.message_match(frame, appended, p["assets"]))
        frame = message_frame(p, fresh)
        paste(frame, first["text"].crop((98,127,308,144)).convert("RGB"), (98,127))
        self.assertIsNone(oracle.message_match(frame, fresh, p["assets"]))

    def test_unknown_dynamic_control_and_missing_parameter_fail_before_capture(self):
        p = self.prepared["chest-empty"]
        with patch.object(oracle, "common_tokens", return_value=[-7, -1]):
            with self.assertRaises(ValueError): oracle.message_pages(p["original"], p["assets"], [{"fragment": 422}])
        with self.assertRaises(KeyError): oracle.message_pages(p["original"], p["assets"], [{"fragment": 422, "open": True}])

    def test_no_key_quiet_resets_on_notice_and_its_own_wgc_transient(self):
        p = self.prepared["chest-empty"]
        report = {"checkpoints": []}
        with patch.object(runner, "browse_match", return_value={"kind": "closed"}), patch.object(runner.time, "monotonic") as clock:
            predicate = runner.quiet_focus(p, p["immediate"], report, "gate")
            clock.return_value = 0; self.assertIsNone(predicate(None))
            clock.return_value = 1.9; self.assertIsNone(predicate(None))
            report["checkpoints"].append({"kind": "wgc_transient", "name": "other-wgc-retry-1"})
            clock.return_value = 2; self.assertIsNotNone(predicate(None))
            report["checkpoints"].append({"kind": "wgc_transient", "name": "gate-wgc-retry-1"})
            clock.return_value = 2.1; self.assertIsNone(predicate(None))
            clock.return_value = 4.2; self.assertIsNotNone(predicate(None))
        self.assertEqual(report["gate_quiet"]["resets"], 1)
        self.assertFalse(report["gate_quiet"]["native_timing_claimed"])

    def test_selection_requires_full_dry_lut_and_actual_actor_at_expected_edge(self):
        for name in ("treasure-money", "chest-empty", "loot-item"):
            p = self.prepared[name]
            state = p["focused"]
            frame = Image.new("P", (320, 200), 74); frame.putpalette(p["assets"]["palette"])
            at = runner.screen(state, p["hero"])
            actor = runner.actor_focus_resources(p["original"], state, p["assets"], 0, p["assets"]["cursor"])
            paste(frame, actor["templates"][0], (at[0], at[1] - 6), True)
            probe = p["movement"]
            paste(frame, probe["ranges"][0][1], probe["position"])
            with self.subTest(name=name):
                self.assertIsNotNone(runner.selection_match(frame.convert("RGB"), p, state))
                paste(frame, probe["base"], probe["position"])
                self.assertIsNone(runner.selection_match(frame.convert("RGB"), p, state))

    def test_whole_save_only_excludes_active_plus2_not_y_direction_or_roster(self):
        source = self.prepared["loot-callback"]["model"]["immediate"]
        changed = bytearray(source); changed[UNITS + 13 * STRIDE + 2] ^= 1
        comparison = runner.compare_saves(source, oracle.checksum(changed))
        self.assertFalse(comparison["raw_equal"])
        self.assertTrue(comparison["mask_equal"])
        self.assertEqual([row["offset"] for row in comparison["cache_class_changes"]], [UNITS + 13 * STRIDE + 2])
        for offset in (UNITS + 1, UNITS + 3, 0x8a3 + 2, UNITS + 14 * STRIDE + 2, 0x800, META + 18 + 7):
            with self.subTest(offset=offset):
                changed = bytearray(source); changed[offset] ^= 1
                with self.assertRaises(AssertionError): runner.compare_saves(source, oracle.checksum(changed))
        changed = bytearray(source); changed[META + 1] = 65
        with self.assertRaises(AssertionError): runner.compare_saves(oracle.checksum(changed), oracle.checksum(changed))

    def test_save_gate_matches_native_acted_rule_and_waiter_is_healthy_real_raw1(self):
        for name, p in self.prepared.items():
            with self.subTest(name=name):
                with self.assertRaises(AssertionError): runner.require_save_enabled(p["model"]["immediate"])
                runner.require_save_enabled(p["model"]["final"])
                actor = p["model"]["final"][UNITS + STRIDE:UNITS + 2 * STRIDE]
                self.assertEqual((actor[6], actor[7], actor[5] & 0x85), (2, 9, 0))
                self.assertEqual(actor[0x22:0x28], bytes(6))
                self.assertEqual(struct.unpack_from("<HH", actor, 0x40), (28, 28))
        p = self.prepared["chest-empty"]; at = (52, 76)
        frame = Image.new("P", (320, 200), 74); frame.putpalette(p["assets"]["palette"])
        positions = [tuple(a + b for a, b in zip(at, offset)) for offset in runner.MENU_OFFSETS]
        for index, position in zip((37, 41, 42, 45), positions):
            paste(frame, p["assets"]["commands"][index], position)
        menu = runner.RoundOracle(p["assets"], at)
        self.assertIsNotNone(menu.secondary(frame.convert("RGB"), 0, save_available=False))
        self.assertIsNone(menu.secondary(frame.convert("RGB"), 0, save_available=True))
        paste(frame, p["assets"]["commands"][39], positions[1])
        self.assertIsNone(menu.secondary(frame.convert("RGB"), 0, save_available=False))

    def test_physical_save_destination_rejects_hero_and_uses_real_static_dry_empty_tile(self):
        for name in oracle.SCENARIOS:
            p = self.prepared[name]
            anchor = (2, 10) if name == "treasure-money" else (6, 14)
            state, _ = runner.focus_view(p["model"]["final"], tuple(p["model"]["final"][META + 5:META + 7]), anchor)
            with self.subTest(name=name):
                before = bytes(state)
                plan = runner.save_empty_step(p, state)
                self.assertEqual(state, before)
                occupied = {tuple(state[UNITS + i * STRIDE:UNITS + i * STRIDE + 2])
                            for i in range(state[META + 1]) if not state[UNITS + i * STRIDE + 5] & 1}
                self.assertNotIn(tuple(plan["world"]), occupied)
                self.assertNotIn((plan["world"][0], plan["world"][1] + 1), occupied)
                moved, _ = runner.focus_view(state, anchor, tuple(plan["world"]))
                self.assertEqual(moved[META + 3:META + 5], state[META + 3:META + 5])
                self.assertEqual(plan["checked_terrain_pixels"], 576)
                self.assertTrue(plan["static_dry_unoccupied_same_camera"])
                if p["physical"]:
                    self.assertEqual(tuple(state[UNITS:UNITS + 2]), (7, 14))
                    self.assertEqual(plan["rejected_candidates"][0], {
                        "key": "RIGHT", "world": [7, 14], "reason": "actor_or_lower_actor_overlap"})
                    self.assertNotEqual(plan["world"], [7, 14])
                else:
                    self.assertEqual(plan["key"], "RIGHT")  # 前六场已通过的业务键不变。

    def test_save_destination_cannot_accept_occupied_anchor_or_all_occupied_neighbors(self):
        p = self.prepared["loot-item"]
        state, _ = runner.focus_view(p["model"]["final"], tuple(p["model"]["final"][META + 5:META + 7]), (6, 14))
        bad = bytearray(state)
        bad[UNITS:UNITS + 2] = bytes((6, 14))
        with self.assertRaisesRegex(ValueError, "起点被活动actor占据"):
            runner.save_empty_step(p, oracle.checksum(bad))
        for raw, world in enumerate(((7, 14), (5, 14), (6, 13), (6, 15))):
            bad[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2] = bytes(world)
            bad[UNITS + raw * STRIDE + 5] &= ~1
        with self.assertRaisesRegex(ValueError, "无合法静态干燥空格"):
            runner.save_empty_step(p, oracle.checksum(bad))

    def test_protocol_every_business_key_waits_for_a_resource_gate_and_first_error_stops(self):
        class Capture:
            def __init__(self): self.keys = []; self.pending = False
            def key(self, key):
                if self.pending: raise AssertionError("key without checkpoint")
                self.keys.append(key); self.pending = True
            def wait(self, image): self.pending = False
        for name, p in self.prepared.items():
            capture = Capture(); checkpoints = []
            def wait_state(capture, name, predicate, report, **kwargs):
                capture.pending = False; checkpoints.append(name); return None, {"kind": "protocol_model"}
            def measurement(capture, assets, plan, save, data, stamp, report, checkpoint, **kwargs):
                route = runner.save_empty_step(p, plan["expected_source"])
                self.assertEqual(plan["steps"], [(route["key"], tuple(route["position"]))])
                if "previous_measurement_step" in report:
                    self.assertEqual(plan["steps"], report["previous_measurement_step"])
                report["previous_measurement_step"] = plan["steps"]
                self.assertEqual(plan["expected_source"][META], 2)
                runner.require_save_enabled(plan["expected_source"])
                measured = bytearray(plan["expected_source"])
                measured[META + 5:META + 9] = measuring_focus(measured, plan["steps"])
                measured = oracle.checksum(measured)
                save.write_bytes(bytes(a ^ b ^ c for a, b, c in zip(p["encoded"], p["source"], measured)))
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                save = Path(directory) / "FD2.SAV"; save.write_bytes(p["encoded"])
                guard = [p["encoded"], save.stat().st_mtime_ns]
                with patch.object(runner, "wait_title"), patch.object(runner, "wait_state", side_effect=wait_state), patch.object(runner, "measure_inventory_save", side_effect=measurement):
                    runner.exercise(capture, p, save, guard, {"checkpoints": []})
                self.assertEqual(capture.keys.count("F2"), 1)
                self.assertIn("loot-inventory-after-action", checkpoints)
                self.assertIn("loot-end-default-yes", checkpoints)
                self.assertIn("loot-load-default-yes", checkpoints)
                self.assertIn("loot-wait-raw1-acted", checkpoints)
                self.assertFalse(capture.pending)
        p = self.prepared["loot-item"]; capture = Capture()
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "FD2.SAV"; save.write_bytes(p["encoded"])
            with patch.object(runner, "wait_title"), patch.object(runner, "wait_state", side_effect=TimeoutError("first gate")):
                with self.assertRaisesRegex(TimeoutError, "first gate"):
                    runner.exercise(capture, p, save, [p["encoded"], save.stat().st_mtime_ns], {"checkpoints": []})
            self.assertEqual(capture.keys, ["UP", "ENTER"])

    def test_identical_bytes_rewritten_during_cleanup_fail_and_never_start_sdl(self):
        p = self.prepared["chest-empty"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); original = root / "original_game"; original.mkdir()
            (original / "START.SAV").write_bytes(self.encrypted)
            binary = root / "helper.exe"; binary.write_bytes(b"not executable; pure test")
            args = SimpleNamespace(root=root, artifacts=root / "artifacts", driver=binary, dosbox=binary, sdl=binary, scenario="chest-empty")
            launched = []
            @contextmanager
            def running(argv, cwd, env, log, driver, receipts):
                launched.append(argv)
                receipts.append({"pid": 123, "cleaned": True, "forced_cleanup": False, "exit_code": 0})
                yield SimpleNamespace(pid=123)
                save = args.artifacts / "original/game/FD2.SAV"
                os.utime(save, ns=(save.stat().st_atime_ns, save.stat().st_mtime_ns + 2_000_000_000))
            with patch.object(runner, "prepare", return_value=p), patch.object(runner, "running", side_effect=running), patch.object(runner, "exercise", side_effect=lifecycle_completed):
                result = runner.run(args)
            self.assertEqual(len(launched), 1)
            self.assertFalse(result["ok"])
            self.assertFalse(result["cases"][0]["probe_cases_passed"])
            self.assertFalse(result["cases"][0]["copy_save_matches_completed_state"])
            self.assertTrue((args.artifacts / "original/comparison.json").is_file())
            self.assertFalse((args.artifacts / "sdl").exists())

    def test_probe_only_is_not_paired_acceptance_and_bad_sdl_rejected_before_mkdir(self):
        p = self.prepared["chest-empty"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); original = root / "original_game"; original.mkdir()
            (original / "START.SAV").write_bytes(self.encrypted)
            binary = root / "helper.exe"; binary.write_bytes(b"pure test")
            args = SimpleNamespace(root=root, artifacts=root / "artifacts", driver=binary, dosbox=binary, sdl=root / "absent", scenario="chest-empty")
            with self.assertRaises(ValueError): runner.run(args)
            self.assertFalse(args.artifacts.exists())
            args.sdl = None
            @contextmanager
            def running(argv, cwd, env, log, driver, receipts):
                receipts.append({"pid": 123, "cleaned": True, "forced_cleanup": False, "exit_code": 0})
                yield SimpleNamespace(pid=123)
            with patch.object(runner, "prepare", return_value=p), patch.object(runner, "running", side_effect=running), patch.object(runner, "exercise", side_effect=lifecycle_completed):
                result = runner.run(args)
            self.assertTrue(result["probe_cases_passed"], result)
            self.assertTrue(result["probe_only"])
            self.assertFalse(result["ok"])
            self.assertEqual([case["kind"] for case in result["cases"]], ["original"])


if __name__ == "__main__": unittest.main()
