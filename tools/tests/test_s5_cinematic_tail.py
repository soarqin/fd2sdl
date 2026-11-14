"""240fa真实库存前缀/append-before-sync及完整准备交接；纯测不签native。"""
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
import copy
import hashlib
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from native import s5_cinematic_fixture as fixture
from native import s3_fixture as state
import run_s5_cinematic_check as host
import run_s3_stage_check as shared
from run_g4_runtime_check import encode


class SourceTail(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p=host.prepare(ROOT,fixture.SCENARIOS[1])

    def test_true_input_uses_real_prefix_not_six_items_beyond_count(self):
        p=self.p
        self.assertEqual(p['manifest']['rare_items']['raw0_to5_first_hidden_slots'],[3,3,5,2,4,5])
        self.assertEqual(fixture.six_presence(p['records']),6)
        old=[bytearray(unit) for unit in p['records']]
        for i in range(6):
            slot=fixture.item_prefix_slot(old[i],209+i);self.assertIsNotNone(slot)
            old[i][10+2*slot:12+2*slot]=bytes((128,255));old[i][24:26]=bytes((0,209+i))
        self.assertEqual(sum(any(not u[10+2*k]&128 and u[11+2*k]==item for k in range(8))
            for u in old[:16] for item in range(209,215)),6)
        self.assertEqual(fixture.six_presence(old),0)

    def test_append24_23_then_sync_matches_full_original_CPU_not_generated_graph(self):
        # 独立完整原240fa/112a5/1145a/11506 CPU tail02；固定late源身份另在derive核。
        for kind in ('original','sdl'):
            prepared=fixture.victory20_settlement(self.p,kind)
            self.assertEqual((prepared['stage'],prepared['count']),(21,18))
            self.assertEqual(hashlib.sha256(prepared['roster']).hexdigest(),
                '26d21b5bce4e8c370aa17ef3c33e1480a38ccd6ba34daa45b5e07ff67057f55f')
            self.assertEqual(prepared['roster'][18*80:],self.p['source'][0x8a3+18*80:0x12a3])
            self.assertEqual([prepared['roster'][raw*80+8] for raw in (16,17)],[24,23])
        altered=copy.deepcopy(self.p);source=bytearray(altered['source'])
        # 若真实CONTINUE的raw class不同，仅原端按源重建；SDL不能顺带写+2。
        source[state.UNITS+2]=91;altered['source']=state.checksum(source)
        original=fixture.victory20_settlement(altered,'original')
        sdl=fixture.victory20_settlement(altered,'sdl')
        self.assertEqual(original['roster'][2],0);self.assertEqual(sdl['roster'][2],91)

    def test_preflight_requires_actual_Town21_type1_and_four_slot_scope(self):
        proof=self.p['manifest']['preflight']
        self.assertEqual((proof['prepared_stage'],proof['prepared_roster_count'],proof['Town_prepare_type']),(21,18,1))
        self.assertIn('four full manual slots',proof['scope_end'])
        self.assertEqual([a['identity']['part'] for a in self.p['sheet_anchors']],[1,35,68,69,85,100])

    def test_story10_ACKs_are_outside_sheet_series_before_four_slots(self):
        pages=[dict(fragment=f,token_offset=n,portrait_id=7) for f,n in
            ((5,0),(7,0),(8,0),(9,0),(9,10),(10,0),(10,20))]
        p={'source':bytearray(23000),'assets':{},'titles':{3:[0,1,2]},'original':ROOT/'original_game',
           'pages':pages,'sheet_anchors':[]}
        record={'kind':'original','continuous_wgc':{'frames':[]}};events=[];active=[False]
        capture=SimpleNamespace(last=None,wait=Mock(),key=lambda key:events.append(('key',key,active[0])))
        @contextmanager
        def series(*args,**kwargs):
            self.assertEqual(kwargs,{'interval_ms':16});active[0]=True;events.append(('begin',))
            try:yield
            finally:active[0]=False;events.append(('end',))
        def cp(label,predicate,**kwargs):events.append(('cp',label,active[0]))
        prepared=dict(stage=21,count=18,roster=bytes(2560),resource=1,options=bytes(4))
        def slots(*args,**kwargs):
            self.assertEqual(args[-1],prepared);self.assertEqual(kwargs,{'label_prefix':'s5-stage21'})
            events.append(('slots',))
        with patch.object(host.shared,'checkpoint',return_value=cp),patch.object(host,'wait_title'),\
             patch.object(host.natural,'actor_browse_anywhere',return_value=(p['source'],{})),\
             patch.object(host.natural,'selection_summary'),patch.object(host.natural,'range_probe'),\
             patch.object(host,'series',series),patch.object(host,'sheet_gate',return_value={'ok':True}),\
             patch.object(host.fixture,'victory20_settlement',return_value=prepared),\
             patch.object(host.shared,'manual_slots',side_effect=slots):
            host.victory20(capture,p,None,None,record)
        self.assertEqual([e[1] for e in events if e[0]=='key' and e[2]],['ENTER'])
        self.assertIn(('cp','s5-20-story10-first',True),events)
        self.assertIn(('cp','s5-story-10-0',False),events);self.assertIn(('cp','s5-story-10-20',False),events)
        self.assertEqual(record['resource_acknowledgements'],pages)
        self.assertEqual(record['resource_observations'],[pages[-2]])
        self.assertEqual(events[-1],('slots',))
        self.assertTrue(record['source_family']['complete_story10_ACK'])


class ManualSlotLabels(unittest.TestCase):
    def test_old_S3_default_and_S5_stage21_labels_keep_exact_same_keys_and_container_gate(self):
        p=fixture.derive(ROOT/'original_game',fixture.SCENARIOS[1])
        for prefix,stage in ((None,3),('s5-stage21',21)):
            with self.subTest(prefix=prefix),tempfile.TemporaryDirectory() as directory:
                work=Path(directory);save=work/'FD2.SAV';save.write_bytes(p['encoded'])
                prepared=fixture.victory20_settlement(p,'original');prepared['stage']=stage
                guard=[save.read_bytes(),save.stat().st_mtime_ns];record={};labels=[];keys=[];index=[0]
                capture=SimpleNamespace(artifacts=work,key=keys.append)
                def wait(*args):
                    before=state.decode_save(save)
                    after=state.write_slot(before,prepared,index[0]);index[0]+=1;save.write_bytes(encode(after));return after
                with patch.object(shared,'checkpoint',return_value=lambda label,*a,**k:labels.append(label)),\
                     patch.object(shared,'preparation_town',return_value={'assets':{}}),\
                     patch.object(shared,'notice_resources',return_value={}),patch.object(shared,'wait_measured_save',side_effect=wait):
                    shared.manual_slots(capture,{'original':work,'assets':{}},save,guard,record,prepared,label_prefix=prefix)
                self.assertEqual(keys,['ENTER','RIGHT','ENTER','ENTER','ENTER','DOWN','ENTER','ENTER',
                    'DOWN','ENTER','ENTER','DOWN','ENTER','ENTER'])
                self.assertEqual(len(record['manual_saves']),4)
                stage_prefix='s3-stage3' if prefix is None else prefix
                common_prefix='s3' if prefix is None else prefix
                self.assertEqual(labels[:3],[stage_prefix+'-prepare',stage_prefix+'-hotel',stage_prefix+'-save-command'])
                self.assertEqual([Path(r['snapshot']).name for r in record['manual_saves']],
                    [f'{stage_prefix}-manual{i}.SAV' for i in range(4)])
                self.assertTrue(all(r['measurement']['stage']==stage and r['measurement']['checked_bytes']==22987 for r in record['manual_saves']))
                self.assertEqual(labels[-1],common_prefix+'-manual-saved-3')

    def test_artifact_prefix_cannot_escape_its_directory(self):
        capture=SimpleNamespace(key=Mock())
        self.assertRaises(ValueError,shared.manual_slots,capture,{},None,None,{}, {},label_prefix='../stage21')
        capture.key.assert_not_called()


if __name__=='__main__':unittest.main()
