import importlib.util
import io
import json
import os
from pathlib import Path
import queue
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

PATH=Path(__file__).resolve().parents[1]/'verify_f4_text_composition.py'
if not PATH.exists():PATH=Path(__file__).resolve().parent/'after/tools/verify_f4_text_composition.py'
spec=importlib.util.spec_from_file_location('f4_text_candidate',PATH)
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class OriginalCompositionTest(unittest.TestCase):
    def oracle(self):return object.__new__(module.Original)
    def test_cross_line_and_shadow_are_raw_owner_offsets(self):
        oracle=self.oracle();oracle.glyph=lambda _: [(0,0,205),(-1,1,74),(0,1,74)]
        before=module.seed(64000,320);page=bytearray(before);mask=bytearray(64000)
        oracle.letter(page,mask,0,319,320,201)
        expected=bytearray(before);expected[319]=201;expected[638]=expected[639]=76
        self.assertEqual(page,expected);self.assertEqual([i for i,v in enumerate(mask) if v],[319,638,639])
    def test_456_stride_is_not_a_320_projection(self):
        oracle=self.oracle();oracle.glyph=lambda _: [(15,15,205),(14,16,74),(15,16,74)]
        page=bytearray(153216);mask=bytearray(len(page));oracle.letter(page,mask,0,77*456+77,456)
        self.assertEqual(page[92*456+92],205)
        self.assertEqual(page[93*456+91:93*456+93],b'LL')
        self.assertEqual(sum(mask),3)
    def test_preclear_prefix_precedes_font_read(self):
        oracle=self.oracle();reads=[];oracle.glyph=lambda index:reads.append(index) or []
        page=bytearray([91])*64000;mask=bytearray(len(page))
        with self.assertRaisesRegex(ValueError,'owner'):oracle.letter(page,mask,99999,63999,320,205,74)
        self.assertEqual(page[-2:],bytes([91,74]));self.assertEqual(sum(mask),1);self.assertEqual(reads,[])
    def test_zero_literal_is_a_write_not_transparency(self):
        oracle=self.oracle();oracle.image=lambda _: (2,1,bytes([0,83]),bytes([1,0]))
        page=bytearray([19])*20;mask=bytearray(20);oracle.blit(page,mask,0,7,10)
        self.assertEqual(page[7:9],bytes([0,19]));self.assertEqual(sum(mask),1)
    def test_general_dialog_controls_cannot_enter_literal_fold(self):
        for tokens in ([1,-3,-1],[1,-4,-1],[1,-17,0,-1],[1],[1,-1,2,-1]):
            with self.subTest(tokens=tokens),self.assertRaises(ValueError):module.pure_tokens(tokens)
    def test_original_role_must_come_from_mask_and_not_palette(self):
        font=bytes(1824*32);row={'source_sha256':module.sha(bytes(32)),'abi_input_buffer_sha256':module.sha(font),'stride':320,'page_bytes':64000,'seed':83}
        before=module.seed(64000,320);page=bytearray(before);mask=bytearray(64000)
        page[1288]=205;mask[1288]=1
        self.assertEqual(module.validate_glyph(module.payload(page,mask),row,0,font),[(0,0,205)])
        page[1288]=99
        with self.assertRaisesRegex(ValueError,'role'):module.validate_glyph(module.payload(page,mask),row,0,font)
        page[1288]=205;page[0]^=1
        with self.assertRaisesRegex(ValueError,'unwritten'):module.validate_glyph(module.payload(page,mask),row,0,font)
    def test_glyph10_cannot_sneak_in_a_glyph(self):
        font=bytes(1824*32);row={'source_sha256':module.sha(bytes(32)),'abi_input_buffer_sha256':module.sha(font),'stride':320,'page_bytes':64000,'seed':83}
        page=module.seed(64000,320);mask=bytearray(64000);page[1288]=205;mask[1288]=1
        with self.assertRaisesRegex(ValueError,'glyph10'):module.validate_glyph(module.payload(page,mask),row,10,font)

class ProbeBoundaryTest(unittest.TestCase):
    def test_bad_cap_or_mode_never_creates_or_launches(self):
        with tempfile.TemporaryDirectory() as temp,patch.object(module.core.subprocess,'Popen') as launch:
            root=Path(temp)
            for mode,cap in [('--bad',64000),('--f4-text',0),('--f4-text',224001),('--f4-text',True),('--f4-text',None)]:
                with self.subTest(mode=mode,cap=cap),self.assertRaises(ValueError):
                    module.core.Probe(root/'none.exe',root,root,'probe',1,{},mode=mode,max_page_bytes=cap)
            launch.assert_not_called();self.assertEqual(list(root.iterdir()),[])
    def test_700_row_packet_requires_explicit_capacity(self):
        size=224000;count=2*size+1536;header={'schema':'fd2-f4-reader-v1','status':0,'payload_bytes':count}
        raw=json.dumps(header).encode()+b'\n'+bytes(count)
        for cap,valid in [(153216,False),(224000,True)]:
            probe=object.__new__(module.core.Probe);probe.max_page_bytes=cap;probe.expected_inputs={};probe.deferred_inputs=set();probe.replies=queue.Queue()
            probe.process=types.SimpleNamespace(stdout=io.BytesIO(raw));probe._read()
            self.assertEqual(probe.replies.get()[0],'inputs_bound');result=probe.replies.get()
            if valid:self.assertEqual((result[0],len(result[1])),(header,count))
            else:self.assertIsInstance(result,ValueError)
    def test_output_alias_into_original_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);original=root/'original_game';original.mkdir();alias=root/'alias'
            try:
                if os.name=='nt':
                    result=subprocess.run(['cmd','/c','mklink','/J',str(alias),str(original)],capture_output=True)
                    if result.returncode:self.skipTest('temporary junction unavailable')
                else:alias.symlink_to(original,target_is_directory=True)
                with self.assertRaisesRegex(ValueError,'original'):module.core.checked_output(root,alias/'new',root)
            finally:
                if alias.exists():os.rmdir(alias) if os.name=='nt' else alias.unlink()
    def test_missing_direct_sources_fails_before_any_binary(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);path=root/'producer.json';path.write_text(json.dumps({'schema':'fd2-f4-text-producer-v1','source_root':str(root),'sources':{}}))
            with self.assertRaisesRegex(ValueError,'direct source'):module.producer(path,root,{})
    def test_producer_source_cannot_escape_declared_root(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);path=root/'producer.json';sources={'../outside':'0'*64,**{x:'0'*64 for x in module.REQUIRED_SOURCES}}
            path.write_text(json.dumps({'schema':'fd2-f4-text-producer-v1','source_root':str(root),'sources':sources}))
            with self.assertRaisesRegex(ValueError,'escaped'):module.producer(path,root,{})
    def test_late_guard_preserves_primary_error(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);source=root/'input';source.write_bytes(b'A');guards={};module.snapshot(source,guards);source.write_bytes(b'B')
            report={'ok':False,'error':'first business failure'};module.core.finish_report(root,report,guards)
            self.assertEqual(report['error'],'first business failure');self.assertFalse(report['ok']);self.assertFalse(report['guards_unchanged'])
            self.assertEqual(report['secondary_errors'][0]['phase'],'late_guard')

if __name__=='__main__':unittest.main()
