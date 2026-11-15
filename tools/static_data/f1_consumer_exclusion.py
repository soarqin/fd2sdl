"""F1正常来源的8条consumer排除；显式继承冻结Domain/原CPU证据，不开启玩法。"""
from __future__ import annotations

import json
from pathlib import Path
import struct
import sys

import audit_stage_data as audit

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = 'fd2-f1-consumer-exclusion-proof-v1'
SOURCE_SHA = '18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517'
CPU_SHA = '893065b48064a41735e8a98e5ebd4ac13ab6b55ace849bbd4e5040c6b85977a3'
IMAGE_SHA = '8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
EXCLUDED_IDS = ('stage:01:cell_action:00012', 'stage:31:turn:00000', 'stage:31:turn:00001',
                'stage:31:turn:00002', 'stage:31:turn:00003', 'stage:31:cell_action:00012',
                'stage:31:drop:00009', 'stage:31:drop:00014')
TOOLS = {'f1_domain_source.py':'0f4a5eee21378732d166df83149c8e2e42887468002918369470914d3a4f639c',
         'verify_f1_boundaries.py':'3a9ae3ceb49bd460f7b137e7e2c1ecf8fce031f088f25c5dc6922134d9ead20e'}


def verify_contract(source, cpu, entries):
    """摘要之外独立重读map/slot；完整writer/CFG门由verify_source复核。"""
    if (cpu.get('schema') != 'fd2-f1-boundary-original-cpu-v1' or cpu.get('ok') is not True or
            cpu.get('source_sha256') != audit.digest(json.dumps(source,sort_keys=True).encode())):
        raise ValueError('F1 consumer exclusion CPU/source join differs')
    unused = cpu['unused_records']; opening = unused['opening_root']
    if (unused.get('full_transitive_consumer_and_map_writer_proof') is not True or
            opening.get('stage_writes') != [32,31,0] or opening.get('return_stage') != 0 or
            opening.get('battle_consumers_in_original_root') != [] or
            unused.get('stage31_turn_and_drop_sources') != [identity for identity in EXCLUDED_IDS if ':turn:' in identity or ':drop:' in identity]):
        raise ValueError('F1 opening consumer exclusion contract differs')
    maps = unused['action12']
    if len(maps) != 2 or {row['source_id'] for row in maps} != {EXCLUDED_IDS[0],EXCLUDED_IDS[5]}:
        raise ValueError('F1 map exclusion source IDs differ')
    for stage in (1,31):
        grid, meta = entries[stage*3][1], entries[stage*3+1][1]
        width,height = struct.unpack_from('<HH',grid)
        low12 = sum((grid[4+4*i+2]&31)==12 for i in range(width*height))
        witness = next(row for row in maps if row['source_id'] == f'stage:{stage:02}:cell_action:00012')
        if (len(grid) != 4+width*height*4 or low12 or meta[0x53+12*3:0x53+13*3] != bytes.fromhex('020000') or
                witness.get('action_bytes') != '020000' or witness.get('all_map_cells_with_low5_equal12') != low12 or
                witness.get('dynamic_map_writer_closure_proven') is not True):
            raise ValueError('F1 original map low5/action12 or writer contract differs')
    meta = entries[31*3+1][1]
    if ([meta[3+i*3:6+i*3].hex() for i in range(4)] != ['030001','040100','050200','060301'] or
            [meta[0x83+i*26+22:0x83+i*26+25].hex() for i in (9,14)] != ['020400','020500']):
        raise ValueError('F1 original stage31 turn/drop source bytes differ')


def validate(path, sources):
    snapshots = {}
    def read(path):
        path = Path(path).resolve(strict=True); raw = path.read_bytes(); snapshots[path] = raw
        return raw
    def frozen(row, expected):
        raw = read(row['path'])
        if row.get('sha256') != expected or audit.digest(raw) != expected:
            raise ValueError('F1 frozen consumer exclusion input identity differs')
        return raw
    manifest_raw = read(path); manifest = json.loads(manifest_raw,object_pairs_hook=audit.unique_keys)
    if (manifest.get('schema') != SCHEMA or manifest.get('excluded_source_ids') != list(EXCLUDED_IDS) or
            any(manifest.get('claims',{}).get(key) is not False for key in
                ('new_original_native','all_normal_semantic_complete','F1_full_gate_passed','new_runtime_bindings'))):
        raise ValueError('F1 consumer exclusion proof schema/scope differs')
    source = json.loads(frozen(manifest['source'],SOURCE_SHA),object_pairs_hook=audit.unique_keys)
    cpu = json.loads(frozen(manifest['cpu'],CPU_SHA),object_pairs_hook=audit.unique_keys)
    image = frozen(manifest['image'],IMAGE_SHA)
    tool_rows = manifest['tools']
    if len(tool_rows) != len(TOOLS) or {Path(row['path']).name for row in tool_rows} != set(TOOLS):
        raise ValueError('F1 consumer exclusion tools missing/duplicate')
    for row in tool_rows:
        name = Path(row['path']).name
        if Path(row['path']).resolve() != (ROOT/'tools/ida'/name).resolve():
            raise ValueError('F1 consumer exclusion checker is not the current project tool')
        frozen(row,TOOLS[name])
    if set(manifest['original']) != {'FD2.EXE','FDFIELD.DAT'}:
        raise ValueError('F1 consumer exclusion original scope differs')
    for name,row in manifest['original'].items():
        if Path(row['path']).resolve() != sources.paths[name].resolve():
            raise ValueError('F1 consumer exclusion original path differs')
        if frozen(row,audit.SOURCE_HASHES[name]) != sources.data[name]:
            raise ValueError('F1 consumer exclusion original identity differs')
    sys.path.insert(0,str(ROOT/'tools/ida'))
    import verify_f1_boundaries as boundary
    boundary.verify_source(source,image)
    verify_contract(source,cpu,sources.entries['FDFIELD.DAT'])
    if any(input_path.read_bytes()!=raw for input_path,raw in snapshots.items()):
        raise ValueError('F1 consumer exclusion input changed during verification')
    return {'schema':SCHEMA,'path':str(Path(path).resolve()),'sha256':audit.digest(manifest_raw),
            'excluded_source_ids':list(EXCLUDED_IDS),
            'source_sha256':SOURCE_SHA,'cpu_sha256':CPU_SHA,'image_sha256':IMAGE_SHA,
            'qualified_scope':'fixed original-generated map/metadata, complete reviewed writer/caller closure; no edited/external SAV reachability grant',
            'inherited_bounded_original_CPU':True,'new_original_CPU_executed':False,
            'all_normal_semantic_complete':False,'F1_full_gate_passed':False,'new_runtime_bindings':False}, snapshots
