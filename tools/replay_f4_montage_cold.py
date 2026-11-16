"""Retained original montage→actual bounded formal cold pair, no guest execution.

Probe reads only first/second/scene/RNG scalars; old commands are never C stdin.
The original40/3596/2779 payload and complete source remain independent expected.
"""
from __future__ import annotations
import argparse,json,struct,subprocess,sys,time
from audit_g4_render_source import archive_entries
from pathlib import Path
import replay_f4_montage as retained

FLOW='stage.cinematic.ending';ROUND='stage.cinematic.montage_round'
EXTRA={'command','flow','node'}
LEGACY={'status','hp','rng','yes','pixels_hex'}
OPS={'o':'cin_montage_open','i':'cin_montage_intro','f':'cin_montage_intro_finish',
 'h':'cin_montage_half','r':'branch_cin_montage_rng','p':'unit_field_write','t':'cin_montage_hit',
 'c':'cin_montage_cue','d':'cin_montage_frame','q':'cin_montage_close'}

def bindings(document):
    result={}
    for flow in document['flows']:
        if flow['id']not in(FLOW,ROUND):continue
        nodes={}
        for node in flow['nodes']:
            if node['id']in nodes:raise ValueError('duplicate formal coldnode')
            nodes[node['id']]=node
        result[flow['id']]=nodes
    if set(result)!={FLOW,ROUND}:raise ValueError('missing formal montage flows')
    return result

def check_binding(row,nodes):
    if set(row)!=LEGACY|EXTRA or row['flow']not in nodes or row['node']not in nodes[row['flow']]:
        raise ValueError('actual cold row/source binding differs')
    words=row['command'].split();op=words[0];node=nodes[row['flow']][row['node']]
    if op not in OPS or node['op']!=OPS[op]:raise ValueError('actual cold op/binding differs')
    args=node.get('args',[]);values=[int(x)for x in words[1:]]
    if op=='o':same=values==args[:3]and len(values)==3
    elif op in('i','h','c','d'):same=values==args[:1]and len(values)==1
    elif op=='r':same=args==[3]and len(values)==1
    elif op=='p':same=len(values)==2 and args==[values[0],values[0],18,0,values[1]]
    else:same=not values and not args
    if not same:raise ValueError('actual cold formal argument binding differs')

def cue_parts(figani,actors):
    result={}
    for half,actor in enumerate(actors):
        entry=3*actor+1;raw=figani[entry];count=raw[0]
        if count>22 or len(raw)<8+4*count:raise ValueError('cue metadata original clip boundary')
        parts=[]
        for part in range(count):
            at=struct.unpack_from('<I',raw,8+4*part)[0]
            end=struct.unpack_from('<I',raw,12+4*part)[0]if part+1<count else len(raw)
            if not 8+4*count<=at<end<=len(raw) or at+13>end:raise ValueError('cue metadata original part span')
            parts.append({'entry':entry,'cue':raw[at+5]})
        result[half]=parts
    return result

def check_cue_audit(audit,rows,metadata,nodes):
    if not isinstance(audit,list)or len(audit)>66:raise ValueError('cue API audit count/protocol')
    source_cues={i for i,row in enumerate(rows)if row['command'].startswith('c ')};observed=set();zero=0;previous=-1;seen=set()
    for call in audit:
        keys={'event_before','half','part','entry','cue','flow','node','rng_before','rng_after',
            'clock_before','clock_after','hp_before','hp_after'}
        if set(call)!=keys or any(type(call[k])is not int for k in ('event_before','half','part','entry','cue','rng_before','rng_after','clock_before','clock_after')):
            raise ValueError('cue API audit fields')
        if any(not isinstance(call[k],list)or len(call[k])!=2 or any(type(v)is not int for v in call[k])for k in ('hp_before','hp_after')):raise ValueError('cue API HP protocol')
        index=call['event_before'];half=call['half'];part=call['part']
        key=(index,half,part,call['flow'],call['node'])
        if key in seen:raise ValueError('duplicate cue API audit')
        seen.add(key)
        if not previous<=index<len(rows)or half not in metadata or not 0<=part<len(metadata[half]):raise ValueError('cue API audit order/domain')
        previous=index
        prefix=rows[:index];actual_half=next((int(r['command'].split()[1])for r in reversed(prefix)if r['command'].startswith('h ')),None)
        last=prefix[-1]if prefix else {'hp':[0,0],'rng':0}
        clocks=3*sum(r['command'].startswith('d ')for r in prefix)
        if actual_half!=half or metadata[half][part]!={'entry':call['entry'],'cue':call['cue']} or \
            call['hp_before']!=last['hp']or call['hp_after']!=last['hp']or call['rng_before']!=last['rng']or call['rng_after']!=last['rng']or \
            call['clock_before']!=clocks or call['clock_after']!=clocks:
            raise ValueError('cue API raw+5 or pure HP/RNG/clock prefix differs')
        node=nodes.get(call['flow'],{}).get(call['node'])
        if node is None or node['op']!='cin_montage_cue'or node.get('args')!=[part]:raise ValueError('cue API formal binding')
        if call['cue']:
            row=rows[index]
            if index in observed or row['command']!=f'c {part}'or row['flow']!=call['flow']or row['node']!=call['node']:
                raise ValueError('nonzero cue lacks exact original PC event')
            observed.add(index)
        else:zero+=1
    if observed!=source_cues:raise ValueError('unaccounted original cue PC event')
    return {'API_calls':len(audit),'source_PC_events':len(observed),'pure_zero_cue_queries':zero}

