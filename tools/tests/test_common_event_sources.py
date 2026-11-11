"""G4事件原来源合同：ACK前后顺序、整数身份、未绑定和错误重放负例。"""
import importlib.util
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
spec=importlib.util.spec_from_file_location('common_event_source',ROOT/'tools/static_data/verify_common_events.py')
verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify)

class CommonEventSourcesTest(unittest.TestCase):
    def tables(self):
        return json.loads((ROOT/'data/static/tables/common_event.json').read_text(encoding='utf-8'))
    def flows(self):
        return json.loads((ROOT/'data/static/flows/common_event.json').read_text(encoding='utf-8'))
    def sources(self):
        return {'k_common_event_nested_descriptors':bytes([0,211,0,0,213,0,0,101,0,0,11,0]),
                'k_common_event_cell_items':bytes([29,43,51,61,71])}
    def flow(self,document,name):
        return next(f for f in document['flows'] if f['id']==name)
    def node(self,document,flow,node):
        return next(n for n in self.flow(document,flow)['nodes'] if n['id']==node)
    def test_current_source_contract(self):
        verify.validate_tables(self.tables(),self.sources())
        result=verify.validate_flow(self.flows())
        self.assertEqual((result['flow_count'],result['source_control_paths_checked'],result['bindings_added']),(21,19,0))
        self.assertEqual(result['existing_bindings_migrated'],6)
    def test_refresh0_calls_water_and_refresh1_skips_from_original_JNZ(self):
        raw=verify.source_range((ROOT/'original_game/FD2.EXE').read_bytes(),0x11cbc,12)
        result=verify.validate_refresh_water_source(raw)
        self.assertTrue(result['raw_argument_0_calls_water'] and result['raw_argument_1_skips_water'])
        inverted=bytearray(raw);inverted[5]=0x74
        with self.assertRaisesRegex(ValueError,'水色原条件'):verify.validate_refresh_water_source(inverted)
        data=self.flows();data['operations']['refresh_map']['args'][0]['name']='water'
        with self.assertRaisesRegex(ValueError,'0推进/1跳过'):verify.validate_flow(data)
    def test_table_source_byte_shape_identity_and_integer_rejected(self):
        mutations=[lambda d:d['tables'][0]['values'][0].__setitem__(0,2),
            lambda d:d['tables'][0]['values'][3].__setitem__(1,0),
            lambda d:d['tables'][1]['values'].__setitem__(4,True),
            lambda d:d['tables'][1].update(dimensions=[4]),
            lambda d:d['tables'][1]['source'].update(canonical_va='0x52742'),
            lambda d:d['tables'][0]['source'].update(file_sha256='0'*64)]
        for mutate in mutations:
            d=self.tables();mutate(d)
            with self.subTest(d=d),self.assertRaises(ValueError):verify.validate_tables(d,self.sources())
    def test_grant_must_follow_wait_ack_but_key_remove_precedes_ack(self):
        d=self.flows()
        self.node(d,'common.cell.grant','item.portrait')['on']['next']='item.grant'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows()
        self.node(d,'common.key.consume','has.key')['on']['yes']='used.say'
        self.node(d,'common.key.consume','used.wait')['on']['ack']='consume'
        self.node(d,'common.key.consume','consume')['on']['yes']='used.close'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
    def test_child_actor_identity_and_parent_after_child_order(self):
        for index in range(3):
            d=self.flows();self.node(d,f'common.loot.child.{index}','child')['args'][1]=0
            with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.loot.physical','child')['args'][1]=-1
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.loot.child.0','child')['on']['ack']='done'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
    def test_retry_after_late_failure_and_new_stage_binding_rejected(self):
        d=self.flows();self.node(d,'common.inventory.once','say')['on']['error']='caller'
        with self.assertRaisesRegex(ValueError,'冻结'):verify.validate_flow(d)
        d=self.flows();d['bindings']={'stage.002.action.001':'common.cell.grant'}
        with self.assertRaisesRegex(ValueError,'绑定'):verify.validate_flow(d)
    def test_source_specific_map_tail_and_random_order(self):
        d=self.flows();self.node(d,'common.map.replace','cells')['on']['next']='pan.a'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.random.pair','schedule')['args'][1]=0
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.loot.physical','clear')['op']='clear_keys'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
    def test_new_or_duplicate_flow_is_not_source_closed(self):
        d=self.flows();d['flows'].append(d['flows'][0])
        with self.assertRaisesRegex(ValueError,'未审'):verify.validate_flow(d)
        d=self.flows();d['flows'].append({'id':'unreviewed.callback','entry':'end','nodes':[{'id':'end','op':'end','args':[],'on':{}}]})
        with self.assertRaisesRegex(ValueError,'未审'):verify.validate_flow(d)

if __name__=='__main__':unittest.main()
