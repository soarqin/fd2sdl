"""New FDOTHER spans: independent fixed inputs and finite original/C readers.

No GUI/IDA/build/resource writes. Original kernel helper is extracted from the
existing qualified harness; no decoder/model/VM is introduced. Normal caller,
page history, timing/RNG/audio qualification remains separate.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import sys
import traceback
import zlib
import f4_original_pixels as original
import verify_f4_resource_pixels as core
import verify_f4_extra_resources as extra
from audit_g4_render_source import archive_entries

OTHER_SHA=core.RESOURCES['FDOTHER.DAT']
FX=(18,19,20,21,22,23,24,25,26,27,28,30,32,33,34,37,38,39,43,44,45,54,58,65,66,67,68,79)
DIRECT=(10,11,56,59,60,61,62,97,98)
COUNTS={'command_raw':78,'map_lmi':230,'arrival_lmi':12,'direct_image':9,'prepare_mixed':142,'other_figani':701,'challenge_shape':84}

def prepare_format(entry,frame):
    if entry==13:
        return 0 if frame==0 else 1 if frame in (11,12,13,14,16,17) else 2 if 3<=frame<=10 else None
    count={12:29,14:32,29:24,63:31}.get(entry,0)
    if not 0<=frame<count or (entry==14 and 18<=frame<=22):return None
    return 0 if frame==0 or frame>=23 else 4 if frame==2 else 2 if 3<=frame<=10 else 3 if frame==15 else 1

def collect(resource):
    blocks=archive_entries(resource);base=original.u32(resource,6);entries=[]
    for block in blocks:entries.append((base,block));base+=len(block)
    core.require(len(blocks)==103 and base==len(resource),'complete fixed FDOTHER archive')
    rows=[]
    def frames(entry,count,start):return extra.directory(blocks[entry],count,start)
    def add(entry,frame,offset,blob,family,codec,**extra_fields):
        begin=entries[entry][0]+offset
        rows.append({'id':f'OTHER-{entry:03d}-{frame:03d}','file':'FDOTHER.DAT','entry':entry,'frame':frame,
            'start':begin,'end':begin+len(blob),'source_sha256':core.sha(blob),'family':family,'codec':codec,
            'width':original.u16(blob),'height':original.u16(blob,2),**extra_fields})
    for i,(off,blob) in enumerate(frames(2,78,0)):
        core.require(len(blob)==4+original.u16(blob)*original.u16(blob,2),'raw command exact extent')
        add(2,i,off,blob,'command_raw','raw_zero',source_reader='1741c/176b4/179d5/19953 ->4ed34+4ed4f',
            C_reader='field_command_assets_open.frames + fd2_map_sprite_blit(0)',binding='menu frame producer; typed uniform reader',request=['battle','command',i,0,0])
    for entry,count,family in ((6,230,'map_lmi'),(9,12,'arrival_lmi')):
        core.require(blocks[entry][:4]==b'LMI1' and original.u16(blocks[entry],4)==count,'LMI explicit count')
        for i,(off,blob) in enumerate(frames(entry,count,6)):
            unknown=entry==6 and i==125
            add(entry,i,off,blob,family,'unknown_raw_sized' if unknown else 'lmi',source_reader='4ebff full decode;4ebab zero-transparent draw;4ec66 reader; frame125 lacks typed caller',
                C_reader='battle_animation.c::lmi_image / battle_page_image',binding='map6 known frame union229;125 remains normal producer inquiry' if entry==6 else '32999 arrival loop12',
                request=None if unknown else ['battle','maplmi',entry,i,0])
    for entry in DIRECT:
        add(entry,0,0,blocks[entry],'direct_image','lmi' if entry==10 else 'rle',
            source_reader='26152 table[11,61,62]/10;31529 59/60;31c49 56;33faf 97/98',
            C_reader='image_entry shared RLE reader / fd2_lmi_decode_image',
            binding='source-unreferenced fixed117e7 bypassed challenge' if entry in (97,98) else 'formal direct image loader',
            request=None if entry in (97,98) else ['battle','direct',entry,0,0])
    for entry in (12,13,14,29,63):
        count=len(archive_entries(blocks[entry])) if blocks[entry][:6]==b'LLLLLL' else original.u16(blocks[entry],4)
        for i,(off,blob) in enumerate(frames(entry,count,6)):
            fmt=prepare_format(entry,i)
            structural='lmi' if entry==13 and i>=23 else 'rle' if i==0 or i>=23 else 'raw_indexed' if i==2 or 3<=i<=10 or i==15 else 'lmi'
            codec='rle' if fmt==0 else 'lmi' if fmt==1 else 'raw_copy' if fmt in (2,3,4) else structural
            binding=('original hotel arrow formal; C binding missing before media candidate' if entry==13 and i in (11,12,13,14,17)
                else 'no current typed consumer; exact original caller domain not promoted' if fmt is None else 'formal typed prepare reader')
            add(entry,i,off,blob,'prepare_mixed',codec,format=fmt,source_reader='26ce4/26eda raw0;27079 LMI arrows;279bc/29daa background/money;2825b labels;28b41 animation',
                C_reader='hotel_frame / service_frame',binding=binding,request=None if fmt is None else ['prepare','prepare',entry,i,0])
    for entry in FX:
        for i,(off,part) in enumerate(frames(entry,blocks[entry][0],8)):
            add(entry,i,off+9,part[9:],'other_figani','rle',x=original.u16(part),y=original.u16(part,2),
                part_start=entries[entry][0]+off,part_end=entries[entry][0]+off+len(part),
                source_reader='2eb9f directory/part+9 ->4e98d; wrapper caller/marker/clock separate',
                C_reader='battle_animation.c::clip_open_visual ->image_open ->blit',
                binding='FDOTHER FX/Scene package; exact caller domain separate',request=['battle','otherfx',entry,i,0])
    block=blocks[96];w,h,count=struct.unpack_from('<3H',block)
    core.require((w,h,count)==(24,24,84),'challenge shape exact header')
    for i,(off,blob) in enumerate(frames(96,count,6)):
        add(96,i,off,blob,'challenge_shape','rle24',width=24,height=24,request=None,
            source_reader='33faf ->34366 challenge;117e7@118aa bypasses call118ac',
            C_reader=None,binding='source-unreferenced fixed normal entry CFG; not credits')
    core.require(len(rows)==1256 and len({(r['start'],r['end']) for r in rows})==1256,'physical denominator/unique spans')
    core.require(dict(Counter(r['family'] for r in rows))==COUNTS,'family denominator')
    return rows

def cases(rows):
    result=[]
    for row in rows:
        if row['request'] is None:continue
        result.append({'row':row,'request':row['request']})
        if row['family'] in ('map_lmi','arrival_lmi','other_figani'):
            request=list(row['request']);request[-1]=1;result.append({'row':row,'request':request})
    core.require(len(result)==2093,'fixed independent request denominator')
    return result

def collect_ui137(resource):
    blocks=archive_entries(resource);base=original.u32(resource,6)+sum(len(x) for x in blocks[:5])
    block=blocks[5];core.require(block[:4]==b'LMI1' and original.u16(block,4)==138,'full mixed UI directory')
    offset,blob=extra.directory(block,138,6)[137]
    core.require((original.u16(blob),original.u16(blob,2))==(86,86),'source2af28 image137 dimensions')
    original.rle_extent(blob[4:],86,86)
    return [{'id':'OTHER-005-137','file':'FDOTHER.DAT','entry':5,'frame':137,'start':base+offset,
        'end':base+offset+len(blob),'source_sha256':core.sha(blob),'family':'prepare_mixed','codec':'rle',
        'width':86,'height':86,'format':0,'request':['prepare','prepare',5,137,0],
        'source_reader':'2af28 source137 ->16886 blit_image ->4e98d',
        'C_reader':'field_prepare_service_pick.h prepare_service_selection_resources ->service_frame(5,137,0)',
        'binding':'formal deployment selection_label; actual caller placement5,7 distinct from returned copy'}]

def prescribe(resource,ui137=False):
    rows=collect_ui137(resource) if ui137 else collect(resource)
    return rows,[{'row':r,'request':r['request']} for r in rows] if ui137 else cases(rows)

def original_parameters(case,source):
    row=case['row'];variant=case['request'][-1];family=row['family'];w,h=row['width'],row['height']
    if row['codec']=='rle':original.rle_extent(source[4:],w,h)
    if row['codec']=='lmi':core.require(original.lmi_extent(source)==(w,h),'complete LMI source extent')
    if family=='command_raw':return 320,200,{0x4ed34,0x4ed4f},'raw',0x4ed34,(original.PAGE+4+4*320,original.INPUT,320),False
    if family in ('map_lmi','arrival_lmi') and variant:
        core.require(w+72<=456 and h+72<=336,'transparent map page domain')
        return 456,336,{0x4ebab,0x4ec66},'portrait',0x4ebab,(original.PAGE+72+72*456,original.INPUT,456),False
    if row['codec']=='lmi':return w,h,{0x4ebff,0x4ec66},'portrait',0x4ebff,(original.PAGE,original.INPUT,w),True
    if row['codec']=='raw_copy':
        core.require(len(source)==4+w*h,'raw exact source extent')
        return w,h,{0x4ed0b},'raw',0x4ed0b,(original.PAGE,original.INPUT,w),True
    core.require(row['codec']=='rle','explicit qualified codec')
    stride,height=(320,200) if family=='other_figani' and variant else (w,h)
    x,y=(row['x'],row['y']) if family=='other_figani' and variant else (0,0)
    core.require(x+w<=stride and y+h<=height,'normal RLE destination bound')
    return stride,height,{0x4e98d},'figani',0x4e98d,(original.INPUT,x,y,original.PAGE,stride,0xffffffff),family=='prepare_mixed'

def read_expected(path,digest,guards,resource,ui137=False):
    report=json.loads(extra.snapshot(path,guards,digest))
    count=1 if ui137 else 2093
    core.require(report['schema']=='fd2-f4-remainder-original-v1' and report['ok'] and report['guards_unchanged'] and
        report.get('scope_kind','remainder')==('ui137' if ui137 else 'remainder') and
        report['original_cases']==count and report['actual_C_cases']==0 and
        report['original_cases_executed']+report['inherited_original_cases']==count,
        'independent retained original report qualification')
    _,prescribed=prescribe(resource,ui137);core.require(len(report['cases'])==len(prescribed),'full expected count')
    for index,(expected,wanted) in enumerate(zip(report['cases'],prescribed)):
        core.require(expected['request']==wanted['request'] and expected['source_sha256']==wanted['row']['source_sha256'],'exact fixed input/request order')
        row=wanted['row'];source=resource[row['start']:row['end']]
        stride,height,owners,family,entry,argv,copy=original_parameters(wanted,source)
        core.require(expected['index']==index and expected['asset']==row['id'] and expected['stride']==stride and
            expected['height']==height and expected['owners']==sorted(owners) and expected['entry']==entry and
            expected['args']==list(argv) and expected['copy_mask_normalized']==copy,
            'retained original explicit source parameters')
        original_item=expected['original_payload'];original_name=original_item['path']
        core.require(Path(original_name).name==original_name and original_name.endswith('.zbin'),'bounded original payload path')
        packed=extra.snapshot(path.parent/original_name,guards,original_item['sha256'])
        raw=core.bounded_unpack(packed,expected['payload_bytes']);n=stride*height
        pixels,palette,mask,pmask=core.split_payload(raw,n)
        core.require(set(mask)<={0,1} and set(pmask)<={0,1} and sum(mask)==expected['original_pixel_addresses_written'],
            'retained observed original mask')
        normalized=pixels+palette+(bytes([1])*n if copy else mask)+pmask
        item=expected['comparison_payload'];name=item['path']
        core.require(Path(name).name==name and name.endswith('.zbin'),'bounded retained payload path')
        if name==original_name:
            core.require(item['sha256']==original_item['sha256'],'shared payload identity');comparison=packed
        else:comparison=extra.snapshot(path.parent/name,guards,item['sha256'])
        core.require(core.bounded_unpack(comparison,expected['payload_bytes'])==normalized,
            'comparison must be actual original pixels plus explicit copy mask normalization')
        wanted['packed']=comparison
        wanted['expected_size']=expected['payload_bytes'];wanted['stride']=expected['stride'];wanted['height']=expected['height']
        wanted['expected_packed']=wanted['packed']
    return prescribed

def actual_inputs(args,root,guards):
    core.require(args.battle and args.prepare and args.producer_receipt and args.expected_sha,
        'actual producer receipt/exact two binaries/expected hash required')
    receipt=json.loads(extra.snapshot(args.producer_receipt.resolve(),guards))
    core.require(receipt['schema']=='fd2-f4-remainder-producer-v1','complete producer receipt schema')
    roles=set()
    seen=set()
    for item in receipt['files']:
        path=Path(item['path']).resolve()
        core.require(str(path) not in seen,'duplicate producer path')
        seen.add(str(path));blob=extra.snapshot(path,guards,item['sha256'])
        core.require(guards[str(path)]['mtime_ns']==item['mtime_ns'] and len(blob)==item['size'],'producer file identity')
        roles.add(item['role'])
    core.require({'build_log','tlog','static_library','producer_source','executable'}<=roles,
        'producer build/source/tlog/library closure required')
    for name,path in (('battle',args.battle.resolve()),('prepare',args.prepare.resolve())):
        record=receipt['executables'][name]
        core.require(Path(record['path']).resolve()==path,'exact snapshot executable path')
        core.require(str(path) in seen and guards[str(path)]['sha256']==record['sha256'],
            'exact executable must be bound in producer files')
    return receipt

def inherit_prefix(path,digest,source_directory,out,work,resource,guards):
    old=json.loads(extra.snapshot(path,guards,digest))
    core.require(digest=='9d7cbc83506b63b07aa2235ccd5b058d4419cf5be06fc014d7b4ba2c3a58f2e9' and
        old['schema']=='fd2-f4-remainder-original-v1' and old['guards_unchanged'] and not old['ok'] and
        old['original_cases']==328 and old['actual_C_cases']==0 and len(old['cases'])==328,
        'only the retained qualified328 prefix is eligible; whole failed run never promoted')
    for name in ('verify_f4_remainder_resources.py','f4_original_pixels.py'):
        matches=[v for k,v in old['inputs_before'].items() if Path(k).name==name]
        core.require(len(matches)==1,'unique old generating source identity')
        extra.snapshot(source_directory/name,guards,matches[0]['sha256'])
    results=[]
    for index,(item,wanted) in enumerate(zip(old['cases'],work)):
        row=wanted['row'];source=resource[row['start']:row['end']]
        stride,height,owners,family,entry,argv,copy=original_parameters(wanted,source)
        core.require(item['index']==index and item['request']==wanted['request'] and
            item['source_sha256']==row['source_sha256'] and item['stride']==stride and item['height']==height and
            item['entry']==entry and item['owners']==sorted(owners) and item['args']==list(argv) and
            item['copy_mask_normalized']==copy and item['source_bytes_unchanged'] and
            item['write_extent_passed'] and item['hardware_IO_executed']==0,'inherited exact source/request/ABI contract')
        copied=set()
        for key in ('original_payload','comparison_payload'):
            file=item[key]['path'];core.require(Path(file).name==file and file.endswith('.zbin'),'inherited payload basename')
            if file not in copied:
                packed=extra.snapshot(path.parent/file,guards,item[key]['sha256'])
                core.bounded_unpack(packed,item['payload_bytes'])
                with (out/file).open('xb') as stream:stream.write(packed)
                copied.add(file)
        item={**item,'inherited_from':str(path),'inherited_report_sha256':digest};results.append(item)
    core.require(len(results)==328,'inherited prefix completeness')
    return results

def run_original(out,work,resource,image,functions,report):
    machine=None
    try:
        with (out/'cases.jsonl').open('x',encoding='utf8') as log:
            start=len(report['cases'])
            for index,case in enumerate(work[start:],start):
                row=case['row'];source=resource[row['start']:row['end']]
                core.require(core.sha(source)==row['source_sha256'],'same fixed loaded source span')
                stride,height,owners,family,entry,argv,copy=original_parameters(case,source)
                machine=original.OriginalPixels(image,functions,owners,source,row['width'],height,stride,83,family,
                    initial=bytes(stride*height) if copy else None)
                machine.call(entry,argv);pixels,palette=machine.payload()
                raw=pixels+palette+bytes(machine.mask)+bytes(machine.palette_mask)
                comparison=pixels+palette+(bytes([1])*len(pixels) if copy else bytes(machine.mask))+bytes(machine.palette_mask)
                file=f'original-{index:04d}.zbin';packed=zlib.compress(raw,9);(out/file).write_bytes(packed)
                cpfile=f'copy-{index:04d}.zbin' if copy else file;cppacked=zlib.compress(comparison,9) if copy else packed
                if copy:(out/cpfile).write_bytes(cppacked)
                item={'index':index,'asset':row['id'],'request':case['request'],'source_sha256':row['source_sha256'],
                    'stride':stride,'height':height,'payload_bytes':len(raw),'entry':entry,'owners':sorted(owners),'args':list(argv),
                    'initial_page':'explicit zero' if copy else '(offset*17+row+83)&255',
                    'original_write_mask_sha256':core.sha(machine.mask),'original_pixel_addresses_written':sum(machine.mask),
                    'copy_mask_normalized':copy,
                    'mask_qualification':'C returned copy full mask; actual original sparse mask retained separately' if copy else 'actual original write hook and C two initialized bottoms',
                    'original_payload':{'path':file,'sha256':core.sha(packed)},
                    'comparison_payload':{'path':cpfile,'sha256':core.sha(cppacked)},
                    'visited_heads':sorted(machine.visited),'hardware_IO_executed':0,
                    'source_bytes_unchanged':True,'write_extent_passed':True}
                machine.close();machine=None;report['cases'].append(item);report['original_cases']+=1
                report['original_cases_executed']+=1
                log.write(json.dumps(item)+'\n');log.flush()
                if index%100==0:print(json.dumps({'original_cases':index+1,'total':len(work),'asset':row['id']}),flush=True)
    finally:
        if machine:machine.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--output-root',type=Path,required=True)
    parser.add_argument('--image',type=Path);parser.add_argument('--expected',type=Path);parser.add_argument('--expected-sha')
    parser.add_argument('--battle',type=Path);parser.add_argument('--prepare',type=Path);parser.add_argument('--producer-receipt',type=Path)
    parser.add_argument('--plan-only',action='store_true')
    parser.add_argument('--ui137',action='store_true',help='one newly source-qualified old333 UI tail, not a rerun of old331')
    parser.add_argument('--inherit-prefix',type=Path);parser.add_argument('--inherit-sha')
    parser.add_argument('--inherit-source-directory',type=Path)
    args=parser.parse_args();root=args.root.resolve();out=core.checked_output(root,args.out,args.output_root)
    out.mkdir();guards={};probes={};failure=None
    actual=args.expected is not None
    report={'schema':'fd2-f4-remainder-C-v1' if actual else 'fd2-f4-remainder-original-v1','ok':False,
        'original_cases':0,'original_cases_executed':0,'inherited_original_cases':0,'actual_C_cases':0,'GUI_or_build_executed':False,
        'scope':'Finite explicit reader/page parameters; not complete binding/cold walker, natural gameplay, first allocation/history, timing/RNG/audio.',
        'scope_kind':'ui137' if args.ui137 else 'remainder',
        'cases':[]}
    try:
        for path in (Path(__file__),Path(original.__file__),Path(core.__file__),Path(extra.__file__),Path(archive_entries.__code__.co_filename)):
            extra.snapshot(path.resolve(),guards)
        resource=extra.snapshot(root/'original_game/FDOTHER.DAT',guards,OTHER_SHA)
        extra.snapshot(root/'original_game/FD2.EXE',guards,core.EXE_SHA)
        rows,work=prescribe(resource,args.ui137)
        report.update({'physical_spans':len(rows),'families':dict(Counter(r['family'] for r in rows)),
            'C_probe_span_count':sum(r['request'] is not None for r in rows),'request_count':len(work),
            'unexecuted_span_count':sum(r['request'] is None for r in rows),
            'unexecuted_limits':('UI58 not qualified by this single UI137 supplement' if args.ui137 else
                '86 fixed-entry bypassed challenge;13 hotel/5 church no typed consumer;map6/125 unknown raw-sized43B not LMI. No arbitrary codec promotion.')})
        (out/'catalog.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        if args.plan_only:report['ok']=True
        elif actual:
            actual_inputs(args,root,guards)
            work=read_expected(args.expected.resolve(),args.expected_sha,guards,resource,args.ui137)
            for name,path in (('battle',args.battle),('prepare',args.prepare)):
                if any(c['request'][0]==name for c in work):probes[name]=core.Probe(path.resolve(),root,out,name,30,{'FDOTHER.DAT':resource})
            for index,case in enumerate(work):
                name=case['request'][0];header,raw=probes[name].request(case);core.validate_actual(header,raw)
                expected=core.bounded_unpack(case['packed'],case['expected_size'])
                diff=core.mismatch(expected,raw,case['stride']*case['height'])
                packet=zlib.compress(raw,9);file=f'actual-{index:04d}.zbin';(out/file).write_bytes(packet)
                report['cases'].append({'index':index,'request':case['request'],'asset':case['row']['id'],
                    'match':diff is None,'difference':diff,'actual_payload':{'path':file,'sha256':core.sha(packet)}})
                report['actual_C_cases']+=1;core.require(diff is None,f'actual C difference {index} {case["request"]}: {diff}')
            report['ok']=True
        else:
            core.require(args.image is not None,'strict --image required for original')
            core.require(sys.flags.optimize==0,'original guards require assertions enabled')
            import unicorn,unicorn.x86_const as regs
            core.require(unicorn.__version__=='2.1.4','qualified Unicorn version2.1.4')
            for path in (Path(sys.executable),Path(unicorn.__file__),Path(regs.__file__),Path(unicorn.__file__).parent/'lib/unicorn.dll'):
                extra.snapshot(path,guards)
            report['runtime']={'python':sys.version,'executable':sys.executable,'unicorn':unicorn.__version__}
            image=extra.snapshot(args.image.resolve(),guards,core.IMAGE_SHA)
            manifest=Path(original.__file__).parent/'fixtures/f4_pixel_kernels.json'
            functions=original.validate_manifest(image,extra.snapshot(manifest,guards,original.MANIFEST_SHA))
            if args.inherit_prefix:
                core.require(not args.ui137,'UI137 cannot inherit unrelated remainder prefix')
                core.require(args.inherit_sha and args.inherit_source_directory,'inherit prefix identity/source archive required')
                report['cases']=inherit_prefix(args.inherit_prefix.resolve(),args.inherit_sha,
                    args.inherit_source_directory.resolve(),out,work,resource,guards)
                report['inherited_original_cases']=report['original_cases']=len(report['cases'])
            run_original(out,work,resource,image,functions,report);report['ok']=True
    except BaseException:
        failure=traceback.format_exc();report['error']=failure;core.write_diagnostic(out/'failure.txt',failure,report,'first error')
    finally:
        for name,probe in probes.items():
            try:probe.close(failure is not None)
            except BaseException:core.secondary_error(report,'owned '+name+' cleanup',traceback.format_exc())
        report['C_loaded_input_bindings']={name:p.bound_inputs for name,p in probes.items()}
        core.finish_report(out,report,guards)
    print(json.dumps({k:report.get(k) for k in ('ok','physical_spans','C_probe_span_count','request_count','original_cases','actual_C_cases','guards_unchanged','error')}))
    return 0 if report['ok'] else 1
if __name__=='__main__':raise SystemExit(main())
