"""正式callback82的分层来源/C收据join；不让通用ACK探针代签Scene写点。"""
from pathlib import Path
import json
import re

import convert_stage_callbacks as callbacks
import s5_stage_transitions as source

READSET_SHA256 = '683ec0e987b4fff9d515677df1fc0d9d1d77d9e95ab11f816135fd2bc06cdbb2'
PLAY_PASS = 'callback82 Play: actual shared helper, both source branches, ACK and HOST/error xy prefixes PASS'
PROJECTION_PASS = 'field_s5_callback82: selector/AI writes and local resource projection PASS (no GUI)'
PLAY_TARGET = 'fd2_field_s5_callback82_play_test'
PLAY_SOURCES = ('field_s5_callback82_play_test.c', 'field_s5_callback82_scope_checks.c')


def play_build_identity(proof, snapshots):
    """同一Play目标须编译真实Scene HOST/display测试，不接受仅主TU的旧身份。"""
    cmake = callbacks.ROOT/'src/CMakeLists.txt'
    required = [cmake] + [callbacks.ROOT/'src/tests'/name for name in PLAY_SOURCES]
    if any(str(path) not in proof['inputs_sha256'] for path in required):
        raise ValueError('callback82 actual Play lacks delegated Scene TU/CMake input identity')
    if proof.get('actual_Play_C_Scene', {}).get('build_target') != PLAY_TARGET:
        raise ValueError('callback82 actual Play build target differs')
    # 仅核当前有源literal编译合同；未知变量/别名形式不推断成该目标。
    text = snapshots[cmake].decode('utf-8-sig')
    text = re.sub(r'#\[(=*)\[.*?\]\1\]', '', text, flags=re.S)
    text = re.sub(r'(?m)#.*$', '', text)
    recipes = re.findall(r'(?i:\badd_executable)\s*\(\s*' + re.escape(PLAY_TARGET) +
                         r'\s+([^)]*)\)', text)
    if len(recipes) != 1:
        raise ValueError('callback82 actual Play requires one exact CMake target')
    tokens = [token.strip('"') for token in re.findall(r'"[^"]*"|\S+', recipes[0])]
    if any(tokens.count('tests/' + name) != 1 for name in PLAY_SOURCES):
        raise ValueError('callback82 actual Play CMake target lacks both exact test TUs')
    return {'target': PLAY_TARGET, 'sources': [str(path) for path in required[1:]],
            'cmake': str(cmake), 'cmake_sha256': proof['inputs_sha256'][str(cmake)]}


