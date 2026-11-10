"""CDB符号owner协议纯解析/mock；无attach、窗口或实际外部CDB进程。"""
from pathlib import Path
import ctypes
import io
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import fd2_cdb_prepare_snapshot as cdb
import fd2_dosbox_snapshot as oracle


class CDBSnapshotTest(unittest.TestCase):
    def test_main_thread_frame_is_owned_unique_and_not_fixed_stack_offset(self):
        trace=['.  0  Id: 1234.81a Suspend: 1 Teb: 0000 Unfrozen',
            '00 00000012`abc00000 00007ff1`00001000 ntdll!NtWaitForSingleObject',
            '0a 00000012`abc00500 00007ff1`00005000 fd2sdl!main+0x4c',
            '   1  Id: 1234.826 Suspend: 1 Teb: 0000 Unfrozen',
            '00 00000013`abc00000 00007ff1`00001000 SDL3!SDL_WaitCondition']
        result=cdb.main_frame(trace,0x1234)
        self.assertEqual((result['thread_id'],result['frame']),(0x81a,10))
        for bad in (trace[2:3],trace+trace,trace):
            with self.assertRaises(ValueError):cdb.main_frame(bad,0x9999 if bad is trace else 0x1234)

    def test_cpp_values_require_one_result_and_allow_real_zero_offset(self):
        for line,value in (('unsigned __int64 0',0),('unsigned int64 0n2572',2572),
            ('unsigned __int64 0x00000012`abc00000',0x12abc00000),
            ('struct fd2_prepare_state * 0x0000006e`221b5000',0x6e221b5000),
            ('struct fd2_bgm_player * 0x00000123`abc00000',0x123abc00000),
            ('int64 0n0',0),('long 0n20',20),('unsigned int 0x8',8)):
            self.assertEqual(cdb.cpp_integer([line]),value)
        for lines in ([],['0:000> ?? 0x1234'],['unsigned int64 0x10','unsigned int64 0x20'],
            ['ERROR: symbol not found'],['int64 0n-1'],['struct fd2_prepare_state * 0x1234 extra'],
            ['*** Type referenced: unsigned __int64 ***'],
            ["Couldn't resolve error at 'unsigned __int64)(&app_context.preparation)'"]):
            with self.assertRaises(ValueError):cdb.cpp_integer(lines)

    def test_take_queries_pdb_values_without_the_cast_rejected_by_native04(self):
        with tempfile.TemporaryDirectory(prefix='g3-cdb-layout-') as tmp:
            work=Path(tmp);exe=work/'fd2sdl.exe';pdb=work/'fd2sdl.pdb'
            exe.write_bytes(b'owned app');pdb.write_bytes(b'owned pdb')
            record={};debugger=Mock();debugger.__enter__=Mock(return_value=debugger);debugger.__exit__=Mock(return_value=False)
            values={'&app_context.preparation':'struct fd2_prepare_state * 0x12340000',
                'sizeof(app_context.preparation)':'unsigned int64 0n2580'}
            for name,offset in (('stage_id',0),('unit_count',1),('gold_or_flags',8),('options',12),('unit_area',20)):
                values['(char *)&app_context.preparation.'+name+' - (char *)&app_context.preparation']='int64 0n'+str(offset)
            def request(command):
                if command.startswith('~*kn'):
                    return ['.  0  Id: 1234.81a Suspend: 1 Teb: 0000 Unfrozen',
                        '1b 00000012`abc00500 00007ff1`00005000 fd2sdl!main+0x4c']
                if command=='dv /t /v app_context':return ['00001234 struct fd2_field_play_context app_context = struct fd2_field_play_context']
                if command.startswith('?? '):return [values[command[3:]]]
                return []
            debugger.request.side_effect=request
            identity={'exe_sha256':cdb.sha(exe),'pdb_sha256':cdb.sha(pdb)}
            data=bytearray(2580);data[:2]=bytes((27,21));target=Mock(pid=0x1234)
            with patch.object(cdb,'symbols',return_value=identity),patch.object(cdb,'CDB',return_value=debugger),\
                patch.object(cdb,'read_owned_memory',return_value=(bytes(data),{'method':'ReadProcessMemory'})) as read:
                state,receipt=cdb.take('cdb.exe',target,exe,pdb,work,record)
                read.assert_called_once_with(target,0x12340000,2580,exe)
            self.assertEqual((state['stage'],state['count']),(27,21))
            self.assertEqual(receipt['layout']['offsets']['unit_area'],20)
            commands=[row.args[0] for row in debugger.request.call_args_list]
            self.assertIn('?? &app_context.preparation',commands)
            self.assertIn('?? sizeof(app_context.preparation)',commands)
            self.assertFalse(any('__int64' in command for command in commands))
            self.assertFalse(any(command.startswith('.writemem ') for command in commands))
            self.assertTrue(receipt['all_fields_from_one_ReadProcessMemory'])
            self.assertTrue(receipt['all_reads_in_one_stopped_attach'])
            debugger.__exit__.assert_called_once()

    def memory_fixture(self,path):
        target=Mock(pid=123);target._handle=0x123456789abc;target.poll.return_value=None
        data=bytes(range(32));api=SimpleNamespace(GetProcessId=Mock(return_value=123))
        def exit_code(handle,out):out._obj.value=259;return True
        def image(handle,flags,out,length):out.value=str(path.resolve());length._obj.value=len(out.value);return True
        def read(handle,address,out,size,count):
            ctypes.memmove(out,data,size);count._obj.value=size;return True
        api.GetExitCodeProcess=Mock(side_effect=exit_code);api.QueryFullProcessImageNameW=Mock(side_effect=image)
        api.ReadProcessMemory=Mock(side_effect=read)
        return target,data,api

    def test_owned_rpm_uses_existing_64bit_handle_one_full_read_and_both_owner_checks(self):
        with tempfile.TemporaryDirectory(prefix='g3-owned-rpm-') as tmp:
            exe=Path(tmp)/'owned.exe';exe.write_bytes(b'owned')
            target,data,api=self.memory_fixture(exe);address=0x123456780000
            actual,receipt=cdb.read_owned_memory(target,address,len(data),exe,api=api)
            self.assertEqual(actual,data);self.assertEqual(receipt['method'],'ReadProcessMemory')
            self.assertTrue(receipt['existing_Popen_handle_used']);self.assertFalse(receipt['OpenProcess_used'])
            api.ReadProcessMemory.assert_called_once();args=api.ReadProcessMemory.call_args.args
            self.assertEqual((args[0],args[1].value,args[3]),(target._handle,address,len(data)))
            self.assertEqual(api.GetProcessId.call_count,2);self.assertEqual(api.QueryFullProcessImageNameW.call_count,2)
            self.assertEqual(receipt['owner_before'],receipt['owner_after'])
            self.assertTrue(receipt['all_requested_bytes_read']);self.assertFalse(receipt['target_memory_written'])

    def test_owned_rpm_rejects_wrong_owner_or_exit_before_and_after_read(self):
        with tempfile.TemporaryDirectory(prefix='g3-owned-rpm-bad-') as tmp:
            exe=Path(tmp)/'owned.exe';exe.write_bytes(b'owned')
            for failure in ('pid_before','pid_after','image_before','image_after','exit_code','poll_after','query_error'):
                with self.subTest(failure=failure):
                    target,data,api=self.memory_fixture(exe)
                    if failure=='pid_before':api.GetProcessId.return_value=999
                    elif failure=='pid_after':api.GetProcessId.side_effect=[123,999]
                    elif failure in ('image_before','image_after'):
                        good=api.QueryFullProcessImageNameW.side_effect
                        def wrong(handle,flags,out,length):out.value=str(Path(tmp)/'other.exe');length._obj.value=len(out.value);return True
                        count=[0]
                        def image(*args):count[0]+=1;return (wrong if failure=='image_before' or count[0]>1 else good)(*args)
                        api.QueryFullProcessImageNameW.side_effect=image
                    elif failure=='exit_code':
                        def ended(handle,out):out._obj.value=0;return True
                        api.GetExitCodeProcess.side_effect=ended
                    elif failure=='poll_after':target.poll.side_effect=[None,0]
                    else:api.QueryFullProcessImageNameW.side_effect=None;api.QueryFullProcessImageNameW.return_value=False
                    with self.assertRaises((ValueError,OSError)):cdb.read_owned_memory(target,0x123456780000,len(data),exe,api=api)
                    self.assertEqual(api.ReadProcessMemory.call_count,1 if failure in ('pid_after','image_after','poll_after') else 0)

    def test_owned_rpm_never_accepts_short_read_api_failure_or_invalid_range(self):
        with tempfile.TemporaryDirectory(prefix='g3-owned-rpm-length-') as tmp:
            exe=Path(tmp)/'owned.exe';exe.write_bytes(b'owned')
            for mode in ('short','false'):
                with self.subTest(mode=mode):
                    target,data,api=self.memory_fixture(exe)
                    def read(handle,address,out,size,count):count._obj.value=size-1;return mode!='false'
                    api.ReadProcessMemory.side_effect=read
                    with self.assertRaises((ValueError,OSError)):cdb.read_owned_memory(target,0x12340000,len(data),exe,api=api)
                    api.ReadProcessMemory.assert_called_once()
            for address,size in ((0,32),(-1,32),(True,32),(0x1000,0),(0x1000,1024*1024+1),(2**64-1,32)):
                target,data,api=self.memory_fixture(exe)
                with self.assertRaises(ValueError):cdb.read_owned_memory(target,address,size,exe,api=api)
                api.ReadProcessMemory.assert_not_called()
            for handle in (None,0,True):
                target,data,api=self.memory_fixture(exe);target._handle=handle
                with self.assertRaises(ValueError):cdb.read_owned_memory(target,0x1000,32,exe,api=api)
                api.GetProcessId.assert_not_called()

    def test_owned_rpm_win32_prototypes_preserve_handle_pointer_and_size_widths(self):
        api=SimpleNamespace(**{name:Mock() for name in ('GetProcessId','GetExitCodeProcess','QueryFullProcessImageNameW','ReadProcessMemory')})
        with patch.object(cdb.ctypes,'WinDLL',return_value=api):self.assertIs(cdb._memory_api(),api)
        self.assertEqual(api.GetProcessId.argtypes,[cdb.wt.HANDLE])
        self.assertEqual(api.QueryFullProcessImageNameW.argtypes,[cdb.wt.HANDLE,cdb.wt.DWORD,cdb.wt.LPWSTR,ctypes.POINTER(cdb.wt.DWORD)])
        self.assertEqual(api.ReadProcessMemory.argtypes,[cdb.wt.HANDLE,cdb.wt.LPCVOID,cdb.wt.LPVOID,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)])

    def test_codeview_guid_and_age_must_match_pe_and_pdb(self):
        with tempfile.TemporaryDirectory(prefix='g3-cdb-pure-') as tmp:
            paths=[Path(tmp)/name for name in ('app.exe','app.pdb','dbghelp.dll')]
            for path in paths:path.write_bytes(path.name.encode())
            first={'guid':'11111111-2222-3333-4444-555555555555','age':7}
            reader=Mock(side_effect=[first,dict(first)])
            proof=cdb.symbols(*paths,reader=reader);self.assertTrue(proof['GUID_age_match'])
            for change in ({'age':8},{'guid':'aaaaaaaa-2222-3333-4444-555555555555'}):
                with self.assertRaises(ValueError):cdb.symbols(*paths,reader=Mock(side_effect=[first,{**first,**change}]))

    def test_decode_uses_pdb_offsets_and_full_unknown_roster_tail(self):
        layout={'size':2580,'offsets':{'stage_id':0,'unit_count':1,'gold_or_flags':8,'options':12,'unit_area':20}}
        data=bytearray(2580);data[:2]=bytes((27,21));struct.pack_into('<I',data,8,0xffffffff);data[12:16]=b'\x80\x01\x02\x03'
        data[20:]=bytes(i&255 for i in range(2560));state=cdb.decode(bytes(data),layout)
        self.assertEqual((state['stage'],state['count'],state['resource']),(27,21,0xffffffff))
        self.assertEqual(state['roster'][-1],255);self.assertEqual(len(state['roster']),2560)
        broken=bytes(data[:-1])+b'\x00';other=cdb.decode(broken,layout)
        self.assertEqual(oracle.check_state(other,state)['roster_mismatch_offsets'],[2559])
        for bad in ({**layout,'size':2579},{**layout,'offsets':{**layout['offsets'],'unit_area':0}},
            {**layout,'offsets':{**layout['offsets'],'options':2579}}):
            with self.assertRaises(ValueError):cdb.decode(bytes(data),bad)

    def test_failure_still_sends_detach_quit_and_proves_same_target_alive(self):
        with tempfile.TemporaryDirectory(prefix='g3-cdb-close-') as tmp:
            target=Mock(pid=123);target.poll.return_value=None;record={};probe=cdb.CDB('cdb.exe',target,tmp,record)
            process=Mock();process.poll.side_effect=[None,0,0];probe.process=process
            probe.__exit__(ValueError,ValueError('read failed'),None)
            process.stdin.write.assert_called_once_with(b'.detach;q\n')
            process.wait.assert_called_once_with(timeout=10)
            self.assertTrue(record['detached']);self.assertTrue(record['closed'])
            self.assertTrue((Path(tmp)/'cdb-transcript.txt').exists())

    def test_sympath_marker_is_separate_line_not_an_extra_symbol_path(self):
        target=Mock(pid=123);target.poll.return_value=None
        probe=cdb.CDB('cdb.exe',target,'.',{'requests':[]});probe.process=Mock();probe.process.poll.return_value=None
        probe.nonce='testnonce';probe._until=Mock(return_value=['Symbol search path is: E:/symbols;E:/fallback'])
        result=probe.request('.sympath E:/symbols;E:/fallback')
        self.assertEqual(result,['Symbol search path is: E:/symbols;E:/fallback'])
        probe.process.stdin.write.assert_called_once_with(
            b'.sympath E:/symbols;E:/fallback\n.echo FD2_testnonce_1\n')
        probe._until.assert_called_once_with('FD2_testnonce_1')
        self.assertNotIn(b';.echo',probe.process.stdin.write.call_args.args[0])
        # 命令回显里出现marker不算ACK，必须等其独立输出行。
        fresh=cdb.CDB('cdb.exe',target,'.',{})
        fresh.events.put('0:000> .echo FD2_testnonce_1');fresh.events.put('FD2_testnonce_1')
        self.assertEqual(fresh._until('FD2_testnonce_1'),['0:000> .echo FD2_testnonce_1'])

    def test_reader_strips_only_anchored_prompt_keeps_raw_and_rejects_command_echo(self):
        target=Mock(pid=0x1234);probe=cdb.CDB('cdb.exe',target,'.',{})
        raw=(b'0:065> .echo FD2_marker\r\n'
            b'0:065> FD2_marker\r\n'
            b'2:00a:x86> ?? (unsigned __int64)(0x1234)\r\n'
            b'2:00a:x86> unsigned __int64 0x00000012`abc00000\r\n'
            b'> unsigned int64 0n2572\r\n')
        probe.process=SimpleNamespace(stdout=io.BytesIO(raw));probe._read_stdout()
        self.assertEqual(bytes(probe.raw_output),raw)
        self.assertEqual(probe.output[0],'0:065> .echo FD2_marker')
        self.assertEqual(probe.output[1],'0:065> FD2_marker')
        self.assertEqual(probe._until('FD2_marker'),['.echo FD2_marker'])
        expression=[probe.events.get(),probe.events.get()]
        self.assertEqual(cdb.cpp_integer(expression),0x12abc00000)
        self.assertEqual(cdb.cpp_integer([probe.events.get()]),2572)
        self.assertIsNone(probe.events.get())
        with self.assertRaises(ValueError):cdb.cpp_integer(['?? (unsigned __int64)(0x1234)'])
        with self.assertRaises(ValueError):cdb.cpp_integer(['diagnostic address 0x1234'])
        for line in ('prefix 0:065> FD2_marker','abc> FD2_marker','12:xyz> FD2_marker','0:065x> FD2_marker'):
            self.assertEqual(cdb.normalize_output_line(line),line)

    def test_reader_normalizes_first_thread_banner_before_main_frame_parser(self):
        target=Mock(pid=0x1234);probe=cdb.CDB('cdb.exe',target,'.',{})
        raw=(b'3:002> .  0  Id: 1234.81a Suspend: 1 Teb: 0000 Unfrozen\r\n'
            b'0a 00000012`abc00500 00007ff1`00005000 fd2sdl!main+0x4c\r\n'
            b'3:002> FD2_stack\r\n')
        probe.process=SimpleNamespace(stdout=io.BytesIO(raw));probe._read_stdout()
        trace=probe._until('FD2_stack');frame=cdb.main_frame(trace,0x1234)
        self.assertEqual((frame['thread_id'],frame['frame']),(0x81a,10))
        self.assertTrue(probe.output[0].startswith('3:002> '))


if __name__=='__main__':unittest.main()
