"""Arrival冷图的独立原caller轨迹门；合成source摘要只用于拒绝负例。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('arrival_source',ROOT/'tools/ida/verify_fd2_arrival_owner.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)


class ArrivalColdSourceTest(unittest.TestCase):
    def document(self):
        return json.loads((ROOT/'data/static/flows/runtime.json').read_text(encoding='utf-8'))

    def original(self,empty=False):
        # 仅给compare_flow的最小协议样本；真实两路CPU另由CLI执行。
        return {'project_count':12,'empty_group':empty,'events':[
            {'call':'0x25a96','frame_after_project':0},
            {'call':'0x127e0','frame_after_project':6},
            {'call':'0x11eee','frame_after_project':7},
            {'call':'0x127a9','frame_after_project':8}]}

    def flow(self,document):
        return next(f for f in document['flows'] if f['id']=='runtime.arrival')

    def test_current_trace_has_source53_steps_for_normal_and_empty(self):
        for empty in (False,True):
            self.assertEqual(module.compare_flow(self.document(),self.original(empty))['nodes'],53)

    def test_missing_clear_and_wait_reordering_fail(self):
        for swap in (False,True):
            document=self.document();flow=self.flow(document);nodes={n['id']:n for n in flow['nodes']}
            if swap:
                nodes['project.00']['on']['next']='wait.00'
                nodes['wait.00']['on']['next']='clear.00'
                nodes['clear.00']['on']['next']='cue'
            else:nodes['project.00']['on']['next']='wait.00'
            with self.assertRaisesRegex(ValueError,'原完整caller顺序'):
                module.compare_flow(document,self.original())

    def test_postbase_must_follow_project_and_precede_clear(self):
        document=self.document();nodes={n['id']:n for n in self.flow(document)['nodes']}
        nodes['project.06']['on']['next']='clear.06';nodes['clear.06']['on']['next']='base.06';nodes['base.06']['on']['next']='wait.06'
        with self.assertRaisesRegex(ValueError,'原完整caller顺序'):
            module.compare_flow(document,self.original())

    def test_empty_group_cannot_skip_whole_sequence(self):
        document=self.document();self.flow(document)['entry']='done'
        with self.assertRaisesRegex(ValueError,'原完整caller顺序'):
            module.compare_flow(document,self.original(True))

    def test_bad_exit_loop_or_missing_flow_is_rejected(self):
        for kind in range(3):
            document=self.document();flow=self.flow(document)
            if kind==0:flow['nodes'][0]['on']['next']='missing'
            elif kind==1:flow['nodes'][0]['on']['next']=flow['nodes'][0]['id']
            else:document['flows'].remove(flow)
            with self.assertRaises(ValueError):module.compare_flow(document,self.original())

    def test_wrong_original_identity_fails_before_optional_cpu_dependency(self):
        with self.assertRaisesRegex(ValueError,'strict镜像身份'):
            module.check(b'wrong image',b'wrong fdother')
        image=bytes(0x32d18)
        with patch.object(module,'IMAGE_SHA',module.hashlib.sha256(image).hexdigest()):
            with self.assertRaisesRegex(ValueError,'完整字节'):
                module.check(image,b'wrong fdother')


if __name__=='__main__':unittest.main()
