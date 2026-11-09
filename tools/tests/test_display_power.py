"""临时显示电源副本的journal／恢复纯测；不调用真实DLL、powercfg或改变方案。"""
import copy
import ctypes
from contextlib import nullcontext
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_display_power as power

ORIGINAL = "381b4222-f694-41f0-9685-ff5bb260df2e"
OTHER = "11111111-1111-4111-8111-111111111111"
TEMPORARY = "22222222-2222-4222-8222-222222222222"


class FakePower:
    def __init__(self, journal, source="AC"):
        self.journal, self.current, self.supply = journal, ORIGINAL, source
        self.plans = {ORIGINAL: {"AC": 300, "DC": 120}, OTHER: {"AC": 30, "DC": 60}}
        self.owner = {"pid": os.getpid(), "created": 7}
        self.events = []; self.fail = None

    def active(self): return self.current
    def settings(self, scheme): return dict(self.plans[scheme])
    def source(self): return self.supply
    def identity(self, pid): return self.owner if self.owner and self.owner["pid"] == pid else None

    def check_journal(self, phase):
        value = json.loads(self.journal.read_text())
        if value["phase"] != phase: raise AssertionError("操作前未持久记录意图")
        return value

    def duplicate(self, original, temporary):
        state = self.check_journal("copy_pending")
        if state["temporary"] != temporary: raise AssertionError("目标GUID未先落盘")
        self.events.append("copy")
        self.plans[temporary] = dict(self.plans[original])
        if self.fail == "copy": raise OSError("copy interrupted after native side effect")

    def write_idle(self, scheme, source, value):
        self.check_journal("write_pending"); self.events.append(("write", source))
        self.plans[scheme][source] = value
        if self.fail == "write": raise OSError("write failed")

    def activate(self, scheme):
        self.check_journal("restoring" if scheme == ORIGINAL else "activate_pending")
        self.current = scheme; self.events.append(("activate", scheme))
        if self.fail == "activate" and scheme != ORIGINAL: raise KeyboardInterrupt()

    def delete(self, scheme):
        self.events.append(("delete", scheme))
        if self.fail == "delete": raise OSError("delete failed")
        if self.current == scheme: raise AssertionError("不得删除活动方案")
        self.plans.pop(scheme, None)


class PowerJournalTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(); self.addCleanup(folder.cleanup)
        self.journal = Path(folder.name) / "restore.json"
        self.api = FakePower(self.journal)
        self.patches = [patch('ctypes.WinDLL', side_effect=AssertionError("禁止真实DLL"), create=True),
            patch('subprocess.run', side_effect=AssertionError("禁止真实powercfg"))]
        if os.name != 'nt':
            self.patches.append(patch.object(power, '_exclusive', side_effect=lambda path: nullcontext()))
        for item in self.patches: item.start(); self.addCleanup(item.stop)

    def test_only_current_supply_copy_changes_and_original_restored(self):
        for source in ("AC", "DC"):
            self.api.supply = source; record = {}
            with power.temporary_video_idle(record, self.journal, api=self.api):
                self.assertNotEqual(self.api.current, ORIGINAL)
                self.assertEqual(self.api.plans[ORIGINAL], {"AC": 300, "DC": 120})
                expected = {"AC": 300, "DC": 120}; expected[source] = 0
                self.assertEqual(self.api.plans[self.api.current], expected)
                self.assertEqual(json.loads(self.journal.read_text())["phase"], "held")
            self.assertEqual(self.api.current, ORIGINAL)
            self.assertEqual(set(self.api.plans), {ORIGINAL, OTHER})
            self.assertTrue(record["restored"] and record["state"]["original_values_unchanged"])

    def test_body_error_and_cancellation_restore_and_preserve_primary(self):
        for failure in (ValueError("body"), KeyboardInterrupt(), SystemExit(0)):
            record = {}
            with self.assertRaises(type(failure)) as caught:
                with power.temporary_video_idle(record, self.journal, api=self.api): raise failure
            self.assertIs(caught.exception, failure)
            self.assertEqual(self.api.current, ORIGINAL)
            self.assertTrue(record["restored"])

    def test_native_failure_at_each_mutation_still_cleans_known_guid(self):
        for step in ("copy", "write", "activate"):
            self.api.fail = step; record = {}
            with self.assertRaises(KeyboardInterrupt if step == "activate" else OSError):
                with power.temporary_video_idle(record, self.journal, api=self.api): self.fail("entered")
            self.assertEqual(self.api.current, ORIGINAL)
            self.assertEqual(set(self.api.plans), {ORIGINAL, OTHER})
            self.assertTrue(record["restored"])

    def test_user_changed_plan_never_overwritten_and_conflict_persisted(self):
        record = {}
        with self.assertRaisesRegex(RuntimeError, "用户电源方案"):
            with power.temporary_video_idle(record, self.journal, api=self.api): self.api.current = OTHER
        self.assertEqual(self.api.current, OTHER)
        self.assertEqual(set(self.api.plans), {ORIGINAL, OTHER})
        self.assertEqual(json.loads(self.journal.read_text())["phase"], "conflict")
        self.assertIn("restore_error", record)

    def test_user_switch_during_restore_journal_write_is_preserved(self):
        write = power._write
        def switched(path, state):
            write(path, state)
            if state['phase'] == 'restoring': self.api.current = OTHER
        record = {}
        with patch.object(power, '_write', side_effect=switched):
            with self.assertRaisesRegex(RuntimeError, '用户电源方案'):
                with power.temporary_video_idle(record, self.journal, api=self.api): pass
        self.assertEqual(self.api.current, OTHER)
        self.assertNotIn(('activate', ORIGINAL), self.api.events)
        self.assertEqual(set(self.api.plans), {ORIGINAL, OTHER})
        state = json.loads(self.journal.read_text())
        self.assertEqual(state['restore_pre_set_active'], OTHER)
        self.assertEqual(state['phase'], 'conflict')
        self.assertNotIn('restored', record)

    def test_user_switch_during_activate_journal_write_never_enters(self):
        write = power._write
        def switched(path, state):
            write(path, state)
            if state['phase'] == 'activate_pending': self.api.current = OTHER
        record = {}; entered = Mock()
        with patch.object(power, '_write', side_effect=switched):
            with self.assertRaisesRegex(RuntimeError, '持久化后用户已换方案'):
                with power.temporary_video_idle(record, self.journal, api=self.api): entered()
        entered.assert_not_called()
        self.assertFalse(any(event[0] == 'activate' for event in self.api.events if isinstance(event, tuple)))
        self.assertEqual(self.api.current, OTHER)
        self.assertEqual(set(self.api.plans), {ORIGINAL, OTHER})
        state = json.loads(self.journal.read_text())
        self.assertEqual(state['activate_pre_set_active'], OTHER)
        self.assertEqual(state['phase'], 'conflict')
        self.assertNotIn('restored', record)

    def test_user_switch_after_temporary_delete_cannot_report_restored(self):
        delete = self.api.delete
        def switched(scheme): delete(scheme); self.api.current = OTHER
        record = {}
        with patch.object(self.api, 'delete', side_effect=switched):
            with self.assertRaisesRegex(RuntimeError, '用户电源方案'):
                with power.temporary_video_idle(record, self.journal, api=self.api): pass
        self.assertEqual(self.api.current, OTHER)
        self.assertEqual(set(self.api.plans), {ORIGINAL, OTHER})
        state = json.loads(self.journal.read_text())
        self.assertEqual(state['final_active'], OTHER)
        self.assertTrue(state['original_values_unchanged'])
        self.assertEqual(state['phase'], 'conflict')
        self.assertNotIn('restored', record)

    def orphan(self, owner):
        self.api.plans[TEMPORARY] = {"AC": 0, "DC": 120}; self.api.current = TEMPORARY
        power._write(self.journal, {"schema": power.SCHEMA, "phase": "held", "owner": owner,
            "original": ORIGINAL, "temporary": TEMPORARY,
            "original_values": {"AC": 300, "DC": 120}, "source": "AC"})

    def test_dead_or_reused_pid_restored_before_new_round(self):
        self.orphan({"pid": os.getpid(), "created": 6})  # 同PID、不同创建时间。
        record = {}
        power.recover_video_idle(record, self.journal, api=self.api)
        self.assertEqual(self.api.current, ORIGINAL)
        self.assertNotIn(TEMPORARY, self.api.plans)
        self.assertEqual(record["previous_recovery"]["phase"], "restored")

    def test_live_owner_and_unknown_schema_never_mutate(self):
        self.orphan(self.api.owner)
        with self.assertRaisesRegex(RuntimeError, "仍存活"):
            power.recover_video_idle({}, self.journal, api=self.api)
        self.assertEqual(self.api.events, [])
        value = json.loads(self.journal.read_text()); value["schema"] = "unknown"
        power._write(self.journal, value)
        with self.assertRaisesRegex(ValueError, "未知"):
            power.recover_video_idle({}, self.journal, api=self.api)
        self.assertEqual(self.api.events, [])

    def test_cleanup_failure_preserves_primary_and_pending_journal(self):
        record = {}; original = ValueError("business")
        with self.assertRaises(ValueError) as caught:
            with power.temporary_video_idle(record, self.journal, api=self.api):
                self.api.fail = "delete"; raise original
        self.assertIs(caught.exception, original)
        self.assertIn("restore_error", record)
        self.assertNotEqual(json.loads(self.journal.read_text())["phase"], "restored")
        self.assertEqual(self.api.current, ORIGINAL)


