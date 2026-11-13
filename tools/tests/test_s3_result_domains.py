"""S3条件结果的原读点有效域；不把完整目标最大raw前缀提前套到早支。"""
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/ida'))
import s2_result_source as source
import verify_s2_results as oracle


class ConditionalResultDomains(unittest.TestCase):
    def test_signed_read_point_boundaries_preserve_maximum_path_domain(self):
        for target,early,limit,late in ((0x20765,27,5,60),(0x20926,1,6,65)):
            self.assertEqual(source.MINIMUM[target],late)
            for turn in (0x80000000,0xffffffff,0,limit):
                self.assertEqual(source.required_raw_prefix(target,turn),early)
            for turn in (limit+1,0x7fffffff):self.assertEqual(source.required_raw_prefix(target,turn),late)
        self.assertEqual(source.required_raw_prefix(0x20822,0),65)
        roster=bytearray(160);roster[88]=18;roster[85]=1
        self.assertEqual(source.required_raw_prefix(0x20872,1,roster),1)
        roster[88]=17;self.assertEqual(source.required_raw_prefix(0x20872,1,roster),53)

    def test_additional_vectors_are_legal_early_counts_not_dummy_records(self):
        cases=list(oracle.conditional_prefix_inputs());self.assertEqual(len(cases),84)
        self.assertEqual(len(list(oracle.vector_inputs())),832)
        for case in cases:
            count=len(case['records'])//80
            self.assertGreaterEqual(count,source.required_raw_prefix(case['target'],case['turn'],case['roster']))
            self.assertEqual(len(case['records']),count*80)
        self.assertTrue(any(c['target']==0x20765 and len(c['records'])//80==59 and c['turn']==5 for c in cases))
        self.assertTrue(any(c['target']==0x20926 and len(c['records'])//80==64 and c['turn']==6 for c in cases))

    def test_cold_guard_is_early_domain_and_late_raw_stays_behind_true_condition(self):
        flows={}
        for name in ('stage','common_event'):
            flows.update({f['id']:f for f in json.loads((ROOT/f'data/static/flows/{name}.json').read_text())['flows']})
        f=flows['common.result.source_020765'];nodes={n['id']:n for n in f['nodes']}
        self.assertEqual(nodes['domain']['args'],[0,26,0,0]);self.assertEqual(nodes['turn']['args'],[4,5])
        self.assertEqual(nodes['turn']['on']['yes'],'late.domain');self.assertEqual(nodes['hidden59']['args'],[59,0])
        self.assertEqual(nodes['late.domain']['args'],[59,59,0,0])
        f=flows['common.result.source_020872'];nodes={n['id']:n for n in f['nodes']}
        self.assertEqual(nodes['domain']['args'],[0,0,0,0]);self.assertEqual(nodes['roster']['args'],[18])
        self.assertEqual(nodes['roster']['on'],{'yes':'done','no':'late.domain','error':'failed'})
        self.assertEqual(nodes['late.domain']['args'],[52,52,0,0])
        f=flows['stage.result.source_020926'];nodes={n['id']:n for n in f['nodes']}
        self.assertEqual(nodes['domain']['args'],[1]);self.assertEqual(nodes['turn']['args'],[4,6])
        self.assertEqual(nodes['turn']['on']['yes'],'late.domain');self.assertEqual(nodes['hidden64']['args'],[64,0])
        self.assertEqual(nodes['late.domain']['args'],[65])


if __name__=='__main__':unittest.main()
