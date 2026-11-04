"""自然START资源、逐wait输入和原版先行配对；不启动进程或窗口。"""
from pathlib import Path
from contextlib import contextmanager
import copy
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import run_original_opening as probe
import fd2_opening_oracle as oracle


class PageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assets = probe.round_assets(ROOT/"original_game")
        cls.pages = probe.build_pages(ROOT/"original_game",cls.assets)

    def frame(self,page):
        frame = Image.new("RGB",(320,200),(17,19,23))
        text = page["text"]
        frame.paste(text.convert("RGB"),(0,0),text.point(lambda c:255 if c else 0,mode="L"))
        frame.paste(self.assets["dialog_top"].convert("RGB"),page["border_position"])
        frame.paste(page["portraits"][0].convert("RGB"),page["portrait_position"])
        if page["arrow_position"] is not None:
            frame.paste(page["arrows"][0].convert("RGB"),page["arrow_position"])
        return frame

    def test_all117_have_complete_distinct_wait_gates_and_only20_require_arrows(self):
        self.assertEqual(len(self.pages),117)
        self.assertEqual(sum(p["arrow_position"] is not None for p in self.pages),20)
        for stage,count in ((32,56),(31,39),(0,22)):
            self.assertEqual(sum(p["stage"]==stage for p in self.pages),count)
        for page in self.pages:
            frame = self.frame(page)
            detail = probe.page_match(frame,page,self.assets)
            self.assertIsNotNone(detail)
            self.assertEqual(detail["text"]["checked_pixels"],page["known_text_pixels"])
            self.assertGreaterEqual(detail["known_text_pixels"],1024)
            point = next((i%320,i//320) for i,c in enumerate(page["text"].getdata()) if c)
            frame.putpixel(point,(255,0,0))
            self.assertIsNone(probe.page_match(frame,page,self.assets))

    def test_arrow_and_full_portrait_are_required_not_only_text(self):
        page = next(p for p in self.pages if p["arrow_position"] is not None)
        frame = self.frame(page)
        x,y = page["arrow_position"]
        frame.paste((255,0,0),(x,y,x+14,y+10))
        self.assertIsNone(probe.page_match(frame,page,self.assets))
        for page in self.pages[::8]:
            frame = self.frame(page)
            x,y = page["portrait_position"]
            frame.putpixel((x,y),(255,0,0))
            self.assertIsNone(probe.page_match(frame,page,self.assets))


class InputTest(unittest.TestCase):
    def test_each_wait_is_acked_before_its_single_key_and_then_ui_save(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            save = work/"FD2.SAV"
            save.write_bytes(b"unchanged-until-explicit-save")
            capture = Mock(artifacts=work)
            pages = [{"stage":32,"fragment":i,"text":Mock()} for i in range(117)]
            names = []
            def wait(capture,name,predicate,checkpoints,**kwargs):
                names.append(name)
                if name.startswith("wait-"):
                    self.assertEqual(capture.key.call_count,len(names))
                return None,{"focus_screen":[172,28],"inferred_camera":[0,13]}
            report = {"checkpoints":[],"title_menu_count":3,"load_available":True}
            with patch.object(probe,"wait_title"),patch.object(probe,"wait_state",side_effect=wait), \
                    patch.object(probe,"require_source_unchanged") as guard, \
                    patch.object(probe,"decode_save",return_value=b"before"), \
                    patch.object(probe,"wait_measured_save",return_value=b"after"), \
                    patch.object(probe,"measurement",return_value={"checked":True}):
                probe.exercise(capture,{},pages,{3:[None]*3},save,b"source",123,{},report)
            keys = [call.args[0] for call in capture.key.call_args_list]
            self.assertEqual(keys[:118],["ENTER"]*118)
            self.assertEqual(keys[118:],["LEFT","ENTER","UP","ENTER","LEFT","ENTER","ENTER"])
            self.assertEqual(names[-1],"measurement-saved-browse")
            self.assertEqual(guard.call_count,124)

    def test_wait_timeout_never_sends_retry_skip_or_measurement_keys(self):
        capture = Mock(artifacts=Path("unused"))
        pages = [{"stage":32,"fragment":0,"text":Mock()} for _ in range(117)]
        def wait(*args,**kwargs):
            raise TimeoutError("native resource gate absent")
        with patch.object(probe,"wait_title"),patch.object(probe,"wait_state",side_effect=wait):
            with self.assertRaisesRegex(TimeoutError,"resource"):
                probe.exercise(capture,{},pages,{3:[None]*3},None,None,None,{},
                               {"checkpoints":[],"title_menu_count":3,"load_available":True})
        self.assertEqual([call.args[0] for call in capture.key.call_args_list],["ENTER"])

    def test_unsafe_measuring_cell_stops_before_left_or_menu_keys(self):
        for point in ([28,28],[172,4],[172,172]):
            capture = Mock(artifacts=Path("unused"))
            with patch.object(probe,"wait_title"),patch.object(probe,"require_source_unchanged"), \
                    patch.object(probe,"wait_state",return_value=(None,{"focus_screen":point})), \
                    self.assertRaisesRegex(ValueError,"without scrolling"):
                probe.exercise(capture,{},[],{3:[None]*3},None,None,None,{},
                               {"checkpoints":[],"title_menu_count":3,"load_available":True})
            self.assertEqual([call.args[0] for call in capture.key.call_args_list],["ENTER"])


class TitleMenuTest(unittest.TestCase):
    def test_stage29_uses_native3_sdl2_and_stage0_uses3_each_with_full_start_template(self):
        for stage,counts in ((29,{"original":3,"sdl":2}),(0,{"original":3,"sdl":3}),
                             (0xff,{"original":2,"sdl":2})):
            source = bytearray(probe.decode_save(ROOT/"original_game/START.SAV"))
            source[oracle.META+2] = stage
            source[-4:] = sum(source[:-4]).to_bytes(4,"little")
            self.assertEqual(probe.title_menu_counts(source),counts)
            for kind,count in counts.items():
                capture = Mock(artifacts=Path("unused"))
                titles = {2:[object()],3:[object()]}
                with patch.object(probe,"wait_title") as title, \
                        patch.object(probe,"wait_state",side_effect=TimeoutError("first wait")), \
                        self.assertRaises(TimeoutError):
                    probe.exercise(capture,{},[{"stage":32,"fragment":0,"text":Mock()}],titles,
                                   None,None,None,{}, {"checkpoints":[],"title_menu_count":count,"load_available":True})
                title.assert_called_once_with(capture,titles[count][0])
                self.assertEqual([call.args[0] for call in capture.key.call_args_list],["ENTER"])

    def test_invalid_active_or_roster_count_is_rejected_before_launch(self):
        source = probe.decode_save(ROOT/"original_game/START.SAV")
        for offset,value in ((oracle.META+1,97),(oracle.META+9,33)):
            bad = bytearray(source)
            bad[offset] = value
            with self.assertRaises((ValueError,AssertionError)):
                probe.title_menu_counts(bad)
            with self.assertRaises((ValueError,AssertionError)):
                probe.load_menu_availability(bad)

    def test_load_gate_is_source_bounded_and_native_ff_file_stays_available(self):
        start = probe.decode_save(ROOT/"original_game/START.SAV")
        for stage,expected in ((29,{"original":True,"sdl":False}),
                               (0,{"original":True,"sdl":True}),
                               (0xff,{"original":True,"sdl":False})):
            source = bytearray(start)
            source[oracle.META+2] = stage
            source[-4:] = sum(source[:-4]).to_bytes(4,"little")
            self.assertEqual(probe.load_menu_availability(source),expected)
        # stage0并不足以通过import；不为坐标/资源/shape另写一套codec或假称可恢复。
        for address,value in ((oracle.UNITS,255),(oracle.UNITS+7,140),(0,29),
                              (oracle.META+3,99),(oracle.META+1,65)):
            bad = bytearray(start)
            bad[address] = value
            bad[-4:] = sum(bad[:-4]).to_bytes(4,"little")
            with self.assertRaisesRegex(ValueError,"工具范围限制"):
                probe.load_menu_availability(bad)


class SecondaryLoadGateTest(unittest.TestCase):
    def test_disabled_load_matches_all_opaque_pixels_and_rejects_normal_or_disabled_selection(self):
        from run_original_battle_menu import MENU_OFFSETS
        assets = probe.round_assets(ROOT/"original_game")
        focus = (148,28)
        menu = probe.RoundOracle(assets,focus)
        def frame(load_frame,selected):
            image = Image.new("RGB",(320,200))
            for item,command in enumerate((12,13,14,15)):
                index = load_frame if item==2 else command*3+(item==selected)
                at = tuple(a+b for a,b in zip(focus,MENU_OFFSETS[item]))
                image.paste(assets["commands"][index].convert("RGB"),at)
            return image
        for selected in (0,1,None):
            actual = frame(44,selected)
            detail = menu.secondary(actual,selected,load_available=False)
            self.assertIsNotNone(detail)
            self.assertEqual(detail["matches"][2]["frame"],44)
            self.assertEqual(detail["matches"][2]["checked_pixels"],476)
            self.assertIsNone(menu.secondary(actual,selected))
            self.assertIsNone(menu.secondary(frame(42,selected),selected,load_available=False))
            damaged = actual.copy()
            damaged.putpixel((173,31),(255,0,0))
            self.assertIsNone(menu.secondary(damaged,selected,load_available=False))
        self.assertIsNone(menu.secondary(frame(44,2),2,load_available=False))


class MeasurementTest(unittest.TestCase):
    def test_geometry_hidden_and_single_measuring_left_are_required_without_mutation(self):
        source = probe.decode_save(ROOT/"original_game/START.SAV")
        measured = bytearray(source)
        measured[oracle.META+3:oracle.META+9] = bytes((0,13,6,14,6,1))
        frozen = bytes(measured)
        first = {"inferred_camera":[0,13],"focus_screen":[172,28]}
        expected = oracle.geometry(ROOT/"original_game")
        detail = probe.measurement(measured,source,first,expected)
        self.assertEqual(detail["natural_first_focus_world"],[7,14])
        self.assertEqual(detail["measuring_navigation"],["LEFT"])
        self.assertEqual(bytes(measured),frozen)
        for address,value in ((oracle.UNITS+3,2),(oracle.UNITS+9*oracle.STRIDE+5,0),
                              (oracle.UNITS+7,9),(oracle.META+5,7),(0x312b,source[0x312b]^1)):
            bad = bytearray(measured)
            bad[address] = value
            with self.assertRaises(AssertionError):
                probe.measurement(bad,source,first,expected)

    def test_all_paired_fields_must_match_and_unknown_bytes_are_not_claimed(self):
        source = probe.decode_save(ROOT/"original_game/START.SAV")
        measured = bytearray(source)
        measured[oracle.META+3:oracle.META+9] = bytes((0,13,6,14,6,1))
        expected = oracle.geometry(ROOT/"original_game")
        first = {"inferred_camera":[0,13],"focus_screen":[172,28]}
        summary = oracle.measurement(measured,source,first,expected)
        compared = oracle.compare_measurements(summary,copy.deepcopy(summary))
        self.assertTrue(compared["ok"])
        for field in compared["compared_fields"]:
            bad = copy.deepcopy(summary)
            bad[field] = None
            with self.subTest(field=field),self.assertRaisesRegex(AssertionError,field):
                oracle.compare_measurements(summary,bad)
        self.assertIn("unknown",compared["excluded"])


class RunnerModelTest(unittest.TestCase):
    def test_original_then_sdl_isolated_saves_and_failed_original_never_starts_sdl(self):
        for failure in (False,True):
            with self.subTest(failure=failure),tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                binaries = [work/(name+".exe") for name in ("game","driver","dosbox")]
                for binary in binaries:
                    binary.write_bytes(b"not launched")
                args = SimpleNamespace(root=ROOT,artifacts=work/"output",game=binaries[0],
                                       driver=binaries[1],dosbox=binaries[2])
                calls = []
                @contextmanager
                def running(command,cwd,env,log,driver,receipts):
                    receipts.append({"cleaned":True,"forced_cleanup":False,"exit_code":0})
                    calls.append((command,cwd,env.copy()))
                    yield Mock(pid=100+len(receipts))
                def copied(source,destination):
                    destination.mkdir()
                    (destination/"FD2.SAV").write_bytes((source/"FD2.SAV").read_bytes())
                def exercise(capture,assets,pages,titles,save,source,stamp,expected,record):
                    if failure:
                        raise TimeoutError("resource gate missing")
                    record["measurement"] = {"kind":record["kind"]}
                with patch.object(probe,"round_assets"),patch.object(probe,"build_pages",return_value=[]), \
                        patch.object(probe,"geometry",return_value={"actors":[]}), \
                        patch.object(probe,"title_templates"),patch.object(probe,"hashes",return_value={"original":"stable"}), \
                        patch.object(probe,"running",side_effect=running),patch.object(probe,"Capture"), \
                        patch.object(probe.shutil,"copytree",side_effect=copied), \
                        patch.object(probe,"exercise",side_effect=exercise), \
                        patch.object(probe,"compare_measurements",return_value={"ok":True}) as compare:
                    report = probe.run(args)
                self.assertEqual([case["kind"] for case in report["cases"]],
                                 ["original"] if failure else ["original","sdl"])
                self.assertEqual(report["ok"],not failure)
                self.assertFalse(report["probe_only"])
                self.assertTrue(report["original_unchanged"])
                self.assertEqual([case["title_menu_count"] for case in report["cases"]],
                                 [3] if failure else [3,2])
                self.assertEqual([case["load_available"] for case in report["cases"]],
                                 [True] if failure else [True,False])
                self.assertEqual(calls[0][0][0],str(args.dosbox.resolve()))
                self.assertEqual(calls[0][1],args.artifacts/"original")
                self.assertTrue(all(receipt["cleaned"] for receipt in report["processes"]))
                if failure:
                    compare.assert_not_called()
                    self.assertIn("resource gate missing",report["error"])
                else:
                    compare.assert_called_once()
                    self.assertEqual(calls[1][0],[str(args.game.resolve())])
                    self.assertEqual(calls[1][1],ROOT)
                    self.assertEqual(calls[1][2]["FD2SDL_SAVE_PATH"],str(args.artifacts/"sdl/FD2.SAV"))
                saved = json.loads((args.artifacts/"comparison.json").read_text(encoding="utf-8"))
                self.assertEqual(saved["ok"],not failure)


if __name__=="__main__":
    unittest.main()
