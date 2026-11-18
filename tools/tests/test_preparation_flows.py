"""G3冷图提交/取消边与源顺序；只遍历定义，不冒充UI或服务实现验收。"""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
import generate


class PreparationFlowsTest(unittest.TestCase):
    def setUp(self):
        self.document=generate.load_json(ROOT/'data/static/flows/preparation.json')

    def walk(self,flow_id,start,choices=None,stop=None):
        flow=next(f for f in self.document['flows'] if f['id']==flow_id)
        nodes={n['id']:n for n in flow['nodes']};choices=copy.deepcopy(choices or {});trace=[];pc=start
        for _ in range(1000):
            if trace and pc==stop:return trace
            node=nodes[pc];trace.append(node)
            if not node['on']:return trace
            options=choices.get(node['op'],[])
            port=options.pop(0) if options else next(p for p in ('next','yes','slot','ack','no','cancel') if p in node['on'])
            pc=node['on'][port]
        self.fail('cold fixture did not terminate')

    def test_schema_contract_external_labels_and_no_native_service_handler(self):
        manifest=generate.load_json(ROOT/'data/static/manifest.json')
        generate.validate_flows(self.document)
        generate.validate_flow_contract(self.document,manifest['flow_contracts']['preparation.json'],set(),{})
        self.assertFalse(set(self.document['operations']) & {'shop_buy','shop_sell','church_revive','church_promote','equip_member','run_native','call_flow'})
        broken=copy.deepcopy(self.document);broken['flows'][0]['nodes'][0]['op']='shop_buy'
        with self.assertRaises(ValueError):generate.validate_flows(broken)

    def test_buy_equipment_cancel_after_append_still_pays_once(self):
        trace=self.walk('services','buy.pick',{'confirm':['yes','cancel'],'inventory_full':['no']},'buy.pick')
        operations=[n['op'] for n in trace]
        self.assertEqual([op for op in operations if op in ('item_append','equipment_set','money_sub')],['item_append','money_sub'])
        self.assertLess(operations.index('preflight'),operations.index('item_append'))
        self.assertLess(operations.index('item_append'),operations.index('service_animate'))
        self.assertLess(operations.index('service_animate'),operations.index('money_sub'))

    def test_sell_and_give_keep_distinct_write_orders(self):
        sell=self.walk('services','sell.member',{},'sell.member')
        self.assertEqual([n['op'] for n in sell if n['op'] in ('money_add','item_remove','recompute')],
                         ['money_add','item_remove','recompute'])
        give=self.walk('services','give.notice',{'inventory_full':['no']},'give.notice')
        self.assertEqual([n['op'] for n in give if n['op'] in ('item_capture','item_remove','item_append','recompute')],
                         ['item_capture','item_remove','item_append','recompute'])
        self.assertFalse(any(n['op'] in ('money_sub','money_add') for n in give))

    def test_revive_fee_flags_hp_before_music_and_rebuild_candidates(self):
        trace=self.walk('services','revive.build',{},'revive.build');ops=[n['op'] for n in trace]
        self.assertEqual([op for op in ops if op in ('money_sub','revive_flags_clear','revive_hp')],
                         ['money_sub','revive_flags_clear','revive_hp'])
        self.assertLess(ops.index('revive_hp'),ops.index('music'))
        self.assertEqual([n['args'] for n in trace if n['op']=='music'],[[17,1],[11,1]])

    def test_promotion_zero_skips_ack_commits_and_finalize_follows_close(self):
        trace=self.walk('services','promote.build',{'growth_nonzero':['no']*6},'promote.build')
        self.assertFalse(any(n['op'] in ('growth_commit','movement_commit','money_sub','money_add') for n in trace))
        self.assertEqual([n['args'] for n in trace if n['op']=='promotion_visual'],
                         [[0,1],[1,9],[2,16],[3,20],[4,200],[5,24],[6,0]])
        ops=[n['op'] for n in trace]
        self.assertLess(ops.index('promotion_item_consume'),ops.index('promotion_identity'))
        self.assertLess(ops.index('promotion_identity'),ops.index('recompute'))
        final=ops.index('promotion_finalize');self.assertEqual(ops[final-1],'close_page')
        trace=self.walk('services','promote.build',{'growth_nonzero':['yes']*6},'promote.build')
        for i,n in enumerate(trace):
            if n['op']=='growth_commit':
                # 原490..494自带-3等待，显示返回即写，不再另耗一键/RNG。
                self.assertEqual(trace[i-1]['op'],'show_message')
                self.assertEqual(trace[i-2]['op'],'clear_keys')
                self.assertIn(trace[i-1]['args'][1],range(490,495))
            if n['op']=='movement_commit':self.assertEqual(trace[i-1]['op'],'wait_key')
        movement=next(n for n in trace if n['id']=='promote.movement.say')
        self.assertEqual(movement['args'],[0,596,5])
        self.assertEqual({n['args'][2] for n in trace if n['id'].startswith('promote.growth.') and n['id'].endswith('.say')},{3})
        self.assertEqual(self.document['operations']['show_message']['args'][2]['max'],5)
        broken=copy.deepcopy(self.document)
        next(n for n in broken['flows'][0]['nodes'] if n['id']=='promote.movement.say')['args'][2]=6
        with self.assertRaises(ValueError):generate.validate_flows(broken)

    def test_equipment_loop_and_entry_music_order_are_cold(self):
        trace=self.walk('services','equip.member',{
            'inventory_empty':['no'],'equipment_compatible':['yes'],
            'pick_equipment_slot':['slot','cancel'],'pick_member':['slot','cancel'],
        },'service.return')
        self.assertEqual([n['op'] for n in trace if n['op'] in ('equipment_set','recompute','equipment_redraw','equipment_close')],
                         ['equipment_set','recompute','equipment_redraw','equipment_close'])
        self.assertEqual(next(n['args'] for n in trace if n['op']=='equipment_set'),[1])
        for selector,track in ((1,14),(3,15),(4,11),(5,14)):
            nodes={n['id']:n for n in self.document['flows'][0]['nodes']}
            trace=[];pc='open'
            while pc!='kind':
                node=nodes[pc];trace.append(node)
                if node['op']=='service_is_church':port='yes' if selector==4 else 'no'
                elif node['op']=='selector_is':port='yes' if node['args']==[selector] else 'no'
                else:port='next'
                pc=node['on'][port]
            self.assertEqual([n['args'] for n in trace if n['op']=='music'],[[-1,0],[track,0]])
            self.assertEqual([n['args'] for n in trace if n['op']=='sfx'],[[1,3]])
            branch='church' if selector==4 else 'drug' if selector==3 else 'other'
            route=['open','entry.sfx','entry.stop','entry.transition','entry.church']
            if selector!=4:route.append('entry.drug')
            route.extend([f'entry.music.{branch}','entry.present'])
            self.assertEqual([n['id'] for n in trace],route)
            self.assertEqual([n['op'] for n in trace[:4]],
                             ['service_resources','sfx','music','service_transition'])
            self.assertEqual(trace[-1]['op'],'service_enter')

    def test_sfx_known_bank_bounds_and_deployment_no_entry_cue(self):
        services=self.document['flows'][0]
        cue=next(n for n in services['nodes'] if n['op']=='sfx')
        for args in ([13,1],[1,0],[1,4],[-1,1]):
            broken=copy.deepcopy(self.document)
            next(n for n in broken['flows'][0]['nodes'] if n['id']==cue['id'])['args']=args
            with self.assertRaises(ValueError):generate.validate_flows(broken)
        self.assertFalse(any(n['op']=='sfx' for n in self.document['flows'][1]['nodes']))

    def test_notown_manual_save_carries_direct_input_source_role(self):
        flow=next(f for f in self.document['flows'] if f['id']=='deployment')
        save=next(n for n in flow['nodes'] if n['id']=='notown.save')
        self.assertEqual((save['op'],save['args']),('manual_save',[0]))
        self.assertEqual(self.document['operations']['manual_save']['args'],
                         [{'name':'caller_kind','min':0,'max':1}])
        trace=self.walk('deployment','notown.mode',{'confirm':['yes']},'select.quota')
        self.assertEqual([n['args'] for n in trace if n['op']=='manual_save'],[[0]])
        self.assertFalse(any(n['op']=='show_message' and n['args'][1]==660 for n in trace))
        for args in ([],[-1],[2]):
            broken=copy.deepcopy(self.document)
            f=next(f for f in broken['flows'] if f['id']=='deployment')
            next(n for n in f['nodes'] if n['id']=='notown.save')['args']=args
            with self.assertRaises(ValueError):generate.validate_flows(broken)

    def test_deployment_final_cancel_preserves_prior_sort_and_stage25_prefix_order(self):
        trace=self.walk('deployment','select.mode',{'stage_at_least':['no','no','no'],
            'stage_is':['no','no','no','no','no','no','no','yes','no','no','no','no','no','yes'],
            'confirm':['no']})
        # 路径断言用显式node支路，避免把runtime已排序状态当可回滚快照。
        nodes={n['id']:n for f in self.document['flows'] if f['id']=='deployment' for n in f['nodes']}
        self.assertEqual(nodes['prefix.25.first']['args'],[9]);self.assertEqual(nodes['prefix.25.second']['args'],[29])
        self.assertEqual(nodes['prefix.25.first']['on']['next'],'prefix.25.first.cache')
        self.assertEqual(nodes['prefix.25.first.cache']['on']['next'],'prefix.25.second')
        self.assertEqual(nodes['deploy.confirm']['on']['no'],'deploy.close.no')
        self.assertEqual(nodes['retry']['on']['no'],'rejected')
        self.assertTrue(any(n['op']=='selection_reorder' for n in trace))
        self.assertFalse(any('rollback' in n['op'] for n in trace))
        self.assertEqual(nodes['required.failure.say']['args'],[0,657,2])
        self.assertEqual(nodes['required.failure.ack']['args'],[0])

    def test_every_prefix_reorder_rebuilds_cache_before_next_prefix_or_dialog(self):
        # 原2b843不仅重排80B，还在返回前按新UID顺序重建11019目录。
        # stage25两次原调用分别重建，不能合成一次或藏进C业务叶。
        flow=next(f for f in self.document['flows'] if f['id']=='deployment')
        nodes={n['id']:n for n in flow['nodes']}
        expected={'prefix.text9':9,'prefix.text21':21,'prefix.text24':24,
                  'prefix.25.first':9,'prefix.25.second':29}
        self.assertEqual({n['id']:n['args'][0] for n in flow['nodes'] if n['op']=='prefix_reorder'},expected)
        for prefix in expected:
            next_node=nodes[nodes[prefix]['on']['next']]
            self.assertEqual(next_node['id'],prefix+'.cache')
            self.assertEqual(next_node['op'],'cache_rebuild')
            self.assertEqual(next_node['args'],[])
            self.assertEqual(next_node['on']['error'],'failure')
        stage25=self.walk('deployment','prefix.25.first',{},'deploy.open')
        self.assertEqual([(n['op'],n['args']) for n in stage25],
                         [('prefix_reorder',[9]),('cache_rebuild',[]),
                          ('prefix_reorder',[29]),('cache_rebuild',[])])
        for prefix in ('prefix.text9','prefix.text21','prefix.text24'):
            trace=self.walk('deployment',prefix,{},'deploy.open')
            self.assertEqual([n['op'] for n in trace],['prefix_reorder','cache_rebuild'])


if __name__=='__main__':unittest.main()
