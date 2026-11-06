"""B7自然阶段协议/真实资源纯测；不调用真实planner、游戏或窗口。"""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_stage1_natural as module
import fd2_stage1_complete_oracle as o
import run_original_stage1_complete as runner
from test_stage1_complete_oracle import ORIGINAL, synthetic_entry, synthetic_post_turn3
from test_stage1_complete_runner import encode


class Stage1NaturalTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.data = o.tables(ORIGINAL)

    def prepare(self, folder):
        source = synthetic_entry()
        fixture = Path(folder) / "source.SAV"; fixture.write_bytes(encode(source))
        report = Path(folder) / "origin.json"; report.write_text("{}", encoding="utf-8")
        with patch.object(o, "validate_origin_report", return_value={"sha256": runner.sha(report), "actual_entry_story_pages": 25}):
            return module.prepare(ORIGINAL, fixture, "natural-stage1", report), fixture

    def test_preflight_complete_stage1_dynamic_fragments_and_unknown_source_retained(self):
        with tempfile.TemporaryDirectory() as folder:
            prepared, fixture = self.prepare(folder)
            self.assertEqual(prepared["encoded"], fixture.read_bytes())
            self.assertEqual(prepared["manifest"]["plaintext_patches"], [])
            self.assertEqual({f:sum(p["fragment"] == f for p in prepared["stories"])
                for f in (4, 6, 7, 8, 9, 10)}, {4:9, 6:1, 7:1, 8:19, 9:9, 10:1})
            self.assertEqual(len(prepared["stories"]),46)
            self.assertEqual(len(module.reader_story_pages(prepared["stories"])),42)
            self.assertEqual(len(prepared["entry_pages"]), 15)
            self.assertNotIn("expected_preparation", prepared)
            self.assertFalse(prepared["origin"]["inherited_runtime_as_B7_pass"])
            self.assertEqual(prepared["entry"]["camera"], [3,18])
            self.assertEqual(prepared["expected_acknowledgements"]["script13_acknowledgements"], 9)

    def test_actual_roster_not_random_prediction_but_normalization_and_unknown_bytes_strict(self):
        before = synthetic_post_turn3()
        state, masks, _ = o.settlement_projection(before, self.data)
        # 独立本端资源数可有不同战果；不是跨端共享RNG或全SAV比较。
        state["resource"] += 3000
        measured = o.write_stage2_slot(before, state, 0)
        actual, detail = o.check_natural_stage2_manual_save(before, measured, before, self.data)
        self.assertEqual(actual["roster"], state["roster"])
        self.assertEqual(detail["new_unit8_known_bytes"], 70)
        for at in (o.SLOT_BASE+0x28, o.SLOT_BASE+400+72, o.SLOT_BASE+480,
                o.SLOT_BASE+34, o.UNITS+40, o.META+9):
            bad = bytearray(measured); bad[at] ^= 1
            with self.subTest(at=at), self.assertRaises(AssertionError):
                o.check_natural_stage2_manual_save(before, o.checksum(bad), before, self.data)
        # 新构造只9B/cache未写，由同端实存继承，不能扩大到属性或旧actor。
        bad = bytearray(measured); bad[o.SLOT_BASE+400+0x28] = 153
        actual, _ = o.check_natural_stage2_manual_save(before, o.checksum(bad), before, self.data)
        self.assertEqual(actual["roster"][400+0x28], 153)

    def test_natural_max_turn_stop_does_not_reopen_or_claim_victory(self):
        with tempfile.TemporaryDirectory() as folder:
            prepared, save = self.prepare(folder)
            prepared.update(root=Path(folder), planner=Path(folder)/"planner.exe", max_turns=1)
            state = prepared["source"]; guard = [save.read_bytes(), save.stat().st_mtime_ns]
            capture = SimpleNamespace(artifacts=Path(folder), key=Mock(), wait=Mock())
            report = {"kind":"original", "checkpoints":[]}
            def checkpoint(cap, name, predicate, records, **kwargs):
                records.append({"name":name}); return None, {}
            reader = SimpleNamespace(victory_text_complete=False)
            plan = {"next_action":{"kind":"end_turn"}, "rest_tail":[]}
            with patch.object(module, "wait_title"), patch.object(module, "wait_state", side_effect=checkpoint), \
                    patch.object(module.natural, "read_plan", return_value=plan) as planning, \
                    patch.object(module, "Stage1ProgressReader", return_value=reader), \
                    patch.object(module.natural, "end_phase", return_value=state), \
                    patch.object(module.natural, "wait_progress", return_value=state), \
                    patch.object(module.natural, "save_active", return_value=state) as save_action, \
                    patch.object(runner, "finish_stage2") as finish:
                with self.assertRaisesRegex(module.natural.NaturalStopped, "max-turns"):
                    module.exercise(capture, prepared, save, guard, report)
            planning.assert_called_once_with(prepared["planner"], prepared["root"], save, stage=1)
            self.assertEqual(save_action.call_args.kwargs, {"stage":1})
            finish.assert_not_called()
            self.assertNotIn("business_sequence_completed", report)
            self.assertEqual(capture.key.call_count, 2)  # 标题Continue；没有重开或补输入。
            self.assertEqual((save.read_bytes(), save.stat().st_mtime_ns), tuple(guard))

    def death_reader(self):
        source=bytearray(synthetic_post_turn3())
        source[o.UNITS+4*80+7]=1  # 自然unit1，不把旧controlled unit51加入成长read-set。
        source=o.checksum(source);assets=runner.round_assets(ORIGINAL)
        pages=o.stage1_story_pages(ORIGINAL,assets,source)
        report={"resource_acknowledgements":[]}
        reader=module.Stage1ProgressReader(ORIGINAL,source,assets,report,stories=pages,victory_fragment=10)
        return source,assets,pages,report,reader

    def death_frame(self,page,assets):
        frame=page["text"].convert("RGB")
        frame.paste(assets["dialog_top"].convert("RGB"),page["border_position"])
        frame.paste(page["portraits"][0].convert("RGB"),page["portrait_position"])
        return frame

    def test_equivalent_death_page_has_one_ack_candidates_not_unique_raw_or_victory(self):
        source,assets,pages,report,reader=self.death_reader()
        page=next(p for p in pages if p["fragment"]==11)
        frame=self.death_frame(page,assets)
        old=module.natural.ProgressReader(ORIGINAL,source,assets,{"resource_acknowledgements":[]},
            stories=[p for p in pages if p["fragment"]<11],victory_fragment=10)
        self.assertIsNone(old.message(frame))
        full=module.natural.ProgressReader(ORIGINAL,source,assets,{"resource_acknowledgements":[]},stories=pages,victory_fragment=10)
        with self.assertRaisesRegex(module.natural.NaturalStopped,"多种提示"):full.message(frame)
        detail=reader.message(frame);self.assertIsNotNone(detail)
        self.assertEqual(detail["resource"]["fragment_candidates"],[11,13,15])
        self.assertFalse(detail["resource"]["death_caller_uniquely_observed"])
        self.assertTrue(detail["representative_fragment_is_resource_key_only"])
        capture=SimpleNamespace(key=Mock());reader.acknowledge(capture,detail,None)
        capture.key.assert_called_once_with("ENTER")
        proof=report["villager_death_acknowledgements"][0]
        self.assertEqual(proof["fragment_candidates"],[11,13,15])
        self.assertEqual(proof["raw_candidates"],[5,7,9])
        self.assertFalse(proof["death_caller_uniquely_observed"])
        self.assertFalse(reader.victory_text_complete)
        self.assertIsNone(reader.message(frame))  # 同页未消失，不误发第二ACK。
        self.assertIsNone(reader.message(Image.new("RGB",(320,200))))
        self.assertIsNotNone(reader.message(frame))  # 真正离页之后允许下一同族死亡消息。

    def test_death_wrong_family_unequal_resource_and_other_multiple_contract_still_rejected(self):
        source,assets,pages,report,reader=self.death_reader()
        male=next(p for p in pages if p["fragment"]==11)
        female=next(p for p in pages if p["fragment"]==12)
        wrong=self.death_frame(male,assets);wrong.paste(female["portraits"][0].convert("RGB"),male["portrait_position"])
        self.assertIsNone(reader.message(wrong))
        correct=reader.message(self.death_frame(female,assets));self.assertIsNotNone(correct)
        self.assertEqual(correct["page"]["death_fragment_candidates"],[12,14,16])
        changed=[dict(p) for p in pages]
        next(p for p in changed if p["fragment"]==13)["arrow_position"]=(10,10)
        with self.assertRaises(ValueError):module.reader_story_pages(changed)
        duplicate=dict(male);duplicate["fragment"]=5
        other=module.Stage1ProgressReader(ORIGINAL,source,assets,{"resource_acknowledgements":[]},
            stories=pages+[duplicate],victory_fragment=10)
        with self.assertRaisesRegex(module.natural.NaturalStopped,"多种提示"):
            other.message(self.death_frame(male,assets))

    def test_death_ack_count_does_not_replace_script13_or_settlement_counts(self):
        with tempfile.TemporaryDirectory() as folder:
            prepared,save=self.prepare(folder);prepared.update(root=Path(folder),planner=Path(folder)/"planner",max_turns=1)
            state=prepared["source"];guard=[save.read_bytes(),save.stat().st_mtime_ns]
            capture=SimpleNamespace(artifacts=Path(folder),key=Mock(),wait=Mock());report={"kind":"original","checkpoints":[]}
            def checkpoint(cap,name,predicate,rows,**kwargs):rows.append({"name":name});return None,{}
            def reader_ctor(original,source,assets,actual_report,**kwargs):
                actual_report["resource_acknowledgements"]=[{"signature":["story",4,i]} for i in range(9)]+[
                    {"signature":["story",6,0]}]+[{"signature":["story",8,i]} for i in range(19)]+[
                    {"signature":["story",9,i]} for i in range(9)]+[{"signature":["story",10,0]},
                    {"signature":["story",11,8],"resource_contract_kind":"villager_death_equivalent_page"}]
                return SimpleNamespace(victory_text_complete=True)
            with patch.object(module,"wait_title"),patch.object(module,"wait_state",side_effect=checkpoint), \
                    patch.object(module.natural,"read_plan",return_value={"next_action":{"kind":"end_turn"}}), \
                    patch.object(module,"Stage1ProgressReader",side_effect=reader_ctor), \
                    patch.object(module.natural,"end_phase",return_value=state),patch.object(module.natural,"wait_progress",return_value=None), \
                    patch.object(runner,"finish_stage2") as finish:
                module.exercise(capture,prepared,save,guard,report)
            self.assertEqual(len(report["script13_acknowledgements"]),9)
            self.assertEqual(len(report["settlement_acknowledgements"]),30)
            self.assertEqual(len(report["resource_acknowledgements"]),40)
            finish.assert_called_once()


if __name__ == "__main__": unittest.main()