def compare_stream(actual,events,case,nodes,metadata):
    first=last=None;count=frames=0;rows=[]
    for event in events:
        line=actual.readline()
        if not line:raise ValueError('cold C omitted original event')
        row=retained.unique(line);check_binding(row,nodes)
        if row['command']!=event['command']:raise ValueError('cold C command/source PC order differs')
        retained.compare({k:row[k]for k in LEGACY},event)
        rows.append({k:v for k,v in row.items()if k!='pixels_hex'})
        first=first or row;last=row;count+=1;frames+=int('pixels'in event)
    line=actual.readline()
    if not line:raise ValueError('cold C omitted boundary/clock summary')
    summary=retained.unique(line)
    keys={'kind','status','rows','rng','wait_nodes','wait_total','clock_reads','uploads','presents','delay_calls',
        'entry','close','flow_nodes','only_close_next_cut','whole_ending_claimed','cue_audit'}
    if set(summary)!=keys or summary['kind']!='summary' or summary['status']!=0 or \
       any(type(summary[k])is not int for k in ('status','rows','rng','wait_nodes','wait_total','clock_reads','uploads','presents','delay_calls','flow_nodes')):
        raise ValueError('cold C boundary/clock summary protocol differs')
    calls=case['BIOS_calls']
    if summary['rows']!=count or summary['rng']!=2 or summary['wait_nodes']!=calls or summary['wait_total']!=calls or \
       summary['clock_reads']!=3*calls or summary['uploads']!=frames+64 or summary['presents'] or summary['delay_calls']!=64 or \
       summary['flow_nodes']!=len(nodes[FLOW]) or summary['only_close_next_cut']is not True or summary['whole_ending_claimed']is not False or \
       summary['entry']!=first['node'] or summary['close']!=last['node']:
        raise ValueError('cold C source wait/upload/boundary contract differs')
    check_cue_audit(summary['cue_audit'],rows,metadata,nodes)
    if actual.read(1):raise ValueError('cold C extra output after original events and boundary')
    return summary

