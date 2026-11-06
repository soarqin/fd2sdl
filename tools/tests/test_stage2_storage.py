"""stage2初始存取原资源与完整容器模型；不启动游戏、驱动或窗口。"""
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_stage1_complete_oracle as o
import fd2_stage2_storage as s
import run_original_stage1_complete as r
from run_original_overview import overview_templates,match_overview,RECTANGLES
from test_stage1_complete_oracle import ORIGINAL,synthetic_post_turn3
from test_stage1_complete_runner import encode,active_fixture


class Stage2StorageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.data = o.tables(ORIGINAL)

    def model_source(self,folder):
        before=synthetic_post_turn3();state,_,_=o.settlement_projection(before,self.data)
        manual=o.write_stage2_slot(before,state,0);projection=o.stage2_projection(state,self.data)
        plain=active_fixture(manual,state,projection,self.data)
        source=Path(folder)/"stage2.SAV";source.write_bytes(encode(plain))
        pages=o.stage2_pages(ORIGINAL,r.round_assets(ORIGINAL),state,self.data)
        report={"kind":"original","status":"passed","business_sequence_completed":True,"model_only":True,
            "guards":{"all_model_guards":True},"processes":[{"exit_code":0,"cleaned":True,"forced_cleanup":False,"pid":0}],
            "completed_save":{"snapshot":str(source),"sha256":r.sha(source)},
            "stage2_entry_acknowledgements":[],"checkpoints":[],"capture_hashes":{}}
        for i,page in enumerate(pages):
            # 文件只是模型token证据，不伪称此报告是实际WGC或运行来源。
            name=f"b7-entry-{i:02d}";image=Path(folder)/(name+".png");image.write_bytes(f"model page {i}".encode())
            report["capture_hashes"][image.name]=r.sha(image)
            report["stage2_entry_acknowledgements"].append({"fragment":page["fragment"],"offset":page["token_offset"],"portrait":page["portrait_id"]})
            report["checkpoints"].append({"name":name,"fragment":page["fragment"],"token_offset":page["token_offset"],
                "portrait_id":page["portrait_id"],"text":{"ok":True},"border":{"ok":True}})
        path=Path(folder)/"endpoint.json";path.write_text(json.dumps(report),encoding="utf-8")
        return source,path,plain,state,report

    def test_same_source_identity_full15_known_records_and_provenance_bound(self):
        with tempfile.TemporaryDirectory() as folder:
            source,path,plain,state,report=self.model_source(folder)
            prepared=s.prepare(ORIGINAL,source,"stage2-storage",path)
            self.assertEqual(prepared["encoded"],source.read_bytes())
            self.assertTrue(prepared["exercise_storage"])
            self.assertEqual(prepared["expected_acknowledgements"],{"stage2_entry_acknowledgements":15})
            for mutation in ("guard","ack","hash","business"):
                changed=json.loads(json.dumps(report))
                if mutation=="guard":changed["guards"]["all_model_guards"]=False
                elif mutation=="ack":changed["stage2_entry_acknowledgements"].pop()
                elif mutation=="hash":changed["capture_hashes"]["b7-entry-00.png"]="0"*64
                else:changed["business_sequence_completed"]=False
                path.write_text(json.dumps(changed),encoding="utf-8")
                with self.subTest(mutation=mutation),self.assertRaises(ValueError):s.prepare(ORIGINAL,source,"stage2-storage",path)

    def test_overview_stage2_full_rectangles_original_conditions601602_and_no_other_stage(self):
        with tempfile.TemporaryDirectory() as folder:
            _,_,plain,_,_=self.model_source(folder)
            assets=r.round_assets(ORIGINAL);templates=overview_templates(ORIGINAL,plain,assets["palette"])
            self.assertEqual(templates["fields"],{"stage":3,"turn":1,"resource":0,"side_counts_left_to_right":[8,6,1]})
            self.assertEqual([row["fragment"] for row in templates["conditions"]],[601,602])
            frame=templates["canvas"].convert("RGB");self.assertIsNotNone(match_overview(frame,templates))
            for _,x,y,w,h in RECTANGLES:
                bad=frame.copy();pixel=bad.getpixel((x,y));bad.putpixel((x,y),tuple(255-value for value in pixel))
                self.assertIsNone(match_overview(bad,templates))
            for stage in (0,1):
                old=bytearray(plain);old[o.META+2]=stage
                self.assertEqual(overview_templates(ORIGINAL,old,assets["palette"])["fields"]["stage"],stage+1)
            bad=bytearray(plain);bad[o.META+2]=3
            with self.assertRaises(ValueError):overview_templates(ORIGINAL,bad,assets["palette"])

    def test_four_manual_slots_load_and_active_resave_protocol_no_gameplay_or_extra_scene(self):
        with tempfile.TemporaryDirectory() as folder:
            source,path,plain,state,_=self.model_source(folder)
            prepared=s.prepare(ORIGINAL,source,"stage2-storage",path)
            outputs=[];current=plain
            for slot in range(4):
                current=o.write_stage2_slot(current,state,slot);outputs.append(current)
            active=active_fixture(current,state,prepared["entry"],self.data);outputs.extend((active,active))
            pending=iter(outputs);save=Path(folder)/"working.SAV";save.write_bytes(source.read_bytes())
            guard=[save.read_bytes(),save.stat().st_mtime_ns]
            events=[];record={"kind":"original","checkpoints":[]}
            capture=SimpleNamespace(artifacts=Path(folder),key=lambda key:events.append(("key",key)),wait=lambda *_:None)
            def checkpoint(cap,name,predicate,rows,**kwargs):
                events.append(("ack",name));rows.append({"name":name});return None,{"kind":"model only"}
            def measured(target,*_):
                result=next(pending);target.write_bytes(encode(result));return result
            with patch.object(s,"wait_title"),patch.object(s,"wait_state",side_effect=checkpoint), \
                    patch.object(r,"wait_state",side_effect=checkpoint),patch.object(r,"wait_measured_save",side_effect=measured), \
                    patch.object(s,"wait_measured_save",side_effect=measured):
                s.exercise(capture,prepared,save,guard,record)
            self.assertEqual([row["slot"] for row in record["manual_saves"]],[0,1,2,3])
            self.assertEqual(len(record["active_saves"]),2)
            self.assertEqual(record["manual_load"]["slot"],3)
            self.assertEqual(record["active_load"]["restored_focus"],[6,22])
            self.assertEqual(len(record["stage2_entry_acknowledgements"]),15)
            self.assertTrue(record["business_sequence_completed"]);self.assertTrue(r.snapshots_unchanged(record))
            self.assertIn(("ack","b7-stage2-hero-detail"),events)
            self.assertIn(("ack","b7-stage2-overview"),events)
            self.assertIn(("ack","b7-stage2-load-restored-focus"),events)
            self.assertFalse(any("end-" in name or "settlement-" in name for kind,name in events if kind=="ack"))


if __name__=="__main__":unittest.main()
