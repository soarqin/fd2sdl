"""S5完整原caller前缀协议：不把错误分支/布尔/旧text bank或cache当相同。"""
from pathlib import Path
import copy
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'ida'))
import verify_s5_stage_callers as subject


def pair(stage=22, variant=0, turn=15):
    stories = {20: [5, 7], 22: [9, 12], 26: [8, 9, 10, 11, 12], 28: [10, 11], 29: [9, 10, 0]}[stage]
    hot_stage = {26: 27, 29: 30}.get(stage, stage)
    raw20 = [126, 126, 1, 0] if stage == 28 else [0, 20, 0, 0]
    run = dict(stage=stage, variant=variant, turn=turn, stories=stories, hot_stage=hot_stage,
               roster_count=20, raw20=raw20, visual_timing=[])
    row = dict(run, result=0 if stage == 26 and variant == 3 else -1, count=64, rng_calls=0,
               raw20_cache_before=0, NoTown=int(stage in (22, 26, 28)), export_preparation=-1,
               prepared_stage=0, text_entries=[30, 31] if stage == 29 else [stage+1])
    return row, run


class CallerProtocol(unittest.TestCase):
    def test_exact_source_prefix_and_current_source_only_gate(self):
        for stage, variant in ((20, 6), (22, 0), (26, 3), (28, 0), (29, 0)):
            row, run = pair(stage, variant)
            self.assertTrue(subject.validate_probe([row], [run])['all_declared_branch_write_and_timing_observations_equal'])

    def test_counts_keys_and_exact_integer_types(self):
        row, run = pair()
        self.assertRaises(ValueError, subject.validate_probe, [], [run])
        for key in ('turn', 'result', 'rng_calls'):
            changed = copy.deepcopy(row); changed[key] = False
            self.assertRaises(ValueError, subject.validate_probe, [changed], [run])
        changed = copy.deepcopy(row); changed['stories'][0] = True
        self.assertRaises(ValueError, subject.validate_probe, [changed], [run])
        changed = dict(row, extra=1)
        self.assertRaises(ValueError, subject.validate_probe, [changed], [run])

    def test_signed_turn_and_roster_branch_story_mismatch_rejected(self):
        row, run = pair()
        for wrong in ([9, 13], [9, 10], [8, 12]):
            self.assertRaises(ValueError, subject.validate_probe, [dict(row, stories=wrong)], [run])
        self.assertRaises(ValueError, subject.validate_probe, [dict(row, turn=0xffffffff)], [run])

    def test_actual_death_cache_write_and_no_rng(self):
        row, run = pair(28)
        for name, wrong in (('raw20', [126, 126, 1, 1]), ('raw20', [126, 126, 0, 0]),
                            ('rng_calls', 1), ('raw20_cache_before', 2)):
            self.assertRaises(ValueError, subject.validate_probe, [dict(row, **{name: wrong})], [run])

    def test_old_and_new_text_resource_owner_are_distinct(self):
        for stage, wrong in ((22, [24]), (29, [30]), (29, [31]), (29, [31, 30])):
            row, run = pair(stage)
            self.assertRaises(ValueError, subject.validate_probe, [dict(row, text_entries=wrong)], [run])

    def test_good26_no_town_export_never_bypassed(self):
        row, run = pair(26, 3)
        for change in (dict(NoTown=0), dict(export_preparation=1), dict(result=-1),
                       dict(export_preparation=0, prepared_stage=26)):
            self.assertRaises(ValueError, subject.validate_probe, [dict(row, **change)], [run])
        self.assertTrue(subject.validate_probe([dict(row, export_preparation=0, prepared_stage=27)], [run]))

    def test_actual_third_pulse_wait_cannot_be_dropped_or_boolean(self):
        row, run = pair(28, 1)
        run['visual_timing'] = [[1, 0, 255, 0], [2, 8, 0, 0], [2, 300, 0, 0]]
        row['visual_timing'] = copy.deepcopy(run['visual_timing'])
        subject.validate_probe([row], [run])
        self.assertRaises(ValueError, subject.validate_probe,
                          [dict(row, visual_timing=row['visual_timing'][:-1])], [run])
        changed = copy.deepcopy(row); changed['visual_timing'][2][1] = True
        self.assertRaises(ValueError, subject.validate_probe, [changed], [run])


if __name__ == '__main__': unittest.main()
