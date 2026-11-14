"""首次Save的有限Load禁门、真实FDOTHER资源与共享默认合同；无GUI。"""
from contextlib import nullcontext
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch
from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_s5_stage_check as host
import run_s5_cinematic_check as cinematic
import run_s3_stage_check as shared
from native import s5_fixture
from run_original_battle_menu import MENU_OFFSETS
from run_original_comparison import title_templates
from run_original_end_turn import RoundOracle,round_assets


class S5StoragePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p21=s5_fixture.prepare(ROOT/'original_game')
        cls.p24=cinematic.prepare(ROOT,'stage24-notown-shake')
        cls.assets=round_assets(ROOT/'original_game')
        cls.titles=title_templates(ROOT/'original_game/FDOTHER.DAT')

    def record(self,kind,stage=21,bound=False):
        return {'kind':kind,'title_menu_policy':{'menu_count':3 if kind=='original' or bound else 2,
            'active_snapshot_stage':20 if stage==20 else 29,'manual_slot0_stage':stage,
            'active_capability_bound':bound,'stage_definition_sha256':'frozen-definition-sha'}}

    def test_missing_capability_is_sufficient_negative_not_a_general_positive_Load_oracle(self):
        for stage in (21,24):
            record=self.record('sdl',stage)
            self.assertFalse(host.initial_load_availability(record))
            self.assertTrue(record['initial_load_availability']['title_support_is_not_full_Load_qualification'])
            self.assertTrue(host.initial_load_availability(self.record('original',stage)))
            with self.assertRaisesRegex(ValueError,'实际导入预检'):
                host.initial_load_availability(self.record('sdl',stage,True))
        self.assertTrue(host.initial_load_availability(self.record('sdl',20,True)))
        for bad in (self.record('unknown'),self.record('sdl',22)):
            with self.assertRaises(ValueError):host.initial_load_availability(bad)

    def test_actual_full_secondary_resources_require_disabled_Load_and_exact_selection(self):
        focus=(148,28);menu=RoundOracle(self.assets,focus)
        def frame(load_frame,selected):
            image=Image.new('RGB',(320,200))
            for item,command in enumerate((12,13,14,15)):
                index=load_frame if item==2 else command*3+(item==selected)
                at=tuple(a+b for a,b in zip(focus,MENU_OFFSETS[item]))
                image.paste(self.assets['commands'][index].convert('RGB'),at)
            return image
        for selected in (0,1):
            actual=frame(44,selected);detail=menu.secondary(actual,selected,load_available=False)
            self.assertIsNotNone(detail);self.assertEqual(len(detail['matches']),4)
            self.assertEqual(detail['matches'][2]['frame'],44)
            self.assertEqual(detail['matches'][2]['checked_pixels'],476)
            self.assertIsNone(menu.secondary(actual,selected))
            for enabled in (42,43):self.assertIsNone(menu.secondary(frame(enabled,selected),selected,load_available=False))
            self.assertIsNone(menu.secondary(frame(44,1-selected),selected,load_available=False))
            damaged=actual.copy();damaged.putpixel((173,31),(255,0,255))
            self.assertIsNone(menu.secondary(damaged,selected,load_available=False))
        self.assertIsNone(menu.secondary(frame(44,2),2,load_available=False))

    def test_shared_Save_keeps_old_default_and_passes_explicit_flag_to_both_secondary_checks(self):
        source=self.p21['source']
        for optional,wanted in (({},True),({'load_available':False},False)):
            with self.subTest(optional=optional),tempfile.TemporaryDirectory() as d:
                work=Path(d);save=work/'FD2.SAV';save.write_bytes(self.p21['encoded'])
                capture=SimpleNamespace(artifacts=work,last=None,key=Mock());menu=Mock()
                p={'original':work,'assets':{}};guard=[save.read_bytes(),save.stat().st_mtime_ns];record={}
                with patch.object(shared.natural,'cursor_bank'),patch.object(shared.natural,'empty_tile',return_value={'world':(4,4)}),\
                     patch.object(shared.protocol,'navigate',return_value=source),patch.object(shared,'RoundOracle',return_value=menu),\
                     patch.object(shared,'wait_measured_save',return_value=source):
                    shared.save_activity(capture,p,source,save,guard,record,lambda label,predicate:predicate(None),
                        'probe',lambda *args:{'checked':True},**optional)
                self.assertEqual([c.kwargs for c in menu.secondary.call_args_list],
                    [{'load_available':wanted},{'load_available':wanted}])
                self.assertEqual([c.args[1] for c in menu.secondary.call_args_list],[0,1])
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],['ENTER','ENTER','LEFT','ENTER','ENTER'])

    def test_both_S5_manual_callers_pass_first_Save_flag_and_only_then_start_Load(self):
        for kind in ('original','sdl'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as d:
                work=Path(d);save=work/'FD2.SAV';save.write_bytes(self.p21['encoded'])
                (work/'game.log').write_text('',encoding='utf8')
                capture=SimpleNamespace(artifacts=work,last=None,process=object(),key=lambda key:None,wait=lambda image:None)
                p={**self.p21,'titles':self.titles,'assets':{},'selected_actor':0}
                record=self.record(kind);anchor='field result query: stage=21 result=0'
                with patch.object(shared,'checkpoint',return_value=lambda *a,**k:None),\
                     patch.object(shared,'preparation_town',return_value={'assets':{}}),patch.object(host,'wait_title'),\
                     patch.object(host,'notice_resources'),\
                     patch.object(host.oracle,'entry21',return_value=(p['source'],[bytearray(80)],[bytes([1]*80)])),\
                     patch.object(host.state_oracle,'pages',return_value=[]),patch.object(host.protocol,'navigate',return_value=p['source']),\
                     patch.object(host.natural,'selection_summary'),patch.object(host.natural,'range_probe'),patch.object(host.natural,'cursor_bank'),\
                     patch.object(host,'wait_log',return_value=anchor),patch.object(shared,'save_activity',return_value=p['source']) as saved,\
                     patch.object(shared,'load_activity',return_value=p['source']) as loaded,patch.object(host,'end21'):
                    host.title_entry(capture,p,save,None,record)
                self.assertEqual(saved.call_args.kwargs,{'load_available':kind=='original'})
                loaded.assert_called_once()
                p={**self.p24,'titles':self.titles};record=self.record(kind,24);record['continuous_wgc']={'frames':[]}
                with patch.object(shared,'checkpoint',return_value=lambda *a,**k:None),patch.object(cinematic,'wait_title'),\
                     patch.object(cinematic,'series',return_value=nullcontext()),patch.object(cinematic,'shake_gate',return_value={'ok':True}),\
                     patch.object(shared,'save_activity') as saved:
                    cinematic.entry24(capture,p,save,None,record)
                self.assertEqual(saved.call_args.kwargs,{'load_available':kind=='original'})


if __name__=='__main__':unittest.main()
