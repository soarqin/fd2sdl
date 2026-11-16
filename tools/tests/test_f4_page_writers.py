"""Pure fault/domain contracts; no original code, C producer, resources or GUI."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

PATH = Path(__file__).resolve().parents[1]/'verify_f4_page_writers.py'
SPEC = importlib.util.spec_from_file_location('f4_page_writers',PATH)
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)

class PageWriterContracts(unittest.TestCase):
    def setUp(self):
        self.luts=[bytes((i+j)&255 for i in range(256)) for j in range(23)]
        self.cases=M.cases(self.luts)

    def test_exact_fixed_domain_and_parameter_bindings(self):
        self.assertEqual(len(self.cases),66)
        self.assertEqual([c['case'] for c in self.cases],list(range(66)))
        self.assertEqual([sum(c['kind']==k for c in self.cases) for k in range(5)],[4,27,27,4,4])
        for row in self.cases[20:]:
            self.assertEqual(row['lut'],self.luts[row['lut_index']].hex())
        for row in self.cases:
            self.assertLessEqual(row['x']+24,456)
            self.assertLessEqual(row['y']+24,336)

    def test_source_and_skip_before_values_cover_all_256(self):
        raw=M.stream()
        self.assertEqual(len(raw),303)
        self.assertEqual(set(raw[row*25+1+x] for row in range(11) for x in range(24)),set(range(256)))
        self.assertEqual(raw[275:286],bytes([0xd7])*11)
        self.assertEqual(raw[-17:],bytes([1,0,0xc2,0x44,193,0x88,0,1,127,128,192,193,254,255,0,23,0]))
        for row in self.cases[:20]:
            before=M.before_page(row)
            self.assertEqual(len(before),M.PAGE)
            self.assertEqual({before[(row['y']+y)*456+row['x']+x] for y in range(11,22) for x in range(24)},set(range(256)))

    def test_real_compare_positive_then_before_after_first_last_byte(self):
        row=self.cases[0]; before=M.before_page(row)
        # Hand-declared synthetic after fixture only tests the comparator.
        after=bytes([173])*M.PAGE
        actual=dict(row,before_hex=before.hex(),after_hex=after.hex(),inputs_unchanged=True)
        M.compare_row(actual,row,[before,after,b'',b''])
        for key in ('before_hex','after_hex'):
            for at in (0,M.PAGE-1):
                bad=dict(actual); raw=bytearray.fromhex(bad[key]);raw[at]^=1;bad[key]=raw.hex()
                with self.assertRaisesRegex(ValueError,f'{key} first mismatch {at}'):
                    M.compare_row(bad,row,[before,after,b'',b''])

    def test_inputs_cannot_come_from_actual(self):
        row=self.cases[20];before=M.before_page(row)
        actual=dict(row,before_hex=before.hex(),after_hex=before.hex(),inputs_unchanged=True)
        for key in row:
            bad=dict(actual);bad[key]='changed'
            with self.assertRaisesRegex(ValueError,'input mismatch'):
                M.compare_row(bad,row,[before,before,b'',b''])
        bad=dict(actual,inputs_unchanged=False)
        with self.assertRaisesRegex(ValueError,'owner changed'):
            M.compare_row(bad,row,[before,before,b'',b''])
        bad=dict(actual,after_hex='00')
        with self.assertRaisesRegex(ValueError,'extent mismatch'):
            M.compare_row(bad,row,[before,before,b'',b''])

    def test_artifact_extent_and_hash_guard(self):
        page=bytes([31])*M.PAGE
        audit={key:M.sha(page) for key in ['before_sha256','after_sha256','original_write_mask_sha256','original_destination_read_mask_sha256']}
        metadata={'schema':'fd2-f4-page-writer-original-v1','identity':{'unchanged':True,'before':[],'after':[]},'audits':[audit]*66}
        encoded=json.dumps(metadata).encode('utf8')
        raw=M.HEAD.pack(M.MAGIC,66,len(encoded))+encoded+page*(66*4)
        got,records=M.artifact(raw)
        self.assertEqual(len(records),66)
        self.assertEqual(records[-1][-1],page)
        for bad in (raw[:-1],raw+b'x',b'broken',M.HEAD.pack(M.MAGIC,65,len(encoded))+raw[M.HEAD.size:]):
            with self.assertRaises(ValueError):M.artifact(bad)
        bad=bytearray(raw);bad[-1]^=1
        with self.assertRaisesRegex(ValueError,'page/audit hash mismatch'):M.artifact(bad)

    def test_late_guard_preserves_primary_error_and_mtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'source';path.write_bytes(b'one')
            _,before=M.frozen(path)
            M.finish([before])
            path.write_bytes(b'two')
            with self.assertRaisesRegex(ValueError,'first pixel error') as caught:
                M.finish([before],ValueError('first pixel error'))
            self.assertFalse(caught.exception.receipt['identity']['unchanged'])
            self.assertTrue(caught.exception.receipt['identity']['late_errors'])
            with self.assertRaisesRegex(ValueError,'late SHA/size/mtime'):
                M.finish([before])
            path.unlink()
            with self.assertRaisesRegex(ValueError,'first pixel error') as caught:
                M.finish([before],ValueError('first pixel error'))
            self.assertFalse(caught.exception.receipt['identity']['after'][0]['available'])

    def test_real_verify_loop_and_late_producer_rejection(self):
        # Synthetic saved oracle solely for validator fault gates, no original
        # execution or producer. Its independent pin is explicit, not inferred.
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); original=root/'original_game';original.mkdir()
            (original/'FDOTHER.DAT').write_bytes(b'fake resource')
            producer=root/'producer';producer.write_bytes(b'not executable')
            source=root/'source.c';source.write_bytes(b'fixture source')
            expected=root/'expected.bin';actual=root/'actual.jsonl'
            pages=[];audits=[];mask=bytes(M.PAGE)
            with actual.open('w',encoding='utf8') as output:
                for row in self.cases:
                    before=M.before_page(row)
                    pages.append((before,before,mask,mask))
                    audits.append(dict(before_sha256=M.sha(before),after_sha256=M.sha(before),original_write_mask_sha256=M.sha(mask),original_destination_read_mask_sha256=M.sha(mask)))
                    output.write(json.dumps(dict(row,before_hex=before.hex(),after_hex=before.hex(),inputs_unchanged=True))+'\n')
            metadata=dict(schema='fd2-f4-page-writer-original-v1',cases=self.cases,audits=audits,identity=dict(unchanged=True,before=[],after=[]))
            encoded=json.dumps(metadata).encode('utf8')
            with expected.open('wb') as output:
                output.write(M.HEAD.pack(M.MAGIC,66,len(encoded)));output.write(encoded)
                for record in pages:
                    for page in record:output.write(page)
            args=SimpleNamespace(producer=producer,producer_source=[source],expected=expected,expected_sha256=M.sha(expected.read_bytes()),original=original,actual=actual)
            with mock.patch.object(M,'OTHER_SHA',M.sha(b'fake resource')),mock.patch.object(M,'luts',return_value=self.luts):
                result=M.verify_actual(args)
                self.assertTrue(result['passed']);self.assertTrue(result['identity']['unchanged'])
                self.assertEqual(result['full_pages_compared'],132)
                compare=M.compare_row
                def change_producer(row,spec,record):
                    compare(row,spec,record)
                    if spec['case']==65:producer.write_bytes(b'changed producer')
                with mock.patch.object(M,'compare_row',side_effect=change_producer),self.assertRaisesRegex(ValueError,'late SHA/size/mtime') as caught:
                    M.verify_actual(args)
                self.assertFalse(caught.exception.receipt['identity']['unchanged'])

    def test_success_report_uses_pinned_expected_digest(self):
        # This isolates the report/read contract; the seven tests above cover
        # artifact/pixel validation. A saved source is read once for pin/parse,
        # once for the required late guard, never a third time for the report.
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);original=root/'original_game';original.mkdir()
            (original/'FDOTHER.DAT').write_bytes(b'fake resource')
            producer=root/'producer';producer.write_bytes(b'not executable')
            source=root/'source.c';source.write_bytes(b'fixture source')
            expected=root/'expected.bin';expected.write_bytes(b'pinned fixture')
            actual=root/'actual.jsonl';actual.write_text('{}\n'*66,encoding='utf8')
            digest=M.sha(b'pinned fixture')
            args=SimpleNamespace(producer=producer,producer_source=[source],expected=expected,
                expected_sha256=digest,original=original,actual=actual)
            read_bytes=Path.read_bytes;expected_reads=[]
            def observed_read(path):
                if path.resolve()==expected.resolve():expected_reads.append(str(path))
                return read_bytes(path)
            with mock.patch.object(M,'OTHER_SHA',M.sha(b'fake resource')),\
                mock.patch.object(M,'luts',return_value=self.luts),\
                mock.patch.object(M,'artifact',return_value=(dict(cases=self.cases),[None]*66)),\
                mock.patch.object(M,'compare_row'),mock.patch.object(Path,'read_bytes',observed_read):
                result=M.verify_actual(args)
            self.assertTrue(result['passed'])
            self.assertEqual(result['expected_sha256'],digest)
            self.assertTrue(result['identity']['unchanged'])
            self.assertEqual(len(expected_reads),2)  # pin/parse plus late guard

if __name__=='__main__':unittest.main()
