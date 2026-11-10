"""G3独立原数据状态／全容器模型；不启动原版、SDL、窗口或构建。"""
from pathlib import Path
import copy
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import fd2_g3_prepare_oracle as oracle


class G3PrepareOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixtures={name:oracle.derive_fixture(ROOT/'original_game',name) for name in oracle.SCENARIOS}
        cls.tables,_=oracle.tables(ROOT/'original_game')

    def test_all_fixtures_crypto_patch_scope_and_source_identity(self):
        for name,(encoded,plain,state,after,manifest) in self.fixtures.items():
            with self.subTest(name=name):
                self.assertEqual(oracle.crypt(encoded),plain)
                oracle.valid_container(plain)
                self.assertEqual(len(state['roster']),2560);self.assertEqual(len(after['roster']),2560)
                self.assertTrue(manifest['controlled_fixture']);self.assertFalse(manifest['natural_arrival_claimed'])
                allowed=set(range(oracle.SLOT_BASE,oracle.SLOT_BASE+2570))|set(range(oracle.FILE_SIZE-4,oracle.FILE_SIZE))
                self.assertTrue(all(p['offset'] in allowed and p['before']!=p['after'] and p['reason'] for p in manifest['plaintext_patches']))
                self.assertFalse(manifest['expected_action']['original_runtime_executed'])
                self.assertFalse(manifest['shared_rng_trajectory_claimed'])

    def test_buy_yes_no_reject_and_original_write_order(self):
        for name in ('p1-buy-equip-yes','p1-buy-equip-no'):
            _,_,before,after,m=self.fixtures[name]
            self.assertEqual(m['expected_action']['outcome'],'completed');self.assertEqual(after['resource'],0)
            unit=oracle.record(after,0);self.assertEqual(unit[24:26],bytes((64 if name.endswith('yes') else 0,128)))
            self.assertEqual(oracle.record(before,0)[13],132)
            self.assertEqual(unit[12],0 if name.endswith('yes') else 64)
            steps=[e['step'] for e in m['expected_action']['business_write_trace']]
            self.assertEqual(steps,['buy_append','equipment_flags','recompute_sender','money_subtract'] if name.endswith('yes') else ['buy_append','money_subtract'])
        for name,outcome in (('p1-buy-insufficient','insufficient'),('p1-buy-full','full')):
            _,_,before,after,m=self.fixtures[name]
            self.assertEqual(before,after);self.assertEqual(m['expected_action']['outcome'],outcome)
            self.assertEqual(m['expected_action']['business_write_trace'],[])

    def test_sell_and_self_give_raw_alias(self):
        _,_,before,after,m=self.fixtures['p2-sell-equipped']
        price=3*oracle.word(oracle.item(self.tables,132),19)//4
        self.assertEqual(after['resource'],(before['resource']+price)&0xffffffff)
        self.assertEqual([x['step'] for x in m['expected_action']['business_write_trace']],['money_add','sell_remove','recompute_sender'])
        self.assertEqual(oracle.record(after,0)[25],oracle.record(before,0)[25])
        _,_,before,after,m=self.fixtures['p2-self-give']
        unit=oracle.record(after,2);old=oracle.record(before,2)
        self.assertEqual(unit[10:14],bytes((64,128,0,20)));self.assertEqual(after['resource'],before['resource'])
        self.assertEqual(unit[:10],old[:10]);self.assertEqual(unit[26:72],old[26:72]);self.assertEqual(unit[25],old[25])
        self.assertEqual([x['raw'] for x in m['expected_action']['business_write_trace']],[2,2,2])

    def test_revive_preserves_nonzero_hp_mp_status_and_whole_flags(self):
        _,_,before,after,m=self.fixtures['p3-revive']
        old=oracle.record(before,2);unit=oracle.record(after,2)
        self.assertTrue(oracle.word(old,64)>0);self.assertEqual(old[5],1)
        expected=bytearray(old);expected[5]=0;expected[64:66]=expected[66:68]
        self.assertEqual(unit,bytes(expected));self.assertEqual(after['resource'],0)
        self.assertEqual([x['step'] for x in m['expected_action']['business_write_trace']],['money_subtract','revive_flags','revive_hp'])
        _,_,before,after,m=self.fixtures['p3-revive-insufficient'];self.assertEqual(before,after)
        self.assertEqual(m['expected_action']['outcome'],'insufficient')

    def test_normal_special_promotion_identity_skill_tail_and_cost(self):
        for name,new_id,raw in (('p4-promote-normal',36,2),('p4-promote-special90',52,1)):
            _,_,before,after,m=self.fixtures[name];old=oracle.record(before,raw);unit=oracle.record(after,raw)
            self.assertEqual(unit[7],new_id);self.assertEqual(unit[8],old[8]);self.assertEqual(unit[26:31],old[26:31])
            self.assertEqual(unit[33],1);self.assertEqual(unit[60],0);self.assertEqual(unit[64:66],unit[66:68]);self.assertEqual(unit[68:70],unit[70:72])
            self.assertEqual(unit[2],old[2]);self.assertEqual(after['resource'],before['resource'])
            steps=[x['step'] for x in m['expected_action']['business_write_trace']]
            self.assertEqual(steps[0],'promotion_consume' if raw==1 else 'promotion_identity')
            if raw==1:self.assertEqual(unit[24:26],bytes((128,90)))
            row=self.tables['growth'][new_id*11:(new_id+1)*11]
            for i,offset in enumerate((0x37,0x39,0x3e,0x42,0x46)):
                self.assertEqual(oracle.word(unit,offset),(oracle.word(old,offset)+row[2*i+1])&65535)

    def test_selection_final_no_and_required_failure_keep_sorted_full_tail(self):
        for name in ('p5-select17-final-no','p5-select21-final-no','p6-notown22-missing24'):
            _,_,before,after,m=self.fixtures[name];count=before['count']
            self.assertEqual(before['roster'][:80],after['roster'][:80]);self.assertEqual(before['count'],after['count'])
            self.assertEqual(before['roster'][80*count:],after['roster'][80*count:]);self.assertNotEqual(before['roster'],after['roster'])
            self.assertEqual(sorted(oracle.record(before,i) for i in range(count)),sorted(oracle.record(after,i) for i in range(count)))
            self.assertEqual(m['expected_action']['outcome'],'required_member_missing' if name.startswith('p6') else 'final_no_sorted_retained')
        self.assertEqual(oracle.record(self.fixtures['p5-select21-final-no'][3],1)[8],9)
        self.assertTrue(self.fixtures['p6-notown22-missing24'][4]['notown'])
        self.assertFalse(self.fixtures['p6-notown22-missing24'][4]['native_hotel_save_after_action'])
        self.assertFalse(self.fixtures['p5-select21-final-no'][4]['native_hotel_save_after_action'])

    def test_all_four_slots_full_container_unknown_tail_and_unrelated_byte_guards(self):
        for name,(_,plain,_,after,_) in self.fixtures.items():
            for slot in range(4):
                with self.subTest(name=name,slot=slot):
                    saved=oracle.write_slot(plain,after,slot)
                    self.assertTrue(oracle.check_save(plain,saved,after,slot)['ok'])
                    at=oracle.SLOT_BASE+slot*oracle.SLOT_STRIDE
                    allowed=set(range(at,at+2570))|set(range(oracle.FILE_SIZE-4,oracle.FILE_SIZE))
                    self.assertTrue(all(i in allowed for i,(a,b) in enumerate(zip(plain,saved)) if a!=b))
                    for offset in (0,at+2560+11,at+80*31+79,0x8a3):
                        bad=bytearray(saved);bad[offset]^=1;bad=oracle.checksum(bad)
                        self.assertFalse(oracle.check_save(plain,bad,after,slot)['ok'])

    def test_invalid_container_and_source_state_are_not_guessed(self):
        _,plain,before,_,_=self.fixtures['p2-self-give']
        with self.assertRaises(ValueError):oracle.valid_container(plain[:-1])
        bad=bytearray(plain);bad[0]^=1
        with self.assertRaises(ValueError):oracle.valid_container(bytes(bad))
        for slot in (-1,4):
            with self.assertRaises(ValueError):oracle.write_slot(plain,before,slot)
        bad_state=copy.deepcopy(before);bad_state['stage']=30
        with self.assertRaises(ValueError):oracle.write_slot(plain,bad_state,0)
        with self.assertRaises(ValueError):oracle.derive_fixture(ROOT/'original_game','unknown')


if __name__=='__main__':unittest.main()
