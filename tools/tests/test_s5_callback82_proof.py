"""82分层收据的来源/定义/输入/实际C拒绝门；合成收据不冒称执行C。"""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
import s5_callback82_proof as gate
import verify_stage_release_traces as traces


class Callback82ProofTests(unittest.TestCase):
    def setUp(self):
        self.image=bytes(0x60000);self.path=Path('unit/layered.json')
        self.machine_path=Path('unit/source-C.json');self.readset=Path('unit/domain.json')
        self.play=Path('unit/play.exe');self.projection=Path('unit/projection.exe')
        self.common_path=gate.callbacks.ROOT/'data/static/flows/common_event.json'
        self.stage_path=gate.callbacks.ROOT/'data/static/flows/stage.json'
        self.cmake_path=gate.callbacks.ROOT/'src/CMakeLists.txt'
        self.scope_path=gate.callbacks.ROOT/'src/tests/field_s5_callback82_scope_checks.c'
        programs=gate.source.caller_programs(self.image)
        self.common={'flows':[gate.source.compile_program('common.callback.082',programs['common.callback.082'])],
            'bindings':{'stage.029.action.082':'common.callback.082'}}
        self.stage={'flows':[gate.source.teleport_helper()]}
        self.ledger={'records':[{'source_id':'dispatch:event_callback:082',
            'decoded':{'target_va':'0x362e8'},'source':{'canonical_va':'0x51cd9','raw_hex':'e8620200'}},
            {'source_id':'stage:29:turn:00000','references':['dispatch:event_callback:082']}]}
        self.decision={'slot':82,'source_id':'dispatch:event_callback:082','target_va':'0x362e8',
            'source_pointer':self.ledger['records'][0]['source'],'caller_sources':['stage:29:turn:00000'],
            'disposition':'blocked','definition':None,'blocker':{'family':'generic_ACK_not_supported'}}
        self.machine={'schema':'fd2-s5-teleport-full-source-C-v1','original_CPU_executed':True,
            'actual_C_executed':True,'C_consumer':{'vectors':8,'ordered_prefixes':408},
            'inputs_sha256':{str(self.readset):gate.callbacks.digest(b'readset')},
            'caller_traces':[{'state16':s,'state32_unchanged':True,
                'actual_AI_raw52_and_display_writes_verified':s is not None}for s in (None,0,1,2,3,4)]}
        self.proof={'schema':'fd2-s5-callback82-layered-C18-v1','formal_binding':'stage.029.action.082',
            'source_id':'dispatch:event_callback:082','target_va':'0x362e8','common_alias':'common.callback.082',
            'shared_definition':'stage.source.teleport','passed':True,'shipping_binding_approved':False,
            'definitions':[],'original_source_caller':{'proof':str(self.machine_path),
                'original_CPU_executed':True,'readset_sha256':gate.callbacks.digest(b'readset'),
                'complete_362e8_state16':[0,1,2,3,4],'complete_33f78_traces':1,
                'AI_raw52_display_and_state32_verified':True},
            'teleport_module':{'proof':str(self.machine_path),'actual_C_module_executed':True,
                'controlled_argument_states':8,'ordered_fullpage_VGA_xy_prefixes':408},
            'actual_Play_C_Scene':{'actual_C_executed':True,'states':[0,4],'scenarios':['a','b','HOST','error'],
                'build_target':'fd2_field_s5_callback82_play_test',
                'receipt':{'executable':str(self.play),'sha256':gate.callbacks.digest(b'play'),
                    'exit_code':0,'stdout':gate.PLAY_PASS+'\n','stderr':''}},
            'projection_C':{'actual_C_executed':True,'state16':[0,1,2,3,4],
                'receipt':{'executable':str(self.projection),'sha256':gate.callbacks.digest(b'projection'),
                    'exit_code':0,'stdout':gate.PROJECTION_PASS+'\n','stderr':''}}}

    def run_gate(self, mutate=lambda p:None, machine_mutate=lambda m:None,
                 common_mutate=lambda c:None, decision_mutate=lambda d:None, raw_mutate=lambda m:None):
        proof=copy.deepcopy(self.proof);common=copy.deepcopy(self.common);machine=copy.deepcopy(self.machine)
        decision=copy.deepcopy(self.decision);common_mutate(common);decision_mutate(decision);machine_mutate(machine)
        data={self.machine_path:json.dumps(machine).encode(),self.readset:b'readset',self.play:b'play',
            self.projection:b'projection',self.common_path:json.dumps(common).encode(),
            self.stage_path:json.dumps(self.stage).encode()}
        for name in ('field_s5_callback82_play_test.c','field_s5_callback82_test.c'):
            data[gate.callbacks.ROOT/'src/tests'/name]=b'synthetic protocol fixture, no actual C execution'
        data[self.scope_path]=b'synthetic delegated Scene HOST/display fixture, no actual C execution'
        data[self.cmake_path]=(b'add_executable(fd2_field_s5_callback82_play_test '
            b'tests/field_s5_callback82_play_test.c tests/field_s5_callback82_scope_checks.c '
            b'$<TARGET_OBJECTS:fd2sdl_core>)\n')
        proof['definitions']=[{'domain':domain,'id':flow['id'],'node_count':len(flow['nodes']),
            'definition_sha256':gate.callbacks.digest(json.dumps(flow,sort_keys=True).encode())}
            for domain,document in [('common_event',common),('stage',self.stage)] for flow in document['flows']]
        proof['inputs_sha256']={str(path):gate.callbacks.digest(raw)for path,raw in data.items()if path!=self.readset}
        for name in ('original_source_caller','teleport_module'):
            proof[name]['proof_sha256']=gate.callbacks.digest(data[self.machine_path])
        mutate(proof);data[self.path]=json.dumps(proof).encode();raw_mutate(data)
        real_read=Path.read_bytes
        def read(path):return data[path] if path in data else real_read(path)
        with patch.object(Path,'read_bytes',read),patch.object(gate.callbacks,'IMAGE_SHA',gate.callbacks.digest(self.image)),\
                patch.object(gate,'READSET_SHA256',gate.callbacks.digest(b'readset')):
            return gate.validate(self.path,common,self.stage,decision,self.ledger,self.image)

    def test_all_layers_join_explicit_binding_without_generic_vectors(self):
        summary,inputs=self.run_gate()
        self.assertEqual(summary['generic_ACK_vectors_contributed'],0)
        self.assertEqual(summary['actual_Play_states'],[0,4]);self.assertFalse(summary['native_GUI_claimed'])
        self.assertIn(self.machine_path,inputs)
        with self.assertRaisesRegex(ValueError,'pure ACK'):
            list(traces.bound_callbacks(self.common,[self.decision]))
        result=list(traces.bound_callbacks(self.common,[self.decision],summary))
        self.assertEqual(result[0][2]['disposition'],'separately_verified')
        for binding in ('stage.028.action.082','stage.030.action.082'):
            with self.subTest(binding=binding),self.assertRaises(ValueError):
                list(traces.bound_callbacks({'bindings':{binding:'common.callback.082'}},[self.decision],summary))

    def test_wrong_identity_scope_booleans_or_missing_layer_rejected(self):
        mutations=[lambda p:p.update(schema='fd2-s5-callback82-layered-C14-v1'),
            lambda p:p.update(target_va='0x33f78'),lambda p:p.update(formal_binding='stage.028.action.082'),
            lambda p:p.update(shipping_binding_approved=True),lambda p:p.update(inputs_sha256={}),
            lambda p:p['original_source_caller'].update(complete_33f78_traces=True),
            lambda p:p['original_source_caller'].update(complete_362e8_state16=[False,1,2,3,4]),
            lambda p:p['teleport_module'].update(actual_C_module_executed=False),
            lambda p:p['actual_Play_C_Scene'].update(states=[0,1,2,3,4]),
            lambda p:p['actual_Play_C_Scene']['receipt'].update(exit_code=False),
            lambda p:p['actual_Play_C_Scene']['receipt'].update(stdout='another exit0 probe PASS\n'),
            lambda p:p['projection_C']['receipt'].update(stdout='another exit0 probe PASS\n'),
            lambda p:p['inputs_sha256'].pop(str(gate.callbacks.ROOT/'src/tests/field_s5_callback82_play_test.c')),
            lambda p:p['projection_C'].update(actual_C_executed=False)]
        for mutate in mutations:
            with self.subTest(mutate=mutate),self.assertRaises(ValueError):self.run_gate(mutate)

    def test_joint_formal_and_proof_wrong_same_ID_graph_still_rejected(self):
        def change(c):c['flows'][0]['nodes'][0]['args']=[999]
        with self.assertRaisesRegex(ValueError,'cold definition'):
            self.run_gate(common_mutate=change)
        with self.assertRaisesRegex(ValueError,'caller'):
            self.run_gate(decision_mutate=lambda d:d.update(caller_sources=['stage:28:turn:00000']))

    def test_nested_flags_and_changed_executable_do_not_inherit_top_pass(self):
        for mutate in (lambda m:m.update(actual_C_executed=False),
                lambda m:m['caller_traces'].pop(),lambda m:m['C_consumer'].update(ordered_prefixes=407)):
            with self.subTest(mutate=mutate),self.assertRaises(ValueError):self.run_gate(machine_mutate=mutate)
        with self.assertRaisesRegex(ValueError,'input changed'):
            self.run_gate(raw_mutate=lambda d:d.update({self.play:b'different executable'}))

    def test_delegated_Scene_TU_CMake_and_exact_target_are_mandatory(self):
        mutations=[lambda p:p['inputs_sha256'].pop(str(self.scope_path)),
            lambda p:p['inputs_sha256'].pop(str(self.cmake_path)),
            lambda p:p['inputs_sha256'].update({str(self.scope_path):'0'*64}),
            lambda p:p['inputs_sha256'].update({str(self.cmake_path):'0'*64}),
            lambda p:p['actual_Play_C_Scene'].pop('build_target'),
            lambda p:p['actual_Play_C_Scene'].update(build_target='another_exit0_target')]
        for index,mutate in enumerate(mutations):
            with self.subTest(case=index),self.assertRaises(ValueError):self.run_gate(mutate)

    def test_CMake_recipe_cannot_jointly_remove_or_move_delegated_TU(self):
        original=(b'add_executable(fd2_field_s5_callback82_play_test '
            b'tests/field_s5_callback82_play_test.c tests/field_s5_callback82_scope_checks.c '
            b'$<TARGET_OBJECTS:fd2sdl_core>)\n')
        changed=[original.replace(b'tests/field_s5_callback82_scope_checks.c ',b''),
            original.replace(b'fd2_field_s5_callback82_play_test ',b'another_target '),
            original.replace(b'fd2_field_s5_callback82_play_test ',b'FD2_FIELD_S5_CALLBACK82_PLAY_TEST '),
            original+original, b'# '+original, b'#[[\n'+original+b']]\n']
        for raw in changed:
            with self.subTest(recipe=raw),self.assertRaises(ValueError):
                self.run_gate(mutate=lambda p:p['inputs_sha256'].update(
                    {str(self.cmake_path):gate.callbacks.digest(raw)}),
                    raw_mutate=lambda d:d.update({self.cmake_path:raw}))


if __name__=='__main__':unittest.main()
