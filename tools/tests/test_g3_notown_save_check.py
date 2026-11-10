"""NoTown正式Save最短协议、完整写集与prepare-only回归；无窗口。"""
from pathlib import Path
import copy
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_notown_save_check as runner


class NoTownSaveTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.prepared=runner.prepare(ROOT)

    def test_reuses_source_prefix_and_preserves_complete_21_roster(self):
        p=self.prepared;source=runner.deployment_run.prepare(ROOT,runner.SCENARIO)
        self.assertEqual(p['steps'][:3],source['steps'][:3]);self.assertEqual(p['encoded'],source['encoded'])
        self.assertEqual((p['before']['stage'],p['before']['count'],len(p['before']['roster'])),(27,21,2560))
        self.assertEqual(p['after'],p['before']);self.assertNotEqual(p['before'],source['after'])
        self.assertEqual(p['manifest']['expected_state_roster_sha256'],runner.oracle.digest(p['before']['roster']))
        self.assertEqual(p['manifest']['parameters']['selected'],[])
        self.assertEqual(p['manifest']['expected_action']['business_write_trace'],[])
        self.assertTrue(runner.validate_plan(p));self.assertEqual(p['steps'][-1]['chosen'],[])
        self.assertEqual(p['steps'][-1]['remaining'],19)

    def test_low_color_picker_still_requires_complete_source_resource(self):
        p=self.prepared
        for name,plain,slot in (('notown-picker0',p['plain'],0),('notown-picker1',p['plain'],1),
                ('notown-saved-picker1',p['expected_plain'],1)):
            predicate,options=p['checks'][name]
            self.assertTrue(options['allow_low_color_resource'])
            frame=Image.new('RGB',(320,200));image=runner.picker_image(p['assets'],plain,slot)
            frame.paste(image.convert('RGB'),(5,112));self.assertTrue(predicate(frame))
            self.assertIsNone(predicate(Image.new('RGB',(320,200))))
            frame.putpixel((5,112),(255,255,255));self.assertIsNone(predicate(frame))
        self.assertFalse(p['checks']['notown-selection-after-save'][1]['allow_low_color_resource'])

    def test_single_save_confirmation_has_no_660_or_extra_ACK(self):
        p=self.prepared
        self.assertEqual(tuple((s['name'],s.get('key')) for s in p['steps']),runner.STEP_KEYS)
        self.assertEqual([s['save'] for s in p['steps'] if 'save' in s],[1])
        self.assertIsNone(p['steps'][-2]['key']);self.assertEqual(p['steps'][-2]['additional_ACKs'],0)
        for mutate in ('extra_ack','missing_person','missing_roster_tail','extra_save'):
            with self.subTest(mutate=mutate):
                q=copy.deepcopy({k:v for k,v in p.items() if k not in ('assets','checks','source_rois','titles')})
                if mutate=='extra_ack':q['steps'][-2]['key']='ENTER'
                elif mutate=='missing_person':q['before']['count']=20
                elif mutate=='missing_roster_tail':q['before']['roster']=q['before']['roster'][:21*80]
                else:q['steps'][-1]['save']=1
                with self.assertRaises(ValueError):runner.validate_plan(q)

    def test_full_writer_gate_rejects_source_active_unknown_tail_and_missing21_mutations(self):
        p=self.prepared;expected=p['expected_plain'];oracle=runner.oracle
        self.assertTrue(oracle.check_save(p['plain'],expected,p['before'],1)['ok'])
        at=oracle.SLOT_BASE+oracle.SLOT_STRIDE
        for offset in (0x8a3,oracle.SLOT_BASE,at+20*80+7,at+2560+1,at+2560+10,at+oracle.SLOT_STRIDE):
            with self.subTest(offset=offset):
                bad=bytearray(expected);bad[offset]^=1;bad=oracle.checksum(bad)
                self.assertFalse(oracle.check_save(p['plain'],bad,p['before'],1)['ok'])
        missing=copy.deepcopy(p['before']);missing['count']=20
        self.assertFalse(oracle.check_save(p['plain'],oracle.write_slot(p['plain'],missing,1),p['before'],1)['ok'])

    def test_actual_exercise_sends_only_one_save_enter_then_waits_without_ACK(self):
        p=self.prepared;keys=[];events=[];capture=SimpleNamespace(key=lambda key:keys.append(key))
        def measure(c,p,s,g,r,slot):
            self.assertEqual(slot,1);keys.append('ENTER');events.append('save')
        with patch.object(runner.service,'wait_title'),patch.object(runner.service,'require_source_unchanged'),\
             patch.object(runner.service,'save_measurement',side_effect=measure),\
             patch.object(runner.service,'wait_state',side_effect=lambda c,n,*a,**k:events.append(n)):
            runner.service.exercise(capture,p,None,[],{'checkpoints':[]},SimpleNamespace())
        self.assertEqual(keys,['DOWN','ENTER','ENTER','ENTER','DOWN','ENTER','ESC'])
        index=events.index('save');self.assertEqual(events[index+1:],['notown-saved-picker1','notown-selection-after-save'])

    def test_default_prepare_has_no_GUI_and_no_native_pass_claim(self):
        with tempfile.TemporaryDirectory(prefix='g3-notown-save-pure-') as tmp:
            args=SimpleNamespace(root=ROOT,artifacts=Path(tmp)/'prepared',sdl=None,driver=None,dosbox=None,execute=False)
            with patch.object(runner,'running',side_effect=AssertionError('不得开窗口')),\
                 patch.object(runner,'owned_job_required',side_effect=AssertionError('不得启动owned job')):
                result=runner.run(args)
            self.assertFalse(result['ok']);self.assertEqual(result['cases'],[])
            self.assertEqual(result['status'],'ready_not_executed');self.assertTrue(result['final_sources_unchanged'])
            self.assertEqual((args.artifacts/'controlled.SAV').stat().st_size,22987)
            self.assertEqual(result['scope']['full_roster_bytes'],2560)
            self.assertFalse(result['scope']['runtime_hot_owner_snapshot_claimed'])


if __name__=='__main__':unittest.main()
