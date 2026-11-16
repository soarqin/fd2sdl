"""Six FULL histories: independent source recurrence/tape plus original masks.

derive runs no C/original decoder. Renderer receives the complete explicit tape;
it never generates expected with C record_fx_segment. Game effects/jitter and
return App-before are declared input values, not natural producer claims.
"""
from __future__ import annotations
import argparse,gzip,json,struct
from pathlib import Path
import verify_f4_composite_history as c
from f4_full_magic_model import IntegerFX
import f4_full_magic_model as scalar

b=c.b;core=c.core;sha,require,freeze,guards=c.sha,c.require,c.freeze,c.guards
PAYLOAD=455552;PINS={'full_join':'0569a80ab72c1db29bc7969f674e2e874cee2c72ce1a4573866a1741e752e400'}
KEYS=[*b.KEYS,'shake_sign','background_flip','secondary_cues','stops','returns','owner_live']
SOURCES=[*c.SOURCES,'src/tests/f4_full_magic_history_probe.h','src/magic_fx.c','src/magic_fx.h','src/field_game.c',
    'data/static/tables/magic_fx.json']
SCOPE=('Six FULL normal given-input histories: actual original masks/source integer phases, FX tapes, '
 'layer order, transition/multitarget HP-next-frame/tint/idle and normal return; whole owners with explicit before.')
EXCLUDED=('No natural caster9/spell/effect/RNGmix producer, fresh App/work allocation proof, live11cac clock/audio, '
 'ID6 FP provider or FULL9 part31 onwards. Final work/board are named last-live snapshots after source retirement, '
 'not an original still-live malloc owner. No new original or C execution during derive.')

def requests():return [[0,2,0,2],[2,0,1,2],[4,2,0,2],[7,0,1,2],[8,2,0,3],[9,0,1,1]]
def validate(v):
    require(type(v) is list and all(type(x) is int for x in v) and v in requests(),'six fixed full parameter rows');return v
def lines(spec):
    yield ' '.join(map(str,spec['request']))
    for s in spec['segments']:yield ' '.join(map(str,[s['phase'],len(s['random']),len(s['jitter']),*s['random'],*s['jitter']]))
def rows_copy(dst,origin,board):
    require(len(board)==64000 and 0<=origin and origin+199*640+320<=172800,'FULL row-copy owner')
    for y in range(200):dst[origin+y*640:origin+y*640+320]=board[y*320:(y+1)*320]
def view(work,origin):
    require(0<=origin and origin+199*640+320<=172800,'FULL projection owner')
    return b''.join(work[origin+y*640:origin+y*640+320] for y in range(200))

