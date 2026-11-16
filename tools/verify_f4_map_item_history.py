"""Finite MAP/Item page histories from full original source and retained masks.

No decoder or original execution. pair invokes only a pinned private console
probe. Refresh/terrain pages and idle phase are explicit fixture inputs, not a
claim about live BIOS, first App, normal reachable actors or audible samples.
"""
from __future__ import annotations
import argparse
import gzip
import json
from pathlib import Path
import struct
import subprocess
import sys
import threading
import zlib
import verify_f4_physical_composition as core
from verify_f4_background_pixels import archive_entry

sha,require,freeze,guards=core.sha,core.require,core.freeze,core.guards
P,V,W=153216,64000,172800
PAYLOAD=2*P+2*V+W+1536
PINS={
 'source':'18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517',
 'scope':'67ecf2532928a0359c63e5770eeace5d5feecf56098d2fe4d72b2dc1b5fc2c75',
 'image':'8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1',
 'original_report':'6c95a250ec75333a35930c026e37160c4cd6c21632d4a7673aace2cf17fddb30',
 'icon_report':'ce755c30f69441f96f0f56ab3985e06f7cdaf977a056a98db920ac0cdd6d9321',
 'catalog':'7cbef9183ac5265ef11f92d92fc6e145db25bc9db6765771430d8908c79915cd',
 'other':'882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667',
 'icons':'7efb4448d05f19c1e17ebd53f3e3afead235f5c008d5167548d834c3686b1e44'}
SOURCES=[*core.SOURCES,'src/tests/f4_map_item_history_probe.h','src/field_unit.c',
 'src/map_sprite.c','src/map_sprite.h','src/portrait.c','src/portrait.h',
 'data/static/flows/magic.json','src/CMakeLists.txt']
SCOPE=('Actual magic_map_frames with original LMI/FDICON readers and fixed-page painters: '
 'whole App/VGA/board/work/base histories, three actor styles and item overrides, '
 'source raw order, hidden/direct distinction, four idle inputs, HOST/skip prefix.')
EXCLUDED=('Refresh and terrain are explicit known-page adapters; no original live clock/first-App '
 'or natural producer proof; no effect/MP/item commit, teleport23/wrapper quake/radial, '
 'audio output, physical stores mask or new original CPU. C work staging beyond 64000 '
 'is observed preservation, not an original MAP allocation claim.')
POSITIONS=[[(4,3),(6,4),(8,4)],[(0,0),(-1,-1),(0,0)],
 [(12,8),(13,9),(12,8)],[(6,4),(6,4),(6,4)],[(-2,0),(6,4),(8,4)]]
IDS=[0,30,96]
TARGETS=[2,1]
# Exact 20c6f caller literals: code20 uses core4/ramp; 21 core0/duotone;
# code24 core11/ramp. They must not inherit the magic-ID style selection.
OVERRIDES={20:(4,'ramp'),21:(0,'duotone'),24:(11,'ramp')}
EXTRA_CUES=[(22,7,3),(25,3,5),(25,6,5),(18,4,7),(19,3,8),
 (19,6,8),(8,3,10),(8,6,10),(9,15,15),(9,19,15)]

def validate_request(v):
    require(type(v) is list and len(v)==6 and all(type(x) is int for x in v),'six exact integer request fields')
    id,item,idle,layout,bottom,stop=v
    require(0<=id<=32 and id!=23 and 0<=idle<=3 and 0<=layout<=4 and bottom in (0,1) and 0<=stop<=4,
        'bounded source/fixture domain')
    require(item==0 or item in OVERRIDES and OVERRIDES[item][0]==id,'exact item override input')
    return v

def requests():
    rows=[]
    def add(v):
        validate_request(v)
        if v not in rows:rows.append(v)
    for id in range(33):
        if id!=23:add([id,0,id%4,id%4,id%2,0])
    for item,(id,_) in OVERRIDES.items():
        for idle in range(4):
            for bottom in range(2):add([id,item,idle,(idle+bottom)%4,bottom,0])
    for id in (0,4,13):
        for layout in range(5):
            for bottom in range(2):add([id,0,3,layout,bottom,0])
        for stop in (1,2,3,4):add([id,0,3,3,1,stop])
    require(0<len(rows)<=128,'fixed request cap')
    return rows

