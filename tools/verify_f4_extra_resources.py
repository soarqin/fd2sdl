"""F4 extra spans and source-derived byte readers; reuse the existing probe owner.

No decoder implementation, original CPU, build or GUI. plan-only scans fixed
resources. Optional actual mode executes only the explicitly derived raw/LUT/pal
cases, never promotes image rows whose independent original oracle is missing.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import traceback
import zlib

EXTRA = {
    'BG.DAT':'b9fc21d019d6256a4bb7e6da1cefcb0bfe331d8ff74a52a8201570afc98b56de',
    'TAI.DAT':'d56fea9c43f8bb59aad89ad76698885d7e07f380d12a4547888a0b60ea5e0410',
    'TITLE.DAT':'06120a4605e054f2ff1e08f882661b9b345b39978aabf5126ec137e1207815b3',
}
RAW_COPY_SHA='537645c5a91e3dca833f1b36d8af0658afbafd3759241b736f53f5f57e6c90bf'
CONTROL_SHA='18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517'
RAW_UI=set(range(20))|set(range(23,31))|set(range(53,58))|set(range(59,68))|{92}
RLE_UI=set(range(31,53))|{93}|set(range(119,137))
LMI_UI={20,21,22}|set(range(68,92))|set(range(94,119))
PALETTES={0,8,57,76,99,101,102}


def module(path, name, raw):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);exec(compile(raw,str(path),'exec'),value.__dict__)
    return value


def snapshot(path, guards, expected=None):
    before=path.stat();raw=path.read_bytes();after=path.stat()
    digest=hashlib.sha256(raw).hexdigest()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns) or (expected and digest!=expected):
        raise ValueError(f'输入身份改变：{path}')
    guards[str(path)]={'path':str(path),'sha256':digest,'size':len(raw),'mtime_ns':before.st_mtime_ns}
    return raw


def directory(data, count, start=6):
    table_end=start+count*4
    if not count or table_end>len(data):raise ValueError('目录越界')
    offsets=struct.unpack_from(f'<{count}I',data,start)
    if offsets[0]<table_end or any(a>=b for a,b in zip(offsets,offsets[1:])) or offsets[-1]>=len(data):
        raise ValueError('目录非递增或越owner')
    return [(a,data[a:b]) for a,b in zip(offsets,(*offsets[1:],len(data)))]


def collect(resources, archive_entries):
    entries={}
    for name,data in resources.items():
        if name not in ('FDOTHER.DAT',*EXTRA):continue
        blocks=archive_entries(data);at=struct.unpack_from('<I',data,6)[0]
        entries[name]=[]
        for block in blocks:
            entries[name].append((at,block));at+=len(block)
        if at!=len(data):raise ValueError('archive owner tail')
    rows=[];seen={}
    def add(file, offset, block, family, request, binding, codec, derived=None):
        key=(file,offset,offset+len(block))
        if key in seen:
            seen[key]['bindings'].append(binding);return seen[key]
        row={'key':f'{file}:{offset}:{offset+len(block)}','file':file,'start':offset,'end':offset+len(block),
             'sha256':hashlib.sha256(block).hexdigest(),'family':family,'codec':codec,
             'request':request,'bindings':[binding],'binding_status':('reader_available_index_producer_join_pending' if family in ('bg','tai') else 'historical_leaf_current_binding_unproven' if family=='title' else 'codec_and_binding_unqualified' if codec=='unknown_ui' else 'named_reader_family_parameter'),
             'oracle':'source-derived byte contract' if derived else 'independent original payload required'}
        if len(block)>=4 and codec in ('rle','lmi','raw_image','unknown_ui'):
            row['width'],row['height']=struct.unpack_from('<HH',block)
        if derived:row['derived']=derived
        rows.append(row);seen[key]=row;return row
    for name,op in [('BG.DAT','bg'),('TAI.DAT','tai'),('TITLE.DAT','title')]:
        for index,(at,block) in enumerate(entries[name]):
            add(name,at,block,op,['scene',op,index,0,0],
                'battle_animation image_entry/BG-TAI' if op!='title' else 'historical TITLE7 leaf; current normal binding not claimed','rle')
    other=entries['FDOTHER.DAT']
    at,block=other[1];width,height,count=struct.unpack_from('<HHH',block)
    if (width,height,count)!=(24,24,20):raise ValueError('cursor header')
    for frame,(offset,data) in enumerate(directory(block,count)):
        add('FDOTHER.DAT',at+offset,data,'cursor',['scene','cursor',frame,0,0],
            'field_visual.c load_cursor_frames -> shape reader; actual draw is a separate compositor','rle_cursor')
    at,block=other[3]
    if block[:4]!=b'LMI1' or struct.unpack_from('<H',block,4)[0]!=23:raise ValueError('LUT directory')
    for frame,(offset,data) in enumerate(directory(block,23)):
        if len(data)!=256:raise ValueError('LUT payload')
        add('FDOTHER.DAT',at+offset,data,'lut',['scene','lut',frame,0,0],
            'field_visuals_open -> field_lut_frame; no LUT application/RMW mask claim','raw_lut',{'kind':'lut'})
    at,block=other[5];count=struct.unpack_from('<H',block,4)[0]
    if block[:4]!=b'LMI1' or count!=138:raise ValueError('mixed UI directory')
    for frame,(offset,data) in enumerate(directory(block,count)):
        codec='raw_image' if frame in RAW_UI else 'rle' if frame in RLE_UI else 'lmi' if frame in LMI_UI else 'unknown_ui'
        op={'raw_image':'uiraw','rle':'uirle','lmi':'uilmi'}.get(codec)
        derive={'kind':'ui_raw'} if codec=='raw_image' else None
        if derive:
            width,height=struct.unpack_from('<HH',data)
            if not width or not height or len(data)!=4+width*height or width+4>320 or height+4>200:
                raise ValueError(f'known raw UI extent {frame}')
        add('FDOTHER.DAT',at+offset,data,'ui',['scene',op,frame,0,0] if op else None,
            'Scene ui_sheet / field_info mixed reader / defeat / map FX; frame-specific raw/RLE/LMI is not inferred from LMI1',codec,derive)
    for frame in PALETTES:
        at,data=other[frame]
        if len(data)!=768 or max(data)>63:raise ValueError('raw6 palette domain')
        add('FDOTHER.DAT',at,data,'palette',['scene','palette',frame,0,0],
            'main res_load_palette / Game base0 / cinema palette_load57 -> VGA setter','raw_palette',{'kind':'palette'})
    for entry,stage in [(15,9),(55,28)]:
        at,data=other[entry]
        if len(data)!=64004 or struct.unpack_from('<HH',data)!=(320,200):raise ValueError('raw wave extent')
        add('FDOTHER.DAT',at,data,'wave_raw',['scene','waveraw',stage,0,0],
            '10652 kind1 -> field_background_rebuild source owner; no wave transform/clock claim','raw_image',{'kind':'wave_raw'})
    for entry in (16,17,35,36,40,41,42,46,47):
        at,data=other[entry]
        add('FDOTHER.DAT',at,data,'background_rle',['scene','other',entry,0,0],
            '10652 kind2 half-image/kind3 scroll -> background copy_image','rle')
    for entry in (69,70,71,72,73,74,75,100):
        at,data=other[entry]
        add('FDOTHER.DAT',at,data,'intro_rle',['scene','other',entry,0,0],
            'main res_load_image scroll/intro/cutaway','rle')
    at,data=other[7];nested=archive_entries(data);offset=struct.unpack_from('<I',data,6)[0]
    if len(nested)!=7:raise ValueError('title nested menu directory')
    for frame,block in enumerate(nested):
        add('FDOTHER.DAT',at+offset,block,'title_nested',['scene','nested',frame,0,0],
            'main res_load_nested_image / ar7b -> image_decode_entry','rle');offset+=len(block)
    classifications=[]
    known_images={1,5,7,15,16,17,35,36,40,41,42,46,47,55,69,70,71,72,73,74,75,100}
    known_fx={18,19,20,21,22,23,24,25,26,27,28,30,32,33,37,38,39,43,44,34,45,54,58,65,66,67,68}
    cin_images={56,59,60}
    known_audio={31,77,78,*range(80,96)}
    for index,(at,data) in enumerate(other):
        role='palette' if index in PALETTES else 'LUT' if index==3 else 'font_bitmap' if index==4 else 'raw_command_image_container_78' if index==2 else 'map_FX_LMI_directory_230_selected_reader_only' if index==6 else 'FIGANI_style_FX_or_scene_sheet' if index in known_fx else 'cinema_RLE_image' if index in cin_images else 'audio_bank' if index in known_audio else 'image/raw/mixed_container' if index in known_images else 'structural_LMI_container_consumer_unqualified' if data[:4]==b'LMI1' else 'nested_container_content_unqualified' if data[:6]==b'LLLLLL' else 'unclassified'
        classifications.append({'entry':index,'start':at,'end':at+len(data),'sha256':hashlib.sha256(data).hexdigest(),
            'role':role,'signature':data[:6].hex(),'scope':'semantic role only for named consumer evidence; unknown is not metadata/image by guess'})
    return rows,classifications


def derived_payload(row, resources):
    data=resources[row['file']][row['start']:row['end']]
    kind=row['derived']['kind'];palette=bytes([165])*768;pmask=bytes(768)
    if kind=='lut':
        return data+palette+bytes([1])*256+pmask,256,1,'returned LUT byte owner only; no original RMW/mask comparison'
    if kind=='palette':
        page=bytes((i*17+i//320+83)&255 for i in range(64000))
        return page+data+bytes(64000)+bytes([1])*768,320,200,'immutable loaded raw6 palette bytes via C setter; original DAC/write-mask not qualified'
    if kind=='wave_raw':
        return data[4:]+palette+bytes([1])*64000+pmask,320,200,'10652 immutable raw source bytes; source-copy mask is not original pixel-write mask'
    if kind=='ui_raw':
        width,height=struct.unpack_from('<HH',data);page=bytearray((i*17+i//320+83)&255 for i in range(64000));mask=bytearray(64000)
        for y in range(height):
            target=(y+4)*320+4;source=4+y*width
            page[target:target+width]=data[source:source+width];mask[target:target+width]=bytes([1])*width
        return bytes(page)+palette+bytes(mask)+pmask,320,200,'strict retained1685c->4ed0b opaque row copy; explicit zeros write; mathematically derived, not CPU'
    raise ValueError('unqualified derived kind')


def validate_control(core, image, raw):
    if hashlib.sha256(raw).hexdigest()!=CONTROL_SHA:raise ValueError('control source identity')
    source=json.loads(raw)
    for va in (0x10652,0x1685c):
        fn=next(f for f in source['functions'] if f['va']==va)
        for chunk in fn['chunks']:
            data=bytes.fromhex(chunk['bytes'])
            core.require(image[chunk['start']:chunk['end']]==data and core.sha(data)==chunk['sha256'],'control strict chunk')
    return {'source_sha256':CONTROL_SHA,'functions':[hex(x) for x in (0x10652,0x1685c)],
            'qualification':'inherited complete source with chunk identity; no new original execution; LUT check is immutable payload retrieval only'}


def validate_raw_copy(core, image, raw):
    core.require(core.sha(raw)==RAW_COPY_SHA,'raw-copy source identity')
    source=json.loads(raw)
    core.require(source['image_sha256']==core.IMAGE_SHA,'raw-copy strict image identity')
    functions=[f for f in source['functions'] if f['va']==0x4ed0b]
    core.require(len(functions)==1,'raw-copy unique owner')
    function=functions[0];covered=set();decoded=set()
    core.require([(c['start'],c['end']) for c in function['chunks']]==[(0x4ed0b,0x4ed34)],'complete41B raw-copy owner')
    for chunk in function['chunks']:
        begin,end=chunk['start'],chunk['end'];data=bytes.fromhex(chunk['raw_hex'])
        core.require(len(data)==end-begin and image[begin:end]==data and core.sha(data)==chunk['sha256'],'raw-copy chunk identity')
        covered.update(range(begin,end))
    for instruction in function['instructions']:
        begin,size=instruction['ea'],instruction['size'];data=bytes.fromhex(instruction['raw_hex'])
        core.require(len(data)==size and image[begin:begin+size]==data,'raw-copy exact instruction')
        decoded.update(range(begin,begin+size))
    core.require(covered==decoded and len(covered)==41,'raw-copy full instruction coverage')
    return {'source_sha256':RAW_COPY_SHA,'VA':'0x4ed0b..0x4ed34','instructions':len(function['instructions']),
            'qualification':'complete retained strict chunks/instructions; source-derived row-copy math, not new CPU'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--output-root',type=Path,help='optional declared output root; actual/big payloads should use an owned F: directory')
    parser.add_argument('--plan-only',action='store_true');parser.add_argument('--build-receipt',type=Path)
    parser.add_argument('--image',type=Path,default=Path('E:/Projects/fd2sdl/tools/fd2_le_relocated_relbase.bin'))
    parser.add_argument('--control-source',type=Path)
    parser.add_argument('--raw-copy-source',type=Path,default=Path('F:/fd2sdl-evidence/s5-20261114/notice-source01/domain05.json'))
    args=parser.parse_args();root=args.root.resolve()
    guards={}
    snapshot(Path(__file__).resolve(),guards)
    core_path=root/'tools/verify_f4_resource_pixels.py';helper_path=root/'tools/audit_g4_render_source.py'
    core=module(core_path,'fd2_extra_core',snapshot(core_path,guards))
    helper=module(helper_path,'fd2_extra_archive',snapshot(helper_path,guards))
    out=core.checked_output(root,args.out,args.output_root);core.require(args.plan_only or args.build_receipt,'build receipt required')
    resources={};report={'schema':'fd2-f4-extra-reader-v1','ok':False,'plan_only':args.plan_only,'actual_C_cases_executed':0,'original_CPU_executed':False,'decoder_executed_by_scan':False}
    probes={};failure=None;out.mkdir(parents=True)
    try:
        for name,expected in {**core.RESOURCES,**EXTRA}.items():resources[name]=snapshot(root/'original_game'/name,guards,expected)
        snapshot(root/'original_game/FD2.EXE',guards,core.EXE_SHA)
        for name in ('src/field_command.c','src/field_info.c','src/field_visual.c','src/field_background.c','src/battle_animation.c','src/main.c','data/static/flows/stage.json','data/static/tables/magic_fx.json'):
            snapshot(root/name,guards)
        image=snapshot(args.image.resolve(),guards,core.IMAGE_SHA)
        control=args.control_source.resolve() if args.control_source else root/'build/f1/domain-boundaries/source09.json'
        report['control_source']=validate_control(core,image,snapshot(control,guards,CONTROL_SHA))
        report['raw_copy_source']=validate_raw_copy(core,image,snapshot(args.raw_copy_source.resolve(),guards,RAW_COPY_SHA))
        rows,roles=collect(resources,helper.archive_entries)
        report.update({'unique_spans':len(rows),'families':dict(Counter(r['family'] for r in rows)),
            'UI_directory_frames':138,'UI_existing_C_required_prefix':137,'UI_unclassified_frames':[58,137],'byte_only_cases':32,'raw_UI_full_mask_cases':43,
            'source_derived_cases':sum('derived' in r for r in rows),'new_LUT_application_cases':0,'LUT_application_qualification':'candidate05/root source46 + 20 extra = 66 original cases; inherited, not run or promoted here','independent_original_payload_required':sum('derived' not in r for r in rows),
            'out_of_scope_known_consumers':{'FDOTHER2':'78 raw command frames; reader field_command_assets_open; not part of the333 or planned C cases','FDOTHER6':'230 frame LMI container; map physical/magic actual lmi_image reader; not part of the333'},
            'raw_UI_oracle_source_level':'strict complete4ed0b chunk/all21 instructions + strict1685c wrapper; mathematical row-copy, no new CPU',
            'limits':['No pixel/RLE/LMI decoder runs in plan-only.','Derived LUT/wave/palette compare returned byte values only; masks are protocol/C-copy observations, not original drawing masks.','Raw/palette mathematically derived expected is not original CPU.','Image CLI connections are not original payloads or C passes.','Old 14711/18637 resource scope is separate; no new functional rate.']})
        (out/'catalog.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        (out/'FDOTHER-entry-roles.json').write_text(json.dumps(roles,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        if args.build_receipt:
            snapshot(args.build_receipt.resolve(),guards)
        if not args.plan_only:
            core.BUILD_SOURCES|={'src/field_background.c','src/field_background.h','src/field_visual.h','src/field_visual.c','src/field_page_private.h','data/static/tables/field_background.json','data/static/manifest.json','tools/static_data/generate.py'}
            binaries=core.checked_build(args.build_receipt.resolve(),root,guards)
            expected_inputs={name:data for name,data in resources.items() if name!='ANI.DAT'}
            probes['scene']=core.Probe(binaries['scene'],root,out,'scene-extra',30,expected_inputs)
        with (out/'derived-cases.jsonl').open('x',encoding='utf-8') as log:
            for row in rows:
                if 'derived' not in row:continue
                expected,stride,height,scope=derived_payload(row,resources)
                item={'asset':row['key'],'request':row['request'],'oracle':'source-derived (not CPU)','scope':scope,'expected_sha256':core.sha(expected),'actual_C_executed':False}
                if not args.plan_only:
                    header,actual=probes['scene'].request({'request':row['request'],'stride':stride,'height':height})
                    core.validate_actual(header,actual);report['actual_C_cases_executed']+=1
                    n=stride*height
                    compare_masks=row['derived']['kind']=='ui_raw'
                    compared=actual if compare_masks else actual[:n+768]+expected[n+768:]
                    difference=core.mismatch(expected,compared,n)
                    item.update({'original_write_masks_compared':compare_masks,'actual_page_mask_sha256':core.sha(actual[n+768:2*n+768]),'actual_palette_mask_sha256':core.sha(actual[2*n+768:])})
                    item.update({'actual_C_executed':True,'actual_sha256':core.sha(actual),'match':difference is None,'difference':difference})
                    name=f"actual-{report['actual_C_cases_executed']:05d}.zbin"
                    with (out/name).open('xb') as file:file.write(zlib.compress(actual,9))
                log.write(json.dumps(item,ensure_ascii=False)+'\n');log.flush()
                core.require(item.get('match',True),f'extra reader mismatch {item}')
        report['ok']=True
    except BaseException:
        failure=traceback.format_exc();report['error']=failure
        core.write_diagnostic(out/'failure.txt',failure,report,'failure log')
    finally:
        for name,probe in probes.items():
            try:probe.close(failure is not None)
            except BaseException:core.secondary_error(report,f'cleanup {name}',traceback.format_exc())
        core.finish_report(out,report,guards)
    print(json.dumps({k:report.get(k) for k in ('ok','plan_only','unique_spans','source_derived_cases','actual_C_cases_executed','guards_unchanged','error')},ensure_ascii=False))
    return 0 if report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
