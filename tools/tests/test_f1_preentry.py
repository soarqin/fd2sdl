"""F1 loader取证信任边界；纯合成，不运行窗口或DOSBox。"""
from pathlib import Path
import json
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_le_preentry_check as probe
import run_original_comparison as native

class PreentryContracts(unittest.TestCase):
    def gdt(self):
        return ['GDT Base:00170010 Limit:00003FFF',
          '0170: b:00000000 type: 1A parbg','      l:FFFFFFFF dpl : 0  10011',
          '0178: b:00000000 type: 12 parbg','      l:FFFFFFFF dpl : 0  10011',
          '0088: b:0017A660 type: 1A parbg','      l:0000B6EF dpl : 0  10000']
    def test_selectors_come_from_current_GDT_not_fixed_numbers(self):
        rows=probe.descriptors(self.gdt());self.assertEqual(probe.flat_selectors(rows),(0x170,0x178))
        changed=self.gdt();changed[1]=changed[1].replace('0170','0280');changed[3]=changed[3].replace('0178','0288')
        self.assertEqual(probe.flat_selectors(probe.descriptors(changed)),(0x280,0x288))
        for bad in (rows[:1],rows+[rows[0]],rows[2:]):
            with self.subTest(bad=bad),self.assertRaises(ValueError):probe.flat_selectors(bad)
        with self.assertRaises(ValueError):probe.descriptors(['no descriptor'])
    def test_allocation_must_be_unique_in_bounds_and_disjoint(self):
        objects=[SimpleNamespace(index=1,vsize=5),SimpleNamespace(index=2,vsize=9)]
        rows=[{'requested_size':8,'linear':0x1000},{'requested_size':12,'linear':0x2000}]
        self.assertEqual(probe.allocation_bases(rows,objects,4),{1:0x1000,2:0x2000})
        for bad in (rows[:1],rows+[rows[0]],[rows[0],{'requested_size':12,'linear':0x1002}],
                    [{'requested_size':8,'linear':-1},rows[1]],
                    [rows[0],{'requested_size':12,'linear':16*1024*1024-1}]):
            with self.subTest(bad=bad),self.assertRaises(ValueError):probe.allocation_bases(bad,objects,4)
    def test_output_cannot_clobber_source_install_or_existing_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);original=root/'original';original.mkdir();install=root/'emulator';install.mkdir()
            exe=install/'dosbox-x.exe';exe.write_bytes(b'fixture')
            for path in (original,original/'new',install/'new',root):
                with self.subTest(path=path),self.assertRaises(ValueError):probe.output_guard(path,original,exe)
            self.assertEqual(probe.output_guard(root/'new',original,exe),root/'new')
    def test_loader_extension_still_rejects_guest_writes_and_command_injection(self):
        rpc=probe.LoaderRPC.__new__(probe.LoaderRPC);rpc._owner=Mock();rpc._exchange=Mock(return_value=['ok'])
        for allowed in ('EXEC RUN','EXEC GDT','EXEC BPDEL *','EXEC BPINT 31 05','EXEC BP 280:1234'):
            self.assertEqual(rpc.request(allowed),['ok'])
        for bad in ('BREAK','EXEC SR EAX 1','EXEC SM 178:0 12','EXEC BP 280:1\nEXEC RUN'):
            rpc._exchange.reset_mock()
            with self.subTest(bad=bad),self.assertRaises(ValueError):rpc.request(bad)
            rpc._exchange.assert_not_called()
    def test_hidden_console_owns_real_handles_without_redirecting_curses(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for hidden in (False,True):
                target=Mock(pid=123);target.poll.return_value=0
                receipts=[]
                with patch.object(native.sys,'platform','win32'),patch.object(native.subprocess,'STARTUPINFO',
                        return_value=SimpleNamespace(dwFlags=0,wShowWindow=0),create=True),\
                     patch.object(native.subprocess,'STARTF_USESHOWWINDOW',1,create=True),\
                     patch.object(native.subprocess,'CREATE_NEW_CONSOLE',16,create=True),\
                     patch.object(native.subprocess,'Popen',return_value=target) as launch:
                    with native.running(['dosbox-x.exe'],root,{},root/'log',Path('driver'),receipts,hidden_console=hidden):pass
                    kwargs=launch.call_args.kwargs
                    # reference声明的quiet必须出现在实际Popen argv及运行收据中；
                    # running内部追加覆盖，不能只看调用者传入的原始command。
                    actual_command=launch.call_args.args[0]
                    self.assertEqual(actual_command,['dosbox-x.exe','-set','mixer nosound=true'])
                    self.assertEqual(receipts[0]['command'],actual_command)
                    self.assertTrue(receipts[0]['native_quiet_requested'])
                    self.assertEqual('stdout' in kwargs,not hidden)
                    self.assertEqual(kwargs['startupinfo'].wShowWindow,0 if hidden else 4)
                    self.assertEqual(kwargs.get('creationflags',0),16 if hidden else 0)
                    self.assertTrue(receipts[0]['cleaned']);self.assertFalse(receipts[0]['forced_cleanup'])

    def test_dump_rejects_real_or_v86_even_when_old_flat_descriptor_remains(self):
        descriptor=['SelectorInfo 178:', '  : b:00000000 type:12 parbg',
                    '    l:FFFFFFFF dpl : 0 10011']
        for registers in ({'CR0':0,'EFLAGS':2}, {'CR0':1,'EFLAGS':(1<<17)|2}):
            with self.subTest(registers=registers),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);rpc=Mock();rpc.regs.return_value=registers
                def response(command):
                    if command.startswith('EXEC SELINFO '):return descriptor
                    # 模拟先前描述符仍可见，但MEMDUMP返回环读/零填充RAM的错误域。
                    if command.startswith('EXEC MEMDUMPBIN '):
                        with (root/'invalid.bin').open('wb') as stream:stream.truncate(16*1024*1024)
                        return ['DEBUG: Memory dump binary success.']
                    raise AssertionError(command)
                rpc.request.side_effect=response
                with self.assertRaises(ValueError):probe.dump(rpc,root,'invalid',0x178)

    def test_source_guard_reports_changed_missing_and_unreadable_without_raising(self):
        with tempfile.TemporaryDirectory() as tmp:
            good=Path(tmp)/'good';good.write_bytes(b'original')
            guards={str(good):probe.sha(good),str(good.with_name('missing')):'f'*64,'unreadable':'e'*64}
            actual_sha=probe.sha
            def read(path):
                if path=='unreadable':raise PermissionError('fixture denied')
                return actual_sha(path)
            with patch.object(probe,'sha',side_effect=read):
                rows=probe.check_sources(guards)
            self.assertEqual([r['ok'] for r in rows],[True,False,False])
            self.assertEqual([r['error']['type'] for r in rows[1:]],['FileNotFoundError','PermissionError'])
            good.write_bytes(b'changed')
            self.assertFalse(probe.check_sources({str(good):guards[str(good)]})[0]['ok'])

    def test_prelaunch_host_error_is_preserved_with_source_manifest_and_late_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);original=root/'original';original.mkdir();(original/'FD2.EXE').write_bytes(b'fixture')
            install=root/'install';install.mkdir()
            dosbox=install/'dosbox-x.exe';dosbox.write_bytes(b'dosbox')
            driver=root/'driver.exe';driver.write_bytes(b'driver')
            bootstrap=root/'user.conf';bootstrap.write_bytes(b'bootstrap')
            args=SimpleNamespace(original=original,dosbox=dosbox,driver=driver,
                bootstrap_config=bootstrap,artifacts=root/'capture')
            actual_sha=probe.sha;first=True
            def guarded_sha(path):
                nonlocal first
                if Path(path).resolve()==bootstrap.resolve():
                    if not first:raise PermissionError('late guard denied')
                    first=False
                return actual_sha(path)
            with patch.object(probe,'owned_job_required'),patch.object(probe,'sha',side_effect=guarded_sha),\
                    patch.object(probe,'parse_le',side_effect=ValueError('primary parse failure')),\
                    patch.object(probe,'running') as launch:
                report=probe.run(args)
            launch.assert_not_called()
            self.assertEqual(report['error']['message'],'primary parse failure')
            self.assertFalse(report['ok']);self.assertFalse(report['sources_unchanged'])
            self.assertIn(str((probe.ROOT/'tools/rebuild_fd2_analysis.py').resolve()),report['source_guards'])
            self.assertIn(str((probe.ROOT/'tools/analyze_fd2_audio.py').resolve()),report['source_guards'])
            self.assertTrue(any(row.get('error',{}).get('type')=='PermissionError' for row in report['source_guard_check']))
            self.assertEqual(json.loads((args.artifacts/'report.json').read_text(encoding='utf8')),report)

    def test_missing_prelaunch_input_still_creates_failure_receipt(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);original=root/'original';original.mkdir();install=root/'install';install.mkdir()
            args=SimpleNamespace(original=original,dosbox=install/'dosbox-x.exe',driver=root/'driver.exe',
                bootstrap_config=root/'missing.conf',artifacts=root/'capture')
            with patch.object(probe,'owned_job_required'),patch.object(probe,'running') as launch:
                report=probe.run(args)
            launch.assert_not_called()
            self.assertFalse(report['ok']);self.assertEqual(report['error']['type'],'FileNotFoundError')
            self.assertTrue((args.artifacts/'report.json').is_file())

    def test_foreground_worker_failure_cannot_leave_a_passable_audit(self):
        ready=threading.Event();owner=threading.get_ident();record={}
        def sample():
            if threading.get_ident()!=owner:
                ready.set();raise OSError('foreground sampler failed')
            return {'hwnd':1,'pid':7}
        with patch.object(probe.ForegroundAudit,'sample',side_effect=sample):
            with self.assertRaisesRegex(RuntimeError,'前台采样不完整'):
                with probe.ForegroundAudit(record):self.assertTrue(ready.wait(1))
        self.assertTrue(record['thread_joined']);self.assertFalse(record['sampling_complete'])
        self.assertEqual(record['errors'],[{'type':'OSError','message':'foreground sampler failed'}])
        self.assertEqual(record['after'],{'hwnd':1,'pid':7})

    def test_foreground_failure_keeps_primary_body_exception(self):
        ready=threading.Event();owner=threading.get_ident();record={}
        def sample():
            if threading.get_ident()!=owner:
                ready.set();raise OSError('secondary sampler failure')
            return {'hwnd':1,'pid':7}
        with patch.object(probe.ForegroundAudit,'sample',side_effect=sample):
            with self.assertRaisesRegex(ValueError,'primary body failure'):
                with probe.ForegroundAudit(record):
                    self.assertTrue(ready.wait(1));raise ValueError('primary body failure')
        self.assertFalse(record['sampling_complete']);self.assertEqual(record['errors'][0]['type'],'OSError')

    def test_foreground_after_failure_is_recorded_and_fails_qualification(self):
        owner=threading.get_ident();count=0;record={}
        def sample():
            nonlocal count
            if threading.get_ident()==owner:
                count+=1
                if count==2:raise OSError('after sample failed')
            return {'hwnd':1,'pid':7}
        with patch.object(probe.ForegroundAudit,'sample',side_effect=sample):
            with self.assertRaisesRegex(RuntimeError,'前台采样不完整'):
                with probe.ForegroundAudit(record):pass
        self.assertTrue(record['thread_joined']);self.assertFalse(record['sampling_complete'])
        self.assertEqual(record['errors'][0]['message'],'after sample failed')

    def test_foreground_success_records_a_bounded_sampler_and_endpoints(self):
        ready=threading.Event();owner=threading.get_ident();record={}
        def sample():
            if threading.get_ident()!=owner:ready.set()
            return {'hwnd':1,'pid':7}
        with patch.object(probe.ForegroundAudit,'sample',side_effect=sample):
            with probe.ForegroundAudit(record):self.assertTrue(ready.wait(1))
        self.assertTrue(record['sampling_complete']);self.assertTrue(record['thread_joined'])
        self.assertGreaterEqual(record['sample_count'],1);self.assertEqual(record['before'],record['after'])
        self.assertFalse(record['errors'])

if __name__=='__main__':unittest.main()
