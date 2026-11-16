"""Pure source-model and verifier fault contracts, no C/original execution."""
import argparse, importlib.util, json, sys, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
PATH=Path(__file__).resolve().parents[1]/'verify_f4_scene_fx_parameters.py'
SPEC=importlib.util.spec_from_file_location('finite_fx',PATH)
M=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(M)

class GeometryContracts(unittest.TestCase):
    def test_source_strict_y_and_half_open_x(self):
        page=bytearray(M.PAGE);lut=bytes((i+1)&255 for i in range(256))
        M.circle(page,lut,10,10,2,0,192)
        self.assertEqual(page[M.VIEW+10*456+7: M.VIEW+10*456+14],bytes([1]*6+[0]))
        self.assertEqual(page[M.VIEW+8*456+10],0)
        self.assertEqual(page[M.VIEW+12*456+10],0)
    def test_original_first_row_is_not_silently_zero_clipped(self):
        page=bytearray(M.PAGE);lut=bytes((i+1)&255 for i in range(256))
        M.circle(page,lut,12,-8,2,-8,192)
        self.assertEqual(page[M.VIEW-8*456+12],1)
        # This is a source leaf contract, not a claimed formal negative centre.
    def test_top18_and_commit_not_projected(self):
        lut=[bytes(range(256))]*10;images={i:(1,1,b'\0')for i in range(114,125)}
        row=M.teleport_expected((12,12,11,11,4,0,0),lut,images)
        self.assertEqual(row['top'],1)
        self.assertEqual([r['index']for r in row['frames']if r['tag']=='wipe'],list(range(18)))
        before=next(r for r in row['frames']if r['tag']=='contract'and r['index']==0)
        after=next(r for r in row['frames']if r['tag']=='commit')
        self.assertEqual(after['vga_sha256'],before['vga_sha256'])
        self.assertEqual((after['x'],after['y']),(12,12))
    def test_fixed_new_domains(self):
        self.assertEqual(len(M.TRANSITION_NEW),4);self.assertEqual(len(M.TELEPORT_NEW),3)
        self.assertEqual(M.TRANSITION_NEW[2],(6,3,10,10,0))
        self.assertEqual(M.TELEPORT_NEW[2],(255,255,13,12,6,1,0))
        for p in M.TELEPORT_NEW:self.assertGreaterEqual(24*p[5]+15,0)
    def test_duplicate_keys_rejected(self):
        with self.assertRaises(ValueError):M.strict_json('{"x":1,"x":2}')

