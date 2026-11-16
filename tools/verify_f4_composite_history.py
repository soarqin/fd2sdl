"""COMPOSITE CAST complete history from source formulas and retained original masks.

No original/C decode or execution in derive. The return App-before and phase3
page writer are declared inputs, not fresh-heap or live-clock qualifications.
"""
from __future__ import annotations
import argparse,gzip,json,struct
from pathlib import Path
import verify_f4_battle_history as b

core=b.core;sha,require,freeze,guards=b.sha,b.require,b.freeze,b.guards
P=153216;PAYLOAD=b.PAYLOAD+P
PINS={'remainder':'6c95a250ec75333a35930c026e37160c4cd6c21632d4a7673aace2cf17fddb30',
 'palette_source':'9b6588c0cb25af1fe1a74272cd7fe639d8e85c2dd2d4bd7eebf043ce66bc9e11',
 'canonical':'ad0aa3aa2ba6cc0e04dbd040215ae452e476d07815be7a33d8c0a74a32c17095'}
KEYS=[*b.KEYS,'secondary_cues','stops','returns','cast_bank_open','map_bank_open','host']
SOURCES=[*b.SOURCES,'src/tests/f4_composite_history_probe.h','src/field_visual.c','src/field_visual.h']
SCOPE=('COMPOSITE32..35 complete CAST: source slot1 intro/MP, original37 particles,8+9slides, '
 '11alternating/41fade RGB/DAC, bank stop/return order; all work/VGA/board and declared reconstructed App-before.')
EXCLUDED=('No natural caster9/spell producer, fresh malloc/App-before proof, actual11cac live pixels/clock/audio, '
 'new original CPU or Game/core RNG/MP commit. Subsequent MAP groups/popups use MAP98 plus original cold group contract, '
 'not this CAST history. C extra44800 work bytes remain guard, not original128000 allocation.')

def requests():
    return [[id,side,bottom,0] for id in range(32,36) for side in (0,2) for bottom in (0,1)]+[
        [34,2,1,1],[34,2,1,2],[34,2,1,3]]

def validate(v):
    require(type(v) is list and len(v)==4 and all(type(x) is int for x in v),'four exact integer fields')
    id,side,bottom,control=v
    require(32<=id<=35 and side in (0,2) and bottom in (0,1) and 0<=control<=3 and
        (not control or id==34),'fixed composite/control domain');return v

def chop(n,d):return n//d if n>=0 else -((-n)//d)

