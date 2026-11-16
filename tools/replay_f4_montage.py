"""Replay retained S5 montage original frames against the current C CLI only.

index validates/pins old evidence; replay runs --montage-vectors. No original
execution, Unicorn, GUI, build, coldgraph expected, or new decoder is used.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
import zlib

ROOT=Path(__file__).resolve().parents[1]
if not (ROOT/'tools/ida/verify_s5_montage_frames.py').is_file():ROOT=Path.cwd()
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/ida'))
import verify_s5_montage_frames as legacy
from verify_f4_resource_pixels import checked_output

RECEIPT_SHA='688b62ed183e0c6c207f29eff3903efe663ae4e3916194756cfe535797cf5c50'
READSET_SHA='4cd565bc6e1c245ab5e6bc1d7d2ee45901c47b987bcf92490d4378c184d02a46'
LEGACY_LF_SHA='328e1b503fe8c62327c9e515fe63d4c0d7370105b6ae48627ca39031500e847e'
SCHEMA='fd2-f4-retained-montage-index-v1'
PAGE=64000

def digest(raw):return hashlib.sha256(raw).hexdigest()
def frozen(path,expected=None):
    path=Path(path).resolve();s=path.stat();raw=path.read_bytes();t=path.stat()
    fact=dict(path=str(path),sha256=digest(raw),size=len(raw),mtime_ns=s.st_mtime_ns)
    if (s.st_size,s.st_mtime_ns)!=(t.st_size,t.st_mtime_ns) or (expected and fact['sha256']!=expected):
        raise ValueError(f'input identity mismatch: {path}')
    return raw,fact
def late(facts):
    after,errors=[],[]
    for old in facts:
        try:
            _,new=frozen(old['path']);after.append(new)
            if new!=old:errors.append(dict(path=old['path'],error='late SHA/size/mtime mismatch'))
        except (OSError,ValueError) as exc:
            after.append(dict(path=old['path'],available=False));errors.append(dict(path=old['path'],error=str(exc)))
    return dict(before=facts,after=after,unchanged=not errors,errors=errors)
def dump(path,value):
    with path.open('x',encoding='utf8') as stream:json.dump(value,stream,ensure_ascii=False,indent=2)
def unique(raw):return json.loads(raw,object_pairs_hook=legacy.unique_keys)

def output_path(args):
    output=checked_output(args.root,args.out,args.output_root)
    retained=args.retained.resolve()
    if output==retained or output.is_relative_to(retained) or retained.is_relative_to(output):
        raise ValueError('new output must not contain or enter retained evidence')
    return output

def frame(packed):
    decoder=zlib.decompressobj();raw=decoder.decompress(packed,PAGE+1)
    if len(raw)!=PAGE or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('retained frame compressed/exact64000 extent mismatch')
    return raw

def case_events(commands,raw_events,packed,case,image):
    events=unique(raw_events);lines=commands.decode('ascii').splitlines()
    if not events or lines!=[e.get('command') for e in events]:raise ValueError('retained command/event order or count differs')
    if commands.decode('ascii') not in ('\n'.join(lines)+'\n','\r\n'.join(lines)+'\r\n'):
        raise ValueError('retained command line endings or trailing data differ')
    pair,rng=case['pair'],case['rng_value'];actors=[image[0x526e9+pair],image[0x526fd+pair]];scene=image[0x52711+pair]
    if case['actors']!=actors or case['scene']!=scene or lines[0]!=f'o {actors[0]} {actors[1]} {scene}' or lines[-1]!='q':
        raise ValueError('retained pair/source table or open/close differs')
    pcs={'o':{0x31aee},'i':{0x2ea3d,0x2eb19},'f':{0x2e6bf},'h':{0x2ebe1},'r':{0x4ebe3},
         'p':{0x2ef9e},'t':{0x2efc8},'c':{0x25a96},'d':{0x2f0c6},'q':{0x2e763}}
    hp=[0,0];calls=0;at=0;frames=0;halves=[]
    for index,event in enumerate(events):
        command=event['command'];words=command.split(' ');op=words[0]
        if op not in pcs or event['source_pc'] not in pcs[op]:raise ValueError('retained source PC/operation differs')
        arity={'o':4,'i':2,'f':1,'h':2,'r':2,'p':3,'t':1,'c':2,'d':2,'q':1}[op]
        if len(words)!=arity or any(not word.isdecimal() for word in words[1:]):raise ValueError('retained command grammar differs')
        if op=='o' and index!=0 or op=='q' and index!=len(events)-1:raise ValueError('extra retained open/close')
        if op=='h':halves.append(int(words[1]))
        if op=='r':
            if int(words[1])!=rng:raise ValueError('retained RNG command differs')
            calls+=1
        if op=='p':
            actor,value=map(int,words[1:])
            if actor not in (0,1) or value!=63680:raise ValueError('retained explicit HP value differs')
            hp[actor]=value
        if event['hp']!=hp or any(type(v)is not int for v in event['hp']) or type(event['rng'])is not int or event['rng']!=calls:
            raise ValueError('retained HP/RNG prefix differs')
        if type(event['yes'])is not int or event['yes']!=(int(rng%100<3) if op=='r' else -1):raise ValueError('retained branch differs')
        common={'command','source_pc','hp','rng','yes'}
        if op in ('i','d'):
            if set(event)!=common|{'frame_offset','frame_size','pixels_sha256'}:raise ValueError('retained frame row keys differ')
            size=event['frame_size']
            if type(size)is not int or size<=0 or type(event['frame_offset'])is not int or event['frame_offset']!=at or at+size>len(packed):raise ValueError('retained frame span missing/overlap')
            pixels=frame(packed[at:at+size]);at+=size;frames+=1
            if digest(pixels)!=event['pixels_sha256']:raise ValueError('retained frame raw SHA differs')
            event={**event,'pixels':pixels}
        elif set(event)!=common:raise ValueError('retained nonframe row has extras')
        yield event
    if at!=len(packed) or frames!=case['full_frames'] or len(events)!=case['commands'] or calls!=2 or halves!=[0,1]:
        raise ValueError('retained full case/packed frame denominator differs')

def compare(actual,event):
    # Reuse the old independently qualified exact status/HP/RNG/branch/full-page
    # comparator only. legacy.main/execute are never called or selected.
    legacy.validate_row(actual,{**event,'pixels':event.get('pixels',b'')})

def compare_stream(actual,events,label):
    for number,event in enumerate(events):
        line=actual.readline()
        if not line:raise ValueError(f'{label} command{number}: C omitted row')
        try:compare(unique(line),event)
        except (ValueError,KeyError,TypeError) as exc:
            raise ValueError(f"{label} command{number} {event['command']}: {exc}") from exc
    if actual.read(1):raise ValueError(f'{label}: actual C has extra/unmatched output')

def read_case(base,case,manifest_files=None):
    directory=base/f"pair{case['pair']:02d}-rng{case['rng_value']}"
    values=[];facts=[]
    for name in ('commands.txt','original-events.json','original-frames.zbin'):
        raw,info=frozen(directory/name)
        if manifest_files is not None and info!=manifest_files[str((directory/name).resolve())]:raise ValueError('retained case pin changed')
        values.append(raw);facts.append(info)
    return values,facts,directory

def source_inputs(args):
    facts=[]
    def get(path,expected=None):
        raw,info=frozen(path,expected);facts.append(info);return raw
    get(__file__);script=get(legacy.__file__)
    if digest(script.replace(b'\r\n',b'\n'))!=LEGACY_LF_SHA:raise ValueError('legacy validator/source normalised bytes differ')
    get(checked_output.__code__.co_filename)
    get(legacy.unique_keys.__code__.co_filename)
    get(ROOT/'tools/ida/verify_s5_cinematic.py')
    receipt=unique(get(args.retained/'comparison.json',RECEIPT_SHA))
    if not receipt['ok'] or not receipt['sources_unchanged'] or receipt['case_count']!=40 or receipt['full_frame_count']!=2779 or receipt['probe_before_sha256']!=receipt['probe_after_sha256']:
        raise ValueError('legacy comparison qualification differs')
    image=get(args.image,legacy.IMAGE_SHA);readset=unique(get(args.readset,READSET_SHA));legacy.validate_source(image,readset)
    for name in ('FIGANI','TAI','BG','FDOTHER'):
        source=args.root/'original_game'/(name+'.DAT')
        old=[v for k,v in receipt['input_sha256'].items() if k.replace('\\','/').endswith('/original_game/'+name+'.DAT')]
        if len(old)!=1:raise ValueError('old resource pin missing')
        get(source,old[0])
    cases=receipt['cases']
    if [(c['pair'],c['rng_value']) for c in cases]!=[(p,r) for p in range(20) for r in (0,50)]:raise ValueError('exact40 ordered pair/RNG set required')
    if any(not c['ok'] or c['rng_calls']!=2 or c['function_entries']!=2 for c in cases):raise ValueError('old case qualification missing')
    return receipt,cases,image,facts

def index(args):
    receipt,cases,image,facts=source_inputs(args);rows=[];frames=0
    for case in cases:
        values,current,directory=read_case(args.retained,case);facts+=current
        events=list(case_events(*values,case,image))
        # Independently retained old actual-C output is checked, never copied as
        # expected. No old PE or original code runs during this qualification.
        old,info=frozen(directory/'actual-C.jsonl');facts.append(info)
        compare_stream(io.BytesIO(old),events,directory.name+' old-C')
        _,info=frozen(directory/'actual-C.stderr.txt');facts.append(info)
        rows.append({**case,'canonical_stdin_sha256':digest(('\n'.join(e['command'] for e in events)+'\n').encode('ascii'))})
        frames+=sum('pixels'in e for e in events)
    if len(rows)!=40 or frames!=2779:raise ValueError('retained40/2779 final denominator differs')
    identity=late(facts)
    if not identity['unchanged']:raise ValueError('retained index late input guard differs')
    output=output_path(args);output.mkdir(parents=True,exist_ok=False)
    manifest=dict(schema=SCHEMA,retained=str(args.retained.resolve()),receipt_sha256=RECEIPT_SHA,
        cases=rows,frame_count=frames,files=facts,identity=identity,
        legacy_generator_lf_sha256=LEGACY_LF_SHA,original_execution_this_run=False,C_execution_this_run=False,
        limits=receipt['limits'],scope='retained original command/events/frames; not a new original execution or production coldwalker')
    dump(output/'manifest.json',manifest);_,pin=frozen(output/'manifest.json')
    return dict(ok=True,manifest=pin,cases=40,frames=frames,original_execution_this_run=False,C_execution_this_run=False)

def replay(args):
    raw,manifest_fact=frozen(args.manifest,args.manifest_sha256);manifest=unique(raw)
    if manifest['schema']!=SCHEMA or manifest['receipt_sha256']!=RECEIPT_SHA or manifest['frame_count']!=2779 or len(manifest['cases'])!=40:
        raise ValueError('retained manifest schema/denominator differs')
    if str(args.retained.resolve())!=manifest['retained']:raise ValueError('retained root differs from pinned manifest')
    stable=late(manifest['files'])
    if not stable['unchanged']:raise ValueError('retained manifest input pin differs')
    receipt,cases,image,common=source_inputs(args)
    for path in [args.probe,args.build_receipt,*args.source]:
        _,info=frozen(path);common.append(info)
    common.append(manifest_fact)
    output=output_path(args);output.mkdir(parents=True,exist_ok=False)
    report=dict(schema='fd2-f4-montage-retained-currentC-v1',ok=False,cases=[],source_before=common,
        original_execution_this_run=False,build_or_GUI_this_run=False,limits=receipt['limits'],
        coldwalker_or_natural_owner_qualified=False)
    first=None;files={f['path']:f for f in manifest['files']};active=None
    try:
        for case in cases:
            values,current,directory=read_case(args.retained,case,files)
            events=list(case_events(*values,case,image));commands=('\n'.join(e['command'] for e in events)+'\n').encode('ascii')
            pinned=manifest['cases'][len(report['cases'])]
            if digest(commands)!=pinned['canonical_stdin_sha256']:raise ValueError('canonical stdin pin differs')
            work=output/directory.name;work.mkdir()
            with (work/'stdin.txt').open('xb') as stdin:stdin.write(commands)
            active=(work,current)
            before=late(common+current)
            if not before['unchanged']:raise ValueError('pre-generation source/input guard differs')
            argv=[str(args.probe.resolve()),'--montage-vectors'];started=time.time_ns()
            dump(work/'generation-start.json',dict(argv=argv,cwd=str(args.root.resolve()),stdin_sha256=digest(commands),started_ns=started,before=before))
            try:
                with (work/'actual-C.jsonl').open('xb') as stdout,(work/'actual-C.stderr.txt').open('xb') as stderr:
                    child=subprocess.run(argv,input=commands,stdout=stdout,stderr=stderr,cwd=args.root,timeout=60,
                        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            except Exception as exc:
                dump(work/'generation-failure.json',dict(argv=argv,ended_ns=time.time_ns(),error=str(exc)))
                raise
            dump(work/'generation.json',dict(argv=argv,cwd=str(args.root.resolve()),stdin_sha256=digest(commands),started_ns=started,ended_ns=time.time_ns(),exit_code=child.returncode,before=before))
            if child.returncode:raise ValueError(f'actual C exit {child.returncode}: {directory.name}')
            _,actual_fact=frozen(work/'actual-C.jsonl')
            with (work/'actual-C.jsonl').open('rb') as actual:
                compare_stream(actual,events,directory.name)
            after=late(common+current+[actual_fact]);dump(work/'after.json',after)
            if not after['unchanged']:raise ValueError('post-generation source/binary/actual guard differs')
            report['cases'].append(dict(pair=case['pair'],rng=case['rng_value'],commands=len(events),frames=case['full_frames'],ok=True,actual=actual_fact))
            active=None
            print(json.dumps(dict(pair=case['pair'],rng=case['rng_value'],current_C_case_passed=True)),flush=True)
    except Exception as exc:
        first=dict(type=type(exc).__name__,message=str(exc))
        if active:
            work,current=active;created=[];read_errors=[]
            for name in ('actual-C.jsonl','actual-C.stderr.txt'):
                try:
                    if (work/name).is_file():created.append(frozen(work/name)[1])
                except (OSError,ValueError) as error:read_errors.append(str(error))
            dump(work/'failure-after.json',dict(primary_error=first,source_guard=late(common+current+created),output_read_errors=read_errors))
    final=late(common+manifest['files']);report['source_after']=final
    if first is None and (len(report['cases'])!=40 or sum(c['frames'] for c in report['cases'])!=2779):
        first=dict(type='ValueError',message='actual C40/2779 final denominator differs')
    report.update(ok=first is None and final['unchanged'],error=first,
        case_count=len(report['cases']),full_frame_count=sum(c['frames'] for c in report['cases']))
    if first is None and not final['unchanged']:report['error']=dict(type='ValueError',message='late source/binary/retained guard differs')
    dump(output/'comparison.json',report)
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['index','replay'])
    for name in ['root','retained','image','readset','out']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--output-root',type=Path)
    parser.add_argument('--manifest',type=Path);parser.add_argument('--manifest-sha256')
    parser.add_argument('--probe',type=Path);parser.add_argument('--build-receipt',type=Path)
    parser.add_argument('--source',type=Path,action='append',default=[])
    args=parser.parse_args()
    if args.mode=='replay' and not (args.manifest and args.manifest_sha256 and args.probe and args.build_receipt and args.source):parser.error('replay requires pinned manifest, exact probe/build receipt and source snapshots')
    try:
        result=index(args) if args.mode=='index' else replay(args)
        print(json.dumps(result,ensure_ascii=False,indent=2));return int(not result['ok'])
    except Exception as exc:
        print(json.dumps(dict(ok=False,error=dict(type=type(exc).__name__,message=str(exc))),ensure_ascii=False))
        print(f'保留montage核验失败：{exc}',file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
