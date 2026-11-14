"""S5正式输入夹具来源/拒绝纯测；不启动窗口，不把prepared当pass。"""
import copy
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from native import s5_fixture as fixture
from run_graphics_tests import decode_save


class S5NativeSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared=fixture.prepare(ROOT/'original_game')

    def test_controlled_manual_slot_keeps_entire_active_source_and_original_roster(self):
        p=self.prepared;source=decode_save(ROOT/'original_game/FD2.SAV');derived=p['source']
        self.assertEqual(derived[:0x312b],source[:0x312b])
        self.assertEqual(p['manual_state']['roster'],source[0x8a3:0x12a3])
        self.assertEqual((p['manual_state']['stage'],p['manual_state']['count']),(21,16))
        self.assertTrue(p['manifest']['controlled_fixture'])
        self.assertFalse(p['manifest']['runtime_alias_or_API_injection'])
        self.assertFalse(p['manifest']['natural_whole_stage_claimed'])
        self.assertEqual([r['offset'] for r in p['manifest']['plaintext_patches']],
            [i for i,(a,b) in enumerate(zip(source,derived)) if a!=b])

    def test_source_entry_is_16_roster_and_50_group0_not_sdl_definition(self):
        p=self.prepared
        view,records,masks=fixture.entry21(p['source'],p['manual_state'],p['data'],p['original'])
        self.assertEqual(len(records),66)
        self.assertEqual(len(masks),66)
        self.assertEqual(view[fixture.META:fixture.META+3],bytes((1,66,21)))
        for raw in range(16):
            original=p['manual_state']['roster'][raw*80:(raw+1)*80]
            self.assertEqual(records[raw][7:32],original[7:32])
            self.assertEqual(records[raw][40:49],original[40:49])
        bad=copy.deepcopy(p['data']);bad['stages'][21]['metadata']=bytes((21,15,70))+bad['stages'][21]['metadata'][3:]
        with self.assertRaises(ValueError):fixture.entry21(p['source'],p['manual_state'],bad,p['original'])
        self.assertEqual(p['data']['stages'][21]['metadata'][1],16)

if __name__=='__main__':unittest.main()
