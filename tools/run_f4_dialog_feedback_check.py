"""SDL-only ordinary Scene feedback, inheriting the retained original15->16.

Default prepares source/oracle only. --run is one owned strict-ON WGC SDL window;
no original launch, new originalCPU, synthetic scene API or early-key claim.
"""
from __future__ import annotations
import argparse,hashlib,json,sys
from contextlib import contextmanager
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
if not(ROOT/'tools/run_f4_flash_check.py').is_file():ROOT=Path.cwd()
sys.path.insert(0,str(ROOT/'tools'))
import run_f4_flash_check as prior
import verify_f4_dialog_history as dialog
from run_original_comparison import quiet_game_command
from run_f2_input_check import source_facts
from verify_f4_resource_pixels import checked_output
host=prior.host;late=prior.late_host;shared=prior.shared;state=prior.state;fixture=prior.fixture
sha=lambda b:hashlib.sha256(b).hexdigest()
OLD_REPORT='9b1902bb78ed5828da2b7e46c9e5bb195faa4add6c37fb083b1c0d500b1bebb1'
OLD_PAIR='71d8fa7494709829ad80719ef3f16a8fcba7fb4da90f90e52a9cc68c180df7ff'
DIALOG_REPORT='a4a5a0921ba8bcc1bb12d402dd84df0f6a42089a170044620f084de5e42d17d9'
DAC_SHA='b439808b4974bb9ceca12b8de39c3f44cd0bef7269f823aff32765ce71ed8a2a'
DEFAULT=Path('F:/fd2sdl-evidence')
def require(x,why):
 if not x:raise ValueError(why)
def read(path,g,digest=None):
 old=g.get(str(Path(path).resolve()));current={};raw=dialog.text.snapshot(path,current,digest)
 require(old is None or old==current[str(Path(path).resolve())],'source changed since first read')
 g.update(current);return raw

def marker_xy(k):return 148-143*k//11,124-12*k//11

def marker_at(rgb,xy):
 x,y=xy
 return b''.join(rgb[((y+r)*320+x)*3:((y+r)*320+x+24)*3]for r in range(24))

def disjoint(a,b):return a[0]+24<=b[0]or b[0]+24<=a[0]or a[1]+24<=b[1]or b[1]+24<=a[1]

def marker_group(samples,pack):
 groups=[];current=[];classified=[]
 for s in samples:
  require(len(s['RGB'])==192000,'full64000 RGB required')
  choices=[]
  for k in range(12):
   got=marker_at(s['RGB'],marker_xy(k))
   # Candidate anchors exclude74: once the border/source geometry is identified,
   # every74 interior store must also match. A bad74 pixel is never discarded.
   if all(got[t*3:t*3+3]==pack.marker[t*3:t*3+3]for t in pack.anchor_pixels):
    require(got==pack.marker,'raw74/nonzero marker field differs at a source-qualified candidate')
    choices.append(k)
  require(len(choices)<=1,'ambiguous source marker parameters')
  if not choices:continue
  k=choices[0];classified.append({'index':s['index'],'k':k})
  if k==11 and not current:continue # shared close15 endpoint alone is not opening16
  if current and k<current[-1][1]:groups.append(current);current=[]
  current.append((s,k))
 if current:groups.append(current)
 valid=[]
 for group in groups:
  pair=next(((a,b)for a in group for b in group if disjoint(marker_xy(a[1]),marker_xy(b[1]))),None)
  if pair:valid.append((group,pair))
 require(len(valid)==1,'need one ordered opening16 marker group with disjoint sampled positions')
 group,(a,b)=valid[0];base=bytearray(a[0]['RGB']);x,y=marker_xy(a[1]);other=b[0]['RGB']
 for r in range(24):base[((y+r)*320+x)*3:((y+r)*320+x+24)*3]=other[((y+r)*320+x)*3:((y+r)*320+x+24)*3]
 witnesses=[]
 for s,k in group:
  x,y=marker_xy(k);expected=bytearray(base)
  for r in range(24):expected[((y+r)*320+x)*3:((y+r)*320+x+24)*3]=pack.marker[r*72:(r+1)*72]
  require(bytes(expected)==s['RGB'],'marker full64000 source relation differs, including raw74 or outside page')
  witnesses.append({key:s[key]for key in('index','path','sha256')if key in s}|{'k':k,'xy':[x,y],'RGB_sha256':sha(s['RGB']),'whole64000_exact':True,'raw74_pixels':324})
 return bytes(base),witnesses,classified

