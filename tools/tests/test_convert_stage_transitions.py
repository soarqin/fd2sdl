"""S2入场／胜利：完整入口、有限常量规则、独立原码轨迹及拒绝边界。"""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/static_data'))
import convert_stage_transitions as converter
import generate


def straight_fixture():
    entry = 0x11000
    codes = [b'\x6a\x02', b'\x6a\x01', b'\xe8' + struct.pack('<i', 0x135DD - (entry + 9)), b'\x83\xc4\x08', b'\xc3']
    disasm = ['push 2', 'push 1', 'call pan', 'add esp,8', 'retn']
    instructions, address = {}, entry
    for raw, text in zip(codes, disasm):
        instructions[address] = {'ea': address, 'size': len(raw), 'raw_hex': raw.hex(), 'disasm': text}
        address += len(raw)
    refs = {entry + 4: [{'to': 0x135DD, 'type': 'CALL_NEAR'}]}
    return entry, instructions, refs


class TransitionConversionTest(unittest.TestCase):
    def test_straight_shared_path_preserves_call_order_and_full_values(self):
        entry, instructions, refs = straight_fixture()
        path, calls, blocker = converter.linear_path(entry, instructions, refs)
        self.assertIsNone(blocker)
        self.assertEqual(calls, [(entry + 4, 0x135DD)])
        events, blocker = converter.static_events(path, instructions, refs, 3)
        self.assertIsNone(blocker)
        self.assertEqual([(e['op'], e['args']) for e in events], [('pan', [1, 2])])

    def test_branch_indirect_call_loop_and_missing_tail_are_not_flattened(self):
        entry, instructions, refs = straight_fixture()
        for text, xrefs, family in [('jne condition', [], 'conditional_transition'),
                                   ('call eax', [], 'indirect_call'),
                                   ('jmp back', [{'to': entry, 'type': 'JUMP_NEAR'}], 'bounded_loop_or_private_cfg'),
                                   ('jmp missing', [{'to': 0x12000, 'type': 'JUMP_NEAR'}], 'external_shared_tail')]:
            changed = copy.deepcopy(instructions)
            changed[entry]['disasm'] = text
            with self.subTest(text=text):
                self.assertEqual(converter.linear_path(entry, changed, {entry: xrefs})[2]['family'], family)

    def test_unconfirmed_operand_and_private_write_remain_blocked(self):
        entry, instructions, refs = straight_fixture()
        changed = copy.deepcopy(instructions)
        changed[entry]['raw_hex'] = '50'
        changed[entry]['size'] = 1
        events, blocker = converter.static_events(list(changed), changed, refs, 3)
        self.assertIsNone(events)
        self.assertEqual(blocker['family'], 'dynamic_call_operand')
        changed[entry]['disasm'] = 'mov byte ptr [eax+26h],64h'
        changed[entry]['raw_hex'] = 'c6802600000064'
        changed[entry]['size'] = 7
        self.assertEqual(converter.static_events([entry], changed, {}, 3)[1]['family'], 'private_hot_write')

    def test_lowering_does_not_execute_state_or_lose_original_wait(self):
        events = [{'kind': 'call', 'source_va': '0x10010', 'op': 'stage_setup_source', 'args': [], 'force': 0, 'fade': 0},
                  {'kind': 'call', 'source_va': '0x10020', 'op': 'reset_directions', 'args': [], 'force': 0, 'fade': 0},
                  {'kind': 'write', 'source_va': '0x10030', 'address': '0x51a83', 'value': 0}]
        flow, sources = converter.lower_events(events, 3, 'entry')
        self.assertEqual(flow['id'], 'stage.source.entry.003')
        self.assertEqual([(n['op'], n['args']) for n in flow['nodes']],
                         [('call_init', []), ('reset_directions', []), ('sleep_ms', [20]), ('display_mode', [0]), ('end', [])])
        self.assertEqual(len(sources), len(flow['nodes']))

    def test_cpu_oracle_is_independent_of_static_argument_decoder(self):
        entry, instructions, refs = straight_fixture()
        image = bytearray(0x64000)
        for va, instruction in instructions.items():
            raw = bytes.fromhex(instruction['raw_hex'])
            image[va:va + len(raw)] = raw
        actual = converter.execute_linear(bytes(image), entry, 3, set(instructions))
        self.assertIsNone(actual['blocker'])
        self.assertEqual(actual['events'][0]['args'], [1, 2])
        expected, _ = converter.static_events(list(instructions), instructions, refs, 3)
        converter.compare_original_trace(expected, actual['events'])
        expected[0]['args'][1] = 256
        with self.assertRaisesRegex(ValueError, 'independent'):
            converter.compare_original_trace(expected, actual['events'])

    def test_output_preserves_existing_evidence_and_original_directory(self):
        (ROOT / 'build/s2').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build/s2') as directory:
            for path in (Path(directory), ROOT / 'original_game/new-transition', ROOT / 'data/static/new-transition'):
                with self.subTest(path=path), self.assertRaises(ValueError):
                    converter.validate_output(path)

    def test_readset_schema_and_non_original_image_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'schema'):
            converter.source_index({}, b'')
        with self.assertRaisesRegex(ValueError, 'identity'):
            converter.source_index({'schema': 'fd2-s2-transition-domain-v1', 'strict_byte_match': True}, b'not original')

    def test_shared_call_missing_cycle_abi_and_budget_are_rejected(self):
        helpers = converter.shared_flows()
        document = {'domain': 'stage', 'operations': {op: {'args': []} for op in converter.SHARED_CALLS}, 'flows': helpers}
        generate.validate_stage_source_calls(document)
        self.assertGreater(converter.validate_shared_calls(helpers)['stage.source.initialize'], 131)
        changed = copy.deepcopy(document);changed['flows'].pop(1)
        with self.assertRaisesRegex(ValueError, 'missing'):generate.validate_stage_source_calls(changed)
        changed = copy.deepcopy(document);changed['flows'][1]['nodes'][0].update(op='call_init',args=[])
        with self.assertRaisesRegex(ValueError, 'cycle'):generate.validate_stage_source_calls(changed)
        changed = copy.deepcopy(document);changed['operations']['call_init']['args']=[{'name':'callee'}]
        with self.assertRaisesRegex(ValueError, 'ABI'):generate.validate_stage_source_calls(changed)
        changed = copy.deepcopy(document)
        many = [{'id':f'n{i}', 'op':'call_init', 'args':[], 'on':{'next':f'n{i+1}'}} for i in range(470)]
        many.append({'id':'n470','op':'end','args':[],'on':{}})
        changed['flows'].append({'id':'stage.source.victory.budget','entry':'n0','nodes':many})
        with self.assertRaisesRegex(ValueError, 'budget'):generate.validate_stage_source_calls(changed)

    def test_layout_keeps_actor_order_special_override_and_view_prefix(self):
        arrays = [{'source_va':hex(0x52000+i*4),'raw_hex':raw,'first_raw':1} for i,raw in enumerate(('0203','0405','0102'))]
        flow, sources = converter.lower_events([{'kind':'call','op':'layout','args':arrays+[1,2,2,9,8,3,4,5],'source_va':'0x12345'}],4,'victory')
        actions = [(n['op'],n['args']) for n in flow['nodes']]
        self.assertEqual(actions[:2],[('call_fade_out',[]),('clear_acted',[])])
        self.assertEqual(actions[2:5],[('unit_field_write',[1,1,0,0,2]),('unit_field_write',[1,1,1,0,4]),('unit_field_write',[1,1,2,0,1])])
        self.assertEqual(actions[8:11],[('unit_field_write',[2,2,0,0,9]),('unit_field_write',[2,2,1,0,8]),('unit_field_write',[2,2,2,0,3])])
        self.assertEqual(actions[-6:],[('display_mode',[0]),('view_set',[4,5,4,5]),('refresh',[1]),('call_fade_in',[]),('delay',[200]),('end',[])])
        self.assertEqual(len(sources),len(flow['nodes']))

    def test_layout_requires_source_array_provenance_not_only_equal_bytes(self):
        entry,instructions,refs=straight_fixture()
        codes=[b'\x6a\x00']*8+[b'\x68\x00\x20\x05\x00']*3
        address=entry;instructions={}
        for raw in codes:
            instructions[address]={'ea':address,'size':len(raw),'raw_hex':raw.hex(),'disasm':'push constant'};address+=len(raw)
        instructions[address]={'ea':address,'size':5,'raw_hex':'e800000000','disasm':'call layout'}
        refs={address:[{'to':0x233C6,'type':'CALL_NEAR'}]}
        events,blocker=converter.static_events(list(instructions),instructions,refs,4,image=bytes(0x64000))
        self.assertIsNone(events);self.assertEqual(blocker['family'],'layout_source_array')

    def test_composed_predicates_keep_prefix_arms_and_error_terminals(self):
        def query(op,result,args):return {'kind':'query','target':'inline:1','source_va':'0x11111','op':op,'args':args,'result':result}
        def call(op,args):return {'kind':'call','op':op,'args':args,'source_va':'0x22222','force':0,'fade':0}
        paths=[[query('branch_turn',1,[4,18]),call('roster_append',[1])],
               [query('branch_turn',0,[4,18]),query('branch_roster_text',1,[18]),call('roster_append',[2])],
               [query('branch_turn',0,[4,18]),query('branch_roster_text',0,[18]),call('roster_append',[3])]]
        flow,_=converter.lower_cases([{'events':path} for path in paths],15,'victory')
        nodes={n['id']:n for n in flow['nodes']}
        for turn,roster,expected in ((1,0,1),(0,1,2),(0,0,3)):
            label=flow['entry'];seen=[]
            while nodes[label]['op']!='end':
                n=nodes[label]
                if n['op'].startswith('branch_'):
                    value=turn if n['op']=='branch_turn' else roster
                    self.assertEqual(nodes[n['on']['error']]['op'],'fail')
                    label=n['on']['yes' if value else 'no']
                else:seen.append(n['args'][0]);label=n['on']['next']
            self.assertEqual(seen,[expected])

    def test_source_coordinates_and_full_coverage_are_checked_independently_of_counts(self):
        image=bytearray(0x54000);image[0x11000]=0xC3
        bindings=[]
        for table,base in (('stage_entry',0x51D71),('stage_victory',0x51DE9)):
            for slot in range(30):
                image[base+slot*4:base+slot*4+4]=struct.pack('<I',0x11000)
                bindings.append({'table':table,'slot':slot,'source_id':f'dispatch:{table}:{slot:03d}',
                    'source':{'canonical_va':hex(base+slot*4)},'target_va':'0x11000','owners':['0x11000']})
        sha=hashlib.sha256(image).hexdigest()
        readset={'schema':'fd2-s2-transition-domain-v1','strict_byte_match':True,'image_sha256':sha,
            'ledger_sha256':converter.S1_LEDGER_SHA256,'bindings':bindings,'functions':[{'va':0x11000,
                'blocks':[{'start':0x11000,'end':0x11001,'successors':[]}],
                'chunks':[{'start':0x11000,'end':0x11001,'raw_hex':'c3','sha256':hashlib.sha256(b'\xc3').hexdigest()}],
                'instructions':[{'ea':0x11000,'size':1,'raw_hex':'c3','disasm':'retn'}],'xrefs':[]}]}
        with patch.object(converter,'IMAGE_SHA256',sha),patch.object(converter,'SUPPORTING_OWNERS',()):
            self.assertEqual(set(converter.source_index(readset,bytes(image))[0]),{0x11000})
            for mutate in (lambda r:r['bindings'][0].update(target_va='0x11001'),
                           lambda r:r['bindings'][0].update(source_id='dispatch:stage_entry:001'),
                           lambda r:r['bindings'][0]['source'].update(canonical_va='0x51d75'),
                           lambda r:r['functions'][0].update(instructions=[])):
                changed=copy.deepcopy(readset);mutate(changed)
                with self.assertRaises(ValueError):converter.source_index(changed,bytes(image))

    def test_dynamic_text_and_id_filter_are_compared_to_real_input_not_observed_output(self):
        source=[{'kind':'filter_position','args':[16,31,16,6]},
                {'kind':'call','target':'0x15f84','op':'text','args':['current_text_resource',{'event_index':12,'add':5}], 'force':0,'fade':0}]
        actual=[{'kind':'actor_write','actor':17,'offset':0,'size':1,'value':16},
                {'kind':'actor_write','actor':17,'offset':1,'size':1,'value':6},
                {'kind':'call','target':'0x15f84','op':'text','args':[0x109000,260],'force':0,'fade':0}]
        fixture={'actor_count':18,'actor_ids':{15:31,17:31,95:31},'event_bytes':{12:255}}
        converter.compare_original_trace(source,actual,fixture)
        changed=copy.deepcopy(actual);changed[-1]['args'][1]=4
        with self.assertRaisesRegex(ValueError,'independent'):converter.compare_original_trace(source,changed,fixture)
        changed=copy.deepcopy(actual);changed.insert(0,{'kind':'actor_write','actor':95,'offset':0,'size':1,'value':16})
        with self.assertRaisesRegex(ValueError,'independent'):converter.compare_original_trace(source,changed,fixture)

    def test_source_dedup_requires_whole_graph_and_preserves_distinct_branch_edges(self):
        first={'id':'stage.source.entry.012','entry':'choose','nodes':[
            {'id':'choose','op':'branch_any_hidden','args':[0,0],'on':{'yes':'yes','no':'no'}},
            {'id':'yes','op':'end','args':[],'on':{}},{'id':'no','op':'fail','args':[],'on':{}}]}
        second=copy.deepcopy(first);second['id']='stage.source.entry.018'
        third=copy.deepcopy(first);third['id']='stage.source.entry.019'
        third['nodes'][0]['on']={'yes':'no','no':'yes'}
        definitions,bindings=converter.deduplicate_source_flows([first,second,third])
        self.assertEqual([flow['id'] for flow in definitions],[first['id'],third['id']])
        self.assertEqual(bindings,{second['id']:first['id']})

    def test_formal_definition_verification_rejects_changed_graph_and_alias_target(self):
        flow={'id':'stage.source.entry.012','entry':'done','nodes':[{'id':'done','op':'end','args':[],'on':{}}]}
        report={'flows':[flow],'source_bindings':{'stage.source.entry.018':flow['id']}}
        document={'flows':[flow],'bindings':dict(report['source_bindings'])}
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'stage.json'
            path.write_text(json.dumps(document),encoding='utf8')
            self.assertTrue(converter.verify_definitions(report,path)['verified'])
            changed=copy.deepcopy(document);changed['flows'][0]['nodes'][0]['op']='fail'
            path.write_text(json.dumps(changed),encoding='utf8')
            with self.assertRaisesRegex(ValueError,'differ'):converter.verify_definitions(report,path)
            changed=copy.deepcopy(document);changed['bindings']['stage.source.entry.018']='stage.source.entry.999'
            path.write_text(json.dumps(changed),encoding='utf8')
            with self.assertRaisesRegex(ValueError,'differ'):converter.verify_definitions(report,path)


if __name__ == '__main__':
    unittest.main()
