"""B7 central原版先行/分支终态/首错晚守卫模型；Popen/窗口全部mock。"""
from contextlib import contextmanager
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_stage1_complete as r
import fd2_stage1_branches as branches


class Stage1LifecycleTest(unittest.TestCase):
    def fixture(self, base):
        root = base / "project"; original = root / "original_game"; original.mkdir(parents=True)
        (original / "resource").write_bytes(b"readonly")
        evidence = base / "prior"; evidence.mkdir()
        source = evidence / "entry.SAV"; source.write_bytes(b"model fixture only")
        report = evidence / "endpoint.json"; report.write_text("{}", encoding="utf-8")
        bins = base / "bins"; bins.mkdir()
        for name in ("driver", "dosbox", "app"): (bins / name).write_bytes(name.encode())
        args = SimpleNamespace(root=root, driver=bins/"driver", dosbox=bins/"dosbox", sdl=bins/"app",
            scenario="villager-defeat", save_fixture=source, source_report=report, artifacts=base/"out")
        prepared = {"scenario":args.scenario,"encoded":source.read_bytes(),"manifest":{},
            "origin":{"model_only":True},"expected_acknowledgements":{},"required_snapshots":(),
            "result_capture_required":True}
        return args, prepared

    def model_run(self, args, prepared, action):
        calls = []
        @contextmanager
        def running(command, cwd, env, log, driver, processes):
            calls.append(command)
            processes.append({"pid":0,"exit_code":0,"cleaned":True,"forced_cleanup":False,"model_only":True})
            yield SimpleNamespace(pid=0)
        def capture(driver, process, work, inputs): return SimpleNamespace(artifacts=work)
        with patch.object(r,"prepare",return_value=prepared), patch.object(r,"owned_job_required"), \
                patch.object(r,"running",side_effect=running),patch.object(r,"Capture",side_effect=capture), \
                patch.object(r,"exercise",side_effect=action):
            result = r.run(args)
        return result, calls

    def complete_result(self, capture, prepared, save, guard, record):
        image = capture.artifacts / "result.png"; image.write_bytes(b"model artifact, not actual WGC")
        record["result_capture"] = r.fact(image); record["business_sequence_completed"] = True

    def test_defeat_contract_needs_real_result_fact_not_fake_completed_save(self):
        with tempfile.TemporaryDirectory() as folder:
            args, prepared = self.fixture(Path(folder))
            with patch.object(branches,"prepare",return_value=prepared) as delegated:
                self.assertIs(r.prepare(args.root/"original_game",args.save_fixture,args.scenario,args.source_report),prepared)
            delegated.assert_called_once()
            result, calls = self.model_run(args, prepared, self.complete_result)
            self.assertTrue(result["ok"]); self.assertEqual(len(calls),2)
            self.assertTrue(all(case["guards"]["snapshot_files_unchanged"] for case in result["cases"]))
            self.assertNotIn("completed_save",result["cases"][0])
            image = Path(result["cases"][0]["result_capture"]["path"]); image.write_bytes(b"bad")
            self.assertFalse(r.snapshots_unchanged(result["cases"][0]))

    def test_original_first_error_preserved_and_no_sdl_after_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            args, prepared = self.fixture(Path(folder))
            result, calls = self.model_run(args,prepared,Mock(side_effect=RuntimeError("primary resource failed")))
            self.assertFalse(result["ok"]); self.assertEqual(len(calls),1)
            self.assertEqual(result["error"],"primary resource failed")
            self.assertTrue(result["cases"][0]["guards"]["owned_cleanup"])

    def test_late_binary_drift_sticky_blocks_sdl_and_does_not_override_cleanup(self):
        with tempfile.TemporaryDirectory() as folder:
            args, prepared = self.fixture(Path(folder))
            def action(*params):
                self.complete_result(*params); args.driver.write_bytes(b"changed after actual body")
            result, calls = self.model_run(args,prepared,action)
            self.assertFalse(result["ok"]); self.assertEqual(len(calls),1)
            self.assertFalse(result["cases"][0]["guards"]["tools_and_binaries_unchanged"])
            self.assertTrue(result["cases"][0]["guards"]["owned_cleanup"])

    def test_every_early_natural_turn_save_and_plan_hash_is_required(self):
        with tempfile.TemporaryDirectory() as folder:
            base=Path(folder);case={"required_snapshots":(),"actual_turn_saves":[],"natural_actions":[]}
            for turn in (2,3):
                save=base/f"turn-{turn:03d}.SAV";save.write_bytes(f"actual model save {turn}".encode())
                plan=base/f"turn-{turn:03d}-plan.json";plan.write_text(json.dumps({"turn":turn}),encoding="utf-8")
                case["actual_turn_saves"].append({"snapshot":str(save),"sha256":r.sha(save)})
                case["natural_actions"].append({"plan":str(plan),"sha256":r.sha(plan)})
            self.assertTrue(r.snapshots_unchanged(case))
            for row,key in ((case["actual_turn_saves"][0],"snapshot"),(case["natural_actions"][0],"plan")):
                target=Path(row[key]);before=target.read_bytes();target.write_bytes(before+b"tampered")
                self.assertFalse(r.snapshots_unchanged(case));target.write_bytes(before)
                self.assertTrue(r.snapshots_unchanged(case))
            del case["natural_actions"][0]["sha256"]
            self.assertFalse(r.snapshots_unchanged(case))

    def test_original_early_turn_save_or_plan_drift_prevents_sdl(self):
        for field,key in (("actual_turn_saves","snapshot"),("natural_actions","plan")):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as folder:
                args,prepared=self.fixture(Path(folder))
                def action(capture,p,save,guard,record):
                    self.complete_result(capture,p,save,guard,record)
                    path=capture.artifacts/("turn-002.SAV" if field=="actual_turn_saves" else "turn-002-plan.json")
                    path.write_bytes(b"original model evidence")
                    record[field]=[{key:str(path),"sha256":r.sha(path)}]
                    path.write_bytes(b"changed after earlier round")
                result,calls=self.model_run(args,prepared,action)
                self.assertFalse(result["ok"]);self.assertEqual(len(calls),1)
                self.assertFalse(result["cases"][0]["guards"]["snapshot_files_unchanged"])
                self.assertTrue(result["cases"][0]["guards"]["owned_cleanup"])

    def test_png_close_seal_checked_late_but_not_claimed_creation_time_or_bmp(self):
        with tempfile.TemporaryDirectory() as folder:
            base=Path(folder);png=base/"checkpoint.png";bmp=base/"window-current.bmp"
            png.write_bytes(b"model png");bmp.write_bytes(b"mutable current")
            case={"required_snapshots":(),"artifacts":str(base),
                "capture_hashes":{png.name:r.sha(png),bmp.name:r.sha(bmp)}}
            self.assertTrue(r.snapshots_unchanged(case))
            bmp.write_bytes(b"changed current BMP")
            self.assertTrue(r.snapshots_unchanged(case))
            png.write_bytes(b"changed sealed PNG")
            self.assertFalse(r.snapshots_unchanged(case))

    def test_registered_source_page_unchanged_positive_and_late_drift_blocks_sdl(self):
        for changed in (False,True):
            with self.subTest(changed=changed),tempfile.TemporaryDirectory() as folder:
                args,prepared=self.fixture(Path(folder))
                page=args.source_report.parent/"entry-00.png";page.write_bytes(b"previous native model page")
                prepared["origin"]["previous_native_entry_provenance"]={"entry_captures":[{"path":str(page),"sha256":r.sha(page)}]}
                def action(*params):
                    self.complete_result(*params)
                    if changed:page.write_bytes(b"source page changed after preflight")
                result,calls=self.model_run(args,prepared,action)
                self.assertEqual(result["ok"],not changed)
                self.assertEqual(len(calls),1 if changed else 2)
                self.assertEqual(result["cases"][0]["guards"]["source_fixture_and_report_unchanged"],not changed)
                self.assertEqual(len(result["source_evidence_files"]),3)

    def test_registered_source_page_hash_mismatch_rejects_before_output(self):
        with tempfile.TemporaryDirectory() as folder:
            args,prepared=self.fixture(Path(folder))
            page=args.source_report.parent/"entry-00.png";page.write_bytes(b"previous native model page")
            prepared["origin"]["previous_native_entry_provenance"]={"entry_captures":[{"path":str(page),"sha256":"0"*64}]}
            with self.assertRaisesRegex(ValueError,"PNG"):
                self.model_run(args,prepared,self.complete_result)
            self.assertFalse(args.artifacts.exists())


if __name__ == "__main__": unittest.main()
