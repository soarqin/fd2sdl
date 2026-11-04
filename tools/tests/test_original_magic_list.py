"""原版MAGIC只读probe纯回归；没有窗口或游戏进程。"""
from contextlib import contextmanager
from pathlib import Path
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
import run_original_magic_list as probe
from run_original_battle_menu import MENU_OFFSETS


class ResourceFixture:
    @classmethod
    def setUpClass(cls):
        (cls.source,cls.focus,cls.assets,cls.actors,cls.selection,cls.templates,
         cls.fields) = probe.prepare(probe.ROOT/"original_game")

    def list_frame(self,unselected=False):
        image = Image.new("RGB",(320,200))
        for name,(at,part) in self.templates.items():
            if name=="unselected" or (unselected and name=="selected"):
                continue
            image.paste(part.convert("RGB"),at)
        if unselected:
            at,part = self.templates["unselected"]
            image.paste(part.convert("RGB"),at)
        return image

    def command_frame(self,*,highlight=True,actor_overlay=True):
        image = Image.new("RGB",(320,200))
        position = (124,100)
        for item,index in enumerate((2,4 if highlight else 3,6,9)):
            part = self.assets["upper_commands"][index][0] if item==0 and actor_overlay else self.assets["commands"][index]
            image.paste(part.convert("RGB"),tuple(a+b for a,b in zip(position,MENU_OFFSETS[item])))
        return image


class ResourceTest(ResourceFixture,unittest.TestCase):
    def test_complete_list_name_mp_cost_and_selection_not_black_or_unselected(self):
        frame = self.list_frame()
        detail = probe.list_page(frame,self.templates,self.fields)
        self.assertEqual(detail["checked_pixels"],310*99+33*17+44*8)
        self.assertEqual((detail["current_mp"],detail["max_mp"],detail["cost"]),(8,8,2))
        self.assertIsNone(probe.list_page(self.list_frame(True),self.templates,self.fields))
        self.assertIsNone(probe.list_page(Image.new("RGB",(320,200)),self.templates,self.fields))
        for point in ((5,94),(98,13),(267,59)):
            bad = frame.copy()
            bad.putpixel(point,(255,0,0))
            self.assertIsNone(probe.list_page(bad,self.templates,self.fields))

    def test_command_checks_full_upper_actor_overlay_and_selected_magic(self):
        command = probe.PlayerOracle(self.source,self.assets,[],{})
        self.assertIsNotNone(probe.command_page(self.command_frame(),command,(124,100),self.templates,1))
        self.assertIsNone(probe.command_page(self.command_frame(actor_overlay=False),command,(124,100),self.templates,1))
        self.assertIsNone(probe.command_page(self.command_frame(highlight=False),command,(124,100),self.templates,1))
        detail = probe.command_page(self.command_frame(),command,(124,100),self.templates,1,after_cancel=True)
        self.assertTrue(detail["list_resource_absent"])
        self.assertEqual([m["frame"] for m in detail["matches"]],[2,4,6,9])

    def test_reopened_all_three_rectangles_must_equal_first(self):
        first = self.list_frame()
        detail = probe.compare_reopened(first,first.copy(),self.templates)
        self.assertTrue(detail["name_mp_cost_selection_unchanged"])
        for point in ((5,94),(98,13),(267,59)):
            second = first.copy()
            second.putpixel(point,(255,0,0))
            with self.assertRaises(AssertionError):
                probe.compare_reopened(first,second,self.templates)


