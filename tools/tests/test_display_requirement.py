"""临时显示需求纯模型：真实WinDLL/Popen禁用，检查状态、退出与finally顺序。"""
from contextlib import ExitStack
import copy
import ctypes
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
with patch('ctypes.WinDLL', side_effect=AssertionError('纯测禁止真实Windows API'), create=True):
    import fd2_display_requirement as display
    import run_graphics_tests as suite_module
    import run_original_battle_animation as battle


def on():
    return dict(session_query_ok=True, same_as_active_console=True, SM_REMOTESESSION=0,
                console_display=dict(ok=True, state=1), session_display=dict(ok=True, state=1))


class FakeWindows:
    def __init__(self, state=None, *, acquire_fail=False, release_fail=False):
        self.state = on() if state is None else state
        self.acquire_fail, self.release_fail = acquire_fail, release_fail
        self.previous = display.ES_CONTINUOUS | 1
        self.current = self.previous
        self.tid = 37
        self.events = []
        self.sets = []

    def snapshot(self):
        self.events.append('query')
        return copy.deepcopy(self.state)

    def thread_id(self):
        return self.tid

    def set_state(self, flags):
        self.events.append(('set', flags))
        self.sets.append(flags)
        fail = self.acquire_fail if len(self.sets) == 1 else self.release_fail
        if fail:
            return dict(return_value=0, last_error=5 if len(self.sets) == 1 else 31)
        previous, self.current = self.current, flags
        return dict(return_value=previous, last_error=0)


class PureTest(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch('ctypes.WinDLL',
            side_effect=AssertionError('纯测禁止真实Windows API'), create=True))
        self.stack.enter_context(patch('subprocess.Popen',
            side_effect=AssertionError('纯测禁止启动程序')))
        self.stack.enter_context(patch.object(display.sys, 'platform', 'win32'))

    def fake(self, api):
        self.stack.enter_context(patch.object(display, '_windows_calls', return_value=api))
        return api


