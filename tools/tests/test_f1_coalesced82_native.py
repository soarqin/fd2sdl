"""F1容量代表的独立source/owner负例；不启动GUI或使用生产图作expected。"""
import copy
from contextlib import contextmanager
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch
from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_f1_coalesced82_check as runner
from native import f1_coalesced82_fixture as fixture
from native import s3_fixture as state


class Coalesced82(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.p=runner.prepare(ROOT)

    def after(self,kind='original'):
        p=self.p;expected=p['expected'][kind];before=p['source'];after=bytearray(before)
        for i,unit in enumerate(expected['records']):after[state.UNITS+i*80:state.UNITS+(i+1)*80]=unit
        after[state.META:state.META+10]=expected['view'][state.META:state.META+10]
        return state.checksum(after)

    def test_source27_and_only_declared_controlled_prefix(self):
        p=self.p;plain,evidence=fixture.origin(fixture.SOURCE/'s5-29-entry-active.SAV',fixture.SOURCE/'comparison.json')
        self.assertEqual(len(evidence),9);self.assertEqual(p['source'][state.META:state.META+3],bytes((1,27,29)))
        self.assertEqual(p['source'][3:6],bytes((2,82,0)));self.assertEqual(p['source'][fixture.STATE+16],2)
        self.assertEqual(p['source'][state.UNITS+27*80:fixture.STATE],plain[state.UNITS+27*80:fixture.STATE])
        self.assertFalse(p['manifest']['natural_kills_executed']);self.assertFalse(p['manifest']['runtime_alias_or_API_injection'])
        for actor in(23,24,25,26):
            unit=p['source'][state.UNITS+80*actor:state.UNITS+80*(actor+1)]
            self.assertTrue(unit[5]&1);self.assertEqual(unit[64:66],bytes(2))
        for i in(9,10):
            self.assertEqual(p['data']['stages'][29]['templates'][i][17],8)
            self.assertEqual(p['data']['stages'][29]['templates'][i][2],1)

    def test_other_source_or_SDL_report_is_rejected(self):
        with tempfile.TemporaryDirectory()as tmp:
            root=Path(tmp);save=root/'source.SAV';report=root/'comparison.json'
            save.write_bytes(b'not original');report.write_text('{"kind":"sdl","ok":true}')
            with self.assertRaisesRegex(ValueError,'同源原版'):fixture.origin(save,report)

    def test_missing_officer_callback_or_wrong_initial_schedule_is_not_repaired(self):
        plain,_=fixture.origin(fixture.SOURCE/'s5-29-entry-active.SAV',fixture.SOURCE/'comparison.json')
        original=copy.deepcopy(self.p['data'])
        original['stages'][29]['metadata']=bytes(plain[:len(original['stages'][29]['metadata'])])
        for offset in(3,state.UNITS+23*80+50,fixture.STATE+16,state.UNITS+20*80+5):
            bad=bytearray(plain);bad[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):fixture.inject(state.checksum(bad),original)

    def test_three_callers_and_constructor_do_not_promote_inactive_tail(self):
        e=self.p['expected']['original']
        self.assertEqual([c['actor']for c in e['teleports']],[22,29,30])
        self.assertEqual([len(c['before_records'])for c in e['teleports']],[27,29,29])
        self.assertEqual([c['destination']for c in e['teleports']],[[22,18],[21,18],[23,18]])
        self.assertEqual(len(e['records']),29);self.assertEqual([u[52]for u in e['records'][27:]],[8,8])
        self.assertEqual([u[61]for u in e['records'][27:]],[1,1])
        self.assertEqual(list(e['records'][22][:2]),[22,18]);self.assertEqual(e['records'][22][52]&15,0)
        self.assertEqual(len(self.p['story_pages']),2);self.assertTrue(all(len(a)==9 for a in self.p['teleport_anchors']))

    def test_whole_Save_keeps_loaded_owner_and_inactive_disk_tail(self):
        for kind in('original','sdl'):
            after=self.after(kind);view=self.p['expected'][kind]['view']
            result=fixture.check_activity(self.p,self.p['source'],after,view,kind)
            self.assertEqual(result['checked_bytes'],22987);self.assertTrue(result['inactive29_95_SAV_tail_preserved'])
            self.assertEqual(after[state.UNITS+29*80:fixture.STATE],self.p['source'][state.UNITS+29*80:fixture.STATE])

    def test_owned_hole_tail_AI_metadata_or_state_damage_is_rejected(self):
        view=self.p['expected']['original']['view']
        offsets=(state.UNITS+24*80+40,state.UNITS+29*80,state.UNITS+27*80+52,4,fixture.STATE+16)
        for offset in offsets:
            bad=bytearray(self.after());bad[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(AssertionError):
                fixture.check_activity(self.p,self.p['source'],state.checksum(bad),view,'original')

    def test_only_new_ctor_holes_are_explicit_exclusions(self):
        bad=bytearray(self.after());bad[state.UNITS+27*80+40]^=1
        result=fixture.check_activity(self.p,self.p['source'],state.checksum(bad),self.p['expected']['original']['view'],'original')
        self.assertTrue(any(r.get('raw')==27 and r.get('offset')==40 for r in result['fresh_exclusions']))
        self.assertTrue(all(r.get('raw',27)>=27 for r in result['fresh_exclusions']))

    def test_no_import_preflight_never_sends_the_first_window_key(self):
        capture=Mock()
        with self.assertRaisesRegex(ValueError,'快照预检'):runner.exercise(capture,self.p,None,None,{'kind':'original'})
        capture.key.assert_not_called()

    def test_complete_numbered_source_frames90_and91_are_both_valid_witnesses(self):
        # 原1a63d的esi+83和1a65f的esi>6，允许两张不同高度的完整资源。
        with tempfile.TemporaryDirectory()as tmp:
            for frame_id,pixels in((90,1785),(91,1615)):
                frame=Image.new('RGB',(320,200),(19,23,29))
                banner=runner.natural.round_banner(self.p['original'],self.p['assets'],2,frame_id=frame_id)
                frame.paste(banner.convert('RGB'),(120,84))
                path=Path(tmp)/f'round-{frame_id}.png';frame.save(path)
                batch={'continuous_wgc':{'frames':[{'path':str(path),'sha256':state.digest(path.read_bytes())}]}}
                found=runner.round_witness(self.p,batch,2)
                self.assertEqual(found['resource_frame'],frame_id)
                self.assertEqual(found['resource']['checked_pixels'],pixels)
                with self.assertRaisesRegex(AssertionError,'Round003'):runner.round_witness(self.p,batch,3)
                # 数字外的完整边框仍不可漏检。
                frame.putpixel((122,85),(255,0,255));frame.save(path)
                batch['continuous_wgc']['frames'][0]['sha256']=state.digest(path.read_bytes())
                with self.assertRaises(AssertionError):runner.round_witness(self.p,batch,2)
        with self.assertRaises(ValueError):runner.natural.round_banner(self.p['original'],self.p['assets'],2,frame_id=89)

    def test_protocol_keeps_slot2_and_has_two_ENDs_two_ACKs_two_Rounds(self):
        p={**self.p,'snapshot_import_preflight':{'passed':True,'inputs_sha256':{},
            'encoded_snapshot_sha256':state.digest(self.p['encoded'])}}
        capture=Mock();record={'kind':'original'};checkpoints=[]
        with tempfile.TemporaryDirectory()as tmp:
            capture.artifacts=Path(tmp)
            target=Path(tmp)/'measured.SAV';target.write_bytes(fixture.encode(self.after()))
            def cp(name,*args,**kwargs):checkpoints.append(name)
            def save(*args,**kwargs):
                if args[7]=='f1-82-count29-active':record['completed_save']={'snapshot':str(target)}
                if args[7]=='f1-82-first-END-count27':return fixture.first_end(p,'original')['view']
                return p['source']
            @contextmanager
            def batch(*args,**kwargs):
                self.assertEqual(args[-1],(0,1,2));self.assertEqual(kwargs,{'interval_ms':33})
                record['teleport_batches']=[{}];yield
            @contextmanager
            def first_batch(c,row,**kwargs):
                self.assertEqual(kwargs,{'interval_ms':33});yield
            with patch.object(runner.shared,'checkpoint',return_value=cp),patch.object(runner,'wait_title'),\
                 patch.object(runner.shared,'save_activity',side_effect=save)as saves,\
                 patch.object(runner.shared,'load_activity',return_value=p['source'])as load,\
                 patch.object(runner.protocol,'end_phase')as end,patch.object(runner.entry,'observed_batch',batch),\
                 patch.object(runner,'series',first_batch),\
                 patch.object(runner,'round_witness',side_effect=lambda p,b,t:{'turn':t}):
                runner.exercise(capture,p,None,None,record)
        self.assertEqual([c.args[0]for c in capture.key.call_args_list],['UP','ENTER','ENTER','ENTER'])
        self.assertEqual([c.args[7]for c in saves.call_args_list],['f1-82-preload-owned','f1-82-first-END-count27','f1-82-count29-active'])
        load.assert_called_once();self.assertEqual(end.call_count,2)
        self.assertEqual([c.args[2][state.META]for c in end.call_args_list],[1,2])
        self.assertEqual([r['turn']for r in record['native_round_banners']],[2,3])
        self.assertEqual([r['fragment']for r in record['resource_acknowledgements']],[4,4])
        self.assertTrue(record['capacity_domain']['fragment4_complete_ACK'])

    def test_first_END_is_no82_and_final_turn_is3_from_original_phase_order(self):
        first=fixture.first_end(self.p,'original');view=first['view']
        self.assertEqual(view[state.META:state.META+3],bytes((2,27,29)))
        self.assertEqual(view[3:6],bytes((2,82,0)));self.assertEqual(view[fixture.STATE+16],2)
        fixture.check_first_end(self.p,self.p['source'],view,view,'original')
        bad=bytearray(view);bad[state.META+1]=29
        with self.assertRaises(AssertionError):fixture.check_first_end(self.p,self.p['source'],state.checksum(bad),view,'original')
        self.assertEqual(self.p['expected']['original']['view'][state.META:state.META+3],bytes((3,29,29)))

    def test_first_END_HP0_flags_are_exact_one_and_not_an_exclusion(self):
        first=fixture.first_end(self.p,'original');view=first['view']
        for actor in(23,24,25,26):
            self.assertEqual(first['records'][actor][5],1)
            for bad_flags in(0,3,5,129):
                bad=bytearray(view);bad[state.UNITS+actor*80+5]=bad_flags
                with self.subTest(actor=actor,flags=bad_flags),self.assertRaises(AssertionError):
                    fixture.check_first_end(self.p,self.p['source'],state.checksum(bad),view,'original')
        for actor in(20,21,22):self.assertEqual(first['records'][actor][5],4)
        result=fixture.check_first_end(self.p,self.p['source'],view,view,'original')
        self.assertEqual(result['fresh_exclusions'],[])

    def test_rebind_story_and_teleport_use_backend_Save_view_records_and_roster(self):
        loaded=bytearray(fixture.first_end(self.p,'original')['view'])
        loaded[state.META+5:state.META+9]=bytes((21,22,5,5))
        loaded[0x8a3+31*80+19]^=0x55;loaded=state.checksum(loaded)
        p={**self.p,'expected':dict(self.p['expected'])};pages=[{'from':'current backend'}]
        with patch.object(runner.late,'pages_for',return_value=pages)as rebuild,\
             patch.object(runner.entry,'teleport_anchors',return_value=['fresh anchors'])as anchors:
            current=runner.rebind_current(p,loaded,'original')
        self.assertIs(rebuild.call_args.args[1],current['story_records'])
        self.assertEqual(rebuild.call_args.args[4],loaded[0x8a3:0x12a3])
        self.assertNotEqual(rebuild.call_args.args[4],self.p['source'][0x8a3:0x12a3])
        self.assertIs(anchors.call_args.args[1],current);self.assertIs(p['story_pages'],pages)

    def test_entry_uses_shared_strict_ON_policy_and_does_not_execute_by_default(self):
        prepared={**self.p,'source_evidence_paths':list(self.p['source_evidence_paths'])}
        with patch.object(runner,'prepare',return_value=prepared),patch.object(runner.late,'snapshot_preflight'),\
             patch.object(runner.host,'run_prepared',return_value={'ok':False,'status':'prepared','scenario':fixture.SCENARIO})as run:
            self.assertEqual(runner.main(['--root',str(ROOT),'--artifacts','build/f1/unused-pure-test']),0)
        self.assertTrue(run.call_args.kwargs['require_display_on']);self.assertFalse(run.call_args.args[0].run)


if __name__=='__main__':unittest.main()