class InputTest(ResourceFixture,unittest.TestCase):
    names = ["continued-source-browse","z-raw0","z-raw1","raw1-movement-selection",
             "raw1-zero-move-command","raw1-magic-selected","magic-list-first",
             "list-cancel-back-command","magic-list-reopened"]

    def exercise(self,capture,save,report):
        probe.exercise(capture,self.source,self.focus,self.assets,self.actors,self.selection,
                       self.templates,self.fields,{3:[None,None,None]},save,save.read_bytes(),
                       save.stat().st_mtime_ns,report)

    def test_each_key_requires_previous_gate_then_cancel_and_reopen_without_cast(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(b"unchanged")
            capture = Mock()
            called = []
            def wait(capture,name,predicate,checkpoints,**kwargs):
                called.append(name)
                self.assertEqual(capture.key.call_count,len(called)+1)
                point = [52,76] if name=="z-raw0" else [124,100]
                return self.list_frame(),{"focus_screen":point}
            report = {"checkpoints":[]}
            with patch.object(probe,"wait_title"),patch.object(probe,"wait_state",side_effect=wait):
                self.exercise(capture,save,report)
            self.assertEqual(called,self.names)
            self.assertEqual([c.args[0] for c in capture.key.call_args_list],
                             ["UP","ENTER","Z","Z","ENTER","ENTER","LEFT","ENTER","ESC","ENTER"])
            self.assertFalse(report["input_scope"]["cast_confirmation_sent"])
            self.assertTrue(report["reopened_comparison"]["name_mp_cost_selection_unchanged"])

    def test_any_failed_gate_stops_before_next_key_and_never_sends_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(b"unchanged")
            for failure,stop in enumerate(self.names):
                capture = Mock()
                def wait(capture,name,predicate,checkpoints,**kwargs):
                    if name==stop:
                        raise TimeoutError("resource missing")
                    return self.list_frame(),{"focus_screen":[52,76] if name=="z-raw0" else [124,100]}
                with self.subTest(stop=stop),patch.object(probe,"wait_title"), \
                        patch.object(probe,"wait_state",side_effect=wait),self.assertRaises(TimeoutError):
                    self.exercise(capture,save,{"checkpoints":[]})
                self.assertEqual(capture.key.call_count,failure+2)

    def test_continue_title_failure_cannot_start_a_new_game(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(b"unchanged")
            capture = Mock()
            capture.wait.side_effect = TimeoutError("Continue title missing")
            with patch.object(probe,"wait_title"),self.assertRaises(TimeoutError):
                self.exercise(capture,save,{"checkpoints":[]})
            self.assertEqual([c.args[0] for c in capture.key.call_args_list],["UP"])


class RunnerModelTest(ResourceFixture,unittest.TestCase):
    def test_resolved_original_alias_is_rejected_before_mkdir_or_source_read(self):
        candidate = Path("C:/magic-list-candidate")
        original = Path("E:/readonly-evidence")
        output = original/"new-artifacts"
        resolved = {candidate:candidate,output:output,candidate/"original_game":original}
        def resolve(path,*args,**kwargs):
            return resolved.get(path,path)
        args = SimpleNamespace(root=candidate,artifacts=output)
        with patch.object(Path,"resolve",resolve),patch.object(Path,"mkdir") as mkdir, \
                patch.object(Path,"read_bytes") as read_bytes,patch.object(probe,"hashes") as hashes, \
                patch("subprocess.Popen",side_effect=AssertionError("pure test must not launch")):
            with self.assertRaisesRegex(ValueError,"original_game"):
                probe.run(args)
        mkdir.assert_not_called()
        read_bytes.assert_not_called()
        hashes.assert_not_called()

    def test_native_only_stays_probe_with_guards_and_cleanup_receipt_even_on_failure(self):
        for failure in (False,True):
            with self.subTest(failure=failure),tempfile.TemporaryDirectory() as directory:
                base = Path(directory)
                driver,dosbox = base/"driver.exe",base/"dosbox.exe"
                driver.write_bytes(b"not launched")
                dosbox.write_bytes(b"not launched")
                args = SimpleNamespace(root=probe.ROOT,driver=driver,dosbox=dosbox,artifacts=base/"output")
                @contextmanager
                def running(command,cwd,env,log,driver,receipts):
                    receipts.append({"pid":123,"cleaned":True,"forced_cleanup":False,"exit_code":0})
                    yield Mock(pid=123)
                def exercise(*args):
                    if failure:
                        raise TimeoutError("resource missing")
                with patch.object(probe,"hashes",return_value={"source":"stable"}), \
                        patch.object(probe,"running",side_effect=running),patch.object(probe,"Capture"), \
                        patch.object(probe.shutil,"copytree",side_effect=lambda source,destination:destination.mkdir()), \
                        patch.object(probe,"exercise",side_effect=exercise), \
                        patch("subprocess.Popen",side_effect=AssertionError("pure test must not launch")):
                    report = probe.run(args)
                self.assertFalse(report["ok"])
                self.assertTrue(report["probe_only"])
                self.assertEqual(report["probe_cases_passed"],not failure)
                self.assertTrue(report["copy_save_unchanged_after_cleanup"] and report["fixture_unchanged"])
                self.assertEqual(report["processes"][0]["exit_code"],0)
                self.assertEqual(json.loads((args.artifacts/"comparison.json").read_text(encoding="utf-8"))["ok"],False)

    def test_cli_uses_probe_receipt_without_claiming_sdl_pair_pass(self):
        arguments = ["run_original_magic_list.py","--root",".","--driver","driver.exe",
                     "--dosbox","dosbox.exe","--artifacts","unused-output"]
        for passed,code in ((True,0),(False,1)):
            with patch.object(probe.sys,"argv",arguments),patch.object(probe.sys,"platform","win32"), \
                    patch.object(probe,"run",return_value={"ok":False,"probe_only":True,"probe_cases_passed":passed}), \
                    patch("builtins.print"):
                self.assertEqual(probe.main(),code)


if __name__=="__main__":
    unittest.main()
