"""Thirty UI0 dynamic fragments through real Prepare/Play consumers.

No original execution, build or GUI. Expected uses retained glyph/portrait pixels,
original tokens and source caller coordinates, never current C output.
"""
from __future__ import annotations
import argparse,hashlib,json,sys,struct,zlib,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if not(ROOT/'tools/audit_g4_render_source.py').is_file():ROOT=Path.cwd()
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tools/static_data')]
import verify_f4_text_composition as text
import verify_f4_defeat_pages as source
from run_graphics_tests import decode_save
core=text.core;audit=text.audit
REMAINDER_SHA='6c95a250ec75333a35930c026e37160c4cd6c21632d4a7673aace2cf17fddb30'
SLOT_SHA='df7c9c5e119df9d26950e8d4f0ddb1c4450f52776130915cb643ad6c35cc44af'
PIXEL_SHA='ca9c1187e0777087214f25d1c5e9e7db9b2a766ffb4597d97559257e141a522b'
PLAY=(422,424,425,426,429,430,432,434,435,487,488,490,491,492,493,494,587)
PREPARE=(439,490,491,492,493,494,502,506,508,509,511,590,594,595,596,657,659)
LIMITS={426:65535,430:65535,435:65535,439:60000,487:6553,488:0x7fffffff,
 **{i:30 for i in range(490,495)},502:60000,508:45000,590:765000,596:2,659:45000}
DYNAMIC=set(PLAY)|set(PREPARE)|{549}
MINIMUM={'src/tests/field_prepare_play_test.c','src/tests/field_progress_play_test.c',
 'src/tests/f4_dynamic_text_hooks.h','src/tests/f4_dynamic_text_probe.h',
 'src/tests/f4_dynamic_prepare_probe.h','src/tests/f4_dynamic_progress_probe.h',
 'src/field_prepare_play.c','src/field_prepare_service_present.h','src/field_play.c',
 'src/font.c','src/text.c','src/field_visual.c','src/portrait.c','src/save.c','src/field_rng.c'}


def sha(b):return hashlib.sha256(b).hexdigest()
def require(x,why):
 if not x:raise ValueError(why)
def snapshot(p,guards,expected=None):
 current={};raw=text.snapshot(p,current,expected);name=str(Path(p).resolve())
 require(name not in guards or guards[name]==current[name],'input changed since first qualification: '+name)
 guards.update(current);return raw
def scalar_bounds(maximum,nonzero=False):
 values={maximum,1 if nonzero else 0}
 for p in range(1,11):
  for v in(10**p-1,10**p):
   if (v>0 or not nonzero)and v<=maximum:values.add(v)
 return sorted(values)
def names_by_length(ui,lo,hi):
 result={}
 for n in range(lo,hi+1):
  tokens=ui[n];require(tokens[-1]==-1 and all(t>=0 for t in tokens[:-1]),'nested names must remain source-qualified plain')
  result.setdefault(len(tokens)-1,n)
 return list(result.values())
