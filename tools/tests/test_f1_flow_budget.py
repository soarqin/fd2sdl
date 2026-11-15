"""F1预算证明负例：独立手写图，拒绝漏call、假epoch和绕过counter门。"""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import audit_static_flow_budget as audit

def node(identity,op='noop',on=None,args=None):
    return {'id':identity,'op':op,'args':args or [],'on':on or {}}

def flow(identity,nodes):return {'id':identity,'entry':nodes[0]['id'],'nodes':nodes}
def document(domain,flows):return {'domain':domain,'flows':flows}
def scene_docs(flows):return {'scene':document('scene',[]),'stage':document('stage',flows)}
def line(identity,length):
    return flow(identity,[node(str(i),on={'next':str(i+1)})if i+1<length else node(str(i),'end')for i in range(length)])

class FlowBudgetProof(unittest.TestCase):
    def source_copy(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        root=Path(temporary.name)
        for name in audit.audit(ROOT)['inputs']:
            path=Path(name);target=root/path.relative_to(ROOT)
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        return root

    def test_actual_sources_prove_all_existing_budget_domains_separately(self):
        result=audit.audit(ROOT)
        self.assertTrue(result['scene_budget']['proved']);self.assertTrue(result['other_budgets']['proved'])
        self.assertFalse(result['unknown_dynamic_backedges']);self.assertFalse(result['whole_F1_complete'])
        self.assertLessEqual(result['scene_budget']['max_epoch_steps'],65536)
        self.assertLessEqual(result['scene_budget']['max_depth_frames'],8)
        choice=next(r for r in result['other_budgets']['flows']if r['domain']=='runtime'and r['id']=='choice')
        self.assertFalse(choice['budget_rejection_exists']);self.assertFalse(choice['finite_duration_claimed'])

    def test_change_after_contract_read_cannot_replace_the_signed_input_hash(self):
        root=self.source_copy();real=audit.consumer_contracts
        def change_after_contracts(current,read_source=None):
            result=real(current,read_source)
            header=current/'src/static_flow.h'
            header.write_bytes(header.read_bytes().replace(b'65536u',b'1u'))
            return result
        with patch.object(audit,'consumer_contracts',side_effect=change_after_contracts):
            with self.assertRaisesRegex(ValueError,'audit inputs changed during read'):
                audit.audit(root)

    def test_transient_second_read_cannot_audit_different_text_under_original_hash(self):
        root=self.source_copy();header=root/'src/static_flow.h';original=header.read_bytes()
        real=audit.consumer_contracts
        def changed_between_reads(current,read_source=None):
            # 首次snapshot冻结A；磁盘暂为B时合同必须仍审A，末尾回A。
            read_source(header)
            header.write_bytes(original.replace(b'65536u',b'1u'))
            try:return real(current,read_source)
            finally:header.write_bytes(original)
        with patch.object(audit,'consumer_contracts',side_effect=changed_between_reads):
            result=audit.audit(root)
        self.assertTrue(result['scene_budget']['proved']);self.assertTrue(result['sources_unchanged'])
        self.assertEqual(result['inputs'][str(header.resolve())]['sha256'],hashlib.sha256(original).hexdigest())

    def test_standard_calls_share_parent_budget_and_are_fully_charged(self):
        root=flow('root',[node('a','call_fade_in',{'next':'b'}),node('b','call_fade_in',{'next':'end'}),node('end','end')])
        with patch.object(audit,'BUDGET',10),self.assertRaisesRegex(ValueError,'step budget'):
            audit.scene_proof(scene_docs([root,line('stage.source.fade.in',5)]))

    def test_cinematic_calls_get_local_budget_but_keep_depth(self):
        root=flow('root',[node('a','call_cinematic_ending',{'next':'b'}),node('b','call_cinematic_ending',{'next':'end'}),node('end','end')])
        with patch.object(audit,'BUDGET',8):
            result=audit.scene_proof(scene_docs([root,line('stage.cinematic.ending',6)]))
        row=next(r for r in result['flows']if r['id']=='root')
        self.assertEqual(row['max_epoch_steps'],3);self.assertEqual(row['max_depth_frames'],2)
        self.assertEqual(result['max_epoch_steps'],6)

    def test_credits_reset_node_is_charged_before_reset(self):
        credits=flow('stage.cinematic.credits',[node('epoch','cin_credits_epoch',{'next':'a'}),
            node('a',on={'next':'b'}),node('b',on={'next':'epoch'})])
        with patch.object(audit,'BUDGET',3):
            self.assertEqual(audit.scene_proof(scene_docs([credits]))['max_epoch_steps'],3)
        with patch.object(audit,'BUDGET',2),self.assertRaisesRegex(ValueError,'step budget'):
            audit.scene_proof(scene_docs([credits]))

    def test_epoch_is_not_an_arbitrary_escape_from_cycle_checks(self):
        wrong=flow('root',[node('loop','cin_credits_epoch',{'next':'loop'})])
        with self.assertRaisesRegex(ValueError,'outside credits'):audit.scene_proof(scene_docs([wrong]))
        wrong=flow('root',[node('loop',on={'next':'loop'})])
        with self.assertRaisesRegex(ValueError,'cycle'):audit.scene_proof(scene_docs([wrong]))

    def test_ninth_stack_frame_fails_even_if_all_local_budgets_are_small(self):
        flows=[flow(f'scene.opening.{i:03d}',[node('go','opening_stage',{'next':'end'},[i+1]),node('end','end')])for i in range(8)]
        docs={'scene':document('scene',flows+[line('scene.opening.008',1)]),'stage':document('stage',[])}
        with self.assertRaisesRegex(ValueError,'depth'):audit.scene_proof(docs)
        docs['scene']['flows']=flows[:7]+[line('scene.opening.007',1)]
        self.assertEqual(audit.scene_proof(docs)['max_depth_frames'],8)

    def test_unknown_and_missing_callees_are_not_opaque_one_step_leaves(self):
        for op in ('call_missing','call_fade_in'):
            with self.subTest(op=op),self.assertRaises(ValueError):
                audit.scene_proof(scene_docs([flow('root',[node('call',op)])]))

    def test_empty_picker_cancel_cannot_fake_a_real_input_epoch(self):
        services=flow('services',[node('pick','pick_member',{'cancel':'pick','slot':'end'}),node('end','finish')])
        docs={'preparation':document('preparation',[services]),'common_event':document('common_event',[])}
        with self.assertRaisesRegex(ValueError,'cycle'):audit.other_budget_proof(docs)

    def test_counter_loop_must_not_bypass_its_comparison(self):
        source=json.loads((ROOT/'data/static/flows/loot.json').read_text(encoding='utf8'))
        damaged=copy.deepcopy(source)
        loop=next(f for f in damaged['flows']if f['id']=='loot')
        next(n for n in loop['nodes']if n['op']=='collect_next')['on']['next']='collect.next'
        with self.assertRaisesRegex(ValueError,'finite guard'):
            audit.other_budget_proof({'loot':damaged,'common_event':document('common_event',[])})

    def test_repeated_reset_cannot_be_called_a_monotone_counter(self):
        nodes={'reset':node('reset','reward_cursors_reset',{'next':'step'}),
            'step':node('step','reward_growth_next',{'next':'guard'}),
            'guard':node('guard','reward_growth_available',{'yes':'reset'},[5])}
        with self.assertRaisesRegex(ValueError,'reset repeats'):
            audit.counter_contract(nodes,{'reward_notice'},{'reward_cursors_reset'},
                {'reward_growth_next':('reward_growth_available',[5])})

    def test_unknown_cross_owner_call_is_blocking(self):
        docs={'common_event':document('common_event',[flow('broken',[node('x','call_unknown')])])}
        with self.assertRaisesRegex(ValueError,'cross-owner call'):audit.other_budget_proof(docs)

    def test_projection_join_is_counted_once_per_pending_path(self):
        graph=flow('diamond',[node('fork','result_leader_dead',{'yes':'a','no':'b'}),
            node('a',on={'next':'end'}),node('b',on={'next':'end'}),node('end','end')])
        result=audit.projection_budget_proof(document('common_event',[graph]))
        self.assertEqual(result['max_expanded_total_steps'],5)
        self.assertEqual(result['max_pending_slots'],1)

    def test_projection_shared_tail_can_exhaust_total_steps_with_short_paths(self):
        nodes=[node(str(i),'branch_actor',{'yes':str(i+1),'no':str(i+1)})for i in range(16)]
        nodes.append(node('16','end'))
        with self.assertRaisesRegex(ValueError,'worklist budget'):
            audit.projection_budget_proof(document('common_event',[flow('explosion',nodes)]))

    def test_projection_pending_limit_is_distinct_from_total_steps(self):
        def graph(count):
            return flow('queue',[node(str(i),'branch_actor',{'yes':str(i+1),'no':'end'})for i in range(count)]+
                [node(str(count),on={'next':'end'}),node('end','end')])
        result=audit.projection_budget_proof(document('common_event',[graph(64)]))
        self.assertEqual(result['max_pending_slots'],64)
        with self.assertRaisesRegex(ValueError,'worklist budget'):
            audit.projection_budget_proof(document('common_event',[graph(65)]))

if __name__=='__main__':unittest.main()
