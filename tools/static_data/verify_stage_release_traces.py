#!/usr/bin/env python3
"""S3逐正式callback绑定复用独立原码模板，代入各自真实资源域。

不是自然关卡/GUI/RNG重放：原callee替身及C构造区域排除沿S2原收据。
既有有限模板的caller参数差异由单独的来源读集门签核，不在此假造轨迹。
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re

import convert_stage_callbacks as callbacks
from stage_source_ledger import validate_output


def bound_callbacks(document, decisions):
    by_slot = {row['slot']: row for row in decisions}
    for identity, definition in sorted(document.get('bindings', {}).items()):
        match = re.fullmatch(r'stage\.(\d{3})\.action\.(\d{3})', identity)
        if not match:
            continue
        stage, slot = map(int, match.groups())
        if stage < 2:
            continue
        row = by_slot.get(slot)
        if stage > 19 or row is None or definition != row.get('definition') or row.get('blocker'):
            raise ValueError(f'正式callback绑定无已证目标/域: {identity}')
        if not any(source.startswith(f'stage:{stage:02d}:') for source in row.get('caller_sources', [])):
            raise ValueError(f'正式callback绑定无此stage原caller: {identity}')
        yield identity, stage, row


def build(original, ledger_directory, domain_path, image_path, candidate_path, c_probe):
    ledger, _, domain, image, sources, snapshots = callbacks.load_inputs(
        original, ledger_directory, domain_path, image_path)
    document = json.loads(snapshots[callbacks.ROOT / 'data/static/flows/common_event.json'])
    candidate_raw = Path(candidate_path).read_bytes()
    candidate = json.loads(candidate_raw)
    actual = {flow['id']: flow for flow in document['flows']}
    field = callbacks.audit.archive_entries(sources['FDFIELD.DAT'])
    scripts = callbacks.audit.source_scripts(Path(original) / 'FD2.EXE')
    traces, bindings, inherited = [], [], []
    for identity, stage, row in bound_callbacks(document, candidate['decisions']):
        if row['disposition'] != 'converted_candidate':
            inherited.append({'binding_id': identity, 'stage': stage, 'slot': row['slot'],
                              'definition': row['definition'],
                              'scope': 'existing template; original caller parameter/resource proof required separately'})
            continue
        flow = actual[row['definition']]
        if callbacks.digest(callbacks.encode(flow).encode()) != row['definition_sha256']:
            raise ValueError(f'实际callback图不同，不能复用原模板: {identity}')
        metadata = field[stage * 3 + 1][1]
        groups = Counter(metadata[131 + index * 26 + 21] for index in range(metadata[2]))
        seen = {}
        start = len(traces)
        for raw_case in callbacks.mechanism_cases(metadata):
            case = callbacks.fit_mechanism_case(flow, raw_case, groups, scripts)
            original_result = callbacks.source_trace(image, domain, row['slot'], case, groups)
            graph_result = callbacks.graph_trace(flow, case, groups)
            if original_result['trace'] != graph_result['trace']:
                raise ValueError(f'原码/唯一冷图绑定轨迹不等: {identity}/{case["id"]}')
            for at, target in original_result['conditional_edges']:
                seen.setdefault(at, set()).add(target)
            traces.append({'binding_id': identity, 'source_id': row['source_id'], 'case_id': case['id'],
                           'resource_fixture_stage': stage, 'input': case,
                           'source_trace': original_result['trace'],
                           'source_checkpoints': original_result['checkpoints'],
                           'source_finish': original_result['finish'],
                           'candidate_definition': row['definition']})
        expected = callbacks.source_cfg_branches(domain, row['slot'])
        if any(sorted(seen.get(at, ())) != targets for at, targets in expected.items()):
            raise ValueError(f'绑定原条件两边缺失: {identity}')
        bindings.append({'binding_id': identity, 'source_id': row['source_id'], 'definition': row['definition'],
                         'stage': stage, 'cases': len(traces)-start,
                         'original_conditionals': {hex(at): list(map(hex, targets)) for at, targets in expected.items()},
                         'caller_sources': [x for x in row['caller_sources'] if x.startswith(f'stage:{stage:02d}:')]})
    comparison = callbacks.verify_c_probe(c_probe, original, {'cases': traces})
    if any(path.read_bytes() != raw for path, raw in snapshots.items()) or Path(candidate_path).read_bytes() != candidate_raw:
        raise ValueError('输入在绑定比较期间改变')
    for name, raw in sources.items():
        if (Path(original) / name).read_bytes() != raw:
            raise ValueError('原资源在绑定比较期间改变')
    return {'schema': 'fd2-s3-callback-binding-traces-v1', 'passed': True,
            'input_sha256': {**{str(path): callbacks.digest(raw) for path, raw in snapshots.items()},
                             str(candidate_path): callbacks.digest(candidate_raw)},
            'summary': {'new_template_bindings_compared': len(bindings),
                        'different_definitions_compared': len({r['definition'] for r in bindings}),
                        'original_vectors': len(traces), 'inherited_template_bindings': len(inherited),
                        'C_normal': comparison['normal_cases'],
                        'C_wait_prefix': sum(row['mode'] in (1, 2, 3) for row in comparison['cases']),
                        'C_capacity': comparison['capacity_refusal_cases']},
            'bindings': bindings, 'existing_template_caller_proof_separate': inherited,
            'scope': 'explicit caller stages and both original conditional edges, actual C begin/peek/reply; original leaf stubs and appended constructor-region exclusions remain; not native GUI/natural reachability/all-state coverage'}, comparison


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'ledger-dir', 'domain', 'image', 'candidates', 'c-probe', 'output-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        output = validate_output(args.output_dir, original=args.original)
        report, comparison = build(args.original, args.ledger_dir, args.domain, args.image, args.candidates, args.c_probe)
        output.mkdir(parents=True)
        for name, value in [('bindings.json', report), ('C-original-comparison.json', comparison)]:
            (output / name).write_text(callbacks.encode(value), encoding='utf-8')
        print(callbacks.encode(report['summary']), end='')
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
