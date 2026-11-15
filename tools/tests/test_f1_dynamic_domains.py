"""动态join的纯合成回归；不依赖原资源、IDA或Unicorn。"""
import importlib.util
from pathlib import Path
import sys
import unittest

DIRECTORY = Path(__file__).resolve().parents[1] / 'ida'
sys.path.insert(0, str(DIRECTORY))
import audit_f1_dynamic_domains as module
import verify_f1_stage11_order as stage11


def graph(nodes):
    return {'entry': 'gate', 'nodes': nodes}


class DynamicDomainsTests(unittest.TestCase):
    def test_mutually_exclusive_groups_not_double_counted(self):
        flow = graph([
            {'id': 'gate', 'op': 'branch', 'args': [], 'on': {'yes': 'a', 'no': 'b'}},
            {'id': 'a', 'op': 'append_group', 'args': [3, 0], 'on': {'next': 'done'}},
            {'id': 'b', 'op': 'append_group', 'args': [3, 0], 'on': {'next': 'done'}},
            {'id': 'done', 'op': 'end', 'args': []}])
        self.assertEqual(module.paths_max_groups(flow), {3: 1})
        flow['nodes'][1]['on']['next'] = 'b'
        self.assertEqual(module.paths_max_groups(flow), {3: 2})

    def test_cycle_missing_duplicate_and_dynamic_stage_rejected(self):
        for nodes in [
            [{'id': 'gate', 'op': 'end', 'args': [], 'on': {'next': 'gate'}}],
            [{'id': 'gate', 'op': 'end', 'args': [], 'on': {'next': 'absent'}}],
            [{'id': 'gate', 'op': 'end', 'args': []}] * 2,
            [{'id': 'gate', 'op': 'append_group_source', 'args': [5, 0]}],
        ]:
            with self.assertRaises(ValueError):
                module.paths_max_groups(graph(nodes))

    def test_original_al_wrap_and_unknown_source(self):
        self.assertEqual([module.source_value(s, t) for t in [13, 15, 18, 22] for s in [3, 4]],
                         [254, 255, 2, 3, 8, 9, 16, 17])
        self.assertEqual(module.source_value(2, 7), 3)
        with self.assertRaises(ValueError):
            module.source_value(5, 7)

    def test_state_counter_bounds_not_number_of_drop_callers(self):
        self.assertEqual(module.group_bound(64, {}, []), {3: 1, 4: 1, 5: 1})
        self.assertEqual(module.group_bound(74, {}, []), {4: 1, 5: 1, 6: 1, 7: 1})
        self.assertEqual(module.group_bound(82, {}, []), {1: 1, 2: 1, 3: 1})

    def test_caller_full_word_and_effective_template_prefix(self):
        template = bytearray(26)
        template[21:25] = bytes([1, 2, 37, 0])
        meta = {'turn': [(1, 37, 0)], 'lookup': [(37, 1)],
                'actions': [(2, 293), (2, 37)], 'unknown_02': 1,
                'templates': [template, template]}
        result = module.callers(meta, 37)
        self.assertEqual([r['kind'] for r in result], ['turn', 'lookup', 'cell_descriptor', 'drop'])
        self.assertEqual(result[-2]['slot'], 1)
        self.assertEqual(result[-1]['template'], 0)

    def test_movement_pairs_complete_not_distinct_reference_coverage(self):
        self.assertEqual(module.script_actors(bytes([2, 1, 2, 3, 0, 4, 1, 0x80, 1, 3, 5])), [3, 4])
        for raw in [bytes([0, 1]), bytes([1]), bytes([1, 1, 2, 3, 0]), b'']:
            with self.assertRaises(ValueError):
                module.script_actors(raw)

    def test_flow_or_binding_drift_does_not_inherit_manual_policy(self):
        with self.assertRaises(ValueError):
            module.verify_reviewed_scope({}, {})

    def test_inactive_allocation_is_not_a_footstep_table_bound(self):
        self.assertIsNone(stage11.footstep_table_boundary(1, 1000))
        self.assertIsNone(stage11.footstep_table_boundary(29, 1000))
        self.assertEqual(stage11.footstep_table_boundary(0, 1000)['actual_read_address'], hex(999))
        self.assertEqual(stage11.footstep_table_boundary(165, 1000)['actual_read_address'], hex(1164))
        with self.assertRaises(ValueError):
            stage11.footstep_table_boundary(256, 1000)



if __name__ == '__main__':
    unittest.main()
