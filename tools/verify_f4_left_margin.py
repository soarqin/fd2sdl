"""原full tile值/mask＋raw opcode位置＋完整源参数派生left margin整页。

不运行原CPU；derive不运行C。base LUT的C0与stride2孔分别处理。
"""
from __future__ import annotations
import argparse,gzip,hashlib,json,re,struct,subprocess,sys,threading,zlib
from pathlib import Path
helper_root=Path(sys.argv[sys.argv.index('--root')+1]).resolve() if '--root' in sys.argv else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(helper_root/'tools'))
from verify_f4_background_pixels import freeze,guards,fresh,seed,sha,cleanup_probe,save_report
P,V=456*336,64000
SOURCES=['src/tile.c','src/tile.h','src/field_game.c','src/field_game.h','src/field.h','src/field_unit.c','src/field_unit.h',
 'src/field_page.c','src/field_page.h','src/field_page_private.h','src/field_visual.c','src/field_visual_state.h',
 'src/image.c','src/shape.c','src/archive.c','src/archive.h','src/input.h','src/dialog_flow.h','src/vga.h',
 'src/tests/field_unit_render_test.c','tools/verify_f4_background_pixels.py']

def entries(b):
    if b[:6]!=b'LLLLLL':raise ValueError('archive magic')
    end=struct.unpack_from('<I',b,6)[0];off=[]
    for pos in range(6,end-3,4):
        x=struct.unpack_from('<I',b,pos)[0]
        if x>=len(b) or (off and x<=off[-1]):break
        off.append(x)
    return [b[x:off[i+1] if i+1<len(off) else len(b)] for i,x in enumerate(off)]

def cell_at(mapdata,x,y):
    w,h=struct.unpack_from('<HH',mapdata);i=y*w+x
    if i < -1 or i>=w*h:raise ValueError('source index outside map/header')
    return struct.unpack_from('<I',mapdata,4+4*i)[0] if i==-1 else (struct.unpack_from('<I',mapdata,4+4*i)[0]&0x001f03ff)|0xff000000

def phase_classes(field,shape):
    rows=[]
    for stage in range(33):
        m=field[3*stage];w,h=struct.unpack_from('<HH',m)
        if len(m)!=4+4*w*h:raise ValueError('map extent')
        index=field[3*stage+1][0];attrs=shape[2*index+1];flags=[]
        for y in range(8):
            for x in range(-1,13):
                tid=cell_at(m,x,y)&1023
                if tid>=len(attrs)//4:raise ValueError('typed header/cell attrs')
                flags.append(attrs[tid*4])
        tc=2 if any(f&8 or not f&16 and f&4 for f in flags) else 1
        ac=2 if any(not f&8 and f&16 for f in flags) else 1
        actor=next(([x,y] for y in range(2,6) for x in range(2,11) if attrs[(cell_at(m,x,y)&1023)*4]&128),[8,6])
        rows.append(dict(stage=stage,shape=index,terrain_classes=tc,idle_classes=ac,actor=actor))
    if sum(r['terrain_classes']*r['idle_classes'] for r in rows)!=38:raise ValueError('38 phase families')
    return rows

def kinds(stream):
    """只解析控制区间，不解像素：0=stride孔，1=正常写，2=C0 RMW位置。"""
    out=bytearray(576);at=0
    for y in range(24):
        x=0
        while x<24:
            if at>=len(stream):raise ValueError('opcode truncated')
            c=stream[at];at+=1;n=(c&63)+1;mode=c&192;span=2*n if mode==64 else n
            if x+span>24:raise ValueError('opcode row overflow')
            if mode==192:out[y*24+x:y*24+x+n]=bytes([2])*n
            elif mode==64:
                for i in range(n):out[y*24+x+2*i+1]=1
            else:out[y*24+x:y*24+x+n]=bytes([1])*n
            at+=n if mode==128 else 0 if mode==192 else 1
            if at>len(stream):raise ValueError('opcode payload truncated')
            x+=span
    if at!=len(stream):raise ValueError('opcode tail')
    return out

def draw(page,mask,x,y,pixels,kind,lut=None,base=False):
    for row in range(24):
        for col in range(24):
            i=row*24+col;j=(y+row)*456+x+col
            if not 0<=x+col<456 or not 0<=y+row<336:raise ValueError('original destination owner')
            if kind[i]==1:page[j]=lut[pixels[i]] if lut is not None else pixels[i];mask[j]=1
            elif kind[i]==2 and base and lut is not None:page[j]=lut[page[j]];mask[j]=1

