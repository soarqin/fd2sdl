"""交叉SAV工具纯准备/错误门；mock收据不冒充实际运行，无GUI。"""
from pathlib import Path
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_cross_save_check as cross


class CrossSaveTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _,plain,_,after,_=cross.oracle.derive_fixture(ROOT/'original_game','p2-self-give')
        cls.plain=cross.oracle.write_slot(plain,after,0);cls.encoded=cross.oracle.crypt(cls.plain);cls.after=after

    def test_full_persistent_source_extraction_and_new_slot_allowlist(self):
        state=cross.state_from_slot(self.plain)
        self.assertEqual(state,self.after);self.assertEqual(len(state['roster']),2560)
        expected=cross.oracle.write_slot(self.plain,state,1);at=cross.oracle.SLOT_BASE;stride=cross.oracle.SLOT_STRIDE
        self.assertEqual(expected[at:at+stride],self.plain[at:at+stride])
        self.assertEqual(expected[:at],self.plain[:at])  # active/未知头全保留。
        self.assertEqual(expected[at+stride+2560+10:-4],self.plain[at+stride+2560+10:-4])
        for offset in (0x8a3,at,at+stride+2560+10,at+2*stride):
            data=bytearray(expected);data[offset]^=1;bad=cross.oracle.checksum(data)
            self.assertFalse(cross.oracle.check_save(self.plain,bad,state,1)['ok'])

    def test_actual_encoded_not_regenerated_fixture_is_preparation_input(self):
        data={'encoded':self.encoded,'plain':self.plain,'state':cross.state_from_slot(self.plain)}
        prepared=cross.prepare(ROOT/'original_game',data)
        self.assertIs(prepared['encoded'],self.encoded)
        self.assertEqual([step['save'] for step in prepared['steps'] if 'save' in step],[1])
        self.assertEqual(prepared['after'],prepared['before'])
        names=[step.get('name') for step in prepared['steps']]
        self.assertIn('title-source-slot0',names);self.assertIn('loaded-members',names)
        self.assertIn('loaded-first-member-inventory',names);self.assertIn('hotel-saved-slot1',names)
        self.assertEqual(prepared['expected_plain'],cross.oracle.write_slot(self.plain,self.after,1))

    def receipts(self,folder):
        report={'ok':True,'final_sources_unchanged':True,'display_requirement':{'released':True},
            'original_before':{},'inputs':{str(folder/name):'a'*64 for name in
            ('fd2sdl.exe','fd2_field_play_input_driver.exe','dosbox-x.exe')},'cases':[]}
        for ordinal,kind in enumerate(('original','sdl')):
            work=folder/kind;work.mkdir();measured=work/'measured.SAV';measured.write_bytes(self.encoded)
            live=work/'game/FD2.SAV' if kind=='original' else work/'FD2.SAV';live.parent.mkdir(exist_ok=True)
            live.write_bytes(self.encoded)
            case={'kind':kind,'ok':True,'business_sequence_completed':True,
                'guards':{'sources_unchanged':True,'save_matches_completed':True,'cleanup':True},
                'state_comparison':{'ok':True,'checked_container_bytes':cross.oracle.FILE_SIZE},
                'processes':[{'pid':123+ordinal,'cleaned':True,'forced_cleanup':False,'exit_code':0}],
                'completed_save':{'path':str(measured),'sha256':cross.sha(measured),
                    'mtime_ns':live.stat().st_mtime_ns,'slot':0},'checkpoints':[]}
            (work/'comparison.json').write_text(json.dumps(case),encoding='utf-8');report['cases'].append(case)
        path=folder/'comparison.json';path.write_text(json.dumps(report),encoding='utf-8');return path,report

    def test_receipt_roles_bound_to_distinct_producer_files_and_tampered_save_rejected(self):
        with tempfile.TemporaryDirectory(prefix='g3-cross-pure-') as tmp:
            path,report=self.receipts(Path(tmp))
            with patch.object(cross,'fingerprint',return_value={}):
                donors,sealed=cross.evidence(path,ROOT/'original_game')
                self.assertEqual(set(donors),{'original','sdl'})
                self.assertNotEqual(donors['original']['receipt']['measured_save'],donors['sdl']['receipt']['measured_save'])
                self.assertEqual(donors['original']['encoded'],self.encoded)
                self.assertGreaterEqual(len(sealed),7)
                target=Path(report['cases'][1]['completed_save']['path']);data=bytearray(target.read_bytes());data[10]^=1;target.write_bytes(data)
                with self.assertRaisesRegex(ValueError,'SHA'):cross.evidence(path,ROOT/'original_game')

    def test_failed_or_wrong_slot_producer_is_not_accepted_as_native_source(self):
        for kind in ('failed','wrong_slot','forced'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory(prefix='g3-cross-pure-') as tmp:
                path,report=self.receipts(Path(tmp))
                if kind=='failed':report['ok']=False
                elif kind=='wrong_slot':report['cases'][0]['completed_save']['slot']=1
                else:report['cases'][0]['processes'][0]['forced_cleanup']=True
                path.write_text(json.dumps(report),encoding='utf-8')
                with patch.object(cross,'fingerprint',return_value={}):
                    with self.assertRaises(ValueError):cross.evidence(path,ROOT/'original_game')

    def test_source_check_occurs_before_outer_display_context_restores(self):
        report={};events=[]
        with patch.object(cross,'unchanged',side_effect=lambda *a:events.append('source-check') or True):
            try:
                with cross.final_source_guard(report,{},None,None,None,None):
                    events.append('owned-cleaned');raise ValueError('intentional')
            except ValueError:events.append('outer-display-restore')
        self.assertEqual(events,['owned-cleaned','source-check','outer-display-restore'])
        self.assertTrue(report['sources_before_display_release'])


if __name__=='__main__':unittest.main()
