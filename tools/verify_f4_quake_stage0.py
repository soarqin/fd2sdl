"""Pair actual stage0 Game quake pages with retained original sampler/mask Qs.

Only an immutable console probe runs. No original CPU/GUI/build/Save writer.
Three original sampler calls plus source composition yield nine Q/idle pages;
they do not establish a WGCwholeQ0 checkpoint or natural shared-clock trajectory.
"""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import threading
import zlib
from verify_f4_background_pixels import freeze, guards, fresh, save_report, cleanup_probe, archive_entry

SOURCES=['src/tests/field_unit_render_test.c','src/tests/f4_quake_stage0_probe.h',
 'src/field_game.c','src/field_game.h','src/field_save.c','src/save.c','src/save.h',
 'src/field_unit.c','src/field_unit.h','src/field_visual.c','src/field_visual_state.h',
 'src/field_page.c','src/field_page.h','src/field_page_private.h','src/tile.c','src/tile.h',
 'src/field_effect.c','src/field_effect.h','src/field_background.c','src/field_background.h',
 'src/field_round.h','src/field_visual.h','src/dialog_flow.h','src/vga.c','src/vga.h',
 'src/map_sprite.c','src/map_sprite.h','src/shape.c','src/shape.h','src/field.c','src/field.h',
 'src/field_unit_base.c','src/field_unit_base.h','src/image.c','src/image.h','src/archive.c','src/archive.h',
 'src/input.c','src/input.h','src/bios_clock.c','src/bios_clock.h','src/static_data.h',
 'data/static/manifest.json','data/static/tables/battle_animation.json',
 'data/static/tables/field_game_effects.json']
FIXTURE_SHA='1a15fec8ceb499bda25941f662b74790a94d0eddeebdfb4d2f83ab5a5f7f958a'
RESOURCES={'FDFIELD.DAT':'81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d',
 'FDSHAP.DAT':'901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c',
 'FDICON.B24':'7efb4448d05f19c1e17ebd53f3e3afead235f5c008d5167548d834c3686b1e44',
 'FDOTHER.DAT':'882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667'}
def sha(raw):return hashlib.sha256(raw).hexdigest()
def require(ok,why):
    if not ok:raise ValueError(why)
def unpack(raw,size):
    d=zlib.decompressobj();result=d.decompress(raw,size+1)
    require(d.eof and not d.unused_data and len(result)==size,'bounded original payload')
    return result
def plain(raw):
    state=0xa5;result=bytearray(raw)
    for i in range(len(result)):
        state=(state+0x9014)&65535;state=((state<<3)|(state>>13))&65535;result[i]^=state&255
    require(len(result)==22987 and sum(result[:-4])==int.from_bytes(result[-4:],'little'),'fixture checksum')
    return bytes(result)
def expected_frames(paths,pins,qualification,qualification_pin,facts):
    require(len(paths)==len(pins)==2,'exact two retained batches, no duplicate execution')
    q=json.loads(freeze(qualification,facts,qualification_pin))
    require(q.get('passed') and q.get('actual_original_sampler_cases')==3 and q.get('derived_Q_idle_cases')==9 and
        q.get('qualification_only_no_new_execution') is True,'original effective-owner qualification')
    pins_by_path={str(Path(p).resolve()):pin for p,pin in zip(paths,pins)}
    require(all(pins_by_path.get(str(Path(r['report']).resolve()))==r['report_sha256'] and
        r['effective_original_owner_bytes']==64000 and r['mapped_not_write_guard_bytes']==153216-64000 and
        r['all_CPU_reads_and_writes_in_effective_owner'] for r in q['samplers']),'64000 temporary owner; tail only guard')
    frames={};samplers=set()
    for path,pin in zip(paths,pins):
        path=Path(path).resolve();j=json.loads(freeze(path,facts,pin))
        require(j['status']=='passed' and j['guards_unchanged'] and j['fixture_sha256']==FIXTURE_SHA and
            j['C_cases']==0 and not j['GUI'] and not j['stubs'],'retained original provenance')
        for s in j['samplers']:
            require(s['q'] not in samplers and s['no_stub_no_patch'] and s['normal_stack_probe'] and
                s['counts']['io']==0 and s['write_extent']==[0,64000],'three unique original sampler executions')
            samplers.add(s['q'])
        for row in j['cases']:
            key=(row['q'],row['idle']);require(key not in frames and row['bytes']==64000,'unique Q/idle')
            payload=(path.parent/row['payload']).resolve();require(payload.parent==path.parent,'oracle payload containment')
            data=unpack(freeze(payload,facts,row['payload_sha256']),64000)
            require(sha(data)==row['VGA_sha256'],'whole derived original VGA identity');frames[key]=data
    require(samplers=={0,1,2} and set(frames)=={(q,i) for q in range(3) for i in range(3)},'complete source threeQ/threeidle')
    return frames
def loaded_header(row,resources,fixture_plain):
    require(row.get('schema')=='fd2-f4-quake-stage0-v1','actual schema')
    wanted={'field':resources['FDFIELD.DAT'],'shape':archive_entry(resources['FDSHAP.DAT'],0),
            'icon':resources['FDICON.B24'],'fixture_plain':fixture_plain}
    require(set(row)==set(wanted)|{'schema'},'exact actual loaded input keys')
    for k,raw in wanted.items():require(bytes.fromhex(row[k])==raw,'actual same loaded bytes: '+k)
