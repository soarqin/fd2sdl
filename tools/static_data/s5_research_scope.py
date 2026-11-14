"""26..29只读研究闭包；虚拟alias仅用于发现原引用错误，绝不安装/开放。"""
from pathlib import Path
import argparse
import json

import stage_release_scope as scope
import s2_source_disposition as disposition
import s5_stage_transitions as conversion
from s5_stage_resources import ResourceReferences
from stage_source_ledger import Sources
from s5_output import validate_output


def build(original,overlay_path,candidate_path):
    original=Path(original);overlay_raw=Path(overlay_path).read_bytes();candidate_raw=Path(candidate_path).read_bytes()
    if scope.audit.digest(overlay_raw)!=scope.S2_SHA256:raise ValueError('frozen S2 source differs')
    overlay=json.loads(overlay_raw);candidate=json.loads(candidate_raw)
    _,expected,inputs=conversion.validate_candidate(candidate)
    for path in (scope.ROOT/'data/static').rglob('*.json'):inputs[path]=path.read_bytes()
    flows,aliases,_=scope.audit.load_definitions(scope.ROOT/'data/static')
    catalog={(('common_event' if identity.startswith('common.') else 'stage'),identity):flow
             for identity,flow in flows.items() if identity.startswith(('stage.','common.'))}
    dispatch,changes,source_inputs=disposition.apply_source_extension(overlay['dispatch'],candidate,catalog)
    inputs.update(source_inputs)
    sources=Sources(original);resources=ResourceReferences(original,sources.data)
    inputs.update(resources.snapshots)
    inventory=scope.audit.audit(original);field=sources.entries['FDFIELD.DAT'];shape=sources.entries['FDSHAP.DAT'];text=sources.entries['FDTXT.DAT']
    by_source={row['source_id']:row for row in candidate['rows']};rows=[]
    for st in range(26,30):
        initial=inventory['stages'][st];wanted=[(f'stage.entry.{st:03d}',f'dispatch:stage_entry:{st:03d}'),
            (f'stage.result.{st:03d}',f'dispatch:stage_runtime_result:{st:03d}'),
            (f'stage.victory.{st:03d}',f'dispatch:stage_victory:{st:03d}')]
        wanted += [(f'stage.{st:03d}.action.{slot:03d}',f'dispatch:event_callback:{slot:03d}')
                   for slot in sorted({r[2] for r in scope.callback_requirements(initial)})]
        virtual=dict(aliases);bindings=[];blockers=[]
        for runtime,identity in wanted:
            row=dispatch[identity];definition=row.get('definition_id')
            if row['disposition'] in ('blocked','unresolved'):
                blockers.append({'source_id':identity,'blocker':row['blocker'],'shipping_remains_refused':True})
                definition=by_source[identity]['definition_id']
            if row['definition_domain']=='common_event' and identity.startswith('dispatch:stage_runtime_result:'):
                runtime=f'stage.{st:03d}.result'
            actual=scope.audit.resolve(definition,flows,aliases)
            if not actual:raise ValueError('research exact source definition missing: '+identity)
            virtual[runtime]=actual['id']
            bindings.append({'runtime_id':runtime,'source_id':identity,'definition_id':actual['id'],
                'original_target':row['target_va'],'disposition':row['disposition']})
        reviewed=scope.audit.audit_stage(st,field,shape,text,flows,virtual,inventory['dispatch_tables'])
        refs=[];errors=list(reviewed['reference_errors'])
        for binding in bindings:
            try:
                if ('stage',binding['definition_id']) in expected or ('common_event',binding['definition_id']) in expected:
                    refs.extend(resources.references(st,binding,flows,inventory['movement_sources']))
                else:refs.extend(scope.resource_references(reviewed,[binding],flows,inventory['movement_sources']))
            except ValueError as error:errors.append(f"{binding['runtime_id']}: {error}")
        entry=scope.audit.resolve(f'stage.entry.{st:03d}',flows,virtual)
        counts=scope.entry_counts(entry,scope.audit.parse_metadata(field[st*3+1][1]),flows,virtual)
        rows.append({'stage':st,'current_battle_capability':initial['battle_enabled'],
            'research_only_entry_counts':sorted(counts),'virtual_required_bindings':bindings,
            'known_blockers':blockers,'resource_reference_errors':errors,'resource_references':refs,
            'default255_records_retained':sum(r['default_fill'] for r in initial['metadata']['turn_slots']),
            'native_or_dynamic_identity_proven':False})
    inputs.update({p:sources.data[n] for n,p in sources.paths.items()})
    inputs[Path(overlay_path)]=overlay_raw;inputs[Path(candidate_path)]=candidate_raw;inputs[Path(__file__)]=Path(__file__).read_bytes()
    sources.unchanged()
    if any(path.read_bytes()!=raw for path,raw in inputs.items()):raise ValueError('research source inputs changed')
    return {'schema':'fd2-s5-B-research-scope-v1','stages':rows,
        'summary':{'stages':4,'required_virtual_bindings':sum(len(r['virtual_required_bindings']) for r in rows),
            'known_source_blockers':sum(len(r['known_blockers']) for r in rows),
            'resource_reference_errors':sum(len(r['resource_reference_errors']) for r in rows)},
        'input_sha256':{str(p):scope.audit.digest(raw) for p,raw in inputs.items()},
        'claims':{'virtual_aliases_installed':False,'battle_capabilities_changed':False,
            'ending_pair_source_closed':candidate['claims']['all13_isolated_sources_converted'],
            'source_reading_is_runtime_proof':False,'source_conditions_or_dynamic_identity_validated':False,'new_GUI_executed':False}}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('original','s2-disposition','s5-extension','output-dir'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();output=validate_output(a.output_dir,a.original)
    report=build(a.original,a.s2_disposition,a.s5_extension)
    output.mkdir(parents=True);(output/'research.json').write_text(conversion.encode(report),encoding='utf8')
    print(conversion.encode(report['summary']),end='')


if __name__=='__main__':main()