def style(id,item):
    if item:return OVERRIDES[item][1]
    return ('ramp' if id<4 else 'duotone' if 4<=id<=8 or id==32 else
        'solid' if 13<=id<=22 or 25<=id<=27 else 'none')

def image_draw(page,image,x,y,transform=None):
    w,h,pixels,mask=image
    require(len(page)==P and len(pixels)==len(mask)==w*h and set(mask)<={0,1},'image/mask owner extent')
    require(0<=x and x+w<=456 and 0<=y and y+h<=336,'source-defined App placement')
    for row in range(h):
        a=row*w;b=(y+row)*456+x
        for col in range(w):
            if mask[a+col]:
                value=pixels[a+col];page[b+col]=value if transform is None else transform(value)

def project(page,vga):
    for y in range(192):vga[(y+4)*320+4:(y+4)*320+316]=page[(y+72)*456+72:(y+72)*456+384]

def seed(n,factor,stride,base,bottom=0):
    return bytearray(((i*factor+i//stride+base)&255)^(255 if bottom else 0) for i in range(n))

def history(v,tables,pictures):
    """Literal 1c4cc/1cd17/1cac7/1cb94/1c2da order, independent of C calls."""
    id,item,idle,layout,bottom,stop=validate_request(v)
    paint=style(id,item);positions=POSITIONS[layout]
    app=seed(P,17,456,83,bottom);vga=seed(V,19,320,61,bottom)
    board=seed(V,13,320,31,bottom);work=seed(W,23,640,41,bottom)
    base=bytearray(app);terrain=bytearray(app)
    palette=bytes(i*7%64 for i in range(768));dac=bytes((i*11+29)&255 for i in range(768))
    frame=ticks=delays=cues=pumps=refreshes=last_cue=0
    def refresh():
        nonlocal terrain,refreshes
        refreshes+=1
        for y in range(192):
            app[(y+72)*456+72:(y+72)*456+384]=bytes(40+(x*7+y*11+refreshes*3)%173 for x in range(312))
        terrain=bytearray(app);project(app,vga)
    def snapshot(final,label):
        payload=bytes(app)+bytes(vga)+bytes(board)+bytes(work)+bytes(base)+palette+dac
        require(len(payload)==PAYLOAD,'whole owner packet size')
        return dict(final=final,frame=frame,ticks=ticks,delays=delays,cues=cues,last_cue=last_cue,
            pumps=pumps,refreshes=refreshes,payload_bytes=PAYLOAD,label=label),payload
    def uploaded(label):
        nonlocal frame,pumps
        pumps+=1
        result=snapshot(False,label);frame+=1
        return result
    def cue(sample):
        nonlocal cues,last_cue
        cues+=1;last_cue=sample
    def actor(page,raw,transform=None):
        x,y=positions[raw]
        if -1<=x<=13 and -1<=y<=9:
            phase=2 if idle==3 else idle
            image_draw(page,pictures[('icon',IDS[raw]*12+phase)],72+24*x,66+24*y,transform)
    if stop in (2,3):
        yield snapshot(True,'preexisting skip/host: no refresh or upload')
        return
    refresh();board[:]=vga;base[:]=app
    for n in range(tables['count'][id]):
        vga[:]=board;app[:]=base
        for raw in TARGETS:
            x,y=positions[raw]
            image_draw(app,pictures[('lmi',tables['start'][id]+n)],72+24*x,66+24*y)
        project(app,vga)
        sample=tables['cue'][id] if n==0 and tables['cue'][id] else next(
            (s for key,at,s in EXTRA_CUES if key==id and at==n),None)
        if sample is not None:cue(sample)
        yield uploaded('LMI source frame')
        if stop==1:
            yield snapshot(True,'HOST at first upload; no wait/next source')
            return
        ticks+=1
    refresh();board[:]=vga
    yield uploaded('post-FX refresh, still first map-base owner')
    if stop==1:
        yield snapshot(True,'HOST at first upload; no style or base replacement')
        return
    base[:]=app
    if paint=='ramp':
        tint_base=tables['tint'][id]
        for n in range(10):
            vga[:]=board;app[:]=base
            phase=7-n%8
            if phase==0:phase=8
            for raw in TARGETS:actor(app,raw,lambda value,p=phase:(tint_base+((p+value)&7))&255)
            project(app,vga);yield uploaded('RAMP fixed-dir0 source tick')
            if stop==4:
                yield snapshot(True,'HOST at first RAMP upload; retain tint prefix, no wait/tail')
                return
            ticks+=1
        vga[:]=board
        yield uploaded('RAMP old-VGA tail; App retains last tint')
    elif paint=='duotone':
        pages=[]
        for variant in range(2):
            page=bytearray(terrain)
            for raw,(x,y) in enumerate(positions):
                if layout==4 and raw==2 or not (-1<=x<=13 and -1<=y<=9):continue
                if raw in TARGETS:image_draw(page,pictures[('lmi',74+variant)],72+24*x,66+24*y)
                else:actor(page,raw)
            pages.append(page)
        app[:]=pages[0]
        for n in range(8):
            project(pages[n&1],vga);yield uploaded('DUOTONE two terrain owners')
            if stop==4:
                yield snapshot(True,'HOST at first DUOTONE upload; no delay/refresh tail')
                return
            delays+=90
        refresh();yield uploaded('DUOTONE normal refresh tail')
    elif paint=='solid':
        work[:V]=board
        # 原1c309先cue再画。这里仅在上传/return观察累计值，不签逐store/PIC时刻。
        cue(1)
        for raw in TARGETS:actor(app,raw,lambda value:200)
        project(app,work)
        for n in range(11):
            vga[:]=work[:V] if n&1 else board
            yield uploaded('SOLID old/200 snapshot alternation')
            if stop==4:
                yield snapshot(True,'HOST at first SOLID upload; App/work already modified, no wait')
                return
            if n<10:ticks+=1
    yield snapshot(True,'complete finite core return')

def directory(blob,count,start):
    rows=[]
    for i in range(count):
        a=struct.unpack_from('<I',blob,start+4*i)[0]
        b=struct.unpack_from('<I',blob,start+4*(i+1))[0] if i+1<count else len(blob)
        require(start+4*count<=a<b<=len(blob),'complete source directory')
        rows.append((a,b))
    return rows

def crop_original(raw,stride,height,w,h,mask_sha):
    n=stride*height
    require(len(raw)==2*n+1536,'complete retained original payload')
    pixels,mask=raw[:n],raw[n+768:2*n+768]
    require(set(mask)<={0,1} and sha(mask)==mask_sha and not any(raw[-768:]),'original actual mask/no palette store')
    image=b''.join(pixels[(72+y)*stride+72:(72+y)*stride+72+w] for y in range(h))
    opaque=b''.join(mask[(72+y)*stride+72:(72+y)*stride+72+w] for y in range(h))
    require(sum(mask)==sum(opaque),'all original writes inside declared 72,72 crop')
    return w,h,image,opaque

def source_inputs(a,facts):
    raw={key:freeze(getattr(a,key),facts,pin) for key,pin in PINS.items()}
    source,scope=json.loads(raw['source']),json.loads(raw['scope']);contract=[]
    for va in (0x1c4cc,0x1cd17,0x1cac7,0x1cb94,0x1c2da,0x20c6f):
        f=next((f for f in source['functions'] if f['va']==va),None)
        if f is None:f=next(f for f in scope['functions'] if f['va']==va)
        for chunk in f['chunks']:
            data=bytes.fromhex(chunk.get('bytes',chunk.get('raw_hex','')))
            require(data==raw['image'][chunk['start']:chunk['end']] and sha(data)==chunk['sha256'],'strict complete source chunks')
        contract.append(dict(va=va,chunks=[dict(start=c['start'],end=c['end'],sha256=c['sha256']) for c in f['chunks']],
            recorded_heads=len(f['instructions'])))
    tables={name:list(raw['image'][va:va+count]) for name,va,count in
        [('start',0x51f33,33),('count',0x51f54,33),('cue',0x51f75,33),('tint',0x52006,30)]}
    require(len(set(zip(tables['start'],tables['count'])))==15 and len({i for s,n in zip(tables['start'],tables['count']) for i in range(s,s+n)})==167,'source table denominator')
    require(tables['tint'][:4]==[32]*4,'source RAMP table')
    report=json.loads(raw['original_report']);icons=json.loads(raw['icon_report'])
    require(report['ok'] and report['guards_unchanged'] and len(report['cases'])==2093 and
        icons['guards_unchanged'] and icons['original_cases_executed']==18637,'retained original qualification')
    lmi_rows={tuple(r['request']):r for r in report['cases']}
    icon_rows={r['asset']:r for r in icons['results'] if r['family']=='icon' and r['variant']=='normal'}
    catalog={r['key']:r for line in raw['catalog'].splitlines() for r in [json.loads(line)] if r['family']=='icon'}
    block=archive_entry(raw['other'],6)
    require(block[:6]==b'LMI1\xe6\0','real MAP directory230')
    spans=directory(block,230,6);pictures={};bindings=[]
    needed={74,75}
    for id,item,idle,layout,bottom,stop in requests():
        needed.update(range(tables['start'][id],tables['start'][id]+tables['count'][id]))
    for frame in sorted(needed):
        row=lmi_rows[('battle','maplmi',6,frame,1)];a0,b0=spans[frame]
        w,h=struct.unpack_from('<HH',block,a0)
        require(row['source_sha256']==sha(block[a0:b0]) and row['entry']==0x4ebab and
            row['stride']==456 and row['height']==336 and row['args'][0]==0x300000+72+72*456 and
            row['hardware_IO_executed']==0 and row['source_bytes_unchanged'] and row['write_extent_passed'],'actual original LMI transparent input/placement')
        item=row['original_payload'];parent=a.original_report.resolve().parent;path=(parent/item['path']).resolve()
        require(path.parent==parent,'original LMI payload containment')
        packed=freeze(path,facts,item['sha256']);decoded=core.unpack(packed,2*P+1536)
        pictures[('lmi',frame)]=crop_original(decoded,456,336,w,h,row['original_write_mask_sha256'])
        require(384+w<=456 and 282+h<=336,'all source extreme cells fit real App')
        bindings.append(dict(kind='lmi',frame=frame,source_sha256=row['source_sha256'],original_payload_sha256=item['sha256']))
    for unit in IDS:
        for phase in range(3):
            frame=12*unit+phase;key=f'icon:{frame}';row=icon_rows[key];cat=catalog[key]
            require(row['source_sha256']==cat['source_sha256']==sha(raw['icons'][cat['start']:cat['end']]) and
                row['stride']==456 and row['owner_height']==336 and row['source_unchanged'] and row['base_pointer_and_bytes_unchanged'] and
                row['hardware_IO_instructions_executed']==0,'actual original icon input and owners')
            parent=a.icon_report.resolve().parent;path=(parent/row['payload']).resolve()
            require(path.parent==parent,'original icon payload containment')
            packed=freeze(path,facts,row['payload_sha256']);decoded=core.unpack(packed,2*P+1536)
            pictures[('icon',frame)]=crop_original(decoded,456,336,24,24,row['write_mask_sha256'])
            bindings.append(dict(kind='icon',frame=frame,source_sha256=row['source_sha256'],original_payload_sha256=row['payload_sha256']))
    return tables,pictures,contract,bindings

def derive(a,root,facts):
    tables,pictures,contract,bindings=source_inputs(a,facts);out=core.fresh(root,a.output);cases=[]
    for number,v in enumerate(requests()):
        states=[];filename=f'case-{number:03d}.gz'
        with gzip.open(out/filename,'xb') as stream:
            for point,data in history(v,tables,pictures):
                stream.write(data);states.append(dict(**point,sha256=sha(data)))
        require(states and states[-1]['final'] and sum(s['final'] for s in states)==1,'one final owner state')
        cases.append(dict(case=number,request=v,style=style(v[0],v[1]),payload=filename,
            sha256=sha((out/filename).read_bytes()),states=states))
        if number%15==0:print(json.dumps(dict(derived_cases=number+1,total=len(requests()))),flush=True)
    result=dict(schema='fd2-f4-map-history-expected-v1',passed=True,cases=cases,
        frames=sum(len(v['states'])-1 for v in cases),inputs=PINS,source_contract=contract,pixel_bindings=bindings,
        independent_expected='source order plus actual original transparent-draw masks/pixels; explicit known refresh/terrain inputs',
        source_tables=tables,new_original_CPU=0,actual_C=0,GUI=0,scope=SCOPE,excluded=EXCLUDED,
        binding_domain='32 explicit table-core parameters excluding separate23; 3 true Item overrides; not all natural producer paths',
        identity=guards(facts),secondary_errors=[],primary_error=None)
    require(result['identity']['unchanged'],'late expected source/payload guard');core.save_report(out/'expected.json',result)
    require(result['passed'],'expected report write');print(json.dumps(dict(passed=True,path=str(out/'expected.json'),cases=len(cases),frames=result['frames'])))

def compare_state(row,case,point,actual,wanted):
    keys=['final','frame','ticks','delays','cues','last_cue','pumps','refreshes','payload_bytes']
    require(row==dict(schema='fd2-f4-map-history-state-v1',case=case,**{k:point[k] for k in keys}),'actual source state/clock/cue order')
    require(len(wanted)==PAYLOAD and sha(wanted)==point['sha256'] and len(actual)==PAYLOAD,'complete expected/current owner packet')
    if actual!=wanted:
        at=next(i for i,(x,y) in enumerate(zip(actual,wanted)) if x!=y)
        raise ValueError(f'case{case} {point["label"]} byte{at}: actual{actual[at]} expected{wanted[at]}')

def footer(spec):
    point=spec['states'][-1];stop=spec['request'][5]
    return dict(case_end=spec['case'],frames=point['frame'],host=stop in (1,3,4),skip=stop==2,
        **{k:point[k] for k in ['ticks','delays','cues','pumps','refreshes']},other_owners_unchanged=True)

def pair(a,root,facts):
    expected=json.loads(freeze(a.expected,facts,a.expected_sha256))
    require(expected['schema']=='fd2-f4-map-history-expected-v1' and expected['passed'] and
        expected['identity']['unchanged'] and expected['inputs']==PINS and
        [r['request'] for r in expected['cases']]==requests(),'complete fixed independent expected')
    directory=a.resource_root.resolve()
    resources={name:freeze(directory/name,facts,PINS[key]) for name,key in [('FDOTHER.DAT','other'),('FDICON.B24','icons')]}
    producer=a.producer.resolve();freeze(producer,facts,a.producer_sha256);freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for name in SOURCES:freeze(root/name,facts)
    out=core.fresh(root,a.output);result=dict(passed=False,actual_C_cases=0,actual_C_frames=0,
        primary_error=None,secondary_errors=[]);process=reader=None;errors=[];stream_hash=core.hashlib.sha256()
    def consume():
        try:
            with gzip.open(out/'actual.bin.gz','xb') as sink:
                result['loaded_inputs']=[]
                for name,resource in resources.items():
                    line,row=core.jsonline(process.stdout);stream_hash.update(line)
                    require(row==dict(schema='fd2-f4-input-v1',input_name=name,payload_bytes=len(resource)),'actual loaded-resource header')
                    loaded=core.exact(process.stdout,len(resource));require(loaded==resource,'actual loaded resource differs from frozen original')
                    stream_hash.update(loaded);result['loaded_inputs'].append(dict(name=name,size=len(loaded),sha256=sha(loaded)))
                for spec in expected['cases']:
                    parent=a.expected.resolve().parent;path=(parent/spec['payload']).resolve()
                    require(path.parent==parent,'expected contained payload');freeze(path,facts,spec['sha256'])
                    with gzip.open(path,'rb') as wanted:
                        for point in spec['states']:
                            line,row=core.jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                            data=core.exact(wanted,PAYLOAD);actual=core.exact(process.stdout,PAYLOAD)
                            stream_hash.update(actual);sink.write(actual);compare_state(row,spec['case'],point,actual,data)
                            if not point['final']:result['actual_C_frames']+=1
                        require(not wanted.read(1),'expected exact tail')
                    line,row=core.jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                    require(row==footer(spec),'actual return/HOST/skip and owner footer')
                    result['actual_C_cases']+=1
                line,row=core.jsonline(process.stdout);stream_hash.update(line);sink.write(line)
                require(row==dict(end=len(requests()),binding_unchanged=True) and not process.stdout.read(1),'actual full end and input guards')
        except Exception as e:
            errors.append(str(e))
            if process.poll() is None:process.terminate()
    try:
        with (out/'stderr.txt').open('xb') as err:
            process=subprocess.Popen([str(producer),'--f4-map-history',str(directory)],cwd=root,
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            reader=threading.Thread(target=consume,daemon=True);reader.start()
            for spec in expected['cases']:process.stdin.write((' '.join(map(str,spec['request']))+'\n').encode('ascii'))
            process.stdin.close();code=process.wait(timeout=240);reader.join(timeout=10)
            require(not reader.is_alive(),'reader still active')
            require(not errors,errors[0] if errors else '');require(code==0,f'probe exit{code}')
        result.update(passed=True,exit_code=code,actual_stream_sha256=stream_hash.hexdigest(),
            compared_bytes=sum(len(v['states'])*PAYLOAD for v in expected['cases']))
    except Exception as e:
        result['primary_error']=errors[0] if errors else str(e)
        if errors and str(e)!=errors[0]:result['secondary_errors'].append(dict(operation='probe.wait',error=str(e)))
    finally:
        if process is not None and process.stdin and not process.stdin.closed:
            try:process.stdin.close()
            except OSError as e:
                result.update(passed=False,primary_error=result['primary_error'] or 'stdin close failed')
                result['secondary_errors'].append(dict(operation='stdin.close',error=str(e)))
        core.cleanup_probe(process,reader,result)
        result.update(identity=guards(facts),new_original_CPU=0,GUI=0,scope=SCOPE,excluded=EXCLUDED,
            source_PE_binding='parent separately joins immutable PE/build receipt/tlog inputs to source snapshot')
        if not result['identity']['unchanged']:result.update(passed=False,primary_error=result['primary_error'] or 'late input guard')
        core.save_report(out/'report.json',result)
        print(json.dumps({k:result[k] for k in ['passed','actual_C_cases','actual_C_frames','primary_error']}))
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    sub=p.add_subparsers(dest='mode',required=True);d=sub.add_parser('derive');q=sub.add_parser('pair')
    for key in PINS:d.add_argument('--'+key.replace('_','-'),required=True,type=Path)
    for key in ['expected','producer','build-receipt','resource-root']:q.add_argument('--'+key,required=True,type=Path)
    for key in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+key,required=True)
    for command in (d,q):command.add_argument('--output',required=True,type=Path)
    a=p.parse_args();facts=[]
    try:
        for path in [__file__,core.__file__,core.freeze.__code__.co_filename]:freeze(path,facts)
        derive(a,a.root.resolve(),facts) if a.mode=='derive' else pair(a,a.root.resolve(),facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,zlib.error) as e:
        print(json.dumps(dict(passed=False,error=str(e),identity=guards(facts))));raise SystemExit(1)
if __name__=='__main__':main()
