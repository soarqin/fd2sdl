"""F1排除只凭显式有源bundle；合成负例不签原CPU或正常完整玩法。"""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
import f1_consumer_exclusion as exclusion


class ConsumerExclusionTest(unittest.TestCase):
    def fixture(self):
        source={'fixture':'independent-small-source'}
        entries=[(0,b'') for _ in range(99)]
        for stage in (1,31):
            metadata=bytearray(131+26*15)
            metadata[0x53+12*3:0x53+13*3]=bytes.fromhex('020000')
            if stage==31:
                for i,raw in enumerate(('030001','040100','050200','060301')):
                    metadata[3+i*3:6+i*3]=bytes.fromhex(raw)
                for i,raw in ((9,'020400'),(14,'020500')):
                    metadata[131+i*26+22:131+i*26+25]=bytes.fromhex(raw)
            entries[stage*3]=(0,struct.pack('<HHI',1,1,0))
            entries[stage*3+1]=(0,bytes(metadata))
        cpu={'schema':'fd2-f1-boundary-original-cpu-v1','ok':True,
             'source_sha256':exclusion.audit.digest(json.dumps(source,sort_keys=True).encode()),
             'unused_records':{'full_transitive_consumer_and_map_writer_proof':True,
                'opening_root':{'stage_writes':[32,31,0],'return_stage':0,'battle_consumers_in_original_root':[]},
                'stage31_turn_and_drop_sources':[s for s in exclusion.EXCLUDED_IDS if ':turn:' in s or ':drop:' in s],
                'action12':[{'source_id':f'stage:{stage:02}:cell_action:00012','action_bytes':'020000',
                             'all_map_cells_with_low5_equal12':0,'dynamic_map_writer_closure_proven':True}for stage in (1,31)]}}
        return source,cpu,entries

    def test_map_slot_and_original_opening_contract_match(self):
        exclusion.verify_contract(*self.fixture())

    def test_source_join_writer_and_opening_consumer_changes_fail(self):
        source,cpu,entries=self.fixture()
        mutations=(lambda p:p.update(source_sha256='0'*64),
            lambda p:p['unused_records'].update(full_transitive_consumer_and_map_writer_proof=False),
            lambda p:p['unused_records']['action12'][0].update(dynamic_map_writer_closure_proven=False),
            lambda p:p['unused_records']['opening_root'].update(stage_writes=[32,31]),
            lambda p:p['unused_records']['opening_root'].update(return_stage=31),
            lambda p:p['unused_records']['opening_root'].update(battle_consumers_in_original_root=[0x1a813]),
            lambda p:p['unused_records']['stage31_turn_and_drop_sources'].pop())
        for change in mutations:
            bad=copy.deepcopy(cpu);change(bad)
            with self.subTest(change=change),self.assertRaises(ValueError):
                exclusion.verify_contract(source,bad,entries)

    def test_map_low5_or_turn_drop_bytes_cannot_be_relabelled_unused(self):
        source,cpu,entries=self.fixture()
        for stage,entry,offset,value in ((1,0,6,12),(31,0,6,12),(1,1,0x53+12*3,0),
                (31,1,3,255),(31,1,131+9*26+23,7)):
            bad=copy.deepcopy(entries);raw=bytearray(bad[stage*3+entry][1]);raw[offset]=value
            bad[stage*3+entry]=(0,bytes(raw))
            with self.subTest(stage=stage,offset=offset),self.assertRaises(ValueError):
                exclusion.verify_contract(source,cpu,bad)

    def test_bundle_schema_and_scope_cannot_claim_full_or_new_bindings(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'proof.json'
            for doc in ({'schema':'other'}, {'schema':exclusion.SCHEMA,'excluded_source_ids':[]},
                    {'schema':exclusion.SCHEMA,'excluded_source_ids':list(exclusion.EXCLUDED_IDS),
                     'claims':{'new_original_native':False,'all_normal_semantic_complete':True,
                               'F1_full_gate_passed':False,'new_runtime_bindings':False}}):
                path.write_text(json.dumps(doc),encoding='utf-8')
                with self.subTest(doc=doc),self.assertRaisesRegex(ValueError,'schema/scope'):
                    exclusion.validate(path,None)

    def test_changed_bundle_source_cannot_resign_its_own_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'source.json';source.write_text('{}',encoding='utf-8')
            path=Path(directory)/'proof.json'
            doc={'schema':exclusion.SCHEMA,'excluded_source_ids':list(exclusion.EXCLUDED_IDS),
                 'claims':{key:False for key in ('new_original_native','all_normal_semantic_complete','F1_full_gate_passed','new_runtime_bindings')},
                 'source':{'path':str(source),'sha256':exclusion.audit.digest(source.read_bytes())}}
            path.write_text(json.dumps(doc),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'frozen.*identity'):
                exclusion.validate(path,None)


if __name__=='__main__':unittest.main()
