"""Reproduce F4 pure pixel payloads from fixed user resources and strict image.

Shared source-qualified kernel is f4_original_pixels; no private ignored harness,
Domain JSON, golden output, C decoder or GUI is needed. --scope base executes all
289 ANI frames (18759 states), including the122 formerly inherited in the old
18637 receipt. That is a new requested regeneration, not a rewrite of that old
receipt. --scope extra is the256 existing typed extra images, not75 derived bytes.
This repository entry is supplied for reproduction only; adding it does not rerun
historical original/C batches or prove their current binary equivalent.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import sys
import traceback
import zlib
import f4_original_pixels as kernel
import verify_f4_resource_pixels as core
import verify_f4_extra_resources as extra
from audit_g4_render_source import archive_entries

sha,u16,u32,rle_extent,lmi_extent=kernel.sha,kernel.u16,kernel.u32,kernel.rle_extent,kernel.lmi_extent

def archive(data):
    at=u32(data,6);result=[]
    for block in archive_entries(data):result.append((at,block));at+=len(block)
    core.require(at==len(data),'complete archive owner')
    return result

def offsets(data,table,count,directory_end):
    values=[u32(data,table+4*i) for i in range(count)]
    core.require(values and values[0]>=directory_end and values[-1]<len(data) and
        all(a<b for a,b in zip(values,values[1:])),'finite source directory')
    return list(zip(values,values[1:]+[len(data)]))

def catalog_base(resources):
    entries = {name: archive(data) for name, data in resources.items() if name != "FDICON.B24"}
    rows, blobs, counts = [], {}, Counter()

    def add(key, family, file, begin, end, width, height, **extra):
        data = resources[file][begin:end]
        assert len(data) == end-begin
        row = {"key": key, "family": family, "file": file, "start": begin,
               "end": end, "source_sha256": sha(data), "width": width, "height": height,
               "reader_scope": "format/extent verified, not original file-I/O or natural binding", **extra}
        rows.append(row)
        blobs[key] = data
        counts[family] += 1

    # Terrain header/frame directory is FDSHAP even entry 2*stage.
    for stage in range(33):
        begin, data = entries["FDSHAP.DAT"][stage*2]
        width, height, count = struct.unpack_from("<3H", data)
        assert (width, height) == (24, 24)
        for frame, (start, end) in enumerate(offsets(data, 6, count, 6+4*count)):
            source = data[start:end]
            add(f"shape:{stage}:{frame}", "shape", "FDSHAP.DAT", begin+start, begin+end,
                width, height, entry=stage*2, frame=frame, modes=rle_extent(source, width, height))
    icon = resources["FDICON.B24"]
    width, height, count = struct.unpack_from("<3H", icon)
    assert (width, height) == (24, 24)
    for frame, (start, end) in enumerate(offsets(icon, 6, count, 6+4*count)):
        add(f"icon:{frame}", "icon", "FDICON.B24", start, end, width, height,
            frame=frame, modes=rle_extent(icon[start:end], width, height))
    for portrait, (begin, data) in enumerate(entries["DATO.DAT"]):
        first = u32(data)
        assert first and first % 4 == 0
        for frame, (start, end) in enumerate(offsets(data, 0, first//4, first)):
            width, height = lmi_extent(data[start:end])
            add(f"portrait:{portrait}:{frame}", "portrait", "DATO.DAT", begin+start, begin+end,
                width, height, entry=portrait, frame=frame)
    empty_figani = []
    for entry, (begin, data) in enumerate(entries["FIGANI.DAT"]):
        if data == b"\0\0\x0a":
            empty_figani.append(entry)
            continue
        count = data[0]
        assert count and u32(data, 8) == 8+count*4
        for part, (start, end) in enumerate(offsets(data, 8, count, 8+4*count)):
            assert end-start >= 13
            header = start+9
            width, height = u16(data, header), u16(data, header+2)
            assert width and height
            add(f"figani:{entry}:{part}", "figani", "FIGANI.DAT", begin+header, begin+end,
                width, height, entry=entry, frame=part, raw_x=u16(data, start), raw_y=u16(data, start+2),
                marker=data[start+4], layer=data[start+7], modes=rle_extent(data[header+4:end], width, height))
    old = {} # fresh regeneration executes every ANI frame; no private inherited payload
    for entry, (begin, data) in enumerate(entries["ANI.DAT"]):
        assert data[:28] == b"AFM - Animation File Manager"
        count, width, height, size = struct.unpack_from("<4H", data, 0xa5)
        assert (width, height, size) == (320, 200, 64000)
        cursor = 0xad
        for frame in range(count):
            length, commands = struct.unpack_from("<HH", data, cursor)
            cursor += 8
            start, cursor = cursor, cursor+length
            assert cursor <= len(data)
            inherited = old.get((entry, frame))
            if inherited:
                assert inherited["commands"] == commands and inherited["payload_bytes"] == length
            add(f"ani:{entry}:{frame}", "ani", "ANI.DAT", begin+start, begin+cursor,
                width, height, entry=entry, frame=frame, commands=commands,
                inherited_original_hashes=inherited)
        assert cursor == len(data)
    begin, font = entries["FDOTHER.DAT"][4]
    assert len(font) % 32 == 0
    for glyph in range(len(font)//32):
        add(f"glyph:{glyph}", "glyph", "FDOTHER.DAT", begin+glyph*32, begin+(glyph+1)*32,
            16, 16, entry=4, glyph=glyph)
    assert counts == {"shape": 8256, "icon": 1680, "portrait": 544, "figani": 2118, "ani": 289, "glyph": 1824}, counts
    assert len(empty_figani) == 144
    return rows, blobs, counts, empty_figani

def expand_base(rows):
    result=[]
    for row in rows:
        family=row['family']
        variants=('forward','reverse') if family=='portrait' else ('normal','acted') if family=='icon' else ('bg0','bg74') if family=='glyph' else ('normal',)
        for variant in variants:result.append((row,variant))
    core.require(len(result)==18759,'fresh base original-state denominator')
    return result

def run(out,scope,image,functions,resources,rows,blobs,report):
    animations={};machine=None
    work=expand_base(rows) if scope=='base' else [(row,'normal') for row in rows]
    afm_owners={owner for owner in functions if 0x36e3d<=owner<=0x36ff4}
    full_font=archive(resources['FDOTHER.DAT'])[4][1]
    try:
        with (out/'results.jsonl').open('x',encoding='utf8') as log:
            for index,(row,variant) in enumerate(work):
                family=row['family'];source=blobs[row['key']]
                if scope=='extra':
                    w,h=u16(source),u16(source,2)
                    if row['codec']=='rle_cursor':w=h=24;owners={0x4e22a};entry=0x4e22a;family='shape';args=(kernel.INPUT,kernel.PAGE,w);rle_extent(source,w,h)
                    elif row['codec']=='lmi':owners={0x4ebff,0x4ec66};entry=0x4ebff;family='portrait';args=(kernel.PAGE,kernel.INPUT,w);lmi_extent(source)
                    else:core.require(row['codec']=='rle','extra known codec');owners={0x4e98d};entry=0x4e98d;family='figani';args=(kernel.INPUT,0,0,kernel.PAGE,w,0xffffffff);rle_extent(source[4:],w,h)
                    stride,height=w,h
                else:
                    stride,height=(456,336) if family in ('shape','icon') else (320,200) if family in ('portrait','glyph','ani') else (row['width'],row['height'])
                    owners={0x4e1a6 if variant=='acted' else 0x4e22a} if family in ('shape','icon') else {0x4ebff,0x4ec31,0x4ec66} if family=='portrait' else {0x4ed7a} if family=='glyph' else afm_owners if family=='ani' else {0x4e98d}
                    if family in ('shape','icon'):entry=next(iter(owners));args=(kernel.INPUT,kernel.PAGE+72*456+72,stride)
                    elif family=='figani':entry=0x4e98d;args=(kernel.INPUT,0,0,kernel.PAGE,stride,0xffffffff)
                    elif family=='portrait':
                        entry=0x4ec31 if variant=='reverse' else 0x4ebff
                        args=(kernel.PAGE+(87+115*320 if variant=='reverse' else 232+5*320),kernel.INPUT,stride)
                    elif family=='glyph':
                        source=full_font;entry=0x4ed7a;args=(kernel.INPUT,row['glyph'],kernel.PAGE+8+4*320,stride,205,74,0 if variant=='bg0' else 74)
                if family=='ani':
                    entry_id=row['entry']
                    if entry_id not in animations:
                        core.require(row['frame']==0,'ANI initial frame required')
                        machine=kernel.OriginalPixels(image,functions,owners,bytes(65536),320,200,320,83,'ani')
                        machine.call(0x36fd3,(64000,kernel.PAGE,kernel.PALETTE));animations[entry_id]=machine
                    machine=animations[entry_id]
                    core.require(len(source)<=65536,'ANI explicit scratch input extent')
                    machine.source=source+bytes(65536-len(source));machine.cpu.mem_write(kernel.INPUT,machine.source)
                    machine.mask[:]=bytes(machine.size);machine.palette_mask[:]=bytes(768)
                    entry,args=0x36ff4,(row['commands'],kernel.INPUT)
                else:
                    machine=kernel.OriginalPixels(image,functions,owners,source,w if scope=='extra' else row['width'],height,stride,83,family)
                machine.call(entry,args);pixels,palette=machine.payload()
                raw=pixels+palette+bytes(machine.mask)+bytes(machine.palette_mask);packed=zlib.compress(raw,9)
                file=f'case-{index:05d}.zbin';(out/file).write_bytes(packed)
                item={'asset':row['key'],'family':row['family'],'variant':variant,'stride':stride,'owner_height':height,
                    'source_sha256':row.get('source_sha256',row.get('sha256')),'abi_input_buffer_sha256':sha(machine.source),
                    'owners':sorted(owners),'entry':entry,'args':list(args),'payload':file,'payload_sha256':sha(packed),
                    'payload_bytes':len(raw),'pixels_sha256':sha(pixels),'write_mask_sha256':sha(machine.mask),
                    'pixel_addresses_written':sum(machine.mask),'palette_addresses_written':sum(machine.palette_mask),
                    'visited_heads':sorted(machine.visited),'no_IO_or_outside_write':True,'source_unchanged':True}
                report['results'].append(item);report['original_cases_executed']+=1
                log.write(json.dumps(item)+'\n');log.flush()
                if family!='ani':machine.close()
                machine=None
                if index%500==0:print(json.dumps({'done':index+1,'total':len(work)}),flush=True)
    finally:
        if machine and machine not in animations.values():machine.close()
        for value in animations.values():value.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--output-root',type=Path,required=True);parser.add_argument('--image',type=Path)
    parser.add_argument('--scope',choices=('base','extra'),required=True)
    parser.add_argument('--execute-original',action='store_true',help='explicit new original run; default only inventories inputs')
    args=parser.parse_args();root=args.root.resolve();out=core.checked_output(root,args.out,args.output_root);out.mkdir()
    guards={};report={'schema':'fd2-f4-reproducible-original-v2','scope':args.scope,'ok':False,'original_cases_executed':0,
        'actual_C_cases_executed':0,'GUI_or_build_executed':False,'results':[],
        'limits':'Original pixel kernel ABI only; no natural binding/walker/heap, whole application, clocks/RNG/audio or current C claim.'}
    try:
        for path in (Path(__file__),Path(kernel.__file__),Path(core.__file__),Path(extra.__file__),Path(archive_entries.__code__.co_filename)):
            extra.snapshot(path.resolve(),guards)
        resources={name:extra.snapshot(root/'original_game'/name,guards,digest) for name,digest in core.RESOURCES.items()}
        extra.snapshot(root/'original_game/FD2.EXE',guards,core.EXE_SHA)
        if args.scope=='base':rows,blobs,counts,empty=catalog_base(resources);work=expand_base(rows);report['empty_FIGANI_entries']=empty
        else:
            rows,_=extra.collect(resources,archive_entries);rows=[r for r in rows if 'derived' not in r and r['codec']!='unknown_ui']
            core.require(len(rows)==256,'extra fixed known-image count')
            blobs={r['key']:resources[r['file']][r['start']:r['end']] for r in rows};counts=Counter(r['family'] for r in rows);work=rows
        report.update({'physical_spans':len(rows),'families':dict(counts),'planned_original_states':len(work)})
        with (out/'catalog.jsonl').open('x',encoding='utf8') as stream:
            for row in rows:stream.write(json.dumps(row)+'\n')
        if args.execute_original:
            core.require(args.image is not None and sys.flags.optimize==0,'strict image/active guards required')
            import unicorn,unicorn.x86_const as regs
            core.require(unicorn.__version__=='2.1.4','qualified Unicorn2.1.4')
            for path in (Path(sys.executable),Path(unicorn.__file__),Path(regs.__file__),Path(unicorn.__file__).parent/'lib/unicorn.dll'):
                extra.snapshot(path,guards)
            image=extra.snapshot(args.image.resolve(),guards,core.IMAGE_SHA)
            manifest=Path(kernel.__file__).parent/'fixtures/f4_pixel_kernels.json'
            functions=kernel.validate_manifest(image,extra.snapshot(manifest,guards,kernel.MANIFEST_SHA))
            run(out,args.scope,image,functions,resources,rows,blobs,report)
        report['ok']=True
    except BaseException:
        report['error']=traceback.format_exc();core.write_diagnostic(out/'failure.txt',report['error'],report,'first error')
    finally:core.finish_report(out,report,guards)
    print(json.dumps({k:report.get(k) for k in ('ok','physical_spans','planned_original_states','original_cases_executed','guards_unchanged','error')}))
    return 0 if report['ok'] else 1
if __name__=='__main__':raise SystemExit(main())