def derive(a,root,facts):
    contract=json.loads(freeze(a.contract,facts,a.contract_sha256));pins=contract['inputs']
    if contract['schema']!='fd2-f4-left-margin-contract-v1':raise ValueError('contract schema')
    resource={n:freeze(getattr(a,key),facts,pins[key]) for n,key in [('FDFIELD.DAT','field'),('FDSHAP.DAT','shape'),('FDOTHER.DAT','other')]}
    field,shape,other=[entries(resource[n]) for n in ('FDFIELD.DAT','FDSHAP.DAT','FDOTHER.DAT')]
    source=json.loads(freeze(a.source_json,facts,pins['source_json']));image=freeze(a.image,facts,pins['image'])
    for record in contract['functions']:
        f=next(f for f in source['functions'] if f['va']==record['va'])
        for c in f['chunks']:
            if {k:c[k] for k in ('start','end','sha256')} not in record['chunks'] or bytes.fromhex(c['bytes'])!=image[c['start']:c['end']]:raise ValueError('complete source chunk')
    if list(image[0x51a97:0x51aab])!=list(range(11))+list(range(9,0,-1)):raise ValueError('range20/11')
    old_lut=freeze(a.lut_oracle,facts,pins['lut_oracle'])
    if old_lut[:8]!=b'F4PW0001' or struct.unpack_from('<I',old_lut,8)[0]!=66:raise ValueError('retained66 original leaf artifact')
    full=json.loads(freeze(a.oracle,facts,pins['oracle']))
    if full['mode']!='full' or not full['guards_unchanged'] or full['original_cases_executed']!=18637:raise ValueError('retained full original qualification')
    rows={r['asset']:r for r in full['results'] if r['family']=='shape' and r['variant']=='normal'}
    bg=json.loads(freeze(a.bg_oracle,facts,pins['bg_oracle']))
    if not bg['ok'] or bg['original_cases']!=256:raise ValueError('retained BG leaves')
    backgrounds={}
    for index,asset in zip(range(232,241),[16,17,35,36,40,41,42,46,47]):
        r=bg['cases'][index];p=r['original_payload'];b=zlib.decompress(freeze(Path(a.bg_oracle).parent/p['path'],facts,p['sha256']))
        n=r['width']*r['height'];backgrounds[asset]=b[:n]
    lutdata=other[3]
    if lutdata[:4]!=b'LMI1' or struct.unpack_from('<H',lutdata,4)[0]!=23:raise ValueError('LUT source')
    luts=[]
    for i in range(23):
        start=struct.unpack_from('<I',lutdata,6+i*4)[0];end=struct.unpack_from('<I',lutdata,10+i*4)[0] if i<22 else len(lutdata)
        if end-start!=256:raise ValueError('LUT extent')
        luts.append(lutdata[start:end])
    cache={}
    def tile(index,frame):
        key=f'shape:{index}:{frame}'
        if key not in cache:
            sheet=shape[index*2];w,h,count=struct.unpack_from('<HHH',sheet)
            if (w,h)!=(24,24) or frame>=count:raise ValueError('frame source')
            start=struct.unpack_from('<I',sheet,6+frame*4)[0];end=struct.unpack_from('<I',sheet,10+frame*4)[0] if frame+1<count else len(sheet)
            stream=sheet[start:end];r=rows[key]
            if sha(stream)!=r['source_sha256']:raise ValueError('original frame input binding')
            if not re.fullmatch(r'case-\d{5}\.zbin',r['payload']):raise ValueError('payload name')
            packed=freeze(Path(a.oracle).parent/r['payload'],facts,r['payload_sha256']);b=zlib.decompress(packed)
            if r['page_bytes']!=P or len(b)!=2*P+1536:raise ValueError('normal full leaf extent')
            pixels=bytes(b[(y+72)*456+72+x] for y in range(24) for x in range(24))
            mask=bytes(b[P+768+(y+72)*456+72+x] for y in range(24) for x in range(24));kind=kinds(stream)
            if bytes(int(k==1) for k in kind)!=mask or sum(mask)!=r['pixel_addresses_written']:raise ValueError('opcode class vs original normal mask')
            cache[key]=(pixels,kind)
        return cache[key]
    groups=phase_classes(field,shape);out=fresh(root,a.output);results=[];id=0
    for group in groups:
        stage=group['stage'];index=group['shape'];m=field[3*stage];attrs=shape[index*2+1]
        w,h=struct.unpack_from('<HH',m)
        for t in range(group['terrain_classes']):
            for ac in range(group['idle_classes']):
                idle=2*ac
                for rp in range(11):
                    before=seed(P,456,17,83,7,id);bvga=seed(V,320,29,93,11,id)
                    page=bytearray(before);vga=bytearray(bvga);pmask=bytearray(P);vmask=bytearray(V)
                    visible=None
                    if stage in (9,24,25,28,29):
                        b=other[15 if stage in (9,24,25) else 55]
                        if len(b)!=64004:raise ValueError('raw wave extent')
                        wave=image[0x627c8:0x627d8];visible=b''.join(b[4+y*320+wave[y&15]:4+y*320+wave[y&15]+312] for y in range(192))
                    elif stage in (17,21,22,27):
                        top={17:16,21:35,22:40,27:46}[stage];stride=462 if stage in (17,27) else 408
                        canvas=backgrounds[top]+backgrounds[top+1];visible=b''.join(canvas[y*stride:y*stride+312] for y in range(192))
                    elif stage==23:
                        b=backgrounds[42];visible=b''.join(b[((y-1)%192)*312:((y-1)%192+1)*312] for y in range(192))
                    if visible is not None:
                        for y in range(192):
                            start=(72+y)*456+72;page[start:start+312]=visible[y*312:y*312+312];pmask[start:start+312]=bytes([1])*312
                    for y in range(8):
                        for x in range(-1,13):
                            c=cell_at(m,x,y);tid=c&1023;flags=attrs[tid*4]
                            frame=tid+(2*t if flags&8 else idle//2 if flags&16 else t if flags&4 else 0)
                            pix,kind=tile(index,frame);mapped=luts[rp] if c>>24!=255 else None
                            draw(page,pmask,72+x*24,72+y*24,pix,kind,mapped,True)
                    ax,ay=group['actor']
                    # source127a9 actor全SKIP；129ec left/raw4=1后画本格/上格/左两格。
                    for x,y in [(ax,ay),(ax,ay-1),(ax-1,ay),(ax-1,ay-1)]:
                        c=cell_at(m,x,y);tid=c&1023;flags=attrs[tid*4]
                        if flags&128:
                            pix,kind=tile(index,tid+1+(2*t if flags&8 else 0));draw(page,pmask,72+x*24,72+y*24,pix,kind)
                    for y in range(192):
                        start=(y+4)*320+4;src=(y+72)*456+72;vga[start:start+312]=page[src:src+312];vmask[start:start+312]=bytes([1])*312
                    raw=before+page+bvga+vga+bytes([31])*768+bytes([126])*768+pmask+vmask
                    packed=zlib.compress(raw,9);name=f'expected-{id:03}.zbin';(out/name).write_bytes(packed)
                    results.append(dict(case=id,stage=stage,terrain=t,idle=idle,range=rp,actor=group['actor'],source=[-1,0],
                        payload=name,sha256=sha(packed),raw_sha256=sha(raw),inputs_unchanged=True));id+=1
    if id!=418:raise ValueError('418 denominator')
    guard=guards(facts)
    if not guard['unchanged']:raise ValueError('late input guard')
    result=dict(schema='fd2-f4-left-margin-expected-v1',cases=results,phase_families=groups,identity=guard,
        normal_declared_tuples=5280,unique_source_phase_families=38,unique_luts=11,
        retained_original_tiles=len(cache),new_original_CPU=0,actual_C=0,
        base_lut='C0 LUT[old]; normal write LUT[original payload]; stride2 holes unchanged',
        qualification='source-derived complete App/VGA composition; actor source all-SKIP; actual physical mask not observed',
        excluded='first wave same-tick before; arbitrary 2D range/raw3 propagation; native/live clock; first App allocation before')
    path=out/'expected.json';path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(path=str(path),sha256=sha(path.read_bytes()),cases=418,retained_original_tiles=len(cache),C=0,CPU=0)))

