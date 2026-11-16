"""Current C typed-text/summary/credits composition against retained original leaves.

No original execution, build, GUI, decoder or runtime text VM. The finite literal
fold uses independent original glyph write roles and fixed caller parameters.
1180 binding rows are 1179 distinct C requests; both denominators are reported.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import traceback
import zlib

ROOT=Path(__file__).resolve().parents[1]
if not (ROOT/'tools/audit_g4_render_source.py').is_file():ROOT=Path.cwd()
BOOT={}
def sha(blob):return hashlib.sha256(blob).hexdigest()
def snapshot(path,guards,expected=None):
    path=Path(path).resolve();before=path.stat();raw=path.read_bytes();after=path.stat()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns) or (expected and sha(raw)!=expected):
        raise ValueError(f'input identity changed: {path}')
    guards[str(path)]={'path':str(path),'sha256':sha(raw),'size':len(raw),'mtime_ns':after.st_mtime_ns}
    return raw
def load_module(name,file):
    local=Path(__file__).resolve().parent/file
    path=local if local.is_file() else ROOT/'tools'/file
    raw=snapshot(path,BOOT);spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);sys.modules[name]=module
    exec(compile(raw,str(path),'exec'),module.__dict__);return module
core=load_module('f4_text_resource_core','verify_f4_resource_pixels.py')
audit=load_module('f4_text_source_audit','audit_g4_render_source.py')

GLYPH_REPORT_SHA='ce755c30f69441f96f0f56ab3985e06f7cdaf977a056a98db920ac0cdd6d9321'
UI_REPORT_SHA='d4382319728dd6ea685b99e3e2950cf486be68c9cc5d9f6c4fe7e2cf8e4a316d'
SOURCE07_SHA='79db0379b9c24d929482dd7f1cc698269fcff7e8c01a1cfebeaf107d808f09fa'
RAW_COPY_SHA='537645c5a91e3dca833f1b36d8af0658afbafd3759241b736f53f5f57e6c90bf'
REQUIRED_SOURCES={
 'src/tests/field_prepare_play_test.c','src/tests/scene_cinematic_test.c',
 'src/tests/f4_text_probe_common.h','src/tests/f4_text_prepare_probe.h','src/tests/f4_text_cinematic_probe.h',
 'src/tests/f4_reader_probe.h','src/tests/f4_remainder_probe.h','src/field_prepare_play.c',
 'src/field_prepare_play.h','src/field_prepare_service_present.h','src/scene_cinematic.c',
 'src/scene_cinematic.h','src/field_info.c','src/field_info.h','src/font.c','src/font.h',
 'src/text.c','src/text.h','src/archive.c','src/archive.h','src/image.c','src/image.h',
 'src/field_page.c','src/field_page.h','src/field_page_private.h','src/CMakeLists.txt',
 'src/tests/test_temp.h','src/field_prepare_service_flow.h','src/field_prepare_service_pick.h',
 'src/field_prepare_view.h','src/field_play.h','src/field_game.h','src/field_unit.h',
 'src/field.h','src/field_audio.h','src/battle_animation.h','src/field_background.h',
 'src/field_visual.h','src/field_command.h','src/save.h','src/static_data.h',
 'src/field_item.h','src/field_magic.h','src/figani.h','src/portrait.h','src/vga.h'}

def seed(size,stride):return bytearray((i*17+i//stride+83)&255 for i in range(size))
def payload(page,mask):return bytes(page)+bytes([165])*768+bytes(mask)+bytes(768)
def pure_tokens(tokens):
    if not tokens or tokens[-1]!=-1 or any(t<0 and t!=-2 for t in tokens[:-1]):
        raise ValueError('only the source-qualified glyph/newline/END finite projection is accepted')
    return tokens[:-1]
def validate_glyph(raw,row,glyph,font):
    n=64000;core.require(len(raw)==2*n+1536,'retained glyph length')
    page,palette,mask,pmask=core.split_payload(raw,n)
    core.require(set(mask)<={0,1} and not any(pmask) and palette==bytes([165])*768,'glyph palette/mask')
    core.require(row.get('stride')==320 and row.get('page_bytes')==64000 and row.get('seed')==83,'original glyph fixed source parameters')
    core.require(row['source_sha256']==sha(font[glyph*32:(glyph+1)*32]) and
        row['abi_input_buffer_sha256']==sha(font),'same source glyph and font owner')
    baseline=seed(n,320);roles=[]
    for at,written in enumerate(mask):
        if written:
            core.require(page[at] in (205,74),'original glyph role must be fg or shadow')
            dx,dy=at%320-8,at//320-4
            core.require(-1<=dx<16 and 0<=dy<17,'original glyph footprint')
            roles.append((dx,dy,page[at]))
        else:core.require(page[at]==baseline[at],'unwritten original glyph changed')
    core.require(glyph!=10 or not roles,'original glyph10 is a no-font-read blank')
    return roles

class Original:
    def __init__(self,glyph_report,ui_report,resources,guards):
        self.guards=guards;self.glyph_report=glyph_report;self.ui_report=ui_report
        self.font=audit.archive_entries(resources['FDOTHER.DAT'])[4];self.glyphs={};self.images={}
        self.rows=json.loads(snapshot(glyph_report,guards,GLYPH_REPORT_SHA))
        core.require(self.rows['schema']=='fd2-f4-normal-resource-oracle-pilot-v1' and
            self.rows['guards_unchanged'] and self.rows['actual_C_cases_executed']==0,'original glyph report')
        self.glyph_rows={int(r['asset'].split(':')[1]):r for r in self.rows['results'] if r['family']=='glyph' and r['variant']=='bg0'}
        core.require(set(self.glyph_rows)==set(range(1824)) and len(self.font)==1824*32,'complete original glyph set')
        self.ui=json.loads(snapshot(ui_report,guards,UI_REPORT_SHA))
        core.require(self.ui['ok'] and self.ui['guards_unchanged'] and self.ui['original_cases']==256,'retained UI original qualification')
        block=audit.archive_entries(resources['FDOTHER.DAT'])[5]
        core.require(block[:4]==b'LMI1' and struct.unpack_from('<H',block,4)[0]==138,'mixed UI exact directory')
        self.ui_blocks=[]
        off=struct.unpack_from('<138I',block,6)
        core.require(off[0]>=558 and all(a<b for a,b in zip(off,off[1:])) and off[-1]<len(block),'UI directory bounds')
        self.ui_blocks=[block[a:b] for a,b in zip(off,(*off[1:],len(block)))]
    def glyph(self,index):
        if index not in self.glyphs:
            row=self.glyph_rows[index];name=row['payload']
            core.require(re.fullmatch(r'case-\d{5}\.zbin',name) is not None,'glyph payload basename')
            raw=core.bounded_unpack(snapshot(self.glyph_report.parent/name,self.guards,row['payload_sha256']),129536)
            core.require(sha(raw[:64000])==row['pixels_sha256'] and sha(raw[64768:128768])==row['write_mask_sha256'],'glyph payload hashes')
            self.glyphs[index]=validate_glyph(raw,row,index,self.font)
        return self.glyphs[index]
    def image(self,frame):
        if frame not in self.images:
            match=[r for r in self.ui['cases'] if r['request'][1:3]==(['uilmi',frame] if frame==22 else ['uirle',frame])]
            core.require(len(match)==1,'unique original UI leaf')
            row=match[0];core.require(row['source_sha256']==sha(self.ui_blocks[frame]),'UI same source span')
            name=row['original_payload']['path'];core.require(re.fullmatch(r'original-\d{3}\.zbin',name) is not None,'UI payload basename')
            n=row['width']*row['height'];raw=core.bounded_unpack(snapshot(self.ui_report.parent/name,self.guards,row['original_payload']['sha256']),2*n+1536)
            pixels,palette,mask,pmask=core.split_payload(raw,n)
            core.require(set(mask)<={0,1} and not any(pmask) and palette==bytes([165])*768,'UI palette/mask')
            self.images[frame]=(row['width'],row['height'],pixels,mask)
        return self.images[frame]
    def put(self,page,mask,at,color):
        core.require(0<=at<len(page),'first real owner store outside bounds')
        page[at]=color;mask[at]=1
    def letter(self,page,mask,index,at,stride,foreground=205,background=0):
        if background:
            for y in range(16):
                for x in range(16):self.put(page,mask,at+y*stride+x,background)
        for x,y,color in self.glyph(index):self.put(page,mask,at+y*stride+x,foreground if color==205 else 76)
    def literal(self,page,mask,tokens,at,stride,pitch=0,foreground=205):
        start=at
        for token in pure_tokens(tokens):
            if token==-2:start+=pitch*stride;at=start
            else:self.letter(page,mask,token,at,stride,foreground);at+=16
    def blit(self,page,mask,frame,at,stride):
        width,height,pixels,writes=self.image(frame)
        for i,written in enumerate(writes):
            if written:self.put(page,mask,at+(i//width)*stride+i%width,pixels[i])
    def raw_bar(self,page,mask,frame,at,stride):
        raw=self.ui_blocks[frame];core.require(len(raw)==9 and raw[:4]==b'\1\0\5\0','original raw1x5 bar')
        for y,color in enumerate(raw[4:]):self.put(page,mask,at+y*stride,color)
    def summary(self,page,mask,tokens):
        # 18c6d/18795/17d6f/187d6 fixed valid record: HP=MP=0, maxima=1, LV=1.
        s=456;x=y=77;self.blit(page,mask,22,y*s+x,s)
        for row in (22,31):
            for col in range(1,102):self.raw_bar(page,mask,29,(y+row)*s+x+21+col,s)
            self.raw_bar(page,mask,30,(y+row)*s+x+123,s)
        for frame,dx,dy in [(31,132,4),(32,138,4),*( (42,126+6*i,21) for i in range(3)),*( (42,126+6*i,30) for i in range(3))]:
            self.blit(page,mask,frame,(y+dy)*s+x+dx,s)
        self.literal(page,mask,tokens,(y+4)*s+x+5,s)

def cases(texts):
    work=[]
    for low,high in audit.NAME_RANGES:
        for fragment in range(low,high+1):
            for style in range(2):work.append({'request':['prepare','plain',fragment,style,0],'stride':320,'height':200,'entry':0,'fragment':fragment,'kind':'plain'})
    for fragment in range(1,137):work.append({'request':['prepare','summary',fragment,0,0],'stride':456,'height':336,'entry':0,'fragment':fragment,'kind':'summary','source_origin':[77,77],'typed_record_projection':{'text_id':fragment-1,'level':1,'hp':0,'mp':0,'hp_max':1,'mp_max':1},'natural_record_producer_claimed':False})
    for fragment,kind,offset,pitch in [(10,0,5865,0),(11,0,12265,0),(44,4,76848,25),(45,3,32008,20)]:
        work.append({'request':['cinematic','credit',fragment,kind,0],'stride':320,'height':700 if kind==4 else 400,'entry':31,'fragment':fragment,'kind':'credit','offset':offset,'pitch':pitch,'binding':'fixed'})
    for low,high,kind,offset in [(1,136,1,5915),(150,178,2,12315)]:
        for fragment in range(low,high+1):work.append({'request':['cinematic','credit',fragment,kind,0],'stride':320,'height':400,'entry':0,'fragment':fragment,'kind':'credit','offset':offset,'pitch':0,'binding':'name' if kind==1 else 'class'})
    # Exact conservative BYTE parameter scan, not a new claim of natural producer reachability.
    for text_id in range(256):
        fragment=(text_id+12)&255
        if fragment>=len(texts[31]):continue
        try:tokens=pure_tokens(texts[31][fragment])
        except ValueError:continue
        x,y=8,100;fits=True
        for token in tokens:
            if token==-2:x=8;y+=20
            else:fits &= x>=1 and x+16<=320 and y>=0 and y+17<=400;x+=16
        if fits:work.append({'request':['cinematic','credit',fragment,3,0],'stride':320,'height':400,'entry':31,'fragment':fragment,'kind':'credit','offset':32008,'pitch':20,'binding':f'conditional description text_id={text_id}'})
    core.require(len(work)==1180,'fixed typed binding projection denominator')
    unique={}
    for item in work:
        key=tuple(item['request'])
        if key in unique:unique[key].setdefault('also_bindings',[]).append(item['binding'])
        else:unique[key]=item
    core.require(len(unique)==1179,'exact duplicate fixed45/description33 only')
    return list(unique.values())

def source_contract(path,raw_copy_path,image,guards):
    source=json.loads(snapshot(path,guards,SOURCE07_SHA))
    raw_copy=json.loads(snapshot(raw_copy_path,guards,RAW_COPY_SHA))
    for va in (0x18c6d,0x18795,0x17d6f,0x187d6,0x1875d,0x1685c,0x4ed0b):
        matches=[f for f in (raw_copy if va==0x4ed0b else source)['functions'] if f['va']==va];core.require(len(matches)==1,'one complete source owner')
        covered=set();heads=set()
        for c in matches[0]['chunks']:
            b=bytes.fromhex(c.get('bytes',c.get('raw_hex')));core.require(image[c['start']:c['end']]==b,'strict source chunk')
            covered.update(range(c['start'],c['end']))
        for i in matches[0]['instructions']:
            start=i.get('va',i.get('ea'));heads.update(range(start,start+i['size']))
        core.require(covered==heads,'complete summary instruction coverage')

def producer(path,root,guards):
    receipt=json.loads(snapshot(path,guards))
    core.require(receipt['schema']=='fd2-f4-text-producer-v1' and Path(receipt['source_root']).resolve()==root,'producer schema/root')
    core.require(REQUIRED_SOURCES<=set(receipt['sources']),'producer missing direct source dependencies')
    for relative,digest in receipt['sources'].items():
        p=(root/relative).resolve();core.require(p.is_relative_to(root),'producer source escaped root');snapshot(p,guards,digest)
    roles=set();seen=set()
    for item in receipt['files']:
        p=Path(item['path']).resolve();core.require(str(p) not in seen,'duplicate producer path');seen.add(str(p))
        snapshot(p,guards,item['sha256']);f=guards[str(p)]
        core.require((f['size'],f['mtime_ns'])==(item['size'],item['mtime_ns']),'producer exact identity');roles.add(item['role'])
    core.require({'build_log','tlog','static_library','producer_source','executable'}<=roles,'producer compilation closure')
    executables={}
    for name in ('prepare','cinematic'):
        item=receipt['executables'][name];p=Path(item['path']).resolve()
        core.require(str(p) in seen and guards[str(p)]['sha256']==item['sha256'],'binary must be in qualified producer files');executables[name]=p
    return executables

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT);parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--output-root',type=Path,required=True)
    parser.add_argument('--glyph-report',type=Path,required=True);parser.add_argument('--ui-report',type=Path,required=True)
    parser.add_argument('--source07',type=Path,required=True);parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--raw-copy-source',type=Path,required=True)
    parser.add_argument('--producer-receipt',type=Path);parser.add_argument('--plan-only',action='store_true')
    args=parser.parse_args();root=args.root.resolve();out=core.checked_output(root,args.out,args.output_root);out.mkdir()
    guards=dict(BOOT);probes={};failed=False
    report={'schema':'fd2-f4-text-composition-C-v1','ok':False,'actual_C_cases':0,'original_cases_this_run':0,
        'binding_rows':1180,'unique_parameter_requests':1179,'plan_only':args.plan_only,'cases':[],
        'original_game_executed':False,'GUI_or_build_executed':False,
        'mask_qualification':'target-independent value writes; whole bridge copies of unchanged before are not original/C physical store counts',
        'scope':'Finite source-derived current C typed literal/summary-font/credits bridge. No natural all-text, protocol, timing, PCM, arbitrary credits producer or signed bar full-domain claim.'}
    try:
        snapshot(Path(__file__),guards);resources={}
        for name,digest in [('FDOTHER.DAT',audit.OTHER_SHA),('FDTXT.DAT',audit.TEXT_SHA)]:resources[name]=snapshot(root/'original_game'/name,guards,digest)
        image=snapshot(args.image,guards,core.IMAGE_SHA);source_contract(args.source07,args.raw_copy_source,image,guards)
        texts=[audit.text_fragments(x) for x in audit.archive_entries(resources['FDTXT.DAT'])]
        oracle=Original(args.glyph_report.resolve(),args.ui_report.resolve(),resources,guards);work=cases(texts)
        if not args.plan_only:
            core.require(args.producer_receipt is not None,'current qualified build receipt required before C launch')
            executables=producer(args.producer_receipt.resolve(),root,guards)
            for role in ('prepare','cinematic'):probes[role]=core.Probe(executables[role],root,out,role,30,resources,mode='--f4-text',max_page_bytes=224000)
        with (out/'cases.jsonl').open('x',encoding='utf-8') as log:
            for index,case in enumerate(work):
                n=case['stride']*case['height'];page=seed(n,case['stride']);mask=bytearray(n)
                tokens=texts[case['entry']][case['fragment']]
                if case['kind']=='summary':oracle.summary(page,mask,tokens)
                else:oracle.literal(page,mask,tokens,121*320+40 if case['kind']=='plain' else case['offset'],case['stride'],case.get('pitch',0),201+4*case['request'][3] if case['kind']=='plain' else 205)
                expected=payload(page,mask);item={**case,'index':index,'expected_sha256':sha(expected),'actual_C_executed':False}
                if not args.plan_only:
                    header,actual=probes[case['request'][0]].request(case);core.validate_actual(header,actual)
                    difference=core.mismatch(expected,actual,n);item.update(actual_C_executed=True,actual_sha256=sha(actual),match=difference is None,difference=difference)
                    report['actual_C_cases']+=1
                    if difference:
                        for kind,blob in [('expected',expected),('actual',actual)]:
                            with (out/f'{kind}-{index:04d}.zbin').open('xb') as stream:stream.write(zlib.compress(blob,9))
                    if difference:
                        log.write(json.dumps(item,ensure_ascii=False)+'\n');log.flush();report['cases'].append(item)
                        raise ValueError(f'current C text mismatch {index}/{case["request"]}: {difference}')
                log.write(json.dumps(item,ensure_ascii=False)+'\n');log.flush();report['cases'].append(item)
        report.update(ok=True,retained_original_glyphs_used=len(oracle.glyphs),retained_UI_leaves_used=sorted(oracle.images))
    except BaseException:
        failed=True;report['error']=traceback.format_exc();core.write_diagnostic(out/'failure.txt',report['error'],report,'first error')
    finally:
        for role,probe in probes.items():
            try:probe.close(failed)
            except BaseException:core.secondary_error(report,'owned probe cleanup '+role,traceback.format_exc())
        report['C_loaded_input_bindings']={r:p.bound_inputs for r,p in probes.items()}
        core.finish_report(out,report,guards)
    print(json.dumps({k:report.get(k) for k in ('ok','binding_rows','unique_parameter_requests','actual_C_cases','guards_unchanged','error','secondary_errors')}))
    return 0 if report['ok'] else 1
if __name__=='__main__':raise SystemExit(main())