def validate(path, common, stage, decision, ledger, image):
    snapshots = {module_path:module_path.read_bytes()
                 for module_path in (Path(__file__), Path(source.__file__))}
    def integers(value, expected):
        return isinstance(value, list) and all(type(n) is int for n in value) and value == expected

    def read(path):
        path = Path(path); raw = path.read_bytes(); snapshots[path] = raw
        return json.loads(raw, object_pairs_hook=callbacks.audit.unique_keys)

    def files(values):
        if not isinstance(values, dict) or not values:
            raise ValueError('callback82 evidence lacks complete input identities')
        for name, digest in values.items():
            if not isinstance(name, str) or not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{64}', digest):
                raise ValueError('callback82 evidence input identity malformed')
            source = Path(name); raw = source.read_bytes(); snapshots[source] = raw
            if callbacks.digest(raw) != digest: raise ValueError('callback82 evidence input changed: '+name)

    proof = read(path)
    if (proof.get('schema') != 'fd2-s5-callback82-layered-C18-v1' or proof.get('passed') is not True or
            proof.get('shipping_binding_approved') is not False or
            proof.get('formal_binding') != 'stage.029.action.082' or
            proof.get('source_id') != 'dispatch:event_callback:082' or proof.get('target_va') != '0x362e8' or
            proof.get('common_alias') != 'common.callback.082' or
            proof.get('shared_definition') != 'stage.source.teleport'):
        raise ValueError('callback82 exact layered source/binding/domain differs')
    incoming = sorted(r['source_id'] for r in ledger['records']
                      if 'dispatch:event_callback:082' in r.get('references', []))
    source_row = next((r for r in ledger['records'] if r['source_id'] == 'dispatch:event_callback:082'), None)
    if (source_row is None or source_row['decoded']['target_va'] != '0x362e8' or
            decision.get('source_id') != source_row['source_id'] or decision.get('slot') != 82 or
            decision.get('target_va') != '0x362e8' or decision.get('source_pointer') != source_row['source'] or
            sorted(decision.get('caller_sources', [])) != incoming or
            not any(s.startswith('stage:29:') for s in incoming) or
            common.get('bindings', {}).get('stage.029.action.082') != 'common.callback.082'):
        raise ValueError('callback82 original slot/target/caller/unique formal alias differs')
    wanted = {('common_event', 'common.callback.082'): common,
              ('stage', 'stage.source.teleport'): stage}
    if callbacks.digest(image) != callbacks.IMAGE_SHA:
        raise ValueError('callback82 strict original image differs')
    expected = {('common_event', 'common.callback.082'): source.compile_program('common.callback.082',
                    source.caller_programs(image)['common.callback.082']),
                ('stage', 'stage.source.teleport'): source.teleport_helper()}
    definitions = proof.get('definitions', [])
    if len(definitions) != 2 or {(d.get('domain'), d.get('id')) for d in definitions} != set(wanted):
        raise ValueError('callback82 evidence lacks exact two definitions')
    for definition in definitions:
        key = definition['domain'], definition['id']
        matches = [f for f in wanted[key]['flows'] if f['id'] == key[1]]
        if (len(matches) != 1 or matches[0] != expected[key] or
                definition.get('node_count') != len(matches[0]['nodes']) or
                # 分层收据的既有canonical编码为sort_keys=True单行、无末换行；
                # 通用converter的indent2+换行摘要另有域，不能混用。
                definition.get('definition_sha256') != callbacks.digest(json.dumps(matches[0],sort_keys=True).encode())):
            raise ValueError('callback82 current unique cold definition differs')
    files(proof.get('inputs_sha256'))
    for name in ('common_event', 'stage'):
        current = callbacks.ROOT/f'data/static/flows/{name}.json'
        if str(current) not in proof['inputs_sha256']:
            raise ValueError('callback82 proof lacks current formal input identity')
    play_build = play_build_identity(proof, snapshots)
    original = proof.get('original_source_caller', {}); module = proof.get('teleport_module', {})
    if (original.get('original_CPU_executed') is not True or
            original.get('readset_sha256') != READSET_SHA256 or
            not integers(original.get('complete_362e8_state16'), [0, 1, 2, 3, 4]) or
            type(original.get('complete_33f78_traces')) is not int or original['complete_33f78_traces'] != 1 or
            original.get('AI_raw52_display_and_state32_verified') is not True or
            module.get('actual_C_module_executed') is not True or
            module.get('controlled_argument_states') != 8 or module.get('ordered_fullpage_VGA_xy_prefixes') != 408):
        raise ValueError('callback82 original caller/module layered scope incomplete')
    for layer in (original, module):
        name = layer.get('proof'); digest = layer.get('proof_sha256')
        if not name or proof['inputs_sha256'].get(name) != digest:
            raise ValueError('callback82 nested source proof identity missing')
        machine = read(name)
        if (machine.get('schema') != 'fd2-s5-teleport-full-source-C-v1' or
                machine.get('original_CPU_executed') is not True or machine.get('actual_C_executed') is not True or
                machine.get('C_consumer', {}).get('vectors') != 8 or
                machine.get('C_consumer', {}).get('ordered_prefixes') != 408):
            raise ValueError('callback82 nested actual source/C comparison incomplete')
        files(machine.get('inputs_sha256'))
        if READSET_SHA256 not in machine['inputs_sha256'].values():
            raise ValueError('callback82 nested proof not tied to complete corrected owner readset')
        callers = machine.get('caller_traces', [])
        if (len(callers) != 6 or
                not integers(sorted(r['state16'] for r in callers if r['state16'] is not None), [0, 1, 2, 3, 4]) or
                sum(r['state16'] is None for r in callers) != 1 or
                any(r.get('state32_unchanged') is not True for r in callers) or
                any(r.get('actual_AI_raw52_and_display_writes_verified') is not True for r in callers if r['state16'] is not None)):
            raise ValueError('callback82 nested full original caller branches incomplete')
    for name, states_key, states, stdout, test_source in (
            ('actual_Play_C_Scene', 'states', [0, 4], PLAY_PASS, 'field_s5_callback82_play_test.c'),
            ('projection_C', 'state16', [0, 1, 2, 3, 4], PROJECTION_PASS, 'field_s5_callback82_test.c')):
        layer = proof.get(name, {}); receipt = layer.get('receipt', {})
        executable = receipt.get('executable')
        if (layer.get('actual_C_executed') is not True or not integers(layer.get(states_key), states) or
                type(receipt.get('exit_code')) is not int or receipt['exit_code'] != 0 or
                not executable or proof['inputs_sha256'].get(executable) != receipt.get('sha256') or
                not isinstance(receipt.get('stdout'), str) or receipt['stdout'].splitlines() != [stdout] or
                receipt.get('stderr') != '' or
                str(callbacks.ROOT/'src/tests'/test_source) not in proof['inputs_sha256']):
            raise ValueError('callback82 actual Play/projection C receipt incomplete')
    if len(proof['actual_Play_C_Scene'].get('scenarios', [])) != 4:
        raise ValueError('callback82 actual Play success/HOST/error scenarios incomplete')
    return {'binding_id': 'stage.029.action.082', 'stage': 29, 'slot': 82,
            'source_id': proof['source_id'], 'target_va': proof['target_va'],
            'definition': proof['common_alias'], 'shared_definition': proof['shared_definition'],
            'proof': str(path), 'proof_sha256': callbacks.digest(snapshots[Path(path)]),
            'original_caller_states': [0, 1, 2, 3, 4], 'module_states': 8, 'ordered_prefixes': 408,
            'actual_Play_states': [0, 4], 'projection_states': [0, 1, 2, 3, 4],
            'actual_Play_build_identity': play_build,
            'generic_ACK_vectors_contributed': 0, 'native_GUI_claimed': False}, snapshots