class Composite(b.Derived):
    def __init__(self,v,parts,images,bgs,terrain,oracle,tokens,base,cost,particles,luts,colors,cast_colors):
        id,side,bottom,control=validate(v)
        super().__init__([24,side,bottom,0],parts,images,bgs,terrain,oracle,tokens,base,cost)
        self.id=id;self.control=control;self.skip=control==1;self.caster=9;self.clip=parts['clips'][28];self.next_part=0
        self.particles=particles;self.luts=luts;self.colors=colors;self.cast_colors=cast_colors
        self.app=bytearray(core.seed(P,17,456,83,bottom))
        self.secondary=self.stops=self.returns=0;self.cast_bank=True;self.host=False
    def point(self,kind,status=0):
        state,_=super().point(kind,status)
        state.update(phase=0,part=-1,payload_bytes=PAYLOAD,secondary_cues=self.secondary,stops=self.stops,
            returns=self.returns,cast_bank_open=self.cast_bank,map_bank_open=True,host=self.host)
        return state,bytes(self.work)+bytes(self.vga)+bytes(self.board)+bytes(self.app)+self.base+self.dac
    def particle(self,dst,origin,stride,index,x=0):
        meta,picture=self.particles[self.id+33][index];b.draw(dst,origin,stride,meta,picture,x,owner=128000 if dst is self.work else len(dst))
    def recolor(self):
        lut=self.luts[19 if self.id<=33 else 15]
        # 4e8d3 has 4e98d's same SKIP/stride2 stores, mapping source colour only.
        # It is not base24's C0 -> LUT[old destination] operation.
        for image,x,y in [(self.bgs[3],0,50),(self.terrain,164,157)]:
            meta,(pixels,mask)=image;b.draw(self.board,0,320,meta,(bytes(lut[c] for c in pixels),mask),x,y)
    def palette(self,factor):
        if self.skip or self.host:return
        rgb=self.colors[self.id-32];dac=bytearray(self.dac)
        for i in range(765):
            level=rgb[i%3]+chop(factor*(self.base[i]-rgb[i%3]),40);dac[i]=(level<<2)|(level>>4)
        self.dac=bytes(dac)
    def run(self):
        self.cast_done=True;b.summary(self.board,self.oracle,self.tokens,self.side,1,999)
        if not self.skip:
            self.started=True;yield from self.fade();self.dac=bytes((x<<2)|(x>>4) for x in self.base)
            yield from self.intro()
        flashes=0
        for index in range(self.clip['intro']):
            p=self.clip['parts'][index]
            if p['marker']==1:
                self.mp-=self.cost;b.summary(self.board,self.oracle,self.tokens,self.side,1,self.mp)
                self.recolor();self.cue(0);flashes=6
            for _ in range(p['duration']):
                self.work[:64000]=self.board;self.fig(self.work,0,320,28,index)
                if not self.skip:self.vga[:]=self.work[:64000]
                if flashes:
                    if not self.skip:
                        self.dac=bytes((x<<2)|(x>>4) for x in self.cast_colors[self.id])+self.dac[3:]
                        yield from self.upload();self.delay+=30;self.dac=bytes(3)+self.dac[3:]
                    flashes-=1
                yield from self.upload()
                if not self.skip:self.ticks+=1
        if not self.skip:
            self.ticks+=6
            for index in range(8):
                b.copy_rows(self.work,0,640,self.board);self.fig(self.work,0,640,27,0,20*index)
                self.vga[:]=b.view(self.work);yield from self.upload();self.ticks+=1
            for index in range(8,-1,-1):
                b.copy_rows(self.work,0,640,self.board);self.particle(self.work,0,640,0,30*index)
                self.vga[:]=b.view(self.work);yield from self.upload();self.ticks+=1
            if self.id in (33,34):self.particle(self.board,0,320,0)
            for index in range(1,len(self.particles[self.id+33])):
                self.work[:64000]=self.board;self.particle(self.work,0,320,index);self.vga[:]=self.work[:64000]
                yield from self.upload()
                if (self.id==34 and index==2) or (self.id==33 and index==6):self.cue(1)
                elif self.id in (32,35) and index==1:self.cue(2);self.secondary+=1
                self.ticks+=2
            if self.id in (32,35):
                for index in range(11):
                    if not index%2:self.cue(1)
                    self.work[:64000]=self.board;self.particle(self.work,0,320,len(self.particles[self.id+33])-2+(index&1))
                    self.vga[:]=self.work[:64000];yield from self.upload();self.ticks+=2
                    self.palette(40-4*index);yield from self.upload()
            self.palette(0);self.vga[:]=bytes(64000);self.returns+=1
            if self.control==2:
                self.app[72*456+72]=239;self.failed=True;yield self.point('cast_end',-1);return
            for y in range(192):
                self.app[(y+72)*456+72:(y+72)*456+384]=bytes(40+(x*7+y*11+9)%173 for x in range(312))
                self.vga[(y+4)*320+4:(y+4)*320+316]=self.app[(y+72)*456+72:(y+72)*456+384]
            if self.control==3:self.host=True
            else:
                for factor in range(41):self.palette(factor);yield from self.upload();self.delay+=6
                self.stops+=1;self.cast_bank=False
        yield self.point('cast_end')