class DisplayContextTest(PureTest):
    def test_interrupt_after_register_fills_handle_still_unregisters_or_retains(self):
        for unregister_code in (0, 5):
            with self.subTest(unregister_code=unregister_code):
                count = len(display._unreleased_notifications)
                interrupted = KeyboardInterrupt()

                def register(guid, flags, recipient, output):
                    output._obj.value = 123  # native已登记，Python在返回处收到中断。
                    raise interrupted

                power = SimpleNamespace(PowerSettingRegisterNotification=Mock(side_effect=register),
                    PowerSettingUnregisterNotification=Mock(return_value=unregister_code))
                kernel = SimpleNamespace(ProcessIdToSessionId=Mock(side_effect=lambda pid, out:
                    setattr(out._obj, 'value', 1) or 1), WTSGetActiveConsoleSessionId=Mock(return_value=1))
                user = SimpleNamespace(GetSystemMetrics=Mock(return_value=0))
                fake_dll = lambda name, **kwargs: power if name == 'powrprof.dll' else kernel if name == 'kernel32.dll' else user
                api = SimpleNamespace(snapshot=display._display_snapshot, set_state=Mock(),
                                      thread_id=Mock(return_value=37))
                body = Mock(); record = {}
                try:
                    callback_type = getattr(ctypes, 'WINFUNCTYPE', ctypes.CFUNCTYPE)
                    with (patch.object(display.ctypes, 'WinDLL', side_effect=fake_dll),
                          patch.object(display.ctypes, 'WINFUNCTYPE', callback_type, create=True),
                          patch.object(display, '_windows_calls', return_value=api)):
                        with self.assertRaises(KeyboardInterrupt) as caught:
                            with display.windows_display_requirement(record): body()
                    self.assertIs(caught.exception, interrupted)
                    power.PowerSettingRegisterNotification.assert_called_once()
                    power.PowerSettingUnregisterNotification.assert_called_once()
                    self.assertEqual(power.PowerSettingUnregisterNotification.call_args.args[0].value, 123)
                    body.assert_not_called(); api.set_state.assert_not_called()
                    self.assertFalse(record['requested'] or record['acquired'] or record['released'])
                    self.assertEqual(record['acquire_error']['type'], 'KeyboardInterrupt')
                    self.assertEqual(len(display._unreleased_notifications), count + bool(unregister_code))
                    if unregister_code:
                        self.assertEqual(display._unreleased_notifications[-1][2].value, 123)
                finally:
                    del display._unreleased_notifications[count:]  # 仅本测试的fake注册。

    def test_failed_unregister_retains_callback_and_blocks_request(self):
        # 真实ctypes结构/回调，只把DLL函数换为同步fake；绝不加载原生DLL。
        count = len(display._unreleased_notifications)

        def register(guid, flags, recipient, output):
            self.assertEqual(flags, 2)
            output._obj.value = 123

            class Setting(ctypes.Structure):
                _fields_ = [('guid', ctypes.c_ubyte * 16), ('length', ctypes.c_uint32),
                            ('data', ctypes.c_ubyte * 4)]

            setting = Setting()
            setting.guid[:] = bytes(guid._obj)
            setting.length = 4; setting.data[0] = 1
            recipient._obj.callback(None, 0x8013, ctypes.addressof(setting))
            return 0

        power = SimpleNamespace(PowerSettingRegisterNotification=Mock(side_effect=register),
                                PowerSettingUnregisterNotification=Mock(return_value=5))
        kernel = SimpleNamespace(ProcessIdToSessionId=Mock(side_effect=lambda pid, out:
            setattr(out._obj, 'value', 1) or 1), WTSGetActiveConsoleSessionId=Mock(return_value=1))
        user = SimpleNamespace(GetSystemMetrics=Mock(return_value=0))
        fake_dll = lambda name, **kwargs: power if name == 'powrprof.dll' else kernel if name == 'kernel32.dll' else user
        try:
            # 非Windows仅给同步fake提供本地callback trampoline，不当作Win ABI验收。
            callback_type = getattr(ctypes, 'WINFUNCTYPE', ctypes.CFUNCTYPE)
            with (patch.object(display.ctypes, 'WinDLL', side_effect=fake_dll),
                  patch.object(display.ctypes, 'WINFUNCTYPE', callback_type, create=True)):
                state = display._display_snapshot()
            self.assertEqual(power.PowerSettingUnregisterNotification.call_count, 2)
            self.assertEqual(len(display._unreleased_notifications), count + 2)
            self.assertFalse(state['console_display']['ok'] or state['session_display']['ok'])
            api = FakeWindows(state); record = {}
            with patch.object(display, '_windows_calls', return_value=api):
                with self.assertRaises(RuntimeError):
                    with display.windows_display_requirement(record): self.fail('entered')
            self.assertEqual(api.sets, [])
        finally:
            # 仅fake注册，可清本测试引用；真实失败路径保持到进程结束。
            del display._unreleased_notifications[count:]

    def test_once_on_main_thread_until_owned_cleanup_then_restore_previous(self):
        api = self.fake(FakeWindows())
        record = {}
        with display.windows_display_requirement(record):
            self.assertEqual(api.current, 0x80000003)
            api.events.append('target')
            api.events.append('collector_cleanup')
            api.events.append('job_cleanup')
        self.assertEqual(api.events, ['query', ('set', 0x80000003), 'target',
            'collector_cleanup', 'job_cleanup', ('set', api.previous)])
        self.assertEqual(api.current, api.previous)
        self.assertEqual(record['owner_thread_id'], record['release']['thread_id'])
        self.assertTrue(record['acquired'] and record['released'])
        self.assertEqual(record['status'], 'released')
        self.assertEqual(record['acquire']['last_error'], 0)
        self.assertTrue(record['acquire_can_force_display_on'])
        self.assertTrue(record['conditional_acquire_not_atomic'])
        self.assertFalse(record['separate_wake_API_called'])

    def test_off_dim_unknown_remote_and_bad_notifications_never_request_or_enter(self):
        bad = []
        for value in (0, 2, None):
            state = on(); state['console_display']['state'] = value; bad.append(state)
        for key, value in (('same_as_active_console', False), ('session_query_ok', False),
                           ('SM_REMOTESESSION', 1)):
            state = on(); state[key] = value; bad.append(state)
        state = on(); state['session_display']['ok'] = False; bad.append(state)
        for state in bad:
            with self.subTest(state=state):
                api = FakeWindows(state); record = {}; body = Mock()
                with patch.object(display, '_windows_calls', return_value=api):
                    with self.assertRaisesRegex(RuntimeError, '显示非ON或未知'):
                        with display.windows_display_requirement(record): body()
                body.assert_not_called()
                self.assertEqual(api.sets, [])
                self.assertFalse(record['requested'] or record['acquired'] or record['released'])

    def test_acquire_failure_retains_actual_error_without_release_or_body(self):
        api = self.fake(FakeWindows(acquire_fail=True)); record = {}; body = Mock()
        with self.assertRaisesRegex(RuntimeError, '申请失败'):
            with display.windows_display_requirement(record): body()
        body.assert_not_called()
        self.assertEqual(api.sets, [0x80000003])
        self.assertEqual(record['acquire']['return_value'], 0)
        self.assertEqual(record['acquire']['last_error'], 5)
        self.assertFalse(record['acquired'] or record['released'])

    def test_query_exception_does_not_request(self):
        api = self.fake(FakeWindows()); api.snapshot = Mock(side_effect=OSError('notification'))
        record = {}
        with self.assertRaisesRegex(OSError, 'notification'):
            with display.windows_display_requirement(record): self.fail('entered')
        self.assertEqual(api.sets, [])
        self.assertEqual(record['acquire_error']['type'], 'OSError')

    def test_every_body_exception_releases_after_body_finally_and_preserves_identity(self):
        for error in (ValueError('business'), SystemExit(0), KeyboardInterrupt(), TimeoutError('job')):
            with self.subTest(error=type(error).__name__):
                api = FakeWindows(); record = {}
                with patch.object(display, '_windows_calls', return_value=api):
                    with self.assertRaises(type(error)) as caught:
                        with display.windows_display_requirement(record):
                            try: raise error
                            finally: api.events.append('owned_cleanup')
                self.assertIs(caught.exception, error)
                self.assertEqual(api.events[-2:], ['owned_cleanup', ('set', api.previous)])
                self.assertTrue(record['released'])
                self.assertEqual(record['status'], 'failed')

    def test_release_failure_does_not_hide_primary_and_never_reports_released(self):
        for primary in (None, ValueError('first-business')):
            with self.subTest(primary=primary):
                api = FakeWindows(release_fail=True); record = {}
                with patch.object(display, '_windows_calls', return_value=api):
                    with self.assertRaises(ValueError if primary else RuntimeError) as caught:
                        with display.windows_display_requirement(record):
                            if primary: raise primary
                if primary: self.assertIs(caught.exception, primary)
                self.assertFalse(record['released'])
                self.assertEqual(record['release']['last_error'], 31)
                self.assertEqual(record['status'], 'failed')
                self.assertIn('release_error', record)
                if primary: self.assertEqual(record['body_error']['message'], 'first-business')

    def test_other_thread_never_clears_its_own_requirements_as_if_owner(self):
        api = self.fake(FakeWindows()); record = {}
        with self.assertRaisesRegex(RuntimeError, '同一线程'):
            with display.windows_display_requirement(record): api.tid += 1
        self.assertEqual(api.sets, [0x80000003])
        self.assertFalse(record['released'])

    def test_linux_is_noop_without_calling_windows_factory(self):
        with patch.object(display.sys, 'platform', 'linux'), patch.object(display, '_windows_calls') as factory:
            record = {}; body = Mock()
            with display.windows_display_requirement(record): body()
            factory.assert_not_called(); body.assert_called_once()
        self.assertEqual(record['status'], 'not_applicable')
        self.assertFalse(record['requested'])


