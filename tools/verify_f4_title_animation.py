"""Main title ANI palette borrowing, actual decoder, retained source pixels.

No original execution/build/GUI/device. ANI0 expected bytes are retained C pages
bound to all96 independent original hashes (not original byte-diff/mask claims).
ANI3 frame0 uses the complete original full-resource payload and full write mask.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,sys,traceback,zlib
ROOT=Path(__file__).resolve().parents[1]
if not(ROOT/'docs/formats/original-reference.json').is_file():ROOT=Path.cwd()
BOOT={}
def frozen(path,guards,expected=None):
    path=Path(path).resolve();s=path.stat();raw=path.read_bytes();t=path.stat();digest=hashlib.sha256(raw).hexdigest()
    if(s.st_size,s.st_mtime_ns)!=(t.st_size,t.st_mtime_ns)or(expected and digest!=expected):raise ValueError('input changed: '+str(path))
    guards[str(path)]={'path':str(path),'size':len(raw),'mtime_ns':t.st_mtime_ns,'sha256':digest};return raw
def module(name,path):
    raw=frozen(path,BOOT);spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec)
    sys.modules[name]=m;exec(compile(raw,str(path),'exec'),m.__dict__);return m
core_path=Path(__file__).resolve().parent/'verify_f4_resource_pixels.py'
if not core_path.is_file():core_path=ROOT/'tools/verify_f4_resource_pixels.py'
core=module('f4_title_ani_core',core_path)
audit=module('f4_title_ani_audit',ROOT/'tools/audit_g4_render_source.py')
sha,require=core.sha,core.require
ANI122_SHA='6f8ae7149395f9b5143d16130bbcfaf0ecd8e03563d348041148c61b7f30a3c7'
FULL_SHA='ce755c30f69441f96f0f56ab3985e06f7cdaf977a056a98db920ac0cdd6d9321'
TITLE_SOURCE_SHA='f64dc247c5e8ff970a900d990961dbdad23c33e8bf27e0127ab9eb889ccf5119'
REQUIRED={'src/main.c','src/animation.c','src/tests/main_title_resource_test.c',
 'src/tests/f4_title_composition_probe.h','src/tests/f4_title_animation_probe.h','src/tests/f4_reader_probe.h',
 'src/archive.c','src/vga.c','src/input.c','src/scene.c','src/CMakeLists.txt'}

def rgb(page,dac):return bytes(dac[3*p+c]for p in page for c in range(3))
def expanded(base):
    require(len(base)==768 and max(base)<=63,'normal6bit base')
    return bytes((v<<2)|(v>>4)for v in base)
def schedule(case):
    require(type(case)is int and 0<=case<4,'normal finite caller cases')
    frames=1 if case==2 else 6 if case==3 else 96
    for i in range(frames):
        yield {'request':['title','start'if i==0 else 'frame',case,i,0],'stride':1,'height':64768,
            'uploads':i,'delays':i,'clears':i+1,'queued':0,'host':0,'source_result':0,'last':False,
            'ani':3 if case==2 else 0,'frame':i}
    yield {'request':['title','done',case,frames,0],'stride':1,'height':64768,'uploads':frames,'delays':frames,
        'clears':1 if case==2 else 6 if case==3 else 97,'queued':int(case==2),'host':int(case==3),
        'source_result':1 if case==2 else -2 if case==3 else 0,'last':True,'ani':3 if case==2 else 0,'frame':frames-1}

class Original:
    def __init__(self,args,resources,guards):
        self.args,self.guards=args,guards;self.cache={}
        self.palettes=audit.archive_entries(resources['FDOTHER.DAT'])
        self.ani=audit.archive_entries(resources['ANI.DAT'])
        self.known=json.loads(frozen(args.ani122,guards,ANI122_SHA))
        require(self.known['frames']==122 and self.known['schema']=='fd2-s5-ani-palette-source-v1','original122 qualification')
        self.hashes={r['frame']:r for case in self.known['cases']if case['entry']==0 for r in case['frames']}
        require(set(self.hashes)==set(range(96)),'all96 original hashes')
        full=json.loads(frozen(args.full,guards,FULL_SHA))
        require(full['guards_unchanged']and full['actual_C_cases_executed']==0,'original full source qualification')
        rows=[r for r in full['results']if r['asset']=='ani:3:0'];require(len(rows)==1,'one original ANI3 frame0')
        row=rows[0];raw=core.bounded_unpack(frozen(args.full.parent/row['payload'],guards,row['payload_sha256']),129536)
        page,pal,mask,pmask=core.split_payload(raw,64000)
        require(sha(page)==row['pixels_sha256']and mask==bytes([1])*64000,'ANI3 initial full overwrite/source page')
        self.cache[3,0]=page
    def pixels(self,entry,frame):
        if(entry,frame)not in self.cache:
            require(entry==0 and frame in self.hashes,'retained finite history')
            raw=core.bounded_unpack(frozen(self.args.retained/f'actual-00-{frame:03d}.zbin',self.guards),129536)
            require(sha(raw[:64000])==self.hashes[frame]['pixels_sha256']and
                sha(raw[64000:64768])==self.hashes[frame]['temporary_palette_sha256'],'independent original hash-bound retained page/TEMP')
            self.cache[entry,frame]=raw[:64000]
        return self.cache[entry,frame]
    def expected(self,case):
        base=self.palettes[99 if case['ani']==3 else 0];dac=expanded(base)
        if case['last']and case['source_result']==0:dac=bytes(768) # real caller fade-out endpoint
        page=self.pixels(case['ani'],case['frame'])
        return page+base+dac+bytes(65536),sha(rgb(page,dac))

def producer(path,root,guards):
    doc=json.loads(frozen(path,guards))
    require(doc['schema']=='fd2-f4-title-animation-producer-v1'and Path(doc['source_root']).resolve()==root,'fresh main/decoder producer')
    require(REQUIRED<=set(doc['sources']),'actual direct producer source set')
    for relative,digest in doc['sources'].items():
        p=(root/relative).resolve();require(p.is_relative_to(root),'producer escaped source root');frozen(p,guards,digest)
    roles=set();seen=set()
    for item in doc['files']:
        p=Path(item['path']).resolve();require(str(p)not in seen,'duplicate producer artifact');seen.add(str(p));frozen(p,guards,item['sha256'])
        require((guards[str(p)]['size'],guards[str(p)]['mtime_ns'])==(item['size'],item['mtime_ns']),'exact compiled artifact');roles.add(item['role'])
    require({'build_log','tlog','static_library','producer_source','executable'}<=roles,'CL/link compiled closure')
    executable=doc['executables']['title_animation'];p=Path(executable['path']).resolve()
    require(str(p)in seen and guards[str(p)]['sha256']==executable['sha256'],'actual archived PE')
    return p

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('out','output-root','ani122','full','retained','ani-source','title-source','image'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--root',type=Path,default=ROOT);p.add_argument('--producer-receipt',type=Path);p.add_argument('--plan-only',action='store_true');args=p.parse_args()
    root=args.root.resolve();out=core.checked_output(root,args.out,args.output_root);out.mkdir();guards=dict(BOOT);probe=None;failed=False
    report={'schema':'fd2-f4-title-animation-owner-v1','ok':False,'actual_C_cases':0,'original_executions':0,'GUI_or_build':False,
        'plan_only':args.plan_only,'actual_C_processes_started':0,'cases':[],'limits':['normal ANI0/ordinary queued ignored/ANI3 skip/ANI0 HOST; malformed partial-error and chained-hook are direct CTest only',
        'ANI0 bytes match independent original hashes via retained current C; no old-original byte diff or physical write mask',
        'no new original timing/audio/hardware qualification; fixed original ANI0 uses no differing palette colours, so this is base/DAC ownership, not a proven visible RGB bug']}
    try:
        frozen(Path(__file__),guards);resources={name:frozen(root/'original_game'/name,guards,core.RESOURCES[name])for name in('FDOTHER.DAT','ANI.DAT')}
        image=frozen(args.image,guards,core.IMAGE_SHA)
        frozen(args.ani_source,guards,core.SOURCES['afm']);core.source_guard(image,args.ani_source,core.SOURCES['afm'])
        frozen(args.title_source,guards,TITLE_SOURCE_SHA);core.source_guard(image,args.title_source,TITLE_SOURCE_SHA)
        oracle=Original(args,resources,guards)
        require(args.plan_only or args.producer_receipt is not None,'fresh qualified producer required before C launch')
        executable=None if args.plan_only else producer(args.producer_receipt,root,guards)
        with(out/'cases.jsonl').open('x',encoding='utf-8')as log:
            for which in range(4):
                if not args.plan_only:
                    probe=core.Probe(executable,root,out,f'main-ANI-{which}',60,resources,mode='--f4-title-ani',max_page_bytes=64768)
                    report['actual_C_processes_started']+=1
                for index,case in enumerate(schedule(which)):
                    wanted,wanted_rgb=oracle.expected(case)
                    row={**case,'expected_sha256':sha(wanted),'expected_RGB_sha256':wanted_rgb,'actual_C_executed':False}
                    if probe:
                        header,actual=probe.request(case)if index==0 else probe.receive(case);core.validate_actual(header,actual)
                        require(header.get('actual_main_wrapper')is True and header.get('actual_ANI_decoder')is True and
                            header.get('history_mask_measured')is False and header.get('buffer_semantics')=='VGA64000+base768'and header.get('palette_semantics')=='actualDAC','real main/decoder full-owner qualification')
                        require(header.get('function_returned')is case['last'],'frame observation versus actual function return')
                        for key in('uploads','delays','clears','queued','host','source_result'):require(header.get(key)==case[key],'source prefix '+key)
                        delta=core.mismatch(wanted,actual,64768);got_rgb=sha(rgb(actual[:64000],actual[64768:65536]))
                        if delta is None and got_rgb!=wanted_rgb:delta={'field':'derived_RGB','expected':wanted_rgb,'actual':got_rgb}
                        row.update(actual_C_executed=True,difference=delta,actual_sha256=sha(actual),actual_RGB_sha256=got_rgb);report['actual_C_cases']+=1
                        if delta:
                            for label,blob in(('expected',wanted),('actual',actual)):(out/f'{label}-{which}-{index:03d}.zbin').write_bytes(zlib.compress(blob,9))
                            log.write(json.dumps(row)+'\n');log.flush();raise ValueError('full page/base/DAC mismatch '+str(case['request']))
                    log.write(json.dumps(row)+'\n');report['cases'].append(row)
                if probe:
                    probe.close(False);report.setdefault('actual_C_loaded_inputs',{})[str(which)]=probe.bound_inputs
                    report.setdefault('owned_C_processes',[]).append({'case':which,'pid':probe.process.pid,'returncode':probe.process.returncode,'reader_retired':not probe.thread.is_alive()});probe=None
                log.flush()
        require(len(report['cases'])==203,'four finite histories=199 frames+4 final source guards')
        report['ok']=True
    except BaseException:
        failed=True;report['error']=traceback.format_exc();core.write_diagnostic(out/'failure.txt',report['error'],report,'primary error')
    finally:
        if probe:
            try:probe.close(failed)
            except BaseException:core.secondary_error(report,'owned C cleanup',traceback.format_exc())
        core.finish_report(out,report,guards)
    print(json.dumps({k:report.get(k)for k in('ok','actual_C_cases','guards_unchanged','error','secondary_errors')}));return 0 if report['ok']else 1
if __name__=='__main__':raise SystemExit(main())