def particle_inputs(a,resource,facts):
    report=json.loads(freeze(a.remainder,facts,PINS['remainder']))
    require(report['ok'] and report['guards_unchanged'] and report['original_cases']==2093 and report['actual_C_cases']==0,
        'retained independent original leaf report')
    pictures={};bindings=[]
    for entry in range(65,69):
        data=b.archive_entry(resource,entry);count=data[0];require(count>=2 and len(data)>=8+4*count,'finite particle directory')
        selected=[]
        for part in range(count):
            start=struct.unpack_from('<I',data,8+4*part)[0]
            end=struct.unpack_from('<I',data,12+4*part)[0] if part+1<count else len(data)
            require(8+4*count<=start<end<=len(data),'strict particle span')
            raw=data[start:end];x,y=struct.unpack_from('<HH',raw);w,h=struct.unpack_from('<HH',raw,9)
            require(x+w<=320 and y+h<=200,'whole source particle geometry')
            row=next(r for r in report['cases'] if r['request']==['battle','otherfx',entry,part,0])
            require(row['source_sha256']==sha(raw[9:]) and row['stride']==w and row['height']==h and
                not row['copy_mask_normalized'] and row['source_bytes_unchanged'] and row['write_extent_passed'] and
                row['hardware_IO_executed']==0,'same raw original part/source/parameter')
            item=row['original_payload'];p=(a.remainder.resolve().parent/item['path']).resolve()
            require(p.parent==a.remainder.resolve().parent,'retained particle containment')
            n=w*h;pixels=core.unpack(freeze(p,facts,item['sha256']),2*n+1536)
            mask=pixels[n+768:2*n+768];require(set(mask)<={0,1} and sha(mask)==row['original_write_mask_sha256'] and
                sum(mask)==row['original_pixel_addresses_written'] and not any(pixels[-768:]),'actual original hook mask/no DAC stores')
            selected.append((dict(x=x,y=y,width=w,height=h),(pixels[:n],mask)))
            bindings.append(dict(entry=entry,part=part,source_sha256=sha(raw[9:]),payload=item,mask_sha256=sha(mask)))
        pictures[entry]=selected
    require(len(bindings)==37,'37 unique actual particles');return pictures,bindings

def inputs(a,facts):
    raw,more,parts,images,bgs,terrain,oracle,tokens,base,_,contracts,text_guards=b.read_inputs(a,facts)
    palette=json.loads(freeze(a.palette_source,facts,PINS['palette_source']));contracts.append(core.source_check(raw['image'],palette,0x2df01))
    source=json.loads(raw['source']);contracts.append(core.source_check(raw['image'],source,0x2d80d))
    canonical=freeze(a.canonical,facts,PINS['canonical']).decode('utf-8')
    start=canonical.index('char __cdecl sub_4E8D3(');end=canonical.index('/* ---------------------------------------------------------------',start)
    require('v7 += v9;' in canonical[start:end] and '*(_BYTE *)(v10 + a6)' in canonical[start:end],
        'complete published source LUT source-colour/skip branches')
    contracts.append(dict(va=0x4e8d3,end=0x4e98d,bytes_sha256=sha(raw['image'][0x4e8d3:0x4e98d]),
        source='complete pinned canonical pseudoC plus original mask branch-equivalence; no new instruction extraction'))
    results={r['asset']:r for r in json.loads(raw['original_report'])['results'] if r['family']=='figani' and r['variant']=='normal'}
    for key,meta in parts['parts'].items():
        if meta['entry'] in (27,28):core.pixels(key,a,results,parts['parts'],facts,images)
    particles,bindings=particle_inputs(a,more['other'],facts)
    lut=b.archive_entry(more['other'],3);require(lut[:4]==b'LMI1' and struct.unpack_from('<H',lut,4)[0]==23,'original23 LUT directory')
    luts={}
    for index in (15,19):
        at=struct.unpack_from('<I',lut,6+4*index)[0];last=struct.unpack_from('<I',lut,10+4*index)[0]
        luts[index]=lut[at:last];require(len(luts[index])==256,'raw original full256 LUT')
    image=raw['image'];colors=[tuple(image[at+i] for at in (0x5265c,0x52660,0x52664)) for i in range(4)]
    cast_colors={id:tuple(image[at+id] for at in (0x51aad,0x51ad1,0x51af5)) for id in range(32,36)}
    costs={id:image[0x619fd+7*id+5] for id in range(32,36)}
    return raw,more,parts,images,bgs,terrain,oracle,tokens,base,costs,particles,luts,colors,cast_colors,contracts,text_guards,bindings

