"""正式entry29新代表的原资源／输入／WGC有限门；不启动GUI。"""
from contextlib import contextmanager
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch
from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_s5_entry29_check as runner
from native import s5_entry29_fixture as fixture


class Entry29Native(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/'build/s5').mkdir(parents=True,exist_ok=True)
        cls.p=runner.prepare(ROOT)

    def test_original_ABI_seven_calls_reuses_raw24_and_rejects_changed_complete_source(self):
        calls=self.p['manifest']['teleport_calls']
        self.assertEqual([(c['actor'],c['destination'])for c in calls],
            [(21,[21,5]),(22,[23,5]),(23,[20,5]),(24,[24,5]),(24,[22,18]),(25,[21,18]),(26,[23,18])])
        self.assertTrue(all(c['draw']==c['destination']for c in calls))
        bad={'raw':bytearray(self.p['data']['raw'])};bad['raw'][0x33ea4-1]^=1
        with self.assertRaises(ValueError):fixture.source_calls(bad,self.p['original'])
        bad={'raw':bytearray(self.p['data']['raw'])};bad['raw'][0x33140+1]=1
        with self.assertRaisesRegex(ValueError,'共享尾'):fixture.source_calls(bad,self.p['original'])

    def test_real_NoTown20_plus_group0_seven_final_source_writes_and_full_roster_tail(self):
        e=self.p['entry_expected'];meta=self.p['data']['stages'][29]['metadata']
        self.assertEqual(meta[:3],bytes((29,20,70)));self.assertEqual(len(e['records']),27)
        self.assertEqual(e['view'][fixture.state.META:fixture.state.META+10],bytes((1,27,29,16,17,22,23,6,6,31)))
        self.assertEqual(e['prepared']['roster'][88],9)
        self.assertEqual(e['prepared']['roster'][31*80:],self.p['manual_state']['roster'][31*80:])
        self.assertEqual([list(e['records'][i][:2])for i in range(21,27)],[[21,5],[23,5],[20,5],[22,18],[21,18],[23,18]])
        self.assertTrue(all(u[3]==0 for u in e['records']))
        self.assertEqual(e['script']['va'],'0x63327')
        self.assertEqual(meta[3:6],bytes((255,82,0)))
        self.assertEqual({k:len(v)for k,v in self.p['story_pages'].items()},{0:13,1:9,2:7})

    def test_script87_direction_only_does_not_move_deployments_or_invent_script86(self):
        e=self.p['entry_expected'];row=self.p['data']['stages'][29]
        self.assertEqual([list(u[:2])for u in e['story_records'][0][:20]],row['placement'][70:90])
        self.assertEqual([u[3]for u in e['story_records'][0][:20]],[2]*20)
        self.assertEqual([c['actor']for c in e['teleports']],[21,22,23,24,24,25,26])

    def test_complete_Save_checker_rejects_wrong_teleport_write_and_unused_roster_tail(self):
        p=self.p;e=p['entry_expected'];before=p['source'];metadata=p['data']['stages'][29]['metadata']
        after=bytearray(before);after[:len(metadata)]=metadata;after[0x8a3:0x12a3]=e['prepared']['roster']
        for i,unit in enumerate(e['records']):after[fixture.state.UNITS+80*i:fixture.state.UNITS+80*(i+1)]=unit
        after[fixture.state.META:fixture.state.META+10]=e['view'][fixture.state.META:fixture.state.META+10]
        after=fixture.state.checksum(after)
        def checked(candidate):return fixture.state.check_activity(before,candidate,e['view'],e['records'],e['masks'],metadata,
            kind='sdl',fresh_metadata=True,roster=e['prepared']['roster'])
        self.assertEqual(checked(after)['checked_bytes'],22987)
        for at in(fixture.state.UNITS+24*80,0x8a3+31*80):
            wrong=bytearray(after);wrong[at]^=1
            with self.assertRaises(AssertionError):checked(fixture.state.checksum(wrong))

    def frame(self,index,*,offset=0):
        call=self.p['entry_expected']['teleports'][index];sheet=runner.archive_entry((self.p['original']/'FDOTHER.DAT').read_bytes(),6)
        start,end=struct.unpack_from('<II',sheet,6+4*119);image=runner.decode_lmi(sheet[start:end])
        frame=Image.new('P',(320,200));frame.putpalette(self.p['assets']['palette'])
        cx,cy=call['camera'];x,y=call['draw'];frame.paste(image,(4+(x-cx)*24+offset,5+(y-cy)*24))
        return frame.convert('RGB')

    def test_seven_ordered_true_LMI_frames_return_evidence_dict_and_reject_shift_order_and_hash(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build/s5')as directory:
            rows=[]
            for index in range(7):
                p=Path(directory)/f'{index}.png';self.frame(index).save(p)
                rows.append({'index':index,'path':str(p),'sha256':fixture.state.digest(p.read_bytes())})
            for indices in((0,1,2,3),(4,5,6)):
                found=runner.teleport_gate(rows,self.p,indices)
                self.assertIsInstance(found,dict);self.assertEqual([r['identity']['caller']for r in found['matched']],list(indices))
                self.assertTrue(all(r['checked_pixels']>=24 for r in found['matched']))
            with self.assertRaises(AssertionError):runner.teleport_gate(list(reversed(rows)),self.p,(0,1,2,3))
            p=Path(rows[0]['path']);self.frame(0,offset=1).save(p)
            with self.assertRaisesRegex(AssertionError,'SHA'):runner.teleport_gate(rows,self.p,(0,))
            rows[0]['sha256']=fixture.state.digest(p.read_bytes())
            with self.assertRaises(AssertionError):runner.teleport_gate(rows[:1],self.p,(0,))

    def test_batch_retains_raw_evidence_and_helper_cleanup_even_if_gate_fails(self):
        @contextmanager
        def collected(capture,record,**options):
            self.assertEqual(options,{'interval_ms':16});record['continuous_wgc']={'frames':[],'complete_raw_collection':True}
            yield
            record['helper_cleaned']=True
        with tempfile.TemporaryDirectory(dir=ROOT/'build/s5')as directory:
            capture=Mock();capture.artifacts=Path(directory);record={}
            with patch.object(runner,'series',collected):
                with self.assertRaises(AssertionError):
                    with runner.observed_batch(capture,record,'first4',self.p,(0,1,2,3)):pass
            self.assertTrue(record['teleport_batches'][0]['helper_cleaned'])
            self.assertTrue(record['teleport_batches'][0]['continuous_wgc']['complete_raw_collection'])

    def test_formal_title_NoTown_input_uses_two_series_and_all29_ACK_without_API_fast_entry(self):
        p=self.p;capture=Mock();capture.artifacts=ROOT/'build/s5';names=[];batches=[]
        @contextmanager
        def observed(c,record,label,prepared,indices):batches.append((label,indices));yield
        def cp(name,predicate,**options):names.append(name);return None,None
        source=p['original']/'FD2.EXE'
        proof={'passed':True,'encoded_snapshot_sha256':fixture.state.digest(p['encoded']),
            'inputs_sha256':{str(source):fixture.state.digest(source.read_bytes())}}
        prepared={**p,'snapshot_import_preflight':proof}
        record={'kind':'sdl','title_menu_policy':{'menu_count':3}}
        with patch.object(runner.shared,'checkpoint',return_value=cp),patch.object(runner,'wait_title'),\
             patch.object(runner,'observed_batch',observed),patch.object(runner.shared,'save_activity')as save:
            runner.exercise(capture,prepared,None,None,record)
        self.assertEqual([c.args[0]for c in capture.key.call_args_list][:5],['DOWN','ENTER','ENTER','RIGHT','ENTER'])
        self.assertEqual([c.args[0]for c in capture.key.call_args_list].count('ENTER'),52)
        self.assertEqual(batches,[('entry29-first4',(0,1,2,3)),('entry29-last3',(4,5,6))])
        self.assertEqual(len(record['resource_acknowledgements']),29)
        self.assertEqual(save.call_count,1);self.assertFalse(record['source_family']['callback82_naturally_triggered'])
        self.assertLess(names.index('s5-29-story0-last'),names.index('s5-29-story1-first'))
        self.assertEqual(names[-1],'s5-29-browse')
        invalid={**prepared,'snapshot_import_preflight':{**proof,'encoded_snapshot_sha256':'wrong'}}
        capture.reset_mock()
        with self.assertRaises(ValueError):runner.exercise(capture,invalid,None,None,record)
        capture.key.assert_not_called()


if __name__=='__main__':unittest.main()
