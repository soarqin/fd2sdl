"""原版MAGIC只读probe纯回归；没有窗口或游戏进程。"""
from contextlib import contextmanager
from pathlib import Path
import hashlib
import json
import os
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

    def command_frame(self,*,highlight=True,actor_overlay=True,sdl=False):
        image = Image.new("RGB",(320,200))
        position = (124,100)
        for item,index in enumerate((2,4 if highlight else 3,8 if sdl else 6,9)):
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

    def test_command_item_is_checked_per_role_and_never_claimed_equivalent(self):
        command = probe.PlayerOracle(self.source,self.assets,[],{})
        for role,sdl,item in (("original",False,6),("sdl",True,8)):
            detail = probe.command_page(self.command_frame(sdl=sdl),command,(124,100),self.templates,1,role=role)
            self.assertEqual(detail["matches"][2]["frame"],item)
            self.assertEqual(detail["matches"][2]["checked_pixels"],476)
            self.assertFalse(detail["capability_gates_compared"] or detail["cross_end_capability_equivalence"])
            self.assertTrue(detail["item_gate"]["cross_end_equivalence_excluded"])
            self.assertIsNone(probe.command_page(self.command_frame(sdl=not sdl),command,(124,100),self.templates,1,role=role))
            self.assertIsNone(probe.command_page(self.command_frame(highlight=False,sdl=sdl),command,(124,100),self.templates,1,role=role))


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


