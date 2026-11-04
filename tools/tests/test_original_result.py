"""受控结果门资源、精确派生字节、输入先后与自然退出；无窗口无游戏进程。"""
from contextlib import contextmanager
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import run_original_result as result

ROOT = Path(__file__).resolve().parents[2]


class ResultResourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = ROOT/"original_game"
        cls.fixture = cls.original/"START.SAV"
        cls.source,cls.focus = result.validate_initial(cls.fixture,0)
        cls.encrypted = cls.fixture.read_bytes()
        cls.assets = result.round_assets(cls.original)
        cls.templates = result.result_templates(cls.original,cls.assets)
        cls.actors = result.focus_cycle_assets(cls.original,cls.source,cls.assets)["actors"]

    def test_real_start_derivations_only_change_exact_flags_and_checksum(self):
        for outcome,raws in (("victory",range(4,12)),("defeat",[0])):
            encrypted,plain,manifest = result.derive_terminal(self.source,self.encrypted,outcome)
            expected = bytearray(self.source)
            for raw in raws:
                expected[result.UNITS+raw*result.STRIDE+5] |= 1
            expected[-4:] = sum(expected[:-4]).to_bytes(4,"little")
            self.assertEqual(plain,expected)
            self.assertEqual(manifest["controller_actor"],0 if outcome=="victory" else 1)
            self.assertFalse(manifest["hp_changed"])
            self.assertFalse(manifest["whole_battle_played"])
            self.assertEqual({c["offset"] for c in manifest["changed_plain_bytes"]},
                             {i for i,(a,b) in enumerate(zip(self.source,plain)) if a!=b})
            self.assertEqual({c["offset"] for c in manifest["changed_encrypted_bytes"]},
                             {i for i,(a,b) in enumerate(zip(self.encrypted,encrypted)) if a!=b})
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory)/"derived.SAV"
                path.write_bytes(encrypted)
                self.assertEqual(result.decode_save(path),plain)
        self.assertEqual(self.fixture.read_bytes(),self.encrypted)

    def test_victory_requires_fixed_first_page_glyphs_and_dialog_border(self):
        frame = Image.new("RGB",(320,200))
        text,border = self.templates["victory_text"],self.assets["dialog_top"]
        frame.paste(text.convert("RGB"),(94,119),text.point(lambda x:255 if x else 0,mode="L"))
        self.assertIsNone(result.result_branch(frame,"victory",self.assets,self.templates))
        frame.paste(border.convert("RGB"),(5,112),border.point(lambda x:255 if x else 0,mode="L"))
        self.assertIsNotNone(result.result_branch(frame,"victory",self.assets,self.templates))
        self.assertFalse(result.result_branch(frame,"victory",self.assets,self.templates)["settlement_continuation_verified"])
        bad = frame.copy()
        pixel = next((i%text.width,i//text.width) for i,v in enumerate(text.getdata()) if v)
        bad.putpixel((94+pixel[0],119+pixel[1]),(255,0,0))
        self.assertIsNone(result.result_branch(bad,"victory",self.assets,self.templates))
        self.assertIsNone(result.result_branch(frame,"defeat",self.assets,self.templates))

    def test_defeat_uses_two_complete_native_frames_not_a_generic_black_screen(self):
        for index,frame in enumerate(self.templates["defeat"]):
            detail = result.result_branch(frame,"defeat",self.assets,self.templates)
            self.assertEqual((detail["frame"],detail["full_frame_pixels"]),(index,64000))
            self.assertEqual(detail["game_over_label_verified"],index==1)
            self.assertFalse(detail["defeat_continuation_verified"])
            bad = frame.copy()
            bad.putpixel((0,0),(255,255,255))
            self.assertIsNone(result.result_branch(bad,"defeat",self.assets,self.templates))
        self.assertIsNone(result.result_branch(Image.new("RGB",(320,200)),"defeat",self.assets,self.templates))

    def test_selection_ack_requires_resource_lut_and_focused_actor_before_escape(self):
        for outcome in ("victory","defeat"):
            _,source,manifest = result.derive_terminal(self.source,self.encrypted,outcome)
            actor = self.actors[manifest["controller_actor"]]
            probe = result.selection_probe(self.original,source,actor["raw_actor"],self.assets)
            frame = Image.new("RGB",(320,200))
            template = actor["templates"][0]
            x,y = probe["actor_position"]
            frame.paste(template.convert("RGB"),(x,y-6),template.point(lambda v:255 if v else 0,mode="L"))
            frame.paste(probe["base"].convert("RGB"),probe["position"])
            self.assertIsNone(result.selection_state(frame,self.assets,actor,probe))
            frame.paste(probe["ranges"][0][1].convert("RGB"),probe["position"])
            self.assertIsNotNone(result.selection_state(frame,self.assets,actor,probe))


class ResultInputTest(unittest.TestCase):
    def test_each_original_step_is_acked_and_early_result_never_receives_extra_keys(self):
        titles = {3:[None,None,None]}
        for branch_at,expected in (("continued",["UP","ENTER"]),("focused",["UP","ENTER","Z"]),
                                   ("selected",["UP","ENTER","Z","ENTER"]),
                                   ("result",["UP","ENTER","Z","ENTER","ESC"])):
            capture = Mock()
            case = {"outcome":"victory","actor":{},"probe":{}}
            def wait(capture,name,predicate,checkpoints,**kwargs):
                return None,{"kind":"native_victory_first_page" if name.endswith(branch_at) else "ack"}
            with patch.object(result,"wait_title"),patch.object(result,"wait_state",side_effect=wait):
                found = result.exercise_original(capture,case,{}, {},titles,(76,100),[])
            self.assertEqual(found["branch"]["kind"],"native_victory_first_page")
            self.assertEqual([call.args[0] for call in capture.key.call_args_list],expected)

    def test_defeat_retains_first_frame_then_waits_for_game_over_without_extra_keys(self):
        for branch_at,expected in (("continued",["UP","ENTER"]),("focused",["UP","ENTER","Z"]),
                                   ("selected",["UP","ENTER","Z","ENTER"]),
                                   ("result",["UP","ENTER","Z","ENTER","ESC"])):
            for first_frame in (0,1):
                with self.subTest(branch_at=branch_at,first_frame=first_frame):
                    capture = Mock()
                    names = []
                    def wait(capture,name,predicate,checkpoints,**kwargs):
                        names.append(name)
                        if name.endswith("game-over"):
                            with patch.object(result,"result_branch",return_value={"kind":"native_defeat_resource","frame":0}):
                                self.assertIsNone(predicate(None))
                            with patch.object(result,"result_branch",return_value={"kind":"native_defeat_resource","frame":1}):
                                self.assertEqual(predicate(None)["frame"],1)
                            return None,{"kind":"native_defeat_resource","frame":1}
                        return None,({"kind":"native_defeat_resource","frame":first_frame}
                                     if name.endswith(branch_at) else {"kind":"ack"})
                    with patch.object(result,"wait_title"),patch.object(result,"wait_state",side_effect=wait):
                        found = result.exercise_original(capture,{"outcome":"defeat","actor":{},"probe":{}},
                                                         {},{},{3:[None]*3},(76,100),[])
                    self.assertEqual(found["branch"]["frame"],first_frame)
                    self.assertEqual(found["game_over"]["frame"],1)
                    self.assertEqual(names.count("defeat-original-game-over"),1 if first_frame==0 else 0)
                    self.assertEqual([call.args[0] for call in capture.key.call_args_list],expected)

    def test_defeat_game_over_timeout_cannot_send_continuation_or_recovery_keys(self):
        capture = Mock()
        def wait(capture,name,predicate,checkpoints,**kwargs):
            if name.endswith("game-over"):
                raise TimeoutError("GAME OVER resource missing")
            return None,({"kind":"native_defeat_resource","frame":0}
                         if name.endswith("selected") else {"kind":"ack"})
        with patch.object(result,"wait_title"),patch.object(result,"wait_state",side_effect=wait):
            with self.assertRaisesRegex(TimeoutError,"GAME OVER"):
                result.exercise_original(capture,{"outcome":"defeat","actor":{},"probe":{}},
                                         {},{},{3:[None]*3},(76,100),[])
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],["UP","ENTER","Z","ENTER"])

    def test_selection_failure_cannot_send_escape_or_recovery_keys(self):
        capture = Mock()
        def wait(capture,name,predicate,checkpoints,**kwargs):
            if name.endswith("selected"):
                raise TimeoutError("selection ROI missing")
            return None,{"kind":"ack"}
        with patch.object(result,"wait_title"),patch.object(result,"wait_state",side_effect=wait):
            with self.assertRaisesRegex(TimeoutError,"ROI"):
                result.exercise_original(capture,{"outcome":"defeat","actor":{},"probe":{}},
                                         {},{},{3:[None]*3},(76,100),[])
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],["UP","ENTER","Z","ENTER"])

    def test_sdl_natural_exit_requires_correct_result_once_reason_and_no_phase(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/"game.log"
            for outcome in ("victory","defeat"):
                reason = "stage 0 settlement not implemented" if outcome=="victory" else "stage 0 defeat continuation not implemented"
                good = f"field battle: {outcome}, stage=0\n{reason}\n"
                for code,text,passes in ((2,good,True),(0,good,False),(1,good,False),
                                         (2,good+good,False),(2,good+"field phase: turn=1 phase=1\n",False),
                                         (2,good.replace(outcome,"wrong"),False),(2,good.replace(reason,"missing"),False)):
                    log.write_text(text,encoding="utf-8")
                    capture = Mock()
                    capture.process.wait.return_value = code
                    with patch.object(result,"wait_title"):
                        if passes:
                            receipt = result.exercise_sdl(capture,outcome,{3:[None]*3},log,[])
                            self.assertTrue(receipt["natural_exit"] and receipt["observed_before_cleanup"])
                        else:
                            with self.assertRaises(AssertionError):
                                result.exercise_sdl(capture,outcome,{3:[None]*3},log,[])
                    self.assertEqual([call.args[0] for call in capture.key.call_args_list],["UP","ENTER"])


class ResultRunnerModelTest(unittest.TestCase):
    def test_original_first_both_fixtures_then_sdl_and_probe_never_claims_paired_pass(self):
        for probe in (False,True):
            with self.subTest(probe=probe),tempfile.TemporaryDirectory() as directory:
                base = Path(directory)
                binaries = [base/(name+".exe") for name in ("game","driver","dosbox")]
                for binary in binaries:
                    binary.write_bytes(b"not launched")
                args = SimpleNamespace(root=ROOT,artifacts=base/"output",game=binaries[0],
                                       driver=binaries[1],dosbox=binaries[2],save_fixture=ROOT/"original_game/START.SAV")
                calls = []
                @contextmanager
                def running(command,cwd,env,log,driver,receipts,*,expected_exit=0):
                    process = Mock(pid=100+len(receipts))
                    receipts.append({"pid":process.pid,"cleaned":True,"forced_cleanup":False,
                                     "exit_code":expected_exit,"expected_exit":expected_exit})
                    yield process
                def original(capture,case,*_):
                    calls.append(("original",case["outcome"]))
                    return {"trigger":"selection Esc","branch":{"kind":"native_"+case["outcome"]}}
                def sdl(capture,outcome,*_):
                    calls.append(("sdl",outcome))
                    return {"natural_exit":True,"observed_before_cleanup":True,"exit_code":2}
                with patch.object(result,"running",side_effect=running), \
                        patch.object(result,"Capture"),patch.object(result,"hashes",return_value={"source":"stable"}), \
                        patch.object(result.shutil,"copytree",side_effect=lambda src,dst:dst.mkdir()), \
                        patch.object(result,"exercise_original",side_effect=original), \
                        patch.object(result,"exercise_sdl",side_effect=sdl), \
                        patch.object(result.subprocess,"Popen",side_effect=AssertionError("no game processes in model")):
                    report = result.run(args,probe_original=probe)
                expected = [("original","victory"),("original","defeat")]
                if not probe:
                    expected += [("sdl","victory"),("sdl","defeat")]
                self.assertEqual(calls,expected)
                self.assertEqual(report["ok"],not probe)
                self.assertTrue(report["probe_cases_passed"])
                self.assertTrue(all(case["save_bytes_and_mtime_unchanged"] for case in report["cases"]))
                self.assertEqual(len(report["derived_fixtures"]),2)
                self.assertEqual(json.loads((args.artifacts/"comparison.json").read_text(encoding="utf-8"))["ok"],not probe)


if __name__ == "__main__":
    unittest.main()
