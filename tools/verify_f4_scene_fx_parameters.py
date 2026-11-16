"""Finite F4 transition/teleport parameters, source-derived and retained-qualified.

No original CPU/C/GUI execution. The actual files must come from a separately
bound exact C producer receipt. Explicit S5 pages/actors are controlled fixtures.
"""
from __future__ import annotations
import argparse, hashlib, json, math, struct, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import verify_f4_defeat_pages as leaf
from audit_g4_render_source import archive_entries
PAGE=456*336; VIEW=72*456+72
TRANSITION_SHA='3c1d128a396b6997aefe1daf477e38128462332cf66a01180c0b5fcd72ecac4d'
TELEPORT_SHA='0aacbdde30e309261334236d7cbbab49b58760e3be7659c6630ea5a3e55bf2b7'
TRANSITION_SOURCE_SHA='8d8d8a71301bce4e2ce57b70b8fe031532d4ef9b194b41950a4794d8deb2be0f'
TELEPORT_SOURCE_SHA='683ec0e987b4fff9d515677df1fc0d9d1d77d9e95ab11f816135fd2bc06cdbb2'
# Independent literal inputs, not supplied by measured C rows. New semantics
# outside the old six/four parameter states; audio branching is inherited.
TRANSITION_NEW=((6,5,10,8,0),(7,6,2,2,0),(6,3,10,10,0),(6,6,10,8,0))
TELEPORT_NEW=((12,15,12,15,5,4,0),(13,13,13,13,6,2,0),(255,255,13,12,6,1,0))


def sha(b):return hashlib.sha256(b).hexdigest()
def strict_json(raw):
    def unique(pairs):
        d={}
        for k,v in pairs:
            if k in d:raise ValueError('duplicate JSON key '+k)
            d[k]=v
        return d
    return json.loads(raw,object_pairs_hook=unique)

def seed_pages():
    app=bytearray((i*17+23)&255 for i in range(PAGE))
    base=bytearray((i*7+43)&255 for i in range(PAGE))
    for y in range(192):base[VIEW+y*456:VIEW+y*456+312]=bytes((x+3*y)&255 for x in range(312))
    return app,base,bytearray([165])*64000

def project(app,vga):
    for y in range(192):vga[(y+4)*320+4:(y+4)*320+316]=app[VIEW+y*456:VIEW+y*456+312]

def span(app,lut,y,left,right):
    left=max(left,0);right=min(right,312)
    if right<=left:return
    a=VIEW+y*456+left;b=VIEW+y*456+right
    if not 0<=a<=b<=PAGE:raise ValueError('source span outside explicit App owner')
    app[a:b]=bytes(lut[v] for v in app[a:b])

