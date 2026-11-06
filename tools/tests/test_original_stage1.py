"""B5新增stage1模型／资源／协议回归；不运行游戏或WGC。"""
import copy
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
from types import SimpleNamespace
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT / "tools"))
import fd2_stage1_oracle as oracle
import run_original_stage1 as runner
from fd2_transition_oracle import derive_prepare_fixture, entry_geometry, entry_initialized_records
from run_original_end_turn import META, UNITS, STRIDE, round_assets
from run_original_overview import overview_templates, match_overview


class Stage1Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = ROOT / "original_game"
        encoded, before, state, _ = derive_prepare_fixture(cls.original)
        cls.tables = oracle.stage_tables(cls.original)
        geometry = entry_geometry(cls.original,state)
        records = entry_initialized_records(cls.original,state,geometry)
        synthetic = bytearray(before)
        synthetic[:1171] = cls.tables["metadata"]
        synthetic[0x8a3:0x12a3] = state["roster"]
        for index,(record,_) in enumerate(records): synthetic[UNITS+index*STRIDE:UNITS+(index+1)*STRIDE] = record
        synthetic[META:META+10] = bytes((1,21,1,9,13,19,14,10,1,5))
        synthetic[0x30a3:0x30c3] = bytes(32)
        cls.synthetic = oracle.checksum(synthetic)
        cls.encoded = bytes(a^b^c for a,b,c in zip(encoded,before,cls.synthetic))
        cls.synthetic_sha = hashlib.sha256(cls.encoded).hexdigest()
        cls.assets = round_assets(cls.original)

    def fixture(self, folder):
        path = Path(folder)/"synthetic.SAV"; path.write_bytes(self.encoded); return path

    def derive(self, scenario):
        with tempfile.TemporaryDirectory() as folder:
            return oracle.derive_fixture(self.fixture(folder),scenario,original=self.original)

    def test_fixture_mutates_only_exact_flags_and_checksum(self):
        for scenario,targets,bit in (("turn3",range(5,21),4),("villager-defeat",range(5,11),1)):
            with self.subTest(scenario=scenario):
                _,derived,manifest = self.derive(scenario)
                offsets={UNITS+raw*STRIDE+5 for raw in targets}
                self.assertEqual({r["offset"] for r in manifest["plaintext_patches"] if r["owner"]!="checksum"},offsets)
                for i,(before,after) in enumerate(zip(self.synthetic,derived)):
                    if i in offsets:self.assertEqual(after,before|bit)
                    elif i<len(derived)-4:self.assertEqual(after,before)
                self.assertFalse(manifest["natural_stage1_battle"])
                self.assertEqual(oracle.checksum(derived),derived)

    def test_resource_source_gate_rejects_wrong_known_record_not_unknown_heap(self):
        with tempfile.TemporaryDirectory() as folder:
            path=self.fixture(folder)
            changed=bytearray(self.synthetic);changed[1171]^=0x5a
            changed[UNITS+5*STRIDE+0x28]^=0x42
            changed=oracle.checksum(changed)
            path.write_bytes(bytes(a^b^c for a,b,c in zip(self.encoded,self.synthetic,changed)))
            _,_,manifest=oracle.derive_fixture(path,"turn3",original=self.original)
            self.assertFalse(manifest["source_validation"]["whole_source_sha_pinned"])
            changed=bytearray(self.synthetic);changed[UNITS+5*STRIDE+0x48]=0
            changed=oracle.checksum(changed)
            path.write_bytes(bytes(a^b^c for a,b,c in zip(self.encoded,self.synthetic,changed)))
            with self.assertRaisesRegex(AssertionError,"初始化byte"):
                oracle.derive_fixture(path,"turn3",original=self.original)

    def test_original_event_group_and_script13(self):
        self.assertEqual(list(self.tables["metadata"][3:6]),[3,6,1])
        self.assertEqual(self.tables["script13_positions"],[[15,3],[13,1],[14,2],[17,1],[16,2],[15,1]])
        self.assertEqual(len(self.tables["records"]),6)
        self.assertEqual(oracle.NEW_UNKNOWN,tuple(range(0x28,0x31)))
        for record in self.tables["records"]:
            self.assertEqual(record[7],96);self.assertEqual(record[0x21],3)
            self.assertEqual(record[10:14],bytes((0x40,0,0x40,128)))
            self.assertEqual(struct.unpack_from("<4H",record,0x40),(42,42,0,0))

    def test_complete_stage1_overview_and_stage0_unchanged_scope(self):
        expected=overview_templates(self.original,self.synthetic,self.assets["palette"])
        self.assertEqual(expected["fields"],{"stage":2,"turn":1,"resource":0,"side_counts_left_to_right":[10,5,6]})
        self.assertEqual([row["fragment"] for row in expected["conditions"]],[599,600])
        result=match_overview(expected["canvas"].convert("RGB"),expected)
        self.assertEqual(len(result["rectangles"]),4)
        bad=expected["canvas"].convert("RGB");bad.putpixel((80,61),(255,0,255))
        self.assertIsNone(match_overview(bad,expected))
        source=bytearray(self.synthetic);source[META+2]=0
        self.assertEqual([row["fragment"] for row in overview_templates(self.original,source,self.assets["palette"])["conditions"]],[597,598])
        source[META+2]=2
        with self.assertRaises(ValueError):overview_templates(self.original,source,self.assets["palette"])

    def test_turn3_pages_and_stage1_selection_preflight(self):
        _,source,_=self.derive("turn3")
        pages=oracle.turn3_pages(self.original,self.assets,source,self.tables)
        self.assertEqual(len(pages),9)
        self.assertTrue(all(page["fragment"]==4 and page["known_text_pixels"]>=256 for page in pages))
        _,source,_=self.derive("villager-defeat")
        probe=runner.selection_probe(self.original,source,self.assets,(27,21))
        self.assertEqual(probe["world"],(19,14))
        self.assertEqual(probe["actor_position"],(268,28))
        self.assertTrue(probe["ranges"])

    def measured(self):
        _,source,_=self.derive("turn3")
        measured=bytearray(source);measured[META:META+3]=bytes((4,27,1))
        for raw in range(5,11):measured[UNITS+raw*STRIDE+0x35:UNITS+raw*STRIDE+0x37]=bytes((26,15))
        for index,record in enumerate(self.tables["records"]): measured[UNITS+(21+index)*STRIDE:UNITS+(22+index)*STRIDE]=record
        return source,oracle.checksum(measured),{"camera":[9,13],"measuring_focus":[19,14]}

    def test_full_container_guard_and_only_exact_constructor_holes(self):
        source,measured,observation=self.measured()
        self.assertEqual(oracle.check_measurement(source,measured,self.tables,observation)["active_count"],27)
        for offset in (0,UNITS+7*STRIDE+0x28,0x8a3+31*STRIDE,0x312b+0xa00+15,UNITS+21*STRIDE+0x0d):
            with self.subTest(offset=offset):
                bad=bytearray(measured);bad[offset]^=1
                with self.assertRaises(AssertionError):oracle.check_measurement(source,oracle.checksum(bad),self.tables,observation)
        for offset in (2,*oracle.NEW_UNKNOWN):
            changed=bytearray(measured);changed[UNITS+21*STRIDE+offset]^=0x5a
            oracle.check_measurement(source,oracle.checksum(changed),self.tables,observation)

    def test_turn3_cannot_be_reported_as_legal_end_save(self):
        source,measured,observation=self.measured()
        bad=bytearray(measured);bad[META]=3
        with self.assertRaisesRegex(AssertionError,"turn4"):
            oracle.check_measurement(source,oracle.checksum(bad),self.tables,observation)

    def test_source_directory_rejected_before_mkdir_or_prepare(self):
        args=SimpleNamespace(root=ROOT,artifacts=self.original/"forbidden-new-output",save_fixture=Path("missing.SAV"),
            driver=Path("missing.exe"),dosbox=Path("missing.exe"),sdl=None,scenario="turn3")
        with patch.object(runner,"prepare") as prepare,patch.object(Path,"mkdir") as mkdir:
            with self.assertRaises(ValueError):runner.run(args)
        prepare.assert_not_called();mkdir.assert_not_called()

    def test_three_end_phase_order_repetition_and_extra_turn(self):
        lines=[]
        for turn in (1,2,3):lines.extend((f"field end turn: turn={turn} phase=2 remaining=5",
            f"field phase: turn={turn} phase=1 remaining=6",f"field phase: turn={turn} phase=0 remaining=10",
            f"field phase: turn={turn + 1} phase=2 remaining=5"))
        self.assertEqual(runner.check_phase_log("\n".join(lines))["exact_end_count"],3)
        for bad in (lines+lines[:1],list(reversed(lines)),lines+["field phase: turn=5 phase=2 remaining=5"],lines[:-1]):
            with self.subTest(bad=bad):
                with self.assertRaises(AssertionError):runner.check_phase_log("\n".join(bad))

    def test_mtime_failure_sticky_and_preserves_first_business_error(self):
        case={"probe_cases_passed":True,"status":"passed","error":"first business failure","processes":[]}
        with patch.object(runner,"endpoint_guard"),patch.object(Path,"stat",return_value=SimpleNamespace(st_mtime_ns=2)):
            runner.endpoint_checks(case,Path("source"),{}, {"FD2.EXE":1},Path("fixture"),b"",0,Path("save"),[b"",0])
        self.assertFalse(case["probe_cases_passed"]);self.assertEqual(case["status"],"failed")
        self.assertEqual(case["error"],"first business failure")

    def test_defeat_pair_requires_complete_frame_and_actual_title(self):
        cases=[{"kind":"original","probe_cases_passed":True,"defeat":{"game_over":{"frame":1,"full_frame_pixels":64000},"returned_title_before_cleanup":True}},
            {"kind":"sdl","probe_cases_passed":True,"defeat":{"final_composite":{"frame":1,"full_frame_pixels":64000},"returned_title_before_cleanup":True}}]
        prepared={"scenario":"villager-defeat"}
        self.assertTrue(runner.compare_cases(cases,Path("unused"),prepared)["both_complete_game_over_and_actual_title"])
        for field,value in (("frame",0),("full_frame_pixels",63999)):
            bad=copy.deepcopy(cases);bad[0]["defeat"]["game_over"][field]=value
            with self.assertRaises(AssertionError):runner.compare_cases(bad,Path("unused"),prepared)
        bad=copy.deepcopy(cases);bad[1]["defeat"]["returned_title_before_cleanup"]=False
        with self.assertRaises(AssertionError):runner.compare_cases(bad,Path("unused"),prepared)

    def test_original_cleanup_late_save_change_prevents_sdl_start(self):
        events=[]
        @contextmanager
        def running(command,cwd,env,log,driver,receipts):
            events.append("start-original")
            receipts.append({"cleaned":True,"forced_cleanup":False,"exit_code":0})
            yield SimpleNamespace(pid=123)
            save=cwd/"game/FD2.SAV";stamp=save.stat()
            os.utime(save,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+2_000_000_000))
            events.append("closed-original")
        def copytree(source,destination):destination.mkdir()
        with tempfile.TemporaryDirectory() as folder:
            folder=Path(folder);source=folder/"source";source.mkdir();fixture=self.fixture(source)
            args=SimpleNamespace(root=ROOT,artifacts=folder/"report",save_fixture=fixture,
                driver=ROOT/"README.md",dosbox=ROOT/"README.md",sdl=ROOT/"README.md",scenario="villager-defeat")
            with patch.object(runner,"hashes",return_value={"FD2.EXE":"stable"}),\
                    patch("run_original_reward.hashes",return_value={"FD2.EXE":"stable"}),\
                    patch.object(runner.shutil,"copytree",side_effect=copytree),patch.object(runner,"running",side_effect=running),\
                    patch.object(runner,"Capture",return_value=Mock()),patch.object(runner,"exercise"):
                report=runner.run(args)
            self.assertEqual(events,["start-original","closed-original"])
            self.assertEqual(len(report["cases"]),1)
            self.assertEqual(report["cases"][0]["status"],"failed")
            self.assertTrue(report["cases"][0]["original_unchanged"])
            self.assertFalse(report["cases"][0]["copy_save_matches_completed_state"])
            self.assertFalse(report["ok"] or report["probe_cases_passed"])
            self.assertFalse((args.artifacts/"sdl").exists())


if __name__=="__main__":unittest.main()
