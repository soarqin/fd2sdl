"""从当前唯一源/绑定生成可审阅S5发布补丁，不写生产输入或开放玩法。"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

import audit_stage_data as audit
import stage_release_scope as release
import s2_source_disposition as disposition
import s5_stage_transitions as extension_tool
from s5_output import validate_output


def build(original,overlay_path,extension_path,battle_stages,entry_stages,callback51_source=None):
    original=Path(original);root=release.ROOT
    inputs={Path(overlay_path):Path(overlay_path).read_bytes(),Path(extension_path):Path(extension_path).read_bytes(),
            Path(__file__):Path(__file__).read_bytes()}
    if audit.digest(inputs[Path(overlay_path)])!=release.S2_SHA256:raise ValueError('frozen S2 source identity differs')
    for path in (root/'data/static').rglob('*.json'):inputs[path]=path.read_bytes()
    overlay=json.loads(inputs[Path(overlay_path)],object_pairs_hook=audit.unique_keys)
    extension=json.loads(inputs[Path(extension_path)],object_pairs_hook=audit.unique_keys)
    documents={}
    for name in ('stage','common_event'):
        path=root/f'data/static/flows/{name}.json';documents[name]=json.loads(inputs[path],object_pairs_hook=audit.unique_keys)
    catalog={(domain,f['id']):f for domain,doc in documents.items() for f in doc['flows']}
    dispatch,changes,source_inputs=disposition.apply_source_extension(overlay['dispatch'],extension,catalog)
    inputs.update(source_inputs)
    if callback51_source:
        path=Path(callback51_source);inputs[path]=path.read_bytes();proof=json.loads(inputs[path])
        flow=catalog.get(('common_event','common.callback.051'))
        digest=audit.digest(disposition.encode(flow).encode()) if flow else None
        if (proof.get('schema')!='fd2-s5-callback51-source-v1' or proof.get('original_CPU_executed') is not True or
                proof.get('actual_C',{}).get('verified') is not True or proof.get('target_va')!='0x355f0' or
                proof.get('definition')!='common.callback.051' or proof.get('definition_sha256')!=digest or
                proof.get('expected_definition_sha256')!=digest):raise ValueError('callback51 complete source/current C proof differs')
        for name,expected in proof.get('inputs_sha256',{}).items():
            path=Path(name);inputs[path]=path.read_bytes()
            if audit.digest(inputs[path])!=expected:raise ValueError('callback51 proof inputs changed')
        c=proof['actual_C'];path=Path(c['probe']);inputs[path]=path.read_bytes()
        if audit.digest(inputs[path])!=c['probe_sha256']:raise ValueError('callback51 proof executable changed')
        dispatch['dispatch:event_callback:051'].update(definition_id='common.callback.051',disposition='converted_candidate')
    for path in (Path(extension_tool.__file__),Path(release.__file__),Path(disposition.__file__),Path(audit.__file__)):
        inputs[path]=path.read_bytes()
    inventory=audit.audit(original);flows,aliases,_=audit.load_definitions(root/'data/static')
    field=audit.archive_entries((original/'FDFIELD.DAT').read_bytes());exe=(original/'FD2.EXE').read_bytes()
    pointers=audit.collect_internal_offset_fixups(original/'FD2.EXE')
    selected=set(battle_stages)|set(entry_stages)
    if not selected or set(battle_stages)&set(entry_stages):raise ValueError('empty/overlapping release patch domain')
    patches={domain:{'bindings':{},'flows':[]} for domain in documents};rows=[]
    for st in sorted(selected):
        stage=next(s for s in inventory['stages'] if s['stage']==st);battle=st in battle_stages
        if stage['reference_errors']:raise ValueError('source stage has resource reference errors')
        needed=[(f'stage.entry.{st:03d}',f'dispatch:stage_entry:{st:03d}')]
        if battle:
            needed += [(f'stage.{st:03d}.result' if dispatch[f'dispatch:stage_runtime_result:{st:03d}']['definition_domain']=='common_event'
                        else f'stage.result.{st:03d}',f'dispatch:stage_runtime_result:{st:03d}'),
                       (f'stage.victory.{st:03d}',f'dispatch:stage_victory:{st:03d}')]
            needed += [(f'stage.{st:03d}.action.{slot:03d}',f'dispatch:event_callback:{slot:03d}')
                       for slot in sorted({r[2] for r in release.callback_requirements(stage)})]
        evidence=[]
        for runtime,identity in needed:
            row=dispatch[identity]
            if row['disposition'] in ('blocked','unresolved'):raise ValueError('release candidate requires quarantined source: '+identity)
            if identity=='dispatch:event_callback:041':
                raise ValueError('new release caller41 requires explicit S3 refinement, not frozen S2 old alias')
            source=row['source'];va=int(source['canonical_va'],16)
            raw=audit.source_range(exe,va,4);target=pointers.get(va)
            if (not target or target[2]!=int(row['target_va'],16) or source['raw_hex']!=raw.hex() or
                    source['read_sha256']!=audit.digest(raw)):
                raise ValueError('patch original caller/slot/target differs')
            actual=audit.resolve(row['definition_id'],flows,aliases)
            if not actual:raise ValueError('release candidate lacks exact unique definition')
            domain=row['definition_domain']
            old=documents[domain].get('bindings',{}).get(runtime)
            previous=audit.resolve(old,flows,aliases) if old is not None else None
            if old is not None and (previous is None or previous['id']!=actual['id']):
                raise ValueError('release patch would overwrite different existing binding')
            if old is None:patches[domain]['bindings'][runtime]=actual['id']
            evidence.append({'runtime_id':runtime,'source_id':identity,'target_va':row['target_va'],
                'definition_domain':domain,'definition_id':actual['id'],
                'definition_sha256':audit.digest(disposition.encode(actual).encode()),'already_present':old is not None})
        entry=flows[evidence[0]['definition_id']]
        counts=release.entry_counts(entry,audit.parse_metadata(field[st*3+1][1]),flows,aliases)
        cap={'id':f'stage.capability.{st:03d}','entry':'capability','nodes':[{'id':'capability',
            'op':'stage_capability','args':[int(battle),min(counts),0,0],'on':{},'views':['logic']}]}
        patches['stage']['flows'].append(cap)
        rows.append({'stage':st,'battle_enabled':battle,'entry_count':sorted(counts),'bindings':evidence,
                     'capability':cap['nodes'][0]['args'],'slot_count_source':'DAT metadata.byte1 at runtime; cap.slots0 no duplicate count'})
    for name in audit.SOURCE_HASHES:
        path=original/name;inputs[path]=path.read_bytes()
        if audit.digest(inputs[path])!=audit.SOURCE_HASHES[name]:raise ValueError('original source identity differs')
    if any(path.read_bytes()!=raw for path,raw in inputs.items()):raise ValueError('release patch input changed')
    return {'schema':'fd2-s5-release-patch-v1','patch':patches,'stages':rows,
        'input_sha256':{str(p):audit.digest(raw) for p,raw in inputs.items()},
        'summary':{'battle_stages':sorted(battle_stages),'entry_only_stages':sorted(entry_stages),
            'new_bindings':{domain:len(p['bindings']) for domain,p in patches.items()},
            'capability_definitions':len(rows)},
        'claims':{'formal_inputs_written':False,'new_runtime_bindings':False,'source_or_C_or_native_runtime_proven_by_patch':False},
        'next_gate':{'review_install_once':True,'fresh_scope_battle_stages':sorted(battle_stages),
            'fresh_scope_entry_only_stages':sorted(entry_stages),
            'runtime_gate':'independent source/C oracle and controlled original/native App evidence; terminal30 not ordinary capability'}}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('original','s2-disposition','s5-extension','output-dir'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--battle-stages',type=release.stages,required=True)
    p.add_argument('--entry-stages',type=release.stages,default=[])
    p.add_argument('--callback51-source',type=Path)
    args=p.parse_args();output=validate_output(args.output_dir,original=args.original)
    report=build(args.original,args.s2_disposition,args.s5_extension,args.battle_stages,args.entry_stages,args.callback51_source)
    output.mkdir(parents=True);(output/'patch.json').write_text(disposition.encode(report),encoding='utf8')
    print(disposition.encode(report['summary']),end='')


if __name__=='__main__':main()
