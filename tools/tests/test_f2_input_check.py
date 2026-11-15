"""F2长按工具的纯协议/隔离负例；不启动driver、窗口、显示API或游戏。"""
from contextlib import ExitStack, contextmanager
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from PIL import Image

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import run_f2_input_check as runner
import run_original_comparison as comparison


def receipt(pid, held_ms):
    return {'backend': 'PostMessageW HWND', 'pid': pid, 'hwnd': '0x123',
        'ok': True, 'error': 0, 'kind': 'key', 'keys': 1, 'sent': 1,
        'key_down_sent': True, 'key_up_sent': True, 'release': 1,
        'down_tick_ms': 10000, 'up_tick_ms': 10000 + held_ms,
        'down_qpc': 10000000, 'up_qpc': 10000000 + held_ms * 1000,
        'qpc_frequency': 1000000, 'qpc_valid': True,
        'foreground_guard_ok': True, 'noactivate_verified': True,
        'release_retries': 0}


class F2InputCheckTest(unittest.TestCase):
    def test_qpc_does_not_fall_back_to_coarse_ticks_or_missing_release(self):
        row=receipt(91,1176)
        row['up_tick_ms']=row['down_tick_ms']+1188
        self.assertEqual(runner.qpc_prediction(row,1176),(1176000000,22,16000000))
        for field,value in (('qpc_valid',False),('down_qpc',True),('up_qpc',0),
                ('qpc_frequency',0),('key_up_sent',False),('release',0)):
            with self.subTest(field=field),self.assertRaises(ValueError):
                runner.qpc_prediction(dict(row,**{field:value}),1176)

    def test_qpc_threshold_distance_is_checked_before_millisecond_rounding(self):
        row=receipt(91,1176)
        row['up_qpc']=row['down_qpc']+1181001
        # 1181.001ms既超过4ms误差，也在1193ms阈值前不足12ms；不可四舍五入蒙混。
        with self.assertRaises(ValueError):runner.qpc_prediction(row,1177)
    def test_hardware_threshold_count_is_not_one_per_pump(self):
        # 固定硬件：首make，再在500、533、566...各一次。独立列边界。
        cases = ((0, 1), (499, 1), (500, 2), (532, 2), (533, 3),
            (1176, 22), (780, 10), (3000, 77))
        for milliseconds, expected in cases:
            with self.subTest(milliseconds=milliseconds):
                self.assertEqual(runner.repeat_count(milliseconds), expected)

    def test_prediction_requires_safe_margin_and_rejects_bad_durations(self):
        for requested, count in ((1176, 22), (780, 10)):
            for delta in range(-4, 5):
                with self.subTest(requested=requested, delta=delta):
                    actual, margin = runner.hold_prediction(requested + delta, requested)
                    self.assertEqual(actual, count)
                    self.assertGreaterEqual(margin, 12)
        for actual, requested in ((499, 499), (500, 500), (533, 533),
                (1193, 1193), (1200, 1200), (1176 + 5, 1176),
                (True, 1176), ('1176', 1176), (1176.0, 1176)):
            with self.subTest(actual=actual, requested=requested):
                with self.assertRaises(ValueError):
                    runner.hold_prediction(actual, requested)

    def test_invalid_capture_hold_is_rejected_before_delivery_or_trace(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = comparison.Capture(Path('never-driver'), SimpleNamespace(pid=91), Path(directory), [])
            invalid = (('DOWN', True), ('DOWN', 149), ('UP', 3001),
                ('DOWN', -1), ('DOWN', '500'), ('DOWN', 500.0),
                ('ENTER', 500), ('SHIFT_F1', 500))
            with patch.object(comparison, 'driver_call') as delivery:
                for name, held in invalid:
                    with self.subTest(name=name, held=held), self.assertRaises(ValueError):
                        capture.key(name, hold_ms=held)
                delivery.assert_not_called()
            self.assertEqual(capture.trace, [])
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_capture_hold_boundaries_keep_one_target_command(self):
        for held in (150, 3000):
            with self.subTest(held=held), tempfile.TemporaryDirectory() as directory:
                capture = comparison.Capture(Path('driver'), SimpleNamespace(pid=91), Path(directory), [])
                diagnostic = 'fd2 input: ' + json.dumps(receipt(91, held))
                with patch.object(comparison, 'driver_call', return_value=diagnostic) as delivery:
                    capture.key('DOWN', hold_ms=held)
                delivery.assert_called_once_with(Path('driver'), 'hold-key', capture.process, ('DOWN', held))
                event = capture.trace[0]
                self.assertEqual(event['requested_hold_ms'], held)
                self.assertEqual(event['post_release_settle_ms'], 0)
                self.assertEqual(event['delivery']['sent'], 1)
                self.assertEqual(event['delivery']['release'], 1)
                self.assertEqual(len(Path(directory, 'input-delivery.jsonl').read_text().splitlines()), 1)

    def test_driver_tuple_remains_argv_without_shell_interpolation(self):
        process = SimpleNamespace(pid=72, poll=lambda: None)
        completed = SimpleNamespace(returncode=0, stderr='receipt')
        with patch.object(comparison.subprocess, 'run', return_value=completed) as launch:
            self.assertEqual(comparison.driver_call(Path('driver with spaces.exe'), 'hold-key', process,
                ('UP', 780)), 'receipt')
        self.assertEqual(launch.call_args.args[0], ['driver with spaces.exe', 'hold-key', '72', 'UP', '780'])
        self.assertNotIn('shell', launch.call_args.kwargs)

    def test_output_alias_into_original_is_rejected_before_any_read_or_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); original = root / 'original_game'; original.mkdir()
            sentinel = original / 'FD2.SAV'; sentinel.write_bytes(b'owned test sentinel')
            before = sentinel.stat().st_mtime_ns
            alias = root / 'alias'
            if os.name == 'nt':
                subprocess.run(['cmd', '/c', 'mklink', '/J', str(alias), str(original)],
                    check=True, capture_output=True)
                self.assertEqual(alias.lstat().st_reparse_tag, 0xA0000003)
            else:
                alias.symlink_to(original, target_is_directory=True)
            args = SimpleNamespace(root=root, artifacts=alias/'should-not-exist',
                game=root/'no-game', driver=root/'no-driver', dosbox=root/'no-dosbox', bootstrap=root/'no-conf')
            try:
                with patch.object(runner, 'owned_job_required'), patch.object(runner, 'hashes') as read, \
                        patch.object(runner, 'running') as launch:
                    with self.assertRaisesRegex(ValueError, '原资源'):
                        runner.run(args)
                    read.assert_not_called(); launch.assert_not_called()
                self.assertFalse((original/'should-not-exist').exists())
                self.assertEqual(sentinel.read_bytes(), b'owned test sentinel')
                self.assertEqual(sentinel.stat().st_mtime_ns, before)
            finally:
                if os.name == 'nt': os.rmdir(alias)
                else: alias.unlink()

    def test_output_must_be_fresh_but_unrelated_sibling_is_allowed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); original = root/'original_game'; original.mkdir()
            with self.assertRaises(ValueError): runner.validate_output(original, original)
            with self.assertRaises(ValueError): runner.validate_output(original, original/'child')
            old = root/'old'; old.mkdir()
            with self.assertRaises(ValueError): runner.validate_output(original, old)
            runner.validate_output(original, root/'original_game-evidence')
            self.assertFalse((root/'original_game-evidence').exists())

    @contextmanager
    def fake_run(self, *, counts=None, fail=False, mutate_mtime=False, drop_guard=False):
        """独立22/10业务夹具；仅验证runner守卫与取模，绝非WGC/实机oracle。"""
        with tempfile.TemporaryDirectory() as directory, ExitStack() as stack:
            root = Path(directory); original = root/'original_game'; original.mkdir()
            for name in ('START.SAV', 'FD2.SAV', 'FDOTHER.DAT'):
                (original/name).write_bytes(('fixture-' + name).encode())
            for name in ('app.exe', 'driver.exe', 'dosbox.exe', 'bootstrap.conf'):
                (root/name).write_bytes(name.encode())
            args = SimpleNamespace(root=root, artifacts=root/'out', game=root/'app.exe',
                driver=root/'driver.exe', dosbox=root/'dosbox.exe', bootstrap=root/'bootstrap.conf')
            counts = counts or {'original': 3, 'sdl': 3}
            templates = {n: [Image.new('RGB', (8,8), (n, i, 17)) for i in range(n)] for n in (2,3)}
            order = []
            class Capture:
                def __init__(self, driver, process, artifacts, trace):
                    self.role = artifacts.name; self.trace = trace; self.position = 0
                def key(self, name, *, hold_ms):
                    made = {1176: 22, 780: 10}[hold_ms]
                    self.position = (self.position + (made if name == 'DOWN' else -made)) % counts[self.role]
                    self.trace.append({'requested_hold_ms': hold_ms, 'delivery': receipt(55, hold_ms)})
                def wait(self, expected, **kwargs):
                    actual = templates[counts[self.role]][self.position]
                    if fail and self.role == 'original': raise RuntimeError('business checkpoint failed')
                    if actual.tobytes() != expected.tobytes(): raise AssertionError('independent title fixture disagrees')
                    return actual.copy()
            @contextmanager
            def display(record, *, require_display_on):
                if require_display_on is not True: raise AssertionError('must use strict ON')
                order.append('display-acquire')
                try: yield
                finally: order.append('display-release'); record['released'] = True
            @contextmanager
            def running(command, cwd, environment, log, driver, receipts):
                role = log.parent.name; order.append(role + '-enter')
                try: yield SimpleNamespace(pid=55)
                finally:
                    order.append(role + '-cleanup')
                    if drop_guard and role == 'original': args.bootstrap.unlink()
                    if mutate_mtime:
                        target = original/'START.SAV'; stat = target.stat()
                        os.utime(target, ns=(stat.st_atime_ns, stat.st_mtime_ns + 2000000000))
            for name, value in (('owned_job_required', Mock()), ('Capture', Capture),
                    ('windows_display_requirement', display), ('running', running),
                    ('source_facts', Mock(return_value={'stable-source': {}})),
                    ('title_templates', Mock(return_value=templates)),
                    ('decode_save', Mock(return_value=b'unused')), ('title_menu_counts', Mock(return_value=counts)),
                    ('wait_title', lambda capture, expected: capture.wait(expected))):
                stack.enter_context(patch.object(runner, name, value))
            stack.enter_context(patch.object(runner.original_reference, 'build', return_value={'fixture_only': True}))
            stack.enter_context(patch.dict(os.environ, {}, clear=True))
            yield runner.run(args), order, args

    def test_paired_business_prediction_and_strict_display_cleanup(self):
        with self.fake_run() as (report, order, args):
            self.assertTrue(report['ok'], report.get('error'))
            self.assertEqual(order, ['display-acquire', 'original-enter', 'original-cleanup',
                'sdl-enter', 'sdl-cleanup', 'display-release'])
            self.assertTrue(report['display_requirement']['released'])
            for case in report['cases']:
                self.assertEqual([p['predicted_make_count'] for p in case['checkpoints']], [22,10])
                self.assertEqual([p['expected_selection'] for p in case['checkpoints']], [1,0])
                self.assertTrue(all(p['repeat_threshold_distance_ms'] >= 12 for p in case['checkpoints']))
                self.assertTrue(all(p['exact_produced_count_claimed'] is False for p in case['checkpoints']))
                self.assertEqual(len(case['inputs']), 2)
            self.assertEqual((args.artifacts/'report.json').read_text(encoding='utf-8')[:1], '{')

    def test_different_menu_counts_cannot_both_individually_pass_the_pair(self):
        with self.fake_run(counts={'original': 3, 'sdl': 2}) as (report, _, _):
            self.assertTrue(all(c['ok'] for c in report['cases']))
            self.assertFalse(report['pair_same_menu_count']); self.assertFalse(report['ok'])

    def test_mtime_only_original_change_invalidates_success_without_rewriting_bytes(self):
        with self.fake_run(mutate_mtime=True) as (report, _, _):
            self.assertTrue(report['original_unchanged'])
            self.assertFalse(report['original_mtimes_unchanged']); self.assertFalse(report['ok'])

    def test_business_failure_releases_owned_and_display_and_does_not_start_sdl(self):
        with self.fake_run(fail=True) as (report, order, _):
            self.assertFalse(report['ok'])
            self.assertIn('business checkpoint failed', report['error']['text'])
            self.assertEqual(order, ['display-acquire', 'original-enter', 'original-cleanup', 'display-release'])

    def test_missing_late_guard_does_not_mask_business_failure_or_lose_report(self):
        with self.fake_run(fail=True, drop_guard=True) as (report, order, args):
            self.assertFalse(report['ok']); self.assertFalse(report['guards_unchanged'])
            self.assertIn('business checkpoint failed', report['error']['text'])
            self.assertTrue(any(e['guard'] == 'guards_unchanged' and e['type'] == 'FileNotFoundError'
                for e in report['guard_errors']))
            self.assertTrue((args.artifacts/'report.json').is_file())
            self.assertEqual(order[-2:], ['original-cleanup', 'display-release'])


if __name__ == '__main__': unittest.main()
