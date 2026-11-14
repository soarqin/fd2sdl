"""S5：完整24618调度/原mask与单页C消费差分；I/O叶明确替身，非GUI。"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
from audit_stage_data import archive_entries,unique_keys as strict_object
IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
READSET_SHA='8d8d8a71301bce4e2ce57b70b8fe031532d4ef9b194b41950a4794d8deb2be0f'
PAGE,TEMP,LUT,BANK1,BANK2,STACK,RET=0x100000,0x130000,0x180000,0x1a0000,0x1c0000,0x700000,0x7ffff0
SIZE=456*336;VIEW=72*456+72
def sha(b):return hashlib.sha256(b).hexdigest()
def execute(image,entries,args,audio):
 from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
 from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,
  UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW)
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x800000);u.mem_write(0,image)
 def wr(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def rd(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def argv(n):return [rd(u.reg_read(UC_X86_REG_ESP)+4+4*i) for i in range(n)]
 def ret(value=0,extra=0):
  sp=u.reg_read(UC_X86_REG_ESP);pc=rd(sp);u.reg_write(UC_X86_REG_ESP,sp+4+extra)
  u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,pc)
 original_page=bytes((i*17+23)&255 for i in range(SIZE))
 temporary=bytearray((i*7+43)&255 for i in range(SIZE))
 for y in range(192):temporary[VIEW+y*456:VIEW+y*456+312]=bytes((x+3*y)&255 for x in range(312))
 u.mem_write(PAGE,original_page);u.mem_write(TEMP,bytes(temporary));u.mem_write(0xa0000,b'\xa5'*64000)
 u.mem_write(LUT,entries[3][1]);u.mem_write(BANK1,entries[80][1]);u.mem_write(BANK2,entries[80][1])
 for a,v in ((0x53a49,PAGE),(0x53a6d,LUT),(0x53b13,0x5a5a5a5a),(0x53aa9,7),(0x53aad,11),(0x53ee4,0x12345678),(0x54133,0)):
  wr(a,v)
 u.mem_write(0x53ef1,bytes([audio]));u.mem_write(0x51e62,b'\x01')
 wr(STACK,RET)
 for n,v in enumerate(args):wr(STACK+4+4*n,v)
 sentinels={UC_X86_REG_EBX:0x123456,UC_X86_REG_ESI:0x234567,UC_X86_REG_EDI:0x345678,UC_X86_REG_EBP:0x456789}
 for reg,v in sentinels.items():u.reg_write(reg,v)
 u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x037f)
 events=[];frames=[];loads=[];frees=[];copies=[];visited=set();mask_args=[];ail=[];actor_calls=0
 def hook(uc,pc,size,user):
  nonlocal actor_calls
  visited.add(pc)
  if pc==0x3702f:
   eax=u.reg_read(UC_X86_REG_EAX);ret(eax,4)
  elif pc==0x111ba:
   a=argv(3);assert a[1:]==[0,80] and rd(0x53b13)==0
   ptr=(BANK1,BANK2)[len(loads)];loads.append(a);events.append(['load_bank',80]);ret(ptr)
  elif pc==0x3706e:
   a=argv(1);assert a==[SIZE];events.append(['allocate',SIZE]);ret(TEMP)
  elif pc==0x3776e:
   a=argv(1);frees.extend(a);events.append(['free_temp']);ret()
  elif pc==0x11eee:
   a=argv(6);assert a==[TEMP+VIEW,456,13,8,7,11]
   events.append(['terrain',456,13,8,7,11]);ret()
  elif pc==0x3771c:
   dst,src,length=argv(3)
   if length==SIZE:
    assert (dst,src)==(PAGE,TEMP);copies.append([dst,src,length]);events.append(['restore_page',SIZE])
   u.mem_write(dst,bytes(u.mem_read(src,length)));ret(dst)
  elif pc==0x22046:
   a=argv(6);mask_args.append(a)
   assert bytes(u.mem_read(PAGE,SIZE))==bytes(temporary)
   events.append(['mask',a[0],a[1],a[2],0,192,9-len(mask_args)+1])
  elif pc==0x127a9:
   actor_calls+=1
   # 独立可辨识actor/前景替身；原22046必须确实在两个pass之间调用。
   for y in range(90,114):u.mem_write(PAGE+VIEW+y*456+120,bytes([177])*24)
   events.append(['actors']);ret()
  elif pc==0x11eb0:
   a=argv(6);assert a==[0xa0504,320,PAGE+VIEW,456,312,192]
   events.append(['project',312,192,456,320,4,4])
  elif pc==0x3790a:
   a=argv(1);events.append(['delay_ms',a[0]])
   if a==[5]:
    vga=bytes(u.mem_read(0xa0000,64000));page=bytes(u.mem_read(PAGE,SIZE))
    assert all(vga[(y+4)*320+4:(y+4)*320+316]==page[VIEW+y*456:VIEW+y*456+312] for y in range(192))
    assert vga[:1284]==b'\xa5'*1284 and vga[-1284:]==b'\xa5'*1284
    frames.append({'index':len(frames),'page_sha256':sha(page),'vga_sha256':sha(vga)})
   ret()
  elif pc==0x11df2:
   a=argv(3);events.append(['palette_add',*a]);ret()
  elif pc in {0x39805,0x39521,0x39694,0x39aae,0x39798}:
   n={0x39805:1,0x39521:1,0x39694:3,0x39aae:2,0x39798:1}[pc]
   a=argv(n);ail.append([hex(pc),a]);events.append(['AIL',hex(pc),a]);ret()
 u.hook_add(UC_HOOK_CODE,hook)
 u.emu_start(0x24618,RET,count=20000000)
 assert u.reg_read(UC_X86_REG_EIP)==RET and u.reg_read(UC_X86_REG_ESP)==STACK+4
 assert all(u.reg_read(reg)==v for reg,v in sentinels.items())
 assert len(loads)==2 and frees==[TEMP] and rd(0x53b13)==BANK2
 assert len(mask_args)==len(copies)==len(frames)==actor_calls==9
 x,y,r,step=args
 expected=[[24*x+12,24*y+16,r+i*step,0,192,LUT+struct.unpack_from('<I',entries[3][1],6+(9-i)*4)[0]] for i in range(9)]
 assert mask_args==expected
 assert [x for x in events if x[0]=='delay_ms']==[['delay_ms',5]]*9+[['delay_ms',500]]+[['delay_ms',4]]*32
 assert [x for x in events if x[0]=='palette_add']==[['palette_add',0,255,i] for i in range(0,64,2)]
 assert (len(ail)==5 if audio else not ail)
 if audio:assert ail[2]==['0x39694',[0x12345678,BANK1+71956,11022]] and ail[3]==['0x39aae',[0x12345678,1]]
 assert bytes(u.mem_read(TEMP,SIZE))==bytes(temporary)
 expected_events=[['load_bank',80],['allocate',SIZE],['terrain',456,13,8,7,11]]
 if audio:expected_events += [['AIL',a,b] for a,b in ail]
 for i,a in enumerate(expected):
  expected_events += [['restore_page',SIZE],['mask',a[0],a[1],a[2],0,192,9-i],
   ['actors'],['project',312,192,456,320,4,4],['delay_ms',5]]
 expected_events += [['free_temp'],['delay_ms',500]]
 expected_events += [e for j in range(0,64,2) for e in (['palette_add',0,255,j],['delay_ms',4])]
 expected_events += [['load_bank',80]]
 assert events==expected_events
 return {'args':list(args),'audio_enabled':audio,'events':events,'frames':frames,'mask_args':mask_args,
  'source_instruction_count':len(visited),'source_instruction_vas':[hex(x) for x in sorted(visited)],
  'registers_and_stack_restored':True,'final_bank_is_second_load':True,
  'temporary_only_freed':True,'whole_page_restored_each_frame':True,'exact_312x192_stride_projection':True}
def validate_probe(rows,runs):
 if len(rows)!=len(runs):raise ValueError('C vector count differs')
 for row,original in zip(rows,runs):
  expected={'terrain_calls':1,'uploads':9,'cues':int(bool(original['audio_enabled'])),
   'page_creates':1,'page_destroys':1,'logic_unchanged':True}
  if set(row)!=set(expected)|{'frames'}:raise ValueError('C protocol fields differ')
  for key,value in expected.items():
   if type(row[key]) is not type(value) or row[key]!=value:raise ValueError('C owner/count differs: '+key)
  if len(row['frames'])!=9:raise ValueError('C frame count differs')
  for actual,wanted in zip(row['frames'],original['frames']):
   if set(actual)!={'page_hex','vga_hex'}:raise ValueError('C frame protocol differs')
   for key,length in (('page',SIZE),('vga',64000)):
    text=actual[key+'_hex']
    if not isinstance(text,str) or len(text)!=length*2:raise ValueError('C frame length differs')
    raw=bytes.fromhex(text)
    if len(raw)!=length or sha(raw)!=wanted[key+'_sha256']:raise ValueError('C/original '+key+' pixels differ')
 return {'vectors':len(runs),'page_frames':len(runs)*9,'VGA_frames':len(runs)*9,
  'consumer':'actual scene_transition scope plus actual Game terrain/mask/actors/page; upload/audio and allocation seed are private TU substitutes',
  'whole_Scene_cold_graph_or_App_claimed':False}

def output_guard(output,original,inputs):
 path=output.resolve();source=original.resolve()
 if path.exists() or path==source or source in path.parents or path in {p.resolve() for p in inputs}:
  raise ValueError('output would overwrite evidence or original source')
 if ROOT in path.parents and (ROOT/'build') not in path.parents:
  raise ValueError('project-local output must stay in ignored build directory')

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--image',type=Path,required=True)
 parser.add_argument('--readset',type=Path,required=True)
 parser.add_argument('--original',type=Path,default=ROOT/'original_game')
 parser.add_argument('--probe',type=Path)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args()
 inputs=[args.image,args.readset,args.original/'FDOTHER.DAT',Path(__file__)]
 if args.probe:inputs.append(args.probe)
 output_guard(args.output,args.original,inputs)
 before={str(path.resolve()):sha(path.read_bytes()) for path in inputs}
 image=args.image.read_bytes();source=args.readset.read_bytes();dat=(args.original/'FDOTHER.DAT').read_bytes()
 if sha(image)!=IMAGE_SHA or sha(source)!=READSET_SHA:raise ValueError('strict image/readset identity differs')
 readset=json.loads(source,object_pairs_hook=strict_object)
 spans=[]
 for function in readset['functions']:
  for chunk in function['chunks']:
   a,b=chunk['start'],chunk['end'];raw=bytes.fromhex(chunk['raw_hex'])
   if raw!=image[a:b] or sha(raw)!=chunk['sha256']:raise ValueError('Domain/strict chunk bytes differ')
   spans.append((a,b))
 entries=archive_entries(dat)
 vectors=[(4,3,10,8),(4,3,10,10),(4,3,2,2),(0,0,10,8),(10,6,10,10),(7,5,2,2)]
 runs=[execute(image,entries,v,a) for v in vectors for a in (0,1)]
 substitutes={0x3702f,0x111ba,0x3706e,0x3776e,0x11eee,0x3771c,0x127a9,0x3790a,0x11df2,
  0x39805,0x39521,0x39694,0x39aae,0x39798}
 visited={int(pc,16) for run in runs for pc in run['source_instruction_vas']}
 if not all(pc in substitutes or any(a<=pc<b for a,b in spans) for pc in visited):raise ValueError('executed instruction outside complete source readset')
 consumer=None
 if args.probe:
  lines=[' '.join(map(str,run['args']+[run['audio_enabled']])) for run in runs]
  result=subprocess.run([str(args.probe.resolve()),'--s5-transition-vectors'],cwd=ROOT,
   input='\n'.join(lines)+'\n',text=True,capture_output=True,timeout=120)
  if result.returncode:raise ValueError('C probe failed: '+result.stderr[-1500:])
  consumer=validate_probe([json.loads(line,object_pairs_hook=strict_object) for line in result.stdout.splitlines()],runs)
 for path in inputs:
  if sha(path.read_bytes())!=before[str(path.resolve())]:raise ValueError('source/probe changed during comparison')
 report={'schema':'fd2-s5-transition-full-body-CPU-C-v1','original_CPU_executed':True,'actual_C_executed':consumer is not None,
  'inputs_sha256':before,'all_executed_non_substitute_instructions_in_full_readset':True,
  'runs':runs,'C_consumer':consumer,
  'source_executed':['complete24618 including both1d4cb and pure10b46 return','22046 mask','219ad geometry and originalsqrt/CHP','11eb0 row projection','25a96 SFX bank/11 sample addressing'],
  'substitutes':['Watcom stack probe','malloc/free recorded, deterministic allocated bytes','res_load(FDOTHER80) recorded and actual raw bank supplied','original terrain renderer controlled312x192; C uses matching real RLE Game renderer','original127a9 controlled24x24; C uses matching real Game actor draw','memmove equivalent byte copy','delay_ms trace, no physical time','11df2 parameter trace; completeDAC alreadyS4','AIL device I/O functions recorded'],
  'limits':['controlled renderer/memory state; no natural App source binding/runtime proof','six argument states x two audio gates, not all runtime coordinates','no GUI, physical timing, hardware audio, original cancellation or allocation-failure equivalence']}
 args.output.parent.mkdir(parents=True,exist_ok=True)
 args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps({'output':str(args.output),'sha256':sha(args.output.read_bytes()),'vectors':len(runs),'mask_frames':len(runs)*9,'actual_C_executed':consumer is not None}))

if __name__=='__main__':main()
