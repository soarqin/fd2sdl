"""Pair finite actual magic-work consumers with original-mask/source-derived pages.

No original execution, pixel decoder, GUI or build. Fixed original masks/bytes are
the pixel source; explicit synthetic opaque0 and end-owner parameters are marked.
"""
from __future__ import annotations
import argparse,gzip,json
from pathlib import Path
import subprocess,sys,threading,zlib
import verify_f4_physical_composition as base

sha,freeze,guards,fresh=base.sha,base.freeze,base.guards,base.fresh
require,unpack,exact,jsonline=base.require,base.unpack,base.exact,base.jsonline
WORK,PAYLOAD=172800,604672
PINS=dict(full_oracle='ce755c30f69441f96f0f56ab3985e06f7cdaf977a056a98db920ac0cdd6d9321',
    remainder='6c95a250ec75333a35930c026e37160c4cd6c21632d4a7673aace2cf17fddb30',
    source_contract='0569a80ab72c1db29bc7969f674e2e874cee2c72ce1a4573866a1741e752e400',
    fdother='882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667',
    figani='c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8')
SCOPE='finite linear 172800/128000 work writes; original normal masks; full C work/VGA/board/palette/DAC; typed defined before'
EXCLUDED='not whole FULL phase timeline or natural actor/HP/RNG/clock; synthetic opaque0 has source semantics only; end-owner cases are explicit H prefix, not natural U; ID6 exact original FP remains separate'

