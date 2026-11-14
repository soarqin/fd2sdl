"""S5 callback51完整355f0/共享3530d原CPU与实际Loot父流程；旧G图保留。"""
from pathlib import Path
import argparse
import json
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
import convert_stage_callbacks as converter
import s3_callback_binding_source as reused
from s5_output import validate_output


def build(original,ledger,domain_path,image_path,definition='common.callback.051',c_probe=None):
    _,_,domain,image,sources,snapshots=converter.load_inputs(original,ledger,domain_path,image_path)
    for path in (Path(__file__),Path(reused.__file__),ROOT/'src/tests/s2_callback_source_test.c',
                 ROOT/'src/field_game.c',ROOT/'src/field_game.h',ROOT/'src/field_common_event_runtime.h',
                 ROOT/'src/field_common_event_projection.h',ROOT/'src/field_loot_flow.h'):
        snapshots[path]=path.read_bytes()
    docs=json.loads(snapshots[ROOT/'data/static/flows/common_event.json'])
    actual=next((f for f in docs['flows'] if f['id']==definition),None)
    if not actual:raise ValueError('callback51 definition missing')
    slot=domain['slots'][51]
    if slot['target']!=0x355f0 or slot['owner']!=0x355f0:raise ValueError('callback51 source target/owner differs')
    main=next(f for f in domain['functions'] if f['va']==0x355f0)
    tail=next(f for f in domain['functions'] if f['va']==0x352ca)
    for va,raw in ((0x35631,'6a03'),(0x35633,'e9d5fcffff')):
        if image[va:va+len(bytes.fromhex(raw))].hex()!=raw:raise ValueError('callback51 original shared-tail push differs')
    metadata=converter.audit.archive_entries(sources['FDFIELD.DAT'])[64][1]
    rows=[]
    for case in reused.cases(metadata,51):
        original_result=converter.source_trace(image,domain,51,case,{})
        if original_result['trace']!=[('loot_child',[2,-1]),('story',[3])]:
            raise ValueError('complete original callback51 trace differs')
        rows.append({'slot':51,'source_id':'dispatch:event_callback:051','definition':definition,
            'candidate_definition':definition,'resource_fixture_stage':21,'stage':21,
            'case_id':case['id'],'input':case,'source_trace':original_result['trace'],
            'source_checkpoints':original_result['checkpoints'],'source_finish':original_result['finish']})
    # 原source只执行原parent及已证child替身；actualC沿真实Loot业务，不能让旧story11作期望。
    expected=converter.cold_flow('common.callback.051',converter.programs()[51])
    report={'schema':'fd2-s5-callback51-source-v1','source_id':'dispatch:event_callback:051',
        'target_va':'0x355f0','original_CPU_executed':True,'original_vectors':len(rows),
        'definition':definition,'definition_sha256':converter.digest(converter.encode(actual).encode()),
        'expected_definition':expected,'expected_definition_sha256':converter.digest(converter.encode(expected).encode()),
        'complete_source_owners':[{k:f[k] for k in ('va','chunks')} for f in (main,tail)],
        'explicit_shared_tail':'355f0@35631 push3→35633 JMP3530d; does not execute352ca push11',
        'cases':rows,'actual_C':{'verified':False},'previous_S2_definition':'common.loot.child.2',
        'new_runtime_binding_written':False,
        'scope':'complete parent CPU and original1aa1d presentation/child fixture; realC actual Loot business/owner, side0/1/2 full/empty/cancel; not native or all timing/RNG/pixels'}
    if c_probe:
        requests=list(rows)
        # 三个mode共享所有正常case，既有C协议不改；cut=末story保失败/取消前缀。
        import copy
        guard=next(r for r in rows if r['input']['units'][6]==2)
        wire=[]
        request_modes=[(r,0,0) for r in rows]+[(guard,m,len(guard['source_checkpoints'])-1) for m in (1,2,3)]
        for index,(row,mode,cut) in enumerate(request_modes):
            c=row['input'];wire.append(' '.join(map(str,(index,definition,21,c['count'],c['caller'],c['turn'],
                c['display'],c['contribution'],mode,cut)))+' '+bytes(c['units']).hex()+' '+bytes(c['state']).hex()+' '+bytes(c['metadata'][3:51]).hex())
        import subprocess
        probe=Path(c_probe);before=converter.digest(probe.read_bytes())
        process=subprocess.run([str(probe.resolve()),str((Path(original)/'FDFIELD.DAT').resolve())],
            input='\n'.join(wire)+'\n',capture_output=True,text=True,timeout=180)
        if converter.digest(probe.read_bytes())!=before:raise ValueError('actualC probe changed')
        observed=[json.loads(line) for line in process.stdout.splitlines()] if process.returncode==0 else []
        error=None
        if len(observed)!=len(request_modes):error='actual C missing/extra/failed requests'
        else:
            for index,((row,mode,cut),actual_c) in enumerate(zip(request_modes,observed)):
                checkpoints=row['source_checkpoints'] if not mode else row['source_checkpoints'][:cut+1]
                points=copy.deepcopy(actual_c.get('checkpoints',[]))
                for point,expected_point in zip(points,checkpoints):point['args']=point['args'][:len(expected_point['args'])]
                finish=row['source_finish'] if not mode else {k:v for k,v in checkpoints[-1].items() if k not in ('op','args')}
                if (actual_c.get('case_id')!=index or actual_c.get('status')!=0 or actual_c.get('begin_result')!=0 or
                        points!=checkpoints or actual_c.get('finish')!=finish or
                        (not mode and (actual_c.get('active') or not actual_c.get('repeat_ack_guard'))) or
                        (mode and not actual_c.get('guarded'))):
                    error=f'actual C prefix/owner differs at case{index}/mode{mode}'
                    report['actual_C_first_difference']={'expected':checkpoints,'actual':actual_c};break
        report['actual_C']={'verified':error is None,'probe':str(probe),'probe_sha256':before,
            'normal_vectors':len(rows),'wrong_ACK_HOST_error_vectors':3,'error':error,
            'raw_stdout_sha256':converter.digest(process.stdout.encode()),'raw_stderr':process.stderr}
    report['inputs_sha256']={str(path):converter.digest(raw) for path,raw in snapshots.items()}
    if any(path.read_bytes()!=raw for path,raw in snapshots.items()):raise ValueError('source/proposed definition inputs changed')
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('original','ledger','domain','image','output-dir'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--definition',default='common.callback.051');p.add_argument('--c-probe',type=Path)
    a=p.parse_args();output=validate_output(a.output_dir,a.original)
    report=build(a.original,a.ledger,a.domain,a.image,a.definition,a.c_probe)
    output.mkdir(parents=True);(output/'comparison.json').write_text(converter.encode(report),encoding='utf8')
    print(json.dumps({'original_vectors':report['original_vectors'],'actual_C':report['actual_C']}))
    return int(a.c_probe is not None and not report['actual_C']['verified'])


if __name__=='__main__':sys.exit(main())