class EntryScopeTest(PureTest):
    def suite(self, folder, api, error=None, source_error=None):
        class ModelSuite:
            def __init__(self, args):
                self.work = folder / 'suite'; self.work.mkdir()
                self.report = {'status': 'failed'}

            def run(self):
                self.assert_held()
                try:
                    self.report['status'] = 'passed'
                    if error is not None: raise error
                finally:
                    api.events.extend(['target_cleanup', 'source_finally'])
                    if source_error is not None: raise source_error

            def assert_held(self):
                if api.current != 0x80000003: raise AssertionError('display not held before first target')

        argv = ['suite', '--game', 'game.exe', '--build-dir', str(folder),
                '--driver', 'driver.exe', '--dosbox', 'dosbox.exe']
        with (patch.object(suite_module, 'Suite', ModelSuite), patch.object(sys, 'argv', argv),
              patch.object(sys, 'stdout', io.StringIO()), patch.object(sys, 'stderr', io.StringIO())):
            result = suite_module.main()
        return result, json.loads((folder / 'suite/summary.json').read_text())

    def collector(self, folder, api, error=None, *, business=False, preexisting=False):
        output = folder / 'case'
        original = {'status': 'passed', 'untouched': 'history'}
        if preexisting:
            output.mkdir(); (output / 'comparison.json').write_text(json.dumps(original))
        scenario = 'ai-behavior11-recover' if business else 'physical-full-hit'
        report = {'scenario': scenario, 'status': 'collected' if business else 'passed',
                  'ok': not business, 'collected_only': business, 'collection_passed': True,
                  'paired_collected': True, 'business_regression_passed': business}

        def run(args, *, _final_report=None):
            self.assertEqual(api.current, 0x80000003)
            if preexisting: raise FileExistsError('old artifact')
            output.mkdir()
            if _final_report is not None:
                _final_report['value'] = report
            else:
                (output / 'comparison.json').write_text(json.dumps(report))
            try:
                if error is not None: raise error
                return report
            finally: api.events.extend(['target_cleanup', 'collector_cleanup', 'source_finally'])

        argv = ['battle', '--root', str(folder), '--driver', 'driver.exe', '--dosbox', 'dosbox.exe',
                '--sdl', 'game.exe', '--scenario', scenario, '--artifacts', str(output)]
        if not business: argv.append('--verify-core')
        stream = io.StringIO()
        with (patch.object(battle, 'run', side_effect=run) as called, patch.object(sys, 'argv', argv),
              patch.object(sys, 'stdout', stream)):
            result = battle.main()
        saved = json.loads((output / 'comparison.json').read_text()) if (output / 'comparison.json').is_file() else None
        return result, saved, json.loads(stream.getvalue()), called

    def test_suite_scope_contains_cleanup_and_source_finally(self):
        api = self.fake(FakeWindows())
        with tempfile.TemporaryDirectory() as temp:
            result, report = self.suite(Path(temp), api)
        self.assertEqual(result, 0); self.assertEqual(report['status'], 'passed')
        self.assertTrue(report['display_requirement']['released'])
        self.assertEqual(api.events[-3:], ['target_cleanup', 'source_finally', ('set', api.previous)])

    def test_suite_release_failure_cannot_keep_passed_report_or_exit(self):
        api = self.fake(FakeWindows(release_fail=True))
        with tempfile.TemporaryDirectory() as temp:
            result, report = self.suite(Path(temp), api)
        self.assertEqual(result, 1); self.assertEqual(report['status'], 'failed')
        self.assertFalse(report['display_requirement']['released'])

    def test_suite_interrupt_exit_and_source_guard_failure_always_failed(self):
        for error, source_error, expected in ((SystemExit(0), None, 1), (SystemExit(7), None, 7),
                (KeyboardInterrupt(), None, 130), (TimeoutError('timeout'), None, 1),
                (None, AssertionError('source-guard'), 1)):
            with self.subTest(error=error, source_error=source_error), tempfile.TemporaryDirectory() as temp:
                api = FakeWindows()
                with patch.object(display, '_windows_calls', return_value=api):
                    result, report = self.suite(Path(temp), api, error, source_error)
                self.assertEqual(result, expected); self.assertEqual(report['status'], 'failed')
                self.assertTrue(report['display_requirement']['released'])

    def test_suite_primary_and_release_error_both_survive(self):
        api = self.fake(FakeWindows(release_fail=True))
        with tempfile.TemporaryDirectory() as temp:
            result, report = self.suite(Path(temp), api, ValueError('first-business'))
        self.assertEqual(result, 1); self.assertEqual(report['error'], 'first-business')
        self.assertEqual(report['display_requirement']['release_error']['type'], 'RuntimeError')

    def test_standalone_holds_until_all_cleanup_and_rewrites_own_final_metadata(self):
        api = self.fake(FakeWindows())
        with tempfile.TemporaryDirectory() as temp:
            result, saved, stdout, called = self.collector(Path(temp), api)
        self.assertEqual(result, 0); called.assert_called_once()
        self.assertTrue(saved['display_requirement']['acquired'] and saved['display_requirement']['released'])
        self.assertEqual(saved['status'], 'passed')
        self.assertEqual(api.events[-4:], ['target_cleanup', 'collector_cleanup', 'source_finally', ('set', api.previous)])

    def test_standalone_release_failure_downgrades_core_and_business(self):
        for business in (False, True):
            with self.subTest(business=business), tempfile.TemporaryDirectory() as temp:
                api = FakeWindows(release_fail=True)
                with patch.object(display, '_windows_calls', return_value=api):
                    result, saved, stdout, _ = self.collector(Path(temp), api, business=business)
                self.assertEqual(result, 1); self.assertEqual(saved['status'], 'failed')
                self.assertFalse(saved['ok'] or saved['collection_passed'] or saved['business_regression_passed'])
                self.assertFalse(saved['display_requirement']['released'])

    def test_standalone_source_finally_or_interruption_rewrites_only_own_report_failed(self):
        for error, expected in ((AssertionError('source-finally'), 1), (SystemExit(0), 1),
                                (KeyboardInterrupt(), 130), (TimeoutError('target'), 1)):
            with self.subTest(error=error), tempfile.TemporaryDirectory() as temp:
                api = FakeWindows()
                with patch.object(display, '_windows_calls', return_value=api):
                    result, saved, stdout, _ = self.collector(Path(temp), api, error)
                self.assertEqual(result, expected); self.assertEqual(saved['status'], 'failed')
                self.assertTrue(saved['display_requirement']['released'])

    def test_standalone_off_or_unknown_never_runs_or_creates_output(self):
        for state in (0, None):
            with self.subTest(state=state), tempfile.TemporaryDirectory() as temp:
                snapshot = on(); snapshot['session_display']['state'] = state
                api = FakeWindows(snapshot)
                with patch.object(display, '_windows_calls', return_value=api):
                    result, saved, stdout, called = self.collector(Path(temp), api)
                self.assertEqual(result, 1); self.assertIsNone(saved); called.assert_not_called()
                self.assertEqual(api.sets, []); self.assertFalse(stdout['display_requirement']['requested'])

    def test_standalone_existing_artifact_is_never_rewritten(self):
        api = self.fake(FakeWindows())
        with tempfile.TemporaryDirectory() as temp:
            result, saved, stdout, _ = self.collector(Path(temp), api, preexisting=True)
        self.assertEqual(result, 1)
        self.assertEqual(saved, {'status': 'passed', 'untouched': 'history'})
        self.assertEqual(stdout['status'], 'failed')

    def test_standalone_write_failure_cannot_leave_a_passed_comparison(self):
        api = self.fake(FakeWindows(release_fail=True))
        original_write = Path.write_text

        def fail_pending(path, *args, **kwargs):
            if path.name == 'comparison-display-pending.json':
                raise OSError('disk-write')
            return original_write(path, *args, **kwargs)

        with tempfile.TemporaryDirectory() as temp, patch.object(Path, 'write_text', fail_pending):
            result, saved, stdout, _ = self.collector(Path(temp), api)
        self.assertEqual(result, 1); self.assertIsNone(saved)
        self.assertEqual(stdout['status'], 'failed')
        self.assertIn('release_error', stdout['display_requirement'])
        self.assertEqual(stdout['display_requirement']['report_write_error']['message'], 'disk-write')


if __name__ == '__main__':
    unittest.main()
