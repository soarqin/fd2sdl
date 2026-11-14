"""S5完整22253/LMI/mask原码与实际teleport单叶C逐页/xy差分，非GUI。"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
from verify_s5_transition import ROOT,SIZE,VIEW,PAGE,TEMP,LUT,BANK1,STACK,RET,IMAGE_SHA,sha,output_guard
from audit_stage_data import archive_entries,unique_keys

READSET_SHA='683ec0e987b4fff9d515677df1fc0d9d1d77d9e95ab11f816135fd2bc06cdbb2'
ACTORS,LMI=0x200000,0x300000

def execute(image,entries,args):
 from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
 from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,
  UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW)
 dx,dy,drawx,drawy,focusx,focusy,audio=args
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x800000);u.mem_write(0,image)
 def rd(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def wr(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def argv(n):return [rd(u.reg_read(UC_X86_REG_ESP)+4+4*i) for i in range(n)]
 def ret(value=0,extra=0):
  sp=u.reg_read(UC_X86_REG_ESP);pc=rd(sp);u.reg_write(UC_X86_REG_ESP,sp+4+extra)
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,pc)
 initial_page=bytes((i*17+23)&255 for i in range(SIZE));temporary=bytearray((i*7+43)&255 for i in range(SIZE))
 for y in range(192):temporary[VIEW+y*456:VIEW+y*456+312]=bytes((x+3*y)&255 for x in range(312))
 u.mem_write(PAGE,initial_page);u.mem_write(TEMP,bytes(temporary));u.mem_write(0xa0000,b'\xa5'*64000)
 u.mem_write(LUT,entries[3][1]);u.mem_write(LMI,entries[6][1]);u.mem_write(BANK1,entries[81][1])
 record=bytearray(80);record[0:2]=bytes([12,15]);record[6]=2;record[64:66]=struct.pack('<H',93)
 u.mem_write(ACTORS,bytes(record));before_record=bytes(record)
 for a,v in ((0x53a49,PAGE),(0x53a6d,LUT),(0x53ad1,LMI),(0x53a45,ACTORS),(0x53beb,1),
  (0x53aa9,7),(0x53aad,11),(0x53ab9,focusx),(0x53abd,focusy),(0x53ee4,0x12345678),(0x54133,0)):
  wr(a,v)
 u.mem_write(0x53ef1,bytes([audio]));u.mem_write(0x51e62,b'\x01');wr(STACK,RET)
 for n,v in enumerate((0,dx,dy,drawx,drawy)):wr(STACK+4+4*n,v)
 regs={UC_X86_REG_EBX:0x123456,UC_X86_REG_ESI:0x234567,UC_X86_REG_EDI:0x345678,UC_X86_REG_EBP:0x456789}
 for reg,v in regs.items():u.reg_write(reg,v)
 u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x037f)
 frames=[];events=[];visited=set();counts={'sprite':0,'contract':0,'wipe':0,'expand':0};frees=[];ail=[]
 def snapshot(tag,index):
  raw=bytes(u.mem_read(ACTORS,80));row={'tag':tag,'index':index,'x':raw[0],'y':raw[1],
   'page_sha256':sha(bytes(u.mem_read(PAGE,SIZE))),'vga_sha256':sha(bytes(u.mem_read(0xa0000,64000)))}
  if tag=='icon':row['base_sha256']=sha(bytes(u.mem_read(TEMP,SIZE)))
  frames.append(row)
 def hook(uc,pc,size,user):
  visited.add(pc)
  if pc==0x3702f:ret(u.reg_read(UC_X86_REG_EAX),4)
  elif pc==0x111ba:
   a=argv(3);assert a[1:]==[0,81];events.append(['load_bank',81]);ret(BANK1)
  elif pc==0x3706e:
   assert argv(1)==[SIZE];events.append(['allocate',SIZE]);ret(TEMP)
  elif pc==0x3776e:
   frees.append(argv(1)[0]);ret()
  elif pc==0x11eee:
   assert argv(6)==[TEMP+VIEW,456,13,8,7,11];events.append(['terrain']);ret()
  elif pc==0x3771c:
   dst,src,length=argv(3);u.mem_write(dst,bytes(u.mem_read(src,length)));ret(dst)
  elif pc==0x127a9:
   x,y=bytes(u.mem_read(ACTORS,2));x=72+(x-7)*24;y=66+(y-11)*24
   for sy in range(24):
    if 0<=y+sy<336:
     left=max(0,x);right=min(456,x+24)
     if right>left:u.mem_write(PAGE+(y+sy)*456+left,bytes([177])*(right-left))
   ret()
  elif pc==0x222af:snapshot('begin',0)
  elif pc==0x225a8:snapshot('icon',124)
  elif pc==0x2238d:snapshot('commit',0)
  elif pc in (0x17aa9,0x3790a):
   a=argv(1)[0];caller=rd(u.reg_read(UC_X86_REG_ESP));events.append(['ticks' if pc==0x17aa9 else 'ms',a])
   if 0x22470<=caller<0x22547:
    assert a==1;snapshot('sprite',114+counts['sprite']);counts['sprite']+=1
   elif 0x22547<=caller<0x22656 and pc==0x3790a:
    assert a==10;snapshot('contract',5-counts['contract']);counts['contract']+=1
   elif 0x22253<=caller<0x22470:
    assert a==10;snapshot('wipe',counts['wipe']);counts['wipe']+=1
   elif 0x22656<=caller<0x226ea:
    assert a==1;snapshot('expand',counts['expand']);counts['expand']+=1
   ret()
  elif pc in {0x39805,0x39521,0x39694,0x39aae,0x39798}:
   n={0x39805:1,0x39521:1,0x39694:3,0x39aae:2,0x39798:1}[pc];ail.append([hex(pc),argv(n)]);ret()
 u.hook_add(UC_HOOK_CODE,hook)
 u.emu_start(0x22253,RET,count=30000000)
 assert u.reg_read(UC_X86_REG_EIP)==RET and u.reg_read(UC_X86_REG_ESP)==STACK+4
 assert all(u.reg_read(reg)==value for reg,value in regs.items())
 assert frees==[TEMP,BANK1] and counts=={'sprite':11,'contract':6,'wipe':18 if drawy==11 else 24,'expand':10}
 expected=bytearray(before_record);expected[:2]=bytes([dx,dy]);assert bytes(u.mem_read(ACTORS,80))==expected
 sample=int((dx,dy)==(drawx,drawy));assert len(ail)==(5 if audio else 0)
 if audio:
  offsets=struct.unpack_from('<3I',entries[81][1],6)
  assert ail[2]==['0x39694',[0x12345678,BANK1+offsets[sample],offsets[sample+1]-offsets[sample]]]
  assert ail[3]==['0x39aae',[0x12345678,1]]
 assert events==[['load_bank',81],['allocate',SIZE],['terrain']]+[['ticks',1]]*11+[['ms',10]]*6+[['ticks',1]]*2+[['ms',10]]*counts['wipe']+[['ticks',1]]*10
 expected_tags=[('begin',0)]+[('sprite',i) for i in range(114,125)]+[('icon',124)]+[
  ('contract',i) for i in range(5,-1,-1)]+[('commit',0)]+[('wipe',i) for i in range(counts['wipe'])]+[
  ('expand',i) for i in range(10)]
 assert [(f['tag'],f['index']) for f in frames]==expected_tags
 return {'args':list(args),'frames':frames,'events':events,'counts':counts,'sample':sample if audio else -1,
  'source_instruction_vas':[hex(x) for x in sorted(visited)],'source_instruction_count':len(visited),
  'raw80_only_xy_changed':True,'registers_stack_restored':True,'temporary_page_and_bank_freed':True}

def validate_probe(rows,runs):
 if len(rows)!=len(runs):raise ValueError('teleport C vector count differs')
 for row,source in zip(rows,runs):
  tags=[('begin',0)]+[('sprite',i) for i in range(114,125)]+[('icon',124)]+[
   ('contract',i) for i in range(5,-1,-1)]+[('commit',0)]+[('wipe',i) for i in range(source['counts']['wipe'])]+[
   ('expand',i) for i in range(10)]
  if [(f['tag'],f['index']) for f in source['frames']]!=tags:raise ValueError('incomplete original prefix oracle')
  expected={'top':int(source['args'][3]==11),'sample':source['sample'],
   'uploads':27+source['counts']['wipe'],'page_creates':1,'page_destroys':1,'logic_unchanged':True}
  if set(row)!=set(expected)|{'frames'}:raise ValueError('teleport C fields differ')
  for key,value in expected.items():
   if type(row[key]) is not type(value) or row[key]!=value:raise ValueError('teleport C owner/count differs '+key)
  if len(row['frames'])!=len(source['frames']):raise ValueError('teleport C prefix count differs')
  for actual,wanted in zip(row['frames'],source['frames']):
   keys={'tag','index','x','y','page_hex','vga_hex'}|({'base_hex'} if wanted['tag']=='icon' else set())
   if set(actual)!=keys:raise ValueError('teleport C prefix fields differ')
   for key in ('tag','index','x','y'):
    if type(actual[key]) is not type(wanted[key]) or actual[key]!=wanted[key]:raise ValueError('teleport C ordered prefix differs '+key)
   for key,size in [('page',SIZE),('vga',64000)]+([('base',SIZE)] if wanted['tag']=='icon' else []):
    value=actual[key+'_hex']
    if not isinstance(value,str) or len(value)!=size*2:raise ValueError('teleport C pixel size differs')
    raw=bytes.fromhex(value)
    if len(raw)!=size or sha(raw)!=wanted[key+'_sha256']:raise ValueError('teleport C/original pixels differ '+wanted['tag']+' '+str(wanted['index'])+' '+key)
 return {'vectors':len(runs),'ordered_prefixes':sum(len(r['frames']) for r in runs),'actual_Game_page_LMI_mask_and_actor_consumers':True,
  'upload_audio_allocation_seed_are_private_TU_substitutes':True,'whole_Scene_App_or_native_claimed':False}

def caller_trace(image,state16=None):
 """原33f78/362e8真正控制流；focus/pan/story/构造/22253为明确边界替身。"""
 from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
 from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x800000);u.mem_write(0,image)
 def rd(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def wr(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def argv(n):return [rd(u.reg_read(UC_X86_REG_ESP)+4+4*i) for i in range(n)]
 def ret(value=0,extra=0):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EIP,rd(sp))
  u.reg_write(UC_X86_REG_ESP,sp+4+extra);u.reg_write(UC_X86_REG_EAX,value)
 state=bytearray(32);state[16]=0 if state16 is None else state16;u.mem_write(0x210000,bytes(state))
 wr(0x53ad5,0x210000);wr(0x53a45,ACTORS);wr(0x53beb,40);wr(0x51a83,55)
 records=bytearray(40*80)
 for i in range(40):records[80*i+52]=0xa7
 u.mem_write(ACTORS,bytes(records));wr(STACK,RET)
 if state16 is None:
  for i,value in enumerate((7,22,18)):wr(STACK+4+4*i,value)
 u.reg_write(UC_X86_REG_ESP,STACK);events=[];visited=set()
 def hook(uc,pc,size,user):
  visited.add(pc)
  if pc==0x3702f:ret(u.reg_read(UC_X86_REG_EAX),4)
  elif pc in (0x135dd,0x12cea):events.append(['pan' if pc==0x135dd else 'focus',*argv(2)]);ret()
  elif pc==0x15f84:events.append(['story',argv(9)[1]]);ret()
  elif pc==0x22253:events.append(['teleport',*argv(5)]);ret()
  elif pc==0x10b4e:events.append(['append_group',argv(1)[0]]);ret()
  elif pc==0x344f2:events.append(['ai',*argv(3)]) # 原typed写点真的执行。
  elif pc==0x3633a:events.append(['display',rd(0x51a83)])
 u.hook_add(UC_HOOK_CODE,hook);u.emu_start(0x33f78 if state16 is None else 0x362e8,RET,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==RET and u.reg_read(UC_X86_REG_ESP)==STACK+4
 if state16 is None:
  assert events==[['focus',22,18],['teleport',7,22,18,22,18]]
 else:
  s=state16;expected=[['pan',16,1],['story',s+2],['display',0],['pan',16,14],
   ['focus',22,18],['teleport',24-s,22,18,22,18]]
  if s==4:expected += [['ai',20,20,11]];records[80*20+52]=0xab
  else:
   expected += [['append_group',s],['ai',24-s,24-s,0],['focus',21,18],
    ['teleport',25+2*s,21,18,21,18],['focus',23,18],['teleport',26+2*s,23,18,23,18]]
   records[80*(24-s)+52]=0xa0
  assert events==expected and rd(0x51a83)==1
 assert bytes(u.mem_read(ACTORS,40*80))==records and bytes(u.mem_read(0x210000,32))==state
 return {'state16':state16,'events':events,'actual_AI_raw52_and_display_writes_verified':state16 is not None,
  'state32_unchanged':True,'source_instruction_vas':[hex(pc) for pc in sorted(visited)],
  'substitutes':['focus/pan/story recorded','group constructor not executed; controlled existing40raw','22253 caller ABI recorded, separate full-function vectors above']}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--image',type=Path,required=True);p.add_argument('--readset',type=Path,required=True)
 p.add_argument('--original',type=Path,default=ROOT/'original_game');p.add_argument('--probe',type=Path)
 p.add_argument('--output',type=Path,required=True);args=p.parse_args()
 inputs=[args.image,args.readset,args.original/'FDOTHER.DAT',Path(__file__),Path(__file__).with_name('verify_s5_transition.py')]
 if args.probe:inputs.append(args.probe)
 output_guard(args.output,args.original,inputs);before={str(p.resolve()):sha(p.read_bytes()) for p in inputs}
 image=args.image.read_bytes();readset=args.readset.read_bytes();dat=(args.original/'FDOTHER.DAT').read_bytes()
 if sha(image)!=IMAGE_SHA or sha(readset)!=READSET_SHA:raise ValueError('teleport strict image/readset identity differs')
 domain=json.loads(readset,object_pairs_hook=unique_keys);spans=[]
 for f in domain['functions']:
  for c in f['chunks']:
   a,b=c['start'],c['end'];raw=bytes.fromhex(c['raw_hex'])
   if raw!=image[a:b] or sha(raw)!=c['sha256']:raise ValueError('teleport Domain chunk differs')
   spans.append((a,b))
 entries=archive_entries(dat)
 vectors=[(11,14,11,14,4,3),(11,14,13,14,2,1),(11,11,11,11,4,0),(12,12,11,11,6,2)]
 runs=[execute(image,entries,(*v,a)) for v in vectors for a in (0,1)]
 callers=[caller_trace(image,None)]+[caller_trace(image,s) for s in range(5)]
 substitutes={0x3702f,0x111ba,0x3706e,0x3776e,0x11eee,0x3771c,0x127a9,0x17aa9,0x3790a,
  0x39805,0x39521,0x39694,0x39aae,0x39798}
 visited={int(pc,16) for r in runs+callers for pc in r['source_instruction_vas']}
 substitutes |= {0x135dd,0x12cea,0x15f84,0x22253,0x10b4e}
 missing=sorted(pc for pc in visited if pc not in substitutes and not any(a<=pc<b for a,b in spans))
 if missing:raise ValueError('teleport executed PC outside complete Domain readset: '+str([hex(x) for x in missing]))
 consumer=None
 if args.probe:
  result=subprocess.run([str(args.probe.resolve()),'--s5-teleport-vectors'],cwd=ROOT,
   input='\n'.join(' '.join(map(str,r['args'])) for r in runs)+'\n',text=True,capture_output=True,timeout=240)
  if result.returncode:raise ValueError('teleport C probe failed '+result.stderr[-1500:])
  consumer=validate_probe([json.loads(line,object_pairs_hook=unique_keys) for line in result.stdout.splitlines()],runs)
 for path in inputs:
  if sha(path.read_bytes())!=before[str(path.resolve())]:raise ValueError('teleport input/probe changed')
 report={'schema':'fd2-s5-teleport-full-source-C-v1','original_CPU_executed':True,'actual_C_executed':consumer is not None,
  'inputs_sha256':before,'runs':runs,'caller_traces':callers,'C_consumer':consumer,
  'source_executed':['complete22253','complete22470/22547/22656 including shared tails','original4ebab/4ec66 LMI','22046/219ad/sqrt/CHP','11eb0','25a96 sample address/loop count'],
  'substitutes':['Watcom stack probe','allocation/free trace and deterministic seed','file I/O supplies actualFDOTHER81','terrain controlled pixels, C real matching RLE','actors controlled opaque24x24 from rawxy, C real matching RLE','memmove byte copy','tick/ms wait trace without hardware clock','AIL I/O trace without device'],
  'limits':['four controlled five-argument/focus states x two audio gates, not natural stage','no GUI/physical timing/audible output/cancellation equivalence','Scene cold walker/App and actualC callback82 runtime remain separate gates; six original caller traces above have explicit boundary substitutes']}
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps({'output':str(args.output),'sha256':sha(args.output.read_bytes()),'vectors':len(runs),'ordered_prefixes':sum(len(r['frames']) for r in runs),'actual_C_executed':consumer is not None}))

if __name__=='__main__':main()
