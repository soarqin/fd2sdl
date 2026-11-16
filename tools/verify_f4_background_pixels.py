"""F4共享BG：原payload/完整源派生expected；root可显式pair私有stdout probe。

derive不运行C/原CPU。pair只运行给定已构建console probe，无GUI；全流有界压缩，
不将fixed-NS叶等同live fresh-clock或首次wave malloc-before资格。
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

P,V=456*336,64000
SOURCES=['src/tests/field_unit_render_test.c','src/field_background.c','src/field_background.h',
         'src/field_page.c','src/field_page.h','src/field_page_private.h','src/archive.c','src/archive.h',
         'src/image.c','src/image.h','src/field.h','src/field_game.h','src/field_unit.h','src/input.h','src/dialog_flow.h',
         'src/bios_clock.c','src/bios_clock.h','src/vga.h','src/field_visual_state.h',
         'data/static/tables/field_background.json','data/static/manifest.json']

def sha(raw):return hashlib.sha256(raw).hexdigest()

def freeze(path,facts,pin=None):
    path=Path(path).resolve();before=path.stat();raw=path.read_bytes();after=path.stat()
    fact=dict(path=str(path),size=len(raw),mtime_ns=before.st_mtime_ns,sha256=sha(raw))
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns) or (pin and sha(raw)!=pin):
        raise ValueError(f'input identity mismatch: {path}')
    facts.append(fact);return raw

def guards(facts):
    after=[];errors=[]
    for fact in facts:
        try:
            freeze(fact['path'],after,fact['sha256'])
            if after[-1]!=fact:raise ValueError('size/mtime changed')
        except (OSError,ValueError) as e:errors.append(dict(path=fact['path'],error=str(e)))
    return dict(before=facts,after=after,unchanged=not errors,late_errors=errors)

def fresh(root,path):
    target=Path(path).resolve();original=(root/'original_game').resolve()
    if target==original or original in target.parents or not any(
        allowed in target.parents for allowed in [(root/'build/f4').resolve(),Path('F:/fd2sdl-evidence').resolve()]):
        raise ValueError('output outside F4 roots or inside actual original_game')
    if target.exists():raise ValueError('output must be a fresh directory')
    target.mkdir(parents=True);return target

def archive_entry(raw,n):
    if raw[:6]!=b'LLLLLL':raise ValueError('archive magic')
    end=struct.unpack_from('<I',raw,6)[0];offsets=[]
    for at in range(6,end-3,4):
        x=struct.unpack_from('<I',raw,at)[0]
        if x>=len(raw) or (offsets and x<=offsets[-1]):break
        offsets.append(x)
    if n>=len(offsets):raise ValueError('archive index')
    return raw[offsets[n]:offsets[n+1] if n+1<len(offsets) else len(raw)]

def requests():
    tick=0
    none=[0,1,2,3,4,5,6,7,8,10,11,12,13,14,15,16,18,19,20,26,30,31,32]
    for stage in none:yield dict(stage=stage,op='none',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=True)
    for stage,mx,my in [(17,37,17),(21,32,42),(22,28,32),(27,35,18)]:
        for corner in range(4):yield dict(stage=stage,op='parallax',camera=[mx if corner&1 else 0,my if corner&2 else 0],
            tick=tick,advance=1,setter=0,rebuild=corner==0)
    for stage,aliases in [(9,[24,25]),(28,[29])]:
        for n in range(16):
            tick+=1
            yield dict(stage=stage,op='wave_update',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=n==0)
            yield dict(stage=stage,op='wave_same',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=False)
        for alias in aliases:
            tick+=1
            yield dict(stage=alias,op='wave_update',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=True)
            yield dict(stage=alias,op='wave_same',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=False)
    yield dict(stage=23,op='scroll_initial',camera=[0,0],tick=tick,advance=0,setter=0,rebuild=True)
    for n in range(191):
        tick+=1
        yield dict(stage=23,op='scroll_cycle',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=False)
    for speed in range(1,15):
        yield dict(stage=23,op='scroll_set',camera=[0,0],tick=tick,advance=0,setter=speed,rebuild=False)
        tick+=1
        yield dict(stage=23,op='scroll_step',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=False)
        yield dict(stage=23,op='scroll_same',camera=[0,0],tick=tick,advance=1,setter=0,rebuild=False)

def seed(size,stride,factor,base,step,index):
    return bytes((i*factor+i//stride+base+index*step)&255 for i in range(size))

def contract_inputs(a,facts):
    contract=json.loads(freeze(a.contract,facts,a.contract_sha256))
    if contract.get('schema')!='fd2-f4-background-family-contract-v1':raise ValueError('contract schema')
    return contract

def normal_maps(a,contract,facts):
    field=freeze(a.field,facts,contract['inputs']['field'])
    for r in contract['parallax']:
        raw=archive_entry(field,r['stage']*3)
        w,h=struct.unpack_from('<HH',raw)
        if len(raw)!=4+4*w*h or [w-13,h-8]!=r['camera_max']:raise ValueError('normal map/camera domain')

def derive(a,root,facts):
    contract=contract_inputs(a,facts);pins=contract['inputs'];normal_maps(a,contract,facts)
    source=json.loads(freeze(a.source_json,facts,pins['source_json']))
    image=freeze(a.image,facts,pins['image'])
    for record in contract['functions']:
        va=record['va']
        f=next(f for f in source['functions'] if f['va']==va)
        if len(f['chunks'])!=len(record['chunks']):raise ValueError('source chunk count')
        for c,wanted in zip(f['chunks'],record['chunks']):
            raw=bytes.fromhex(c['bytes'])
            if {k:c[k] for k in ('start','end','sha256')}!=wanted or raw!=image[c['start']:c['end']] or sha(raw)!=wanted['sha256']:raise ValueError('strict source chunk')
            at=c['start']
            for i in f['instructions']:
                if c['start']<=i['va']<c['end']:
                    if i['va']!=at:raise ValueError('complete instruction heads')
                    at+=i['size']
            if at!=c['end']:raise ValueError('instruction end')
    canonical=freeze(a.canonical,facts,pins['canonical'])
    if b'VA=0x4eee0..0x4ef29' not in canonical:raise ValueError('complete wave canonical absent')
    if sha(image[0x4eee0:0x4ef29])!=contract['wave_kernel']['sha256']:raise ValueError('wave leaf bytes')
    offsets=image[0x627c8:0x627d8]
    if list(offsets)!=[2,3,3,4,4,4,3,3,2,1,1,0,0,0,1,1] or image[0x51a10]!=1:raise ValueError('original wave/speed bytes')
    dispatch=json.loads(freeze(a.dispatch,facts,pins['dispatch']))
    if not dispatch['ok'] or dispatch['original_dispatch_cases']!=33:raise ValueError('old dispatch qualification')
    configs=[r['config'] for r in dispatch['traces']]
    other=freeze(a.resource,facts,pins['resource'])
    report_path=Path(a.original_report)
    report=json.loads(freeze(report_path,facts,pins['original_report']))
    if not report['ok'] or report['original_cases']!=256:raise ValueError('old original pixel qualification')
    pictures={}
    for binding in contract['original_payloads']:
        i,asset=binding['case'],binding['asset']
        r=report['cases'][i];p=r['original_payload'];raw=zlib.decompress(freeze(report_path.parent/p['path'],facts,p['sha256']))
        if p['sha256']!=binding['sha256']:raise ValueError('original payload pin')
        n=r['width']*r['height'];mask=raw[n+768:2*n+768]
        if r['request']!=['scene','other',asset,0,0] or len(raw)!=2*n+1536 or any(raw[-768:]):raise ValueError('original half binding')
        if asset==47:
            if [n for n,m in enumerate(mask) if not m]!=[y*462+461 for y in range(122)]:raise ValueError('47 mask')
        elif mask!=bytes([1])*n:raise ValueError('incomplete original canvas')
        pictures[asset]=raw[:n]
    out=fresh(root,a.output);rows=[];last=phase=0;speed=1;scroll=0;visible=None
    source_current=None
    for index,req in enumerate(requests()):
        stage=req['stage'];kind,asset,width,height=configs[stage]
        if req['rebuild']:
            if kind==1:
                raw=archive_entry(other,asset)
                if len(raw)!=64004 or struct.unpack_from('<HH',raw)!=(320,200):raise ValueError('raw wave source')
                source_current=raw[4:];visible=None
            elif kind==2:source_current=pictures[asset]+pictures[asset+1]
            elif kind==3:source_current=pictures[asset];scroll=speed%192
            else:source_current=None
        size=width*height if kind else 0
        before_meta=[kind,width,height,last,phase,speed,1,size,64000 if kind==1 else 0]
        before_scroll=scroll
        if req['setter']:speed=req['setter']
        updated=req['advance'] and kind in (1,3) and req['tick']!=last
        if updated:
            if kind==1:
                visible=b''.join(source_current[y*320+offsets[(y+phase)&15]:y*320+offsets[(y+phase)&15]+312] for y in range(192))
                phase=(phase+1)%16
            else:scroll=(scroll+speed)%192
            last=req['tick']
        if kind==2:
            sx,sy=3*req['camera'][0],2*req['camera'][1]
            if sx+312>width or sy+192>height or (stage==27 and sx+311>=461):raise ValueError('parallax bounded crop')
            visible=b''.join(source_current[(sy+y)*width+sx:(sy+y)*width+sx+312] for y in range(192))
        if kind==3:visible=b''.join(source_current[((y-scroll)%192)*312:((y-scroll)%192+1)*312] for y in range(192))
        before=seed(P,456,17,83,7,index);bvga=seed(V,320,29,93,11,index)
        page,vga=bytearray(before),bytearray(bvga);pmask=bytearray(P);vmask=bytearray(V)
        if kind:
            if visible is None or len(visible)!=59904:raise ValueError('unqualified wave before would be read')
            for y in range(192):
                at=(72+y)*456+72;page[at:at+312]=visible[y*312:y*312+312];pmask[at:at+312]=bytes([1])*312
        for y in range(192):
            at=(4+y)*320+4;src=(72+y)*456+72
            vga[at:at+312]=page[src:src+312];vmask[at:at+312]=bytes([1])*312
        data=before+page+bvga+vga+bytes([31])*768+bytes([126])*768+pmask+vmask
        packed=zlib.compress(data,9);name=f'expected-{index:03}.zbin';(out/name).write_bytes(packed)
        source_same=True
        if kind==3:
            source_same=all(source_current[((y-before_scroll)%192)*312:((y-before_scroll)%192+1)*312]==
                            source_current[((y-scroll)%192)*312:((y-scroll)%192+1)*312] for y in range(192))
        rows.append(dict(case=index,**req,before_meta=before_meta,
            after_meta=[kind,width,height,last,phase,speed,1,size,64000 if kind==1 else 0],
            source_unchanged=source_same,work_unchanged=None if req['op']=='wave_update' else True,
            padding_unchanged=True,other_owners_unchanged=True,payload=name,sha256=sha(packed),raw_sha256=sha(data)))
    if len(rows)!=343:raise ValueError('family denominator')
    guard=guards(facts)
    if not guard['unchanged']:raise ValueError('late source change')
    result=dict(schema='fd2-f4-background-family-expected-v1',cases=rows,identity=guard,contract_sha256=a.contract_sha256,
        new_original_CPU=0,actual_C=0,source_qualified_original_payloads_reused=9,
        fixed_NS_not_live_read_sampling=True,actual_write_mask_observed=False,
        scope='33 config bindings; normal parallax bounds; 32 unique updated wave phases plus 3 aliases; 192 scroll states and speed1..14',
        excluded='first wave work after rebuild at equal tick; arbitrary API pan/speed; whole Game/native timing; physical write bus order')
    target=out/'expected.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(path=str(target),sha256=sha(target.read_bytes()),cases=343,CPU=0,C=0)))

def header(row,resource):
    if row.get('schema')!='fd2-f4-background-family-actual-v1' or row.get('binding')!='FDOTHER.DAT' or row.get('size')!=len(resource):raise ValueError('actual header')
    if bytes.fromhex(row['bytes_hex'])!=resource:raise ValueError('actual loaded bytes differ')

def save_report(path,result):
    try:path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    except OSError as e:
        result['passed']=False;result['secondary_errors'].append(dict(operation='report.write',error=str(e)))
        if result['primary_error'] is None:result['primary_error']='report write failed'

def cleanup_probe(process,reader,result):
    """只收本次Popen；清理错误记录为secondary，绝不遮已有业务错误。"""
    def late(operation,error):
        result['passed']=False
        result['secondary_errors'].append(dict(operation=operation,error=str(error)))
        if result['primary_error'] is None:result['primary_error']='owned probe cleanup failed'
    if process is None:return
    result['owned_pid']=process.pid
    try:
        if process.poll() is None:
            try:process.terminate()
            except OSError as e:late('process.terminate',e)
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:process.kill()
                except OSError as e:late('process.kill',e)
                try:process.wait(timeout=5)
                except (OSError,subprocess.TimeoutExpired) as e:late('process.wait_after_kill',e)
            except OSError as e:late('process.wait',e)
        result['owned_process_ended']=process.poll() is not None
        if not result['owned_process_ended']:late('process.poll','owned process still running')
    except OSError as e:late('process.poll',e);result['owned_process_ended']=False
    if reader is not None:
        try:
            reader.join(timeout=10)
            if reader.is_alive():late('stdout.reader','reader did not finish')
        except (OSError,RuntimeError) as e:late('stdout.reader',e)
    if process.stdout is not None:
        try:process.stdout.close()
        except OSError as e:late('stdout.close',e)

def check_case(row,spec,raw):
    for key in ['case','stage','op','camera','tick','advance','setter','before_meta','after_meta',
                'source_unchanged','padding_unchanged','other_owners_unchanged']:
        if row.get(key)!=spec[key]:raise ValueError(f'case{spec["case"]} {key}: {row.get(key)!r} != {spec[key]!r}')
    if spec['work_unchanged'] is not None and row.get('work_unchanged')!=spec['work_unchanged']:raise ValueError('same-tick work changed')
    if len(raw)!=3*(P+V)+1536 or sha(raw)!=spec['raw_sha256']:raise ValueError('expected owner extent/hash')
    at=0
    for key,n in [('before_page',P),('after_page',P),('before_vga',V),('after_vga',V),('palette',768),('dac',768)]:
        actual=bytes.fromhex(row[key]);wanted=raw[at:at+n];at+=n
        if actual!=wanted:
            i=next((i for i,(x,y) in enumerate(zip(actual,wanted)) if x!=y),min(len(actual),n))
            raise ValueError(f'case{spec["case"]} {key} mismatch {i}; size={len(actual)}, expected={n}')

def pair(a,root,facts):
    contract=contract_inputs(a,facts);normal_maps(a,contract,facts)
    expected_raw=freeze(a.expected,facts,a.expected_sha256);expected=json.loads(expected_raw)
    if expected.get('schema')!='fd2-f4-background-family-expected-v1' or not expected['identity']['unchanged'] or len(expected['cases'])!=343:raise ValueError('expected qualification')
    # 首版derive的身份不改写：它无contract字段，须逐项连接相同输入pin。
    prior={f['sha256'] for f in expected['identity']['before']}
    required={v for k,v in contract['inputs'].items() if k!='field'}|{p['sha256'] for p in contract['original_payloads']}
    if not required<=prior:raise ValueError('expected independent input pins do not join contract')
    if expected.get('contract_sha256',a.contract_sha256)!=a.contract_sha256:raise ValueError('expected contract identity')
    for i,(s,r) in enumerate(zip(expected['cases'],requests())):
        if s['case']!=i or any(s[k]!=v for k,v in r.items()):raise ValueError('expected finite source request domain')
    resource=freeze(a.resource,facts,contract['inputs']['resource'])
    producer=Path(a.producer).resolve();freeze(producer,facts,a.producer_sha256)
    freeze(a.build_receipt,facts,a.build_receipt_sha256)
    # receipt保原身份，来源/PE实际构建关系仍须root按其现有build/tlog协议核查。
    for p in SOURCES:freeze(root/p,facts)
    for p in a.source:freeze(p if p.is_absolute() else root/p,facts)
    out=fresh(root,a.output);result=dict(passed=False,actual_C=0,primary_error=None,secondary_errors=[])
    process=None;reader=None;thread_errors=[];thread_late=[];stream_hash=hashlib.sha256();count=[0]
    def read_stdout():
        try:
            with gzip.open(out/'actual.jsonl.gz','xb') as sink:
                for number in range(345):
                    limit=8000000 if number==0 else 1000000
                    line=process.stdout.readline(limit+1)
                    if not line or len(line)>limit or not line.endswith(b'\n'):raise ValueError(f'bounded/incomplete stdout row{number}')
                    stream_hash.update(line);sink.write(line);row=json.loads(line)
                    if number==0:header(row,resource)
                    elif number==344:
                        if row!={'end':343,'binding_unchanged':True}:raise ValueError('actual end/footer')
                    else:
                        spec=expected['cases'][number-1]
                        if spec['case']!=number-1:raise ValueError('expected row order')
                        path=Path(a.expected).resolve().parent/spec['payload']
                        if path.parent!=Path(a.expected).resolve().parent:raise ValueError('expected payload path')
                        raw=zlib.decompress(freeze(path,facts,spec['sha256']))
                        check_case(row,spec,raw);count[0]+=1
                if process.stdout.read(1):raise ValueError('extra actual stdout')
        except Exception as e:
            thread_errors.append(str(e))
            try:
                if process.poll() is None:process.terminate()
            except OSError as late:thread_late.append(dict(operation='reader.terminate',error=str(late)))
    try:
        with (out/'stderr.txt').open('xb') as err:
            flags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0
            process=subprocess.Popen([str(producer),'--f4-background-family'],cwd=root,
                stdout=subprocess.PIPE,stderr=err,creationflags=flags)
            reader=threading.Thread(target=read_stdout,daemon=True);reader.start()
            code=process.wait(timeout=90);reader.join(timeout=10)
            if reader.is_alive():raise ValueError('stdout reader did not finish')
            if thread_errors:raise ValueError(thread_errors[0])
            if code:raise ValueError(f'probe exit {code}')
        result.update(passed=True,actual_C=count[0],exit_code=code,
            actual_stream_sha256=stream_hash.hexdigest(),actual_gzip_sha256=sha((out/'actual.jsonl.gz').read_bytes()))
    except Exception as e:
        result['primary_error']=thread_errors[0] if thread_errors else str(e)
        if thread_errors and str(e)!=thread_errors[0]:result['secondary_errors'].append(dict(operation='process.wait',error=str(e)))
    finally:
        cleanup_probe(process,reader,result)
        result['secondary_errors'].extend(thread_late)
        if thread_late:result['passed']=False
        result['actual_C']=count[0]
        result['C_probe_launched']=process is not None
        result['identity']=guards(facts)
        if not result['identity']['unchanged']:
            result['passed']=False
            if result['primary_error'] is None:result['primary_error']='late input guard'
        result.update(original_CPU=0,GUI=0,write_mask='derived semantic mask only; actual physical mask not observed',
            source_PE_binding='root must separately validate the pinned receipt against its build/tlog snapshot',
            scope=expected['scope'],excluded=expected['excluded'],fixed_NS_not_live_read_sampling=True)
        save_report(out/'report.json',result)
        print(json.dumps(result));
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    sub=p.add_subparsers(dest='mode',required=True)
    d=sub.add_parser('derive');d.add_argument('--original-report',required=True,type=Path);d.add_argument('--output',required=True,type=Path)
    for key in ['source-json','image','canonical','dispatch']:d.add_argument('--'+key,required=True,type=Path)
    q=sub.add_parser('pair')
    for key in ['expected','producer','build-receipt','output']:q.add_argument('--'+key,required=True,type=Path)
    for key in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+key,required=True)
    q.add_argument('--source',action='append',default=[],type=Path)
    for mode in (d,q):
        for key in ['contract','resource','field']:mode.add_argument('--'+key,required=True,type=Path)
        mode.add_argument('--contract-sha256',required=True)
    a=p.parse_args();root=a.root.resolve();facts=[]
    try:
        freeze(__file__,facts)
        derive(a,root,facts) if a.mode=='derive' else pair(a,root,facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,zlib.error) as e:
        print(json.dumps(dict(passed=False,primary_error=str(e),identity=guards(facts))));raise SystemExit(1)

if __name__=='__main__':main()
