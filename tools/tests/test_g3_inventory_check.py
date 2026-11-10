"""G3 EQT/不同Give正式代表的独立源页与全SAV门；不启动GUI。"""
import copy
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_inventory_check as runner
from fd2_item_list_oracle import inventory_templates
import fd2_g3_prepare_oracle as oracle


class InventoryCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.prepared={s:runner.prepare(ROOT,s) for s in runner.SCENARIOS}

    def test_source_fixture_and_formal_cancel_paths_keep_one_real_hotel_save(self):
        for scenario,p in self.prepared.items():
            with self.subTest(scenario=scenario):
                self.assertEqual((len(p['encoded']),len(p['plain'])),(22987,22987))
                self.assertEqual(p['before']['count'],5);self.assertEqual(p['before']['stage'],1)
                self.assertEqual([n.get('save') for n in p['steps'] if 'save' in n],[0])
                self.assertFalse(any('collector' in n or n.get('key')=='HOST_W' for n in p['steps']))
                self.assertEqual(p['steps'][-1]['name'],'town-final')
                self.assertIn('hotel-slot0-after-save',p['checks'])
                self.assertFalse(p['runtime_protocol']['not_claimed']==[])
        eqt=self.prepared['eqt-reselect']
        self.assertEqual(eqt['manifest']['parameters']['slots'],[0,2])
        names={s['name']:s for s in eqt['steps'] if 'name' in s}
        self.assertEqual(names['equipment-cancel-return']['key'],'ESC')
        self.assertEqual(names['equipment-after33']['key'],'ENTER');self.assertEqual(names['equipment-after43']['key'],'ENTER')
        give=self.prepared['give-different-cancel'];names={s['name']:s for s in give['steps'] if 'name' in s}
        self.assertEqual(names['give-recipient-cancelled']['key'],'ESC')
        self.assertEqual(give['manifest']['parameters'],{'kind':'give','sender':2,'recipient':0,'rank':0})

    def test_different_give_matches_source_order_and_preserves_all_other_records(self):
        p=self.prepared['give-different-cancel'];before,after=p['before'],p['after']
        expected,action=oracle.model_action(before,{'kind':'give','sender':2,'recipient':0,'rank':0},p['tables'])
        self.assertEqual(after,expected);self.assertEqual([x['step'] for x in action['business_write_trace']],['give_remove','give_append','recompute_sender'])
        self.assertEqual(oracle.record(after,0)[24:26],bytes((0,20)))
        self.assertEqual(oracle.inventory_count(oracle.record(after,2)),1)
        for raw in (1,3,4):self.assertEqual(oracle.record(before,raw),oracle.record(after,raw))
        self.assertEqual(before['roster'][400:],after['roster'][400:]);self.assertEqual(before['resource'],after['resource'])

    def equipment_frame(self,state,selected):
        p=self.prepared['eqt-reselect'];_,templates,_=inventory_templates(p['original'],oracle.record(state,4))
        canvas=Image.new('RGB',(320,200),(3,4,5))
        for name,(xy,image) in templates.items():
            if name.startswith('selected_') or name=='unselected':continue
            canvas.paste(image.convert('RGB'),xy)
        xy,image=templates['selected_'+str(selected)];canvas.paste(image.convert('RGB'),xy)
        return canvas

    def test_equipment_each_confirmation_requires_distinct_flag_and_stat_resource_page(self):
        p=self.prepared['eqt-reselect'];first=runner._equip(p['before'],4,0,p['tables'])
        self.assertNotEqual(first,p['before']);self.assertEqual(p['after'],runner._equip(first,4,2,p['tables']))
        before=self.equipment_frame(p['before'],0);after33=self.equipment_frame(first,0)
        gate=p['checks']['equipment-after33'][0]
        self.assertIsNone(gate(before));self.assertIsNotNone(gate(after33))
        wrong_selection=self.equipment_frame(first,2);self.assertIsNone(gate(wrong_selection))
        after43=self.equipment_frame(p['after'],2)
        self.assertIsNotNone(p['checks']['equipment-after43'][0](after43))
        self.assertIsNone(p['checks']['equipment-after43'][0](wrong_selection))
        corrupted=after33.copy();corrupted.putpixel((15,101),(255,1,2));self.assertIsNone(gate(corrupted))

    def test_full_save_gate_rejects_unrelated_active_or_unselected_record_changes(self):
        for p in self.prepared.values():
            expected=oracle.write_slot(p['plain'],p['after'],0)
            self.assertTrue(oracle.check_save(p['plain'],expected,p['after'],0)['ok'])
            for offset in (0x12a3,oracle.SLOT_BASE+4*80+8):
                bad=bytearray(expected);bad[offset]^=1;bad=oracle.checksum(bad)
                self.assertFalse(oracle.check_save(p['plain'],bad,p['after'],0)['ok'])

    def test_default_prepare_only_freezes_sources_and_never_opens_window(self):
        with tempfile.TemporaryDirectory(prefix='g3-inventory-') as tmp:
            args=SimpleNamespace(root=ROOT,artifacts=Path(tmp)/'artifacts',scenario='give-different-cancel',
                driver=None,dosbox=None,sdl=None,run=False)
            with patch.object(runner,'owned_job_required') as owned,patch.object(runner,'running') as launch:
                report=runner.run(args)
            self.assertEqual(report['status'],'ready_not_executed');self.assertFalse(report['ok'])
            self.assertTrue(report['final_sources_unchanged']);self.assertEqual(report['cases'],[])
            owned.assert_not_called();launch.assert_not_called()
            self.assertEqual(json.loads((args.artifacts/'comparison.json').read_text(encoding='utf-8'))['scenario'],args.scenario)
        with self.assertRaises(ValueError):runner.prepare(ROOT,'unknown')


if __name__=='__main__':unittest.main()