def circle(app,lut,cx,cy,radius,first,last):
    # 219ad starts at its exact first row; it has no hidden zero clamp.
    # Full original sqrt/CHP qualification establishes the integer half-width
    # at these finite radii/CW. Strict Y endpoints and half-open X are source.
    for y in range(first,last):
        delta=cy-y
        if -radius<delta<radius:
            half=math.isqrt((radius*radius-delta*delta)*64//25)
            span(app,lut,y,cx-half,cx+half)

def actor(app,xy):
    x=72+(xy[0]-7)*24;y=66+(xy[1]-11)*24
    for row in range(24):
        for col in range(24):
            if 0<=x+col<456 and 0<=y+row<336:app[(y+row)*456+x+col]=177

def mask(base,lut,cx,cy,radius,first,xy):
    app=bytearray(base)
    circle(app,lut,cx,cy,radius,first,192)
    actor(app,xy) # original22077 is between both source219ad calls.
    circle(app,lut,cx,cy,radius,cy,192)
    half=radius*8//5
    for y in range(first,cy):span(app,lut,y,cx-half,cx+half)
    return app

def banks(resource):
    entries=archive_entries(resource);luts=entries[3];sheet=entries[6]
    def entry(raw,index):
        count=struct.unpack_from('<H',raw,4)[0]
        if not 0<=index<count or 6+4*count>len(raw):raise ValueError('table index outside source')
        a=struct.unpack_from('<I',raw,6+4*index)[0]
        b=struct.unpack_from('<I',raw,10+4*index)[0]if index+1<count else len(raw)
        if not 6+4*count<=a<b<=len(raw):raise ValueError('table extent outside source')
        return raw[a:b]
    lut=[entry(luts,i) for i in range(10)]
    if any(len(x)!=256 for x in lut):raise ValueError('LUT is not exactly256')
    images={i:leaf.decode_lmi(entry(sheet,i))for i in range(114,125)}
    return lut,images

def transition_expected(args,luts):
    x,y,radius,step,audio=args
    _,base,vga=seed_pages();frames=[]
    for i in range(9):
        app=mask(base,luts[9-i],24*x+12,24*y+16,radius+i*step,0,(12,15))
        project(app,vga);frames.append({'page_sha256':sha(app),'vga_sha256':sha(vga)})
    return {'terrain_calls':1,'uploads':9,'cues':audio,'page_creates':1,'page_destroys':1,'logic_unchanged':True,'frames':frames}

def teleport_expected(args,luts,images):
    dx,dy,drawx,drawy,fx,fy,audio=args
    app,base,vga=seed_pages();xy=(12,15);rows=[]
    def snap(tag,index,base_too=False):
        row={'tag':tag,'index':index,'x':xy[0],'y':xy[1],'page_sha256':sha(app),'vga_sha256':sha(vga)}
        if base_too:row['base_sha256']=sha(base)
        rows.append(row)
    def image(target,frame):leaf.blit(target,images[frame],72+(drawx-7)*24,73+(drawy-11)*24)
    snap('begin',0)
    for frame in range(114,125):
        app=bytearray(base);image(app,frame);actor(app,xy);project(app,vga);snap('sprite',frame)
    image(base,124);snap('icon',124,True)
    cx=24*fx+12;cy=24*fy+15
    for frame in range(5,-1,-1):
        # These finite normal focus rows are nonnegative; original signed IDIV
        # truncates cy/5 toward zero before multiplying by this frame.
        first=(abs(cy)//5)*(1 if cy>=0 else -1)*frame
        app=mask(base,luts[frame],cx,cy,11,first,xy);project(app,vga);snap('contract',frame)
    xy=(dx,dy);app=mask(base,luts[0],cx,cy,11,0,xy);snap('commit',0)
    relative_x=drawx-7;relative_y=drawy-11;top=relative_y==0;count=18 if top else 24
    for row in range(count):
        sx=72+relative_x*24;sy=72+relative_y*24-(6 if relative_y else 0)+row
        x=4+relative_x*24;y=relative_y*24+(-2 if relative_y else 4)+row
        if not 0<=sx<=432 or not 0<=sy<336:raise ValueError('wipe source outside App')
        for col in range(24):
            if 0<=y<200 and 0<=x+col<320:vga[y*320+x+col]=app[sy*456+sx+col]
        snap('wipe',row)
    for frame in range(10):
        app=mask(base,luts[frame],cx,cy,11,0,xy);project(app,vga);snap('expand',frame)
    return {'top':int(top),'sample':int((dx,dy)==(drawx,drawy))if audio else -1,'uploads':27+count,
        'page_creates':1,'page_destroys':1,'logic_unchanged':True,'frames':rows}

def compare_frames(actual,expected,measured):
    if len(actual)!=len(expected):raise ValueError('ordered frame count differs')
    for index,(a,b)in enumerate(zip(actual,expected)):
        keys={k.replace('_sha256','_hex')if k.endswith('_sha256')else k for k in b}if measured else set(b)
        if set(a)!=keys:raise ValueError(f'frame{index} exact fields differ')
        for k,value in b.items():
            if measured and k.endswith('_sha256'):
                raw=a[k.replace('_sha256','_hex')]
                length=64000 if k=='vga_sha256'else PAGE
                if not isinstance(raw,str)or len(raw)!=2*length or sha(bytes.fromhex(raw))!=value:raise ValueError(f'frame{index} {k} complete pixels differ')
            elif type(a[k])is not type(value)or a[k]!=value:raise ValueError(f'frame{index} {k} differs')

def compare_run(actual,expected):
    if set(actual)!=set(expected):raise ValueError('exact consumer fields differ')
    for k,v in expected.items():
        if k=='frames':compare_frames(actual[k],v,True)
        elif type(actual[k])is not type(v)or actual[k]!=v:raise ValueError('consumer counter/owner differs '+k)

def retained_qualification(t_report,p_report,luts,images):
    if not t_report.get('original_CPU_executed')or not p_report.get('original_CPU_executed'):raise ValueError('retained originals unavailable')
    if len(t_report['runs'])!=12 or len(p_report['runs'])!=8:raise ValueError('retained run denominator differs')
    for row in t_report['runs']:
        e=transition_expected((*row['args'],row['audio_enabled']),luts)
        frames=[{k:v for k,v in x.items()if k!='index'}for x in row['frames']]
        compare_frames(frames,e['frames'],False)
    for row in p_report['runs']:
        e=teleport_expected(row['args'],luts,images)
        compare_frames(row['frames'],e['frames'],False)
    return {'retained_original_runs':20,'transition_original_full_frames':108,'teleport_original_ordered_prefixes':408,'originals_reexecuted':False,
        'qualification':'source model independently reproduces all retained complete page/VGA hashes; not newly executed originals'}

def verify(args):
    facts=[];primary=None;result=None;late=[]
    def read(p,expected=None,role='source'):
        raw,f=leaf.frozen(p,expected);f['role']=role;facts.append(f);return raw
    try:
        read(__file__,role='verifier');read(leaf.__file__,role='pixel_leaf_import');read(leaf.ARCHIVE_SOURCE,role='leaf_archive_import');read(archive_entries.__code__.co_filename,role='archive_import')
        image=read(args.image,leaf.IMAGE_SHA,'strict_image')
        ts=strict_json(read(args.transition_source,TRANSITION_SOURCE_SHA));ps=strict_json(read(args.teleport_source,TELEPORT_SOURCE_SHA))
        for va in (0x219ad,0x22046,0x24618,0x11eb0):leaf.strict_function(ts,va,image)
        for va in (0x22253,0x22470,0x22547,0x22656,0x4ebab,0x4ec66):leaf.strict_function(ps,va,image)
        read(args.original/'FD2.EXE',leaf.EXE_SHA,'original_EXE')
        lut,images=banks(read(args.original/'FDOTHER.DAT',leaf.OTHER_SHA,'original_resource'))
        tr=strict_json(read(args.retained_transition,TRANSITION_SHA,'retained_original_receipt'))
        pr=strict_json(read(args.retained_teleport,TELEPORT_SHA,'retained_original_receipt'))
        qualification=retained_qualification(tr,pr,lut,images)
        actual_counts={}
        for family,vectors,actual_path,producer in [('transition',TRANSITION_NEW,args.transition_actual,args.transition_producer),('teleport',TELEPORT_NEW,args.teleport_actual,args.teleport_producer)]:
            if actual_path is None:
                if producer is not None:raise ValueError('producer without actual')
                continue
            if producer is None:raise ValueError('exact producer required for actual')
            read(producer,role='producer_'+family)
            source_names={Path(p).name for p in args.producer_source or []}
            minimum={f'scene_{family}_source_test.c',f'scene_{family}.c',f'scene_{family}.h','s5_effect_fixture.h','field_game.c','field_visual.c','field_page.c'}
            if not minimum<=source_names:raise ValueError('missing direct producer sources: '+','.join(sorted(minimum-source_names)))
            for p in args.producer_source:read(p,role='producer_source')
            lines=read(actual_path,role='actual_'+family).splitlines()
            if len(lines)!=len(vectors):raise ValueError(family+' no-extra/no-omit denominator differs')
            for raw,param in zip(lines,vectors):compare_run(strict_json(raw),(transition_expected(param,lut)if family=='transition'else teleport_expected(param,lut,images)))
            actual_counts[family]=len(lines)
        result={'schema':'fd2-f4-scene-fx-finite-parameters-v1','equal':True,'retained_qualification':qualification,'actual_C_cases':actual_counts,'qualification_only':not actual_counts,
            'transition_new':TRANSITION_NEW,'teleport_new':TELEPORT_NEW,'original_CPU_executed':False,'native_executed':False,
            'claims':'finite explicit S5 before/actor/page parameters only; no complete formal cold walker, natural input, unknown fresh page or hardware timing claim'}
    except (OSError,ValueError,KeyError,IndexError,struct.error)as exc:primary=exc
    for f in facts:
        try:
            _,now=leaf.frozen(f['path']);now['role']=f['role']
            if now!=f:late.append({'path':f['path'],'error':'late SHA/mtime/size changed'})
        except(OSError,ValueError)as exc:late.append({'path':f['path'],'error':str(exc)})
    if primary or late:
        error=primary or ValueError('late source/binary identity changed')
        error.receipt={'schema':'fd2-f4-scene-fx-finite-parameters-v1','equal':False,'primary_error':str(error),'late_errors':late,'captured_inputs':facts}
        raise error
    result['inputs']=facts;result['guards_unchanged']=True
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['image','transition-source','teleport-source','retained-transition','retained-teleport']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--original',type=Path,default=ROOT/'original_game')
    for family in ('transition','teleport'):
        p.add_argument('--'+family+'-actual',type=Path);p.add_argument('--'+family+'-producer',type=Path)
    p.add_argument('--producer-source',type=Path,action='append')
    a=p.parse_args()
    try:print(json.dumps(verify(a),ensure_ascii=False,indent=2));return 0
    except(OSError,ValueError,KeyError,IndexError,struct.error)as e:
        print(json.dumps(getattr(e,'receipt',{'equal':False,'primary_error':str(e)}),ensure_ascii=False,indent=2))
        print('F4有限参数核验失败：'+str(e),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
