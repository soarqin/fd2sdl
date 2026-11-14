"""355f0共享尾3不能借352ca的11；旧图原域不改，S5新目标须有独立门。"""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
import convert_stage_callbacks as converter
import stage_release_scope as release


class Callback51SourceTests(unittest.TestCase):
    def test_new_graph_uses_child2_then_source_story3_and_old_G_body_unchanged(self):
        self.assertNotIn(51,converter.EXISTING)
        self.assertEqual(converter.programs()[51],[('loot_child',[2,-1]),('story',[3])])
        document=json.loads((ROOT/'data/static/flows/common_event.json').read_bytes())
        old=next(f for f in document['flows'] if f['id']=='common.loot.child.2')
        self.assertEqual(converter.digest(converter.encode(old).encode()),
            'abedd1adae12ffcd5658775e1f9a5382ba6ee66a7bb5af6adb46c4722bbeff42')

    def test_refinement_requires_exact_target_and_actualC_current_graph_SHA(self):
        rows=[{'source_id':f'dispatch:event_callback:{i:03d}','target_va':hex(0x30000+i),
               'definition_id':f'old.{i}','disposition':'converted_candidate'} for i in range(90)]
        rows[51].update(target_va='0x355f0',definition_id='common.loot.child.2')
        decisions=[{'source_id':r['source_id'],'target_va':r['target_va'],'definition':r['definition_id'],
                    'disposition':'existing_definition_reference'} for r in rows]
        decisions[51].update(definition='common.callback.051',definition_sha256='sourceGraph',disposition='converted_candidate')
        proof={'schema':'fd2-s5-callback51-source-v1','source_id':rows[51]['source_id'],'target_va':'0x355f0',
            'original_CPU_executed':True,'actual_C':{'verified':True},'definition':'common.callback.051',
            'definition_sha256':'sourceGraph','expected_definition_sha256':'sourceGraph'}
        updated,changes=release.refine_callbacks(rows,{'decisions':decisions},None,proof)
        self.assertEqual(updated[rows[51]['source_id']]['definition_id'],'common.callback.051')
        self.assertEqual(changes[0]['stage'],'S5')
        self.assertEqual(rows[51]['definition_id'],'common.loot.child.2')
        for mutate in (lambda p:p.update(target_va='0x352ca'),lambda p:p.update(original_CPU_executed=False),
                       lambda p:p['actual_C'].update(verified=False),lambda p:p.update(definition_sha256='other'),
                       lambda p:p.update(expected_definition_sha256='other')):
            bad=copy.deepcopy(proof);mutate(bad)
            with self.subTest(proof=bad),self.assertRaisesRegex(ValueError,'callback51'):
                release.refine_callbacks(rows,{'decisions':decisions},None,bad)


if __name__=='__main__':unittest.main()
