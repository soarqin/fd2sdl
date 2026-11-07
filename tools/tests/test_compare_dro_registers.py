import importlib.util
from pathlib import Path
import unittest


spec = importlib.util.spec_from_file_location('compare_dro_registers', Path(__file__).resolve().parents[1] / 'compare_dro_registers.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def row(index, register, value, sample=0, phase='install', tick=0):
    return dict(ordinal=index, sample=sample, phase=phase, timer_tick=tick, register=register, value=value)


def trace():
    return [row(0, 1, 32), row(1, 0x105, 1), row(2, 0xb0, 1),
        row(3, 0xb0, 33, 400, 'render', 1), row(4, 1, 32, 400, 'render', 1),
        row(5, 0xb0, 1, 800, 'render', 2), row(6, 0xb0, 1, 1200, 'retire', 3)]


def meta(rows):
    return dict(schema='fd2-title18-offline-opl-v1', track=18,
        sample_rate=48000, frames=1200, generated_samples=1200, writes=len(rows),
        timer_ticks=max(r['timer_tick'] for r in rows if r['phase'] in ('render', 'retire')),
        hook_enabled=True, overflow=False,
        phase_counts={phase: sum(row['phase'] == phase for row in rows) for phase in module.PHASES})


def native(values=None, times=None):
    values = values or [(1, 32), (0x105, 1), (0xb0, 1), (0xb0, 33), (0xb0, 1)]
    times = times or [0, 0, 0, 0, 8]
    return dict(ok=True, writer_consistent=True, conversion_table_bytes=bytes([1, 5, 0xb0, 0xbd]).hex(),
        commands=[dict(kind='write', register=register, value=value, time_ms=times[index], pair_index=index)
                  for index, (register, value) in enumerate(values)])


class RegisterGateTest(unittest.TestCase):
    def test_exact_source_writer_projection_and_retire_boundary(self):
        rows = trace()
        result = module.compare(native(), rows, meta(rows))
        self.assertTrue(result['native_register_prefix_passed'])
        self.assertEqual(result['projection']['model_cache_prefix_count'], 3)
        self.assertEqual(result['projection']['ignored']['same_value_raw_writes'], 1)
        self.assertEqual(result['projection']['ignored']['retire_raw_writes'], 1)
        self.assertFalse(result['scope']['retire_matches_native_stop'])

    def test_cache_uses_pre_trigger_value_and_interleaved_banks(self):
        rows = [row(0, 1, 32), row(1, 0x101, 7), row(2, 0xb0, 3), row(3, 0xb0, 35, 400, 'render', 1)]
        projected = module.project_capture(rows, {1, 0xb0})['writes']
        self.assertEqual([(r['register'], r['value']) for r in projected], [(1, 32), (0x101, 7), (0xb0, 3), (0xb0, 35)])

    def test_bd_mask_is_bank_zero_only(self):
        rows = [row(0, 0xbd, 0x20), row(1, 0x1bd, 0x11), row(2, 0xb0, 32, 400, 'render', 1)]
        output = module.project_capture(rows, {0xbd, 0xb0})['writes']
        self.assertEqual([(r['register'], r['value']) for r in output], [(0xbd, 0x20), (0x1bd, 0x11), (0xb0, 32)])

    def test_module_timer_registers_are_consumed_before_capture_cache(self):
        rows = [row(0, 2, 7), row(1, 3, 9), row(2, 4, 96),
                row(3, 0xb0, 32, 400, 'render', 1),
                row(4, 2, 8, 500, 'render', 1), row(5, 3, 10, 500, 'render', 1),
                row(6, 4, 128, 500, 'render', 1)]
        result = module.project_capture(rows, {2, 3, 4, 0xb0})
        self.assertEqual([(r['register'], r['value']) for r in result['writes']], [(0xb0, 32)])
        self.assertEqual(result['ignored']['module_timer_intercepted_raw_writes'], 6)

    def test_bank_one_same_low_registers_are_not_bank_zero_timers(self):
        rows = [row(0, 0x102, 7), row(1, 0x103, 9), row(2, 0x104, 1),
                row(3, 0xb0, 32, 400, 'render', 1), row(4, 0x104, 2, 500, 'render', 1)]
        result = module.project_capture(rows, {2, 3, 4, 0xb0})
        self.assertEqual([(r['register'], r['value']) for r in result['writes']],
            [(0x102, 7), (0x103, 9), (0x104, 1), (0xb0, 32), (0x104, 2)])
        self.assertNotIn('module_timer_intercepted_raw_writes', result['ignored'])

    def test_changed_value_rejected_without_mask(self):
        rows = trace()
        rows[5]['value'] = 2
        result = module.compare(native(), rows, meta(rows))
        self.assertFalse(result['ok'])
        self.assertEqual(result['first_mismatch']['write_index'], 4)

    def test_order_not_sorted_or_edit_distance_aligned(self):
        rows = trace()[:6] + [row(6, 0xc0, 3, 1000, 'render', 3)]
        original = native([(1, 32), (0x105, 1), (0xb0, 1), (0xb0, 33), (0xc0, 3), (0xb0, 1)], [0, 0, 0, 0, 1, 8])
        result = module.compare(original, rows, meta(rows))
        self.assertFalse(result['ok'])
        self.assertEqual(result['exact_common_prefix_writes'], 4)

    def test_missing_trace_records_fail_not_inferred(self):
        for field, value in [('ordinal', 9), ('sample', True), ('register', 0x200), ('value', 256)]:
            with self.subTest(field=field):
                rows = trace()
                rows[3][field] = value
                with self.assertRaises(ValueError): module.compare(native(), rows, meta(rows))

    def test_nonmonotonic_and_bad_phase_count_fail(self):
        rows = trace()
        rows[5]['sample'] = 1
        with self.assertRaises(ValueError): module.compare(native(), rows, meta(rows))
        rows = trace()
        declaration = meta(rows)
        declaration['phase_counts']['render'] = 99
        with self.assertRaises(ValueError): module.compare(native(), rows, declaration)

    def test_hook_and_overflow_fail(self):
        for change in [dict(hook_enabled=False), dict(overflow=True), dict(hook_enabled=1), dict(overflow=0)]:
            with self.subTest(change=change):
                declaration = meta(trace())
                declaration.update(change)
                with self.assertRaises(ValueError): module.compare(native(), trace(), declaration)

    def test_metadata_omission_or_mismatch_cannot_accept_complete_trace(self):
        changes = [dict(writes=8), dict(writes=True), dict(generated_samples=1199),
            dict(generated_samples=True), dict(timer_ticks=2), dict(timer_ticks=True),
            dict(schema='other'), dict(track=19), dict(track=True),
            dict(phase_counts={'install':3,'render':3}),
            dict(phase_counts={'install':3,'render':3,'retire':True})]
        for change in changes:
            with self.subTest(change=change):
                declaration = meta(trace())
                declaration.update(change)
                with self.assertRaises(ValueError): module.compare(native(), trace(), declaration)

    def test_pic_clock_variation_does_not_claim_timing_pass(self):
        result = module.compare(native(times=[0, 0, 0, 0, 99]), trace(), meta(trace()))
        self.assertTrue(result['native_register_prefix_passed'])
        self.assertFalse(result['scope']['timing_passed'])
        self.assertFalse(result['scope']['timing_tolerance_applied'])
        self.assertFalse(result['scope']['entire_A7_passed'])

    def test_exact_quantization_phase_interval_and_empty_boundary(self):
        original = [dict(time_ms=0),dict(time_ms=1)]
        generated = [dict(sample=10),dict(sample=11)]
        result = module.quantization_diagnostic(original, generated, 0, 2, 10, 2000)
        self.assertTrue(result['nonempty_phase_interval'])
        self.assertEqual(result['phase_interval_lower_inclusive_numerator'], 1000)
        self.assertEqual(result['phase_interval_upper_exclusive_numerator'], 2000)
        original[1]['time_ms'] = 2
        result = module.quantization_diagnostic(original, generated, 0, 2, 10, 1000)
        self.assertFalse(result['nonempty_phase_interval'])
        self.assertEqual(result['signed_interval_width_numerator'], 0)
        self.assertEqual(result['first_empty_prefix_write_index'], 1)

    def test_first_keyon_uniquely_separates_cache_from_same_ms_live(self):
        result = module.compare(native(times=[0, 0, 0, 0, 0]), trace(), meta(trace()))
        self.assertEqual(result['native_capture_boundary']['cache_prefix_writes'], 3)
        self.assertEqual(result['native_capture_boundary']['first_trigger']['value'], 33)
        self.assertTrue(result['native_capture_boundary']['uniquely_reconstructed_from_writer'])

    def test_bad_cache_or_bank_one_bd_ambiguity_rejected(self):
        with self.assertRaises(ValueError):
            module.native_boundary(native([(0x105, 1), (1, 32), (0xb0, 33)], [0, 0, 0]))
        with self.assertRaises(ValueError):
            module.native_boundary(native([(1, 32), (0x1bd, 0x21), (0xb0, 33)], [0, 0, 0]))

    def test_short_candidate_is_explicitly_inconclusive(self):
        rows = trace()[:5]
        result = module.compare(native(), rows, meta(rows))
        self.assertEqual(result['status'], 'inconclusive_trace_too_short')
        self.assertFalse(result['ok'])

    def test_long_candidate_tail_remains_unvalidated(self):
        rows = trace()[:6] + [row(6, 0xb0, 33, 1000, 'render', 3)]
        result = module.compare(native(), rows, meta(rows))
        self.assertTrue(result['ok'])
        self.assertEqual(result['candidate_unvalidated_tail_writes'], 1)

    def test_unmapped_writes_do_not_displace_mapped_sequence(self):
        rows = trace()[:6] + [row(6, 0x10, 22, 1000, 'render', 3)]
        result = module.compare(native(), rows, meta(rows))
        self.assertTrue(result['ok'])
        self.assertEqual(result['projection']['ignored']['unmapped_raw_writes'], 1)


if __name__ == '__main__':
    unittest.main()
