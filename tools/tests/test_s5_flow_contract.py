"""S5固定Scene子图容量/调用资格/终态与预算复位的拒绝门。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('s5_flow_generator',ROOT/'tools/static_data/generate.py')
generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)


class S5FixedFlowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document=json.loads((ROOT/'data/static/flows/stage.json').read_bytes())

    def test_original_fixed_call_graph_is_bounded_and_valid(self):
        generator.validate_stage_source_calls(self.document)
        generator.validate_flows(self.document)

    def test_cinematic_cannot_borrow_unqualified_ani1_caller(self):
        bad=copy.deepcopy(self.document)
        flow=next(f for f in bad['flows'] if f['id']=='stage.cinematic.ending')
        next(n for n in flow['nodes'] if n['op']=='cin_animation')['args'][0]=1
        with self.assertRaisesRegex(ValueError,'ANI outside confirmed'):
            generator.validate_stage_source_calls(bad)

    def test_card_shared_prefix_cannot_be_called_from_another_source(self):
        bad=copy.deepcopy(self.document)
        flow=next(f for f in bad['flows'] if f['id']=='stage.cinematic.card_normal')
        flow['id']='stage.source.victory.025'
        with self.assertRaisesRegex(ValueError,'outside original caller'):
            generator.validate_stage_source_calls(bad)

    def test_fixed_call_cannot_accept_an_arbitrary_target_argument(self):
        bad=copy.deepcopy(self.document)
        bad['operations']['call_cinematic_card_first']['args']=[{'name':'target','min':0,'max':255}]
        with self.assertRaisesRegex(ValueError,'call ABI'):
            generator.validate_stage_source_calls(bad)

    def test_budget_epoch_is_only_for_original_credits_outer_loop(self):
        bad=copy.deepcopy(self.document)
        flow=next(f for f in bad['flows'] if f['id']=='stage.cinematic.card_normal')
        next(n for n in flow['nodes'] if n['op']=='call_fade_out')['op']='cin_credits_epoch'
        with self.assertRaisesRegex(ValueError,'epoch outside'):
            generator.validate_stage_source_calls(bad)

    def test_other_visual_cycles_cannot_bypass_the_original_epoch(self):
        bad=copy.deepcopy(self.document)
        flow=next(f for f in bad['flows'] if f['id']=='stage.cinematic.card_normal')
        node=next(n for n in flow['nodes'] if n['op']=='call_fade_out')
        node['on']['next']=node['id']
        with self.assertRaisesRegex(ValueError,'cycle lacks'):
            generator.validate_stage_source_calls(bad)

    def test_original_terminal_has_no_success_or_error_return_port(self):
        bad=copy.deepcopy(self.document)
        flow=next(f for f in bad['flows'] if f['id']=='stage.source.victory.029')
        node=next(n for n in flow['nodes'] if n['op']=='terminal_wait')
        node['on']={'error':'failed'}
        with self.assertRaisesRegex(ValueError,'terminal loop'):
            generator.validate_flows(bad)

    def test_wide_source_pc_does_not_expand_unqualified_stage_domains(self):
        bad=copy.deepcopy(self.document)
        flow=next(f for f in bad['flows'] if f['id']=='stage.source.entry.024')
        self.assertGreater(len(flow['nodes']),254)
        flow['id']='stage.source.entry.unqualified'
        with self.assertRaisesRegex(ValueError,'node capacity'):
            generator.validate_flows(bad)


if __name__=='__main__':unittest.main()