def derive(a,root,facts):
    raw,more,parts,images,bgs,terrain,oracle,tokens,base,costs,particles,luts,colors,cast_colors,contracts,tg,bindings=inputs(a,facts)
    out=core.fresh(root,a.output);cases=[]
    for number,v in enumerate(requests()):
        filename=f'case-{number:02d}.gz';states=[]
        with gzip.open(out/filename,'xb') as stream:
            for point,data in Composite(v,parts,images,bgs,terrain,oracle,tokens,base,costs[v[0]],particles,luts,colors,cast_colors).run():
                require(len(data)==PAYLOAD,'complete composite owner packet');stream.write(data);states.append(dict(**point,sha256=sha(data)))
        cases.append(dict(case=number,request=v,payload=filename,sha256=sha((out/filename).read_bytes()),states=states))
        if number%4==0:print(json.dumps(dict(done=number+1,total=len(requests()))),flush=True)
    identity=guards(facts+list(tg.values()));require(identity['unchanged'],'late original/script/payload identity')
    result=dict(schema='fd2-f4-battle-history-expected-v1',family='composite',passed=True,cases=cases,
        generated_frames=sum(sum(p['kind']=='upload' for p in c['states']) for c in cases),
        inputs={**core.PINS,**b.MORE_PINS,**PINS},source_contract=contracts,particle_bindings=bindings,
        declared_return='new App-before supplied; runtime phase3 writes312x192 only thenprojects; error adapter writesone239pixel thenreturns-1, HOST adapter completesphase3 thenrequestsquit; neither is an original bad-owner fixture; no live clock or fresh allocation claim',
        scope=SCOPE,excluded=EXCLUDED,identity=identity,new_original_CPU=0,actual_C=0,GUI=0,primary_error=None,secondary_errors=[])
    core.save_report(out/'expected.json',result);require(result['passed'],'expected report write')
    print(json.dumps(dict(passed=True,path=str(out/'expected.json'),cases=len(cases),frames=result['generated_frames'])))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path)
    sub=p.add_subparsers(dest='mode',required=True);d=sub.add_parser('derive');q=sub.add_parser('pair')
    for name in [*core.PINS,*b.MORE_PINS,*PINS]:d.add_argument('--'+name.replace('_','-'),required=True,type=Path)
    for name in ['expected','producer','build-receipt','resource-root']:q.add_argument('--'+name,required=True,type=Path)
    for name in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+name,required=True)
    q.add_argument('--controls-only',action='store_true',help='verify full19 identity, thenrunonlyoriginal16..18 asstream0..2')
    for command in (d,q):command.add_argument('--output',required=True,type=Path)
    a=p.parse_args();facts=[]
    try:
        for path in [__file__,b.__file__,core.__file__,b.text.__file__,core.freeze.__code__.co_filename]:freeze(path,facts)
        if a.mode=='derive':derive(a,a.root.resolve(),facts)
        else:b.pair(a,a.root.resolve(),facts,dict(inputs={**core.PINS,**b.MORE_PINS,**PINS},requests=requests(),sources=SOURCES,
            payload=PAYLOAD,keys=KEYS,cli='--f4-composite-history',selection=[16,17,18] if a.controls_only else None,
            scope=('Only original indices16..18 host controls, not16normal or complete19. '+SCOPE) if a.controls_only else SCOPE,excluded=EXCLUDED))
    except (OSError,ValueError,KeyError,IndexError,StopIteration,b.OwnerFailure,b.zlib.error) as e:
        print(json.dumps(dict(passed=False,error=str(e),identity=guards(facts))));raise SystemExit(1)
if __name__=='__main__':main()