def requests(ui):
 work=[];seen=set()
 def add(role,f,v,new,old,x,y,portrait,scope):
  key=(role,f,v,new,old,x,y,portrait)
  if key in seen:return
  seen.add(key)
  for track in(0,1):
   work.append({'role':role,'fragment':f,'value':v,'new':new,'old':old,'x':x,'y':y,'portrait':portrait,'track':track,'seed':0,'scope':scope})
 item=names_by_length(ui,181,395);member=names_by_length(ui,1,136);job=names_by_length(ui,150,178);magic=names_by_length(ui,441,476)
 for role,fragments in [('progress',PLAY),('prepare',PREPARE)]:
  for f in fragments:
   controls=set(t for t in ui[f]if t<0)
   group=(magic if f==587 else job if f==595 else member if f in(506,509,511,590,594,657)else item)
   new=max(group,key=lambda n:len(ui[n]));old=min(item,key=lambda n:len(ui[n]))
   if role=='progress':x=99 if f in(422,424,425,426,429,430,432,434,435,487)else 95;y=146 if f in(429,430)else 127 if x==99 else 157 if f in(490,491,492,493,494,587)else 119;portrait=0
   else:x=95 if f in(490,491,492,493,494,595,596,657)else 12;y=157 if f in(490,491,492,493,494)else 176 if f==596 else 119;portrait=0 if x==95 else 128
   maximum=LIMITS.get(f,0)
   values=scalar_bounds(maximum,f in range(490,495))if -6 in controls else[0]
   for v in values:add(role,f,v,new,old,x,y,portrait,'formal finite value; gain wide DWORD is callee defined, natural producer not asserted'if f==488 else'formal caller bounded family')
   if -4 in controls:
    for n in group:add(role,f,maximum,n,old,x,y,portrait,'every plain-name length class; individual glyph/name leaves inherited')
   if -5 in controls:
    for n in item:
     for o in item:add(role,f,0,n,o,x,y,portrait,'length-class pair product only, not215 squared names')
 for v in(0xffffffff,0x80000000):add('progress',488,v,181,181,95,119,0,'negative first-font-reader U prefix plus finite C H continuation; no original invalid read')
 for selected in range(4):
  work.append({'role':'prepare','fragment':549,'value':selected,'new':181,'old':181,'x':10,'y':119,'portrait':0,'track':0,'seed':0,'scope':'real four-slot draw, all four selected foreground positions; slow0'})
 require({r['fragment']for r in work}==DYNAMIC and len(DYNAMIC)==30,'exact thirty-fragment denominator')
 for i,r in enumerate(work):
  r['id']=i;r['request']=[r[k]for k in('id','fragment','value','new','old','x','y','portrait','track','seed')]
  r['line']='dynamic '+' '.join(map(str,r['request']))+'\n'
 return work

def signed_digits(value):
 signed=value if value<=0x7fffffff else value-0x100000000
 return [0xffef if c=='-'else ord(c)-48 for c in str(signed)]
def expand(ui,f,value,new,old):
 out=[]
 for t in ui[f]:
  if t==-1:break
  if t in(-4,-5):
   name=ui[new if t==-4 else old];require(name[-1]==-1 and all(n>=0 for n in name[:-1]),'nested only known plain names');out.extend(name[:-1])
  elif t==-6:out.extend(signed_digits(value))
  elif t in(-2,-3):out.append(t&65535)
  else:require(t>=0,'unsupported dynamic source control');out.append(t)
 require(len(out)<=128,'original finite formatter capacity');return out

