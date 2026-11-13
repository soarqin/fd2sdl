"""S1：全量来源范围／真实原字节／拒绝与夹具保护，不作玩法oracle。"""
import copy
import contextlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
import stage_source_ledger as ledger


class SourceLedgerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory,cls.common,cls.ledger,cls.fixtures=ledger.build(ROOT/'original_game')
        cls.sources=ledger.Sources(ROOT/'original_game')
        cls.rows=cls.ledger['records'];cls.by_id={r['source_id']:r for r in cls.rows}

    def test_scope_matches_independent_original_directories_and_headers(self):
        # 直接取原DAT头／WORD计数，不取生成器、旧audit标签或summary为expected。
        files=self.sources.entries
        self.assertEqual(len(files['FDFIELD.DAT']),99)
        self.assertEqual(len(files['FDSHAP.DAT']),66)
        self.assertEqual(len(files['FDTXT.DAT']),34)
        counts=self.ledger['summary']['record_kind_counts']
        self.assertEqual(counts['map_cell'],sum(struct.unpack_from('<H',raw)[0]*struct.unpack_from('<H',raw,2)[0] for _,raw in files['FDFIELD.DAT'][::3]))
        self.assertEqual(counts['template'],sum((len(raw)-131)//26 for _,raw in files['FDFIELD.DAT'][1::3]))
        self.assertEqual(counts['placement'],sum(int.from_bytes(raw[:2],'little') for _,raw in files['FDFIELD.DAT'][2::3]))
        self.assertEqual(counts['shape_frame'],sum(int.from_bytes(raw[4:6],'little') for _,raw in files['FDSHAP.DAT'][::2]))
        self.assertEqual(counts['text_fragment'],sum(int.from_bytes(raw[:2],'little')//2 for _,raw in files['FDTXT.DAT']))
        self.assertEqual(self.ledger['summary']['different_dispatch_targets'],193)
        self.assertEqual(sum(self.ledger['summary']['disposition_counts'].values()),len(self.rows))

    def test_real_mode203_and_story_resource_reference(self):
        row=self.by_id['stage:14:drop:00033']
        raw=self.sources.entries['FDFIELD.DAT'][43][1][131+33*26+22:131+33*26+25]
        self.assertEqual(raw,bytes([203,0,0]))
        self.assertEqual(row['decoded']['caller_semantics']['semantic_kind'],'default_noop')
        self.assertEqual(row['decoded']['legacy_classification'],'unmapped_semantics')
        self.assertEqual(row['decoded']['audit_classification'],'decoded_unopened')
        self.assertEqual(row['disposition'],'confirmed') # caller rule，非运行支持
        self.assertIn('text:003:00009',self.by_id['stage:02:drop:00000']['references'])

    def test_default_turn255_and_unused_lookup_have_different_destinations(self):
        turns=[r for r in self.rows if r['kind']=='turn' and r['decoded']['default_fill']]
        self.assertEqual(len(turns),450)
        for row in turns:
            self.assertEqual(row['source']['raw_hex'],'ffff00')
            self.assertEqual(row['disposition'],'rejected')
            self.assertTrue(row['blocker']['next_evidence'])
        self.assertEqual(self.by_id['stage:00:lookup:00001']['disposition'],'excluded')

    def test_raw_template_tail_and_same_index_low_byte_are_retained(self):
        tails=[r for r in self.rows if r['kind']=='template' and not r['decoded']['effective_template']]
        self.assertEqual(len(tails),12)
        self.assertTrue(all(r['disposition']=='excluded' and r['source']['length']==26 for r in tails))
        templates=[r for r in self.rows if r['kind']=='template' and r['decoded']['effective_template']]
        self.assertEqual(len(templates),1887)
        for row in templates:
            st=row['stage'];i=row['decoded']['index']
            self.assertEqual(row['references'],[f'stage:{st:02d}:placement:{i:05d}'])

    def test_target_dedup_does_not_approve_other_binding(self):
        existing=self.by_id['dispatch:stage_runtime_result:000']
        candidate=self.by_id['dispatch:stage_runtime_result:002']
        self.assertEqual(existing['decoded']['target_va'],candidate['decoded']['target_va'])
        self.assertEqual(existing['disposition'],'confirmed')
        self.assertEqual(candidate['disposition'],'unresolved')
        self.assertTrue(candidate['blocker']['source_evidence'])
        self.assertFalse(self.ledger['claims']['same_target_approves_binding'])
        self.assertIn('reuse confirmed target evidence',candidate['blocker']['next_evidence'])
        self.assertNotIn('dispatch:stage_runtime_result:000',candidate['references'])
        self.assertIn('dispatch:stage_runtime_result:000',candidate['blocker']['source_evidence'])
        callback=self.by_id['dispatch:event_callback:000']
        self.assertEqual(callback['decoded']['existing_exact_bindings'][0]['runtime_id'],'stage.000.action.000')
        self.assertEqual(callback['disposition'],'confirmed')

    def test_all_references_fixtures_and_semantic_joins_close(self):
        ids=set(self.by_id)
        self.assertEqual({c['source_id'] for c in self.fixtures['cases']},ids)
        self.assertTrue(set(self.ledger['common_source_join'].values())<=ids)
        self.assertEqual(len(self.ledger['common_source_join']),1497)
        for row in self.rows:
            self.assertTrue(set(row['references'])<=ids)
            if row['blocker']:
                self.assertTrue(row['blocker']['source_evidence'])
                self.assertIn('semantic_families',row['blocker'])
        for values in self.ledger['reviewed_semantic_family_index'].values():self.assertTrue(set(values)<=ids)

    def test_movement_overlaps_and_unconsumed_four_bytes_are_not_lost(self):
        summary=self.ledger['summary']
        self.assertEqual((summary['movement_counted_bytes_sum'],summary['movement_counted_bytes_union'],summary['movement_unconsumed_bytes']),(2896,2894,4))
        gap=self.by_id['movement:unconsumed:000']
        self.assertEqual((gap['source']['canonical_va'],gap['source']['length'],gap['disposition']),('0x63221',4,'excluded'))
        self.assertEqual(self.by_id['movement:stream:024']['decoded']['candidate_relation'],'crosses_next_target')
        self.assertTrue(any(r['kind']=='movement_pair' and r['decoded']['actor_raw_index']==83 for r in self.rows))
        self.assertTrue(any(r['kind']=='movement_pair' and r['decoded']['direction5_preserves_cache'] for r in self.rows))

    def test_deployment_capacity_is_source_role_not_claimed_runtime_consumption(self):
        placements=[r for r in self.rows if r['kind']=='placement' and r['decoded']['source_role']=='declared_deployment_capacity']
        self.assertEqual(len(placements),435)
        self.assertFalse(any(r['decoded']['runtime_consumption_claimed'] for r in placements))
        for st in range(33):
            actual=[r['decoded']['deployment_ordinal'] for r in placements if r['stage']==st]
            count=self.sources.entries['FDFIELD.DAT'][st*3+1][1][1]
            self.assertEqual(actual,list(range(count)))

    def test_duplicate_id_wrong_order_missing_reference_and_unknown_cfg_edge_fail(self):
        row=copy.deepcopy(self.rows[0]);other=copy.deepcopy(row);other['ordinal']=1
        with self.assertRaisesRegex(ValueError,'duplicate'):ledger.validate_records([row,other],self.sources)
        for key,value,pattern in [('ordinal',1,'order'),('references',['missing'],'reference')]:
            row=copy.deepcopy(self.rows[0]);row[key]=value
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,pattern):ledger.validate_records([row],self.sources)
        row=copy.deepcopy(self.rows[0]);row['control_flow']['ordered_successors']=['nearby_call']
        with self.assertRaisesRegex(ValueError,'CFG'):ledger.validate_records([row],self.sources)

    def test_same_bytes_wrong_fixed_slot_is_rejected(self):
        row=copy.deepcopy(self.by_id['stage:00:turn:00004'])
        nextrow=self.by_id['stage:00:turn:00005']
        self.assertEqual(row['source']['raw_hex'],nextrow['source']['raw_hex'])
        row['source']=copy.deepcopy(nextrow['source'])
        with self.assertRaisesRegex(ValueError,'coordinate'):ledger.validate_identity(row,self.sources)

    def test_bad_offset_hash_raw_and_entry_are_rejected(self):
        row=copy.deepcopy(self.rows[0])
        for key,value,pattern in [('file_offset',-1,'offset'),('file_sha256','0'*64,'digest'),('read_sha256','0'*64,'digest'),('raw_hex','00','raw')]:
            changed=copy.deepcopy(row);changed['source'][key]=value
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,pattern):ledger.validate_records([changed],self.sources)
        row=copy.deepcopy(self.by_id['stage:00:turn:00000']);row['source']['entry']=10000
        with self.assertRaisesRegex(ValueError,'entry'):self.sources.read(row['source'])

    def test_header_count_and_pointer_target_tampering_fail(self):
        cases=[('stage:00:map_header:00000','width'),('stage:00:metadata_header:00000','effective_count'),
               ('stage:00:placement_header:00000','count'),('dispatch:event_callback:000','target_va'),('movement:pointer:000','target_va')]
        for identity,key in cases:
            row=copy.deepcopy(self.by_id[identity]);raw=self.sources.read(row['source']);row['decoded'][key]='0x10000' if key=='target_va' else -1
            with self.subTest(identity=identity),self.assertRaisesRegex(ValueError,'decoded'):ledger.validate_decoded(row,raw,self.sources)

    def test_schema_claims_missing_records_and_fixtures_fail(self):
        for key,value in [('schema','future-schema'),('schema_version',True)]:
            changed={**self.ledger,key:value}
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'schema'):ledger.validate_ledger(changed,self.sources)
        changed={**self.ledger,'claims':{**self.ledger['claims'],'new_gameplay_enabled':True}}
        with self.assertRaisesRegex(ValueError,'claims'):ledger.validate_ledger(changed,self.sources)
        changed={**self.ledger,'records':self.rows[:-1]}
        with self.assertRaises(ValueError):ledger.validate_ledger(changed,self.sources)
        changed={**self.fixtures,'cases':self.fixtures['cases'][:-1]}
        with self.assertRaisesRegex(ValueError,'fixture'):ledger.validate_fixtures(changed,self.ledger)

    def test_output_protects_existing_inputs_sources_and_build_symlinks(self):
        (ROOT/'build/s1').mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT/'build/s1') as tmp:
            base=Path(tmp);original=base/'original';original.mkdir()
            sentinel=original/'FD2.EXE';sentinel.write_bytes(b'unchanged evidence')
            for output in [base,original/'new-output',ROOT/'data/static/new',ROOT/'docs/new']:
                with self.subTest(output=output),self.assertRaises(ValueError):ledger.validate_output(output,original=original)
            # Windows本项目original_game为真实Junction；其它平台同样先resolve来源。
            with self.assertRaisesRegex(ValueError,'original'):
                ledger.validate_output(ROOT/'original_game'/'new-output',original=ROOT/'original_game')
            self.assertEqual(sentinel.read_bytes(),b'unchanged evidence')
            self.assertFalse((original/'new-output').exists())

    def test_missing_and_wrong_original_sources_fail_before_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            original=Path(tmp)
            with self.assertRaises(FileNotFoundError):ledger.Sources(original)
            for name in ledger.audit.SOURCE_HASHES:(original/name).write_bytes(b'wrong identity')
            with self.assertRaisesRegex(ValueError,'identity'):ledger.Sources(original)

    def test_cli_output_guard_runs_before_extraction(self):
        with mock.patch.object(sys,'argv',['stage_source_ledger.py','--output-dir',str(ROOT/'data/static/bad')]),mock.patch.object(ledger,'build') as build,contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
            ledger.main()
        build.assert_not_called()

    def test_manifest_is_required_and_bad_digest_refuses_reuse(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)
            with self.assertRaises(FileNotFoundError):ledger.verify_bundle(path,ROOT/'original_game')
            (path/'manifest.json').write_text(json.dumps({'schema':ledger.SCHEMA,'files':{name:'bad' for name in ['inventory.json','common-scope.json','ledger.json','fixtures.json']}}))
            (path/'inventory.json').write_text('{}')
            with self.assertRaisesRegex(ValueError,'digest'):ledger.verify_bundle(path,ROOT/'original_game')


if __name__=='__main__':unittest.main()
