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
        self.assertEqual(row['decoded']['audit_classification'],'supported_definition')
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
        bound=self.by_id['dispatch:stage_runtime_result:002']
        candidate=self.by_id['dispatch:stage_runtime_result:023']
        self.assertEqual(existing['decoded']['target_va'],candidate['decoded']['target_va'])
        self.assertEqual(existing['disposition'],'confirmed')
        self.assertEqual(bound['decoded']['target_va'],existing['decoded']['target_va'])
        self.assertEqual(bound['disposition'],'confirmed')
        stage=json.loads((ROOT/'data/static/flows/stage.json').read_text())
        self.assertEqual(stage['bindings']['stage.result.002'],bound['definition'])
        # S5已正式绑定23；反例移除本槽绑定，不能继续借同target自动批准。
        self.assertEqual(candidate['disposition'],'confirmed')
        self.assertEqual(stage['bindings']['stage.result.023'],candidate['definition'])
        inventory=copy.deepcopy(self.inventory)
        binding=inventory['stages'][23]['dispatch']['result']
        binding.update(classification=ledger.audit.UNKNOWN,definition=None)
        unbound=ledger.normalize(inventory,self.sources,self.common)
        candidate=next(row for row in unbound['records'] if row['source_id']=='dispatch:stage_runtime_result:023')
        self.assertEqual(candidate['disposition'],'unresolved')
        self.assertTrue(candidate['blocker']['source_evidence'])
        self.assertFalse(self.ledger['claims']['same_target_approves_binding'])
        self.assertIn('reuse confirmed target evidence',candidate['blocker']['next_evidence'])
        self.assertNotIn('dispatch:stage_runtime_result:000',candidate['references'])
        self.assertIn('dispatch:stage_runtime_result:000',candidate['blocker']['source_evidence'])
        callback=self.by_id['dispatch:event_callback:000']
        self.assertEqual(callback['decoded']['existing_exact_bindings'][0]['runtime_id'],'stage.000.action.000')
        self.assertEqual(callback['disposition'],'confirmed')

    def test_current_consumers_refine_history_without_approving_runtime(self):
        current=self.ledger['current_consumer_summary']
        self.assertEqual(len(current['refined_source_ids']),34)
        self.assertEqual(len(current['current_unresolved_source_ids']),8)
        self.assertEqual(current['current_disposition_counts'],{'confirmed':69935,'excluded':2077,'rejected':450,'unresolved':8})
        for slot in range(28):
            row=self.by_id[f'dispatch:adjacent_magic:{slot:03d}']
            self.assertEqual(row['disposition'],'unresolved')
            consumer=row['current_consumer']
            self.assertEqual(consumer['status'],'existing_primitive_caller_domain')
            self.assertFalse(consumer['all_normal_paths_proven'])
            self.assertFalse(consumer['original_runtime_executed'])
        alias=self.by_id['dispatch:adjacent_magic:024']['current_consumer']
        self.assertEqual(alias['projection']['MAGIC_MAP_EFFECT_IDS'],16)
        self.assertEqual(alias['projection']['MAGIC_MAP_COST_IDS'],16)
        self.assertIn('player24 uses FULL',alias['caller_scope']['alias24'])
        for slot in (58,85,86,87,88,89):
            row=self.by_id[f'dispatch:event_callback:{slot:03d}']
            self.assertEqual(row['current_consumer']['status'],'source_only_definition')
            self.assertEqual(row['current_consumer']['incoming_sources'],[])
            self.assertEqual(row['current_consumer']['formal_bindings'],[])
        for identity in current['current_unresolved_source_ids']:
            consumer=self.by_id[identity]['current_consumer']
            self.assertTrue(consumer['next_evidence'])
            self.assertIn('reachability remains unresolved',consumer['exclusion_scope'])
        self.assertEqual(self.by_id['stage:29:turn:00000']['source']['raw_hex'],'ff5200')
        self.assertNotIn('stage:29:turn:00000',current['current_unresolved_source_ids'])

    def test_source_only_caller_or_changed_definition_is_not_auto_approved(self):
        row=self.by_id['dispatch:event_callback:058']
        caller={'source_id':'synthetic:caller','kind':'cell_action','disposition':'confirmed','references':[row['source_id']]}
        with self.assertRaisesRegex(ValueError,'acquired a caller'):
            ledger.current_consumer_overlay(self.inventory,self.sources,[copy.deepcopy(row),caller])
        flows,aliases,provenance=ledger.audit.load_definitions(ROOT/'data/static')
        for identity in ('common.cell.grant','common.callback.085','magic.effect.023'):
            bad=copy.deepcopy(flows);bad.pop(identity)
            with self.subTest(identity=identity),mock.patch.object(ledger.audit,'load_definitions',return_value=(bad,aliases,provenance)),self.assertRaisesRegex(ValueError,'lacks'):
                ledger.current_consumer_overlay(self.inventory,self.sources,copy.deepcopy([r for r in self.rows if r['kind']=='dispatch']))
        bad=copy.deepcopy(flows);bad['common.cell.grant']['nodes'][0]['args']=[1]
        with mock.patch.object(ledger.audit,'load_definitions',return_value=(bad,aliases,provenance)),self.assertRaisesRegex(ValueError,'reviewed full-source'):
            ledger.current_consumer_overlay(self.inventory,self.sources,[copy.deepcopy(row)])

    def test_current_consumer_summary_and_claim_tampering_fail(self):
        for name,value in [('current_disposition_counts',{}),('refined_source_ids',[]),('original_runtime_executed',True)]:
            changed={**self.ledger,'current_consumer_summary':{**self.ledger['current_consumer_summary'],name:value}}
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'current consumer'):
                ledger.validate_ledger(changed,self.sources)

    def test_excluded_current_rows_require_proof_and_exact_input_and_row_joins(self):
        import f1_consumer_exclusion as exclusion
        # 只测已验证proof返回值的admission合同；原来源由独立工具与CLI实跑验证。
        verified={'path':'fixture-proof','sha256':'verified-proof','excluded_source_ids':list(exclusion.EXCLUDED_IDS),
                  'qualified_scope':'fixture-normal-original-only'}
        proof_inputs={Path('fixture-source'):b'independent source snapshot'}
        document=copy.deepcopy(self.ledger)
        summary=document['current_consumer_summary'];summary['exclusion_proof']=verified
        summary['current_disposition_counts'].update(excluded=2085,unresolved=0)
        summary['current_unresolved_source_ids']=[]
        summary['refined_source_ids']+=list(exclusion.EXCLUDED_IDS)
        # 正规顺序须沿源账，不按拼接的proof清单重排。
        summary['refined_source_ids']=[r['source_id'] for r in document['records'] if r['source_id'] in summary['refined_source_ids']]
        document['summary']['current_consumer_disposition_counts']=dict(summary['current_disposition_counts'])
        document['inputs']['f1_exclusion_proof']=verified
        document['inputs']['f1_exclusion_inputs_sha256']={str(p):ledger.audit.digest(raw) for p,raw in proof_inputs.items()}
        for row in document['records']:
            if row['source_id'] in exclusion.EXCLUDED_IDS:
                row['current_consumer'].update(disposition='excluded',status='normal_original_consumer_excluded',
                    normal_source_consumer_exclusion_proven=True,exclusion_proof_sha256=verified['sha256'],
                    exclusion_scope=verified['qualified_scope'])
        with mock.patch.object(exclusion,'validate',return_value=(verified,proof_inputs)):
            ledger.validate_ledger(document,self.sources)
            def wrong_row(d):
                next(r for r in d['records'] if r['source_id']==exclusion.EXCLUDED_IDS[0])['current_consumer']['exclusion_proof_sha256']='other'
            def wrong_id(d):
                row=next(r for r in d['records'] if r['source_id']==exclusion.EXCLUDED_IDS[0])
                row['current_consumer'].update(disposition='unresolved',status='unresolved')
                d['current_consumer_summary']['current_disposition_counts'].update(excluded=2084,unresolved=1)
                d['summary']['current_consumer_disposition_counts'].update(excluded=2084,unresolved=1)
                d['current_consumer_summary']['current_unresolved_source_ids']=[exclusion.EXCLUDED_IDS[0]]
                d['current_consumer_summary']['refined_source_ids'].remove(exclusion.EXCLUDED_IDS[0])
            mutations=(lambda d:d.pop('current_consumer_summary'),
                lambda d:d['current_consumer_summary'].update(exclusion_proof=None),
                lambda d:d['inputs'].update(f1_exclusion_proof=None,f1_exclusion_inputs_sha256={}),
                lambda d:d['inputs'].update(f1_exclusion_inputs_sha256={}),wrong_row,wrong_id)
            for change in mutations:
                bad=copy.deepcopy(document);change(bad)
                with self.subTest(change=change),self.assertRaisesRegex(ValueError,'consumer exclusion'):
                    ledger.validate_ledger(bad,self.sources)
            bad=copy.deepcopy(document);bad['current_consumer_summary']['exclusion_proof']=None
            bad['inputs'].update(f1_exclusion_proof=None,f1_exclusion_inputs_sha256={})
            with self.assertRaisesRegex(ValueError,'explicit verified F1 proof'):
                ledger.validate_ledger(bad,self.sources)
        # 无current视图的旧S1格式继续可读，不会自动要求F1证明。
        old=copy.deepcopy(self.ledger);old.pop('current_consumer_summary')
        old['summary'].pop('current_consumer_disposition_counts')
        for row in old['records']:row.pop('current_consumer',None)
        ledger.validate_ledger(old,self.sources)

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