class Full(b.Derived):
    def __init__(self,v,parts,images,bgs,terrain,oracle,tokens,base,cost,particles,tables):
        id,side,bottom,n=validate(v)
        super().__init__([24,side,bottom,0],parts,images,bgs,terrain,oracle,tokens,base,cost)
        self.id=id;self.count=n;self.caster=9;self.clip=parts['clips'][28];self.next_part=0;self.hp=[42]*n;self.current_hp=42
        self.board[:]=bytes(64000);b.summary(self.board,oracle,tokens,side,42,999)
        b.summary(self.board,oracle,tokens,0 if side else 2,42,999);self.image(self.board,0,320,bgs[3],0,50)
        self.app=bytearray(core.seed(c.P,17,456,83,bottom));self.secondary=self.stops=self.returns=0
        self.sign=-1;self.shake=3;self.tint=8;self.flip=0;self.live=True;self.current_index=0
        self.particles=particles;self.tables=tables;self.entry=(tables['friendly_entries'] if side else tables['hostile_entries'])[id]
        self.shake_x=tables['magic_shake_x'];self.shake_y=tables['magic_shake_y']
        self.fx=IntegerFX(id,side,31256,tables,[p[0]['duration'] for p in particles[self.entry]])
        self.tape=[];self.segments=[];self.current_jitter=[];self.origin=19360;self.phase=0
    def point(self,kind,status=0):
        return dict(kind=kind,phase=self.phase,part=-1,frame=self.frame,target=self.target,next_part=0,markers=0,
            idle_part=self.idle,idle_ticks=self.age,shake=self.shake,tint=self.tint,caster_mp=self.mp,active_hp=self.current_hp,
            cast_done=self.cast_done,started=self.started,failed=False,status=status,ticks=self.ticks,delays=self.delay,
            cues=self.cues,last_cue=self.last_cue,pumps=self.pumps,payload_bytes=PAYLOAD,shake_sign=self.sign,
            background_flip=self.flip,secondary_cues=self.secondary,stops=self.stops,returns=self.returns,owner_live=self.live),\
            bytes(self.work)+bytes(self.vga)+bytes(self.board)+bytes(self.app)+self.base+self.dac
    def fig(self,dst,origin,stride,entry,part,x=0,y=0,tint=-1):
        key=f'figani:{entry}:{part}';meta=self.parts['parts'][key];pixels,mask=self.images[key]
        if tint>255:pixels=bytes((tint+(((tint>>8)+value)&7))&255 for value in pixels);tint=-1
        b.draw(dst,origin,stride,meta,(pixels,mask),x,y,tint,172800 if dst is self.work else len(dst))
    def idle_draw(self,unit,origin,x=0,y=0,tint=-1,stride=640):
        self.fig(self.work,origin,stride,unit*3,self.idle,x,y,tint);self.age=(self.age+1)&255
        clip=self.parts['clips'][unit*3]
        if self.age>=clip['parts'][self.idle]['duration']:self.age=0;self.idle=(self.idle+1)%clip['count']
    def fx_step(self,phase):
        before=len(self.fx.trace);out=self.fx.step(phase)
        for kind,_,value,delta in self.fx.trace[before:]:
            if kind==0:
                meta,pm=self.particles[self.entry][value];b.draw(self.work,self.origin+delta,640,meta,pm,owner=172800)
            elif kind in (1,2):self.cue(value);self.secondary+=kind==2
            elif kind==3:self.tape.append(value)
        return out
    def recolor(self,lut):
        for image,x,y in [(self.bgs[3],0,50),(self.terrain,164,157)]:
            meta,(pixels,mask)=image;b.draw(self.board,0,320,meta,(bytes(lut[p] for p in pixels),mask),x,y)
    def intro_full(self):
        if not self.side:self.image(self.board,0,320,self.terrain,164,157)
        for i in range(8,-1,-1):
            origin=0 if self.side else 320;rows_copy(self.work,origin,self.board)
            if self.side:self.fig(self.work,0,640,0,0);self.image(self.work,0,640,self.terrain,164+10*i,157)
            self.fig(self.work,0,640,27,0,origin+(10*i if self.side else -10*i))
            if not self.side:self.fig(self.work,0,640,0,0,320)
            self.vga[:]=view(self.work,origin);self.dac=bytes(((max(0,x-6*i)<<2)|(max(0,x-6*i)>>4)) for x in self.base)
            yield from self.upload()
        if self.side:self.image(self.board,0,320,self.terrain,164,157)
    def cast_full(self):
        self.cast_done=self.started=True;b.summary(self.board,self.oracle,self.tokens,self.side,42,999)
        yield from self.fade();self.dac=bytes((x<<2)|(x>>4) for x in self.base);yield from self.intro_full()
        flash=idle=age=0
        for part in range(self.clip['intro']):
            p=self.clip['parts'][part]
            if p['marker']==1:
                self.mp-=self.cost;b.summary(self.board,self.oracle,self.tokens,self.side,42,self.mp)
                self.recolor(self.luts[self.return_base]);flash=6;self.cue(0)
            for _ in range(p['duration']):
                self.work[:64000]=self.board
                if self.side:self.fig(self.work,0,320,28,part)
                self.fig(self.work,0,320,0,idle)
                if not self.side:self.fig(self.work,0,320,28,part)
                self.vga[:]=self.work[:64000]
                if flash:
                    self.dac=bytes((x<<2)|(x>>4) for x in self.cast_colors)+self.dac[3:]
                    yield from self.upload();self.delay+=30;self.dac=bytes(3)+self.dac[3:];flash-=1
                yield from self.upload();self.ticks+=1;age=(age+1)&255
                if age==self.parts['clips'][0]['parts'][idle]['duration']:age=0;idle=(idle+1)%self.parts['clips'][0]['count']
        if self.id==9:
            for i in range(11):
                rows_copy(self.work,320,self.board)
                if i!=10:self.fig(self.work,320,640,27,0,-10*i)
                self.fig(self.work,320,640,0,0);self.vga[:]=view(self.work,320);yield from self.upload()
            self.delay+=500
        self.origin=19360;duration=self.fx_step(0);self.idle=self.age=0
        for _ in range(duration):
            rows_copy(self.work,self.origin,self.board);self.fx_step(1)
            if self.id!=9:self.fig(self.work,self.origin,640,28,self.clip['count']-1)
            self.idle_draw(0,self.origin);self.fx_step(2);self.vga[:]=view(self.work,self.origin)
            yield from self.upload();self.ticks+=1
        self.segments.append(dict(phase=0,random=self.tape.copy(),jitter=[]));yield self.point('cast_end')
    def run(self):
        yield from self.cast_full()
        for index,unit in enumerate([0,96,30][:self.count]):
            self.phase=1;self.target=index+1;self.current_index=index;self.current_hp=42;self.tape=[];self.current_jitter=[]
            self.origin=19360;duration=self.fx_step(3);self.idle=self.age=0;hp_step=1
            for _ in range(duration):
                if self.id in (3,7,9):self.flip^=1
                rows_copy(self.work,self.origin-640*self.flip,self.board);self.fx_step(4)
                if self.id!=9:self.fig(self.work,self.origin,640,28,self.clip['count']-1)
                self.idle_draw(unit,self.origin,self.sign*self.shake_x[self.shake],self.shake_y[self.shake],self.hit_base+(self.tint<<8))
                self.tint-=1
                if self.tint==1:self.tint=8
                if self.shake!=3:self.shake+=1
                if self.fx_step(5)==1:
                    if hp_step<=self.denominator:
                        self.current_hp=42-hp_step*14//self.denominator;hp_step+=1
                        b.summary(self.board,self.oracle,self.tokens,0 if self.side else 2,self.current_hp,999)
                    jitter=1-len(self.current_jitter)%3;self.current_jitter.append(jitter);self.sign=jitter;self.shake=0
                self.vga[:]=view(self.work,self.origin);yield from self.upload();self.ticks+=1
            if index+1<self.count:
                direction=-1 if self.side else 1;flip=0;self.origin=18880
                for part_pass in range(2):
                    for j in (range(1,5) if not part_pass else range(4,-1,-1)):
                        if self.id in (3,7):flip^=1
                        rows_copy(self.work,self.origin-640*flip,self.board);self.fx_step(4)
                        self.fig(self.work,self.origin,640,28,self.clip['count']-1)
                        target_unit=[0,96,30][index+part_pass];self.fig(self.work,self.origin,640,target_unit*3,0,direction*35*j)
                        self.fx_step(5);self.vga[:]=view(self.work,self.origin);yield from self.upload();self.ticks+=1
                b.summary(self.board,self.oracle,self.tokens,0 if self.side else 2,42,999)
            self.segments.append(dict(phase=1,random=self.tape.copy(),jitter=self.current_jitter.copy()));yield self.point('target_end')
        self.phase=2;self.origin=19360;self.tape=[];duration=self.fx_step(6)
        for _ in range(duration):
            rows_copy(self.work,self.origin,self.board);self.fx_step(7)
            if self.id!=9:self.fig(self.work,self.origin,640,28,self.clip['count']-1)
            self.idle_draw([0,96,30][self.count-1],self.origin);self.fx_step(8)
            self.vga[:]=view(self.work,self.origin);yield from self.upload();self.ticks+=1
        if self.id==9:
            for j in range(7,-1,-1):
                rows_copy(self.work,320,self.board);self.fig(self.work,320,640,27,0,-10*j)
                self.fig(self.work,320,640,0,0);self.vga[:]=view(self.work,320);yield from self.upload()
        for lut_index in range(self.return_base+1,self.return_base+4):
            self.work[:64000]=self.board;self.recolor(self.luts[lut_index]);self.fig(self.work,0,320,28,0)
            self.idle_draw([0,96,30][self.count-1],0,stride=320);self.vga[:]=self.work[:64000];yield from self.upload();self.ticks+=1
        self.work[:64000]=self.board;self.image(self.board,0,320,self.bgs[3],0,50);self.image(self.board,0,320,self.terrain,164,157)
        self.fig(self.work,0,320,28,0);self.idle_draw([0,96,30][self.count-1],0,stride=320)
        self.vga[:]=self.work[:64000];yield from self.upload()
        self.stops+=1;self.ticks+=8;yield from self.fade()
        self.vga[:]=bytes(64000);self.returns+=1
        for y in range(192):
            self.app[(y+72)*456+72:(y+72)*456+384]=bytes(40+(x*7+y*11+9)%173 for x in range(312))
            self.vga[(y+4)*320+4:(y+4)*320+316]=self.app[(y+72)*456+72:(y+72)*456+384]
        for i in range(65):
            self.dac=bytes(((max(0,x-(64-i))<<2)|(max(0,x-(64-i))>>4)) for x in self.base)
            yield from self.upload();self.delay+=2
        self.segments.append(dict(phase=2,random=self.tape.copy(),jitter=[]));self.live=False;yield self.point('finish_end')

