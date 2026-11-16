"""Complete finite promotion history from retained original pixels and full source.

No original CPU/decoder, GUI or device. Derive is independent of the C renderer;
pair invokes the pinned private test CLI. Natural entry/initial palette, unknown
empty idle and hardware timing remain separate qualifications.
"""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import threading
import zlib
import verify_f4_physical_composition as core

sha,require,freeze,guards=core.sha,core.require,core.freeze,core.guards
EXTRA_SHA='d4382319728dd6ea685b99e3e2950cf486be68c9cc5d9f6c4fe7e2cf8e4a316d'
BG_SHA='b9fc21d019d6256a4bb7e6da1cefcb0bfe331d8ff74a52a8201570afc98b56de'
SEQ=[[0,1],[1,9],[2,16],[3,20],[4,200],[5,24],[6,0]]
SOURCES=[*core.SOURCES,'src/tests/f4_promotion_composition_probe.h',
    'src/field_prepare.c','src/field_prepare_service_present.h',
    'data/static/tables/preparation.json','data/static/flows/preparation.json','src/CMakeLists.txt']
SCOPE='Source-defined promotion pairs: whole initialized 128000B work, VGA/base/DAC at every source upload; exact wait order, restore and unchanged metadata.'
EXCLUDED='No natural promotion/first palette/shared RNG/absolute PIC or PCM. Empty new idle48 stops before its first undefined sprite read; no synthetic original frame after it.'

def payload(parent,spec,facts,size):
    p=(parent/spec['path']).resolve();require(p.parent==parent,'payload containment')
    return core.unpack(freeze(p,facts,spec['sha256']),size)

def blit(work,meta,pm,x=0,y=0):
    image,mask=pm;w,h=meta['width'],meta['height'];x+=meta.get('x',0);y+=meta.get('y',0)
    require(0<=x and x+w<=640 and 0<=y and y+h<=200,'true initialized work placement')
    for row in range(h):
        for col in range(w):
            src=row*w+col
            if mask[src]:work[(y+row)*640+x+col]=image[src]

def dac(base,amount,white=False):
    return bytes(((v<<2)|(v>>4)) for c in base
        for v in [min(63,c+amount) if white else max(0,c-amount)])

def history(old,new,last_phase,parts,pm,bgs):
    """Literal 2fb2c/2fe14 order; no calls to the C implementation or cold engine."""
    base=bytes(i*7%64 for i in range(768));saved=core.seed(64000,13,320,31)
    frame=saved;work=bytearray(128000);ticks=delay=pumps=0
    def point(phase,colors):
        nonlocal pumps
        pumps+=1
        return dict(phase=phase,ticks=ticks,delays=delay,pumps=pumps,work_bytes=128000 if phase else 0),\
            (bytes(work) if phase else b'')+frame+base+colors
    def draw(unit,part,bg,x=0):
        nonlocal work,frame
        work=bytearray(128000);blit(work,dict(width=320,height=100),bgs[bg],0,50)
        key=f'figani:{unit*3}:{part}';blit(work,parts[key],pm[key],x)
        frame=b''.join(work[y*640:y*640+320] for y in range(200))
    colors=b''
    for amount in range(64):
        colors=dac(base,amount);yield point(0,colors);delay+=2
    ticks+=1
    for i in range(8,-1,-1):
        draw(old,0,(9-i)%3,i*10);colors=dac(base,6*i);yield point(1,colors)
    for phase,unit,count in [(2,old,16),(5,new,24)]:
        if phase==5:
            for step in range(20):
                colors=dac(base,step*3,True);yield point(3,colors);delay+=10
            if last_phase==3:return
            draw(new,0,0,200);colors=dac(base,0);yield point(4,colors)
        part=age=0
        for i in range(count):
            draw(unit,part,(i+1)%3);yield point(phase,colors)
            age+=1
            if age==parts[f'figani:{unit*3}:{part}']['duration']:
                age=0;part+=1
                if f'figani:{unit*3}:{part}' not in parts:part=0
            ticks+=1
    for amount in range(64):
        colors=dac(base,amount);yield point(6,colors);delay+=2
    frame=saved
    for amount in range(64,-1,-1):
        colors=dac(base,amount);yield point(6,colors);delay+=2

