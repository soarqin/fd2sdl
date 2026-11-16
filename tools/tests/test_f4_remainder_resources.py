"""Finite remainder generator/verifier contracts; no Unicorn/C/GUI/IDA/build."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
import zlib
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import f4_original_pixels as kernel
import verify_f4_remainder_resources as tool
import generate_f4_resource_pixels as generator

class Contracts(unittest.TestCase):
    def test_prepare_typed_domains_and_unpromoted_hotel_tail(self):
        self.assertEqual([tool.prepare_format(13,x) for x in (0,3,10,11,14,16,17)], [0,2,2,1,1,1,1])
        for x in (1,2,15,18,22,23,27,28,-1):self.assertIsNone(tool.prepare_format(13,x))
        self.assertEqual([tool.prepare_format(12,x) for x in (0,1,2,3,15,16,23,28)],[0,1,4,2,3,1,0,0])
        self.assertIsNone(tool.prepare_format(14,18));self.assertIsNone(tool.prepare_format(14,22))
        self.assertIsNone(tool.prepare_format(7,0));self.assertIsNone(tool.prepare_format(12,29))

    def test_live_fixed_resource_inventory_and_order(self):
        # Optional real input is read-only; this test produces no pixels or oracle.
        path=Path.cwd()/'original_game/FDOTHER.DAT'
        if not path.exists():self.skipTest('local read-only resources not supplied')
        raw=path.read_bytes();self.assertEqual(tool.core.sha(raw),tool.OTHER_SHA)
        rows=tool.collect(raw);requests=tool.cases(rows)
        self.assertEqual(len(rows),1256);self.assertEqual(len(requests),2093)
        self.assertEqual(sum(r['request'] is not None for r in rows),1151)
        self.assertEqual(requests[0]['request'],['battle','command',0,0,0])
        self.assertEqual(requests[-1]['request'],['battle','otherfx',79,1,1])
        self.assertEqual(len({tuple(c['request']) for c in requests}),2093)

    def test_count_cannot_be_replaced_by_partial_coverage(self):
        with self.assertRaises(ValueError):tool.cases([])

    def fixture_manifest(self):
        image=bytes(0x64000)
        manifest={'schema':'fd2-f4-pixel-kernel-identities-v1','image_sha256':kernel.sha(image),
            'functions':[{'va':0x10000,'chunks':[{'start':0x10000,'end':0x10002,'sha256':kernel.sha(image[0x10000:0x10002])}],
                'instructions':[{'ea':0x10000,'size':1,'disasm':'nop'},{'ea':0x10001,'size':1,'disasm':'retn'}]}]}
        return image,manifest

    def validate_fixture(self,image,manifest):
        raw=json.dumps(manifest).encode()
        with patch.object(kernel,'IMAGE_SHA',kernel.sha(image)),patch.object(kernel,'MANIFEST_SHA',kernel.sha(raw)):
            return kernel.validate_manifest(image,raw)

    def test_manifest_complete_owner_positive(self):
        image,manifest=self.fixture_manifest();self.assertEqual(list(self.validate_fixture(image,manifest)),[0x10000])

    def test_manifest_missing_tail_rejected(self):
        image,manifest=self.fixture_manifest();manifest['functions'][0]['instructions'].pop()
        with self.assertRaisesRegex(ValueError,'coverage'):self.validate_fixture(image,manifest)

    def test_manifest_duplicate_head_rejected(self):
        image,manifest=self.fixture_manifest();manifest['functions'][0]['instructions'].append(manifest['functions'][0]['instructions'][0])
        with self.assertRaisesRegex(ValueError,'duplicate'):self.validate_fixture(image,manifest)

    def test_manifest_chunk_byte_change_rejected(self):
        image,manifest=self.fixture_manifest();image=image[:0x10001]+b'X'+image[0x10002:]
        manifest['image_sha256']=kernel.sha(image) # isolate the per-chunk gate
        with self.assertRaisesRegex(ValueError,'chunk identity'):self.validate_fixture(image,manifest)

    def test_manifest_duplicate_owner_rejected(self):
        image,manifest=self.fixture_manifest();manifest['functions']*=2
        with self.assertRaisesRegex(ValueError,'duplicate'):self.validate_fixture(image,manifest)

    def test_raw_zero_and_raw_copy_have_distinct_original_calls(self):
        row={'family':'command_raw','codec':'raw_zero','width':1,'height':1}
        zero=tool.original_parameters({'row':row,'request':['battle','command',0,0,0]},b'\1\0\1\0\0')
        row={'family':'prepare_mixed','codec':'raw_copy','width':1,'height':1}
        opaque=tool.original_parameters({'row':row,'request':['prepare','prepare',12,3,0]},b'\1\0\1\0\0')
        self.assertEqual(zero[4],0x4ed34);self.assertFalse(zero[-1]);self.assertEqual(opaque[4],0x4ed0b);self.assertTrue(opaque[-1])

    def test_map_decode_copy_vs_full_page_transparency(self):
        row={'family':'map_lmi','codec':'lmi','width':1,'height':1}
        source=b'\1\0\1\0\0'
        copy=tool.original_parameters({'row':row,'request':['battle','maplmi',6,0,0]},source)
        draw=tool.original_parameters({'row':row,'request':['battle','maplmi',6,0,1]},source)
        self.assertEqual((copy[0],copy[1],copy[4],copy[-1]),(1,1,0x4ebff,True))
        self.assertEqual((draw[0],draw[1],draw[4],draw[-1]),(456,336,0x4ebab,False))

    def expected_fixture(self,path):
        source=b'\1\0\1\0\0';raw=b'\0'+bytes([165])*768+b'\1'+bytes(768)
        payload=path/'payload.zbin';payload.write_bytes(zlib.compress(raw))
        row={'id':'synthetic','source_sha256':tool.core.sha(source),'family':'prepare_mixed','codec':'raw_copy',
            'width':1,'height':1,'start':0,'end':5};request=['prepare','prepare',12,3,0]
        item={'index':0,'asset':'synthetic','request':request,'source_sha256':row['source_sha256'],
            'entry':0x4ed0b,'owners':[0x4ed0b],'args':[kernel.PAGE,kernel.INPUT,1],
            'copy_mask_normalized':True,'original_pixel_addresses_written':1,
            'original_payload':{'path':payload.name,'sha256':tool.core.sha(payload.read_bytes())},
            'comparison_payload':{'path':payload.name,'sha256':tool.core.sha(payload.read_bytes())},
            'payload_bytes':len(raw),'stride':1,'height':1}
        report={'schema':'fd2-f4-remainder-original-v1','ok':True,'guards_unchanged':True,'original_cases':2093,'original_cases_executed':2093,'inherited_original_cases':0,'actual_C_cases':0,'cases':[item]}
        reportpath=path/'report.json';reportpath.write_text(json.dumps(report))
        return reportpath,report,[{'request':request,'row':row}]

    def test_retained_positive_then_payload_tamper_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp);reportpath,report,wanted=self.expected_fixture(path)
            with patch.object(tool,'collect',return_value=[]),patch.object(tool,'cases',return_value=wanted):
                result=tool.read_expected(reportpath,tool.core.sha(reportpath.read_bytes()),{},b'\1\0\1\0\0')
                self.assertEqual(result[0]['packed'],(path/'payload.zbin').read_bytes())
                (path/'payload.zbin').write_bytes(b'changed')
                with self.assertRaises(ValueError):tool.read_expected(reportpath,tool.core.sha(reportpath.read_bytes()),{},b'\1\0\1\0\0')

    def test_retained_changed_request_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp);reportpath,report,wanted=self.expected_fixture(path)
            report['cases'][0]['request']=['battle','command',1,0,0];reportpath.write_text(json.dumps(report))
            with patch.object(tool,'collect',return_value=[]),patch.object(tool,'cases',return_value=wanted):
                with self.assertRaisesRegex(ValueError,'input/request'):tool.read_expected(reportpath,tool.core.sha(reportpath.read_bytes()),{},b'')

    def test_producer_incomplete_roles_rejected_before_process(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp);r=path/'producer.json';r.write_text(json.dumps({'schema':'fd2-f4-remainder-producer-v1','files':[]}))
            args=types.SimpleNamespace(battle=path/'B.exe',prepare=path/'P.exe',producer_receipt=r,expected_sha='a'*64)
            with self.assertRaisesRegex(ValueError,'closure required'):tool.actual_inputs(args,path,{})

    def test_ui137_has_source_RLE_reader_and_single_fixed_copy_request(self):
        path=Path.cwd()/'original_game/FDOTHER.DAT'
        if not path.exists():self.skipTest('local read-only resource not supplied')
        rows,work=tool.prescribe(path.read_bytes(),True)
        self.assertEqual(len(rows),1);self.assertEqual(len(work),1)
        self.assertEqual(work[0]['request'],['prepare','prepare',5,137,0])
        self.assertEqual((rows[0]['width'],rows[0]['height'],rows[0]['codec']),(86,86,'rle'))

    def test_fresh_base_catalog_executes_all_ANI_without_private_expected(self):
        root=Path.cwd();paths={name:root/'original_game'/name for name in tool.core.RESOURCES}
        if not all(p.exists() for p in paths.values()):self.skipTest('local read-only resources not supplied')
        resources={name:p.read_bytes() for name,p in paths.items()}
        rows,blobs,counts,empty=generator.catalog_base(resources)
        self.assertEqual(len(rows),14711);self.assertEqual(len(blobs),14711);self.assertEqual(len(empty),144)
        work=generator.expand_base(rows);self.assertEqual(len(work),18759)
        self.assertEqual(sum(r['family']=='ani' for r,v in work),289)
        self.assertTrue(all(r.get('inherited_original_hashes') is None for r,v in work if r['family']=='ani'))

if __name__=='__main__':unittest.main()
