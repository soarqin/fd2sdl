"""RPC全mock与独立合成relocated RAM；无DOSBox/窗口/实际TCP/Win32操作。"""
from pathlib import Path
import io
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import fd2_dosbox_snapshot as snap
import fd2_g3_prepare_oracle as oracle
from rebuild_fd2_analysis import build_relbase_image,parse_le,parse_objects,parse_fixups


class SnapshotTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=ROOT/'original_game';cls.exe=(cls.original/'FD2.EXE').read_bytes()
        cls.le=parse_le(cls.exe);cls.objects=parse_objects(cls.exe,cls.le)
        cls.fixups=parse_fixups(cls.exe,cls.le,cls.objects)
        _,_,cls.before,cls.expected,_=oracle.derive_fixture(cls.original,'p5-select21-final-no')
        raw=build_relbase_image(cls.exe,cls.le,cls.objects)
        memory=bytearray(snap.RAM_SIZE);cls.bases={1:0x20000,2:0x80000,3:0xa0000}
        for obj in cls.objects:
            memory[cls.bases[obj.index]:cls.bases[obj.index]+obj.vsize]=raw[obj.relbase:obj.relbase+obj.vsize]
        for f in cls.fixups:
            owner=next(obj for obj in cls.objects if obj.relbase<=f.source_addr_relbase<obj.relbase+obj.vsize)
            struct.pack_into('<I',memory,cls.bases[owner.index]+f.source_addr_relbase-owner.relbase,
                cls.bases[f.target_object]+f.target_offset)
        db=cls.bases[2];cls.pointer=0x120000
        memory[cls.pointer:cls.pointer+2560]=cls.expected['roster']
        for at,value in ((0x3bf7,cls.pointer),(0x3bfb,cls.expected['count']),
            (0x3c03,cls.expected['stage']),(0x3bf3,cls.expected['resource'])):
            struct.pack_into('<I',memory,db+at,value)
        for at,value in zip((0x1aab,0x3af9,0x1e61,0x1e62),cls.expected['options']):memory[db+at]=value
        cls.memory=bytes(memory)

    def test_actual_source_fixups_find_owner_with_nonzero_descriptor_base(self):
        actual,receipt=snap.resolve(self.memory,self.original,{'base':0x1500})
        self.assertTrue(snap.check_state(actual,self.expected)['ok'])
        self.assertEqual(receipt['object_DS_offsets'],{str(k):v for k,v in self.bases.items()})
        self.assertEqual(receipt['roster_pointer_linear'],self.pointer+0x1500)
        self.assertFalse(receipt['roster_content_scan_used']);self.assertEqual(receipt['roster_bytes'],2560)
        self.assertGreaterEqual(len(receipt['relocation_proofs']),4)

    def test_duplicate_code_anchor_and_broken_fixup_rejected_without_roster_fallback(self):
        anchor=snap._anchors(self.exe,self.fixups)[0];memory=bytearray(self.memory)
        start=self.bases[1]+anchor['cva']-0x10000;memory[0x180000:0x180000+96]=memory[start:start+96]
        with self.assertRaisesRegex(ValueError,'唯一match'):snap.resolve(bytes(memory),self.original,{'base':0})
        memory=bytearray(self.memory)
        f=next(f for f in self.fixups if 0x26152<=f.source_addr_relbase<0x265ec and f.target_object==2)
        at=self.bases[1]+f.source_addr_relbase-0x10000;memory[at]^=1
        with self.assertRaisesRegex(ValueError,'fixup'):snap.resolve(bytes(memory),self.original,{'base':0})

    def test_cold_table_bad_owner_and_complete_unknown_tail_are_not_ignored(self):
        memory=bytearray(self.memory);memory[self.bases[3]+0x188a]^=1
        with self.assertRaisesRegex(ValueError,'完整原表'):snap.resolve(bytes(memory),self.original,{'base':0})
        memory=bytearray(self.memory);memory[self.pointer+2559]^=1
        actual,_=snap.resolve(bytes(memory),self.original,{'base':0});result=snap.check_state(actual,self.expected)
        self.assertFalse(result['ok']);self.assertEqual(result['roster_mismatch_offsets'],[2559])
        memory=bytearray(self.memory);struct.pack_into('<I',memory,self.bases[2]+0x3bf7,0x160000)
        actual,_=snap.resolve(bytes(memory),self.original,{'base':0})
        self.assertFalse(snap.check_state(actual,self.expected)['ok'])  # 正确roster仍在其它位置，不搜索它。

    def session(self,response,owner=123):
        record={'requests':[]};session=snap.OwnedRPC(record,owner_query=lambda a,b:owner)
        session.process=Mock(pid=123);session.process.poll.return_value=None
        session.peer=('127.0.0.1',2000);session.port=3000;session.connection=Mock();session.stream=io.BytesIO(response)
        return session,record

    def test_rpc_framing_owner_and_read_only_whitelist(self):
        session,record=self.session(b'BEGIN 1 OK\nPONG\nEND 1\n')
        self.assertEqual(session.request('PING'),['PONG'])
        session.connection.sendall.assert_called_once_with(b'REQ 1 PING\n')
        self.assertTrue(record['requests'][0]['ok'])
        for command in ('BREAK','EXEC RUN','EXEC SR EAX 0','EXEC SM DS:0 0','EXEC EV CS\nREQ 2 BREAK'):
            session,_=self.session(b'')
            with self.assertRaises(ValueError):session.request(command)
            session.connection.sendall.assert_not_called()
        session,_=self.session(b'',owner=99)
        with self.assertRaisesRegex(ValueError,'归属'):session.request('PING')
        session.connection.sendall.assert_not_called()

    def test_rpc_failed_truncated_wrong_id_and_nested_response_retain_request(self):
        for response in (b'BEGIN 2 OK\nPONG\nEND 2\n',b'BEGIN 1 OK\nPONG',
            b'BEGIN 1 ERR\nunsupported\nEND 1\n',b'BEGIN 1 OK\nEND 2\n'):
            session,record=self.session(response)
            with self.assertRaises(ValueError):session.request('PING')
            self.assertEqual(len(record['requests']),1)
            self.assertEqual(record['requests'][0]['command'],'PING')

    def test_descriptor_and_registers_fail_closed(self):
        descriptor=snap.parse_selector(['SelectorInfo DS:','DS: b:00001500 type:12 parbg','    l:FFFFFFFF dpl : 3 11111'])
        self.assertEqual((descriptor['base'],descriptor['limit'],descriptor['flags']),(0x1500,0xffffffff,'11111'))
        for lines in ([],['b:00000000 l:FFFFFFFF'],['b:00000000 b:00000000 type:12 l:FFFFFFFF dpl : 3 11111']):
            with self.assertRaises(ValueError):snap.parse_selector(lines)
        registers=snap.parse_registers(["EV of 'CS DS EIP ESP EBP CR0 EFLAGS' is:",'170 178 1234 5000 5050 1 202'])
        self.assertEqual(registers['DS'],0x178)
        for lines in ([],['170 178 1234'],['170 178 1234 5000 5050 1 202']*2):
            with self.assertRaises(ValueError):snap.parse_registers(lines)


if __name__=='__main__':unittest.main()
