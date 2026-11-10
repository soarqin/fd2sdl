"""G3金额离线源/非末态/全ROI负例；不启动游戏或读取SDL终态拟合期望。"""
from pathlib import Path
import json
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_money_content_check as money


class MoneyContentCheckTest(unittest.TestCase):
    def test_original_complete_functions_are_independent_of_an_ignored_database(self):
        proof=money.source_contract(ROOT)
        self.assertEqual([(row['cva'],row['size']) for row in proof['functions']],
            [('0x26a7a',279),('0x26b91',266),('0x26c9b',73)])
        self.assertTrue(all(row['legal_relocated_source_equal'] for row in proof['functions']))

    def test_three_real_fixture_sources_have_bounded_nonterminal_full_roi_candidates(self):
        for scenario,(selector,subtract) in money.SCENARIOS.items():
            _,_,before,after,_=money.oracle.derive_fixture(ROOT/'original_game',scenario)
            first,last,frames,negatives=money.templates(ROOT,selector,before['resource'],after['resource'],subtract)
            self.assertEqual((first.size,last.size),((48,9),(48,9)))
            self.assertTrue(all(negatives.values()));self.assertGreater(len(frames),0);self.assertLessEqual(len(frames),63)
            self.assertTrue(all(1<=frame['phase']<=7 and frame['cycle']<10 for frame in frames))
            for frame in frames:
                self.assertGreater(money.maximum(first,frame['image']),3)
                self.assertGreater(money.maximum(last,frame['image']),3)
            broken=frames[0]['image'].copy();old=broken.getpixel((47,8));broken.putpixel((47,8),tuple(255-channel for channel in old))
            self.assertGreater(money.maximum(broken,frames[0]['image']),3)

    def test_zero_change_or_negative_glyph_owner_cannot_fake_a_rolling_representative(self):
        for old,new in ((100,100),(-1,100),(100,-1)):
            with self.assertRaises(ValueError):money.templates(ROOT,1,old,new,True)

    def test_collection_only_and_duplicate_endpoints_are_not_a_paired_business_gate(self):
        with tempfile.TemporaryDirectory(prefix='g3-money-input-') as tmp:
            path=Path(tmp)/'comparison.json'
            for report in ({'scenario':'p1-buy-equip-yes','ok':False},
                {'scenario':'p1-buy-equip-yes','ok':True,'final_sources_unchanged':True,'cases':[{'kind':'original'},{'kind':'original'}]}):
                path.write_text(json.dumps(report),encoding='utf-8')
                with self.assertRaises(ValueError):money.analyze(ROOT,path,Path(tmp))


if __name__=='__main__':unittest.main()