def read_particles(a,other,join,facts):
    report=json.loads(freeze(a.remainder,facts,c.PINS['remainder']));require(report['ok'] and report['guards_unchanged'],'qualified original FX leaves')
    selected=set((join['tables']['friendly_entries']['values'] if side else join['tables']['hostile_entries']['values'])[id] for id,side,_,_ in requests())
    particles={};bindings=[]
    for entry in sorted(selected):
        block=b.archive_entry(other,entry);meta=join['clips'][str(entry)];require(sha(block)==meta['source_sha256'],'same full FX container')
        parts=[]
        for part,p in enumerate(meta['parts']):
            start=struct.unpack_from('<I',block,8+4*part)[0];end=struct.unpack_from('<I',block,12+4*part)[0] if part+1<meta['count'] else len(block)
            require(sha(block[start+9:end])==p['source_sha256'],'same source part bytes')
            row=next(r for r in report['cases'] if r['request']==['battle','otherfx',entry,part,0]);item=row['original_payload'];path=(a.remainder.resolve().parent/item['path']).resolve()
            require(path.parent==a.remainder.resolve().parent and not row['copy_mask_normalized'] and row['hardware_IO_executed']==0 and
                row['source_bytes_unchanged'] and row['write_extent_passed'] and row['source_sha256']==p['source_sha256'] and
                row['stride']==p['w'] and row['height']==p['h'] and row['entry']==0x4e98d,'original parameter/owner/source binding')
            n=p['w']*p['h'];raw=core.unpack(freeze(path,facts,item['sha256']),2*n+1536);mask=raw[n+768:2*n+768]
            require(set(mask)<={0,1} and sha(mask)==row['original_write_mask_sha256'] and not any(raw[-768:]),'original actual FX mask/noDAC')
            parts.append((dict(x=p['x'],y=p['y'],width=p['w'],height=p['h'],duration=p['duration']),(raw[:n],mask)))
            bindings.append(dict(entry=entry,part=part,source_sha256=p['source_sha256'],original_payload=item))
        particles[entry]=parts
    return particles,bindings