def seed(n,factor,stride,base_value,inverse=0):
    return bytes(((i*factor+i//stride+base_value)&255)^(255 if inverse else 0) for i in range(n))

def part(raw,entry,index):
    import struct
    begin,end=struct.unpack_from('<II',raw,6+4*entry);clip=raw[begin:end]
    require(len(clip)>=8 and index<clip[0] and 8+4*clip[0]<=len(clip),'source part directory')
    at=struct.unpack_from('<I',clip,8+4*index)[0]
    tail=struct.unpack_from('<I',clip,12+4*index)[0] if index+1<clip[0] else len(clip)
    require(8+4*clip[0]<=at<tail<=len(clip),'source part extent')
    x,y=struct.unpack_from('<HH',clip,at);w,h=struct.unpack_from('<HH',clip,at+9)
    return dict(x=x,y=y,w=w,h=h,duration=clip[at+6],count=clip[0],source_sha256=sha(clip[at+9:tail]))

def leaf(a,rows,resources,name,entry,index,facts):
    p=part(resources[name],entry,index)
    row=rows[(name,entry,index)]
    require(row['source_sha256']==p['source_sha256'],'same frozen source used by original leaf')
    parent=Path(a.full_oracle if name=='FIGANI.DAT' else a.remainder).resolve().parent
    desc=row['payload'] if name=='FIGANI.DAT' else row['original_payload']['path']
    pin=row['payload_sha256'] if name=='FIGANI.DAT' else row['original_payload']['sha256']
    path=(parent/desc).resolve();require(path.parent==parent,'original payload containment')
    n=p['w']*p['h'];raw=unpack(freeze(path,facts,pin),2*n+1536)
    p['pixels']=raw[:n];p['mask']=raw[n+768:2*n+768]
    mask_pin=row['write_mask_sha256'] if name=='FIGANI.DAT' else row['original_write_mask_sha256']
    require(set(p['mask'])<={0,1} and sha(p['mask'])==mask_pin,'original actual write mask')
    return p

def expected(request,p,inverse):
    op,entry,index,mode,origin,x,y,tint,view=request;owner=172800 if mode==0 else 128000
    work=bytearray(seed(WORK,17,640,83,inverse));first_bad=None;prefix=0;valid_after_bad=0;cross=0
    for py in range(p['h']):
        for px in range(p['w']):
            at=py*p['w']+px
            if not p['mask'][at]:continue
            dx=origin%640+x+p['x']+px;dy=origin//640+y+p['y']+py;dst=dy*640+dx
            if 0<=dst<owner:
                if first_bad is None:prefix+=1
                else:valid_after_bad+=1
                if not(0<=dx<640):cross+=1
                work[dst]=p['pixels'][at]
            elif first_bad is None:first_bad=dict(source=[px,py],address=dst)
    vga=b''.join(work[view+row*640:view+row*640+320] for row in range(200))
    require(len(vga)==64000,'viewport extent')
    payload=bytes(work)+vga+seed(64000,13,320,31)+bytes(i*7%64 for i in range(768))+bytes((i*11+29)&255 for i in range(768))
    return payload,dict(first_bad_original_store=first_bad,prefix_written=prefix,valid_writes_after_first_bad=valid_after_bad,
        within_owner_cross_row_writes=cross)

def derive(a,root,facts):
    frozen={name:freeze(getattr(a,name),facts,pin) for name,pin in PINS.items()}
    full=json.loads(frozen['full_oracle']);rem=json.loads(frozen['remainder']);contract=json.loads(frozen['source_contract'])
    require(full['guards_unchanged'] and rem['ok'] and rem['guards_unchanged'] and contract['guards_unchanged'],'retained qualification')
    resources={'FIGANI.DAT':frozen['figani'],'FDOTHER.DAT':frozen['fdother']};rows={}
    for row in full['results']:
        if row['family']=='figani' and row['asset']=='figani:0:0':rows[('FIGANI.DAT',0,0)]=row
    for row in rem['cases']:
        if row['request'][:3]==['battle','otherfx',27] and row['request'][4]==0:
            rows[('FDOTHER.DAT',27,row['request'][3])]=row
    parts={key:leaf(a,rows,resources,*key,facts) for key in [('FDOTHER.DAT',27,16),('FDOTHER.DAT',27,17),('FIGANI.DAT',0,0)]}
    p=parts[('FIGANI.DAT',0,0)];minx=min(i%p['w'] for i,m in enumerate(p['mask']) if m)
    negative_x=-p['x']-minx-1
    q=parts[('FDOTHER.DAT',27,16)];first=next(i for i,m in enumerate(q['mask']) if m)
    end_base=lambda owner:owner-2-640*(q['y']+first//q['w'])-q['x']-first%q['w']
    domain=[([0,27,16,0,18880,1,-1,-1,18880],q,'original normal resource/transition tuple'),
        ([0,27,17,0,18880,1,-1,-1,18880],parts[('FDOTHER.DAT',27,17)],'original normal resource/transition tuple'),
        ([2,27,16,0,18880,1,-1,-1,18880],q,'same source tuple through actual FX emit'),
        ([1,0,0,0,640,negative_x,0,-1,0],p,'explicit linear negative-x parameter, not natural timing'),
        ([1,0,0,2,640,negative_x,0,-1,0],p,'derived128000, extra C padding must remain before'),
        ([3,0,0,0,0,-1,1,-1,0],dict(x=0,y=0,w=3,h=2,duration=2,count=1,
            pixels=bytes([0,0,5,6,7,0]),mask=bytes([1,0,1,1,1,1])),'explicit synthetic opaque0/SKIP source-writer vector; no original execution'),
        ([2,27,16,0,end_base(172800),0,0,-1,0],q,'explicit first owner-error H prefix, not natural producer'),
        ([0,27,16,2,end_base(128000),0,0,-1,0],q,'derived owner-error H prefix, not natural producer')]
    out=fresh(root,a.output);cases=[]
    for number,(request,p,qualification) in enumerate(domain):
        left,meta=expected(request,p,0);right,other=expected(request,p,1);require(meta==other,'before-independent mask/write ordering')
        if number<6:require(meta['first_bad_original_store'] is None,'source-valid complete owner case')
        else:require(meta['first_bad_original_store'] is not None and meta['valid_writes_after_first_bad']==0,'defined prefix before H')
        if number in [0,1,2,3,4,5]:require(meta['within_owner_cross_row_writes']>0,'cross-row case must distinguish rectangular clip')
        part_after,ticks_after=request[2],0
        if request[0]==1:
            ticks_after=1
            if ticks_after>=p['duration']:ticks_after=0;part_after=(part_after+1)%p['count']
        raw=left+right;require(len(raw)==PAYLOAD,'packet extent');packed=zlib.compress(raw,6);name=f'case-{number:02d}.zbin';(out/name).write_bytes(packed)
        cases.append(dict(case=number,request=request,owner_bytes=172800 if request[3]==0 else 128000,
            payload_bytes=PAYLOAD,idle_part=part_after,idle_ticks=ticks_after,payload=name,sha256=sha(packed),raw_sha256=sha(raw),
            qualification=qualification,source_write=meta))
    result=dict(schema='fd2-f4-magic-work-expected-v1',passed=True,cases=cases,inputs=PINS,
        scope=SCOPE,excluded=EXCLUDED,identity=guards(facts),new_original_CPU=0,actual_C=0,GUI=0)
    require(result['identity']['unchanged'],'late derive guard');path=out/'expected.json';path.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(path=str(path),sha256=sha(path.read_bytes()),cases=8,actual_C=0,new_original_CPU=0)))

def check_header(row,spec,number):
    wanted=dict(schema='fd2-f4-magic-work-actual-v1',case=number,request=spec['request'],owner_bytes=spec['owner_bytes'],
        payload_bytes=PAYLOAD,idle_part=spec['idle_part'],idle_ticks=spec['idle_ticks'],other_owners_unchanged=True)
    require(row==wanted,'actual case/request/owner/idle header')

def pair(a,root,facts):
    wanted=json.loads(freeze(a.expected,facts,a.expected_sha256))
    require(wanted.get('schema')=='fd2-f4-magic-work-expected-v1' and wanted['passed'] and wanted['identity']['unchanged'] and
        wanted['inputs']==PINS and len(wanted['cases'])==8,'qualified expected')
    resources={name:freeze(a.resource_root/name,facts,PINS[key]) for name,key in [('FDOTHER.DAT','fdother'),('FIGANI.DAT','figani')]}
    producer=a.producer.resolve();freeze(producer,facts,a.producer_sha256);freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for name in list(base.SOURCES)+['src/tests/f4_magic_linear_work_probe.h','src/magic_fx.h']:freeze(root/name,facts)
    for name in a.source:freeze(name if name.is_absolute() else root/name,facts)
    out=fresh(root,a.output);result=dict(passed=False,actual_C=0,primary_error=None,secondary_errors=[],loaded_input_receipts=[])
    process=None;reader=None;errors=[];late=[];count=[0]
    import hashlib
    stream=hashlib.sha256()
    def consume():
        try:
            for name,raw in resources.items():
                line,row=jsonline(process.stdout);stream.update(line)
                require(row==dict(schema='fd2-f4-input-v1',input_name=name,payload_bytes=len(raw)),'same-input header')
                loaded=exact(process.stdout,len(raw));require(loaded==raw,'actual loaded resource bytes differ');stream.update(loaded)
                result['loaded_input_receipts'].append(dict(name=name,size=len(raw),sha256=sha(loaded),same_frozen_bytes=True))
            with gzip.open(out/'actual.bin.gz','xb') as sink:
                for number,spec in enumerate(wanted['cases']):
                    line,row=jsonline(process.stdout);stream.update(line);sink.write(line);check_header(row,spec,number)
                    require(spec['case']==number and spec['payload_bytes']==PAYLOAD,'expected finite order')
                    parent=a.expected.resolve().parent;path=(parent/spec['payload']).resolve();require(path.parent==parent,'expected payload containment')
                    expected_raw=unpack(freeze(path,facts,spec['sha256']),PAYLOAD);require(sha(expected_raw)==spec['raw_sha256'],'expected payload identity')
                    actual=exact(process.stdout,PAYLOAD);stream.update(actual);sink.write(actual)
                    if actual!=expected_raw:
                        i=next(i for i,(x,y) in enumerate(zip(actual,expected_raw)) if x!=y)
                        raise ValueError(f'case{number} byte{i}: actual{actual[i]} expected{expected_raw[i]}')
                    count[0]+=1
                line,row=jsonline(process.stdout);stream.update(line);sink.write(line)
                require(row==dict(end=8,bindings_unchanged=True),'end/footer');require(not process.stdout.read(1),'extra stdout')
        except Exception as e:
            errors.append(str(e))
            try:
                if process.poll() is None:process.terminate()
            except OSError as e:late.append(dict(operation='reader.terminate',error=str(e)))
    try:
        with (out/'stderr.txt').open('xb') as err:
            process=subprocess.Popen([str(producer),'--f4-magic-work',str(a.resource_root.resolve())],cwd=root,
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            reader=threading.Thread(target=consume,daemon=True);reader.start()
            for spec in wanted['cases']:process.stdin.write((' '.join(map(str,spec['request']))+'\n').encode('ascii'))
            process.stdin.close();code=process.wait(timeout=90);reader.join(timeout=10)
            require(not reader.is_alive(),'reader did not finish')
            if errors:raise ValueError(errors[0])
            require(code==0,f'probe exit{code}')
        result.update(passed=True,exit_code=code,compared_bytes=count[0]*PAYLOAD,actual_stream_sha256=stream.hexdigest(),
            actual_archive_sha256=sha((out/'actual.bin.gz').read_bytes()))
    except Exception as e:
        result['primary_error']=errors[0] if errors else str(e)
        if errors and str(e)!=errors[0]:result['secondary_errors'].append(dict(operation='process.wait',error=str(e)))
    finally:
        if process is not None and process.stdin is not None and not process.stdin.closed:
            try:process.stdin.close()
            except OSError as e:late.append(dict(operation='stdin.close',error=str(e)))
        base.cleanup_probe(process,reader,result);result['secondary_errors'].extend(late)
        result.update(actual_C=count[0],C_probe_launched=process is not None,new_original_CPU=0,GUI=0,scope=SCOPE,excluded=EXCLUDED,
            identity=guards(facts),source_PE_binding='root separately validates pinned receipt against actual build/tlog source snapshot',
            actual_archive_layout='eight JSON+binary packets and footer; loaded resource receipts are hash-only in report')
        if late or not result['identity']['unchanged']:
            result['passed']=False
            if result['primary_error'] is None:result['primary_error']='late guard or cleanup'
        base.save_report(out/'report.json',result);print(json.dumps(result))
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);sub=p.add_subparsers(dest='mode',required=True)
    d=sub.add_parser('derive')
    for name in PINS:d.add_argument('--'+name.replace('_','-'),required=True,type=Path)
    q=sub.add_parser('pair')
    for name in ['expected','producer','build-receipt','resource-root']:q.add_argument('--'+name,required=True,type=Path)
    for name in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+name,required=True)
    q.add_argument('--source',action='append',default=[],type=Path)
    for cmd in [d,q]:cmd.add_argument('--output',required=True,type=Path)
    a=p.parse_args();root=a.root.resolve();facts=[]
    try:
        freeze(__file__,facts);freeze(base.__file__,facts);freeze(Path(freeze.__code__.co_filename),facts)
        derive(a,root,facts) if a.mode=='derive' else pair(a,root,facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,zlib.error) as e:
        print(json.dumps(dict(passed=False,primary_error=str(e),identity=guards(facts))));raise SystemExit(1)

if __name__=='__main__':main()