class Original(text.Original):
 def __init__(self,*args,resources,remainder_report,**kwargs):
  super().__init__(*args,resources=resources,**kwargs)
  self.dato=audit.archive_entries(resources['DATO.DAT']);self.portraits={}
  self.remainder_path=remainder_report;self.remainder=json.loads(snapshot(remainder_report,self.guards,REMAINDER_SHA))
  require(self.remainder['ok']and self.remainder['guards_unchanged']and self.remainder['original_cases']==2093,'retained remainder original qualification')
 def portrait(self,id,frame):
  key=(id,frame)
  if key not in self.portraits:
   matches=[r for r in self.rows['results']if r['asset']==f'portrait:{id}:{frame}'and r['variant']=='forward'];require(len(matches)==1,'unique retained portrait')
   row=matches[0];b=self.dato[id];a=struct.unpack_from('<I',b,4*frame)[0];end=struct.unpack_from('<I',b,4*frame+4)[0]if frame<3 else len(b)
   require(sha(b[a:end])==row['source_sha256']and row['stride']==320,'same portrait source span')
   raw=core.expected_payload(self.glyph_report,row,self.guards);pixels,palette,mask,pm=core.split_payload(raw,64000)
   require(sum(mask)==6400 and all(mask[(5+y)*320+232+x]for y in range(80)for x in range(80)),'portrait80 square exact independent stores')
   self.portraits[key]=bytes(pixels[(5+y)*320+232+x]for y in range(80)for x in range(80))
  return self.portraits[key]
 def picture(self,page,mask,id,frame,opening=False):
  origin={128:4283,129:11662,130:6888,131:3208,132:3208}.get(id,36887)
  # All selected normal cases use DATO0/origin36887 and merchant128/origin4283.
  require(id in(0,128),'finite original portrait-origin map')
  x,y=origin%320,origin//320;reverse=opening or origin==36887;data=self.portrait(id,frame)
  for row in range(80):
   for col in range(80):self.put(page,mask,(y+row)*320+(x-col if reverse else x+col),data[row*80+col])
 def arrow(self,page,mask,frame,x):
  raw=self.ui_blocks[frame];w,h=struct.unpack_from('<HH',raw)
  require(w*h+4==len(raw)and 0<w<=32 and 0<h<=32,'complete source raw-arrow rectangle, not decoder guess')
  y=176 if frame==13 else 181
  for j,v in enumerate(raw[4:]):self.put(page,mask,(y+j//w)*320+x+j%w,v)
 def slots(self,page,mask,ui,save,selected):
  row=next(r for r in self.remainder['cases']if r['asset']=='OTHER-013-016')
  raw=core.bounded_unpack(snapshot(self.remainder_path.parent/row['comparison_payload']['path'],self.guards,row['comparison_payload']['sha256']),54856)
  px,_,_,_=core.split_payload(raw,26660)
  for j,v in enumerate(px):self.put(page,mask,(112+j//310)*320+5+j%310,v)
  all_tokens=[]
  for n in range(4):
   fg=201 if n==selected else 205;y=119+19*n
   tokens=expand(ui,549,n+1,181,181);all_tokens.extend(tokens)
   for i,t in enumerate(tokens):self.letter(page,mask,t,y*320+10+16*i,320,fg,0)
   stage=save[0x312b+n*0xa28+0xa00]
   for frag,x in([(514,88)]if stage==255 else[(514+stage,40),(550+stage,130)]):
    self.literal(page,mask,ui[frag],y*320+x,320,0,fg)
  return all_tokens

def source_expected(oracle,ui,save,c):
 page=text.seed(64000,320);mask=bytearray(64000)
 if c['fragment']==549:
  tokens=oracle.slots(page,mask,ui,save,c['value']);return text.payload(page,mask),tokens,{'ticks':0,'pcm':0,'rng':0,'takes':0,'injected':0,'queued_after':0,'peeked_before':0,'rng_before':0x7a18,'rng_after':0x7a18,'source_prefix_only':False}
 oracle.picture(page,mask,c['portrait'],0,True)
 tokens=expand(ui,c['fragment'],c['value'],c['new'],c['old']);at=c['y']*320+c['x'];start=at
 helpers=0;phase=0;pair=0;pages=0;prefix=False
 for token in tokens:
  if token in(65534,65533):
   start+=19*320;at=start
   if token==65533:
    oracle.picture(page,mask,c['portrait'],0);oracle.arrow(page,mask,18,143 if c['role']=='prepare'else 191)
    oracle.arrow(page,mask,13,143 if c['role']=='prepare'else 191);pages+=1
   continue
  if token==65519:
   for y in range(16):
    for x in range(16):oracle.put(page,mask,at+y*320+x,74)
   prefix=True;break
  oracle.letter(page,mask,token,at,320,205,74);at+=16
  if not c['track']:
   helpers+=1;pair+=1
   if pair==2:
    pair=0;phase=(phase+1)%4;oracle.picture(page,mask,c['portrait'],1 if phase==3 else phase)
 rng_after=0x7a18
 for _ in range(pages):
  rng_after=(rng_after+0x9014)&65535;rng_after=((rng_after<<3)|(rng_after>>13))&65535
 expected={'ticks':helpers,'pcm':helpers,'rng':pages,'takes':pages,'injected':pages*(not c['track']),
  'queued_after':0 if pages else c['track'],'peeked_before':c['track'],'rng_before':0x7a18 if c['role']=='prepare'else 0,
  'rng_after':rng_after if c['role']=='prepare'else 0,'source_prefix_only':prefix}
 return text.payload(page,mask),tokens,expected

class Probe(core.Probe):
 def dynamic(self,c):
  if not self.ready:
   item=self.replies.get(timeout=self.timeout)
   if isinstance(item,BaseException):raise item
   require(item[0]=='inputs_bound','resource buffer handshake before any request');self.ready=True
  self.process.stdin.write(c['line'].encode('ascii'));self.process.stdin.flush();item=self.replies.get(timeout=self.timeout)
  if isinstance(item,BaseException):raise item
  header,raw=item
  require(header.get('request')==c['request']and(header.get('op'),header.get('a'),header.get('b'),header.get('variant'))==('dynamic',c['id'],c['fragment'],c['track']*2),'actual finite parameters differ')
  require(header.get('owner_guard')is True and header.get('page_bytes')==64000 and header.get('stride')==320 and header.get('height')==200,'full actual owner required')
  return header,raw

def producer(path,root,guards):
 receipt=json.loads(snapshot(path,guards));require(receipt.get('schema')=='fd2-f4-dynamic-producer-v1'and Path(receipt['source_root']).resolve()==root,'producer schema/root')
 require(MINIMUM<=set(receipt['sources']),'missing direct source contract')
 for name,digest in receipt['sources'].items():
  p=(root/name).resolve();require(p.is_relative_to(root),'source escaped root');snapshot(p,guards,digest)
 seen=set();roles=set()
 for f in receipt['files']:
  p=Path(f['path']).resolve();require(str(p)not in seen,'duplicate producer identity');seen.add(str(p));snapshot(p,guards,f['sha256'])
  require((guards[str(p)]['size'],guards[str(p)]['mtime_ns'])==(f['size'],f['mtime_ns']),'producer mtime/size');roles.add(f['role'])
 require({'build_log','tlog','static_library','producer_source','executable'}<=roles,'actual compilation closure required')
 result={}
 for role in('prepare','progress'):
  f=receipt['executables'][role];p=Path(f['path']).resolve();require(str(p)in seen and guards[str(p)]['sha256']==f['sha256'],'exact archived executable required');result[role]=p
 return result

def source_contract(args,guards):
 image=snapshot(args.image,guards,source.IMAGE_SHA)
 s7=json.loads(snapshot(args.source07,guards,text.SOURCE07_SHA));ps=json.loads(snapshot(args.pixel_source,guards,PIXEL_SHA));ss=json.loads(snapshot(args.slot_source,guards,SLOT_SHA))
 for va in(0x15f84,0x164e8,0x16559,0x16c57):source.strict_function(s7,va,image)
 source.strict_function(ps,0x4ed7a,image);source.strict_function(ss,0x29ab2,image)


def validate_actual(header,raw,c,tokens,meta,expected):
 require(header.get('consumer_result')==0 and header.get('business_unchanged')is True,'actual consumer rejected/changed business')
 require(header.get('deadline_before')==0 and header.get('deadline_after')==0,'this normal text/ACK scope leaked a host deadline')
 require(header.get('tokens')==tokens,'actual formatter tokens differ')
 for k,v in meta.items():require(type(header.get(k))is type(v)and header[k]==v,'actual source phase/input/RNG contract differs '+k)
 difference=core.mismatch(expected,raw,64000)
 require(difference is None,'whole UI owner/palette/value-mask differs: '+str(difference))
 return difference


def main(argv=None):
 p=argparse.ArgumentParser(description=__doc__)
 for n in('out','output-root','glyph-report','ui-report','remainder-report','image','source07','pixel-source','slot-source'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--root',type=Path,default=ROOT);p.add_argument('--producer-receipt',type=Path);p.add_argument('--plan-only',action='store_true');a=p.parse_args(argv)
 root=a.root.resolve();out=core.checked_output(root,a.out,a.output_root);out.mkdir();guards=dict(text.BOOT);probes={};failed=False;primary=None
 report={'schema':'fd2-f4-dynamic-ui-composition-v1','ok':False,'actual_C_cases':0,'original_CPU_Cases_this_run':0,'GUI_or_build':False,'cases':[],
  'scope':'Thirty UI0 fragments finite formal Prepare/Play and real four slots. Normal two input histories; wide positive gain is defined callee domain, negative only first-font-reader prefix plus H; no natural wide/negative proof or global formatter equivalence.'}
 try:
  for f in[Path(__file__),Path(text.__file__),Path(source.__file__),Path(decode_save.__code__.co_filename)]:snapshot(f,guards)
  source_contract(a,guards);resources={}
  for n,digest in[('FDOTHER.DAT',audit.OTHER_SHA),('FDTXT.DAT',audit.TEXT_SHA),('DATO.DAT',core.RESOURCES['DATO.DAT']),('START.SAV','2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310')]:
   resources[n]=snapshot(root/'original_game'/n,guards,digest)
  class FrozenSave:
   def read_bytes(self):return resources['START.SAV']
  save=decode_save(FrozenSave());ui=audit.text_fragments(audit.archive_entries(resources['FDTXT.DAT'])[0])
  oracle=Original(a.glyph_report.resolve(),a.ui_report.resolve(),resources=resources,guards=guards,remainder_report=a.remainder_report.resolve())
  work=requests(ui);report['unique_fragments']=len({c['fragment']for c in work});report['requests']=len(work)
  if not a.plan_only:
   require(a.producer_receipt is not None,'qualified producer receipt before any C launch');executables=producer(a.producer_receipt,root,guards)
   for role in('prepare','progress'):
    binding={k:v for k,v in resources.items()if role=='prepare'or k!='START.SAV'}
    probes[role]=Probe(executables[role],root,out,role,30,binding,mode='--f4-dynamic',max_page_bytes=64000)
  with(out/'cases.jsonl').open('x',encoding='utf-8')as log:
   for c in work:
    expected,tokens,meta=source_expected(oracle,ui,save,c);item={**c,'expected_sha256':sha(expected),'expected_tokens':tokens,'source_meta':meta,'actual_C_executed':False}
    try:
     if not a.plan_only:
      header,raw=probes[c['role']].dynamic(c);item['actual_C_executed']=True;item['actual_sha256']=sha(raw);item['header']=header
      for kind,blob in(('expected',expected),('actual',raw)):
       packed=zlib.compress(blob,9);path=out/f'{kind}-{c["id"]:04d}.zbin'
       with path.open('xb')as stream:stream.write(packed)
       item[kind+'_payload']={'path':path.name,'sha256':sha(packed),'unpacked_sha256':sha(blob),'unpacked_bytes':len(blob)}
      item['difference']=core.mismatch(expected,raw,64000)
      validate_actual(header,raw,c,tokens,meta,expected);report['actual_C_cases']+=1
    except BaseException as e:
     item['error']=str(e);report['failed_case']=item
     log.write(json.dumps(item,ensure_ascii=False)+'\n');log.flush();raise
    log.write(json.dumps(item,ensure_ascii=False)+'\n');log.flush();report['cases'].append({k:v for k,v in item.items()if k not in('line','expected_tokens','header')})
  report['ok']=True;report['plan_only']=a.plan_only;report['retained_original_glyphs_used']=len(oracle.glyphs);report['retained_portrait_frames_used']=len(oracle.portraits)
 except BaseException as e:failed=True;primary=e;report['primary_error']=str(e)
 finally:
  for role,pr in probes.items():
   try:pr.close(failed)
   except BaseException as e:report.setdefault('cleanup_errors',[]).append({'role':role,'error':str(e)});failed=True
  after={};late=[]
  for name,old in guards.items():
   try:
    current={};snapshot(name,current);after[name]=current[name]
    if after[name]!=old:late.append(name)
   except BaseException as e:after[name]={'error':str(e)};late.append(name)
  report['inputs_before']=guards;report['inputs_after']=after;report['guards_unchanged']=not late
  if late:report['late_guard_errors']=late;failed=True
  if failed:report['ok']=False
  with(out/'report.json').open('x',encoding='utf-8')as f:json.dump(report,f,ensure_ascii=False,indent=2)
 print(json.dumps({'ok':report['ok'],'requests':report.get('requests'),'actual_C_cases':report['actual_C_cases'],'report':str(out/'report.json')},ensure_ascii=False))
 if failed:print('动态UI核验失败：'+report.get('primary_error','来源或清理检查未通过'),file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