def qualify(samples,pack):
 base,markers,classified=marker_group(samples,pack);models=pack.final_models(base);last=markers[-1]['index'];final=[]
 for s in samples:
  if s['index']<=last:continue
  matches=[pose for pose,rgb in models if rgb==s['RGB']]
  require(len(matches)<=1,'ambiguous complete final16 pose')
  if matches:final.append({key:s[key]for key in('index','path','sha256')if key in s}|{'portrait_frame':matches[0],'glyphs':10,'RGB_sha256':sha(s['RGB']),'whole64000_exact':True})
 require(final,'no complete source-derived final16 whole64000 frame observed')
 return {'whole_before_relation_qualified':True,'absolute_compositor_qualified':False,'all_CPU_frames_qualified':False,'all_five_open_stages_observed':False,
  'early_key_qualified':False,'before_RGB_sha256':sha(base),'markers':markers,'final16':final,'classified_marker_candidates':classified,'sampled_frames':len(samples)}

class Pack:
 def __init__(self,a,g,resources):
  self.a=a;self.g=g;self.resources=resources
  receipt=json.loads(read(a.dialog_report,g,DIALOG_REPORT))
  require(receipt['ok']and receipt['guards_unchanged']and receipt['actual_C_cases']==155,'retained typed155 identity only, not new B2 native')
  source_inputs=receipt['inputs_before']
  def find(digest):
   paths=[Path(n)for n,f in source_inputs.items()if f['sha256']==digest]
   require(len(paths)==1,'unique retained complete-source identity');return paths[0]
  image=read(find(dialog.core.IMAGE_SHA),g,dialog.core.IMAGE_SHA)
  dialog.source_contract(find(dialog.text.SOURCE07_SHA),find(dialog.text.RAW_COPY_SHA),[find(d)for d in dialog.SNAPSHOT_SHAS],find(dialog.MARKER_SHA),image,g)
  read(a.rgb_source,g,DAC_SHA)
  self.oracle=dialog.Original(a.glyph_report,a.ui_report,resources,g);self.palette=self.oracle.resources['FDOTHER.DAT']
  self.palette=dialog.audit.archive_entries(self.palette)[0]
  require(len(self.palette)==768 and max(self.palette)<=63,'fixed6bit palette0')
  w,h,pixels=self.oracle.raw(0);require((w,h)==(24,24)and 0 not in pixels and pixels.count(74)==324,'complete opaque74 raw marker0')
  self.marker=bytes((c<<2)|(c>>4)for p in pixels for c in self.palette[p*3:p*3+3]);self.anchor_pixels=[i for i,p in enumerate(pixels)if p!=74]
  tokens=list(dialog.audit.text_fragments(dialog.audit.archive_entries(resources['FDTXT.DAT'])[27])[16])
  require(tokens==[-18,0,558,53,54,342,53,54,342,585,585,561,-1],'fixed resource26/fragment16 source tokens')
  self.chars=tokens[2:-1];self.overlays=[]
  for pose in range(4):
   x=bytearray(64000);y=bytearray([255])*64000
   for page in(x,y):
    self.oracle.box(page,1,19,5);self.oracle.portrait(page,50,pose,1)
    for i,t in enumerate(self.chars):self.oracle.letter(page,bytearray(64000),t,119*320+95+16*i,320,205,74)
   self.overlays.append((pose,[(at,bytes((c<<2)|(c>>4)for c in self.palette[x[at]*3:x[at]*3+3]))for at in range(64000)if x[at]==y[at]]))
 def final_models(self,base):
  result=[]
  for pose,writes in self.overlays:
   rgb=bytearray(base)
   for at,color in writes:rgb[at*3:at*3+3]=color
   result.append((pose,bytes(rgb)))
  return result

def retained(a,g,pack,p,model):
 old=json.loads(read(a.retained/'report.json',g,OLD_REPORT));pair=json.loads(read(a.retained/'paired/comparison.json',g,OLD_PAIR))
 require(old['ok']and old['guards_unchanged']and old['original_unchanged']and old['sources_unchanged'],'old original run qualification')
 controlled=read(a.retained/'controlled.SAV',g,old['encoded_SAV_sha256']);require(controlled==p['encoded'],'new fixture bytes differ from inherited controlled.SAV')
 before=json.loads(read(a.retained/'source-model.json',g,old['source_model_sha256']))
 require(json.loads(json.dumps(model))==before,'complete serialized source/rgb/record/camera/cache prefix differs')
 reference=old['references'];require(len(reference)==1,'one retained original profile')
 def identities(obj):
  if isinstance(obj,dict):
   if {'path','sha256','size'}<=set(obj):
    blob=read(obj['path'],g,obj['sha256']);require(len(blob)==obj['size'],'retained profile size')
   else:
    for v in obj.values():identities(v)
  elif isinstance(obj,list):
   for v in obj:identities(v)
 identities(reference[0])
 original=pair['cases'][0];require(original['kind']=='original'and original['ok']and original['sources_unchanged']and original['save_unchanged_since_measured_write'],'original role guards')
 require(original['flash_relation']['input_after15']=='exactly one fast ENTER; no input through first16 wait','old ordinary input boundary')
 rows=original['detail_series']['frames'];require(len(rows)==93,'fixed complete original series')
 for row in rows:read(row['path'],g,row['sha256'])
 samples=prior.read_series(rows,rows[0]['pid'],rows[0]['hwnd'],after_index=-1,before_index=93)
 result=qualify(samples,pack)
 require([w['k']for w in result['markers']]==[2,5,7,11],'retained opening observations changed')
 return {'original_report_sha256':OLD_REPORT,'original_pair_sha256':OLD_PAIR,'controlled_SAV_sha256':sha(controlled),'reference':reference[0],
  'old_SDL_is_not_new_B2':True,'original_UI_relation':result,'original81_82':'not qualified as complete stage or source prefix; kept unknown; no cause inferred'}

