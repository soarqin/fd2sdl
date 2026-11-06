"""B7 controlled协议/原表投影反例；只模型，不启动窗口/驱动/程序。"""
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_stage1_complete_oracle as o
import run_original_stage1_complete as r
from test_stage1_complete_oracle import synthetic_post_turn3

ORIGINAL = Path(__file__).resolve().parents[2] / "original_game"


def encode(plain):
    old = o.decode_save(ORIGINAL / "START.SAV")
    encrypted = (ORIGINAL / "START.SAV").read_bytes()
    return bytes(a ^ b ^ c for a, b, c in zip(old, encrypted, plain))


def active_fixture(before, state, projection, data):
    result = bytearray(before); result[:1171] = data["stages"][2]["metadata"]
    result[0x8a3:0x12a3] = state["roster"]
    for raw, unit in enumerate(projection["units"]): result[o.UNITS + raw * 80:o.UNITS + (raw + 1) * 80] = unit
    result[0x30a3:0x30c3] = bytes(32)
    camera = projection["camera"]
    result[o.META:o.META + 10] = bytes((1, 15, 2, *camera, 6, 22, 6 - camera[0], 22 - camera[1], 6))
    return o.checksum(result)


class Stage1ControlledRunnerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = o.tables(ORIGINAL)
        cls.source = synthetic_post_turn3()

    def prepare(self, folder, scenario="victory-all-villagers"):
        fixture = Path(folder) / "source.SAV"; fixture.write_bytes(encode(self.source))
        (Path(folder) / "report.json").write_text("{}", encoding="utf-8")
        with patch.object(r, "source_origin", return_value={"model_source_only": True}):
            return r.prepare(ORIGINAL, fixture, scenario, Path(folder) / "report.json")

    def test_raw_player8_and_exact_known_mask_no_constructor_zero_guess(self):
        unit, mask = o.template_record(self.data, 1, 22, [22, 4])
        self.assertEqual((unit[7], unit[8], unit[33]), (8, 8, 3))
        self.assertEqual(struct.unpack_from("<4H", unit, 64), (44, 44, 0, 0))
        self.assertEqual(struct.unpack_from("<4H", unit, 72), (35, 16, 101, 6))
        self.assertEqual([i for i, known in enumerate(mask) if not known], [2] + list(range(0x28, 0x31)))
        self.assertEqual(sum(mask), 70)

    def test_three_branches_preflight_complete30_and15_pages_and_original_town62(self):
        with tempfile.TemporaryDirectory() as folder:
            for scenario in r.SUPPORTED:
                p = self.prepare(folder, scenario)
                self.assertEqual(len(p["pages"]), 30); self.assertEqual(len(p["entry_pages"]), 15)
                self.assertEqual(p["pages"][0]["fragment"], 7 if scenario == "victory-villager-loss" else 6)
                self.assertEqual((p["town"]["city_type"], p["town"]["background_entry"]), (2, 62))
                self.assertEqual(p["town"]["marker_xy_inside_view"], [(59, 26), (10, 144), (59, 163), (130, 150), (242, 31)])
                self.assertEqual(list(p["expected_preparation"]["roster"][400:402]), [15, 0])
                self.assertEqual(p["expected_preparation"]["roster"][403], 2)
                self.assertEqual([list(u[:2]) for u in p["entry"]["units"]],
                    [[7,22],[9,22],[8,23],[10,23],[9,24],[7,24],[8,11],[9,9],[8,9],[10,8],[7,8],[6,7],[11,7],[8,7],[9,7]])
                self.assertEqual(p["entry"]["camera"], [3, 18])
                self.assertTrue(any(row["kind"] == "fragment0/speaker-17/8/raw5" and row["camera"] == [3,18]
                    for row in p["entry"]["focus_ledger"]))

    def test_stage2_focus_tracks_hidden_text_speakers_and_final_actor_not_last_pan(self):
        with tempfile.TemporaryDirectory() as folder:
            p = self.prepare(folder)
            state = dict(p["expected_preparation"]); roster = bytearray(state["roster"])
            roster[2 * 80 + 5] |= 1; roster[4 * 80 + 5] |= 1
            state["roster"] = bytes(roster)
            projected = o.stage2_projection(state, p["data"])
            self.assertEqual(projected["camera"], [3, 18])
            hidden = [row["text_id"] for row in projected["focus_ledger"] if row["kind"] == "hidden_text_speaker_no_focus"]
            self.assertIn(4, hidden); self.assertIn(1, hidden)
            last = projected["focus_ledger"][-1]
            self.assertEqual((last["target"], last["camera"]), ([7,22], [3,18]))
            self.assertEqual(last["va"], "0x12cea")

    def test_manual_slot0_full_unknown_container_guard_and_specific_new_holes(self):
        with tempfile.TemporaryDirectory() as folder:
            p = self.prepare(folder)
            before = p["source"]; wanted = p["expected_preparation"]
            measured = o.write_stage2_slot(before, wanted, 0)
            state, detail = o.check_stage2_manual_save(before, measured, wanted, p["roster_masks"])
            self.assertTrue(detail["other_slots_active_resource_unknown_tails_preserved"])
            allowed = bytearray(measured)
            allowed[o.SLOT_BASE + 400 + 0x28] = 123
            state, _ = o.check_stage2_manual_save(before, o.checksum(allowed), wanted, p["roster_masks"])
            self.assertEqual(state["roster"][400 + 0x28], 123)
            for at in (0x28, o.UNITS + 0x28, o.SLOT_BASE + 400 + 64, o.SLOT_BASE + 480, o.SLOT_BASE + o.SLOT_STRIDE + 9):
                wrong = bytearray(measured); wrong[at] ^= 1
                with self.subTest(offset=at), self.assertRaises(AssertionError):
                    o.check_stage2_manual_save(before, o.checksum(wrong), wanted, p["roster_masks"])

    def test_stage2_exact15_records_and_unused_active_manual_regions_reject_changes(self):
        with tempfile.TemporaryDirectory() as folder:
            p = self.prepare(folder); state = p["expected_preparation"]
            before = o.write_stage2_slot(p["source"], state, 0)
            measured = active_fixture(before, state, p["entry"], p["data"])
            view = bytearray(measured)
            detail = o.check_stage2_active_save(before, measured, state, p["entry"], view, p["data"])
            self.assertEqual(detail["active_count"], 15)
            self.assertFalse(detail["stage2_battle_or_turn3_handler_claimed"])
            for at in (o.UNITS + 6 * 80 + 7, o.UNITS + 6 * 80 + 72, o.UNITS + 15 * 80 + 0x28,
                    o.SLOT_BASE + 9, 0x8a3 + 17, o.META + 10):
                wrong = bytearray(measured); wrong[at] ^= 1
                with self.subTest(offset=at), self.assertRaises(AssertionError):
                    o.check_stage2_active_save(before, o.checksum(wrong), state, p["entry"], view, p["data"])

    def test_stage2_parent_checks_four_resources_per_endpoint_and_off_phase(self):
        assets = r.round_assets(ORIGINAL); menu = r.RoundOracle(assets, (76, 124))
        for role, frames in (("original", [21,15,18,12]), ("sdl", [21,17,18,14]), ("sdl", [22,17,18,14])):
            frame = Image.new("RGB", (320, 200))
            for item, identity in enumerate(frames):
                image = assets["commands"][identity]; at = tuple(a + b for a, b in zip(menu.focus, r.MENU_OFFSETS[item]))
                frame.paste(image.convert("RGB"), at, image.point(lambda p: 255 if p else 0, mode="L"))
            proof = r.stage2_parent(frame, menu, role)
            self.assertEqual([m["frame"] for m in proof["matches"]], frames)
            self.assertEqual(proof["selected"], 0 if frames[0] == 22 else None)
            self.assertIsNone(r.stage2_parent(frame, menu, "sdl" if role == "original" else "original"))

    def test_hotel_blink_off_is_unobserved_wrong_or_double_highlight_rejected(self):
        from fd2_transition_oracle import resources, hotel_menu_match
        assets = resources(ORIGINAL)
        for expected in range(4):
            for bright in (False, True):
                frame = assets["hotel_background"].convert("RGB")
                for index in range(4):
                    state = int(bright and index == expected)
                    image = assets["commands"][2 * index + state]
                    frame.paste(image.convert("RGB"), (201 + 26 * index, 169), image.point(lambda p: 255 if p else 0, mode="L"))
                result = r.hotel_menu_state(frame, assets, expected)
                self.assertIsNotNone(result)
                self.assertEqual(result["selection_observed"], bright)
                self.assertEqual(result["selected"], expected if bright else None)
                self.assertTrue(result["next_action_resource_required"])
                if bright:
                    for wrong in set(range(4)) - {expected}:
                        self.assertIsNone(r.hotel_menu_state(frame, assets, wrong))
                    other = (expected + 1) % 4
                    image = assets["commands"][2 * other + 1]
                    frame.paste(image.convert("RGB"), (201 + 26 * other, 169), image.point(lambda p: 255 if p else 0, mode="L"))
                    self.assertIsNone(r.hotel_menu_state(frame, assets, expected))
                else:
                    self.assertIsNone(hotel_menu_match(frame, assets, expected))

    def test_controlled_protocol_single_right_then_picker_ack_no_extra_greeting_key(self):
        # 控制流模型；像素正反门由上面的真实资源测试独立覆盖。
        with tempfile.TemporaryDirectory() as folder:
            p = self.prepare(folder)
            save = Path(folder) / "active.SAV"; save.write_bytes(p["encoded"])
            guard = [save.read_bytes(), save.stat().st_mtime_ns]
            events = []; record = {"kind": "original", "checkpoints": []}
            capture = SimpleNamespace(artifacts=Path(folder), key=lambda key: events.append(("key", key)), wait=lambda *_: None)
            def checkpoint(cap, name, predicate, records, **kwargs):
                events.append(("ack", name)); records.append({"name": name})
                return None, {"kind": "b7_first_ending"} if name == "b7-continue-or-ending" else {}
            manual = o.write_stage2_slot(p["source"], p["expected_preparation"], 0)
            active = active_fixture(manual, p["expected_preparation"], p["entry"], p["data"])
            outputs = iter((manual, active))
            def measured(path, *_):
                plain = next(outputs); path.write_bytes(encode(plain)); return plain
            with patch.object(r, "wait_title"), patch.object(r, "wait_state", side_effect=checkpoint), \
                    patch.object(r, "wait_measured_save", side_effect=measured):
                r.exercise(capture, p, save, guard, record)
            start = events.index(("ack", "b7-hotel"))
            self.assertEqual(events[start:start + 7], [("ack", "b7-hotel"), ("key", "RIGHT"),
                ("ack", "b7-hotel-save"), ("key", "ENTER"), ("ack", "b7-manual-picker0"),
                ("key", "ENTER"), ("ack", "b7-save660")])
            self.assertEqual(len(record["settlement_acknowledgements"]), 30)
            self.assertEqual(len(record["stage2_entry_acknowledgements"]), 15)
            self.assertTrue(record["business_sequence_completed"])
            self.assertTrue(r.snapshots_unchanged(record))

    def test_unsupported_partial_scenarios_and_protected_output_never_spawn_or_mkdir(self):
        with tempfile.TemporaryDirectory() as folder:
            args = SimpleNamespace(root=ORIGINAL.parent, artifacts=ORIGINAL / "never-output",
                save_fixture=Path(folder) / "fixture", source_report=Path(folder) / "source.json",
                scenario="natural-stage1", driver=Path(folder) / "driver", dosbox=Path(folder) / "dosbox", sdl=None)
            with patch.object(Path, "mkdir") as mkdir, patch.object(r, "running") as running:
                with self.assertRaises(ValueError): r.run(args)
            mkdir.assert_not_called(); running.assert_not_called()
            with self.assertRaises(ValueError):r.prepare(ORIGINAL, Path(folder)/"absent", "natural-stage1", Path(folder)/"absent-report")


if __name__ == "__main__": unittest.main()