def derive(a,root,facts):
    raw,parts,eligible,results,join=core.read_inputs(a,facts)
    action=json.loads(raw['action']);source=json.loads(raw['source'])
    contract=[core.source_check(raw['image'],source,0x2fb2c)]
    idle=next(f for f in action['functions'].values() if f['va']==0x2fe14)
    require(idle['strict_continuous_byte_coverage'],'complete retained idle source')
    for chunk in idle['chunks']:
        require(sha(raw['image'][chunk['start']:chunk['end']])==chunk['sha256'],'strict idle chunk')
    contract.append(dict(va=idle['va'],chunks=idle['chunks'],source=idle['source'],source_sha256=idle['source_sha256']))
    bg=freeze(a.bg,facts,BG_SHA);extra=json.loads(freeze(a.extra,facts,EXTRA_SHA));bgs=[]
    require(extra['ok'] and extra['guards_unchanged'],'retained original BG qualification')
    offsets=struct.unpack_from('<4I',bg,6)
    for i in range(3):
        row=next(r for r in extra['cases'] if r['request']==['scene','bg',i,0,0])
        require(row['match'] and [row['width'],row['height']]==[320,100] and
            sha(bg[offsets[i]:offsets[i+1]])==row['source_sha256'],'same complete BG source span')
        data=payload(a.extra.resolve().parent,row['original_payload'],facts,65536)
        require(data[32000:32768]==bytes([165])*768 and not any(data[-768:]),'BG palette unchanged')
        mask=data[32768:64768];require(set(mask)<={0,1},'BG mask')
        bgs.append((data[:32000],mask))
    # Independent source table; sentinel255 can never be a visible prefix item.
    special=list(raw['image'][0x523d5:0x523e7]);require(len(special)==18,'source special table')
    bindings=[(old,old+32) for old in range(18) if old!=7]
    bindings += [(old,old+50) for old in range(18) if old!=7 and special[old]!=255]
    bindings += [(9,52)];require(len(bindings)==32 and len(set(bindings))==32,'source pair denominator')
    cache={};rows=[];out=core.fresh(root,a.output)
    clips={c['entry']:c for c in action['figani']['clips']}
    for number,(old,new) in enumerate(bindings):
        require(not clips[old*3]['empty'],'defined old clip')
        empty=clips[new*3]['empty'];require(not empty or (old,new)==(16,48),'known empty qualification')
        for unit in ([old] if empty else [old,new]):
            for key,p in parts.items():
                if p['entry']==unit*3:core.pixels(key,a,results,parts,facts,cache)
        last=3 if empty else 6;path=f'case-{number:02d}.gz';states=[]
        with gzip.open(out/path,'xb') as sink:
            for n,(header,data) in enumerate(history(old,new,last,parts,cache,bgs)):
                sink.write(data);states.append(dict(frame=n,**header,payload_bytes=len(data),sha256=sha(data)))
        require(len(states)==(109 if empty else 263),'literal complete source frame count')
        rows.append(dict(case=number,request=[old,new,last],empty_prefix_only=empty,payload=path,
            sha256=sha((out/path).read_bytes()),states=states))
    result=dict(schema='fd2-f4-promotion-expected-v1',passed=True,cases=rows,source_contract=contract,
        inputs={**core.PINS,'extra':EXTRA_SHA,'bg':BG_SHA},binding_domain='17 ordinary +14 special-item +9→52; 16→48 prefix only',
        independent_expected='complete source order + retained original pixels and masks; no new decoder/C',
        new_original_CPU=0,actual_C=0,GUI=0,scope=SCOPE,excluded=EXCLUDED,identity=guards(facts))
    require(result['identity']['unchanged'],'late expected guard');core.save_report(out/'expected.json',result)
    print(json.dumps(dict(passed=True,expected=str(out/'expected.json'),cases=len(rows),frames=sum(len(r['states']) for r in rows))))