class NativeBindingTest(unittest.TestCase):
    def test_native_bindings_read_write_activate_delete_and_owner(self):
        guid = power.NativePower.guid(ORIGINAL)
        def active(root, out): out._obj.value = ctypes.addressof(guid); return 0
        def read(root, scheme, subgroup, setting, out):
            self.assertEqual(bytes(subgroup), bytes(power.NativePower.guid(power.VIDEO)))
            self.assertEqual(bytes(setting), bytes(power.NativePower.guid(power.VIDEOIDLE)))
            out._obj.value = 300; return 0
        def times(handle, created, exited, kernel_time, user_time):
            created._obj[0] = 7; created._obj[1] = 1; return 1
        def exit_code(handle, value): value._obj.value = 259; return 1
        dll = SimpleNamespace(**{name: Mock(return_value=0) for name in (
            'PowerGetActiveScheme', 'PowerSetActiveScheme', 'PowerDeleteScheme',
            'PowerReadACValueIndex', 'PowerReadDCValueIndex',
            'PowerWriteACValueIndex', 'PowerWriteDCValueIndex')})
        dll.PowerGetActiveScheme.side_effect = active
        dll.PowerReadACValueIndex.side_effect = dll.PowerReadDCValueIndex.side_effect = read
        kernel = SimpleNamespace(LocalFree=Mock(return_value=0),
            GetSystemPowerStatus=Mock(side_effect=lambda row: row.__setitem__(0, 1) or 1),
            OpenProcess=Mock(return_value=123), GetProcessTimes=Mock(side_effect=times),
            GetExitCodeProcess=Mock(side_effect=exit_code), CloseHandle=Mock(return_value=1))
        calls = []
        with patch('ctypes.WinDLL', side_effect=lambda name, **kw: dll if name == 'powrprof.dll' else kernel, create=True):
            api = power.NativePower(calls)
            self.assertEqual(api.active(), ORIGINAL)
            self.assertEqual(api.settings(ORIGINAL), {'AC': 300, 'DC': 300})
            self.assertEqual(api.source(), 'AC')
            self.assertEqual(api.identity(12), {'pid': 12, 'created': 0x100000007})
            api.write_idle(TEMPORARY, 'DC', 0); api.activate(TEMPORARY); api.delete(TEMPORARY)
        self.assertEqual(dll.PowerWriteDCValueIndex.call_args.args[-1], 0)
        dll.PowerWriteACValueIndex.assert_not_called()
        self.assertEqual(bytes(dll.PowerSetActiveScheme.call_args.args[1]), bytes(power.NativePower.guid(TEMPORARY)))
        kernel.LocalFree.assert_called_once(); kernel.CloseHandle.assert_called_once_with(123)
        self.assertTrue(all(call['code'] == 0 for call in calls))

    def test_duplicate_uses_known_journal_guid_without_shell_or_visible_window(self):
        api = object.__new__(power.NativePower); api.operations = []
        completed = SimpleNamespace(returncode=0, stdout=b'copied', stderr=b'')
        with patch.dict(os.environ, {'SystemRoot': 'C:/Windows'}), patch.object(power.subprocess, 'CREATE_NO_WINDOW', 0x08000000, create=True), patch('subprocess.run', return_value=completed) as run:
            api.duplicate(ORIGINAL, TEMPORARY)
        self.assertEqual(run.call_args.args[0][-3:], ['/duplicatescheme', ORIGINAL, TEMPORARY])
        self.assertFalse(run.call_args.kwargs['shell'])
        self.assertEqual(run.call_args.kwargs['creationflags'], 0x08000000)
        self.assertEqual(api.operations[0]['stdout_hex'], b'copied'.hex())


if __name__ == "__main__": unittest.main()