def check_case(row,spec,raw):
    for k in ['case','stage','terrain','idle','range','actor','source','inputs_unchanged']:
        if row.get(k)!=spec[k]:raise ValueError(f'case{spec["case"]} {k} mismatch')
    if len(raw)!=3*(P+V)+1536 or sha(raw)!=spec['raw_sha256']:raise ValueError('expected extent/hash')
    at=0
    for key,n in [('before_page',P),('after_page',P),('before_vga',V),('after_vga',V),('palette',768),('dac',768)]:
        b=bytes.fromhex(row[key]);wanted=raw[at:at+n];at+=n
        if b!=wanted:
            i=next((i for i,(x,y) in enumerate(zip(b,wanted)) if x!=y),min(len(b),n))
            raise ValueError(f'case{spec["case"]} {key} mismatch {i} xy={i%456},{i//456}')

def pair(a,root,facts):
    contract=json.loads(freeze(a.contract,facts,a.contract_sha256));pins=contract['inputs']
    expected=json.loads(freeze(a.expected,facts,a.expected_sha256))
    if expected['schema']!='fd2-f4-left-margin-expected-v1' or len(expected['cases'])!=418 or not expected['identity']['unchanged']:raise ValueError('expected qualification')
    resources={n:freeze(getattr(a,key),facts,pins[key]) for n,key in [('FDOTHER.DAT','other'),('FDFIELD.DAT','field'),('FDSHAP.DAT','shape')]}
    if phase_classes(entries(resources['FDFIELD.DAT']),entries(resources['FDSHAP.DAT']))!=expected['phase_families']:raise ValueError('33/38 parameter binding')
    producer=Path(a.producer).resolve();freeze(producer,facts,a.producer_sha256);freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for p in SOURCES:freeze(root/p,facts)
    out=fresh(root,a.output);result=dict(passed=False,primary_error=None,secondary_errors=[]);process=reader=None
    errors=[];late=[];count=[0];digest=hashlib.sha256()
    def read_stdout():
        try:
            with gzip.open(out/'actual.jsonl.gz','xb') as sink:
                for number in range(423):
                    limit=8000000 if 1<=number<=3 else 1000000
                    line=process.stdout.readline(limit+1)
                    if not line or len(line)>limit or not line.endswith(b'\n'):raise ValueError(f'bounded stdout row{number}')
                    sink.write(line);digest.update(line);row=json.loads(line)
                    if number==0:
                        if row!={'schema':'fd2-f4-left-margin-actual-v1','cases':418}:raise ValueError('schema/count')
                    elif number<=3:
                        name=['FDOTHER.DAT','FDFIELD.DAT','FDSHAP.DAT'][number-1]
                        if row.get('input')!=name or row.get('size')!=len(resources[name]) or bytes.fromhex(row['bytes_hex'])!=resources[name]:raise ValueError('C actually loaded different bytes')
                    elif number==422:
                        if row!={'end':418,'binding_unchanged':True}:raise ValueError('footer')
                    else:
                        spec=expected['cases'][number-4]
                        if spec['case']!=number-4 or not re.fullmatch(r'expected-\d{3}\.zbin',spec['payload']):raise ValueError('case order/payload path')
                        raw=zlib.decompress(freeze(Path(a.expected).resolve().parent/spec['payload'],facts,spec['sha256']))
                        check_case(row,spec,raw);count[0]+=1
                if process.stdout.read(1):raise ValueError('extra stdout')
        except Exception as e:
            errors.append(str(e))
            try:
                if process.poll() is None:process.terminate()
            except OSError as e:late.append(dict(operation='reader.terminate',error=str(e)))
    try:
        with (out/'stderr.txt').open('xb') as err:
            process=subprocess.Popen([str(producer),'--f4-left-margin'],cwd=root,stdout=subprocess.PIPE,stderr=err,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            reader=threading.Thread(target=read_stdout,daemon=True);reader.start();code=process.wait(timeout=120);reader.join(timeout=10)
            if reader.is_alive():raise ValueError('reader unfinished')
            if errors:raise ValueError(errors[0])
            if code:raise ValueError(f'exit {code}')
        result.update(passed=True,exit_code=code,actual_stream_sha256=digest.hexdigest())
    except Exception as e:
        result['primary_error']=errors[0] if errors else str(e)
        if errors and str(e)!=errors[0]:result['secondary_errors'].append(dict(operation='process.wait',error=str(e)))
    finally:
        cleanup_probe(process,reader,result);result['secondary_errors'].extend(late)
        if late:result['passed']=False
        result.update(actual_C_completed=count[0],C_probe_launched=process is not None,original_CPU=0,GUI=0,identity=guards(facts),
            source_PE_binding='root separately validates pinned receipt against its build/tlog snapshot',
            actual_physical_write_mask_observed=False,qualification=expected['qualification'],excluded=expected['excluded'])
        if not result['identity']['unchanged']:
            result['passed']=False
            if result['primary_error'] is None:result['primary_error']='late input guard'
        save_report(out/'report.json',result);print(json.dumps(result))
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);sub=p.add_subparsers(dest='mode',required=True)
    d=sub.add_parser('derive');q=sub.add_parser('pair')
    for v in (d,q):
        for key in ['contract','field','shape','other','output']:v.add_argument('--'+key,type=Path,required=True)
        v.add_argument('--contract-sha256',required=True)
    for key in ['source-json','image','oracle','bg-oracle','lut-oracle']:d.add_argument('--'+key,type=Path,required=True)
    for key in ['expected','producer','build-receipt']:q.add_argument('--'+key,type=Path,required=True)
    for key in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+key,required=True)
    a=p.parse_args();root=a.root.resolve();facts=[]
    try:
        freeze(__file__,facts);freeze(Path(sys.modules['verify_f4_background_pixels'].__file__),facts)
        freeze(sys.executable,facts)
        if hasattr(zlib,'__file__'):freeze(zlib.__file__,facts)
        derive(a,root,facts) if a.mode=='derive' else pair(a,root,facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,zlib.error) as e:
        print(json.dumps(dict(passed=False,primary_error=str(e),identity=guards(facts))));raise SystemExit(1)
if __name__=='__main__':main()