def compare_frame(row,case,point,actual,wanted):
    header={k:point[k] for k in ['phase','frame','ticks','delays','pumps','work_bytes']}
    require(row==dict(schema='fd2-f4-promotion-frame-v1',case=case,**header),'actual frame/clock order')
    require(len(wanted)==point['payload_bytes'] and sha(wanted)==point['sha256'],'expected state identity')
    require(len(actual)==len(wanted),'actual complete state')
    if actual!=wanted:
        at=next(i for i,(x,y) in enumerate(zip(actual,wanted)) if x!=y)
        raise ValueError(f'case{case} frame{point["frame"]} byte{at}: actual{actual[at]} expected{wanted[at]}')

def cold_sequence(raw):
    document=json.loads(raw);flow=next(f for f in document['flows'] if f['id']=='services')
    nodes={n['id']:n for n in flow['nodes']};require(len(nodes)==len(flow['nodes']),'unique cold nodes')
    require(sum(n['op']=='promotion_visual' for f in document['flows'] for n in f['nodes'])==7,
        'all seven promotion bindings')
    for i,args in enumerate(SEQ):
        node=nodes[f'promote.visual.{i}'];nxt=f'promote.visual.{i+1}' if i<6 else 'promote.return.music'
        require(node['op']=='promotion_visual' and node['args']==args and
            node['on']==dict(next=nxt,error='failure'),'cold phase args/order/error binding')
    require(nodes['promote.return.music']['op']=='music' and
        nodes['promote.return.music']['on']==dict(next='promote.identity',error='failure'),'cold suffix handoff')
    return dict(sequence=SEQ,qualification='static args/edges join only; actual test calls public phase step, not cold walker')

def resource_directory(bg,resource):
    bg,resource=bg.resolve(),resource.resolve()
    require(bg.name=='BG.DAT' and resource.name=='FIGANI.DAT' and bg.parent==resource.parent,
        'probe reads fixed BG.DAT/FIGANI.DAT from the same guarded directory')
    return bg.parent

