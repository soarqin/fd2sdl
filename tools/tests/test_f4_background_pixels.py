"""只测协议／守卫，不运行probe、不读原游戏；合成bytes不作像素oracle。"""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import sys

spec=importlib.util.spec_from_file_location('bg',Path(__file__).resolve().parents[1]/'verify_f4_background_pixels.py')
bg=importlib.util.module_from_spec(spec);spec.loader.exec_module(bg)

class Protocol(unittest.TestCase):
    def setUp(self):
        self.raw=b'a'*bg.P+b'b'*bg.P+b'c'*bg.V+b'd'*bg.V+b'e'*768+b'f'*768+b'\0'*(bg.P+bg.V)
        self.domain=dict(case=0,stage=0,op='none',camera=[0,0],tick=0,advance=1,setter=0,
            before_meta=[0,0,0,0,0,1,1,0,0],after_meta=[0,0,0,0,0,1,1,0,0],
            source_unchanged=True,work_unchanged=True,padding_unchanged=True,other_owners_unchanged=True,
            raw_sha256=hashlib.sha256(self.raw).hexdigest())
        self.row=copy.deepcopy(self.domain)
        at=0
        for k,n in [('before_page',bg.P),('after_page',bg.P),('before_vga',bg.V),('after_vga',bg.V),('palette',768),('dac',768)]:
            self.row[k]=self.raw[at:at+n].hex();at+=n
    def test_valid_protocol_only(self):bg.check_case(self.row,self.domain,self.raw)
    def test_loaded_resource_same_frozen_bytes(self):
        row=dict(schema='fd2-f4-background-family-actual-v1',binding='FDOTHER.DAT',size=3,bytes_hex='616263')
        bg.header(row,b'abc')
        with self.assertRaises(ValueError):bg.header(row,b'aba')
    def test_exact_metadata_not_just_frame(self):
        self.row['after_meta'][4]=1
        with self.assertRaisesRegex(ValueError,'after_meta'):bg.check_case(self.row,self.domain,self.raw)
    def test_complete_before_owner(self):
        self.row['before_page']='00'+self.row['before_page'][2:]
        with self.assertRaisesRegex(ValueError,'before_page'):bg.check_case(self.row,self.domain,self.raw)
    def test_complete_after_owner(self):
        self.row['after_vga']=self.row['after_vga'][:-2]
        with self.assertRaisesRegex(ValueError,'after_vga'):bg.check_case(self.row,self.domain,self.raw)
    def test_expected_raw_pin(self):
        with self.assertRaisesRegex(ValueError,'extent/hash'):bg.check_case(self.row,self.domain,self.raw[:-1])
    def test_same_tick_work_must_hold(self):
        self.row['work_unchanged']=False
        with self.assertRaises(ValueError):bg.check_case(self.row,self.domain,self.raw)
    def test_unknown_new_work_not_expected(self):
        self.domain['work_unchanged']=None;self.row['work_unchanged']=False
        bg.check_case(self.row,self.domain,self.raw)
    def test_guard_pin_and_late_change(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'source';p.write_bytes(b'one');facts=[]
            with self.assertRaises(ValueError):bg.freeze(p,[],'0'*64)
            self.assertEqual(bg.freeze(p,facts),b'one');p.write_bytes(b'two')
            self.assertFalse(bg.guards(facts)['unchanged'])
    def test_output_rejects_original_even_under_allowed_root(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);actual=root/'build/f4/real-original';target=actual/'new'
            real_resolve=Path.resolve
            def resolve(p,*args,**kw):
                return real_resolve(actual if p==root/'original_game' else p,*args,**kw)
            with mock.patch.object(Path,'resolve',autospec=True,side_effect=resolve):
                with self.assertRaisesRegex(ValueError,'original_game'):bg.fresh(root,target)
            self.assertFalse(target.exists())
    def test_output_never_overwrites(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);target=root/'build/f4/old';target.mkdir(parents=True)
            with self.assertRaisesRegex(ValueError,'fresh'):bg.fresh(root,target)
    def test_report_error_keeps_primary(self):
        result=dict(passed=False,primary_error='pixel mismatch',secondary_errors=[])
        with mock.patch.object(Path,'write_text',side_effect=OSError('disk full')):
            bg.save_report(Path('not-written'),result)
        self.assertEqual(result['primary_error'],'pixel mismatch')
        self.assertEqual(result['secondary_errors'][0]['operation'],'report.write')
    def test_cleanup_errors_keep_business_error_and_report_owner(self):
        result=dict(passed=False,primary_error='pixel mismatch',secondary_errors=[])
        p=mock.Mock(pid=123);p.poll.return_value=None
        p.terminate.side_effect=OSError('terminate failed');p.kill.side_effect=OSError('kill failed')
        p.wait.side_effect=bg.subprocess.TimeoutExpired('probe',5)
        p.stdout.close.side_effect=OSError('close failed')
        reader=mock.Mock();reader.is_alive.return_value=False
        bg.cleanup_probe(p,reader,result)
        self.assertEqual(result['primary_error'],'pixel mismatch')
        self.assertEqual(result['owned_pid'],123);self.assertFalse(result['owned_process_ended'])
        self.assertGreaterEqual(len(result['secondary_errors']),4)
    def test_cleanup_error_without_business_error_fails_receipt(self):
        result=dict(passed=True,primary_error=None,secondary_errors=[])
        p=mock.Mock(pid=123);p.poll.return_value=0;p.stdout.close.side_effect=OSError('close failed')
        bg.cleanup_probe(p,None,result)
        self.assertFalse(result['passed']);self.assertTrue(result['owned_process_ended'])
        self.assertEqual(result['primary_error'],'owned probe cleanup failed')
    def test_exact_finite_denominator(self):
        rows=list(bg.requests());self.assertEqual(len(rows),343)
        self.assertEqual(sum(r['op']=='none' for r in rows),23)
        self.assertEqual(sum(r['op']=='parallax' for r in rows),16)
        self.assertEqual(sum(r['op']=='wave_update' for r in rows),35)
        self.assertEqual(sum(r['op']=='wave_same' for r in rows),35)
        self.assertEqual(sum(r['op'].startswith('scroll') for r in rows),234)

if __name__ == '__main__':
    unittest.main()