class DamageTest(ResourceFixture,unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.base = cls.source
        cls.encrypted = (probe.ROOT/"original_game/START.SAV").read_bytes()
        cls.derived_bytes,derived,cls.manifest = probe.derive_damage(cls.base,cls.encrypted)
        (cls.source,cls.focus,cls.assets,cls.actors,cls.selection,cls.templates,
         cls.fields) = probe.prepare(probe.ROOT/"original_game",source=derived)

    def test_damage_manifest_only_deploys_actor_and_preserves_hp_mp_ex_bitmap(self):
        self.assertEqual([(c["offset"],c["before"],c["after"]) for c in self.manifest["changed_plain_bytes"]],
                         [(0x12f3,10,7),(0x12f4,15,16),(0x59c7,88,86)])
        self.assertEqual(self.manifest["derived_encrypted_sha256"],
                         "77cb56bafbd8ceb767ae176c074f18d8c34959476522197bf8f3f926a5c19201")
        self.assertEqual(self.source[probe.UNITS+probe.STRIDE+2:-4],self.base[probe.UNITS+probe.STRIDE+2:-4])
        self.assertFalse(self.manifest["natural_one_turn_route_covered"])
        self.assertEqual(self.manifest["command_move_length"],0)
        self.assertEqual(self.fields["target_contract"]["raw_candidates"],[11])
        self.assertEqual(self.fields["target_contract"]["expected_camera"],[4,13])
        self.assertEqual(self.fields["target_contract"]["initial_focus_screen"],[28,148])
        self.assertEqual(self.fields["damage_contract"]["expected_experience"],42)
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"derived.SAV"
            save.write_bytes(self.derived_bytes)
            self.assertEqual(probe.decode_save(save),self.source)
        for plain,encoded in ((self.source,self.encrypted),(self.base,self.derived_bytes)):
            with self.assertRaises(ValueError):
                probe.derive_damage(plain,encoded)

    def result_frame(self,*,alive=False,acted=True):
        frame = Image.new("RGB",(320,200))
        plan = self.assets["damage_result"]
        frame.paste(plan["empty"].convert("RGB"),plan["empty_at"])
        if alive:
            part = plan["alive_actor"]["templates"][0]
            frame.paste(part.convert("RGB"),plan["empty_at"],part.point(lambda c:255 if c else 0,mode="L"))
        sprite = plan["acted"][0] if acted else probe.sheet_frame(
            (probe.ROOT/"original_game/FDICON.B24").read_bytes(),9*12)
        sprite.putpalette(self.assets["palette"])
        x,y = plan["caster_screen"]
        frame.paste(sprite.convert("RGB"),(x,y-6),sprite.point(lambda c:255 if c else 0,mode="L"))
        return frame

    def test_damage_target_uses_scrolled_frame1_actor_and_unoccluded_range(self):
        target = self.assets["magic_target"]
        def frame(actor):
            image = Image.new("RGB",(320,200))
            part = actor["templates"][0]
            x,y = target["screen"]
            image.paste(part.convert("RGB"),(x,y-6),part.point(lambda c:255 if c else 0,mode="L"))
            image.paste(target["range"]["ranges"][0][1].convert("RGB"),target["range"]["position"])
            return image
        good = frame(target["actor"])
        detail = probe.target_state(good,self.assets,self.templates,self.fields)
        self.assertEqual((detail["kind"],detail["raw_actor"],detail["range_roi"]),
                         ("magic0_target_selection",11,[124,52,24,24]))
        self.assertEqual(detail["focus"]["focus_screen"],[28,148])
        self.assertIsNone(probe.target_state(frame(self.assets["damage_result"]["alive_actor"]),
                                            self.assets,self.templates,self.fields))
        good.paste(target["range"]["base"].convert("RGB"),target["range"]["position"])
        self.assertIsNone(probe.target_state(good,self.assets,self.templates,self.fields))

    def test_damage_kill_requires_acted_and_complete_empty_target_miss_is_distinct(self):
        killed = probe.damage_result(self.result_frame(),self.assets)
        self.assertEqual(killed["outcome"],"kill")
        self.assertEqual(killed["empty_target"]["checked_pixels"],720)
        self.assertTrue(killed["target_raw_flags_not_read"])
        self.assertIsNone(probe.damage_result(self.result_frame(acted=False),self.assets))
        self.assertEqual(probe.damage_result(self.result_frame(alive=True),self.assets)["outcome"],"miss")
        bad = self.result_frame()
        bad.putpixel((28,142),(255,0,0))
        self.assertIsNone(probe.damage_result(bad,self.assets))
        self.assertIsNone(probe.damage_result(Image.new("RGB",(320,200)),self.assets))

    def detail_frame(self):
        frame = Image.new("RGB",(320,200))
        for part,at in self.assets["detail_edges"]:
            frame.paste(part.convert("RGB"),at,part.point(lambda c:255 if c else 0,mode="L"))
        for at,part in self.assets["damage_detail"].values():
            frame.paste(part.convert("RGB"),at)
        return frame

    def test_detail_requires_name_mp6_max8_lv1_ex42_and_native_edges(self):
        detail = probe.caster_detail(self.detail_frame(),self.source,self.assets)
        self.assertEqual((detail["mp"],detail["max_mp"],detail["level"],detail["experience"],detail["checked_pixels"]),
                         (6,8,1,42,1105))
        for point in ((98,13),(267,59),(157,33),(157,44)):
            frame = self.detail_frame()
            frame.putpixel(point,(255,0,0))
            self.assertIsNone(probe.caster_detail(frame,self.source,self.assets))
        self.assertIsNone(probe.caster_detail(self.list_frame(),self.source,self.assets))

    damage_keys = ["UP","ENTER","Z","Z","ENTER","ENTER","LEFT","ENTER","ENTER","ENTER",
                   "RIGHT","RIGHT","UP","UP","UP","F2"]

    def exercise_damage(self,capture,save,report,*,miss=False,fail_at=None):
        observed = []
        def wait(capture,name,predicate,checkpoints,**kwargs):
            observed.append((name,capture.key.call_count))
            if name==fail_at:
                raise TimeoutError("resource missing")
            if name=="damage-cast-outcome":
                self.assertEqual(capture.key.call_count,10)  # 单次提交后只有观察。
                return self.result_frame(alive=miss),{"outcome":"miss" if miss else "kill"}
            if name=="damage-caster-detail":
                frame = self.detail_frame()
                return frame,probe.caster_detail(frame,self.source,self.assets)
            return self.list_frame(),{"focus_screen":[52,76] if name=="z-raw0" else [52,124]}
        with patch.object(probe,"wait_title"),patch.object(probe,"wait_state",side_effect=wait), \
                patch.object(probe,"wait_measured_save",side_effect=AssertionError("damage must not Save")):
            probe.exercise(capture,self.source,self.focus,self.assets,self.actors,self.selection,
                self.templates,self.fields,{3:[None,None,None]},save,self.derived_bytes,
                save.stat().st_mtime_ns,report,scenario="damage")
        return observed

    def test_one_cast_then_observation_and_bounded_return_f2_without_save_or_skip(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(self.derived_bytes)
            capture,report = Mock(),{"checkpoints":[]}
            self.exercise_damage(capture,save,report)
            self.assertEqual([c.args[0] for c in capture.key.call_args_list],self.damage_keys)
            self.assertEqual(report["input_scope"]["cast_confirmation_count"],1)
            self.assertEqual(report["input_scope"]["post_cast_skip_keys"],[])
            self.assertFalse(report["input_scope"]["save_attempted"])
            self.assertEqual(report["damage_measurement"]["experience"],42)
            self.assertEqual(save.read_bytes(),self.derived_bytes)

    def test_miss_or_missing_gate_stops_without_reroll_recovery_or_extra_confirm(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(self.derived_bytes)
            observed = self.exercise_damage(Mock(),save,{"checkpoints":[]})
            for name,key_count in observed:
                capture = Mock()
                with self.subTest(gate=name),self.assertRaises(TimeoutError):
                    self.exercise_damage(capture,save,{"checkpoints":[]},fail_at=name)
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],self.damage_keys[:key_count])
            capture,report = Mock(),{"checkpoints":[]}
            with self.assertRaisesRegex(AssertionError,"未命中"):
                self.exercise_damage(capture,save,report,miss=True)
            self.assertEqual([c.args[0] for c in capture.key.call_args_list],self.damage_keys[:10])
            self.assertNotIn("damage_measurement",report)
            self.assertEqual(save.read_bytes(),self.derived_bytes)

    def test_damage_runner_records_controlled_fixture_and_rejects_any_save_write(self):
        for unexpected_save in (False,True):
            with self.subTest(unexpected_save=unexpected_save),tempfile.TemporaryDirectory() as directory:
                base = Path(directory)
                driver,dosbox = base/"driver.exe",base/"dosbox.exe"
                driver.write_bytes(b"not launched")
                dosbox.write_bytes(b"not launched")
                args = SimpleNamespace(root=probe.ROOT,driver=driver,dosbox=dosbox,
                                       artifacts=base/"output",scenario="damage")
                @contextmanager
                def running(command,cwd,env,log,driver,receipts):
                    receipts.append({"pid":123,"cleaned":True,"forced_cleanup":False,"exit_code":0})
                    yield Mock(pid=123)
                def exercise(capture,source,focus,assets,actors,selection,templates,fields,titles,
                             save,data,stamp,report,*,scenario):
                    self.assertEqual(scenario,"damage")
                    self.assertEqual(save.read_bytes(),self.derived_bytes)
                    report["damage_measurement"] = {"mp":6,"level":1,"experience":42}
                    if unexpected_save:
                        save.write_bytes(data[:-1]+bytes([data[-1]^1]))
                with patch.object(probe,"hashes",return_value={"source":"stable"}), \
                        patch.object(probe,"running",side_effect=running),patch.object(probe,"Capture"), \
                        patch.object(probe.shutil,"copytree",side_effect=lambda source,destination:destination.mkdir()), \
                        patch.object(probe,"exercise",side_effect=exercise), \
                        patch("subprocess.Popen",side_effect=AssertionError("pure test must not launch")):
                    report = probe.run(args)
                self.assertEqual(report["probe_cases_passed"],not unexpected_save)
                self.assertFalse(report["ok"])
                self.assertTrue(report["probe_only"] and report["fixture_unchanged"])
                self.assertEqual(report["copy_save_unchanged_after_cleanup"],not unexpected_save)
                self.assertEqual(report["derived_fixture"],self.manifest)
                self.assertNotIn("measurement_save_requested",report)
                self.assertEqual((args.artifacts/"damage-start.SAV").read_bytes(),self.derived_bytes)


class HealTest(ResourceFixture,unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.base = cls.source
        cls.encrypted = (probe.ROOT/"original_game/START.SAV").read_bytes()
        cls.derived_bytes,derived,cls.manifest = probe.derive_heal(cls.base,cls.encrypted)
        (cls.source,cls.focus,cls.assets,cls.actors,cls.selection,cls.templates,
         cls.fields) = probe.prepare(probe.ROOT/"original_game",source=derived)

    def test_heal_exact_manifest_does_not_change_roster_coordinates_or_caster_mp_ex(self):
        self.assertEqual([(c["offset"],c["before"],c["after"]) for c in self.manifest["changed_plain_bytes"]],
                         [(0x12e3,42,20),(0x130e,0,32),(0x59c7,88,98)])
        self.assertEqual(self.manifest["derived_encrypted_sha256"],
                         "fe694338e87e6c86c242dff743e0ce0a882789c222b0bb11adaebd2aeedfe0e9")
        self.assertEqual(self.source[0x8a3:probe.UNITS],self.base[0x8a3:probe.UNITS])
        self.assertEqual(self.fields["ids"],[0,13])
        self.assertEqual(self.fields["costs"],[2,3])
        self.assertEqual(self.fields["heal_contract"]["experience"],20)
        self.assertFalse(self.manifest["natural_learning_covered"])
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"heal.SAV"
            save.write_bytes(self.derived_bytes)
            self.assertEqual(probe.decode_save(save),self.source)
        with self.assertRaises(ValueError):
            probe.derive_heal(self.source,self.encrypted)

    def test_two_full_lists_and_each_frame1_actor_range_cycle_have_exact_resource_gates(self):
        for selected in (0,1):
            frame = self.list_frame(selected=selected)
            self.assertEqual(probe.list_page(frame,self.templates,self.fields,selected)["checked_pixels"],31603)
            self.assertIsNone(probe.list_page(frame,self.templates,self.fields,1-selected))
        for raw in (0,1,2,3):
            target = self.assets["heal_targets"][raw]
            frame = Image.new("RGB",(320,200))
            part = target["actor"]["templates"][0]
            x,y = target["screen"]
            frame.paste(part.convert("RGB"),(x,y-6),part.point(lambda c:255 if c else 0,mode="L"))
            frame.paste(target["range"]["ranges"][0][1].convert("RGB"),target["range"]["position"])
            view = dict(self.assets,magic_target=target)
            detail = probe.target_state(frame,view,self.templates,self.fields)
            self.assertEqual((detail["raw_actor"],detail["selection_frame"]),(raw,1))
            self.assertEqual(detail["focus"]["focus_screen"],target["screen"])
            frame.paste(target["range"]["base"].convert("RGB"),target["range"]["position"])
            self.assertIsNone(probe.target_state(frame,view,self.templates,self.fields))

    def browse_frame(self,*,acted=True):
        frame = Image.new("RGB",(320,200))
        plan = self.assets["heal_result"]
        part = plan["recipient"]["templates"][0]
        x,y = plan["recipient_screen"]
        frame.paste(part.convert("RGB"),(x,y-6),part.point(lambda c:255 if c else 0,mode="L"))
        sprite = plan["acted"][0] if acted else probe.sheet_frame(
            (probe.ROOT/"original_game/FDICON.B24").read_bytes(),9*12)
        sprite.putpalette(self.assets["palette"])
        x,y = plan["caster_screen"]
        frame.paste(sprite.convert("RGB"),(x,y-6),sprite.point(lambda c:255 if c else 0,mode="L"))
        return frame

    def detail_frame(self,kind):
        frame = Image.new("RGB",(320,200))
        for part,at in self.assets["detail_edges"]:
            frame.paste(part.convert("RGB"),at,part.point(lambda c:255 if c else 0,mode="L"))
        for at,part in self.assets["heal_"+kind+"_detail"].values():
            frame.paste(part.convert("RGB"),at)
        return frame

    def test_heal_requires_acted_before_hp42_and_mp5_lv1_ex20_detail_measurements(self):
        self.assertIsNotNone(probe.healed_browse(self.browse_frame(),self.assets))
        self.assertIsNone(probe.healed_browse(self.browse_frame(acted=False),self.assets))
        recipient = probe.recipient_detail(self.detail_frame("recipient"),self.source,self.assets)
        caster = probe.caster_detail(self.detail_frame("caster"),self.source,self.assets,heal=True)
        self.assertEqual((recipient["hp"],recipient["max_hp"],recipient["checked_pixels"]),(42,42,913))
        self.assertEqual((caster["mp"],caster["experience"],caster["level"],caster["checked_pixels"]),(5,20,1,1105))
        for kind,point in (("recipient",(267,41)),("caster",(267,59)),("caster",(157,44))):
            frame = self.detail_frame(kind)
            frame.putpixel(point,(255,0,0))
            found = probe.recipient_detail(frame,self.source,self.assets) if kind=="recipient" else probe.caster_detail(frame,self.source,self.assets,heal=True)
            self.assertIsNone(found)

    def test_cursor_above_acted_caster_is_composed_without_cropping(self):
        frame = Image.new("RGB",(320,200))
        cursor,sprite = self.assets["cursor"],self.assets["heal_result"]["acted"][0]
        frame.paste(cursor.convert("RGB"),(124,76),cursor.point(lambda c:255 if c else 0,mode="L"))
        frame.paste(sprite.convert("RGB"),(124,94),sprite.point(lambda c:255 if c else 0,mode="L"))
        found = probe.visual_focus(frame,self.assets,self.assets["heal_result"]["composites"])
        self.assertEqual(found["focus_screen"],[124,76])

    def magic_detail_frame(self,selected=-1):
        frame = self.detail_frame("caster")
        key = "unselected" if selected==-1 else "selected" if selected==0 else "selected_1"
        at,image = self.templates[key]
        frame.paste(image.convert("RGB"),at)
        return frame

    def closed_caster_frame(self,acted=True):
        frame = Image.new("RGB",(320,200))
        cursor = self.assets["cursor"]
        sprite = self.assets["heal_result"]["acted"][0] if acted else probe.sheet_frame(
            (probe.ROOT/"original_game/FDICON.B24").read_bytes(),9*12)
        sprite.putpalette(self.assets["palette"])
        frame.paste(cursor.convert("RGB"),(124,100),cursor.point(lambda c:255 if c else 0,mode="L"))
        frame.paste(sprite.convert("RGB"),(124,94),sprite.point(lambda c:255 if c else 0,mode="L"))
        frame.paste(self.assets["detail_close_terrain"]["image"].convert("RGB"),self.assets["detail_close_terrain"]["position"])
        return frame

    def test_caster_magic_detail_is_unselected_full_lower_and_then_closed_acted_browse(self):
        detail = probe.unselected_magic_detail(self.magic_detail_frame(),self.source,self.assets,self.templates)
        self.assertEqual((detail["selected"],detail["checked_lower_pixels"]),(-1,30690))
        self.assertEqual((detail["upper_values"]["mp"],detail["upper_values"]["experience"]),(5,20))
        self.assertFalse(detail["portrait_compared"])
        for frame in (self.magic_detail_frame(0),self.magic_detail_frame(1),self.detail_frame("caster"),self.list_frame(True)):
            self.assertIsNone(probe.unselected_magic_detail(frame,self.source,self.assets,self.templates))
        bad = self.magic_detail_frame()
        bad.putpixel((5,94),(255,0,0))
        self.assertIsNone(probe.unselected_magic_detail(bad,self.source,self.assets,self.templates))
        self.assertIsNotNone(probe.caster_closed_browse(self.closed_caster_frame(),self.source,self.assets,self.templates))
        self.assertIsNone(probe.caster_closed_browse(self.closed_caster_frame(False),self.source,self.assets,self.templates))
        self.assertIsNone(probe.caster_closed_browse(self.magic_detail_frame(),self.source,self.assets,self.templates))

    heal_keys = ["UP","ENTER","Z","Z","ENTER","ENTER","LEFT","ENTER",
        "UP","DOWN","LEFT","RIGHT","UP","ENTER","ESC","UP","ENTER","Z","Z","Z","Z","ENTER",
        "F2","ESC","RIGHT","RIGHT","RIGHT","DOWN","F2","ESC","ESC"]

    def exercise_heal(self,capture,save,report,*,fail_at=None):
        observed = []
        def wait(capture,name,predicate,checkpoints,**kwargs):
            observed.append((name,capture.key.call_count))
            if name==fail_at:
                raise TimeoutError("resource missing")
            if name=="heal-acted-closed-browse":
                self.assertEqual(capture.key.call_count,22)
            if name in ("heal-recipient-detail","heal-caster-detail"):
                kind = "recipient" if name=="heal-recipient-detail" else "caster"
                frame = self.detail_frame(kind)
                return frame,predicate(frame)
            if name=="heal-caster-unselected-magic-detail":
                frame = self.magic_detail_frame()
                return frame,predicate(frame)
            if name=="heal-caster-detail-closed-browse":
                frame = self.closed_caster_frame()
                return frame,predicate(frame)
            return self.list_frame(),{"focus_screen":[52,76] if name=="z-raw0" else [124,100]}
        with patch.object(probe,"wait_title"),patch.object(probe,"wait_state",side_effect=wait), \
                patch.object(probe,"wait_measured_save",side_effect=AssertionError("heal must not Save")):
            probe.exercise(capture,self.source,self.focus,self.assets,self.actors,self.selection,
                self.templates,self.fields,{3:[None,None,None]},save,self.derived_bytes,
                save.stat().st_mtime_ns,report,scenario="heal")
        return observed

    def test_heal_one_cast_two_details_no_save_and_no_extra_input_after_failed_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory)/"FD2.SAV"
            save.write_bytes(self.derived_bytes)
            capture,report = Mock(),{"checkpoints":[]}
            observed = self.exercise_heal(capture,save,report)
            self.assertEqual([c.args[0] for c in capture.key.call_args_list],self.heal_keys)
            self.assertEqual(report["input_scope"]["cast_confirmation_count"],1)
            self.assertEqual(report["heal_recipient_measurement"]["hp"],42)
            self.assertEqual(report["heal_caster_measurement"]["experience"],20)
            self.assertEqual(report["input_scope"]["caster_detail_inputs"],["ESC","ESC"])
            self.assertEqual(report["heal_caster_magic_detail"]["checked_lower_pixels"],30690)
            self.assertTrue(report["heal_caster_browse_closed"]["detail_resources_absent"])
            for name,count in observed:
                capture = Mock()
                with self.subTest(gate=name),self.assertRaises(TimeoutError):
                    self.exercise_heal(capture,save,{"checkpoints":[]},fail_at=name)
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],self.heal_keys[:count])
            self.assertEqual(save.read_bytes(),self.derived_bytes)


