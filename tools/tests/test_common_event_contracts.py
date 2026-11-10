"""有限事件参数的跨字段合同；不修改源定义或执行Game。"""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
import generate


class CommonContractsTest(unittest.TestCase):
    def document(self,op,args):
        source=json.loads((ROOT/'data/static/flows/common_event.json').read_text())
        source['flows']=[{'id':'contract.test','entry':'start','nodes':[
            {'id':'start','op':op,'args':args,'on':{'next':'done'}},
            {'id':'done','op':'end','args':[],'on':{}}]}]
        source['bindings']={}
        return source

    def test_BYTE_field_rejects_wide_value_but_HP_accepts_word(self):
        generate.validate_flows(self.document('unit_field_write',[0,1,18,0,65535]))
        with self.assertRaisesRegex(ValueError,'BYTE field'):
            generate.validate_flows(self.document('unit_field_write',[0,1,0,0,65535]))

    def test_actor_and_item_intervals_reject_reversed_pairs(self):
        valid=self.document('event_item_consume',[0,1,11,12])
        generate.validate_flows(valid)
        for start,end in ((0,1),(2,3)):
            changed=copy.deepcopy(valid)
            args=changed['flows'][0]['nodes'][0]['args'];args[start],args[end]=args[end],args[start]
            with self.assertRaisesRegex(ValueError,'interval'):generate.validate_flows(changed)


if __name__=='__main__':unittest.main()