class VerifyContracts(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        def file(name,raw):
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw);return p
        self.file=file
        self.image=file('image',b'im');self.ts=file('ts',b'{}');self.ps=file('ps',b'{}')
        self.tr=file('tr',b'{}');self.pr=file('pr',b'{}')
        file('original/FD2.EXE',b'exe');file('original/FDOTHER.DAT',b'resource')
        self.producer=file('probe.exe',b'not executable, never run')
        self.sources=[file(n,b'source')for n in ['scene_transition_source_test.c','scene_transition.c','scene_transition.h','s5_effect_fixture.h','field_game.c','field_visual.c','field_page.c']]
        page=bytes([1])*M.PAGE;vga=bytes([2])*64000
        self.expected={'terrain_calls':1,'uploads':9,'cues':0,'page_creates':1,'page_destroys':1,'logic_unchanged':True,
            'frames':[{'page_sha256':M.sha(page),'vga_sha256':M.sha(vga)}]}
        self.actual={'terrain_calls':1,'uploads':9,'cues':0,'page_creates':1,'page_destroys':1,'logic_unchanged':True,
            'frames':[{'page_hex':page.hex(),'vga_hex':vga.hex()}]}
        self.output=file('actual.jsonl',(json.dumps(self.actual)+'\n').encode()*4)
        self.args=argparse.Namespace(image=self.image,transition_source=self.ts,teleport_source=self.ps,
            retained_transition=self.tr,retained_teleport=self.pr,original=self.root/'original',transition_actual=self.output,
            transition_producer=self.producer,teleport_actual=None,teleport_producer=None,producer_source=self.sources)
        for obj,name,value in [(M.leaf,'IMAGE_SHA',M.sha(b'im')),(M.leaf,'EXE_SHA',M.sha(b'exe')),(M.leaf,'OTHER_SHA',M.sha(b'resource')),
            (M,'TRANSITION_SOURCE_SHA',M.sha(b'{}')),(M,'TELEPORT_SOURCE_SHA',M.sha(b'{}')),(M,'TRANSITION_SHA',M.sha(b'{}')),(M,'TELEPORT_SHA',M.sha(b'{}'))]:
            p=patch.object(obj,name,value);p.start();self.addCleanup(p.stop)
        for name,value in [('banks',([],{})),('retained_qualification',{'retained_original_runs':20})]:
            p=patch.object(M,name,return_value=value);p.start();self.addCleanup(p.stop)
        p=patch.object(M.leaf,'strict_function');p.start();self.addCleanup(p.stop)
        self.expected_patch=patch.object(M,'transition_expected',return_value=self.expected);self.expected_mock=self.expected_patch.start();self.addCleanup(self.expected_patch.stop)
    def test_real_verify_loop_accepts_only_complete_expected_rows(self):
        r=M.verify(self.args);self.assertEqual(r['actual_C_cases'],{'transition':4});self.assertFalse(r['qualification_only']);self.assertTrue(r['guards_unchanged'])
    def test_complete_page_or_VGA_byte_damage_rejected(self):
        for owner in ('page_hex','vga_hex'):
            row=json.loads(json.dumps(self.actual));raw=bytearray.fromhex(row['frames'][0][owner]);raw[-1]^=1;row['frames'][0][owner]=raw.hex()
            self.output.write_text((json.dumps(row)+'\n')*4)
            with self.assertRaisesRegex(ValueError,'complete pixels differ'):M.verify(self.args)
    def test_extra_missing_and_duplicate_rows_rejected(self):
        for count in(0,3,5):
            self.output.write_text((json.dumps(self.actual)+'\n')*count)
            with self.assertRaisesRegex(ValueError,'denominator'):M.verify(self.args)
        row=json.loads(json.dumps(self.actual));row['frames'].append(row['frames'][0]);self.output.write_text((json.dumps(row)+'\n')*4)
        with self.assertRaisesRegex(ValueError,'frame count'):M.verify(self.args)
    def test_producer_and_direct_sources_required(self):
        self.args.transition_producer=None
        with self.assertRaisesRegex(ValueError,'exact producer'):M.verify(self.args)
        self.args.transition_producer=self.producer;self.args.producer_source=self.sources[:-1]
        with self.assertRaisesRegex(ValueError,'missing direct producer'):M.verify(self.args)
    def test_late_source_guard_and_primary_error_survive(self):
        for damage in(False,True):
            self.producer.write_bytes(b'fresh')
            row=json.loads(json.dumps(self.actual))
            if damage:row['frames'][0]['page_hex']='00'+row['frames'][0]['page_hex'][2:]
            self.output.write_text((json.dumps(row)+'\n')*4)
            def change(*a):self.producer.write_bytes(b'changed');return self.expected
            self.expected_mock.side_effect=change
            with self.assertRaises(ValueError)as caught:M.verify(self.args)
            receipt=caught.exception.receipt
            self.assertTrue(receipt['late_errors'])
            self.assertIn('complete pixels differ'if damage else 'late source/binary',receipt['primary_error'])
    def test_retained_mismatch_cannot_become_qualification(self):
        with patch.object(M,'retained_qualification',side_effect=ValueError('retained pixels differ')):
            with self.assertRaisesRegex(ValueError,'retained pixels differ'):M.verify(self.args)

if __name__=='__main__':unittest.main()