class PairingTest(unittest.TestCase):
    def model(self,directory,scenario,*,failed=None,mismatch=False,paired=True,late_guard=None):
        base = Path(directory)
        driver,dosbox,sdl = base/"driver.exe",base/"dosbox.exe",base/"fd2sdl.exe"
        for path in (driver,dosbox,sdl):
            path.write_bytes(b"not launched")
        args = SimpleNamespace(root=probe.ROOT,driver=driver,dosbox=dosbox,
                               artifacts=base/"output",scenario=scenario,sdl=sdl if paired else None)
        events,fixtures = [],[]
        fixture = (probe.ROOT/"original_game").resolve()/"START.SAV"
        original_read,original_stat = Path.read_bytes,Path.stat
        hash_calls,tripped,bad_fixture = 0,False,False
        def hashes(path):
            nonlocal hash_calls,tripped,bad_fixture
            hash_calls += 1
            if (late_guard=="original_copy" and hash_calls==2 or
                    late_guard=="sdl_copy" and events[-1:]==["closed-sdl"]) and not tripped:
                save = args.artifacts/("original/game/FD2.SAV" if paired else "game/FD2.SAV")
                if late_guard=="sdl_copy":
                    save = args.artifacts/"sdl/FD2.SAV"
                stamp = save.stat()
                os.utime(save,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+2_000_000_000))
                tripped = True
            if late_guard in ("fixture_bytes","fixture_mtime"):
                bad_fixture = hash_calls==3  # 只模拟该端finally，顶层读回已稳定来源。
            return {"source":"changed" if late_guard=="original_hash" and hash_calls==3 else "stable"}
        def read_bytes(path):
            data = original_read(path)
            return data[:-1]+bytes([data[-1]^1]) if bad_fixture and late_guard=="fixture_bytes" and path==fixture else data
        def stat(path,*args,**kwargs):
            value = original_stat(path,*args,**kwargs)
            if bad_fixture and late_guard=="fixture_mtime" and path==fixture:
                return SimpleNamespace(st_mtime_ns=value.st_mtime_ns+2_000_000_000,st_mode=value.st_mode)
            return value
        @contextmanager
        def running(command,cwd,env,log,driver,receipts):
            kind = "sdl" if command[0]==str(sdl.resolve()) else "original"
            events.append("start-"+kind)
            if kind=="sdl":
                self.assertEqual(cwd,probe.ROOT)
                self.assertEqual(Path(env["FD2SDL_SAVE_PATH"]),args.artifacts/"sdl/FD2.SAV")
            receipts.append({"pid":100+len(events),"cleaned":True,"forced_cleanup":False,"exit_code":0})
            try:
                yield Mock(pid=100+len(events))
            finally:
                events.append("closed-"+kind)
        def exercise(capture,source,focus,assets,actors,selection,templates,fields,titles,
                     save,data,stamp,record,**kwargs):
            kind = record.get("kind","original")
            fixtures.append((kind,data,source))
            self.assertEqual(save.read_bytes(),data)
            if kind==failed:
                raise TimeoutError("stopped at "+kind)
            if scenario=="single":
                record["reopened_comparison"] = {"name_mp_cost_selection_unchanged":True}
            elif scenario=="damage":
                record["damage_measurement"] = {"mp":6,"max_mp":8,"level":1,"experience":42 if not mismatch or kind=="original" else 41}
            elif scenario=="controls":
                measured = bytearray(source)
                measured[probe.META+5:probe.META+9] = bytes((9,15,4,4))
                measured[-4:] = sum(measured[:-4]).to_bytes(4,"little")
                save.write_bytes(bytes(a^b^c for a,b,c in zip(data,source,measured)))
                record["measurement"] = probe.check_load_measurement(source,measured,[("LEFT",(100,100))])
            else:
                record["heal_recipient_measurement"] = {"hp":42,"max_hp":42}
                record["heal_caster_measurement"] = {"mp":5,"max_mp":8,"level":1,"experience":20}
                record["heal_caster_magic_detail"] = {"selected":-1,"checked_lower_pixels":30690}
                record["heal_caster_browse_closed"] = {"detail_resources_absent":True}
        with patch.object(probe,"hashes",side_effect=hashes), \
                patch.object(Path,"read_bytes",read_bytes),patch.object(Path,"stat",stat), \
                patch.object(probe,"running",side_effect=running),patch.object(probe,"Capture"), \
                patch.object(probe.shutil,"copytree",side_effect=lambda source,destination:destination.mkdir()), \
                patch.object(probe,"exercise",side_effect=exercise), \
                patch("subprocess.Popen",side_effect=AssertionError("pure test must not launch")):
            report = probe.run(args)
        return report,events,fixtures,args.artifacts

    def test_single_damage_heal_pair_is_original_first_same_fixture_and_two_receipts(self):
        for scenario in ("single","damage","heal"):
            with self.subTest(scenario=scenario),tempfile.TemporaryDirectory() as directory:
                report,events,fixtures,output = self.model(directory,scenario)
                self.assertTrue(report["ok"] and report["probe_cases_passed"])
                self.assertFalse(report["probe_only"])
                self.assertEqual(events,["start-original","closed-original","start-sdl","closed-sdl"])
                self.assertEqual(fixtures[0][1:],fixtures[1][1:])
                self.assertEqual([c["status"] for c in report["cases"]],["passed","passed"])
                self.assertEqual(len(report["processes"]),2)
                self.assertEqual(report["paired_terminal_values"]["original"],report["paired_terminal_values"]["sdl"])
                self.assertEqual(report["known_sdl_differences"]["ITEM"]["sdl_frame"],8)
                for kind in ("original","sdl"):
                    endpoint = json.loads((output/kind/"comparison.json").read_text(encoding="utf-8"))
                    self.assertEqual(endpoint["kind"],kind)
                    self.assertTrue(endpoint["copy_save_unchanged_after_cleanup"])
                    self.assertTrue(endpoint["probe_cases_passed"])

    def test_native_failure_blocks_sdl_and_sdl_failure_or_mismatch_never_passes_pair(self):
        for failed,mismatch in (("original",False),("sdl",False),(None,True)):
            with self.subTest(failed=failed,mismatch=mismatch),tempfile.TemporaryDirectory() as directory:
                report,events,_,output = self.model(directory,"damage",failed=failed,mismatch=mismatch)
                self.assertFalse(report["ok"] or report["probe_cases_passed"])
                self.assertIn("error",report)
                if failed=="original":
                    self.assertEqual(events,["start-original","closed-original"])
                    self.assertFalse((output/"sdl").exists())
                else:
                    self.assertEqual(events,["start-original","closed-original","start-sdl","closed-sdl"])

    def test_original_heal_remains_probe_and_controls_pair_is_rejected_before_mkdir(self):
        with tempfile.TemporaryDirectory() as directory:
            report,events,_,_ = self.model(directory,"heal",paired=False)
            self.assertTrue(report["probe_cases_passed"] and report["probe_only"])
            self.assertFalse(report["ok"])
            self.assertEqual(events,["start-original","closed-original"])
        args = SimpleNamespace(root=probe.ROOT,artifacts=Path("C:/unused-controls-pair"),
                               scenario="controls",sdl=Path("C:/unused-sdl.exe"))
        with patch.object(Path,"mkdir") as mkdir,patch.object(probe,"hashes") as hashes, \
                patch("subprocess.Popen",side_effect=AssertionError("must not launch")):
            with self.assertRaisesRegex(ValueError,"controls.*--sdl"):
                probe.run(args)
            mkdir.assert_not_called()
            hashes.assert_not_called()

    def test_cli_pair_cannot_exit_success_from_probe_only_receipt(self):
        args = ["run_original_magic_list.py","--root",".","--driver","d.exe","--dosbox","b.exe",
                "--artifacts","unused","--sdl","s.exe"]
        with patch.object(probe.sys,"argv",args),patch.object(probe.sys,"platform","win32"), \
                patch.object(probe,"run",return_value={"ok":False,"probe_cases_passed":True}),patch("builtins.print"):
            self.assertEqual(probe.main(),1)

    def test_each_endpoint_final_copy_mtime_guard_revokes_pass_before_next_endpoint(self):
        for late_guard in ("original_copy","sdl_copy"):
            with self.subTest(guard=late_guard),tempfile.TemporaryDirectory() as directory:
                report,events,fixtures,output = self.model(directory,"single",late_guard=late_guard)
                self.assertFalse(report["ok"] or report["probe_cases_passed"])
                bad = report["cases"][0 if late_guard=="original_copy" else 1]
                self.assertEqual(bad["status"],"failed")
                self.assertFalse(bad["probe_cases_passed"] or bad["endpoint_final_guards_passed"])
                self.assertFalse(bad["copy_save_unchanged_after_cleanup"] or bad["copy_save_matches_completed_state_after_cleanup"])
                endpoint = json.loads((output/bad["kind"]/"comparison.json").read_text(encoding="utf-8"))
                self.assertEqual(endpoint["status"],"failed")
                self.assertEqual((output/bad["kind"]/("game/FD2.SAV" if bad["kind"]=="original" else "FD2.SAV")).read_bytes(),fixtures[-1][1])
                if late_guard=="original_copy":
                    self.assertEqual(events,["start-original","closed-original"])
                    self.assertFalse((output/"sdl").exists())

    def test_final_source_hash_fixture_bytes_and_fixture_mtime_cannot_be_rescued_by_top_guard(self):
        for guard in ("original_hash","fixture_bytes","fixture_mtime"):
            with self.subTest(guard=guard),tempfile.TemporaryDirectory() as directory:
                report,events,_,output = self.model(directory,"single",late_guard=guard)
                self.assertFalse(report["ok"] or report["probe_cases_passed"])
                self.assertEqual(events,["start-original","closed-original"])
                record = report["cases"][0]
                self.assertFalse(record["endpoint_final_guards_passed"])
                self.assertFalse(record["original_unchanged"] if guard=="original_hash" else record["fixture_unchanged"])
                self.assertTrue(report["original_unchanged"] and report["fixture_unchanged"])
                self.assertFalse((output/"sdl").exists())

    def test_controls_uses_completed_save_stamp_and_final_guard_preserves_first_error(self):
        for guard in (None,"original_copy"):
            with self.subTest(guard=guard),tempfile.TemporaryDirectory() as directory:
                report,_,_,_ = self.model(directory,"controls",paired=False,late_guard=guard)
                self.assertEqual(report["probe_cases_passed"],guard is None)
                self.assertFalse(report["copy_save_unchanged_after_cleanup"])
                self.assertEqual(report["copy_save_matches_completed_state_after_cleanup"],guard is None)
        with tempfile.TemporaryDirectory() as directory:
            report,_,_,_ = self.model(directory,"single",failed="original",late_guard="original_copy")
            self.assertEqual(report["error"],"stopped at original")
            self.assertEqual(report["cases"][0]["error"],"stopped at original")


if __name__=="__main__":
    unittest.main()
