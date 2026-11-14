"""S3逐绑定原码工具拒绝无caller、错目标与越界提升；不冒充资源/CPU门。"""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0, str(ROOT / 'tools/static_data'))
spec = importlib.util.spec_from_file_location('stage_release_traces', ROOT / 'tools/static_data/verify_stage_release_traces.py')
traces = importlib.util.module_from_spec(spec)
spec.loader.exec_module(traces)


class ReleaseTraceTest(unittest.TestCase):
    def setUp(self):
        self.row = {'slot': 5, 'definition': 'callback5', 'caller_sources': ['stage:12:turn:00000'], 'blocker': None}

    def test_same_target_does_not_grant_another_stage_caller(self):
        doc = {'bindings': {'stage.009.action.005': 'callback5'}}
        with self.assertRaisesRegex(ValueError, '原caller'):
            list(traces.bound_callbacks(doc, [self.row]))

    def test_exact_binding_keeps_source_stage_and_slot(self):
        doc = {'bindings': {'stage.012.action.005': 'callback5', 'stage.000.action.000': 'legacy'}}
        self.assertEqual(list(traces.bound_callbacks(doc, [self.row])), [('stage.012.action.005', 12, self.row)])

    def test_bad_target_blocker_and_unpublished_stage_refuse(self):
        for definition, row, key in [('wrong', self.row, 'stage.012.action.005'),
                                     ('callback5', {**self.row, 'blocker': {'reason': 'unknown'}}, 'stage.012.action.005'),
                                     ('callback5', self.row, 'stage.030.action.005')]:
            with self.subTest(key=key, definition=definition), self.assertRaises(ValueError):
                list(traces.bound_callbacks({'bindings': {key: definition}}, [row]))


if __name__ == '__main__':
    unittest.main()