def pair(a,root,facts):
    expected=json.loads(freeze(a.expected,facts,a.expected_sha256))
    require(expected.get('schema')=='fd2-f4-promotion-expected-v1' and expected['passed'] and
        expected['identity']['unchanged'] and len(expected['cases'])==32 and
        expected['inputs']=={**core.PINS,'extra':EXTRA_SHA,'bg':BG_SHA},'qualified expected')
    directory=resource_directory(a.bg,a.resource)
    resources={'BG.DAT':freeze(a.bg,facts,BG_SHA),'FIGANI.DAT':freeze(a.resource,facts,core.PINS['resource'])}
    producer=Path(a.producer).resolve();freeze(producer,facts,a.producer_sha256)
    freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for path in SOURCES:freeze(root/path,facts)
    cold=cold_sequence(freeze(root/'data/static/flows/preparation.json',facts))
    out=core.fresh(root,a.output);result=dict(passed=False,actual_C_frames=0,actual_C_cases=0,primary_error=None,secondary_errors=[],cold_binding=cold)
    process=reader=None;errors=[];stream_hash=hashlib.sha256()
    def read_stdout():
        try:
            with gzip.open(out/'actual.bin.gz','xb') as sink:
                result['loaded_inputs']=[]
                for name,resource in resources.items():
                    line,row=core.jsonline(process.stdout);stream_hash.update(line)
                    require(row==dict(schema='fd2-f4-input-v1',input_name=name,payload_bytes=len(resource)),'actual input receipt')
                    loaded=core.exact(process.stdout,len(resource));require(loaded==resource,'same actual loaded bytes')
                    stream_hash.update(loaded);result['loaded_inputs'].append(dict(name=name,sha256=sha(loaded),bytes=len(loaded)))
                for spec in expected['cases']:
                    parent=a.expected.resolve().parent;path=(parent/spec['payload']).resolve()
                    require(path.parent==parent,'expected containment');freeze(path,facts,spec['sha256'])
                    with gzip.open(path,'rb') as wanted:
                        for point in spec['states']:
                            line,row=core.jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                            data=core.exact(wanted,point['payload_bytes'])
                            actual=core.exact(process.stdout,len(data));stream_hash.update(actual);sink.write(actual)
                            compare_frame(row,spec['case'],point,actual,data)
                            result['actual_C_frames']+=1
                        require(not wanted.read(1),'expected exact tail')
                    line,row=core.jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                    short=spec['empty_prefix_only'];frames=109 if short else 263
                    require(row==dict(case_end=spec['case'],frames=frames,ticks=17 if short else 41,
                        delays=328 if short else 586,pumps=frames,other_owners_unchanged=True),'actual scope/owner footer')
                    result['actual_C_cases']+=1
                line,row=core.jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                require(row==dict(end=32,binding_unchanged=True) and not process.stdout.read(1),'actual final footer')
        except Exception as e:
            errors.append(str(e))
            if process.poll() is None:process.terminate()
    try:
        with (out/'stderr.txt').open('xb') as err:
            process=subprocess.Popen([str(producer),'--f4-promotion-composition',str(directory)],cwd=root,
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            reader=threading.Thread(target=read_stdout,daemon=True);reader.start()
            for spec in expected['cases']:process.stdin.write((' '.join(map(str,spec['request']))+'\n').encode('ascii'))
            process.stdin.close();code=process.wait(timeout=180);reader.join(timeout=10)
            require(not reader.is_alive(),'reader still running')
            require(not errors,errors[0] if errors else '');require(code==0,f'probe exit{code}')
        result.update(passed=True,exit_code=code,actual_stream_sha256=stream_hash.hexdigest())
    except Exception as e:result['primary_error']=errors[0] if errors else str(e)
    finally:
        if process is not None and process.stdin and not process.stdin.closed:
            try:process.stdin.close()
            except OSError as e:
                result.update(passed=False,primary_error=result['primary_error'] or 'stdin.close failed')
                result['secondary_errors'].append(dict(operation='stdin.close',error=str(e)))
        core.cleanup_probe(process,reader,result)
        result.update(identity=guards(facts),new_original_CPU=0,GUI=0,scope=SCOPE,excluded=EXCLUDED,
            source_PE_binding='separate parent build/tlog snapshot join required')
        if not result['identity']['unchanged']:result.update(passed=False,primary_error=result['primary_error'] or 'late input guard')
        core.save_report(out/'report.json',result)
        print(json.dumps({k:result[k] for k in ['passed','actual_C_frames','actual_C_cases','primary_error']}))
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    sub=p.add_subparsers(dest='mode',required=True);d=sub.add_parser('derive');q=sub.add_parser('pair')
    for key in core.PINS:d.add_argument('--'+key.replace('_','-'),required=True,type=Path)
    d.add_argument('--extra',required=True,type=Path)
    for name in ['expected','producer','build-receipt']:q.add_argument('--'+name,required=True,type=Path)
    for name in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+name,required=True)
    q.add_argument('--resource',required=True,type=Path)
    for command in [d,q]:command.add_argument('--bg',required=True,type=Path);command.add_argument('--output',required=True,type=Path)
    a=p.parse_args();facts=[]
    try:
        for path in [__file__,core.__file__,core.freeze.__code__.co_filename]:freeze(path,facts)
        derive(a,a.root.resolve(),facts) if a.mode=='derive' else pair(a,a.root.resolve(),facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,zlib.error) as e:
        print(json.dumps(dict(passed=False,error=str(e),identity=guards(facts))));raise SystemExit(1)
if __name__=='__main__':main()
