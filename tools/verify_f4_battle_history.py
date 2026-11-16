"""Source-derived DERIVED histories, composed from retained original pixel leaves.

No decoder/original CPU/build/GUI. A pinned private Battle test CLI is optional
only in pair. Full CAST and two-target part loops are observed at every upload
and caller boundary; first work-before is explicit, not a malloc default claim.
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
import verify_f4_text_composition as text
from verify_f4_background_pixels import archive_entry

sha,require,freeze,guards=core.sha,core.require,core.freeze,core.guards
PAYLOAD=302336
EXTRA_SHA='d4382319728dd6ea685b99e3e2950cf486be68c9cc5d9f6c4fe7e2cf8e4a316d'
MORE_PINS={'extra':EXTRA_SHA,'bg':'b9fc21d019d6256a4bb7e6da1cefcb0bfe331d8ff74a52a8201570afc98b56de',
 'tai':'d56fea9c43f8bb59aad89ad76698885d7e07f380d12a4547888a0b60ea5e0410',
 'other':'882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667',
 'texts':'b47e39ee727482528da061a8705f5cdee8a9cf759447aeda298afe17d9dd8bcc',
 'source07':text.SOURCE07_SHA,'raw_copy_source':text.RAW_COPY_SHA}
CASTERS={24:32,28:19,29:34,30:18,31:32}
SHAKE=[0,4,9,14,18,14]
SOURCES=[*core.SOURCES,'src/tests/f4_battle_history_probe.h','src/tests/f4_map_item_history_probe.h',
 'src/tests/f4_promotion_composition_probe.h','src/tests/f4_magic_linear_work_probe.h',
 'src/field_info.c','src/field_info.h','src/field_page.c','src/portrait.c','src/font.c','src/font.h',
 'src/text.c','src/text.h','data/static/tables/magic.json','src/CMakeLists.txt']
SCOPE=('DERIVED24/28..31 CAST and two-target complete part/marker/idle loops: '
 'given defined before, whole work/VGA/board/base/DAC, source pan/summary/MP/HP timing; '
 'side2 uses original2cf30 left-pan caller; side0 composes independent right-pan2f631 callee, '
 'not an original2cf30 same-input trajectory; pre-skip and first summary real-owner error prefixes are separate host controls.')
EXCLUDED=('No natural spell/caster/MP producer proof; ID31 caster32 is explicit callee input. '
 'No original whole-wrapper CPU, first fresh-work/default/heap proof, Game effect RNG/commit, '
 'normal FINISH return or HOST discarded-temporary-state claim; no audio/device/live-clock qualification. '
 'Original work is128000; C44800 extra bytes are preservation guards.')
KEYS=['kind','phase','part','frame','target','next_part','markers','idle_part','idle_ticks','shake','tint',
 'caster_mp','active_hp','cast_done','started','failed','status','ticks','delays','cues','last_cue','pumps','payload_bytes']

class OwnerFailure(Exception):
    def __init__(self,at):self.at=at;super().__init__(f'first source owner store {at}')

class Original(text.Original):
    def put(self,page,mask,at,color):
        if not 0<=at<len(page):raise OwnerFailure(at)
        page[at]=color;mask[at]=1

def requests():
    return [[id,side,bottom,0] for id in CASTERS for side in (0,2) for bottom in (0,1)]+[
        [28,0,1,1],[24,0,1,2],[24,2,1,2]]

def validate(v):
    require(type(v) is list and len(v)==4 and all(type(x) is int for x in v),'four exact integer fields')
    id,side,bottom,control=v
    require(id in CASTERS and side in (0,2) and bottom in (0,1) and control in (0,1,2) and
        (control!=1 or id==28) and (control!=2 or id==24),'finite declared callee/control domain')
    return v

def copy_rows(dst,offset,stride,src):
    require(len(src)==64000 and 0<=offset and offset+199*stride+320<=128000,'source copy owner')
    for y in range(200):dst[offset+y*stride:offset+y*stride+320]=src[y*320:(y+1)*320]

def view(work,offset=0):
    require(0<=offset and offset+199*640+320<=128000,'source view owner')
    return b''.join(work[offset+y*640:offset+y*640+320] for y in range(200))

def draw(dst,origin,stride,meta,picture,x=0,y=0,tint=-1,owner=None):
    pixels,mask=picture;w,h=meta['width'],meta['height']
    require(len(pixels)==len(mask)==w*h,'original pixel owner')
    owner=len(dst) if owner is None else owner
    for row in range(h):
        for col in range(w):
            i=row*w+col
            if mask[i]:
                at=origin+x+meta.get('x',0)+col+stride*(y+meta.get('y',0)+row)
                if not 0<=at<owner:raise OwnerFailure(at)
                dst[at]=pixels[i] if tint<0 else tint

def summary(page,oracle,tokens,side,hp,mp):
    """18c6d ->17d6f/18795/187d6, exact finite positive record, real linear owner."""
    offset=154*320 if not side else 4*320+171;mask=bytearray(64000)
    oracle.blit(page,mask,22,offset,320)
    for bank,current,maximum,row in [(0,hp,42,22),(1,mp,999,31)]:
        at=offset+21+row*320;length=0 if current==0 else current*101//maximum+1
        if length==0:
            for col in range(1,102):oracle.raw_bar(page,mask,29,at+col,320)
            oracle.raw_bar(page,mask,30,at+102,320)
        else:
            oracle.raw_bar(page,mask,23+3*bank,at,320)
            for col in range(1,length):oracle.raw_bar(page,mask,24+3*bank,at+col,320)
            oracle.raw_bar(page,mask,25+3*bank,at+max(1,length),320)
    for value,maximum,at,count in [(1,1,offset+132+4*320,2),(hp,42,offset+126+21*320,3),(mp,999,offset+126+30*320,3)]:
        bank=31 if value==maximum else 42
        if count==2:bank=31
        require(0<=value<10**count,'finite normal number domain')
        for i,digit in enumerate(f'{value:0{count}d}'):oracle.blit(page,mask,bank+int(digit),at+6*i,320)
    oracle.literal(page,mask,tokens,offset+5+4*320,320)

class Derived:
    def __init__(self,v,parts,images,bgs,terrain,oracle,tokens,base,cost):
        self.id,self.side,self.bottom,self.control=validate(v);self.skip=self.control==1
        self.parts=parts;self.images=images;self.bgs=bgs;self.terrain=terrain;self.oracle=oracle;self.tokens=tokens
        self.work=bytearray(core.seed(172800,23,640,41,self.bottom));self.vga=bytearray(core.seed(64000,19,320,61,self.bottom))
        self.board=bytearray(64000);self.base=base;self.dac=bytes((x<<2)|(x>>4) for x in base)
        self.caster=CASTERS[self.id];self.clip=parts['clips'][self.caster*3+2];self.cost=cost
        self.target=self.markers=self.idle=self.age=self.shake=self.frame=self.ticks=self.delay=self.cues=self.last_cue=self.pumps=0
        self.tint=-1;self.next_part=self.clip['intro'];self.part=-1;self.phase=0
        self.cast_done=self.started=self.failed=False;self.mp=999;self.hp=[1,1]
        self.current_hp=1
        summary(self.board,oracle,tokens,self.side,1,999)
        if self.id==28:summary(self.board,oracle,tokens,0 if self.side else 2,1,999)
        self.image(self.board,0,320,bgs[4 if self.id==28 else 3],0,50)
    def image(self,dst,origin,stride,image,x=0,y=0):
        meta,pm=image;draw(dst,origin,stride,meta,pm,x,y)
    def fig(self,dst,origin,stride,entry,part,x=0,y=0,tint=-1):
        key=f'figani:{entry}:{part}';draw(dst,origin,stride,self.parts['parts'][key],self.images[key],x,y,tint,
            128000 if dst is self.work else len(dst))
    def point(self,kind,status=0):
        return dict(kind=kind,phase=self.phase,part=self.part,frame=self.frame,target=self.target,
            next_part=self.next_part,markers=self.markers,idle_part=self.idle,idle_ticks=self.age,shake=self.shake,tint=self.tint,
            caster_mp=self.mp,active_hp=self.current_hp if self.target<2 else 0,cast_done=self.cast_done,
            started=self.started,failed=self.failed,status=status,ticks=self.ticks,delays=self.delay,
            cues=self.cues,last_cue=self.last_cue,pumps=self.pumps,payload_bytes=PAYLOAD),\
            bytes(self.work)+bytes(self.vga)+bytes(self.board)+self.base+self.dac
    def upload(self):
        if not self.skip:
            self.pumps+=1;point=self.point('upload');self.frame+=1;yield point
    def cue(self,value):
        if not self.skip:self.cues+=1;self.last_cue=value
    def fade(self):
        for amount in range(64):
            self.dac=bytes(((max(0,x-amount)<<2)|(max(0,x-amount)>>4)) for x in self.base)
            yield from self.upload();self.delay+=2
    def intro(self):
        split=self.id!=28;side=bool(self.side)
        if not side and not split:self.image(self.board,0,320,self.terrain,164,157)
        for i in range(8,-1,-1):
            origin=0 if side else 320;copy_rows(self.work,origin,640,self.board)
            if side and not split:self.fig(self.work,0,640,0,0)
            if side:self.image(self.work,0,640,self.terrain,164+10*i,157)
            self.fig(self.work,0,640,self.caster*3,0,origin+(10*i if side else -10*i))
            if not side and not split:self.fig(self.work,0,640,0,0,320)
            self.vga[:]=view(self.work,origin)
            self.dac=bytes(((max(0,x-6*i)<<2)|(max(0,x-6*i)>>4)) for x in self.base)
            yield from self.upload()
        if side:self.image(self.board,0,320,self.terrain,164,157)
    def cast(self):
        self.cast_done=True
        summary(self.board,self.oracle,self.tokens,self.side,1,999)
        if not self.skip:
            self.started=True;yield from self.fade()
            self.dac=bytes((x<<2)|(x>>4) for x in self.base)
            yield from self.intro()
        idle=age=0
        for index in range(self.clip['intro']):
            p=self.clip['parts'][index]
            if self.id!=31 and p['cue']:self.cue(p['cue'])
            if p['marker']==1:
                self.mp-=self.cost;summary(self.board,self.oracle,self.tokens,self.side,1,self.mp)
            for tick in range(p['duration']):
                self.work[:64000]=self.board
                if self.side:self.fig(self.work,0,320,self.caster*3+2,index)
                if self.id==28:self.fig(self.work,0,320,0,idle)
                if not self.side:self.fig(self.work,0,320,self.caster*3+2,index)
                if not self.skip:self.vga[:]=self.work[:64000]
                yield from self.upload()
                if not self.skip:self.ticks+=1
                age+=1
                if age==self.parts['clips'][0]['parts'][idle]['duration']:
                    age=0;idle=(idle+1)%self.parts['clips'][0]['count']
        yield self.point('cast_end')
    def pan(self,target_unit):
        if self.skip:return
        right=not self.side
        for n in (range(1,10) if right else range(9,-1,-1)):
            self.image(self.work,0,640,self.bgs[n%3],320 if right else 0,50)
            self.vga[:]=view(self.work,32*n);yield from self.upload()
        self.work[:128000]=bytes(128000);self.board[:]=bytes(64000)
        self.image(self.board,0,320,self.bgs[4],0,50)
        if right:self.image(self.board,0,320,self.terrain,164,157)
        summary(self.board,self.oracle,self.tokens,2 if right else 0,1,999)
        copy_rows(self.work,320 if right else 0,640,self.board)
        self.fig(self.work,0,640,target_unit*3,0,320 if right else 0)
        for n in (range(1,11) if right else range(9,-1,-1)):
            overlay=(n+1)%3 if right else (n+2)%3
            self.image(self.work,0,640,self.bgs[overlay],0 if right else 320,50)
            self.vga[:]=view(self.work,32*n);yield from self.upload()
    def run(self):
        yield from self.cast()
        count=sum(p['marker']==1 for p in self.clip['parts'][self.clip['intro']:])
        require(count>0,'source effect markers')
        for target,unit in enumerate([0,96]):
            self.target=target;markers=0;self.current_hp=1
            for index in range(self.clip['intro'],self.clip['count']):
                self.part=index;self.phase=1;p=self.clip['parts'][index]
                if index==self.clip['intro']:
                    self.markers=self.idle=self.age=0
                    self.work[:128000]=bytes(128000);copy_rows(self.work,320,640,self.vga)
                    if self.id!=28:yield from self.pan(unit)
                if p['cue']:self.cue(p['cue'])
                if p['marker']==1:
                    markers+=1;self.markers=markers;self.shake=5;self.tint=33
                    self.current_hp=0 if markers==count else 1
                    if self.control==2 and target==0:self.current_hp=32767
                    try:summary(self.board,self.oracle,self.tokens,0 if self.side else 2,self.current_hp,999)
                    except OwnerFailure:
                        require(self.control==2 and target==0,'unexpected source owner failure')
                        self.failed=True;yield self.point('part_error',-1);self.phase=3;yield self.point('final',-1);return
                for tick in range(p['duration']):
                    copy_rows(self.work,320,640,self.board)
                    self.fig(self.work,320,640,unit*3,self.idle,-SHAKE[self.shake],tint=self.tint)
                    self.age+=1
                    if self.age>=self.parts['clips'][unit*3]['parts'][self.idle]['duration']:
                        self.age=0;self.idle=(self.idle+1)%self.parts['clips'][unit*3]['count']
                    self.fig(self.work,320,640,self.caster*3+2,index)
                    if not self.skip:self.vga[:]=view(self.work,320)
                    yield from self.upload()
                    if not self.skip:self.ticks+=1
                    if self.shake:self.shake-=1
                    self.tint=-1
                self.next_part+=1;yield self.point('part_end')
            self.phase=2;self.target=target+1;self.current_hp=1;self.next_part=self.clip['intro']
            yield self.point('target_end')
        self.phase=3;yield self.point('final')

def original_image(a,report,raw_resource,family,entry,facts):
    row=next(r for r in report['cases'] if r['request']==['scene',family,entry,0,0])
    blob=archive_entry(raw_resource,entry)
    require(row['source_sha256']==sha(blob) and row['match'],'same original BG/TAI source span')
    n=row['width']*row['height'];parent=a.extra.resolve().parent;item=row['original_payload'];path=(parent/item['path']).resolve()
    require(path.parent==parent,'BG/TAI payload containment');raw=core.unpack(freeze(path,facts,item['sha256']),2*n+1536)
    pixels,mask=raw[:n],raw[n+768:2*n+768]
    require(set(mask)<={0,1} and not any(raw[-768:]),'original BG/TAI mask/IO')
    return dict(width=row['width'],height=row['height']),(pixels,mask)

def read_inputs(a,facts):
    raw,parts,eligible,results,join=core.read_inputs(a,facts)
    more={k:freeze(getattr(a,k),facts,pin) for k,pin in MORE_PINS.items()}
    source=json.loads(raw['source']);action=json.loads(raw['action']);contracts=[]
    for va in (0x2cf30,0x30e9d,0x2e9a8,0x2f4d4,0x2f631,0x2facd):
        f=next((f for f in source['functions'] if f['va']==va),None)
        if f:contracts.append(core.source_check(raw['image'],source,va))
        else:
            f=next(f for f in action['functions'].values() if f['va']==va)
            require(f['strict_continuous_byte_coverage'],'complete inherited source owner')
            for c in f['chunks']:require(sha(raw['image'][c['start']:c['end']])==c['sha256'],'strict inherited source chunk')
            contracts.append(dict(va=va,chunks=f['chunks'],source=f['source'],source_sha256=f['source_sha256']))
    text_guards=dict(text.BOOT);resources={'FDOTHER.DAT':more['other'],'FDTXT.DAT':more['texts']}
    text.source_contract(a.source07,a.raw_copy_source,raw['image'],text_guards)
    oracle=Original(a.original_report.resolve(),a.extra.resolve(),resources,text_guards)
    tokens=text.audit.text_fragments(text.audit.archive_entries(more['texts'])[0])[1]
    extra=json.loads(more['extra']);require(extra['ok'] and extra['guards_unchanged'],'qualified original BG/TAI/UI')
    bgs={i:original_image(a,extra,more['bg'],'bg',i,facts) for i in range(5)}
    terrain=original_image(a,extra,more['tai'],'tai',3,facts)
    clips={c['entry']:c for c in action['figani']['clips']};pm={}
    entries={0,96*3}
    for caster in CASTERS.values():entries.update([caster*3,caster*3+2])
    for key,p in parts.items():
        if p['entry'] in entries:core.pixels(key,a,results,parts,facts,pm)
    require(all(p['duration']>0 for c in clips.values() if c['entry']%3==2 and not c['empty'] for p in c['parts']),
        'no actual zero-duration DERIVED slot2 part')
    costs={id:raw['image'][0x619fd+7*id+5] for id in CASTERS}
    base=archive_entry(more['other'],0)[:768];require(len(base)==768 and max(base)<64,'original base palette')
    return raw,more,dict(parts=parts,clips=clips),pm,bgs,terrain,oracle,tokens,base,costs,contracts,text_guards

def derive(a,root,facts):
    raw,more,parts,pm,bgs,terrain,oracle,tokens,base,costs,contracts,text_guards=read_inputs(a,facts)
    out=core.fresh(root,a.output);cases=[]
    for number,v in enumerate(requests()):
        filename=f'case-{number:02d}.gz';states=[]
        with gzip.open(out/filename,'xb') as stream:
            for point,data in Derived(v,parts,pm,bgs,terrain,oracle,tokens,base,costs[v[0]]).run():
                require(len(data)==PAYLOAD,'whole DERIVED owners');stream.write(data);states.append(dict(**point,sha256=sha(data)))
        require(states and states[-1]['kind']=='final','complete source caller history')
        cases.append(dict(case=number,request=v,payload=filename,sha256=sha((out/filename).read_bytes()),states=states,
            source_domain='host_control' if v[3] else 'original2cf30_left_pan' if v[1] else 'shared_right_callee_extension'))
        if number%5==0:print(json.dumps(dict(done=number+1,total=len(requests()))),flush=True)
    all_facts=facts+list(text_guards.values());identity=guards(all_facts);require(identity['unchanged'],'late original/source/payload guard')
    result=dict(schema='fd2-f4-battle-history-expected-v1',passed=True,cases=cases,
        inputs={**core.PINS,**MORE_PINS},source_contract=contracts,
        scope=SCOPE,excluded=EXCLUDED,source_selector='fixed slot2 for24/28..31, no fallback; ID31/32 explicit callee input',
        generated_frames=sum(sum(s['kind']=='upload' for s in c['states']) for c in cases),
        marker_and_idle_metadata='normalized logical target state; early reset before pan commutes because pan never reads311e5 globals',
        new_original_CPU=0,actual_C=0,GUI=0,identity=identity,primary_error=None,secondary_errors=[])
    core.save_report(out/'expected.json',result);require(result['passed'],'expected report write')
    print(json.dumps(dict(passed=True,path=str(out/'expected.json'),cases=len(cases),frames=result['generated_frames'])))

def compare(row,number,point,actual,wanted,byte_count=PAYLOAD,keys=KEYS):
    require(row==dict(schema='fd2-f4-battle-history-state-v1',case=number,**{k:point[k] for k in keys}),
        'actual complete part/marker/idle/clock state')
    require(len(wanted)==len(actual)==byte_count and sha(wanted)==point['sha256'],'complete original-derived packet')
    if actual!=wanted:
        at=next(i for i,(x,y) in enumerate(zip(actual,wanted)) if x!=y)
        raise ValueError(f'case{number} {point["kind"]}/target{point["target"]}/part{point["part"]} byte{at}: actual{actual[at]} expected{wanted[at]}')

def selected_cases(cases,selection=None):
    selection=list(range(len(cases))) if selection is None else selection
    require(type(selection) is list and selection and all(type(i) is int and 0<=i<len(cases) for i in selection) and
        len(set(selection))==len(selection),'finite distinct expected selection')
    return [dict(cases[old],case=stream,original_case=old) for stream,old in enumerate(selection)]

def pair(a,root,facts,family=None):
    # 只有两个固定私有调用者给协议；不是从CLI执行任意图或reader。
    family=family or dict(inputs={**core.PINS,**MORE_PINS},requests=requests(),sources=SOURCES,
        payload=PAYLOAD,keys=KEYS,cli='--f4-battle-history',scope=SCOPE,excluded=EXCLUDED)
    byte_count=family['payload']
    wanted=json.loads(freeze(a.expected,facts,a.expected_sha256))
    require(wanted['schema']=='fd2-f4-battle-history-expected-v1' and wanted['passed'] and wanted['identity']['unchanged'] and
        wanted['inputs']==family['inputs'] and [c['request'] for c in wanted['cases']]==family['requests'],'qualified complete expected')
    selected=selected_cases(wanted['cases'],family.get('selection'))
    resources={name:freeze(a.resource_root/name,facts,pin) for name,pin in [('FDOTHER.DAT',MORE_PINS['other']),('FDTXT.DAT',MORE_PINS['texts']),
        ('FIGANI.DAT',core.PINS['resource']),('BG.DAT',MORE_PINS['bg']),('TAI.DAT',MORE_PINS['tai'])]}
    freeze(a.producer,facts,a.producer_sha256);freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for path in family['sources']:freeze(root/path,facts)
    out=core.fresh(root,a.output);result=dict(passed=False,actual_C_cases=0,actual_C_frames=0,primary_error=None,secondary_errors=[],
        expected_total_cases=len(wanted['cases']),requested_cases=len(selected),original_case=[r['original_case'] for r in selected])
    process=reader=None;errors=[];digest=core.hashlib.sha256()
    def consume():
        try:
            with gzip.open(out/'actual.bin.gz','xb') as sink:
                result['loaded_inputs']=[]
                for name,resource in resources.items():
                    line,row=core.jsonline(process.stdout);digest.update(line)
                    require(row==dict(schema='fd2-f4-input-v1',input_name=name,payload_bytes=len(resource)),'actual loaded input header')
                    loaded=core.exact(process.stdout,len(resource));require(loaded==resource,'same actual loaded source bytes')
                    digest.update(loaded);result['loaded_inputs'].append(dict(name=name,sha256=sha(loaded),size=len(loaded)))
                for spec in selected:
                    parent=a.expected.resolve().parent;path=(parent/spec['payload']).resolve();require(path.parent==parent,'expected contained payload')
                    freeze(path,facts,spec['sha256'])
                    with gzip.open(path,'rb') as stream:
                        for point in spec['states']:
                            line,row=core.jsonline(process.stdout);sink.write(line);digest.update(line)
                            expected=core.exact(stream,byte_count);actual=core.exact(process.stdout,byte_count)
                            compare(row,spec['case'],point,actual,expected,byte_count,family['keys']);sink.write(actual);digest.update(actual)
                            if point['kind']=='upload':result['actual_C_frames']+=1
                        require(not stream.read(1),'expected exact tail')
                    line,row=core.jsonline(process.stdout);sink.write(line);digest.update(line);last=spec['states'][-1]
                    require(row==dict(case_end=spec['case'],frames=last['frame'],status=last['status'],failed=last['failed'],
                        skip=family.get('skip_flag',spec['request'][3]==1),other_owners_unchanged=True),'actual return and owner footer')
                    result['actual_C_cases']+=1
                line,row=core.jsonline(process.stdout);sink.write(line);digest.update(line)
                require(row==dict(end=len(selected),binding_unchanged=True) and not process.stdout.read(1),'actual selected end/source guards')
        except Exception as e:
            errors.append(str(e))
            if process.poll() is None:process.terminate()
    try:
        with (out/'stderr.txt').open('xb') as err:
            process=subprocess.Popen([str(a.producer.resolve()),family['cli'],str(a.resource_root.resolve())],cwd=root,
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            reader=threading.Thread(target=consume,daemon=True);reader.start()
            for spec in selected:
                lines=family['request_lines'](spec) if 'request_lines' in family else [' '.join(map(str,spec['request']))]
                for line in lines:
                    require(type(line) is str and 0<len(line)<8192 and '\n' not in line and '\0' not in line,'bounded fixed input line')
                    process.stdin.write((line+'\n').encode('ascii'))
            process.stdin.close();code=process.wait(timeout=240);reader.join(timeout=10)
            require(not reader.is_alive(),'reader still active');require(not errors,errors[0] if errors else '');require(code==0,f'probe exit{code}')
        result.update(passed=True,exit_code=code,actual_stream_sha256=digest.hexdigest(),
            compared_bytes=sum(len(c['states'])*byte_count for c in selected))
    except Exception as e:
        result['primary_error']=errors[0] if errors else str(e)
        if errors and str(e)!=errors[0]:result['secondary_errors'].append(dict(operation='probe.wait',error=str(e)))
    finally:
        if process is not None and process.stdin and not process.stdin.closed:
            try:process.stdin.close()
            except OSError as e:
                result.update(passed=False,primary_error=result['primary_error'] or 'stdin close failed')
                result['secondary_errors'].append(dict(operation='stdin.close',error=str(e)))
        core.cleanup_probe(process,reader,result);result.update(identity=guards(facts),new_original_CPU=0,GUI=0,scope=family['scope'],excluded=family['excluded'],
            source_PE_binding='parent immutable PE/build-receipt/tlog source snapshot join required')
        if not result['identity']['unchanged']:result.update(passed=False,primary_error=result['primary_error'] or 'late input guard')
        core.save_report(out/'report.json',result);print(json.dumps({k:result[k] for k in ['passed','actual_C_cases','actual_C_frames','primary_error']}))
    if not result['passed']:raise SystemExit(1)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    sub=p.add_subparsers(dest='mode',required=True);d=sub.add_parser('derive');q=sub.add_parser('pair')
    for name in [*core.PINS,*MORE_PINS]:d.add_argument('--'+name.replace('_','-'),required=True,type=Path)
    for name in ['expected','producer','build-receipt','resource-root']:q.add_argument('--'+name,required=True,type=Path)
    for name in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+name,required=True)
    for command in (d,q):command.add_argument('--output',required=True,type=Path)
    a=p.parse_args();facts=[]
    try:
        for path in [__file__,core.__file__,text.__file__,core.freeze.__code__.co_filename]:freeze(path,facts)
        derive(a,a.root.resolve(),facts) if a.mode=='derive' else pair(a,a.root.resolve(),facts)
    except (OSError,ValueError,KeyError,IndexError,StopIteration,OwnerFailure,zlib.error) as e:
        print(json.dumps(dict(passed=False,error=str(e),identity=guards(facts))));raise SystemExit(1)
if __name__=='__main__':main()
