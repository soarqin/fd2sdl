#!/usr/bin/env python3
"""Fixed48000 common-DC/capture Fraction vectors, with no resource/C execution.

Formula/source qualification is external: fixed DOSBox-X b6abbd5 mixer.cpp
699..735 and earlier independent source binder. This generator does not prove
original semantics by regenerating itself. It only emits finite synthetic data.
"""
from fractions import Fraction
from pathlib import Path
import argparse,hashlib,json,struct
I0,I1=-(1<<31),(1<<31)-1
THRESH=30000*8192
STEP=(1<<24)//48000

def floor(q):return q.numerator//q.denominator

def i32(x):return I0<=x<=I1

def clip(x):return max(-32768,min(32767,x))

def f32_integer(x):return int(struct.unpack('<f',struct.pack('<f',x))[0])

def model(fm,sb,gain,before,enabled):
    summed=[a+b for a,b in zip(fm,sb)]
    if not all(map(i32,summed)):return dict(error=-2,after=before[:])
    corrected=[x+d for x,d in zip(summed,before)] if enabled else summed[:]
    if not all(map(i32,corrected)):return dict(error=-2,after=before[:])
    after=before[:]
    if enabled:
        for ch,n in enumerate(corrected):
            # Source strict interval; Fraction gives the SAR/floor meaning,
            # independent of implementation's signed division/shift choices.
            scale=1 if abs(n)<THRESH else 64
            correction=floor(Fraction(n*STEP*scale,1<<24))
            after[ch]-=correction
    if not all(map(i32,after)):return dict(error=-2,after=before[:])
    pcm=[clip(floor(Fraction(n*g,1<<26)))for n,g in zip(corrected,gain)]
    return dict(error=0,summed=summed,corrected=corrected,pcm=pcm,after=after)

