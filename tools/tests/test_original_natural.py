"""自然首关只读planner契约/资源/首错停止；无窗口，不当作自然通关证据。"""
import copy
import hashlib
import json
import os
import shutil
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch
from contextlib import contextmanager

from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
import fd2_natural_oracle as oracle
import run_original_natural as runner
from run_graphics_tests import decode_save
from run_original_end_turn import META,UNITS,STRIDE,round_assets,round_two_template
from run_original_reward import actor_view


def test_plan(source):
    def action(raw,destination=None):
        unit=oracle.actor(source,raw)
        return {"kind":"wait","actor_raw":raw,"target_raw":-1,"destination":list(unit[:2]) if destination is None else destination,
            "actor_record_hex":unit.hex(),"reachable_path_directions":[],"command_disabled":[1,1,0,0],"command_first_enabled":2}
    return {"planning_only":True,"original_acceptance":False,"query_side_effects":False,
        "source_turn":source[META],"source_active_count":source[META+1],"source_roster_count":source[META+9],
        "next_action":action(3,[7,17]),"rest_tail":[action(raw) for raw in (0,1,2)]}


def page_frame(page,assets):
    frame=page["text"].convert("RGB")
    position=page.get("portrait_position",(8,115));border=page.get("border_position",(5,112))
    frame.paste(assets["dialog_top"].convert("RGB"),border)
    frame.paste(page["portraits"][0].convert("RGB"),position)
    if page.get("requires_key") or page.get("arrow_position"):
        frame.paste(page["arrows"][0].convert("RGB"),page.get("arrow_position",(191,181)))
    return frame


class NaturalTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=ROOT/"original_game";cls.source=decode_save(cls.original/"START.SAV")
        cls.encoded=(cls.original/"START.SAV").read_bytes();cls.assets=round_assets(cls.original)
        cls.plan=test_plan(cls.source)

    def test_actual_snapshot_records_and_stationary_rest_contract(self):
        actions=oracle.validate_plan(self.plan,self.source)
        self.assertEqual([a["actor_raw"] for a in actions],[3,0,1,2])
        self.assertEqual(actions[0]["destination"],[7,17])
        self.assertTrue(all(a["destination"]==list(oracle.actor(self.source,a["actor_raw"])[:2]) for a in actions[1:]))

    def test_stale_actor_duplicate_rest_and_nonzero_mage_move_rejected(self):
        for error in ("stale","duplicate","restmove","magicmove","turn","realoracle"):
            plan=copy.deepcopy(self.plan)
            if error=="stale":plan["next_action"]["actor_record_hex"]="00"*80
            elif error=="duplicate":plan["rest_tail"][0]=copy.deepcopy(plan["next_action"])
            elif error=="restmove":plan["rest_tail"][0]["destination"][0]+=1
            elif error=="magicmove":plan["next_action"].update(kind="magic",magic_id=0)
            elif error=="turn":plan["source_turn"]+=1
            else:plan["original_acceptance"]=True
            with self.assertRaises(ValueError,msg=error):oracle.validate_plan(plan,self.source)

    def test_hero_rest_reposition_is_explicit_query_intent_and_preserves_source(self):
        plan=copy.deepcopy(self.plan)
        hero=plan["rest_tail"][0]
        hero.update(destination=[8,14],reachable_path_directions=[3],hero_safety_reposition=True)
        before=copy.deepcopy(plan);source=bytes(self.source)
        actions=oracle.validate_plan(plan,self.source)
        self.assertEqual([action["actor_raw"] for action in actions],[3,0,1,2])
        self.assertEqual(actions[1]["destination"],[8,14])
        self.assertEqual(actions[1]["actor_record_hex"],oracle.actor(self.source,0).hex())
        self.assertEqual(plan,before);self.assertEqual(bytes(self.source),source)
        self.assertTrue(all(action["kind"]=="wait" and action["destination"]==list(oracle.actor(self.source,action["actor_raw"])[:2])
            for action in actions[2:]))

    def test_hero_rest_reposition_rejects_identity_marker_and_wrong_path(self):
        for fault in ("raw","unit","text","kind","missingmarker","falsemarker","integer_marker","string_marker",
                      "missingpath","emptypath","mappingpath","bad_direction","negative_direction","string_direction",
                      "bool_direction","float_direction","wrongendpoint","outside_then_return","stale","duplicate"):
            source=bytearray(self.source);plan=copy.deepcopy(self.plan)
            hero=plan["rest_tail"][0]
            hero.update(destination=[8,14],reachable_path_directions=[3],hero_safety_reposition=True)
            plan["rest_tail"]=[hero]
            if fault=="raw":
                hero.update(actor_raw=1,actor_record_hex=oracle.actor(source,1).hex(),
                    destination=[oracle.actor(source,1)[0]+1,oracle.actor(source,1)[1]])
            elif fault in ("unit","text"):
                source[UNITS+(7 if fault=="unit" else 8)]=9
                hero["actor_record_hex"]=oracle.actor(source,0).hex()
                source[-4:]=sum(source[:-4]).to_bytes(4,"little")
            elif fault=="kind":hero["kind"]="attack"
            elif fault=="missingmarker":hero.pop("hero_safety_reposition")
            elif fault=="falsemarker":hero["hero_safety_reposition"]=False
            elif fault=="integer_marker":hero["hero_safety_reposition"]=1
            elif fault=="string_marker":hero["hero_safety_reposition"]="true"
            elif fault=="missingpath":hero.pop("reachable_path_directions")
            elif fault=="emptypath":hero["reachable_path_directions"]=[]
            elif fault=="mappingpath":hero["reachable_path_directions"]={"direction":3}
            elif fault=="bad_direction":hero["reachable_path_directions"]=[4]
            elif fault=="negative_direction":hero["reachable_path_directions"]=[-1]
            elif fault=="string_direction":hero["reachable_path_directions"]=["3"]
            elif fault=="bool_direction":hero["reachable_path_directions"]=[True]
            elif fault=="float_direction":hero["reachable_path_directions"]=[3.0]
            elif fault=="wrongendpoint":hero["reachable_path_directions"]=[0]
            elif fault=="outside_then_return":hero["reachable_path_directions"]=[1]*8+[3]*9
            elif fault=="stale":hero["actor_record_hex"]="00"*STRIDE
            else:plan["rest_tail"].append(copy.deepcopy(hero))
            with self.subTest(fault=fault),self.assertRaises(ValueError):oracle.validate_plan(plan,source)

    def test_semantic_cycle_ignores_turn_cache_unknown_but_observes_live_hp(self):
        source=bytearray(self.source);digest=oracle.semantic_digest(source)
        for at in (META,UNITS+2,UNITS+3,UNITS+4,UNITS+0x28,UNITS+0x3d):source[at]^=1
        self.assertEqual(oracle.semantic_digest(source),digest)
        source[UNITS+0x40]-=1
        self.assertNotEqual(oracle.semantic_digest(source),digest)

    def test_readonly_planner_refuses_even_identical_bytes_rewritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/"actual.SAV";path.write_bytes(self.encoded)
            good=SimpleNamespace(returncode=0,stdout=json.dumps(self.plan),stderr="")
            with patch.object(runner.subprocess,"run",return_value=good):
                self.assertEqual(runner.read_plan(Path("planner"),ROOT,path),self.plan)
            def rewrite(*args,**kwargs):
                os.utime(path,ns=(path.stat().st_atime_ns,path.stat().st_mtime_ns+2_000_000_000));return good
            with patch.object(runner.subprocess,"run",side_effect=rewrite):
                with self.assertRaises(AssertionError):runner.read_plan(Path("planner"),ROOT,path)

    def test_native_round_two_resource_is_exact_old_verified_model(self):
        self.assertEqual(oracle.round_banner(self.original,self.assets,2).convert("RGB").tobytes(),
            round_two_template(self.original/"FDOTHER.DAT",self.assets["palette"]).convert("RGB").tobytes())
        self.assertNotEqual(oracle.round_banner(self.original,self.assets,3).tobytes(),oracle.round_banner(self.original,self.assets,2).tobytes())
        for value in (0,256):
            with self.assertRaises(ValueError):oracle.round_banner(self.original,self.assets,value)

    def test_navigation_compares_cursor_cell_with_lower_actor_head_not_body_under_info_panel(self):
        # sub_127E0上移6px：下邻unit30的头进入光标格末6行；
        # 固定1acf3右板从y161开始，不属于光标格(244,124,24,24)。
        source=bytearray(self.source);source[META+1]=1;source[UNITS+7]=30
        before=bytes(source);bank=oracle.cursor_bank(self.original,source,self.assets)
        _,composites=oracle.cursor_assets(self.original/"FDICON.B24",source,self.assets)
        image,offset=next((image,offset) for image,offset in composites if image.height==42)
        at=(244,124);frame=Image.new("RGB",(320,200))
        frame.paste(image.convert("RGB"),at)
        sheet=oracle.archive_entry((self.original/"FDOTHER.DAT").read_bytes(),5)
        import struct
        start,end=struct.unpack_from("<II",sheet,6+130*4)
        panel=oracle.decode_image(sheet[start:end]);panel.putpalette(self.assets["palette"])
        frame.paste(panel.convert("RGB"),(246,161))
        self.assertIsNone(oracle.cursor_match(frame,self.assets,[(image,offset)],at))
        self.assertIsNotNone(oracle.cursor_match(frame,self.assets,bank,at))
        self.assertTrue(all(image.size==(24,24) and offset==0 for image,offset in bank))
        self.assertIsNone(oracle.cursor_match(frame,self.assets,bank,(at[0]+24,at[1])))
        cursor_pixels=list(self.assets["cursor"].getdata())
        index=next(i for i,value in enumerate(cursor_pixels[:18*24]) if value)
        bad=frame.copy();bad.putpixel((at[0]+index%24,at[1]+index//24),(255,0,255))
        self.assertIsNone(oracle.cursor_match(bad,self.assets,bank,at))
        self.assertEqual(bytes(source),before)

    def test_navigation_current_actor_still_checks_cursor_bottom_rows(self):
        source=bytearray(self.source);source[META+1]=1;source[UNITS+7]=30
        bank=oracle.cursor_bank(self.original,source,self.assets)
        _,composites=oracle.cursor_assets(self.original/"FDICON.B24",source,self.assets)
        image,offset=next((image,offset) for image,offset in composites if offset==-6)
        at=(100,76);frame=Image.new("RGB",(320,200))
        frame.paste(image.convert("RGB"),(at[0],at[1]+offset))
        self.assertIsNotNone(oracle.cursor_match(frame,self.assets,bank,at))
        cursor_pixels=list(self.assets["cursor"].getdata())
        index=next(i for i in range(18*24,24*24) if cursor_pixels[i])
        bad=frame.copy();bad.putpixel((at[0]+index%24,at[1]+index//24),(255,0,255))
        self.assertIsNone(oracle.cursor_match(bad,self.assets,bank,at))

    def test_navigation_checks_both_current_actor_and_lower_actor_head_bands(self):
        source=bytearray(self.source);source[META+1]=2
        source[UNITS+7]=9;source[UNITS+STRIDE+7]=30
        _,composites=oracle.cursor_assets(self.original/"FDICON.B24",source,self.assets)
        current=next(image for image,offset in composites if offset==-6)
        lower=next(image for image,offset in reversed(composites) if offset==0)
        at=(100,76);frame=Image.new("RGB",(320,200))
        frame.paste(current.convert("RGB"),(at[0],at[1]-6))
        frame.paste(lower.crop((0,18,24,24)).convert("RGB"),(at[0],at[1]+18))
        bank=oracle.cursor_bank(self.original,source,self.assets)
        found=oracle.cursor_match(frame,self.assets,bank,at)
        self.assertIsNotNone(found)
        self.assertEqual([part["visible_roi"] for part in found["resource_bands"]],
            [[100,76,24,18],[100,94,24,6]])
        self.assertTrue(all(part["checked_pixels"]>0 for part in found["resource_bands"]))
        self.assertEqual(found["checked_pixels"],sum(part["checked_pixels"] for part in found["resource_bands"]))
        self.assertEqual(found["visible_roi"],[100,76,24,24])
        for start,end in ((0,18*24),(18*24,24*24)):
            index=next(i for i in range(start,end) if self.assets["cursor"].getdata()[i])
            bad=frame.copy();bad.putpixel((at[0]+index%24,at[1]+index//24),(255,0,255))
            self.assertIsNone(oracle.cursor_match(bad,self.assets,bank,at))
        missing_lower=frame.copy();missing_lower.paste((0,0,0),(100,94,124,100))
        self.assertIsNone(oracle.cursor_match(missing_lower,self.assets,bank,at))

    def test_static_range_requires_complete_nonwater_tile_and_real_translation(self):
        probes=oracle.range_probe(self.original,self.source,self.assets,0)
        probe=probes[0];phase,image=probe["ranges"][0]
        frame=Image.new("RGB",(320,200));frame.paste(image.convert("RGB"),probe["position"])
        result=oracle.range_match(frame,probes)
        self.assertEqual(result["checked_pixels"],576)
        self.assertNotIn(0,probe["base"].getdata())
        self.assertFalse(any(224<=x<240 for x in probe["base"].getdata()))
        frame.putpixel(probe["position"],(255,0,255))
        self.assertIsNone(oracle.range_match(frame,probes))

    def test_movement_range_avoids_both_fixed_upper_and_lower_information_boards(self):
        # 18b84的page偏移2285/2436，加VGA原点(4,4)；18c6d frame22=149x42。
        # 1acf3 page x1/242、y157，加同原点；frame130=69x34。
        cases = [((148,28),[(9,9,149,42)],(158,28,172,52)),
                 ((148,28),[(160,9,149,42)],(148,28,160,52)),
                 ((28,148),[(5,161,69,34),(246,161,69,34)],(28,148,52,161)),
                 ((268,148),[(5,161,69,34),(246,161,69,34)],(268,148,292,161)),
                 ((100,76),[(9,9,149,42),(5,161,69,34)],(100,76,124,100)),
                 ((20,20),[(9,9,149,42)],None)]
        for position, panels, expected in cases:
            with self.subTest(position=position,panels=panels):
                self.assertEqual(oracle._range_rectangle(position,panels),expected)

    def test_movement_probe_keeps_strict_visible_rectangle_and_rejects_bare_terrain(self):
        # 只构造查询坐标，不写游戏SAV；真实首关(10,12)静态地形在左板右边露14列。
        source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes((11,12))
        source=oracle.view(source,(11,12),(4,11))
        probe=next(p for p in oracle.range_probe(self.original,source,self.assets,0) if p["world"]==[10,12])
        self.assertEqual(probe["tile_roi"],[148,28,24,24])
        self.assertEqual(probe["range_roi"],[158,28,14,24])
        sheet=oracle.archive_entry((self.original/"FDOTHER.DAT").read_bytes(),5)
        panel=oracle.sheet_frame(sheet,22,lmi=True);panel.putpalette(self.assets["palette"])
        self.assertEqual(panel.size,(149,42))
        for phase,image in probe["ranges"]:
            frame=Image.new("RGB",(320,200));frame.paste(image.convert("RGB"),probe["position"])
            frame.paste(panel.convert("RGB"),(9,9))
            hit=oracle.range_match(frame,[probe])
            self.assertEqual(hit["checked_pixels"],336)
            self.assertEqual(hit["range_roi"],[158,28,14,24])
            plain=probe["base"].crop((10,0,24,24)).convert("RGB")
            self.assertGreaterEqual(sum(max(abs(a-b) for a,b in zip(w,g))>3
                for w,g in zip(image.convert("RGB").getdata(),plain.getdata())),24)
        bare=Image.new("RGB",(320,200));bare.paste(probe["base"].convert("RGB"),(148,28))
        bare.paste(panel.convert("RGB"),(9,9))
        self.assertIsNone(oracle.range_match(bare,[probe]))
        for x,y in ((158,28),(171,28),(158,51),(171,51)):
            bad=frame.copy();bad.putpixel((x,y),(255,0,255))
            self.assertIsNone(oracle.range_match(bad,[probe]))

    def test_cast_range_does_not_apply_movement_information_board_crop(self):
        source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes((11,12))
        source=oracle.view(source,(11,12),(4,11))
        probe=next(p for p in oracle.range_probe(self.original,source,self.assets,0,radius=1) if p["world"]==[10,12])
        self.assertEqual(probe["range_roi"],[148,28,24,24])
        self.assertEqual(probe["excluded_information_panels"],[])
        self.assertTrue(all(image.size==(24,24) for _,image in probe["ranges"]))

    def _occupied_physical_source(self):
        source=bytearray(self.source);source[META+1]=7
        for raw in range(7):source[UNITS+raw*STRIDE+5]=1
        for raw,xy in ((0,(8,16)),(3,(7,16)),(4,(6,16)),(5,(7,15)),(6,(7,17))):
            source[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]=bytes(xy);source[UNITS+raw*STRIDE+5]=0
        return oracle.view(source,(6,16),(2,14))

    def test_physical_third_actor_bottom_strip_is_strict_and_has_positive_range_contrast(self):
        source=self._occupied_physical_source();before=bytes(source)
        with self.assertRaisesRegex(ValueError,"正向range"):
            oracle.range_probe(self.original,source,self.assets,3,radius=1,minimum=1)
        probes=oracle.range_probe(self.original,source,self.assets,3,radius=1,minimum=1,physical_third_actors=(0,))
        self.assertEqual(len(probes),1);probe=probes[0]
        self.assertEqual(probe["world"],[8,16]);self.assertEqual(probe["range_roi"],[148,70,24,6])
        self.assertEqual(probe["occupied_actor_raw"],0);self.assertFalse(probe["actor_pixels_compared"])
        self.assertFalse({10,14,22}&{phase for phase,_ in probe["ranges"]})
        plain=probe["base"].crop((0,18,24,24)).convert("RGB")
        for phase,image in probe["ranges"]:
            self.assertGreaterEqual(sum(max(abs(a-b) for a,b in zip(x,y))>3 for x,y in zip(image.convert("RGB").getdata(),plain.getdata())),24,phase)
            frame=Image.new("RGB",(320,200));frame.paste(image.convert("RGB"),probe["position"])
            hit=oracle.range_match(frame,probes)
            self.assertEqual(hit["checked_pixels"],144);self.assertFalse(hit["actor_pixels_compared"])
            for corner in ((148,70),(171,70),(148,75),(171,75)):
                bad=frame.copy();bad.putpixel(corner,(255,0,255));self.assertIsNone(oracle.range_match(bad,probes))
        bare=Image.new("RGB",(320,200));bare.paste(plain,probe["position"])
        self.assertIsNone(oracle.range_match(bare,probes));self.assertEqual(bytes(source),before)

    def test_occupied_strip_requires_unaffected_nonfocus_actor_and_no_lower_head_empty_first(self):
        source=self._occupied_physical_source()
        # 默认cast路径没有这项许可；caster和当前确认目标也不能成为第三actor。
        for allowed in ((),(3,),(4,)):
            with self.assertRaisesRegex(ValueError,"正向range"):
                oracle.range_probe(self.original,source,self.assets,3,radius=1,minimum=1,physical_third_actors=allowed)
        crowded=bytearray(source);crowded[META+1]=8
        crowded[UNITS+7*STRIDE:UNITS+8*STRIDE]=oracle.actor(source,4)
        crowded[UNITS+7*STRIDE:UNITS+7*STRIDE+2]=bytes((8,17))
        with self.assertRaisesRegex(ValueError,"正向range"):
            oracle.range_probe(self.original,crowded,self.assets,3,radius=1,minimum=1,physical_third_actors=(0,))
        source[UNITS+5*STRIDE+5]|=1
        probes=oracle.range_probe(self.original,source,self.assets,3,radius=1,minimum=1,physical_third_actors=(0,))
        self.assertTrue(probes);self.assertTrue(all("occupied_actor_raw" not in p for p in probes))
        self.assertTrue(all(p["world"]==[7,15] for p in probes))

    def test_lower_actor_head_keeps_strict_24_by_18_positive_range_rectangle(self):
        source=bytearray(self.source);source[META+1]=2
        source[UNITS:UNITS+2]=bytes((11,12));source[UNITS+0x20]=2
        source[UNITS+STRIDE:UNITS+2*STRIDE]=self.source[UNITS+4*STRIDE:UNITS+5*STRIDE]
        source[UNITS+STRIDE:UNITS+STRIDE+2]=bytes((10,13))
        source=oracle.view(source,(11,12),(6,11))
        probe=next(p for p in oracle.range_probe(self.original,source,self.assets,0) if p["world"]==[10,12])
        self.assertEqual(probe["range_roi"],[100,28,24,18])
        self.assertIn([1,100,46,24,6],probe["excluded_actor_heads"])
        frame=Image.new("RGB",(320,200));phase,image=probe["ranges"][0]
        frame.paste(image.convert("RGB"),probe["position"])
        head=runner.sheet_frame((self.original/"FDICON.B24").read_bytes(),96*12).crop((0,0,24,6))
        head.putpalette(self.assets["palette"]);frame.paste(head.convert("RGB"),(100,46))
        self.assertEqual(oracle.range_match(frame,[probe])["checked_pixels"],432)
        bare=frame.copy();bare.paste(probe["base"].crop((0,0,24,18)).convert("RGB"),(100,28))
        self.assertIsNone(oracle.range_match(bare,[probe]))
        for x,y in ((100,28),(123,28),(100,45),(123,45)):
            broken=frame.copy();broken.putpixel((x,y),(255,0,255))
            self.assertIsNone(oracle.range_match(broken,[probe]))
        source[UNITS+STRIDE+5]|=1
        full=next(p for p in oracle.range_probe(self.original,source,self.assets,0) if p["world"]==[10,12])
        self.assertEqual(full["range_roi"],[100,28,24,24])

    def _crowded_stationary_hero(self):
        # 独立原资源事实：tile206在(12,13)，profile1成本20；三个其它邻格占用。
        source=bytearray(self.source);source[META+1]=4
        for raw,xy in enumerate(((12,14),(11,14),(13,14),(12,15))):
            source[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]=bytes(xy)
            source[UNITS+raw*STRIDE+5]=0
        source[UNITS+0x20]=1;source[UNITS+0x3b]=4
        return oracle.view(source,(12,14),(3,13))

    def test_crowded_self_cast_uses_fixed_head_crop_and_every_phase_has_positive_contrast(self):
        source=self._crowded_stationary_hero()
        self.assertEqual(oracle.range_probe(self.original,source,self.assets,0,allow_empty=True),[])
        probe,=oracle.range_probe(self.original,source,self.assets,0,radius=1,minimum=0)
        self.assertEqual(probe["world"],[12,13]);self.assertEqual(probe["range_roi"],[220,4,24,18])
        self.assertEqual(probe["excluded_information_panels"],[])
        self.assertIn([0,220,22,24,6],probe["excluded_actor_heads"])
        plain=probe["base"].crop((0,0,24,18)).convert("RGB")
        self.assertEqual(len(probe["ranges"]),20) # 原23相位中10/14/22裁后色差仅3/4/3像素。
        for phase,image in probe["ranges"]:
            self.assertGreaterEqual(sum(max(abs(a-b) for a,b in zip(w,g))>3
                for w,g in zip(image.convert("RGB").getdata(),plain.getdata())),24,phase)
            frame=Image.new("RGB",(320,200));frame.paste(image.convert("RGB"),(220,4))
            self.assertEqual(oracle.range_match(frame,[probe])["checked_pixels"],432)
            for corner in ((220,4),(243,4),(220,21),(243,21)):
                broken=frame.copy();broken.putpixel(corner,(255,0,255))
                self.assertIsNone(oracle.range_match(broken,[probe]))
        bare=Image.new("RGB",(320,200));bare.paste(plain,(220,4))
        self.assertIsNone(oracle.range_match(bare,[probe]))

    def test_initial_summary_is_complete_foreground_over_actor_and_disappears_after_next_key(self):
        for world,camera,position in (((10,13),(4,11),(160,9)),((11,13),(4,11),(9,9))):
            source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes(world)
            source=oracle.view(source,world,camera);at=runner.screen(source,world)
            panel=oracle.selection_summary(self.original,source,self.assets,0)
            self.assertEqual(panel["position"],position);self.assertEqual(panel["image"].size,(149,42))
            image=oracle.actor_focus_resources(self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
            bare=Image.new("RGB",(320,200));bare.paste(image.convert("RGB"),(at[0],at[1]-6))
            shown=bare.copy();shown.paste(panel["image"].convert("RGB"),position)
            found=oracle.focus_match(shown,self.original,source,self.assets,summary=panel)
            self.assertEqual(found["unit_summary"]["full_resource"]["checked_pixels"],6258)
            self.assertTrue(found["unit_summary"]["foreground_composited_over_actor"])
            self.assertFalse(found["full_actor_compared"])
            if world==(10,13):self.assertIsNone(oracle.focus_match(shown,self.original,source,self.assets))
            for corner in (position,(position[0]+148,position[1]+41)):
                bad=shown.copy();bad.putpixel(corner,(255,0,255))
                self.assertIsNone(oracle.focus_match(bad,self.original,source,self.assets,summary=panel))
            self.assertIsNone(oracle.focus_match(bare,self.original,source,self.assets,summary=panel))
            self.assertIsNotNone(oracle.focus_match(bare,self.original,source,self.assets))

    def _edge_focus_frame(self,world,camera,acted=False):
        source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes(world)
        source[UNITS+5]=0x80 if acted else 0;source=oracle.view(source,world,camera)
        image=oracle.actor_focus_resources(self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
        at=runner.screen(source,world);piece=oracle.visible_resource(image,(at[0],at[1]-6))
        frame=Image.new("RGB",(320,200));frame.paste(piece[0].convert("RGB"),piece[1])
        return source,frame

    def test_viewport_edges_have_unique_focus_WAIT_acted_and_phase_ready_without_border_pixels(self):
        for world,camera in (((0,0),(0,0)),((23,0),(11,0)),((0,23),(0,16)),((23,23),(11,16)),((15,0),(5,0))):
            for acted in (False,True):
                source,frame=self._edge_focus_frame(world,camera,acted)
                before=bytes(source);hit=oracle.focus_match(frame,self.original,source,self.assets)
                self.assertIsNotNone(hit,(world,acted));self.assertEqual(hit["full_actor_compared"],world[1]!=0)
                found=oracle.actor_browse_anywhere(frame,self.original,source,self.assets,0,acted=acted)
                self.assertIsNotNone(found,(world,acted));self.assertEqual(found[1]["camera"],list(camera))
                self.assertIsNone(oracle.actor_browse_anywhere(frame,self.original,source,self.assets,0,acted=not acted))
                if acted:self.assertGreaterEqual(found[1]["visible_acted_witness"]["checked_pixels"],24)
                outside=frame.copy()
                for box in ((0,0,320,4),(0,196,320,200),(0,0,4,200),(316,0,320,200)):outside.paste((255,0,255),box)
                self.assertIsNotNone(oracle.focus_match(outside,self.original,source,self.assets))
                self.assertIsNone(oracle.actor_browse_anywhere(Image.new("RGB",(320,200)),self.original,source,self.assets,0,acted=acted))
                self.assertEqual(bytes(source),before)

    def _edge_command_frame(self,source,selected,disabled):
        frame=Image.new("RGB",(320,200));at=runner.screen(source,oracle.actor(source,0)[:2])
        for item,offset in enumerate(oracle.MENU_OFFSETS):
            image=self.assets["commands"][3*item+(2 if disabled[item] else int(item==selected))]
            visible=oracle.visible_resource(image,tuple(a+b for a,b in zip(at,offset)))
            if visible:frame.paste(visible[0].convert("RGB"),visible[1],visible[0].point(lambda p:255 if p else 0,mode="L"))
        actor_image=oracle.actor_focus_resources(self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
        visible=oracle.visible_resource(actor_image,(at[0],at[1]-6))
        frame.paste(visible[0].convert("RGB"),visible[1],visible[0].point(lambda p:255 if p else 0,mode="L"))
        return frame

    def test_edge_COMMAND_reports_only_observable_capabilities_and_visible_desired_bright(self):
        disabled=[1,1,0,0]
        for world,camera in (((0,0),(0,0)),((23,0),(11,0)),((0,23),(0,16)),((23,23),(11,16)),((15,0),(5,0))):
            source,_=self._edge_focus_frame(world,camera)
            frame=self._edge_command_frame(source,2,disabled)
            result=oracle.command_match(frame,self.original,source,self.assets,0,2,disabled)
            self.assertIsNotNone(result,world);self.assertFalse(result["full_four_icons_compared"])
            self.assertFalse(result["capability_gates_compared"])
            for item in set(range(4))-set(result["observable_indices"]):
                self.assertIsNone(result["matches"][item]["frame"]);self.assertIsNone(result["observed_disabled"][item])
            if 2 not in result["observable_indices"]:
                self.assertIsNone(result["selected"]);self.assertTrue(result["default_unobserved"])
            self.assertFalse(oracle.menu_absent(frame,self.assets,runner.screen(source,world)))
            self.assertIsNone(oracle.focus_match(frame,self.original,source,self.assets))
            wait=self._edge_command_frame(source,3,disabled)
            desired=oracle.command_match(wait,self.original,source,self.assets,0,3)
            self.assertEqual(desired is not None,world[1]!=23)
        top,_=self._edge_focus_frame((15,0),(5,0));shown=self._edge_command_frame(top,3,disabled)
        result=oracle.command_match(shown,self.original,top,self.assets,0,3)
        self.assertEqual(result["matches"][3]["frame"],10)
        wrong_cap=self._edge_command_frame(top,2,disabled);image=self.assets["commands"][3]
        wrong_cap.paste(image.convert("RGB"),(220,6),image.point(lambda p:255 if p else 0,mode="L"))
        self.assertIsNone(oracle.command_match(wrong_cap,self.original,top,self.assets,0,2,disabled))
        image=self.assets["commands"][10]
        opaque=next(i for i,pixel in enumerate(image.getdata()) if pixel)
        broken=shown.copy();broken.putpixel((244+opaque%image.width,28+opaque//image.width),(255,0,255))
        self.assertIsNone(oracle.command_match(broken,self.original,top,self.assets,0,3))

    def test_unobservable_desired_business_icon_stops_before_any_navigation_or_selection_key(self):
        cases=(("attack",(15,0)),("wait",(15,23)),("magic",(0,15)),("herb",(23,15)),("herb",(15,0)))
        for kind,destination in cases:
            action={"kind":kind,"actor_raw":0,"destination":list(destination),"reachable_path_directions":[]}
            capture=SimpleNamespace(key=Mock())
            with patch.object(runner,"navigate") as navigate:
                with self.assertRaisesRegex(runner.NaturalStopped,"不可完整观察"):
                    runner.perform_action(capture,self.original,self.source,action,self.assets,[],None,Mock(),{})
            capture.key.assert_not_called();navigate.assert_not_called()
        action={"kind":"wait","actor_raw":0,"destination":[15,0],"reachable_path_directions":[]}
        capture=SimpleNamespace(key=Mock())
        with patch.object(runner,"range_probe",side_effect=TimeoutError("after visible desired preflight")):
            with self.assertRaisesRegex(TimeoutError,"visible desired"):
                runner.perform_action(capture,self.original,self.source,action,self.assets,[],None,Mock(),{})
        capture.key.assert_not_called()

    def test_top_caster_visible_commit_still_distinguishes_all_unacted_and_bottom_row_range_is_full(self):
        source=bytearray(self.source);source[META+1]=2;source[UNITS:UNITS+2]=bytes((15,0))
        source[UNITS+STRIDE:UNITS+2*STRIDE]=self.source[UNITS+4*STRIDE:UNITS+5*STRIDE]
        source[UNITS+STRIDE:UNITS+STRIDE+2]=bytes((15,1));source[UNITS+STRIDE+5]=0
        source=oracle.view(source,(15,1),(5,0))
        probes=oracle.range_probe(self.original,source,self.assets,0,radius=1,minimum=1)
        resources=oracle.action_completion_resources(self.original,source,self.assets,0,[(self.assets["cursor"],0)],probes)
        self.assertFalse(resources["full_actor_compared"]);self.assertEqual(resources["sprite_crop"],[0,6,24,24])
        frame=Image.new("RGB",(320,200));probe=probes[0]
        l,t,w,h=probe["range_roi"];x,y,_,_=probe["tile_roi"]
        frame.paste(probe["base"].crop((l-x,t-y,l-x+w,t-y+h)).convert("RGB"),(l,t))
        cursor=self.assets["cursor"];at=runner.screen(source,(15,1))
        frame.paste(cursor.convert("RGB"),at,cursor.point(lambda p:255 if p else 0,mode="L"))
        sprite=resources["sprites"][0];frame.paste(sprite.convert("RGB"),resources["position"],sprite.point(lambda p:255 if p else 0,mode="L"))
        found=oracle.action_completion_match(frame,source,self.assets,resources)
        self.assertIsNotNone(found);self.assertFalse(found["full_actor_compared"])
        self.assertEqual(found["commit_witness"],"visible caster acted sprite")
        self.assertGreaterEqual(found["caster_acted_sprite"]["checked_pixels"],24)
        bad=frame.copy();normal=resources["unacted_sprites"][0]
        bad.paste(normal.convert("RGB"),resources["position"],normal.point(lambda p:255 if p else 0,mode="L"))
        not_done=oracle.action_completion_match(bad,source,self.assets,resources)
        self.assertIsNone(not_done["caster_acted_sprite"]);self.assertIsNotNone(not_done["caster_unacted_sprite"])
        bottom=bytearray(self.source);bottom[META+1]=3;bottom[UNITS:UNITS+2]=bytes((15,23));bottom[UNITS+STRIDE+5]=1
        bottom[UNITS+2*STRIDE:UNITS+2*STRIDE+2]=bytes((15,22));bottom=oracle.view(bottom,(15,23),(9,16))
        rows=oracle.range_probe(self.original,bottom,self.assets,0)
        self.assertTrue(rows);self.assertTrue(all(p["range_roi"][1:]==[172,24,24] for p in rows))
        probe=rows[0];shown=Image.new("RGB",(320,200));shown.paste(probe["ranges"][0][1].convert("RGB"),probe["position"])
        self.assertEqual(oracle.range_match(shown,rows)["checked_pixels"],576)
        shown.paste(probe["base"].convert("RGB"),probe["position"]);self.assertIsNone(oracle.range_match(shown,rows))

    def test_selection_summary_pixels_cannot_satisfy_independent_range_gate(self):
        source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes((10,13))
        source=oracle.view(source,(10,13),(4,11));panel=oracle.selection_summary(self.original,source,self.assets,0)
        probe=next(p for p in oracle.range_probe(self.original,source,self.assets,0) if p["world"]==[10,12])
        x,y,width,height=probe["range_roi"];self.assertLessEqual(x+width,160)
        image=oracle.actor_focus_resources(self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
        frame=Image.new("RGB",(320,200));frame.paste(probe["ranges"][0][1].convert("RGB"),(x,y))
        frame.paste(image.convert("RGB"),(148,46));frame.paste(panel["image"].convert("RGB"),panel["position"])
        self.assertIsNotNone(oracle.focus_match(frame,self.original,source,self.assets,summary=panel))
        self.assertIsNotNone(oracle.range_match(frame,[probe]))
        plain=frame.copy();plain.paste(probe["base"].crop((0,0,width,height)).convert("RGB"),(x,y))
        self.assertIsNotNone(oracle.focus_match(plain,self.original,source,self.assets,summary=panel))
        self.assertIsNone(oracle.range_match(plain,[probe]))

    def test_completed_heal_HP_observation_rebuilds_whole_summary_without_source_writes(self):
        import struct
        source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes((10,13))
        struct.pack_into("<H",source,UNITS+0x40,9)
        source=oracle.view(source,(10,13),(4,11));before=bytes(source)
        permit={"raw":0,"record_hex":oracle.actor(source,0).hex(),"actual_completed_heal":True,"kind":"herb192/code5"}
        local=bytearray(source);struct.pack_into("<H",local,UNITS+0x40,42)
        shown_panel=oracle.selection_summary(self.original,local,self.assets,0)
        sprite=oracle.actor_focus_resources(self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
        frame=Image.new("RGB",(320,200));frame.paste(sprite.convert("RGB"),(148,46))
        frame.paste(shown_panel["image"].convert("RGB"),shown_panel["position"])
        fixed=oracle.selection_summary(self.original,source,self.assets,0)
        self.assertIsNone(oracle.focus_match(frame,self.original,source,self.assets,summary=fixed))
        allowed=oracle.selection_summary(self.original,source,self.assets,0,hp_observation=permit)
        found=oracle.focus_match(frame,self.original,source,self.assets,summary=allowed)
        self.assertEqual(found["unit_summary"]["full_resource"]["checked_pixels"],6258)
        fields=found["unit_summary"]["fields"]
        self.assertEqual((fields["source_HP_raw"],fields["observed_HP"]),(9,42))
        self.assertFalse(fields["source_HP_expected_compared"]);self.assertFalse(fields["source_record_compared"])
        self.assertTrue(fields["other_source_fields_compared"]);self.assertNotIn("HP_raw",fields)
        self.assertEqual(bytes(source),before)
        px,py=shown_panel["position"]
        for x,y in ((px+128,py+23),(px+21,py+22),(px+6,py+5),(px+134,py+5),(px+148,py+41)):
            bad=frame.copy();bad.putpixel((x,y),(255,0,255))
            self.assertIsNone(oracle.focus_match(bad,self.original,source,self.assets,summary=allowed),(x,y))
        for bad_permit in ({**permit,"raw":1},{**permit,"record_hex":"00"*80},{**permit,"actual_completed_heal":False}):
            with self.assertRaisesRegex(ValueError,"本轮实际完成治疗"):
                oracle.selection_summary(self.original,source,self.assets,0,hp_observation=bad_permit)

    def test_observed_HP_rejects_overmax_and_still_requires_fixed_MP_level_and_name(self):
        import struct
        source=bytearray(self.source);source[META+1]=1;source[UNITS:UNITS+2]=bytes((10,13))
        struct.pack_into("<H",source,UNITS+0x40,9);source=oracle.view(source,(10,13),(4,11))
        permit={"raw":0,"record_hex":oracle.actor(source,0).hex(),"actual_completed_heal":True}
        expected=oracle.selection_summary(self.original,source,self.assets,0,hp_observation=permit)
        sprite=oracle.actor_focus_resources(self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
        for offset,value in ((0x40,43),(0x44,1),(0x21,2),(8,9)):
            local=bytearray(source)
            if offset in (0x40,0x44):struct.pack_into("<H",local,UNITS+offset,value)
            else:local[UNITS+offset]=value
            if offset==0x40:struct.pack_into("<H",local,UNITS+0x42,100)
            actual=oracle.selection_summary(self.original,local,self.assets,0)
            frame=Image.new("RGB",(320,200));frame.paste(sprite.convert("RGB"),(148,46))
            frame.paste(actual["image"].convert("RGB"),actual["position"])
            self.assertIsNone(oracle.observed_summary(frame,self.original,source,self.assets,expected) if offset==0x40 else
                oracle.focus_match(frame,self.original,source,self.assets,summary=expected))
        local=bytearray(source);struct.pack_into("<H",local,UNITS+0x42,1000)
        with self.assertRaisesRegex(ValueError,"最大HP"):
            oracle.selection_summary(self.original,local,self.assets,0,hp_observation={**permit,"record_hex":oracle.actor(local,0).hex()})

    def test_only_actual_completed_heal_target_gets_same_turn_REST_display_permission(self):
        primary=copy.deepcopy(self.plan["rest_tail"][0]);primary.update(kind="herb",item_slot=2,target_raw=1)
        plans={"next_action":primary,"rest_tail":copy.deepcopy(self.plan["rest_tail"][1:])}
        prepared={"assets":self.assets,"opening":[],"titles":{3:[None]},"geometry":{},"root":ROOT}
        with tempfile.TemporaryDirectory() as directory:
            save=Path(directory)/"actual.SAV";shutil.copy2(self.original/"START.SAV",save)
            before,stamp=save.read_bytes(),save.stat().st_mtime_ns
            seen=[]
            def perform(*args,**kwargs):
                seen.append((args[3]["actor_raw"],kwargs["hp_observation"]))
                kwargs["completion"]["actual_resource_commit"]=True
                return args[2]
            with patch.object(runner,"wait_title"),patch.object(runner,"opening_exercise"), \
                    patch.object(runner,"read_plan",return_value=plans),patch.object(runner,"ProgressReader"), \
                    patch.object(runner,"cursor_bank",return_value=[]),patch.object(runner,"perform_action",side_effect=perform), \
                    patch.object(runner,"wait_progress",return_value=self.source),patch.object(runner,"end_phase",return_value=self.source), \
                    patch.object(runner,"save_active",side_effect=TimeoutError("stop after this completed round")):
                with self.assertRaisesRegex(TimeoutError,"completed round"):
                    runner.exercise(SimpleNamespace(artifacts=Path(directory)),self.original,Path("planner"),prepared,save,[before,stamp],{"title_menu_count":3,"checkpoints":[]},1)
            self.assertEqual([raw for raw,_ in seen],[0,1,2]);self.assertIsNone(seen[0][1]);self.assertIsNone(seen[2][1])
            self.assertEqual(seen[1][1]["raw"],1);self.assertTrue(seen[1][1]["actual_completed_heal"])
            self.assertEqual(seen[1][1]["record_hex"],oracle.actor(self.source,1).hex())
            self.assertEqual(save.read_bytes(),before);self.assertEqual(save.stat().st_mtime_ns,stamp)
            seen.clear()
            with patch.object(runner,"wait_title"),patch.object(runner,"opening_exercise"), \
                    patch.object(runner,"read_plan",return_value=plans),patch.object(runner,"ProgressReader"), \
                    patch.object(runner,"cursor_bank",return_value=[]),patch.object(runner,"perform_action",side_effect=perform), \
                    patch.object(runner,"wait_progress",side_effect=TimeoutError("actual effect not completed")):
                with self.assertRaisesRegex(TimeoutError,"not completed"):
                    runner.exercise(SimpleNamespace(artifacts=Path(directory)),self.original,Path("planner"),prepared,save,[before,stamp],{"title_menu_count":3,"checkpoints":[]},1)
            self.assertEqual(seen,[(0,None)])

    def test_stationary_herb_request_needs_empty_query_path_and_complete_command_before_business(self):
        source=self._crowded_stationary_hero()
        action={"kind":"herb","actor_raw":0,"destination":[12,14],"reachable_path_directions":[],
            "item_slot":2,"target_raw":0,"command_disabled":[0,1,0,0],"command_first_enabled":0,
            "target_query":{"cast":1,"minimum":0,"area":0,"filter":1,"cursor_resource_frame":1}}
        focused=Image.new("RGB",(320,200));image=oracle.actor_focus_resources(
            self.original,source,self.assets,0,self.assets["cursor"])["templates"][0]
        focused.paste(image.convert("RGB"),(220,22));observed=[];capture=SimpleNamespace(key=Mock())
        panel=oracle.selection_summary(self.original,source,self.assets,0)
        focused.paste(panel["image"].convert("RGB"),panel["position"])
        def checkpoint(name,predicate,**kwargs):
            found=predicate(focused);observed.append((name,found))
            if found is None:raise TimeoutError(name)
            return focused,found
        with self.assertRaisesRegex(TimeoutError,"command"):
            runner.perform_action(capture,self.original,source,action,self.assets,[],SimpleNamespace(),checkpoint,{})
        request=observed[0][1];self.assertEqual(observed[0][0],"stationary-selection-request")
        self.assertFalse(request["movement_range_compared"]);self.assertFalse(request["selection_state_proven"])
        self.assertEqual([c.args[0] for c in capture.key.call_args_list],["ENTER","ENTER"])
        for path in (None,[0]):
            invalid=dict(action);invalid.pop("reachable_path_directions")
            if path is not None:invalid["reachable_path_directions"]=path
            capture.key.reset_mock()
            with self.assertRaises(ValueError):runner.perform_action(capture,self.original,source,invalid,self.assets,[],
                SimpleNamespace(),checkpoint,{})
            capture.key.assert_not_called()

    def test_stationary_wait_without_neighbor_range_defers_selection_proof_to_command(self):
        source,action,normal=self._rest_command_fixture((2,3,6,9))
        source=bytearray(source);source[UNITS+STRIDE+0x3b]=0
        self.assertEqual(oracle.range_probe(self.original,source,self.assets,1,allow_empty=True),[])
        with self.assertRaises(ValueError):oracle.range_probe(self.original,source,self.assets,1)
        focused=Image.new("RGB",(320,200))
        image=oracle.actor_focus_resources(self.original,source,self.assets,1,self.assets["cursor"])["templates"][1]
        focused.paste(image.convert("RGB"),(244,94))
        panel=oracle.selection_summary(self.original,source,self.assets,1)
        focused.paste(panel["image"].convert("RGB"),panel["position"])
        _,_,selected=self._rest_command_fixture((2,3,6,10))
        for command_available in (False,True):
            capture=SimpleNamespace(key=Mock());reader=SimpleNamespace(special=Mock());observed=[]
            def checkpoint(name,predicate,**kwargs):
                frame=focused if name=="stationary-selection-request" else selected if name=="command-selected" else normal
                if name=="rest-command" and not command_available:frame=focused
                found=predicate(frame)
                if found is None:raise TimeoutError(name)
                observed.append((name,found));return frame,found
            if command_available:
                runner.perform_action(capture,self.original,source,action,self.assets,[],reader,checkpoint,
                    {"native_audio":{"enabled":False}},fresh=False)
                request=observed[0][1]
                self.assertFalse(request["range_compared"]);self.assertFalse(request["selection_state_proven"])
                self.assertNotIn("range",request)
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],["ENTER","ENTER","DOWN","ENTER"])
            else:
                with self.assertRaises(TimeoutError):runner.perform_action(capture,self.original,source,action,self.assets,[],reader,checkpoint,
                    {"native_audio":{"enabled":False}},fresh=False)
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],["ENTER","ENTER"])
                reader.special.assert_not_called()
        # 非零预算但四邻全占，也允许返回空query，不把它解释为移动非法。
        packed=bytearray(self.source);packed[META+1]=5
        for raw,xy in enumerate(((7,14),(6,14),(8,14),(7,13),(7,15))):
            packed[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]=bytes(xy);packed[UNITS+raw*STRIDE+5]&=~1
        packed=oracle.view(packed,(7,14),(3,10))
        self.assertEqual(oracle.range_probe(self.original,packed,self.assets,0,allow_empty=True),[])

    def test_all_action_range_queries_fail_before_any_navigation_or_key(self):
        source,action,_=self._rest_command_fixture((2,3,6,9))
        source=bytearray(source);source[UNITS+STRIDE+0x3b]=0
        source=oracle.view(source,(6,14),(0,11));action["destination"]=[11,15]
        capture=SimpleNamespace(key=Mock());checkpoint=Mock()
        with self.assertRaises(ValueError):runner.perform_action(capture,self.original,source,action,self.assets,[],
            SimpleNamespace(special=Mock()),checkpoint,{"native_audio":{"enabled":False}},fresh=False)
        capture.key.assert_not_called();checkpoint.assert_not_called()
        source=oracle.view(self.source,(6,14),(0,11));source[UNITS+11*STRIDE:UNITS+11*STRIDE+2]=bytes((9,15))
        action.update(kind="magic",destination=[10,15],magic_id=0,target_raw=11,
            target_query={"cast":5,"minimum":0,"area":0,"filter":0,"cursor_resource_frame":1})
        original_probe=runner.range_probe
        for fail_at in (2,3):
            queries=[]
            def probe(*args,**kwargs):
                queries.append(kwargs)
                if len(queries)==fail_at:raise ValueError("未支持目标range资源")
                return original_probe(*args,**kwargs)
            with patch.object(runner,"range_probe",side_effect=probe):
                with self.assertRaises(ValueError):runner.perform_action(capture,self.original,source,action,self.assets,[],
                    SimpleNamespace(special=Mock()),checkpoint,{"native_audio":{"enabled":False}})
            self.assertEqual(len(queries),fail_at)
            capture.key.assert_not_called();checkpoint.assert_not_called()

    def test_visible_range_rectangles_with_real_lower_panel_and_single_pixel_failures(self):
        probe=oracle.range_probe(self.original,self.source,self.assets,0)[0]
        phase,tile=next((phase,image) for phase,image in probe["ranges"] if image.size==(24,24))
        sheet=oracle.archive_entry((self.original/"FDOTHER.DAT").read_bytes(),5)
        import struct
        start,end=struct.unpack_from("<II",sheet,6+130*4)
        panel=oracle.decode_image(sheet[start:end]);panel.putpalette(self.assets["palette"])
        self.assertEqual(panel.size,(69,34))
        for at,panel_at in (((28,148),(5,161)),((268,148),(246,161))):
            box=oracle._range_rectangle(at,[(*panel_at,69,34)])
            self.assertEqual(box,(at[0],148,at[0]+24,161))
            image=tile.crop((0,0,24,13))
            candidate=dict(probe,position=at,range_roi=[*at,24,13],ranges=[(phase,image)])
            frame=Image.new("RGB",(320,200));frame.paste(tile.convert("RGB"),at)
            frame.paste(panel.convert("RGB"),panel_at)
            self.assertEqual(oracle.range_match(frame,[candidate])["checked_pixels"],312)
            for offset in ((0,0),(23,0),(0,12),(23,12)):
                bad=frame.copy();bad.putpixel((at[0]+offset[0],at[1]+offset[1]),(255,0,255))
                self.assertIsNone(oracle.range_match(bad,[candidate]))

    def test_empty_menu_cell_is_observed_from_full_current_pixels_not_saved_occupancy(self):
        probes=oracle.range_probe(self.original,self.source,self.assets,0)
        probe=next(p for p in probes if 2<=(p["position"][0]-4)//24<=10 and 2<=(p["position"][1]-4)//24<=5)
        frame=Image.new("RGB",(320,200));frame.paste(probe["base"].convert("RGB"),probe["position"])
        result=oracle.empty_tile(self.original,self.source,self.assets,frame)
        self.assertEqual(result["world"],tuple(probe["world"]))
        self.assertEqual(result["observed_empty_tile"]["checked_pixels"],576)
        frame.putpixel(probe["position"],(255,0,255))
        with self.assertRaises(ValueError):oracle.empty_tile(self.original,self.source,self.assets,frame)

    def test_reward_wait_matches_only_confirmed_growth_amount_and_rejects_late_glyph(self):
        pages=oracle.required_reward_pages(self.original,self.assets,self.source)
        self.assertTrue(pages)
        for page in pages:
            frame=page_frame(page,self.assets)
            from fd2_reward_oracle import page_match
            self.assertIsNotNone(runner.sparse_match(frame,page,page_match,self.assets))
            index=next(i for i,p in reversed(list(enumerate(page["text"].getdata()))) if p==205)
            frame.putpixel((index%320,index//320),(255,0,255))
            self.assertIsNone(runner.sparse_match(frame,page,page_match,self.assets))

    def test_story_readset_includes_whole_victory_and_future_raw_speakers(self):
        pages=oracle.stage0_pages(self.original,self.assets)
        self.assertEqual(len([p for p in pages if p["fragment"]==9]),15)
        self.assertTrue(any(p["speaker_control"]==-19 and p["speaker_argument"]==16 for p in pages))
        for page in pages:
            frame=page_frame(page,self.assets)
            self.assertIsNotNone(runner.sparse_match(frame,page,runner.story_match,self.assets))

    def test_native_stage0_group_order_is_not_metadata_template_order(self):
        # 34531:112a5(1)只加persistent，再active group3/7；随后3460b、
        # 34673、346cd分别group4/5/6。原模板t8=3/group7先于t9=1/group3，
        # 直接拼模板会把raw12/13倒置；group10/11本首关链从未追加。
        expected=[0,9,4,30]+[96]*8+[1,3]+[96]*4+[97,96,96,96,96]+[68]*4
        with patch.object(oracle,"build_pages",return_value=[]) as build:
            oracle.stage0_pages(self.original,self.assets)
        kwargs=build.call_args.kwargs
        self.assertEqual(kwargs["identities_by_stage"][0],expected)
        self.assertEqual(kwargs["identities_by_stage"][0][12:14],[1,3])
        self.assertEqual(kwargs["identities_by_stage"][0][15:17],[96,96])
        self.assertEqual(kwargs["stage_plan"],((0,12),))
        self.assertEqual(kwargs["text_identities_by_stage"][0][97],97)

    def test_first_reinforcement_wait_is_native_fragment11_before_fragment3(self):
        from fd2_opening_oracle import fragments
        from rebuild_fd2_analysis import parse_le,parse_objects,build_relbase_image
        # 独立固定原函数完整字节/资源事实；不是由待测page模型生成的期望。
        exe=(self.original/"FD2.EXE").read_bytes();le=parse_le(exe)
        image=build_relbase_image(exe,le,parse_objects(exe,le))
        self.assertEqual(hashlib.sha256(image[0x34531:0x3460b]).hexdigest(),
            "40fe6618481b2bde268a4f1b776cebdcaf937fd80436de5c5d52c1864573c64a")
        raw=dict(fragments((self.original/"FDTXT.DAT").read_bytes(),0,12))
        self.assertEqual(raw[11],[-17,1,558,603,667,342,603,667,342,-1])
        pages=oracle.stage0_pages(self.original,self.assets)
        self.assertEqual({p["fragment"] for p in pages},{3,4,5,6,7,8,9,11})
        self.assertEqual(len(pages),54)
        first=next(p for p in pages if p["fragment"]==11)
        self.assertEqual((first["speaker_control"],first["speaker_argument"],first["portrait_id"]),(-17,1,1))
        self.assertEqual((first["portrait_position"],first["border_position"],first["arrow_position"]),
            ((232,5),(5,2),None))
        self.assertEqual(first["known_text_pixels"],1792)
        self.assertEqual({p["portrait_id"] for p in pages if p["fragment"]==4},{96})

    def test_progress_reader_dispatches_fragment11_ack_then_fragment3(self):
        report={"resource_acknowledgements":[]}
        reader=runner.ProgressReader(self.original,self.source,self.assets,report)
        self.assertEqual(set(reader.fragments),{3,4,5,6,7,8,9,11})
        first=reader.fragments[11][0]
        detail=reader.message(page_frame(first,self.assets))
        self.assertIsNotNone(detail)
        self.assertEqual(detail["signature"],["story",11,9])
        capture=SimpleNamespace(key=Mock());checkpoint=Mock()
        reader.acknowledge(capture,detail,checkpoint)
        capture.key.assert_called_once_with("ENTER")
        self.assertIsNone(reader.story)
        self.assertFalse(reader.victory_text_complete)
        self.assertIsNone(reader.message(page_frame(first,self.assets)))
        # 原34531在fragment11结束后追加group7/script8，再进入fragment3。
        following=reader.message(page_frame(reader.fragments[3][0],self.assets))
        self.assertIsNotNone(following)
        self.assertEqual(following["signature"],["story",3,23])
        reader.acknowledge(capture,following,checkpoint)
        self.assertEqual(capture.key.call_count,2)
        self.assertEqual(reader.story[1],1)
        self.assertFalse(reader.victory_text_complete)
        checkpoint.assert_not_called()

    def test_progress_reader_covers_every_confirmed_story_sequence(self):
        reader=runner.ProgressReader(self.original,self.source,self.assets,
            {"resource_acknowledgements":[]})
        capture=SimpleNamespace(key=Mock())
        # 按原调用顺序跑真实reader分派/ACK，不只直接调用page_match自检。
        for fragment in (11,3,4,5,6,7,8,9):
            for index,page in enumerate(reader.fragments[fragment]):
                detail=reader.message(page_frame(page,self.assets))
                self.assertIsNotNone(detail,(fragment,index))
                self.assertEqual(detail["signature"],["story",fragment,page["token_offset"]])
                reader.acknowledge(capture,detail,Mock())
                self.assertIsNone(reader.message(page_frame(page,self.assets)),
                    ("same frame must not receive another ACK",fragment,index))
            self.assertIsNone(reader.story)
            self.assertEqual(reader.victory_text_complete,fragment==9)
        self.assertEqual(capture.key.call_count,54)

    def test_reward_reader_two_levels_follow_growth_order_despite_retained_rows(self):
        from fd2_reward_oracle import reward_pages
        for identity in (0,1,3,4,9,30):
            reader=runner.ProgressReader(self.original,self.source,self.assets,
                {"resource_acknowledgements":[]})
            growth=[p for p in reader.rewards if p["portrait_id"]==identity and p["fragment"]!=489]
            events=[{"fragment":488,"value":42,"line":0,"requires_key":False}]
            row=2
            for level in range(2):
                events.append({"fragment":489,"value":0,"line":1,"requires_key":True})
                for template in growth:
                    scroll=row==3
                    if scroll:row=2
                    events.append({"fragment":template["fragment"],"value":template["value"],
                        "line":row,"scroll_before":scroll,"requires_key":True})
                    row+=1
            pages=reward_pages(self.original,self.assets,identity,events)
            capture=SimpleNamespace(key=Mock())
            for page in pages[1:]:
                frame=page_frame(page,self.assets)
                detail=reader.message(frame)
                self.assertIsNotNone(detail,(identity,page["fragment"]))
                self.assertEqual(detail["signature"],["reward",identity,page["fragment"]])
                reader.acknowledge(capture,detail,Mock())
                self.assertIsNone(reader.message(frame),("stale cumulative frame",identity,page["fragment"]))
            self.assertIsNone(reader.reward)
            self.assertEqual(capture.key.call_count,2*(1+len(growth)))

    def test_reward_requires_level_ack_and_survives_arrow_or_frame_gaps(self):
        reader=runner.ProgressReader(self.original,self.source,self.assets,{"resource_acknowledgements":[]})
        level=next(p for p in reader.rewards if p["portrait_id"]==4 and p["fragment"]==489)
        growth=next(p for p in reader.rewards if p["portrait_id"]==4 and p["fragment"]==490)
        capture=SimpleNamespace(key=Mock())
        with self.assertRaises(runner.NaturalStopped):
            reader.acknowledge(capture,{"page":growth,"signature":["reward",4,490]},Mock())
        capture.key.assert_not_called()
        reader.acknowledge(capture,reader.message(page_frame(level,self.assets)),Mock())
        expected=reader.reward
        self.assertIsNone(reader.message(Image.new("RGB",(320,200))))
        self.assertIs(reader.reward,expected)
        no_arrow=page_frame(growth,self.assets)
        no_arrow.paste(tuple(self.assets["palette"][74*3:74*3+3]),(191,176,207,192))
        self.assertIsNone(reader.message(no_arrow))
        self.assertIs(reader.reward,expected)
        self.assertIsNone(reader.message(page_frame(level,self.assets)))
        with self.assertRaises(runner.NaturalStopped):
            reader.acknowledge(capture,{"page":level,"signature":["reward",4,489]},Mock())
        self.assertEqual(capture.key.call_count,1)
        self.assertEqual(reader.message(page_frame(growth,self.assets))["signature"],["reward",4,490])

    def test_reward_readset_rejects_too_few_growth_rows_for_visual_progress(self):
        tables,proof=oracle.reward_tables(self.original/"FD2.EXE")
        changed=dict(tables);data=bytearray(tables["growth"])
        for index in (1,2,3,4):data[4*11+2*index+1]=0
        changed["growth"]=bytes(data)
        with patch.object(oracle,"reward_tables",return_value=(changed,proof)):
            with self.assertRaisesRegex(ValueError,"成长项不足"):
                oracle.required_reward_pages(self.original,self.assets,self.source)

    def test_same_wait_page_never_receives_duplicate_ack_without_transition(self):
        report={"resource_acknowledgements":[]}
        reader=runner.ProgressReader(self.original,self.source,self.assets,report)
        page=next(p for p in reader.rewards if p["portrait_id"]==0 and p["fragment"]==489)
        frame=page_frame(page,self.assets);detail=reader.message(frame)
        self.assertIsNotNone(detail)
        capture=SimpleNamespace(key=Mock())
        reader.acknowledge(capture,detail,Mock())
        self.assertIsNone(reader.message(frame))
        self.assertEqual(capture.key.call_count,1)
        self.assertEqual(report["resource_acknowledgements"][0]["key"],"ENTER")

    def test_victory_completion_does_not_require_nonexistent_next_round_banner(self):
        reader=SimpleNamespace(message=Mock(return_value=None),victory_text_complete=True)
        capture=SimpleNamespace(artifacts=Path("unused"))
        def checkpoint(name,predicate,**kwargs):return Image.new("RGB",(320,200)),predicate(Image.new("RGB",(320,200)))
        report={"kind":"original","native_round_banners":[]}
        result=runner.wait_progress(capture,self.original,self.source,1,self.assets,reader,report,checkpoint,phase=True)
        self.assertIsNone(result);self.assertEqual(report["native_round_banners"],[])

    def test_missing_planner_and_immutable_output_rejected_before_mkdir(self):
        with tempfile.TemporaryDirectory() as temporary:
            output=Path(temporary)/"out"
            args=SimpleNamespace(root=ROOT,artifacts=output,planner=Path("absent-planner"),sdl=None,max_turns=50)
            with self.assertRaises(ValueError):runner.run(args)
            self.assertFalse(output.exists())
        args=SimpleNamespace(root=ROOT,artifacts=self.original/"forbidden")
        with self.assertRaises(ValueError):runner.run(args)

    def test_each_wait_magic_and_herb_key_has_resource_ack_before_next_key(self):
        class Capture:
            def __init__(self):self.pending=False;self.keys=[]
            def key(self,key):
                if self.pending:raise AssertionError("two keys without a resource checkpoint")
                self.pending=True;self.keys.append(key)
        for kind in ("wait","magic","herb"):
            source=bytearray(self.source)
            action=copy.deepcopy(self.plan["next_action"])
            if kind=="magic":
                raw=1;source[UNITS+11*STRIDE:UNITS+11*STRIDE+2]=bytes((9,15))
                action.update(kind=kind,actor_raw=raw,destination=list(oracle.actor(source,raw)[:2]),
                    magic_id=0,target_raw=11,target_query={"cast":5,"minimum":0,"area":0,"filter":0,"cursor_resource_frame":1},
                    command_disabled=[0,0,0,0],command_first_enabled=0)
            elif kind=="herb":
                action.update(kind=kind,actor_raw=0,destination=[7,14],item_slot=2,target_raw=0,
                    target_query={"cast":1,"minimum":0,"area":0,"filter":1,"cursor_resource_frame":1})
            capture=Capture();checkpoints=[]
            def checkpoint(name,predicate,**kwargs):
                capture.pending=False;checkpoints.append(name);return None,{"kind":"protocol_only_not_pixel_acceptance"}
            report={"native_audio":{"enabled":False}}
            runner.perform_action(capture,self.original,source,action,self.assets,[],SimpleNamespace(special=Mock()),checkpoint,report)
            self.assertTrue(capture.pending) # 最后一次效果确认由调用者开始观察，不补发任何键。
            self.assertEqual(capture.keys[-1],"ENTER")
            self.assertIn("command-selected",checkpoints)
            if kind!="wait":self.assertIn("target-final",checkpoints)
            if kind=="herb":
                self.assertIn("item-list2",checkpoints);self.assertIn("item-menu-use-selected",checkpoints)

    def test_action_first_resource_failure_does_not_send_confirmation(self):
        capture=SimpleNamespace(key=Mock())
        action=copy.deepcopy(self.plan["next_action"])
        with self.assertRaisesRegex(TimeoutError,"first resource"):
            runner.perform_action(capture,self.original,self.source,action,self.assets,[],SimpleNamespace(special=Mock()),
                Mock(side_effect=TimeoutError("first resource")),{"native_audio":{"enabled":False}})
        self.assertEqual(capture.key.call_count,1)
        self.assertNotEqual(capture.key.call_args.args,("ENTER",)) # 最初只向已知actor导航；失败不确认。

    def test_herb_menu_requires_actual_actor_over_upper_icon(self):
        action={"kind":"herb","actor_raw":0,"destination":[7,14],"item_slot":2,"target_raw":0,
            "command_disabled":[1,1,0,0],"command_first_enabled":2,
            "target_query":{"cast":1,"minimum":0,"area":0,"filter":1,"cursor_resource_frame":1}}
        predicates={}
        def checkpoint(name,predicate,**kwargs):predicates[name]=predicate;return None,{}
        runner.perform_action(SimpleNamespace(key=Mock()),self.original,self.source,action,self.assets,[],
            SimpleNamespace(special=Mock()),checkpoint,{"native_audio":{"enabled":False}})
        from run_original_battle_menu import MENU_OFFSETS,match_indexed
        from run_original_comparison import archive_entry
        from run_original_player import sheet_frame,upper_command_composite
        at=runner.screen(self.source,(7,14));icons=(self.original/"FDICON.B24").read_bytes()
        for phase in range(12):
            for use in (24,25,26):
                frame=Image.new("RGB",(320,200))
                for index,which in enumerate((use,29,30,33)):
                    image=self.assets["commands"][which]
                    if index==0:
                        sprite=sheet_frame(icons,phase);sprite.putpalette(self.assets["palette"])
                        image=upper_command_composite(image,sprite)
                    frame.paste(image.convert("RGB"),(at[0]+MENU_OFFSETS[index][0],at[1]+MENU_OFFSETS[index][1]))
                initial=predicates["item-menu0"](frame);bright=predicates["item-menu-use-selected"](frame)
                self.assertEqual(initial is not None,use!=26)
                self.assertEqual(bright is not None,use==25)
                if initial:
                    self.assertIsNone(initial["selected"]);self.assertFalse(initial["default_selection_compared"])
                if use==25:
                    self.assertFalse(match_indexed(frame,self.assets["commands"][25],(at[0],at[1]-20))["ok"],"裸图会误拒真实actor遮挡")
                if use==24:normal=frame
        capture=SimpleNamespace(key=Mock())
        def checkpoint(name,predicate,**kwargs):
            if name in ("item-menu0","item-menu-use-selected"):
                result=predicate(normal)
                if result is None:raise TimeoutError(name)
                return normal,result
            return None,{}
        with self.assertRaisesRegex(TimeoutError,"item-menu-use-selected"):
            runner.perform_action(capture,self.original,self.source,action,self.assets,[],
                SimpleNamespace(special=Mock()),checkpoint,{"native_audio":{"enabled":False}})
        self.assertEqual(capture.key.call_args.args,("UP",)) # 严格亮25失败后没有列表确认ENTER。

    def _rest_command_fixture(self, frames):
        from run_original_battle_menu import MENU_OFFSETS
        source=oracle.view(self.source,(10,15),(0,11))
        action={"kind":"wait","actor_raw":1,"destination":[10,15],"reachable_path_directions":[],
            "command_disabled":[1,0,0,0],"command_first_enabled":1}
        assets=actor_view(self.original,source,self.assets,1)
        frame=Image.new("RGB",(320,200))
        for index,which in enumerate(frames):
            image=assets["upper_commands"][which][1] if index==0 else assets["commands"][which]
            at=tuple(a+b for a,b in zip((244,100),MENU_OFFSETS[index]))
            frame.paste(image.convert("RGB"),at)
        return source,action,frame

    def test_rest_command_accepts_complete_normal_phase_without_claiming_default(self):
        source,action,frame=self._rest_command_fixture((2,3,6,9))
        predicates={}
        class Captured(Exception):pass
        def checkpoint(name,predicate,**kwargs):
            predicates[name]=predicate
            if name=="rest-command":raise Captured()
            return None,{}
        capture=SimpleNamespace(key=Mock())
        with self.assertRaises(Captured):
            runner.perform_action(capture,self.original,source,action,self.assets,[],
                SimpleNamespace(special=Mock()),checkpoint,{"native_audio":{"enabled":False}},fresh=False)
        predicate=predicates["rest-command"]
        self.assertTrue(all(oracle.command_match(frame,self.original,source,self.assets,1,selected) is None
                            for selected in range(4)),"旧唯一亮帧假设会拒合法normal相位")
        result=predicate(frame)
        self.assertEqual([row["frame"] for row in result["matches"]],[2,3,6,9])
        self.assertEqual(sum(row["checked_pixels"] for row in result["matches"]),1904)
        self.assertIsNone(result["selected"])
        self.assertFalse(result["default_selection_compared"])
        _,_,disabled=self._rest_command_fixture((2,3,6,11))
        self.assertIsNone(predicate(disabled))
        for row in result["matches"]:
            x,y=row["position"];bad=frame.copy();bad.putpixel((x+10,y+10),(255,0,255))
            self.assertIsNone(predicate(bad))
        _,_,selected=self._rest_command_fixture((2,3,6,10))
        self.assertIsNotNone(predicate(selected))

    def test_rest_explicit_down_still_requires_wait_highlight_before_enter(self):
        for acknowledged in (False,True):
            source,action,normal=self._rest_command_fixture((2,3,6,9))
            _,_,selected=self._rest_command_fixture((2,3,6,10))
            capture=SimpleNamespace(key=Mock());reader=SimpleNamespace(special=Mock())
            def checkpoint(name,predicate,**kwargs):
                if name not in ("rest-command","command-selected"):return None,{}
                frame=selected if name=="command-selected" and acknowledged else normal
                result=predicate(frame)
                if result is None:raise TimeoutError("WAIT10尚未资源ACK")
                return frame,result
            if acknowledged:
                runner.perform_action(capture,self.original,source,action,self.assets,[],reader,checkpoint,
                    {"native_audio":{"enabled":False}},fresh=False)
                self.assertEqual([call.args[0] for call in capture.key.call_args_list],["ENTER","ENTER","DOWN","ENTER"])
                reader.special.assert_called_once()
            else:
                with self.assertRaises(TimeoutError):
                    runner.perform_action(capture,self.original,source,action,self.assets,[],reader,checkpoint,
                        {"native_audio":{"enabled":False}},fresh=False)
                self.assertEqual([call.args[0] for call in capture.key.call_args_list],["ENTER","ENTER","DOWN"])
                reader.special.assert_not_called()

    def test_fresh_command_blink_off_keeps_disabled_strict_and_default_unobserved(self):
        source,_,normal=self._rest_command_fixture((2,3,6,9))
        disabled=[1,0,0,0]
        found=oracle.command_match(normal,self.original,source,self.assets,1,1,disabled)
        self.assertIsNotNone(found)
        self.assertEqual(found["expected_disabled"],disabled)
        self.assertTrue(found["capability_gates_compared"])
        self.assertTrue(found["default_unobserved"])
        self.assertFalse(found["default_selection_compared"])
        self.assertIsNone(found["selected"])
        _,_,bright=self._rest_command_fixture((2,4,6,9))
        found=oracle.command_match(bright,self.original,source,self.assets,1,1,disabled)
        self.assertEqual(found["selected"],1)
        self.assertFalse(found["default_unobserved"])
        self.assertTrue(found["default_selection_compared"])
        for frames in ((2,3,6,10),(2,4,6,10),(0,3,6,9),(2,5,6,9),(2,3,8,9)):
            _,_,invalid=self._rest_command_fixture(frames)
            self.assertIsNone(oracle.command_match(invalid,self.original,source,self.assets,1,1,disabled),frames)
        # 显式DOWN的ACK不传disabled，仍须WAIT亮帧10；normal不能据此提交。
        self.assertIsNone(oracle.command_match(normal,self.original,source,self.assets,1,3))

    def test_quiet_resource_window_resets_after_wgc_retry(self):
        report={"kind":"sdl","native_round_banners":[],"checkpoints":[],"native_audio":{"enabled":False}}
        reader=SimpleNamespace(message=Mock(return_value=None),victory_text_complete=False)
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);(directory/"game.log").write_text("field phase: turn=2 phase=2 \n",encoding="utf-8")
            capture=SimpleNamespace(artifacts=directory)
            ticks=iter((0.0,1.0,2.0,2.1,4.2))
            def checkpoint(name,predicate,**kwargs):
                for index in range(5):
                    if index==2:report["checkpoints"].append({"kind":"wgc_transient"})
                    result=predicate(Image.new("RGB",(320,200)))
                    if result:
                        self.assertEqual(index,4)
                        return None,result
                self.fail("quiet resource window never completed")
            with patch.object(runner,"actor_browse_anywhere",return_value=(self.source,{"kind":"actual_resource"})), \
                    patch.object(runner.time,"monotonic",side_effect=lambda:next(ticks)):
                result=runner.wait_progress(capture,self.original,self.source,1,self.assets,reader,report,checkpoint,phase=True)
            self.assertEqual(result,self.source)

    def test_friendly_target_first_raw_includes_side1_npc(self):
        source=bytearray(self.source)
        source[UNITS+1*STRIDE+6]=1
        source[UNITS+1*STRIDE:UNITS+1*STRIDE+2]=bytes((7,15))
        action={"kind":"herb","actor_raw":0,"target_query":{"cast":1,"minimum":0,"area":0,"filter":1}}
        self.assertEqual(runner.target_candidates(source,action)[:2],[0,1])
        source[UNITS+1*STRIDE+5]|=1
        self.assertNotIn(1,runner.target_candidates(source,action))

    def test_late_non_save_mtime_or_planner_drift_stops_before_sdl(self):
        for fault in ("mtime","planner"):
            with tempfile.TemporaryDirectory() as temporary:
                root=Path(temporary);shutil.copytree(self.original,root/"original_game")
                driver=root/"driver.exe";driver.write_bytes(b"pure driver")
                planner=root/"planner.exe";planner.write_bytes(b"pure planner")
                sdl=root/"sdl.exe";sdl.write_bytes(b"pure SDL")
                args=SimpleNamespace(root=root,artifacts=root/"out",planner=planner,sdl=sdl,driver=driver,dosbox=driver,max_turns=50,record_native_audio=False)
                launched=[];environments=[]
                @contextmanager
                def owned(argv,cwd,env,log,driver,receipts):
                    launched.append(argv);environments.append(env)
                    receipts.append({"pid":77,"exit_code":0,"cleaned":True,"forced_cleanup":False})
                    yield SimpleNamespace(pid=77)
                    if fault=="mtime":
                        path=root/"original_game/FDTXT.DAT";os.utime(path,ns=(path.stat().st_atime_ns,path.stat().st_mtime_ns+2_000_000_000))
                    else:planner.write_bytes(b"changed after original cleanup")
                def execute(*values):values[6]["natural_stage0_victory_dialogue_complete"]=True
                with patch.dict(os.environ,{"FD2SDL_FIELD_RNG_SEED":"1234"}),patch.object(runner,"running",side_effect=owned), \
                        patch.object(runner,"exercise",side_effect=execute),patch.object(runner,"after_victory"):
                    result=runner.run(args)
                self.assertEqual(len(launched),1)
                self.assertNotIn("FD2SDL_FIELD_RNG_SEED",environments[0])
                self.assertFalse(result["ok"])
                self.assertFalse(result["cases"][0]["probe_cases_passed"])
                self.assertTrue(result["original_unchanged"])
                self.assertFalse((args.artifacts/"sdl").exists())
                self.assertFalse(result["cases"][0]["original_all_mtimes_unchanged"] if fault=="mtime" else result["cases"][0]["planner_unchanged"])

    def test_quiet_window_starts_again_after_reward_message_ack(self):
        report={"kind":"sdl","native_round_banners":[],"checkpoints":[],"native_audio":{"enabled":False}}
        notice={"kind":"natural_required_message","signature":["reward",0,489],"page":{}}
        reader=SimpleNamespace(message=Mock(side_effect=(None,notice,None,None)),victory_text_complete=False,acknowledge=Mock())
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);(directory/"game.log").write_text("field phase: turn=2 phase=2 \n",encoding="utf-8")
            capture=SimpleNamespace(artifacts=directory)
            ticks=iter((0.0,3.0,5.1));calls=[]
            def checkpoint(name,predicate,**kwargs):
                while True:
                    result=predicate(Image.new("RGB",(320,200)))
                    calls.append(result)
                    if result:return None,result
            with patch.object(runner,"actor_browse_anywhere",return_value=(self.source,{"kind":"resource"})), \
                    patch.object(runner.time,"monotonic",side_effect=lambda:next(ticks)):
                result=runner.wait_progress(capture,self.original,self.source,1,self.assets,reader,report,checkpoint,phase=True)
            self.assertEqual([None,notice,None,calls[-1]],calls)
            self.assertEqual(result,self.source);reader.acknowledge.assert_called_once()

    def test_actual_defeat_is_checkpointed_before_stopping_and_sends_no_key(self):
        report={"kind":"sdl","native_round_banners":[],"checkpoints":[],"native_audio":{"enabled":False}}
        reader=SimpleNamespace(message=Mock(return_value={"kind":"natural_actual_defeat","resource":{"full_frame_pixels":64000}}),victory_text_complete=False)
        seen=[];capture=SimpleNamespace(key=Mock())
        def checkpoint(name,predicate,**kwargs):
            detail=predicate(Image.new("RGB",(320,200)));seen.append(detail);return None,detail
        with self.assertRaisesRegex(runner.NaturalStopped,"完整资源检查点已保存"):
            runner.wait_progress(capture,self.original,self.source,1,self.assets,reader,report,checkpoint,phase=True)
        self.assertEqual(seen[0]["resource"]["full_frame_pixels"],64000);capture.key.assert_not_called()

    def test_natural_full_title_deadline_sends_no_extra_business_skip_key(self):
        # startup-only ESC由Capture执行；此处仅断言exercise不另发跨页skip键。
        capture=SimpleNamespace(key=Mock())
        prepared={"assets":self.assets,"opening":[],"titles":{3:[Image.new("RGB",(320,200))]},"geometry":{}}
        with patch.object(runner,"wait_title") as title,patch.object(runner,"opening_exercise",side_effect=TimeoutError("after title")):
            with self.assertRaisesRegex(TimeoutError,"after title"):
                runner.exercise(capture,self.original,Path("planner"),prepared,Path("save"),[b"",0],{"title_menu_count":3},50)
        self.assertEqual(title.call_args.kwargs,{"timeout":90});capture.key.assert_not_called()

    def _target_completion_fixture(self, acted=True):
        source=bytearray(self.source);source[META+1]=1
        source[UNITS:UNITS+2]=bytes((10,13));source[UNITS+7]=3;source[UNITS+8]=3
        source=oracle.view(source,(10,14),(1,11))
        probe=next(p for p in oracle.range_probe(self.original,source,self.assets,0,radius=1,minimum=1)
                   if p["world"]==[9,13])
        resources=oracle.action_completion_resources(self.original,source,self.assets,0,
                    [(self.assets["cursor"],0)],[probe])
        frame=Image.new("RGB",(320,200))
        frame.paste(probe["base"].convert("RGB"),probe["position"])
        cursor=self.assets["cursor"]
        frame.paste(cursor.convert("RGB"),(220,76),cursor.point(lambda p:255 if p else 0,mode="L"))
        sprite=resources["sprites" if acted else "unacted_sprites"][0]
        frame.paste(sprite.convert("RGB"),resources["position"],sprite.point(lambda p:255 if p else 0,mode="L"))
        return source,resources,frame,probe

    def test_declared_target_completion_uses_full_acted_sprite_not_hidden_or_hp(self):
        source,resources,frame,probe=self._target_completion_fixture()
        found=oracle.action_completion_match(frame,source,self.assets,resources)
        self.assertTrue(found["caster_acted_sprite"])
        self.assertEqual(found["commit_witness"],"full caster acted sprite")
        self.assertFalse(found["HP_or_hidden_inferred"])
        self.assertFalse(found["bare_is_commit_witness"])
        self.assertEqual(found["restored_bare_probe"]["checked_pixels"],576)
        self.assertIs(resources["probes"][0],probe)
        self.assertEqual(found["world"],[10,14])
        changed=bytearray(source);changed[UNITS+0x40:UNITS+0x42]=b"\0\0";changed[UNITS+5]|=1
        self.assertEqual(oracle.action_completion_match(frame,changed,self.assets,resources),found)

    def test_original_LUT18_bare_alias_without_acted_cannot_commit(self):
        source,resources,frame,probe=self._target_completion_fixture(False)
        from run_original_comparison import archive_entry,TOLERANCE
        sheet=archive_entry((self.original/"FDOTHER.DAT").read_bytes(),3)
        import struct
        left,right=struct.unpack_from("<II",sheet,6+18*4);lut=sheet[left:right]
        image=Image.frombytes("P",(24,24),bytes(lut[p] for p in probe["base"].getdata()));image.putpalette(self.assets["palette"])
        difference=sum(max(abs(a-b) for a,b in zip(x,y))>TOLERANCE
            for x,y in zip(image.convert("RGB").getdata(),probe["base"].convert("RGB").getdata()))
        self.assertEqual(difference,0,"真实LUT18允许裸地别名，不能靠稳定采样当提交")
        frame.paste(image.convert("RGB"),probe["position"])
        detail=oracle.action_completion_match(frame,source,self.assets,resources)
        self.assertIsNone(detail["caster_acted_sprite"])
        self.assertIsNotNone(detail["caster_unacted_sprite"])
        self.assertIsNone(detail["commit_witness"])
        report={"kind":"original","checkpoints":[],"native_round_banners":[]}
        reader=SimpleNamespace(message=Mock(return_value=None),victory_text_complete=False)
        def checkpoint(name,predicate,**kwargs):
            for _ in range(4):self.assertIsNone(predicate(frame))
            raise TimeoutError("still unacted; no confirmation")
        capture=SimpleNamespace(key=Mock())
        with self.assertRaises(TimeoutError):
            runner.wait_progress(capture,self.original,source,1,self.assets,reader,report,checkpoint,
                ready_raw=0,ready_target=resources)
        capture.key.assert_not_called()

    def test_target_completion_rejects_retained_range_menu_dialog_and_bad_opaque_pixels(self):
        source,resources,frame,probe=self._target_completion_fixture()
        transformed=frame.copy();transformed.paste(probe["ranges"][0][1].convert("RGB"),probe["position"])
        self.assertIsNone(oracle.action_completion_match(transformed,source,self.assets,resources))
        dialog=frame.copy();dialog.paste(self.assets["dialog_top"].convert("RGB"),(5,112))
        self.assertIsNone(oracle.action_completion_match(dialog,source,self.assets,resources))
        menu=frame.copy();menu.paste(self.assets["commands"][9].convert("RGB"),(220,100))
        self.assertIsNone(oracle.action_completion_match(menu,source,self.assets,resources))
        wrong=copy.copy(resources);wrong["world"]=[9,14]
        self.assertIsNone(oracle.action_completion_match(frame,source,self.assets,wrong))
        for dx,dy in ((0,0),(23,0),(0,23),(23,23)):
            bad=frame.copy();bad.putpixel((196+dx,52+dy),(255,0,255))
            self.assertIsNone(oracle.action_completion_match(bad,source,self.assets,resources))
        bad=frame.copy();sprite=resources["sprites"][0]
        x,y=next((x,y) for y in range(24) for x in range(24) if sprite.getpixel((x,y)))
        bad.putpixel((resources["position"][0]+x,resources["position"][1]+y),(255,0,255))
        self.assertIsNone(oracle.action_completion_match(bad,source,self.assets,resources)["caster_acted_sprite"])

    def test_pending_story_or_growth_cannot_finish_during_a_closed_gap(self):
        source,resources,frame,_=self._target_completion_fixture()
        report={"kind":"original","checkpoints":[],"native_round_banners":[]}
        for pending in ("story","reward"):
            reader=SimpleNamespace(message=Mock(return_value=None),victory_text_complete=False,
                story=([],0) if pending=="story" else None,reward=([],0) if pending=="reward" else None)
            def checkpoint(name,predicate,**kwargs):
                for _ in range(3):self.assertIsNone(predicate(frame))
                raise TimeoutError("pending notice")
            with patch.object(runner,"action_completion_match") as match:
                with self.assertRaises(TimeoutError):
                    runner.wait_progress(SimpleNamespace(key=Mock()),self.original,source,1,self.assets,reader,report,
                        checkpoint,ready_raw=0,ready_target=resources)
                match.assert_not_called()

    def test_missing_caster_is_explicit_unsupported_not_a_hidden_inference_or_180s_wait(self):
        source,resources,frame,_=self._target_completion_fixture()
        frame.paste((0,0,0),(*resources["position"],resources["position"][0]+24,resources["position"][1]+24))
        reader=SimpleNamespace(message=Mock(return_value=None),victory_text_complete=False)
        report={"kind":"original","checkpoints":[],"native_round_banners":[]}
        def checkpoint(name,predicate,**kwargs):
            self.assertIsNone(predicate(frame));return None,predicate(frame)
        capture=SimpleNamespace(key=Mock())
        with patch.object(runner.time,"monotonic",side_effect=(0.0,2.1)):
            with self.assertRaisesRegex(runner.NaturalStopped,"不推断HP或hidden"):
                runner.wait_progress(capture,self.original,source,1,self.assets,reader,report,checkpoint,
                    ready_raw=0,ready_target=resources)
        capture.key.assert_not_called()

    def _entry_projection_measurement(self):
        # 只测试已验端内结果的跨端投影；不是原版构造器或游戏夹具。
        identities=[0,9,4,30,1]+[96]*16
        return {"stage":1,"turn":1,"active_count":21,"camera":[9,13],"first_focus":[20,14],
            "measuring_focus":[19,14],"complete_persistent_roster_equal":True,
            "manual_slots_and_unused_active_meta_tails_preserved":True,"initialized_actor_bytes_checked":[79]*21,
            "actors":[{"raw_actor":i,"unit_id":identity,"text_id":identity,"side":2 if i<5 else 0,
                "position":[20+i%5,14+i//5],"direction":0,"hidden":False,"acted":False}
                for i,identity in enumerate(identities)]}

    def test_entry_projection_separates_player_death_outcomes_from_fixed_geometry(self):
        left=self._entry_projection_measurement();right=copy.deepcopy(left)
        for raw in (1,2,4):left["actors"][raw]["hidden"]=True
        result=runner.compare_entry_measurements(left,right)
        self.assertTrue(result["entry_geometry_equal"]);self.assertTrue(result["NPC_hidden_compared"])
        self.assertEqual(result["random_player_hidden_excluded_from_fixed_geometry"],[1,2,3,4])
        self.assertEqual([x["raw_actor"] for x in result["actual_player_death_outcomes"] if not x["equal"]],[1,2,4])
        self.assertTrue(left["actors"][1]["hidden"]);self.assertFalse(right["actors"][1]["hidden"])

    def test_entry_projection_rejects_wrong_identity_geometry_NPC_hidden_and_missing_own_guards(self):
        first=self._entry_projection_measurement()
        for mutation in ("position","identity","side","direction","acted","raw","NPCdeath","herodeath","ownroster","owninit","ownsave"):
            second=copy.deepcopy(first)
            if mutation=="position":second["actors"][1]["position"][0]+=1
            elif mutation=="identity":second["actors"][1]["unit_id"]=4
            elif mutation=="side":second["actors"][1]["side"]=0
            elif mutation=="direction":second["actors"][1]["direction"]=2
            elif mutation=="acted":second["actors"][1]["acted"]=True
            elif mutation=="raw":second["actors"][1]["raw_actor"]=2
            elif mutation=="NPCdeath":second["actors"][5]["hidden"]=True
            elif mutation=="herodeath":second["actors"][0]["hidden"]=True
            elif mutation=="ownroster":second["complete_persistent_roster_equal"]=False
            elif mutation=="owninit":second["initialized_actor_bytes_checked"].pop()
            else:second["manual_slots_and_unused_active_meta_tails_preserved"]=False
            with self.assertRaises(AssertionError,msg=mutation):runner.compare_entry_measurements(first,second)


if __name__=="__main__":unittest.main()
