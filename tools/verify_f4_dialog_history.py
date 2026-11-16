"""Finite current-C ordinary dialog histories against retained original leaves.

This is not an original executor or a generic text VM. Fixed source-shaped
component histories cover complete VGA pairs, explicit FIFO cutoffs, direction
rounding, and the one known 23/12 out-of-modal glyph prefix. No hardware timing,
PCM or natural caller reachability is inferred from a typed component request.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import traceback
import zlib

ROOT=Path(__file__).resolve().parents[1]
if not (ROOT/'tools/audit_g4_render_source.py').is_file():ROOT=Path.cwd()
# Same single-snapshot module loading as the first, separately sealed text batch.
boot={}
def sha(b):return hashlib.sha256(b).hexdigest()
def snap(p,g,expected=None):
    p=Path(p).resolve();s=p.stat();b=p.read_bytes();t=p.stat()
    if (s.st_size,s.st_mtime_ns)!=(t.st_size,t.st_mtime_ns) or (expected and sha(b)!=expected):raise ValueError('input changed: '+str(p))
    g[str(p)]={'path':str(p),'sha256':sha(b),'size':len(b),'mtime_ns':t.st_mtime_ns};return b
def module(name,file):
    p=Path(__file__).resolve().parent/file
    if not p.exists():p=ROOT/'tools'/file
    b=snap(p,boot);sp=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(sp);sys.modules[name]=m;exec(compile(b,str(p),'exec'),m.__dict__);return m
text=module('f4_dialog_text_basis','verify_f4_text_composition.py');core=text.core;audit=text.audit
boot.update(text.BOOT)
WATER_CONTRACT_SHA='a7cc291f6b334c300f3cd80fc8bfd685d55715898a35b34d44601a9733438ccc'
MARKER_SHA='ab7852af90594011224ae162e50ec8382b9edd45d03c2566103a4fd3e4fab84f'
DATO_SHA='dff859783606ee95d9811e1e564bcb230ef53c4df9130e8b6d42ad37bc8c27bd'
SNAPSHOT_SHAS=['6130f7c9ac741d5966c84b304bf25f3047ef65755b272fe6bbf16d7f148322b2','8b1159ed2f6b9562b4b5e77a9e8b413457dead3575ff5dcd01b1d6cbddfe9446']
REQUIRED_SOURCES={
 'src/tests/scene_handoff_test.c','src/tests/f4_reader_probe.h','src/tests/f4_dialog_history_probe.h',
 'src/scene.c','src/scene.h','src/dialog_flow.c','src/dialog_flow.h','src/font.c','src/font.h','src/portrait.c','src/portrait.h',
 'src/text.c','src/text.h','src/archive.c','src/archive.h','src/vga.c','src/vga.h','src/input.c','src/input.h','src/bios_clock.c','src/bios_clock.h',
 'src/field_page.c','src/field_page.h','src/field_page_private.h','src/field_visual.c','src/field_visual.h','src/tile.c','src/tile.h',
 'src/field_background.c','src/field_background.h','src/field_unit.c','src/field_unit.h','src/field_game.h','src/scene_cinematic.h','src/scene_transition.h',
 'src/scene_teleport.h','src/static_data.h','src/static_flow.h','src/image.c','src/image.h','src/map_sprite.h','src/field_audio.h','src/CMakeLists.txt'}

def seed(complement=False):
    p=text.seed(64000,320)
    return bytearray(c^255 if complement else c for c in p)
def packet(pages,palette=None):return b''.join(pages)+(bytes([165])*768 if palette is None else palette)+bytes(128000+768)
def trunc_div(a,b):return (abs(a)//b)*(-1 if a<0 else 1)
def marker_xy(area,n,frame,reverse):
    rx,ry=(1,1) if n==2 else (3,2) if n==5 else (0,0)
    x,y=24*rx+4,24*ry+4;tx,ty=5,112 if area else 2
    if reverse:x,y,tx,ty=tx,ty,x,y
    return x-trunc_div((x-tx)*frame,n),y-trunc_div((y-ty)*frame,n)
def scroll(page,area):
    a=(119*320+95) if area else (9*320+15)
    for d in (3,3,3,3,3,4):
        for row in range(72):page[a-1+row*320:a-1+row*320+208]=bytes(page[a-1+(row+d)*320:a-1+(row+d)*320+208])
        page[a+72*320:a+72*320+208]=bytes([74])*208

class Original(text.Original):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs);self.portraits={};self.resources=args[2]
        self.dato=audit.archive_entries(self.resources['DATO.DAT'])
    def raw(self,frame):
        raw=self.ui_blocks[frame];w,h=struct.unpack_from('<HH',raw)
        core.require(len(raw)==4+w*h and 0<w<=320 and 0<h<=200,'source-qualified raw UI extent')
        if frame==0:core.require((w,h)==(24,24) and 0 not in raw[4:],'actual marker has no transparent zero')
        return w,h,raw[4:]
    def raw_at(self,page,frame,x,y):
        w,h,pixels=self.raw(frame)
        for sy in range(h):
            for sx in range(w):
                at=(y+sy)*320+x+sx;core.require(0<=at<64000,'normal raw writer within VGA')
                page[at]=pixels[sy*w+sx]
    def box(self,page,area,w,h):
        # 168b6: fixed header raw calls, paired horizontal/vertical loops, row-major fill.
        x,y=5,112 if area else 2;b=y*320+x
        fixed=[(1,0),(2,16*w+3),(3,(16*h+3)*320),(4,(16*h+3)*320+16*w+3),
          (5,3),(6,16*w-13),(7,(16*h+3)*320+3),(8,(16*h+3)*320+16*w-13),
          (14,3*320),(15,3*320+16*w+3),(16,(16*h-13)*320),(17,(16*h-13)*320+16*w+3)]
        for frame,delta in fixed:self.raw_at(page,frame,(b+delta)%320,(b+delta)//320)
        for i in range(w-2):
            self.raw_at(page,9,x+19+16*i,y);self.raw_at(page,12,x+19+16*i,y+16*h+3)
        for j in range(h-2):
            self.raw_at(page,10,x,y+19+16*j);self.raw_at(page,11,x+16*w+3,y+19+16*j)
        for row in range(h):
            for col in range(w):self.raw_at(page,13,x+3+16*col,y+3+16*row)
    def stages(self,page,area):
        states=[]
        for w,h in ((4,2),(8,3),(12,4),(16,5),(19,5)):
            self.box(page,area,w,h);states.append(bytes(page))
        return states
    def portrait(self,page,identity,frame,area):
        key=(identity,frame,area)
        if key not in self.portraits:
            rows=[r for r in self.rows['results'] if r['asset']==f'portrait:{identity}:{frame}' and r['variant']==('reverse' if area else 'forward')]
            core.require(len(rows)==1,'one retained original portrait leaf');row=rows[0]
            block=self.dato[identity];offs=struct.unpack_from('<'+str(struct.unpack_from('<I',block)[0]//4)+'I',block)
            raw=block[offs[frame]:offs[frame+1] if frame+1<len(offs) else len(block)]
            core.require(row['source_sha256']==sha(raw) and row['abi_input_buffer_sha256']==sha(raw),'same original DATO input')
            name=row['payload'];core.require(name.startswith('case-') and Path(name).name==name,'portrait payload basename')
            p=core.bounded_unpack(snap(self.glyph_report.parent/name,self.guards,row['payload_sha256']),129536)
            q,pal,m,pm=core.split_payload(p,64000);core.require(sha(q)==row['pixels_sha256'] and sha(m)==row['write_mask_sha256'] and sum(m)==6400 and not any(pm) and pal==bytes([165])*768,'retained portrait whole output/mask')
            x,y=(8,115) if area else (232,5);want={r*320+c for r in range(y,y+80) for c in range(x,x+80)}
            core.require(set(i for i,v in enumerate(m) if v)==want,'portrait affine source footprint')
            self.portraits[key]=[(i,c) for i,c in enumerate(q) if m[i]]
        for at,c in self.portraits[key]:page[at]=c
    def glyph_at(self,page,glyph,x,y):
        self.letter(page,bytearray(64000),glyph,y*320+x,320)

def meta(**kw):
    m=dict(source_result=0,skip=0,queued=0,last_key=0,rng=0,clears=0,delay_ms=0,ticks=0,audio=0,mouth=0,glyph_counter=0,captured=0,water=0,water_phase=0,water_anchor=0,final_restored=0,host_stopped=0,trace='')
    m.update(kw);return m

def expected(oracle,request):
    _,op,a,b,v=request;pages=[];m=None
    for invert in (False,True):
        p=seed(invert);initial=bytes(p);mm=meta()
        if op=='box':
            stages=oracle.stages(p,a)
            if v==1:
                p=bytearray(stages[3-b] if b<4 else initial)
                mm=meta(trace='UD'*4,queued=1,captured=5,delay_ms=40,final_restored=1)
            elif v==2:
                p=bytearray(stages[b]);mm=meta(trace='UD'*b+'U',queued=1,captured=b+1,delay_ms=b*10,host_stopped=1)
            else:
                p=bytearray(stages[b] if b<4 else stages[-1]);mm=meta(trace='UD'*4+'Cc',captured=5,delay_ms=40,clears=1)
                if v==3:mm.update(trace=mm['trace']+'RK',rng=1,last_key=0x1579)
        elif op=='marker':
            n=5 if v in (4,5) else 0 if v in (2,3) else 2;reverse=v in (1,3,5)
            if n:
                x,y=marker_xy(a,n,b,reverse);oracle.raw_at(p,0,x,y)
                if v==6:mm=meta(trace='U',queued=1,final_restored=1,host_stopped=1)
                else:mm=meta(trace=('UD' if reverse else 'UDCc')*(n+1),queued=int(reverse),clears=0 if reverse else n+1,delay_ms=10*(n+1),final_restored=1)
            else:mm=meta(queued=1,final_restored=1)
        elif op=='face':oracle.portrait(p,b,v,a)
        elif op=='scroll':scroll(p,a)
        elif op=='page':
            if b==3:scroll(p,a)
            if v==0:
                oracle.raw_at(p,18,191 if a else 111,181 if a else 71)
                oracle.raw_at(p,13,191 if a else 111,176 if a else 66)
                mm=meta(trace='RK',rng=1,last_key=0x1579)
            else:mm=meta(queued=1,final_restored=int(b<3))
        elif op=='mouth':
            oracle.box(p,a,19,5);oracle.portrait(p,0,0,a)
            count=1 if v==2 else b;phase=counter=0;seq='';ticks=audio=0
            for i in range(count):
                oracle.glyph_at(p,i,(95 if a else 15)+16*i,119 if a else 9);seq+='IU'
                if v in (1,2):continue
                counter+=1
                if counter==2:
                    counter=0;phase=(phase+1)&3;oracle.portrait(p,0,1 if phase==3 else phase,a);seq+='U'
                seq+='A';audio+=1
                if v==3 and audio==2:break
                seq+='T';ticks+=1
            mm=meta(trace=seq,queued=int(v==1),source_result=-1 if v==3 else 0,skip=int(v==2),host_stopped=int(v==2),mouth=phase,glyph_counter=counter,ticks=ticks,audio=audio)
        elif op=='idle':
            goal=3 if b==2 else 1 if b else 0
            host=b==3
            oracle.raw_at(p,19 if goal==3 else 18,191 if a else 111,181 if a else 71)
            if (not b or v==1) and not host:oracle.raw_at(p,13,191 if a else 111,176 if a else 66)
            mm=meta(trace='R'+'HIUE'*goal+('' if host else 'K'),rng=1,last_key=0 if host else 0x1579,
                water=goal,water_phase=goal,water_anchor=100+2*goal,skip=int(host),host_stopped=int(host))
        elif op=='edge':
            # One pinned literal normal fragment only, not a general text executor.
            tokens=audit.text_fragments(audit.archive_entries(oracle.resources['FDTXT.DAT'])[23])[12]
            core.require(sum(t in (-17,-18) for t in tokens)==4 and sum(t==-3 for t in tokens)==1,'fixed 23/12 source structure')
            seq='';current_area=None;x=y=line=counter=phase=0;saved=None;waits=0
            for i,token in enumerate(tokens):
                if i and tokens[i-1] in (-17,-18):continue
                if token in (-17,-18):
                    if current_area is not None:
                        seq+='RK'+'UD'*4;waits+=1
                        row=112 if current_area else 2
                        for yy in range(row,row+86):p[yy*320+5:yy*320+315]=saved[yy*320+5:yy*320+315]
                    current_area=int(token==-18);saved=bytes(p)
                    oracle.stages(p,current_area);seq+='UD'*4+'Cc'+'IU'
                    identity=tokens[i+1];oracle.portrait(p,identity,0,current_area)
                    x,y=(95,119) if current_area else (15,9);line=0
                elif token in (-2,-3):
                    if line==3:scroll(p,current_area);line-=1;y-=19
                    line+=1;x=95 if current_area else 15;y+=19
                    if token==-3:seq+='RK';waits+=1
                    seq+='IU'
                elif token==-1:
                    seq+='RK'+'UD'*4;waits+=1
                    # Restore only 310x86: source23/12 out-of-modal ink must survive.
                    row=112 if current_area else 2
                    for yy in range(row,row+86):p[yy*320+5:yy*320+315]=saved[yy*320+5:yy*320+315]
                    break
                else:
                    core.require(token>=0,'fixed fragment contains only qualified ordinary controls')
                    oracle.glyph_at(p,token,x,y);x+=16;seq+='IU';counter+=1
                    if counter==2:counter=0;phase=(phase+1)&3;seq+='U'
                    seq+='T'
            # Earlier modal close also only restores its own rectangle. The fixed lower
            # edge is deliberately reconstructed separately below from the original leaf.
            derived=bytes(p);p=seed(invert)
            roles=oracle.glyph(366);outside=set()
            for dx,dy,color in roles:
                px,py=303+dx,119+dy
                if px>=315: p[py*320+px]=205 if color==205 else 76;outside.add(py*320+px)
            core.require(len(outside)==13 and derived==p,'fixed source23/12 full transport resolves to exactly13 retained edge bytes')
            mm=meta(trace=seq,rng=waits,last_key=0x1579,clears=4,delay_ms=320,
                ticks=sum(t>=0 for i,t in enumerate(tokens) if not(i and tokens[i-1] in (-17,-18))),mouth=phase,glyph_counter=counter)
        else:raise ValueError('unknown finite request')
        if op not in ('box','marker','mouth','edge'):mm['final_restored']=int(p==initial)
        if m is None:m=mm
        else:core.require(m==mm,'complement-independent protocol timeline')
        pages.append(bytes(p))
    palette=bytearray([165])*768
    if op=='idle' and b:
        phase=3 if b==2 else 1
        rgb=oracle.strict_image[0x60003:0x60003+93]
        core.require(len(rgb)==93 and rgb[48:]==rgb[:45] and max(rgb)<=63,'original 16-color cyclic source93B')
        for i in range(16):
            for c,value in enumerate(rgb[3*((phase+i)&15):3*((phase+i)&15)+3]):palette[(224+i)*3+c]=(value<<2)|(value>>4)
    return packet(pages,bytes(palette)),m

def cases():
    q=[]
    for a in range(2):
        for v in (0,1):
            for b in range(5):q.append(['scene','box',a,b,v])
        for b in range(4):q.append(['scene','box',a,b,2])
        q.append(['scene','box',a,0,3])
        for v in (0,1,4,5):
            for b in range(6 if v>=4 else 3):q.append(['scene','marker',a,b,v])
        for v in (2,3,6):q.append(['scene','marker',a,0,v])
        for b in range(2):
            for v in range(4):q.append(['scene','face',a,b,v])
        q.append(['scene','scroll',a,0,0])
        for b in range(4):
            for v in range(2):q.append(['scene','page',a,b,v])
        for b in range(1,9):
            for v in (0,1):q.append(['scene','mouth',a,b,v])
        for v in (2,3):q.append(['scene','mouth',a,2,v])
        for b,v in ((0,0),(1,0),(1,1),(2,0),(2,1),(3,0)):q.append(['scene','idle',a,b,v])
    q.append(['scene','edge',0,0,0])
    return [{'request':x,'stride':320,'height':400} for x in q]


def source_contract(source07,raw_copy,snapshots,marker,image,g):
    source=json.loads(snap(source07,g,text.SOURCE07_SHA));raw=json.loads(snap(raw_copy,g,text.RAW_COPY_SHA))
    tables={f['va']:f for f in source['functions']};tables.update({f['va']:f for f in raw['functions']})
    tables.update({f['va']:f for f in json.loads(snap(marker,g,MARKER_SHA))['functions']})
    for path,digest in zip(snapshots,SNAPSHOT_SHAS):tables.update({f['va']:f for f in json.loads(snap(path,g,digest))['functions']})
    for va in (0x15e9e,0x15f84,0x164e8,0x16559,0x165ac,0x1685c,0x168b6,0x16b43,0x16c57,0x16e24,0x4ed0b,0x4ec7c,0x4ecbf,0x4eca4,0x4ecf0,0x4ed34,0x4ed4f):
        core.require(va in tables,'complete normal source owner');f=tables[va];covered=set();heads=set()
        for c in f['chunks']:
            b=bytes.fromhex(c.get('bytes',c.get('raw_hex')));core.require(image[c['start']:c['end']]==b,'strict normal source chunk');covered.update(range(c['start'],c['end']))
        for i in f['instructions']:
            begin=i.get('va',i.get('ea'));heads.update(range(begin,begin+i['size']))
        core.require(covered==heads,'complete normal instruction coverage')

def producer(path,root,g):
    d=json.loads(snap(path,g));core.require(d['schema']=='fd2-f4-dialog-producer-v1' and Path(d['source_root']).resolve()==root,'dialog producer schema/root')
    core.require(REQUIRED_SOURCES<=set(d['sources']),'dialog producer direct dependencies')
    for relative,digest in d['sources'].items():
        p=(root/relative).resolve();core.require(p.is_relative_to(root),'producer source escaped root');snap(p,g,digest)
    files={};roles=set()
    for f in d['files']:
        p=Path(f['path']).resolve();core.require(str(p) not in files,'duplicate producer artifact');snap(p,g,f['sha256']);i=g[str(p)]
        core.require((i['size'],i['mtime_ns'])==(f['size'],f['mtime_ns']),'producer artifact identity');files[str(p)]=f;roles.add(f['role'])
    core.require({'build_log','tlog','static_library','producer_source','executable'}<=roles,'qualified producer build closure')
    e=d['executable'];p=Path(e['path']).resolve();core.require(str(p) in files and g[str(p)]['sha256']==e['sha256'],'qualified dialog executable');return p

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out','output-root','glyph-report','ui-report','source07','raw-copy-source','snapshot-source','snapshot-helper-source','marker-source','water-contract','image','producer-receipt'):p.add_argument('--'+name,type=Path,required=name not in ('root','producer-receipt','water-contract'),default=ROOT if name=='root' else None)
    p.add_argument('--plan-only',action='store_true');a=p.parse_args();root=a.root.resolve();out=core.checked_output(root,a.out,a.output_root);out.mkdir()
    g=dict(boot);probes={};bad=False;report={'schema':'fd2-f4-dialog-history-C-v1','ok':False,'plan_only':a.plan_only,'actual_C_cases':0,'original_execution_this_run':False,'GUI_or_build_this_run':False,'cases':[],
      'mask_qualification':'not measured for read-old-history compositor; zero transport placeholders are not masks or store counts',
      'scope':'finite typed component timelines and exact full VGA pairs; no natural full-text/PCM/hardware timing/feedback gate promotion'}
    try:
        snap(Path(__file__),g);resources={}
        for name,digest in [('FDOTHER.DAT',audit.OTHER_SHA),('FDTXT.DAT',audit.TEXT_SHA),('DATO.DAT',DATO_SHA)]:resources[name]=snap(root/'original_game'/name,g,digest)
        image=snap(a.image,g,core.IMAGE_SHA);source_contract(a.source07,a.raw_copy_source,[a.snapshot_source,a.snapshot_helper_source],a.marker_source,image,g)
        water_contract=json.loads(snap(a.water_contract or root/'build/f4/palette-render-contract01.json',g,WATER_CONTRACT_SHA))
        core.require(water_contract['schema']=='fd2-f4-palette-render-contract-v1' and water_contract['guards_unchanged'] and
            water_contract['water']['raw_source_exact'] and water_contract['water']['va']==0x60003 and
            water_contract['RGB_expand_64_components']==[(i<<2)|(i>>4) for i in range(64)],'inherited independent raw water/RGB contract')
        original=Original(a.glyph_report.resolve(),a.ui_report.resolve(),resources,g);original.strict_image=image;work=cases();report['finite_request_count']=len(work)
        if not a.plan_only:
            core.require(a.producer_receipt is not None,'qualified build required before C launch');exe=producer(a.producer_receipt,root,g)
            probes['scene']=core.Probe(exe,root,out,'scene',30,resources,mode='--f4-dialog',max_page_bytes=128000)
        with (out/'cases.jsonl').open('x',encoding='utf-8') as log:
            for index,c in enumerate(work):
                wanted,metadata=expected(original,c['request']);row={**c,'index':index,'expected_sha256':sha(wanted),'expected_protocol':metadata,'actual_C_executed':False}
                if not a.plan_only:
                    h,got=probes['scene'].request(c)
                    core.require(h.get('history_mask_measured') is False and h.get('palette_semantics')=='actualDAC' and h.get('buffer_semantics')=='two-independent-VGA-pages','history is not a value/physical mask')
                    diff=core.mismatch(wanted,got,128000);md={k:{'expected':v,'actual':h.get(k)} for k,v in metadata.items() if h.get(k)!=v}
                    row.update(actual_C_executed=True,actual_sha256=sha(got),difference=diff,protocol_difference=md,actual_protocol={k:h.get(k) for k in metadata});report['actual_C_cases']+=1
                    if diff or md:
                        for kind,blob in [('expected',wanted),('actual',got)]:
                            with (out/f'{kind}-{index:03d}.zbin').open('xb') as f:f.write(zlib.compress(blob,9))
                        log.write(json.dumps(row)+'\n');log.flush();report['cases'].append(row);raise ValueError(f'dialog composition mismatch: {c["request"]}: {diff or md}')
                log.write(json.dumps(row)+'\n');log.flush();report['cases'].append(row)
        report.update(ok=True,retained_glyphs_used=len(original.glyphs),retained_DATO_parameter_leaves_used=len(original.portraits),full_VGA_buffers=len(work)*2)
    except BaseException:
        bad=True;report['error']=traceback.format_exc();core.write_diagnostic(out/'failure.txt',report['error'],report,'first business')
    finally:
        for name,probe in probes.items():
            try:probe.close(bad)
            except BaseException:core.secondary_error(report,'owned '+name,traceback.format_exc())
        report['C_loaded_input_bindings']={name:p.bound_inputs for name,p in probes.items()};core.finish_report(out,report,g)
    print(json.dumps({k:report.get(k) for k in ('ok','finite_request_count','actual_C_cases','full_VGA_buffers','guards_unchanged','error','secondary_errors')}));return 0 if report['ok'] else 1
if __name__=='__main__':raise SystemExit(main())