def replay(args):
    raw,manifest_fact=retained.frozen(args.manifest,args.manifest_sha256);manifest=retained.unique(raw)
    if manifest['schema']!=retained.SCHEMA or manifest['receipt_sha256']!=retained.RECEIPT_SHA or \
        manifest['frame_count']!=2779 or len(manifest['cases'])!=40 or manifest['retained']!=str(args.retained.resolve()):
        raise ValueError('retained original manifest qualification differs')
    if not retained.late(manifest['files'])['unchanged']:raise ValueError('retained old artifact/source changed')
    receipt,cases,image,facts=retained.source_inputs(args)
    minimum={'s5_stage_source_test.c','f4_montage_cold_probe.h','scene.c','scene_cinematic.c',
        'scene_cinematic_flow.h','battle_animation.c','field_game.c','static_flow.h'}
    if not minimum<={Path(p).name for p in args.source}:raise ValueError('missing actual producer source closure minimum')
    for path in [Path(__file__),args.probe,args.build_receipt,*args.source]:facts.append(retained.frozen(path)[1])
    facts.append(manifest_fact)
    cold_raw,cold_fact=retained.frozen(args.root/'data/static/flows/stage.json');facts.append(cold_fact)
    node_bindings=bindings(retained.unique(cold_raw))
    fig_raw,fig_fact=retained.frozen(args.root/'original_game/FIGANI.DAT');facts.append(fig_fact)
    facts.append(retained.frozen(archive_entries.__code__.co_filename)[1]);figani=archive_entries(fig_raw)
    out=retained.output_path(args);out.mkdir(parents=True,exist_ok=False)
    report={'schema':'fd2-f4-montage-formal-cold-retained-v1','ok':False,'cases':[],
        'original_execution_this_run':False,'GUI_or_build_this_run':False,
        'scope':'all20 original montage pair bindings/two RNG branch classes, real ending segment and shared montage_round; explicit entry and oneclose-next cutoff',
        'limits':['not entire ending/credits, natural arrival, caller HP seed/QK qualification or shared runtime RNG trajectory',
            'NS clock/present are controlled test TU adapters; all actual source tick waits execute, no physical PIT/GUI/audio claim'],
        'source_before':facts,'formal_cold':cold_fact}
    pinned={f['path']:f for f in manifest['files']};first_error=None;active=None
    try:
        for case in cases:
            values,current,directory=retained.read_case(args.retained,case,pinned)
            events=list(retained.case_events(*values,case,image))
            original_stdin=('\n'.join(e['command']for e in events)+'\n').encode('ascii')
            pin=manifest['cases'][len(report['cases'])]
            if retained.digest(original_stdin)!=pin['canonical_stdin_sha256']:raise ValueError('original command/event pin changed')
            first,second=case['actors'];scene=case['scene'];rng=case['rng_value']
            scalars=f'{first} {second} {scene} {rng}\n'.encode('ascii')
            work=out/directory.name;work.mkdir();active=(work,current)
            with (work/'stdin.txt').open('xb')as stream:stream.write(scalars)
            guard=retained.late(facts+current)
            if not guard['unchanged']:raise ValueError('cold pre-generation input/source guard changed')
            argv=[str(args.probe.resolve()),'--f4-montage-cold'];start=time.time_ns()
            retained.dump(work/'generation-start.json',{'argv':argv,'cwd':str(args.root.resolve()),'stdin_sha256':retained.digest(scalars),
                'original_commands_are_C_stdin':False,'started_ns':start,'before':guard})
            try:
                with (work/'actual-C.jsonl').open('xb')as stdout,(work/'actual-C.stderr.txt').open('xb')as stderr:
                    child=subprocess.run(argv,input=scalars,stdout=stdout,stderr=stderr,cwd=args.root,timeout=60,
                        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32'else 0)
            except Exception as exc:
                retained.dump(work/'generation-failure.json',{'argv':argv,'ended_ns':time.time_ns(),'error':str(exc)})
                raise
            retained.dump(work/'generation.json',{'argv':argv,'cwd':str(args.root.resolve()),'stdin_sha256':retained.digest(scalars),
                'started_ns':start,'ended_ns':time.time_ns(),'exit_code':child.returncode,'before':guard})
            if child.returncode or (work/'actual-C.stderr.txt').stat().st_size:
                raise ValueError(f'cold actual C failed: {directory.name}, exit{child.returncode}')
            _,actual_fact=retained.frozen(work/'actual-C.jsonl')
            with (work/'actual-C.jsonl').open('rb')as actual:summary=compare_stream(actual,events,case,node_bindings,cue_parts(figani,case['actors']))
            after=retained.late(facts+current+[actual_fact]);retained.dump(work/'after.json',after)
            if not after['unchanged']:raise ValueError('cold post-generation source/binary/output guard changed')
            report['cases'].append({'pair':case['pair'],'rng':rng,'commands':len(events),'frames':case['full_frames'],
                'ok':True,'actual':actual_fact,'boundary':summary});active=None
            print(json.dumps({'pair':case['pair'],'rng':rng,'formal_cold_case_passed':True}),flush=True)
    except Exception as exc:
        first_error={'type':type(exc).__name__,'message':str(exc)}
        if active:
            work,current=active
            try:retained.dump(work/'failure-after.json',{'primary_error':first_error,'source_guard':retained.late(facts+current)})
            except Exception as diagnostic:report.setdefault('secondary_errors',[]).append({'phase':'failure-after','error':str(diagnostic)})
    final=retained.late(facts+manifest['files']);report['source_after']=final
    total=sum(c['frames']for c in report['cases']);commands=sum(c['commands']for c in report['cases'])
    if first_error is None and(len(report['cases'])!=40 or total!=2779 or commands!=3596):
        first_error={'type':'ValueError','message':'formal cold40/3596/2779 denominator differs'}
    if first_error is None and not final['unchanged']:first_error={'type':'ValueError','message':'late cold source/binary/original guard changed'}
    report.update(ok=first_error is None,error=first_error,case_count=len(report['cases']),command_count=commands,full_frame_count=total,
        whole_ending_or_credits_qualified=False)
    try:retained.dump(out/'comparison.json',report)
    except Exception as diagnostic:
        if first_error:raise RuntimeError(str(first_error)+'; report write also failed: '+str(diagnostic)) from diagnostic
        raise
    return report

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','retained','image','readset','manifest','probe','build-receipt','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--manifest-sha256',required=True);parser.add_argument('--output-root',type=Path)
    parser.add_argument('--source',type=Path,action='append',required=True)
    args=parser.parse_args(argv)
    try:result=replay(args)
    except Exception as exc:
        print(json.dumps({'ok':False,'error':{'type':type(exc).__name__,'message':str(exc)}},ensure_ascii=False))
        print('电影冷流程核验失败：'+str(exc)+'。保留本轮产物，检查首错与来源。',file=sys.stderr);return 1
    print(json.dumps({k:result[k]for k in('ok','case_count','command_count','full_frame_count','error')},ensure_ascii=False))
    return int(not result['ok'])
if __name__=='__main__':raise SystemExit(main())