def current_command(command,executable,count):
 require(count==0 and Path(command[0]).resolve()==executable.resolve(),'only one current formal SDL launch allowed')


def run(a):
 root=a.root.resolve(strict=True);original=root/'original_game';out=checked_output(root,a.artifacts,a.output_root);out.mkdir(exist_ok=False)
 g={};probes=[];source_before=source_facts(root);original_before={str(p.resolve()):prior.fact(p)for p in original.rglob('*')if p.is_file()}
 report={'schema':'fd2.f4.dialog-feedback-native.v1','ok':False,'status':'preparing','controlled_fixture':True,'new_original_executed':False,
  'old_SDL_qualified_as_B2':False,'launches':[],'limitations':['same-run whole64000 UI relation, not absolute field compositor','no allCPU/PIT/audio/early-key or all5stage native claim']}
 previous_running=host.running;previous_output=host.validate_output;previous_late=late.validate_output
 def output(requested,original):return checked_output(root,Path(requested),a.output_root)
 def checks():
  late_inputs=[]
  for n,old in list(g.items()):
   try:
    now={};dialog.text.snapshot(n,now)
    if now[n]!=old:late_inputs.append(n)
   except(OSError,ValueError):late_inputs.append(n)
  try:source_ok=source_facts(root)==source_before
  except(OSError,ValueError):source_ok=False
  try:original_ok={str(p.resolve()):prior.fact(p)for p in original.rglob('*')if p.is_file()}==original_before
  except(OSError,ValueError):original_ok=False
  return {'sources_unchanged':source_ok,'original_unchanged':original_ok,'inputs_unchanged':not late_inputs,'late_inputs':late_inputs}
 @contextmanager
 def running(command,cwd,env,log,driver,receipts,**kw):
  current_command(command,a.sdl,len(report['launches']));actual,_,_=quiet_game_command(command,env);before=checks();require(all(before[k]for k in('sources_unchanged','original_unchanged','inputs_unchanged')),'before-window source guard')
  n=len(receipts)
  with previous_running(command,cwd,env,log,driver,receipts,**kw)as process:
   require(len(receipts)==n+1 and receipts[-1]['command']==actual,'actual SDL receipt command differs');report['launches'].append({'command':actual,'role':'sdl'});yield process
 def exercise(capture,p,save,guard,record):
  cp=shared.checkpoint(capture,record,save,guard);late.continue_result(capture,p,record,cp)
  late.acknowledge(capture,p,record,cp,p['flash15_pages'][:-1],'dialog15-prefix')
  observed=[]
  with prior.detail_series(capture,record)as scope:
   prior.series_checkpoint(capture,scope,'dialog15-last',lambda f:prior.page_match(f,p['flash15_pages'][-1],p['assets']),record['checkpoints'],timeout=60)
   capture.key('ENTER',fast=True)
   def complete(frame):
    row=scope['after'];observed.append({**row,'RGB':frame.convert('RGB').tobytes()})
    if not prior.page_match(frame,p['flash16_pages'][0],p['assets']):return None
    # Once final text is visible, missing early marker samples cannot be repaired by waiting.
    marker_group(observed,pack)
    try:result=qualify(observed,pack)
    except ValueError as e:
     if str(e)=='no complete source-derived final16 whole64000 frame observed':return None
     raise
    record['dialog_UI_relation']=result
    return {'kind':'source_qualified_complete16_UI','marker_whole_frames':len(result['markers']),'full_final_frames':len(result['final16']),'no_ack16':True}
   prior.series_checkpoint(capture,scope,'dialog16-complete',complete,record['checkpoints'],timeout=60)
  require(record['detail_series'].get('checkpoint_PNGs_bound_to_closed_series')is True,'closed owned WGC series required')
  rows=record['detail_series']['frames'];samples=prior.read_series(rows,capture.process.pid,scope['ready']['hwnd'],after_index=-1,before_index=len(rows))
  record['dialog_UI_relation']=qualify(samples,pack);record['input_boundary']='one ENTER after15; no further input through complete16 wait'
  host.require_source_unchanged(save,*guard)
 try:
  for pth in[Path(__file__),Path(prior.__file__),Path(dialog.__file__)]:read(pth,g)
  resources={n:read(original/n,g)for n in('FDOTHER.DAT','DATO.DAT','FDTXT.DAT')}
  pack=Pack(a,g,resources);p=fixture.derive(original,fixture.SCENARIOS[2]);model,records=prior.source_model(p,a.rgb_source)
  report['inheritance']=retained(a,g,pack,p,model);shared.write(out/'source-model.json',model)
  p.update(assets=host.round_assets(original),titles=host.title_templates(original/'FDOTHER.DAT'))
  with state.resource_domain(original,(26,)):
   p['victory_pages']=late.pages_for(p,records,26,(8,13,14),p['manual_state']['roster']);p['flash15_pages']=late.pages_for(p,records,26,(15,),p['manual_state']['roster']);p['flash16_pages']=late.pages_for(p,records,26,(16,),p['manual_state']['roster'])
  (out/'controlled.SAV').write_bytes(p['encoded']);shared.write(out/'fixture.json',p['manifest'])
  for module in tuple(sys.modules.values()):
   name=getattr(module,'__file__',None)
   if name and Path(name).resolve().is_relative_to(root/'tools'):read(name,g)
  if a.run:
   require(sys.platform=='win32','Windows WGC only');prior.owned_job_required()
   for n in('sdl','driver','dosbox','build_dir','snapshot_probe'):
    value=getattr(a,n);require(value is not None and value.exists(),'missing --'+n)
   require(a.dosbox.resolve()==Path(report['inheritance']['reference']['dosbox']['path']).resolve(),'DOSBox identity is inherited, never launched or substituted')
   for binary in(a.sdl,a.sdl.with_suffix('.pdb'),a.driver,a.dosbox,a.snapshot_probe):read(binary,g)
   host.running=running;host.validate_output=output;late.validate_output=output
   args=argparse.Namespace(**vars(a));args.artifacts=out/'current';late.snapshot_preflight(args,p)
   p['source_evidence_paths']=[Path(n)for n in g];p['manifest']['source_guards']={n:v['sha256']for n,v in g.items()}
   result=host.run_prepared(args,p,exercise,(26,),'current-B2-dialog16',report['limitations'],kinds=('sdl',),require_display_on=True)
   passed=result['ok']and len(report['launches'])==1
   report.update(comparison=str(out/'current/comparison.json'),ok=passed,new_SDL_executed=bool(report['launches']),status='passed'if passed else'failed')
  else:report.update(status='prepared',new_SDL_executed=False)
 except BaseException as e:report.update(ok=False,status='failed',error={'type':type(e).__name__,'message':str(e)})
 finally:
  host.running=previous_running;host.validate_output=previous_output;late.validate_output=previous_late
  report['guards']=checks();report['inputs']=g;report['source_before']=source_before;report['original_before']=original_before
  if not all(report['guards'][k]for k in('sources_unchanged','original_unchanged','inputs_unchanged')):report.update(ok=False,status='failed')
  shared.write(out/'report.json',report)
 return report


def main(argv=None):
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','artifacts','rgb-source'):p.add_argument('--'+n,type=Path,required=True)
 for n in('sdl','driver','dosbox','build-dir','snapshot-probe','output-root'):p.add_argument('--'+n,type=Path)
 p.add_argument('--retained',type=Path,default=DEFAULT/'f4-flash-native-20261116-01')
 p.add_argument('--dialog-report',type=Path,default=DEFAULT/'f4-dialog-history-C-20261116-03/report.json')
 p.add_argument('--glyph-report',type=Path,default=DEFAULT/'f4-resource-reader-full-20261116-01/report.json')
 p.add_argument('--ui-report',type=Path,default=DEFAULT/'f4-extra-images256-20261116-02/report.json')
 p.add_argument('--run',action='store_true');a=p.parse_args(argv)
 try:r=run(a)
 except(OSError,ValueError,RuntimeError)as e:p.exit(1,'Scene反馈代表未能准备：'+str(e)+'。请保留失败目录并核来源。\n')
 print(json.dumps({k:r.get(k)for k in('status','ok','error')},ensure_ascii=False));return int(r['status']=='failed')
if __name__=='__main__':raise SystemExit(main())
