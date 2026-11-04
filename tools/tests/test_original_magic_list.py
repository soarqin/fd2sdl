"""原版MAGIC只读probe纯回归；没有窗口或游戏进程。"""
from contextlib import contextmanager
from pathlib import Path
import hashlib
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

    def list_frame(self,unselected=False,selected=0):
        image = Image.new("RGB",(320,200))
        key = "unselected" if unselected else "selected" if selected==0 else f"selected_{selected}"
        for name in (key,"actor_name","current_max_mp"):
            at,part = self.templates[name]
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


class ControlsTest(ResourceFixture,unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.base = cls.source
        cls.encrypted = (probe.ROOT/"original_game/START.SAV").read_bytes()
        cls.derived_bytes,derived,cls.manifest = probe.derive_controls(cls.base,cls.encrypted)
        (cls.source,cls.focus,cls.assets,cls.actors,cls.selection,cls.templates,
         cls.fields) = probe.prepare(probe.ROOT/"original_game",source=derived)

    def test_exact_manifest_changes_only_two_bitmap_bytes_and_checksum(self):
        self.assertEqual([(item["offset"],item["before"],item["after"])
                          for item in self.manifest["changed_plain_bytes"]],
                         [(0x130d,1,15),(0x130e,0,32),(0x59c7,0x58,0x86)])
        self.assertEqual(self.manifest["derived_encrypted_sha256"],
                         "d548be0644e1102552085529d774873a060e585f09d987a87b5892a5ffcae3e1")
        self.assertEqual(self.fields["ids"],[0,1,2,3,13])
        self.assertEqual(self.fields["costs"],[2,6,20,42,3])
        self.assertEqual((self.fields["current_mp"],self.fields["max_mp"]),(8,8))
        self.assertEqual(self.source[0x8a3:probe.UNITS],self.base[0x8a3:probe.UNITS])
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)/"derived.SAV"
            fixture.write_bytes(self.derived_bytes)
            self.assertEqual(probe.decode_save(fixture),self.source)
        self.assertFalse(self.manifest["natural_learning_covered"])
        for source,encoded in ((self.source,self.encrypted),(self.base,self.encrypted[:-1]),
                               (self.base,bytes([self.encrypted[0]^1])+self.encrypted[1:])):
            with self.subTest(source=hashlib.sha256(source).hexdigest()),self.assertRaises(ValueError):
                probe.derive_controls(source,encoded)
        bad = bytearray(self.source)
        bad[0x130e] ^= 1
        with self.assertRaises(ValueError):
            probe.prepare(probe.ROOT/"original_game",source=bad)

    def test_all_five_selection_pages_require_full_31603_pixels_and_correct_index(self):
        for selected in range(5):
            frame = self.list_frame(selected=selected)
            detail = probe.list_page(frame,self.templates,self.fields,selected)
            self.assertEqual((detail["selected"],detail["checked_pixels"]),(selected,31603))
            self.assertEqual(detail["cost"],(2,6,20,42,3)[selected])
            for other in range(5):
                if other!=selected:
                    self.assertIsNone(probe.list_page(frame,self.templates,self.fields,other))
            for point in ((5,94),(98,13),(267,59),(168,108)):
                bad = frame.copy()
                bad.putpixel(point,(255,0,0))
                self.assertIsNone(probe.list_page(bad,self.templates,self.fields,selected))
        self.assertIsNone(probe.list_page(self.list_frame(True),self.templates,self.fields))
        with self.assertRaises(ValueError):
            probe.list_page(self.list_frame(),self.templates,self.fields,5)

    def target_frame(self,*,ordinary_cursor=False,range_visible=True):
        frame = Image.new("RGB",(320,200))
        target = self.assets["magic_target"]
        actor = self.actors[0] if ordinary_cursor else target["actor"]
        sprite = actor["templates"][0]
        x,y = target["screen"]
        frame.paste(sprite.convert("RGB"),(x,y-6),sprite.point(lambda c:255 if c else 0,mode="L"))
        part = target["range"]["ranges"][0][1] if range_visible else target["range"]["base"]
        frame.paste(part.convert("RGB"),target["range"]["position"])
        return frame

    def test_target_requires_frame1_actual_actor_and_range_not_frame0_browse_or_list(self):
        detail = probe.target_state(self.target_frame(),self.assets,self.templates,self.fields)
        self.assertEqual((detail["selection_mode"],detail["selection_frame"],detail["raw_actor"]),(2,1,0))
        self.assertFalse(detail["target_confirmation_sent"])
        self.assertEqual(self.fields["target_contract"]["raw_candidates"],[0,1,2,3])
        self.assertEqual(detail["focus"]["focus_screen"],[52,76])
        for frame in (self.target_frame(ordinary_cursor=True),self.target_frame(range_visible=False),
                      self.list_frame(selected=4),Image.new("RGB",(320,200))):
            self.assertIsNone(probe.target_state(frame,self.assets,self.templates,self.fields))
        bad = self.target_frame()
        bad.paste((255,0,0),(52,70,76,94))
        self.assertIsNone(probe.target_state(bad,self.assets,self.templates,self.fields))

    def test_cancelled_browse_requires_actor_and_untranslated_terrain(self):
        frame = Image.new("RGB",(320,200))
        part = self.actors[1]["templates"][0]
        x,y = self.selection["actor_position"]
        frame.paste(part.convert("RGB"),(x,y-6),part.point(lambda c:255 if c else 0,mode="L"))
        frame.paste(self.selection["base"].convert("RGB"),self.selection["position"])
        self.assertIsNotNone(probe.cancelled_browse(frame,self.assets,self.actors[1],self.selection))
        frame.paste(self.selection["ranges"][0][1].convert("RGB"),self.selection["position"])
        self.assertIsNone(probe.cancelled_browse(frame,self.assets,self.actors[1],self.selection))

    def measured(self,*,bad_mp=False):
        measured = bytearray(self.source)
        measured[probe.META+5:probe.META+9] = bytes((9,15,4,4))
        measured[probe.UNITS+2] ^= 1  # 已有严格测量允许renderer cache class变动。
        if bad_mp:
            measured[probe.UNITS+probe.STRIDE+0x44] -= 1
        measured[-4:] = sum(measured[:-4]).to_bytes(4,"little")
        encoded = bytes(a^b^c for a,b,c in zip(self.derived_bytes,self.source,measured))
        return bytes(measured),encoded

    def exercise_controls(self,capture,save,report,*,fail_at=None,bad_mp=False):
        observed = []
        selected = dict(zip(("up-wrap","right-edge","left-column","left-edge","down-first",
            "down-expensive","insufficient-enter","insufficient-space","right-partial-edge",
            "down-fourth","down-fifth","down-wrap","right-column"),(4,4,0,0,1,2,2,2,2,3,4,0,4)))
        def wait(capture,name,predicate,checkpoints,**kwargs):
            observed.append((name,capture.key.call_count))
            if name==fail_at:
                raise TimeoutError("resource missing")
            if name.removeprefix("controls-") in selected:
                frame = self.list_frame(selected=selected[name.removeprefix("controls-")])
                return frame,predicate(frame)
            point = [52,76] if name=="z-raw0" else [124,100]
            return self.list_frame(),{"focus_screen":point}
        def measured(*args):
            plain,encoded = self.measured(bad_mp=bad_mp)
            save.write_bytes(encoded)
            return plain
        with patch.object(probe,"wait_title"),patch.object(probe,"wait_state",side_effect=wait), \
                patch.object(probe,"wait_measured_save",side_effect=measured):
            probe.exercise(capture,self.source,self.focus,self.assets,self.actors,self.selection,
                self.templates,self.fields,{3:[None,None,None]},save,self.derived_bytes,
                save.stat().st_mtime_ns,report,scenario="controls")
        return observed

    controls_keys = ["UP","ENTER","Z","Z","ENTER","ENTER","LEFT","ENTER",
        "UP","RIGHT","LEFT","LEFT","DOWN","DOWN","ENTER","SPACE","RIGHT",
        "DOWN","DOWN","DOWN","RIGHT","ENTER","ESC","ESC","ESC","ESC",
        "LEFT","ENTER","UP","ENTER","LEFT","ENTER","ENTER"]

    def test_controls_every_key_ack_target_cancel_reset_zero_then_strict_save(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Mock(artifacts=Path(directory))
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(self.derived_bytes)
            report = {"checkpoints":[]}
            observed = self.exercise_controls(capture,save,report)
            self.assertEqual([call.args[0] for call in capture.key.call_args_list],self.controls_keys)
            self.assertEqual([item["selected"] for item in report["controls_navigation"]],
                             [4,4,0,0,1,2,2,2,2,3,4,0,4])
            self.assertEqual(observed[-1],("controls-measurement-saved-browse",33))
            self.assertFalse(report["input_scope"]["target_confirmation_sent"])
            self.assertTrue(report["measurement"]["source_equal_except_declared_cache_and_measuring_focus"])
            self.assertTrue(report["reopened_comparison"]["name_mp_cost_selection_unchanged"])
            self.assertEqual((capture.artifacts/"controls-measured.SAV").read_bytes(),save.read_bytes())

    def test_controls_any_failed_gate_stops_without_recovery_or_unguarded_save(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(self.derived_bytes)
            observed = self.exercise_controls(Mock(artifacts=Path(directory)),save,{"checkpoints":[]})
            for name,key_count in observed[:-1]:
                save.write_bytes(self.derived_bytes)
                capture = Mock(artifacts=Path(directory))
                with self.subTest(gate=name),self.assertRaises(TimeoutError):
                    self.exercise_controls(capture,save,{"checkpoints":[]},fail_at=name)
                self.assertEqual([call.args[0] for call in capture.key.call_args_list],self.controls_keys[:key_count])
                self.assertEqual(save.read_bytes(),self.derived_bytes)

    def test_unknown_measurement_diff_fails_and_preserves_actual_file_without_extra_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Mock(artifacts=Path(directory))
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(self.derived_bytes)
            report = {"checkpoints":[]}
            with self.assertRaisesRegex(AssertionError,"outside the declared"):
                self.exercise_controls(capture,save,report,bad_mp=True)
            self.assertNotIn("measurement",report)
            self.assertEqual([call.args[0] for call in capture.key.call_args_list],self.controls_keys)
            self.assertEqual(save.read_bytes(),self.measured(bad_mp=True)[1])
            self.assertEqual((capture.artifacts/"controls-measured.SAV").read_bytes(),save.read_bytes())

    def test_controls_run_is_still_probe_records_manifest_and_guards_post_save_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            driver,dosbox = base/"driver.exe",base/"dosbox.exe"
            driver.write_bytes(b"not launched")
            dosbox.write_bytes(b"not launched")
            args = SimpleNamespace(root=probe.ROOT,driver=driver,dosbox=dosbox,
                                   artifacts=base/"output",scenario="controls")
            @contextmanager
            def running(command,cwd,env,log,driver,receipts):
                receipts.append({"pid":123,"cleaned":True,"forced_cleanup":False,"exit_code":0})
                yield Mock(pid=123)
            def exercise(capture,source,focus,assets,actors,selection,templates,fields,titles,
                         save,data,stamp,report,*,scenario):
                self.assertEqual(scenario,"controls")
                self.assertEqual(data,self.derived_bytes)
                self.assertEqual(save.read_bytes(),data)
                measured,encoded = self.measured()
                save.write_bytes(encoded)
                report["measurement"] = probe.check_load_measurement(source,measured,[("LEFT",(100,100))])
            with patch.object(probe,"hashes",return_value={"source":"stable"}), \
                    patch.object(probe,"running",side_effect=running),patch.object(probe,"Capture"), \
                    patch.object(probe.shutil,"copytree",side_effect=lambda source,destination:destination.mkdir()), \
                    patch.object(probe,"exercise",side_effect=exercise), \
                    patch("subprocess.Popen",side_effect=AssertionError("pure test must not launch")):
                report = probe.run(args)
            self.assertTrue(report["probe_cases_passed"])
            self.assertFalse(report["ok"])
            self.assertTrue(report["probe_only"] and report["fixture_unchanged"])
            self.assertFalse(report["copy_save_unchanged_after_cleanup"])
            self.assertTrue(report["copy_save_matches_completed_state_after_cleanup"])
            self.assertEqual(report["derived_fixture"],self.manifest)
            self.assertEqual((args.artifacts/"controls-start.SAV").read_bytes(),self.derived_bytes)


if __name__=="__main__":
    unittest.main()