def rows_and_counter():
    rows=[]
    def add(family,fm,sb,gain=(8192,8192),before=(0,0),enabled=True):
        fm,sb,gain,before=map(list,(fm,sb,gain,before))
        expected=model(fm,sb,gain,before,enabled)
        rows.append(dict(family=family,fm=fm,sb=sb,record_q13=gain,dc_before=before,dc_enabled=int(enabled),expected=expected))
        return expected
    for v in [-8193,-8192,-8191,-1,0,1,8191,8192,8193]:
        add('small-signed-floor',(v,-v),(0,0))
    for v in [-THRESH-1,-THRESH,-THRESH+1,THRESH-1,THRESH,THRESH+1]:
        add('strict-fast-boundary',(v//2,-v//2),(v-v//2,-v-(-v//2)))
        add('strict-fast-with-old-DC',(v-123,-v+91),(0,0),before=(123,-91))
    for n in [32767*8192-1,32767*8192,32768*8192,-32768*8192-1,-32768*8192,-32768*8192+1]:
        add('capture-S16-edge',(0,0),(n,-n),enabled=False,before=(111,-222))
    for gain in [I0,-16384,-8192,-1,0,1,8191,8192,8193,16384,I1]:
        add('explicit-record-gain',(1234567,-1234567),(987654,-987654),gain=(gain,gain),before=(-45,67))
    for n in [I0,I0+1,-1,0,1,I1-1,I1]:
        add('I32-valid-no-DC',(n,-1),(0,n),enabled=False,before=(I1,I0))
    for enabled in [False,True]:
        add('FM-SB-cancel',(I1,I0),(I0,I1),enabled=enabled)
        add('sum-overflow-H',(I1,0),(1,0),enabled=enabled)
        add('sum-negative-overflow-H',(0,I0),(0,-1),enabled=enabled)
    add('ns-overflow-H',(I1,0),(0,0),before=(1,0))
    add('right-ns-overflow-H',(0,I0),(0,0),before=(13,-1))
    add('next-DC-overflow-H',(I0,0),(0,0),before=(I1,0))
    # Finite common-DC trajectory with independently specified post-channel
    # inputs. Zero DC here is declared fixture input, not a runtime reading.
    dc=[31,-47]
    for k in range(96):
        fm=[((k%9)-4)*8388608,((k%7)-3)*4194304]
        sb=[((k%5)-2)*16777216,((k%11)-5)*8388608]
        e=add('96-frame-mixed-integer-history',fm,sb,gain=(8192,12288),before=dc)
        if e['error']:raise AssertionError('declared bounded history left source i32 domain')
        dc=e['after']
    # Both finite swapping and FM/SB regrouping preserve a safe two-channel sum.
    for fm,sb,before,gain in [([12345,-67789],[3333,90001],[7,-11],[8192,16384]),
                             ([THRESH-1,-THRESH],[0,0],[0,0],[1,I1])]:
        add('stereo-independent-original',fm,sb,gain,before)
        add('stereo-independent-swapped',fm[::-1],sb[::-1],gain[::-1],before[::-1])
        add('FM-SB-safe-commuted',sb,fm,gain,before)
    # Onepole fixed point of a sustained U8=255, Q13 gain8192 channel.
    target=((255-128)*256)*8192
    fixed=target-1
    assert floor(Fraction(target*38711+fixed*(65536-38711),65536))==fixed
    rounded=f32_integer(fixed)
    assert rounded==target and rounded-fixed==1
    exact=add('U8-255-fixedpoint-explicit-DC0',(0,0),(fixed,fixed),before=(0,0))
    wrong=add('U8-255-float32-rounded-comparison',(0,0),(rounded,rounded),before=(0,0))
    assert exact['pcm']==[32511,32511]and wrong['pcm']==[32512,32512]
    counter=dict(U8=255,channel_gain_q13=8192,channel_constant_target=target,
      channel_onepole_fixed_point=fixed,after_early_float32=rounded,
      explicitly_given_old_DC=[0,0],record_gain_q13=[8192,8192],
      exact=exact,rounded_frontend=wrong,PCM_difference=[b-a for a,b in zip(exact['pcm'],wrong['pcm'])],
      normal_input_parameter_example=True,natural_runtime_joint_before_claim=False,
      note='DC0 is an explicit source-safe fixture input, not claimed to coincide with a warmed live channel. No actual gain/DC is guessed.')
    return rows,counter

def build_vectors():
    rows,_=rows_and_counter()
    raw=b'FD2DC01\0'+struct.pack('<i',len(rows))
    for r in rows:
        e=r['expected']
        values=r['fm']+r['sb']+r['record_q13']+r['dc_before']+[r['dc_enabled'],e['error']]
        values+=e.get('summed',[0,0])+e.get('corrected',[0,0])+e.get('pcm',[0,0])+e['after']
        if len(values)!=18:raise AssertionError('fixed vector record shape')
        raw+=struct.pack('<18i',*values)
    return raw,len(rows)

def project_root():
    for parent in Path(__file__).resolve().parents:
        if (parent/'AGENTS.md').is_file()and(parent/'src').is_dir():return parent
    raise ValueError('cannot locate actual source root')

def original_target(root):
    # Directory resolution only. No original resource or binary bytes are read.
    return (root/'original_game').resolve()

def checked_output(root,build_dir,output):
    root=Path(root).resolve(strict=True)
    if root!=project_root():raise ValueError('root differs from generator actual project')
    build_arg=Path(build_dir);out_arg=Path(output)
    if any(p.casefold()=='original_game'for v in(build_arg,out_arg)for p in v.parts):
        raise ValueError('original_game is never an output')
    build=build_arg.resolve();out=out_arg.resolve()
    original=original_target(root)
    if any(p==original or p.is_relative_to(original)for p in(build,out)):
        raise ValueError('resolved original directory cannot be written, including junction aliases')
    if out_arg.name!='mixdown_dc_vectors.bin' or out==build or not out.is_relative_to(build):
        raise ValueError('fixed vector filename must remain inside declared build directory')
    if build==root:raise ValueError('source root is not a build directory')
    if out.is_relative_to(root):
        rel=out.relative_to(root).parts
        allowed=rel[0]=='build' or (len(rel)>2 and rel[0]=='src'and rel[1].startswith('build'))
        if not allowed:raise ValueError('output inside source tree must use build or src/build*')
    return out

def new_or_identical(path,data):
    if path.exists():
        if not path.is_file()or path.read_bytes()!=data:
            raise FileExistsError('refuse replacing different generated data: '+str(path))
        return False
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('xb')as stream:stream.write(data)
    return True

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=None)
    p.add_argument('--build-dir',type=Path,required=True)
    p.add_argument('--output',type=Path)
    a=p.parse_args(argv)
    root=a.root if a.root is not None else project_root()
    output=a.output if a.output is not None else a.build_dir/'mixdown_dc_vectors.bin'
    try:
        out=checked_output(root,a.build_dir,output)
        raw,count=build_vectors()
        created=new_or_identical(out,raw)
    except(OSError,ValueError)as e:p.exit(1,str(e)+'\n')
    print(json.dumps(dict(path=str(out),frames=count,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),
      created=created,synthetic_fraction_only=True,original_or_C_executed=False)))
    return 0
if __name__=='__main__':raise SystemExit(main())
