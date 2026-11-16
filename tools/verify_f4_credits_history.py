"""Actual complete credits cold walker versus independent source/retained leaves.

Only start(case,0,0) is sent. Neither expected events nor cold graph nodes are
producer commands. No original CPU, GUI, audio device, build or resource writes.
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
if not (ROOT/'tools/verify_f4_text_composition.py').is_file():ROOT=Path.cwd()
BOOT={}
def frozen_module(name,path):
    path=Path(path).resolve();before=path.stat();raw=path.read_bytes();after=path.stat()
    if(before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):raise ValueError('source module changed while reading')
    BOOT[str(path)]={'path':str(path),'sha256':hashlib.sha256(raw).hexdigest(),'size':len(raw),'mtime_ns':after.st_mtime_ns}
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module;exec(compile(raw,str(path),'exec'),module.__dict__);return module
text=frozen_module('f4_credits_retained_text',ROOT/'tools/verify_f4_text_composition.py')
sha,snapshot=text.sha,text.snapshot
BOOT.update(text.BOOT)
core,audit=text.core,text.audit
SOURCE_SHA='a86a98ed7ca90031edef0eb80cce2af1028fb0cf56a809ece3d620178670bdac'
REMAINDER_SHA='6c95a250ec75333a35930c026e37160c4cd6c21632d4a7673aace2cf17fddb30'
RESOURCE_SHA={**core.RESOURCES,'FDTXT.DAT':audit.TEXT_SHA,
    'FDFIELD.DAT':'81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d'}
INPUTS=('FDOTHER.DAT','FDTXT.DAT','FIGANI.DAT','DATO.DAT','FDFIELD.DAT','FDSHAP.DAT','FDICON.B24','TAI.DAT')
REQUIRED_SOURCES={
 'src/tests/s5_stage_source_test.c','src/tests/f4_credits_private.h','src/tests/f4_credits_cold_probe.h',
 'src/tests/f4_reader_probe.h','src/tests/f4_montage_cold_probe.h',
 'src/scene.c','src/scene_cinematic.c','src/scene_cinematic.h','src/scene_cinematic_flow.h',
 'src/static_data.c','src/static_data.h','src/field_game.c','src/field_prepare.c','src/field_unit_base.c',
 'src/field_info.c','src/image.c','src/portrait.c','src/font.c','src/figani.c','src/input.c','src/bios_clock.c',
 'data/static/flows/stage.json','src/CMakeLists.txt'}

def darkness(palette,amount):
    core.require(0<=amount<=63 and len(palette)==768 and max(palette)<=63,'source palette/darkness')
    return bytes(((max(0,c-amount)<<2)|(max(0,c-amount)>>4))for c in palette)

def actor_path(case):
    core.require(type(case)is int and case in(0,1),'finite source input plan')
    # 31e1b count-1, 31e2c --j; 3219d input writes j=1, without breaking n.
    j=2;visits=[];inputs=[]
    while j>=0:
        core.require(len(visits)<3,'input plan must terminate through the source loop')
        visit=len(visits);visits.append(j)
        for frame in range(220 if j else 440):
            if case and ((visit,frame)==(0,17)or(visit,frame)==(1,218)):
                inputs.append((visit,frame));j=1
        j-=1
    return visits,inputs

def roll_darkness():
    value=40
    for i in range(500):
        yield value
        if i<200 and value and i%5==0:value-=1
        if i>300 and i%5==0:value+=1

def complete_owners(source,image,owners):
    result={}
    for va in owners:
        fs=[f for f in source['functions']if f['va']==va];core.require(len(fs)==1,'complete credits source owner')
        f=fs[0];covered=set();decoded=set()
        for c in f['chunks']:
            raw=bytes.fromhex(c.get('raw_hex',c.get('bytes','')))
            core.require(image[c['start']:c['end']]==raw and sha(raw)==c['sha256'],'credits strict chunk')
            covered.update(range(c['start'],c['end']))
        for ins in f['instructions']:
            a=ins.get('ea',ins.get('va'));n=ins['size']
            if 'raw_hex'in ins:core.require(image[a:a+n].hex()==ins['raw_hex'],'credits instruction bytes')
            decoded.update(range(a,a+n))
        core.require(covered and covered==decoded and f['blocks'],'credits complete CFG/head coverage')
        result[va]=f
    return result

def complete_source(path,image,guards):
    source=json.loads(snapshot(path,guards,SOURCE_SHA))
    core.require(source['image_sha256']==core.IMAGE_SHA and source['strict_byte_match'] is True,'credits source image/strict qualification')
    result=complete_owners(source,image,(0x31c49,0x2e9a8,0x311e5))
    # Ordinary source joins (not names/comments or CFG generation as expected).
    body={i['ea']:i for i in result[0x31c49]['instructions']}
    for va,raw in {0x31e20:'48',0x3219d:'c744241401000000',0x321aa:'47'}.items():
        core.require(body[va]['raw_hex']==raw,'source --j/input j=1/card counter')
    return result

class Original(text.Original):
    def __init__(self,args,resources,guards):
        super().__init__(args.glyph_report.resolve(),args.ui_report.resolve(),resources,guards)
        self.resources=resources;self.full_path=args.glyph_report.resolve();self.cache={}
        self.figani=audit.archive_entries(resources['FIGANI.DAT'])
        raw=snapshot(args.catalog,guards,core.CATALOG_SHA)
        self.catalog={r['key']:r for r in map(json.loads,raw.decode().splitlines())}
        core.require(len(self.catalog)==14711,'original resource catalog denominator')
        self.original_rows={(r['asset'],r['variant']):r for r in self.rows['results']}
        self.remainder_path=args.remainder_report.resolve()
        rem=json.loads(snapshot(self.remainder_path,guards,REMAINDER_SHA))
        core.require(rem['ok'] and rem['guards_unchanged'],'retained remainder original qualification')
        hits=[r for r in rem['cases']if r['request']==['battle','direct',56,0,0]]
        core.require(len(hits)==1,'unique FDOTHER56 original leaf')
        r=hits[0];block=audit.archive_entries(resources['FDOTHER.DAT'])[56]
        core.require(r['source_sha256']==sha(block),'same BASE56 source')
        n=64000;packed=r['original_payload']
        data=core.bounded_unpack(snapshot(self.remainder_path.parent/packed['path'],guards,packed['sha256']),2*n+1536)
        page,pal,mask,pmask=core.split_payload(data,n)
        core.require(mask==bytes([1])*n and not any(pmask) and r['stride']==320 and r['height']==200,'BASE56 full source-defined page')
        self.base=page
        tai=self.image_extra('tai',3)
        core.require(tai[0:2]==(10,3) and not any(tai[3]),'TAI3 actual no-store original mask')

    def image_extra(self,op,index):
        key=('extra',op,index)
        if key not in self.cache:
            rows=[r for r in self.ui['cases']if r['request']==['scene',op,index,0,0]]
            core.require(len(rows)==1,'unique extra original leaf');r=rows[0]
            block=audit.archive_entries(self.resources['TAI.DAT'])[index]
            core.require(r['source_sha256']==sha(block),'same TAI3 source')
            n=r['width']*r['height'];p=r['original_payload']
            data=core.bounded_unpack(snapshot(self.ui_report.parent/p['path'],self.guards,p['sha256']),2*n+1536)
            page,pal,mask,pmask=core.split_payload(data,n)
            core.require(set(mask)<={0,1} and not any(pmask),'extra original mask')
            self.cache[key]=(r['width'],r['height'],page,mask)
        return self.cache[key]

    def original(self,asset,variant):
        key=('leaf',asset,variant)
        if key not in self.cache:
            r=self.original_rows[(asset,variant)];row=self.catalog[asset]
            block=self.resources[row['file']][row['start']:row['end']]
            core.require(sha(block)==r['source_sha256']==row['source_sha256'],'same retained original leaf bytes')
            n=r['page_bytes'];raw=core.bounded_unpack(snapshot(self.full_path.parent/r['payload'],self.guards,r['payload_sha256']),2*n+1536)
            page,pal,mask,pmask=core.split_payload(raw,n)
            core.require(sha(page)==r['pixels_sha256'] and sha(mask)==r['write_mask_sha256'] and set(mask)<={0,1} and not any(pmask),'original page/mask identity')
            self.cache[key]=(row,page,mask)
        return self.cache[key]

    def clip(self,entry):
        raw=self.figani[entry];count=raw[0]
        core.require(count and len(raw)>=8+4*count,'source clip directory')
        parts=[]
        for frame in range(count):
            at=struct.unpack_from('<I',raw,8+4*frame)[0]
            end=struct.unpack_from('<I',raw,12+4*frame)[0]if frame+1<count else len(raw)
            core.require(8+4*count<=at<end<=len(raw) and end-at>=13,'source clip part extent')
            row,pixels,mask=self.original(f'figani:{entry}:{frame}','normal')
            x,y=struct.unpack_from('<HH',raw,at)
            core.require((x,y)==(row['raw_x'],row['raw_y']) and sha(raw[at+9:end])==row['source_sha256'],'source FIGANI affine binding')
            parts.append({'x':x,'y':y,'width':row['width'],'height':row['height'],'pixels':pixels,'mask':mask,'ticks':raw[at+6]})
        core.require(raw[2]<=count,'normal intro_end source bound')
        return {'parts':parts,'intro_end':raw[2]}

    def portrait(self,actor,frame):
        row,page,mask=self.original(f'portrait:{actor}:{frame}','forward')
        core.require((row['width'],row['height'])==(80,80),'portrait geometry')
        points={(y+5)*320+x+232 for y in range(80)for x in range(80)}
        core.require(sum(mask)==6400 and all(mask[i]for i in points),'original portrait full top window')
        return bytes(page[(y+5)*320+x+232]for y in range(80)for x in range(80))

    def raw_border(self,page):
        # 31f87→168b6(buffer,320,5,7,5,5), exact ordered tile calls.
        fixed=[(1,5,7),(2,88,7),(3,5,90),(4,88,90),(5,8,7),(6,72,7),
               (7,8,90),(8,72,90),(14,5,10),(15,88,10),(16,5,74),(17,88,74)]
        points=fixed+[(f,24+16*i,y)for i in range(3)for f,y in((9,7),(12,90))]+[
            (f,x,10+16*j)for j in range(1,4)for f,x in((10,5),(11,88))]+[
            (13,8+16*x,10+16*y)for y in range(5)for x in range(5)]
        for frame,x,y in points:
            raw=self.ui_blocks[frame];w,h=struct.unpack_from('<HH',raw)
            core.require(len(raw)==4+w*h,'source raw border tile extent')
            for row in range(h):page[(y+row)*320+x:(y+row)*320+x+w]=raw[4+row*w:4+(row+1)*w]

def draw(page,part,x=0,y=0,stride=320):
    for i,written in enumerate(part['mask']):
        if written:
            at=(y+part['y']+i//part['width'])*stride+x+part['x']+i%part['width']
            core.require(0<=at<len(page),'source-defined destination owner')
            page[at]=part['pixels'][i]

class History:
    """Finite 31c49 source algebra; does not read C/cold to choose any step."""
    def __init__(self,original,texts,image,case):
        self.o,self.texts,self.image,self.case=original,texts,image,case
        self.palette=audit.archive_entries(original.resources['FDOTHER.DAT'])[0]
        self.rng=self.blink=self.idle_part=self.idle_ticks=self.keys=self.wait_nodes=self.wait_ticks=self.clocks=self.epochs=0
        self.visits=0;self.outer=99;self.card_frame=0;self.number=0;self.copy=[0]*8;self.dark=0
        self.page=bytearray((i*17+i//320+83)&255 for i in range(64000));self.dac=darkness(self.palette,0)

    def wait(self,ticks):
        self.wait_nodes+=1;self.wait_ticks+=ticks;self.clocks+=max(1,ticks)+2

    def event(self,stage,source_op,*,dark=None):
        if dark is not None:self.dark=dark;self.dac=darkness(self.palette,dark)
        meta={'stage':stage,'source_op':source_op,'outer':self.outer,'actor_visits':self.visits,
            'card_frame':self.card_frame,'rng':self.rng,'keys':self.keys,'queued':0,
            'wait_nodes':self.wait_nodes,'wait_ticks':self.wait_ticks,'epochs':self.epochs,
            'blink_resets':1,'clock_reads':self.clocks,'input_pumps':0,
            'blink':self.blink,'idle_part':self.idle_part,'idle_ticks':self.idle_ticks,
            'last_copy':self.copy[:],'darkness':self.dark}
        op='start'if self.number==0 else 'frame'
        case={'request':['credits',op,self.case,self.number,0],'stride':320,'height':200,**meta}
        self.number+=1
        return case,bytes(self.page)+self.dac+bytes(64768)

    def idle(self,page,clip):
        part=clip['parts'][self.idle_part];draw(page,part)
        self.idle_ticks=(self.idle_ticks+1)&255
        if self.idle_ticks>=part['ticks']:
            self.idle_ticks=0;self.idle_part=(self.idle_part+1)%len(clip['parts'])

    def events(self):
        roll=bytearray(224000);mask=bytearray(224000)
        self.o.literal(roll,mask,self.texts[31][44],76848,320,25)
        for index,dark in enumerate(roll_darkness()):
            self.page[:]=roll[index*320:index*320+64000];self.copy=[4,0,index*320,0,320,200,320,320]
            yield self.event('roll','cin_present',dark=dark);self.wait(1)
        base=bytearray(self.o.base)
        visits,inputs=actor_path(self.case)
        for visit,outer in enumerate(visits):
            self.epochs+=1;self.visits+=1;self.outer=outer;self.card_frame=0
            actor=1 if outer==0 else 0 if outer==1 else outer
            idle,intro=self.o.clip(actor*3),self.o.clip(actor*3+1)
            for phase in range(8,-1,-1):
                work=bytearray(128000)
                for y in range(200):work[y*640:y*640+320]=base[y*320:y*320+320]
                # 1088d's three normal player records all have side2. Actual TAI3
                # is loaded and affine-routed, but retained mask proves zero stores.
                draw(work,idle['parts'][0],phase*10,0,640)
                self.page[:]=b''.join(work[y*640:y*640+320]for y in range(200))
                yield self.event('intro','palette_subtract',dark=phase*6)
            self.idle_part=self.idle_ticks=0
            for _ in range(20):
                self.page[:]=base;self.copy=[1,0,0,0,320,200,320,320];self.idle(self.page,idle)
                yield self.event('idle','cin_present');self.wait(1)
            for part in intro['parts'][:intro['intro_end']]:
                self.page[:]=base;draw(self.page,part)
                yield self.event('part','cin_present');self.wait(part['ticks'])
            card=bytearray(base);self.o.raw_border(card)
            overlays={}
            for second in(0,1):
                overlay=bytearray(128000);omask=bytearray(128000)
                profile=self.image[0x61da1+actor*24+1]
                for tokens,offset,pitch in[(self.texts[31][10],5865,0),(self.texts[0][actor+1],5915,0),
                    (self.texts[31][11],12265,0),(self.texts[0][profile+150],12315,0),
                    (self.texts[31][45 if second else actor+12],32008,20)]:
                    self.o.literal(overlay,omask,tokens,offset,320,pitch)
                core.require(not any(omask[64000:]),'selected normal literal source footprints inside displayed page')
                overlays[second]=[(i,overlay[i])for i,w in enumerate(omask[:64000])if w]
            portraits=[self.o.portrait(actor,f)for f in(0,3)]
            for frame in range(440 if outer==0 else 220):
                self.card_frame=frame;self.page[:]=card
                if self.blink:self.blink-=1
                else:self.blink=40;self.rng+=1
                portrait=portraits[int(self.blink<2)]
                for y in range(80):self.page[(y+10)*320+8:(y+10)*320+88]=portrait[y*80:(y+1)*80]
                self.idle(self.page,idle)
                for at,color in overlays[int(frame>=220)]:self.page[at]=color
                yield self.event('card','cin_present');self.wait(1)
                if(visit,frame)in inputs:self.keys+=1
            for dark in range(64):yield self.event('fade','palette_subtract',dark=dark)
        case,expected=self.event('done','end')
        case['request'][1]='done';# done frame index is the next counter, not the previous image index
        yield case,expected

def producer(path,root,guards):
    doc=json.loads(snapshot(path,guards))
    core.require(doc['schema']=='fd2-f4-credits-producer-v1' and Path(doc['source_root']).resolve()==root,'credits producer source root/schema')
    core.require(REQUIRED_SOURCES<=set(doc['sources']),'actual CL/source direct closure incomplete')
    for rel,digest in doc['sources'].items():
        p=(root/rel).resolve();core.require(p.is_relative_to(root),'producer source root boundary');snapshot(p,guards,digest)
    roles=set();seen=set()
    for f in doc['files']:
        p=Path(f['path']).resolve();core.require(str(p)not in seen,'producer file repeated');seen.add(str(p))
        snapshot(p,guards,f['sha256']);fact=guards[str(p)]
        core.require((fact['size'],fact['mtime_ns'])==(f['size'],f['mtime_ns']),'producer exact compiled identity');roles.add(f['role'])
    core.require({'build_log','tlog','static_library','producer_source','executable'}<=roles,'qualified actual producer artifacts')
    f=doc['executables']['credits'];p=Path(f['path']).resolve()
    core.require(str(p)in seen and guards[str(p)]['sha256']==f['sha256'],'credits PE is qualified artifact')
    return p

def check_header(header,case,probe,cold):
    core.require(header.get('history_mask_measured')is False and header.get('palette_semantics')=='actualDAC','history is actual page/DAC, no invented mask')
    for key in('source_op','outer','actor_visits','card_frame','rng','keys','queued','wait_nodes','wait_ticks',
                'epochs','blink_resets','clock_reads','input_pumps','blink','idle_part','idle_ticks','last_copy'):
        core.require(header.get(key)==case[key],f'credits {case["stage"]} source prefix {key}: {header.get(key)} != {case[key]}')
    flow,node=header.get('flow'),header.get('node')
    core.require(flow in cold and node in cold[flow] and cold[flow][node]['op']==case['source_op'],'actual formal cold binding')
    core.require(cold[flow][node]['args']==([0,255,case['darkness']]if case['source_op']=='palette_subtract'else []),'source-qualified palette/cold argument binding')
    expected_flow={'roll':'stage.cinematic.credits','intro':'stage.cinematic.card_first',
        'idle':'stage.cinematic.card_first','part':'stage.cinematic.card_first',
        'card':'stage.cinematic.card_last'if case['card_frame']>=220 else 'stage.cinematic.card_first',
        'fade':'stage.source.fade.out','done':'stage.cinematic.credits'}[case['stage']]
    core.require(flow==expected_flow,'actual source call family/order')
    if case['stage']not in('roll',):
        core.require(probe.bound_inputs.get('TAI.DAT',{}).get('outputs_before_handshake')==500,'actual TAI must bind exactly after ROLL500 and before any card output')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('out','output-root','glyph-report','ui-report','remainder-report','catalog','source','source07','raw-copy-source','image'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--root',type=Path,default=ROOT);p.add_argument('--producer-receipt',type=Path)
    p.add_argument('--plan-only',action='store_true');args=p.parse_args()
    root=args.root.resolve();out=core.checked_output(root,args.out,args.output_root);out.mkdir()
    guards={**BOOT,**text.BOOT};report={'schema':'fd2-f4-credits-cold-history-v1','ok':False,'actual_C_frames':0,
        'original_CPU_executed_this_run':0,'GUI_or_build_executed_this_run':False,'plan_only':args.plan_only,
        'scope':'complete fixed credits cold entry and all subflows; controlled legal constructor0/1/2, constant RNG0 and two finite ordinary input plans',
        'limits':['no original malloc-unused WORK bytes, physical masks/PIT/audio/absolute display timing or all-natural identity producer claim',
                  'glyph/FIGANI/portrait/BASE56/TAI original leaves retained; source order/affine/opaque-raw border composition derived independently',
                  'not entire ending/montage/ANI; no clipping domain narrowed to these three representative actors'], 'cases':[]}
    probe=None;failed=False
    try:
        snapshot(Path(__file__),guards)
        resources={name:snapshot(root/'original_game'/name,guards,RESOURCE_SHA[name])for name in INPUTS}
        image=snapshot(args.image,guards,core.IMAGE_SHA);complete_source(args.source,image,guards)
        complete_owners(json.loads(snapshot(args.source07,guards,text.SOURCE07_SHA)),image,(0x168b6,0x2eb9f))
        complete_owners(json.loads(snapshot(args.raw_copy_source,guards,text.RAW_COPY_SHA)),image,(0x4ed0b,))
        cold_doc=json.loads(snapshot(root/'data/static/flows/stage.json',guards))
        cold={f['id']:{n['id']:n for n in f['nodes']}for f in cold_doc['flows']if f['id']in(
            'stage.cinematic.credits','stage.cinematic.card_first','stage.cinematic.card_normal','stage.cinematic.card_last','stage.source.fade.out')}
        core.require(len(cold)==5,'fixed formal credits source graph set')
        texts=[audit.text_fragments(b)for b in audit.archive_entries(resources['FDTXT.DAT'])]
        oracle=Original(args,resources,guards)
        core.require(args.plan_only or args.producer_receipt is not None,'current qualified producer receipt required before C launch')
        executable=None if args.plan_only else producer(args.producer_receipt,root,guards)
        with(out/'frames.jsonl').open('x',encoding='utf-8')as log:
            for number in(0,1):
                history=History(oracle,texts,image,number);counts={};cases=0
                if not args.plan_only:
                    probe=core.Probe(executable,root,out,f'credits-{number}',60,resources,mode='--f4-credits-cold',
                        max_page_bytes=64000,deferred_inputs=frozenset({'TAI.DAT'}))
                for case,expected in history.events():
                    row={**case,'expected_sha256':sha(expected),'actual_C_executed':False}
                    counts[case['stage']]=counts.get(case['stage'],0)+1
                    if not args.plan_only:
                        if cases==0:
                            # One scalar command, then only receive; no expected trace is fed to C.
                            first={**case,'request':['credits','start',number,0,0]};header,actual=probe.request(first)
                        else:header,actual=probe.receive(case)
                        core.validate_actual(header,actual);check_header(header,case,probe,cold)
                        delta=core.mismatch(expected,actual,64000);row.update(actual_C_executed=True,match=delta is None,difference=delta,actual_sha256=sha(actual))
                        report['actual_C_frames']+=1
                        if delta:
                            for kind,raw in(('expected',expected),('actual',actual)):(out/f'{kind}-{number}-{cases:04d}.zbin').write_bytes(zlib.compress(raw,9))
                            log.write(json.dumps(row)+'\n');log.flush();raise ValueError(f'credits case{number} frame{cases}: {delta}')
                    log.write(json.dumps(row)+'\n');cases+=1
                if probe:
                    probe.close(False);report.setdefault('C_loaded_inputs',{})[str(number)]=probe.bound_inputs;probe=None
                report['cases'].append({'case':number,'actors':actor_path(number)[0],'input_points':actor_path(number)[1],
                    'frames_including_final_guard':cases,'counts':counts,'RNG_calls':history.rng,'keys':history.keys,
                    'wait_nodes':history.wait_nodes,'wait_ticks':history.wait_ticks,'clock_reads':history.clocks})
                log.flush()
        report['ok']=True
    except BaseException:
        failed=True;report['error']=traceback.format_exc();core.write_diagnostic(out/'failure.txt',report['error'],report,'primary error')
    finally:
        if probe:
            try:probe.close(failed)
            except BaseException:core.secondary_error(report,'owned credits probe cleanup',traceback.format_exc())
        core.finish_report(out,report,guards)
    print(json.dumps({k:report.get(k)for k in('ok','actual_C_frames','guards_unchanged','error','secondary_errors')}))
    return 0 if report['ok']else 1
if __name__=='__main__':raise SystemExit(main())