def compare_case(row,index,wanted,palette):
    q,idle=index%3,index//3
    require(row.get('case')==index and row.get('q')==q and row.get('idle')==idle,'actual order/Q/idle')
    require(row.get('rng')==0 and all(row.get(k) is True for k in (
        'game_unchanged','input_palette_vga_unchanged','jitter_only','App0_committed')),'actual selfguards')
    actual=bytes.fromhex(row['vga']);require(len(actual)==64000,'VGA extent')
    if actual!=wanted:
        first=next(i for i,(a,b) in enumerate(zip(actual,wanted)) if a!=b)
        raise ValueError(f'case{index} Q{q}/idle{idle} VGA byte{first}: actual{actual[first]} expected{wanted[first]}')
    view=b''.join(wanted[y*320+4:y*320+316] for y in range(4,196))
    require(bytes.fromhex(row['app_view'])==view,'complete defined App viewport')
    require(bytes.fromhex(row['palette'])==palette and bytes.fromhex(row['dac'])==bytes((v<<2)|(v>>4) for v in palette),'unchanged palette/DAC input')
    if not q:
        app=bytearray((53+i*13+i//456)&255 for i in range(153216))
        for y in range(192):app[(72+y)*456+72:(72+y)*456+384]=view[y*312:(y+1)*312]
        require(bytes.fromhex(row['App0'])==app,'old source App0 full write/preserve set')
    else:require('App0' not in row,'fresh other pages only defined viewport; no unknown tail equality')
def line(stream,limit):
    raw=stream.readline(limit+1)
    require(raw and raw.endswith(b'\n') and len(raw)<=limit,'bounded complete actual line')
    return raw,json.loads(raw)

def pair(a):
    root=a.root.resolve();facts=[]
    for p in (__file__,freeze.__code__.co_filename):freeze(p,facts)
    frames=expected_frames(a.oracle,a.oracle_sha256,a.qualification,a.qualification_sha256,facts)
    resources={n:freeze(a.resources/n,facts,pin) for n,pin in RESOURCES.items()}
    require(a.resources.resolve()==(root/'original_game').resolve(),'same actual hardcoded resource directory')
    fixture_plain=plain(freeze(a.fixture,facts,FIXTURE_SHA));palette=archive_entry(resources['FDOTHER.DAT'],0)
    require(len(palette)==768 and max(palette)<=63,'fixed normal raw0 palette')
    producer=a.producer.resolve();freeze(producer,facts,a.producer_sha256);freeze(a.build_receipt,facts,a.build_receipt_sha256)
    for rel in SOURCES:freeze(root/rel,facts)
    out=fresh(root,a.output);result=dict(passed=False,actual_C_cases=0,primary_error=None,secondary_errors=[],
        new_original_CPU=0,GUI=0,source_PE_binding='separate immutable parent producer/tlog source snapshot join required',
        scope='nine current Game begin_earthquake logical VGA/App views from source-valid actual Load; three retained original sampler calls plus original-mask composition, no WGCwholeQ0')
    process=reader=None;errors=[];stream_hash=hashlib.sha256()
    def read():
        try:
            raw,row=line(process.stdout,8000000);stream_hash.update(raw);loaded_header(row,resources,fixture_plain)
            result['loaded_same_bytes']={k:sha(v) for k,v in resources.items()}
            with gzip.open(out/'actual.jsonl.gz','xb') as sink:
                for index in range(9):
                    raw,row=line(process.stdout,1000000);stream_hash.update(raw);sink.write(raw)
                    compare_case(row,index,frames[index%3,index//3],palette);result['actual_C_cases']+=1
                raw,row=line(process.stdout,4096);stream_hash.update(raw);sink.write(raw)
                require(row=={'end':9,'loaded_binding_unchanged':True} and not process.stdout.read(1),'exact footer/eof')
        except Exception as e:
            errors.append(str(e))
            try:
                if process.poll() is None:process.terminate()
            except OSError as late:result['secondary_errors'].append(dict(operation='reader.terminate',error=str(late)))
    try:
        with (out/'stderr.txt').open('xb') as err:
            process=subprocess.Popen([str(producer),'--f4-quake-stage0',str(a.fixture.resolve())],cwd=root,
                stdout=subprocess.PIPE,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
            reader=threading.Thread(target=read,daemon=True);reader.start();code=process.wait(timeout=60);reader.join(timeout=10)
            require(not reader.is_alive(),'stdout reader did not finish');require(not errors,errors[0] if errors else '')
            require(code==0,f'actual exit{code}');result.update(passed=True,exit_code=code,actual_stream_sha256=stream_hash.hexdigest())
    except Exception as e:result['primary_error']=errors[0] if errors else str(e)
    finally:
        cleanup_probe(process,reader,result);result['identity']=guards(facts)
        if not result['identity']['unchanged']:result.update(passed=False,primary_error=result['primary_error'] or 'late source/binary/original guard')
        save_report(out/'report.json',result)
        print(json.dumps({k:result.get(k) for k in ('passed','actual_C_cases','primary_error')}))
    if not result['passed']:raise SystemExit(1)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','resources','fixture','qualification','producer','build-receipt','output'):p.add_argument('--'+n,type=Path,required=True)
    for n in ('qualification-sha256','producer-sha256','build-receipt-sha256'):p.add_argument('--'+n,required=True)
    p.add_argument('--oracle',type=Path,action='append',required=True);p.add_argument('--oracle-sha256',action='append',required=True)
    pair(p.parse_args())
if __name__=='__main__':main()
