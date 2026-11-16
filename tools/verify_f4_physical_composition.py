"""Source-derived FULL physical composition, using retained original FIGANI pixels.

derive never runs an original or C decoder. pair runs only an explicitly pinned
console test probe; no GUI, shipping API, build, mutable resource or expected C.
The normal wrapper's clock/business/first-board owners remain separate contracts.
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

from verify_f4_background_pixels import freeze, guards, fresh, save_report, cleanup_probe

PINS = {
    'action': 'd546aa1715b27cae64fde8aab58042fde11659f5bcdc30fb32fe8c3d03ec475c',
    'source': '18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517',
    'decoder': 'd1cbcbe22837ad0ffefe3e941591ab08d14a140dd871286a9659f978ea78bd33',
    'catalog': '7cbef9183ac5265ef11f92d92fc6e145db25bc9db6765771430d8908c79915cd',
    'original_report': 'ce755c30f69441f96f0f56ab3985e06f7cdaf977a056a98db920ac0cdd6d9321',
    'image': '8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1',
    'resource': 'c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8',
}
SOURCES = ['src/tests/battle_animation_test.c', 'src/tests/f4_physical_composition_probe.h',
    'src/tests/f4_remainder_probe.h', 'src/tests/f4_reader_probe.h',
    'src/battle_animation.c', 'src/battle_animation.h', 'src/figani.c', 'src/figani.h',
    'src/archive.c', 'src/archive.h', 'src/image.c', 'src/image.h', 'src/field.h',
    'src/field_game.h', 'src/field_unit.h', 'src/field_round.h', 'src/field_battle.h',
    'src/input.h', 'src/dialog_flow.h', 'src/vga.h', 'src/field_visual_state.h',
    'src/field_page.h', 'src/field_page_private.h', 'src/static_data.h',
    'data/static/tables/battle_animation.json', 'data/static/manifest.json']
WORK, VGA, ONE, PAYLOAD = 128000, 64000, 257536, 515072
SHAKE_X, SHAKE_Y = [0,4,9,14,18,14], [0,2,4,6,8,10]
SCOPE = ('2ebe1 single full_tick_draw: arbitrary defined board/work-before; board copy, '
    'side/layer order, actual attack+idle, six shake/tint states, full work/VGA/palette/DAC')
EXCLUDED = ('No new original wrapper execution; natural target/caster/camera, board construction, '
    'HP summary/range, action/idle progression, clock/present/audio/commit, skip/HOST/error return '
    'and unknown first owner are separate. Actual C physical store mask is not observed.')

def sha(b): return hashlib.sha256(b).hexdigest()
def require(ok, message):
    if not ok: raise ValueError(message)

def unpack(raw, size):
    dec=zlib.decompressobj();out=dec.decompress(raw,size+1)
    require(len(out)==size and dec.eof and not dec.unconsumed_tail and not dec.unused_data,
        'bounded complete zlib payload')
    return out

def source_check(image, data, va):
    fn=next(f for f in data['functions'] if f['va']==va)
    covered=set();heads=set();chunks=[]
    for c in fn['chunks']:
        start,end=c['start'],c['end'];raw=bytes.fromhex(c.get('bytes',c.get('raw_hex','')))
        require(raw==image[start:end] and sha(raw)==c['sha256'],f'strict source chunk {va:x}')
        require(not covered.intersection(range(start,end)), 'overlapping source chunks')
        covered.update(range(start,end));chunks.append(dict(start=start,end=end,sha256=sha(raw)))
    for i in fn['instructions']:
        start=i.get('va',i.get('ea'));size=i['size']
        if 'raw_hex' in i:require(image[start:start+size].hex()==i['raw_hex'], 'strict instruction bytes')
        require(size>0 and not heads.intersection(range(start,start+size)), 'overlapping instruction heads')
        heads.update(range(start,start+size))
    require(covered and covered==heads, 'complete instruction/byte coverage')
    return dict(va=va,chunks=chunks,instructions=len(fn['instructions']))

def read_inputs(a,facts):
    raw={k:freeze(getattr(a,k),facts,v) for k,v in PINS.items()}
    source=json.loads(raw['source']);decoder=json.loads(raw['decoder']);action=json.loads(raw['action'])
    contract=[source_check(raw['image'],source,0x2ebe1),
        source_check(raw['image'],decoder,0x2eb9f),source_check(raw['image'],decoder,0x4e98d)]
    require(list(struct.unpack_from('<6i',raw['image'],0x5266c))==SHAKE_X and
        list(struct.unpack_from('<6i',raw['image'],0x52684))==SHAKE_Y,'original shake tables')
    require(action['figani']['parts']==2118 and action['guards_unchanged'], 'retained A15 inventory')
    report=json.loads(raw['original_report'])
    require(report['guards_unchanged'] and report['original_cases_executed']==18637 and
        report['hardware_IO_instructions_executed']==0,'original leaf qualification')
    catalog={}
    for line in raw['catalog'].splitlines():
        row=json.loads(line)
        if row['family']=='figani':
            require(row['key'] not in catalog,'duplicate catalog key');catalog[row['key']]=row
            require(sha(raw['resource'][row['start']:row['end']])==row['source_sha256'],
                'catalog uses identical frozen resource bytes')
    results={r['asset']:r for r in report['results'] if r['family']=='figani' and r['variant']=='normal'}
    require(len(catalog)==len(results)==2118 and set(catalog)==set(results),'2118 unique original/cat rows')
    # Only directory/header metadata is parsed here; pixel expected comes from original outputs.
    resource=raw['resource'];end=struct.unpack_from('<I',resource,6)[0]
    require(resource[:6]==b'LLLLLL' and end>=10 and (end-6)%4==0, 'FIGANI directory shape')
    offsets=list(struct.unpack_from('<'+'I'*((end-6)//4),resource,6))
    require(offsets[0]==end and offsets[-1]==len(resource) and len(offsets)==409 and
        all(x<y for x,y in zip(offsets,offsets[1:])), 'FIGANI complete directory')
    parts={};clip_facts=[];eligible={};all_bounds=[]
    for clip in action['figani']['clips']:
        entry=clip['entry'];begin,finish=offsets[entry:entry+2];data=resource[begin:finish]
        if clip['empty']:
            require(data==b'\0\0\x0a','exact empty sentinel');continue
        require(len(data)>=8 and list(data[:3])==[clip['count'],clip['split'],clip['intro']], 'clip header')
        for part,meta in enumerate(clip['parts']):
            at=struct.unpack_from('<I',data,8+part*4)[0]
            tail=struct.unpack_from('<I',data,8+(part+1)*4)[0] if part+1<clip['count'] else len(data)
            x,y=struct.unpack_from('<HH',data,at);w,h=struct.unpack_from('<HH',data,at+9)
            require([x,y,data[at+4],data[at+5],data[at+6],data[at+7],w,h]==
                [meta[k] for k in ['x','y','marker','cue','duration','layer','width','height']], 'original part metadata')
            key=f'figani:{entry}:{part}';cat=catalog[key];row=results[key]
            require([cat['start'],cat['end']]==[begin+at+9,begin+tail] and
                [cat['raw_x'],cat['raw_y'],cat['width'],cat['height']]==[x,y,w,h] and
                row['source_sha256']==cat['source_sha256'] and row['page_bytes']==w*h and
                row['stride']==w and row['owner_height']==h and row['source_unchanged'] and
                row['base_pointer_and_bytes_unchanged'],'original source/parameter binding')
            require(0<=x and 0<=y and x+w<=320 and y+h<=200,'resource rectangle inside original board')
            # Every full-domain placement remains inside the actual 400x320 allocation.
            for dx in [-18,18]:
                for dy in [-10,10]:
                    require(0<=40+x+dx and 40+x+dx+w<=400 and
                        0<=40+y+dy and 40+y+dy+h<=320,'full work geometry, no C clip reached')
            parts[key]=dict(key=key,entry=entry,part=part,unit=clip['unit'],role=clip['role'],
                split=clip['split'],intro=clip['intro'],**meta)
            if clip['role']=='attack' and meta['duration'] and (not clip['split'] or part>=clip['intro']):
                eligible[key]=parts[key]
            all_bounds.append([x,y,x+w,y+h])
        clip_facts.append(dict(entry=entry,parts=clip['count'],split=clip['split'],intro=clip['intro']))
    require(len(parts)==2118,'all parts metadata join')
    combos=sorted({(p['split'],p['layer']&1) for p in eligible.values()})
    require(combos==[(0,0),(0,1),(1,0),(1,1)],'four real split/layer classes')
    return raw,parts,eligible,results,dict(functions=contract,parts=2118,
        geometric_parameter_classes=combos,eligible_attack_parts=len(eligible),
        geometry_bounds=[min(r[0] for r in all_bounds),min(r[1] for r in all_bounds),
            max(r[2] for r in all_bounds),max(r[3] for r in all_bounds)],
        original_clip_metadata=clip_facts)

def seed(n,factor,stride,base,inverse=0):
    return bytes(((i*factor+i//stride+base)&255)^(255 if inverse else 0) for i in range(n))

def pixels(key,a,results,parts,facts,cache):
    if key in cache:return cache[key]
    p=parts[key];r=results[key];parent=Path(a.original_report).resolve().parent
    path=(parent/r['payload']).resolve();require(path.parent==parent,'payload contained in retained oracle')
    data=unpack(freeze(path,facts,r['payload_sha256']),2*p['width']*p['height']+1536)
    n=p['width']*p['height'];image=data[:n];mask=data[n+768:n*2+768]
    require(sha(image)==r['pixels_sha256'] and sha(mask)==r['write_mask_sha256'] and
        set(mask)<={0,1} and sum(mask)==r['pixel_addresses_written'] and
        not any(data[-768:]) and r['palette_addresses_written']==0,'original pixels/mask owner')
    cache[key]=(image,mask);return image,mask

def overlay(work,part,pm,x,y,tint):
    image,mask=pm;w,h=part['width'],part['height'];x+=part['x'];y+=part['y']
    require(0<=x and 0<=y and x+w<=400 and y+h<=320,'composition must not clip')
    for sy in range(h):
        at=sy*w;dst=(y+sy)*400+x
        for sx in range(w):
            if mask[at+sx]:work[dst+sx]=image[at+sx] if tint==-1 else tint

def composed(request,parts,pm,inverse=0,reverse=False):
    actor,target,attack,idle,side,shake,tint=request
    ap=parts[f'figani:{actor*3+1}:{attack}'];ip=parts[f'figani:{target*3}:{idle}']
    work=bytearray(seed(WORK,17,400,83,inverse));board=seed(VGA,13,320,31)
    for y in range(200):work[16040+y*400:16040+y*400+320]=board[y*320:(y+1)*320]
    dx=SHAKE_X[shake];dy=0 if ap['split'] else SHAKE_Y[shake]
    operations=[(ap,pm[ap['key']],40,40,-1),
        (ip,pm[ip['key']],40+(-dx if side else dx),40+(-dy if side else dy),tint)]
    first=(ap['layer']&1) if side else not(ap['layer']&1)
    if bool(first)==bool(reverse):operations.reverse()
    for op in operations:overlay(work,*op)
    vga=b''.join(work[16040+y*400:16040+y*400+320] for y in range(200))
    return bytes(work)+vga+board+bytes(i*7%64 for i in range(768))+bytes((i*11+29)&255 for i in range(768))

def derive(a,root,facts):
    raw,parts,eligible,results,join=read_inputs(a,facts);cache={};chosen=[]
    # Max original write-count, not a C bbox/decoder, selects a meaningful overlap example.
    idles=sorted((p for p in parts.values() if p['role']=='idle'),
        key=lambda p:(-results[p['key']]['pixel_addresses_written'],p['entry'],p['part']))
    for split,layer in join['geometric_parameter_classes']:
        attacks=sorted((p for p in eligible.values() if p['split']==split and (p['layer']&1)==layer),
            key=lambda p:(-results[p['key']]['pixel_addresses_written'],p['entry'],p['part']))
        attack=attacks[0];idle=next(p for p in idles if p['unit']!=attack['unit'])
        for p in [attack,idle]:pixels(p['key'],a,results,parts,facts,cache)
        chosen.append((attack,idle))
    out=fresh(root,a.output);rows=[]
    for attack,idle in chosen:
        for side in [0,2]:
            for shake in range(6):
                tint=33 if shake==5 else -1
                req=[attack['unit'],idle['unit'],attack['part'],idle['part'],side,shake,tint]
                one=composed(req,parts,cache);two=composed(req,parts,cache,1)
                reverse=composed(req,parts,cache,reverse=True)
                difference=sum(a!=b for a,b in zip(one[:WORK],reverse[:WORK]))
                require(difference>0,'chosen real masks must distinguish source layer order')
                payload=one+two;number=len(rows);packed=zlib.compress(payload,6);path=f'case-{number:02d}.zbin'
                (out/path).write_bytes(packed)
                rows.append(dict(case=number,request=req,split=attack['split'],layer=attack['layer'],
                    payload=path,sha256=sha(packed),raw_sha256=sha(payload),payload_bytes=PAYLOAD,
                    attack=attack['key'],idle=idle['key'],order_distinguishing_addresses=difference))
    require(len(rows)==48,'fixed 4x2x6 request domain')
    result=dict(schema='fd2-f4-physical-composition-expected-v1',passed=True,cases=rows,
        independent_expected='retained original 4e98d complete pixels/masks + complete 2ebe1 placement/order',
        inputs=PINS,source_join=join,actual_C=0,new_original_CPU=0,new_GUI=0,
        scope=SCOPE,excluded=EXCLUDED,identity=guards(facts))
    require(result['identity']['unchanged'],'late derive guard')
    path=out/'expected.json';path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(passed=True,expected=str(path),sha256=sha(path.read_bytes()),cases=48,
        new_original_CPU=0,actual_C=0)))

def exact(stream,n):
    data=bytearray()
    while len(data)<n:
        part=stream.read(n-len(data))
        require(part,'short binary payload');data.extend(part)
    return bytes(data)

def jsonline(stream):
    line=stream.readline(8193)
    require(line and len(line)<=8192 and line.endswith(b'\n'),'bounded complete JSON header')
    return line,json.loads(line)

def pair(a,root,facts):
    expected=json.loads(freeze(a.expected,facts,a.expected_sha256))
    require(expected.get('schema')=='fd2-f4-physical-composition-expected-v1' and
        expected.get('passed') and expected['identity']['unchanged'] and expected['inputs']==PINS and
        len(expected['cases'])==48,'qualified expected input')
    resource=freeze(a.resource,facts,PINS['resource'])
    producer=Path(a.producer).resolve();freeze(producer,facts,a.producer_sha256)
    freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for path in SOURCES:freeze(root/path,facts)
    for path in a.source:freeze(path if path.is_absolute() else root/path,facts)
    out=fresh(root,a.output);result=dict(passed=False,actual_C=0,primary_error=None,secondary_errors=[])
    process=None;reader=None;errors=[];late=[];count=[0];stream_hash=hashlib.sha256()
    def read_stdout():
        try:
            with gzip.open(out/'actual.bin.gz','xb') as sink:
                line,row=jsonline(process.stdout);stream_hash.update(line)
                require(row==dict(schema='fd2-f4-input-v1',input_name='FIGANI.DAT',payload_bytes=len(resource)),
                    'same loaded input receipt header')
                loaded=exact(process.stdout,len(resource));require(loaded==resource,'actual loaded bytes differ')
                stream_hash.update(loaded)  # receipt bound but do not duplicate the original resource on disk
                result['loaded_input_receipt']=dict(input_name='FIGANI.DAT',size=len(loaded),sha256=sha(loaded),
                    header_sha256=sha(line),same_frozen_bytes=True)
                for number,spec in enumerate(expected['cases']):
                    line,row=jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                    require(row==dict(schema='fd2-f4-physical-composition-actual-v1',case=number,
                        request=spec['request'],split=spec['split'],layer=spec['layer'],payload_bytes=PAYLOAD,
                        other_owners_unchanged=True,extra_work_unchanged=True),'actual request/owner header')
                    require(spec['case']==number and spec['payload_bytes']==PAYLOAD,'expected case order')
                    parent=Path(a.expected).resolve().parent;path=(parent/spec['payload']).resolve()
                    require(path.parent==parent,'expected payload containment')
                    wanted=unpack(freeze(path,facts,spec['sha256']),PAYLOAD)
                    require(sha(wanted)==spec['raw_sha256'],'expected payload identity')
                    actual=exact(process.stdout,PAYLOAD);stream_hash.update(actual);sink.write(actual)
                    if actual!=wanted:
                        i=next(i for i,(x,y) in enumerate(zip(actual,wanted)) if x!=y)
                        raise ValueError(f'case{number} byte{i}: actual{actual[i]} expected{wanted[i]}')
                    count[0]+=1
                line,row=jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                require(row==dict(end=48,binding_unchanged=True),'actual end/footer')
                require(not process.stdout.read(1),'extra actual stdout')
        except Exception as e:
            errors.append(str(e))
            try:
                if process.poll() is None:process.terminate()
            except OSError as e:late.append(dict(operation='reader.terminate',error=str(e)))
    try:
        with (out/'stderr.txt').open('xb') as err:
            flags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0
            process=subprocess.Popen([str(producer),'--f4-physical-composition',str(a.resource.resolve().parent)],
                cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,creationflags=flags)
            reader=threading.Thread(target=read_stdout,daemon=True);reader.start()
            for spec in expected['cases']:
                process.stdin.write((' '.join(map(str,spec['request']))+'\n').encode('ascii'))
            process.stdin.close();code=process.wait(timeout=90);reader.join(timeout=10)
            require(not reader.is_alive(),'stdout reader did not finish')
            if errors:raise ValueError(errors[0])
            require(code==0,f'probe exit {code}')
        result.update(passed=True,exit_code=code,actual_stream_sha256=stream_hash.hexdigest(),
            actual_gzip_sha256=sha((out/'actual.bin.gz').read_bytes()),compared_bytes=count[0]*PAYLOAD)
    except Exception as e:
        result['primary_error']=errors[0] if errors else str(e)
        if errors and str(e)!=errors[0]:result['secondary_errors'].append(dict(operation='process.wait',error=str(e)))
    finally:
        if process is not None and process.stdin is not None and not process.stdin.closed:
            try:process.stdin.close()
            except OSError as e:
                late.append(dict(operation='stdin.close',error=str(e)))
        cleanup_probe(process,reader,result);result['secondary_errors'].extend(late)
        result.update(actual_C=count[0],C_probe_launched=process is not None,new_original_CPU=0,GUI=0,
            identity=guards(facts),scope=SCOPE,excluded=EXCLUDED,
            source_PE_binding='root must separately validate pinned receipt against build/tlog source snapshot')
        result['actual_archive_layout']='48 JSON+binary case packets and footer; initial full input receipt omitted, hash retained in report'
        if late or not result['identity']['unchanged']:
            result['passed']=False
            if result['primary_error'] is None:result['primary_error']='late input or cleanup guard'
        save_report(out/'report.json',result);print(json.dumps(result))
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    sub=p.add_subparsers(dest='mode',required=True)
    d=sub.add_parser('derive')
    for key in PINS:d.add_argument('--'+key.replace('_','-'),required=True,type=Path)
    q=sub.add_parser('pair')
    for key in ['expected','producer','build-receipt','resource']:q.add_argument('--'+key,required=True,type=Path)
    for key in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+key,required=True)
    q.add_argument('--source',action='append',default=[],type=Path)
    for cmd in [d,q]:cmd.add_argument('--output',required=True,type=Path)
    a=p.parse_args();facts=[];root=a.root.resolve()
    try:
        freeze(__file__,facts);freeze(Path(freeze.__code__.co_filename),facts)
        derive(a,root,facts) if a.mode=='derive' else pair(a,root,facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,zlib.error) as e:
        print(json.dumps(dict(passed=False,primary_error=str(e),identity=guards(facts))));raise SystemExit(1)

if __name__=='__main__':main()