def derive(a,root,facts):
    raw,more,parts,images,bgs,terrain,oracle,tokens,base,_,_,_,_,_,contracts,tg,_=c.inputs(a,facts)
    source=json.loads(raw['source'])
    for va in (0x2ff01,0x31266):contracts.append(core.source_check(raw['image'],source,va))
    join=json.loads(freeze(a.full_join,facts,PINS['full_join']));require(join['guards_unchanged'],'complete source family join')
    for row in join['tables'].values():require(sha(raw['image'][row['va']:row['va']+struct.calcsize('<'+row['format'])*row['count']])==row['sha256'],'original table bytes')
    particles,bindings=read_particles(a,more['other'],join,facts)
    results={r['asset']:r for r in json.loads(raw['original_report'])['results'] if r['family']=='figani' and r['variant']=='normal'}
    for key,meta in parts['parts'].items():
        if meta['entry']==90:core.pixels(key,a,results,parts['parts'],facts,images)
    tables={k:v['values'] for k,v in join['tables'].items()}
    # 2ff6a/2ff7a copy distinct FULL X/Y tables, not physical5266c/52684.
    fn=next(f for f in source['functions'] if f['va']==0x2ff01)
    for va,name,at in ((0x2ff6a,'magic_shake_x',0x5269c),(0x2ff7a,'magic_shake_y',0x526ac)):
        instruction=next(i for i in fn['instructions'] if i['va']==va)
        require(instruction['asm'].lower()==f'mov     esi, offset unk_{at:X}'.lower(),'FULL table producer source')
        blob=raw['image'][at:at+16];tables[name]=list(struct.unpack('<4i',blob))
        contracts.append(dict(va=at,length=16,sha256=sha(blob),values=tables[name],caller=va))
    lut=b.archive_entry(more['other'],3);luts={}
    for i in range(11,23):
        start=struct.unpack_from('<I',lut,6+4*i)[0];end=struct.unpack_from('<I',lut,10+4*i)[0] if i<22 else len(lut)
        luts[i]=lut[start:end];require(len(luts[i])==256,'exact original full LUT')
    out=core.fresh(root,a.output);cases=[]
    for number,v in enumerate(requests()):
        id=v[0];instance=Full(v,parts,images,bgs,terrain,oracle,tokens,base,raw['image'][0x619fd+7*id+5],particles,tables)
        instance.luts=luts;instance.return_base=19 if id==8 else 15 if id>3 else 11
        instance.hit_base=32 if id<4 else 176;instance.denominator=tables['hp_denominators'][id]
        instance.cast_colors=tuple(raw['image'][at+id] for at in (0x51aad,0x51ad1,0x51af5))
        states=[];filename=f'case-{number:02d}.gz'
        with gzip.open(out/filename,'xb') as stream:
            for point,data in instance.run():
                require(len(data)==PAYLOAD,'whole full packet');stream.write(data);states.append(dict(**point,sha256=sha(data)))
        cases.append(dict(case=number,request=v,payload=filename,sha256=sha((out/filename).read_bytes()),states=states,segments=instance.segments))
        print(json.dumps(dict(done=number+1,total=6,frames=states[-1]['frame'])),flush=True)
    identity=guards(facts+list(tg.values()));require(identity['unchanged'],'late source/script/resource/original guards')
    result=dict(schema='fd2-f4-battle-history-expected-v1',family='fullmagic',passed=True,cases=cases,
        generated_frames=sum(sum(p['kind']=='upload' for p in row['states']) for row in cases),
        inputs={**core.PINS,**b.MORE_PINS,**c.PINS,**PINS},source_contract=contracts,pixel_bindings=bindings,
        scope=SCOPE,excluded=EXCLUDED,identity=identity,new_original_CPU=0,actual_C=0,GUI=0,primary_error=None,secondary_errors=[])
    core.save_report(out/'expected.json',result);require(result['passed'],'expected report write');print(json.dumps(dict(passed=True,path=str(out/'expected.json'),frames=result['generated_frames'])))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);sub=p.add_subparsers(dest='mode',required=True)
    d=sub.add_parser('derive');q=sub.add_parser('pair')
    for name in [*core.PINS,*b.MORE_PINS,*c.PINS,*PINS]:d.add_argument('--'+name.replace('_','-'),required=True,type=Path)
    for name in ['expected','producer','build-receipt','resource-root']:q.add_argument('--'+name,required=True,type=Path)
    for name in ['expected-sha256','producer-sha256','build-receipt-sha256']:q.add_argument('--'+name,required=True)
    for cmd in (d,q):cmd.add_argument('--output',required=True,type=Path)
    a=p.parse_args();facts=[]
    try:
        for path in [__file__,scalar.__file__,c.__file__,b.__file__,core.__file__,b.text.__file__,core.freeze.__code__.co_filename]:freeze(path,facts)
        if a.mode=='derive':derive(a,a.root.resolve(),facts)
        else:b.pair(a,a.root.resolve(),facts,dict(inputs={**core.PINS,**b.MORE_PINS,**c.PINS,**PINS},requests=requests(),sources=SOURCES,
            payload=PAYLOAD,keys=KEYS,cli='--f4-full-magic-history',scope=SCOPE,excluded=EXCLUDED,skip_flag=False,request_lines=lines))
    except (OSError,ValueError,KeyError,IndexError,StopIteration,b.OwnerFailure,b.zlib.error) as e:
        print(json.dumps(dict(passed=False,error=str(e),identity=guards(facts))));raise SystemExit(1)
if __name__=='__main__':main()
