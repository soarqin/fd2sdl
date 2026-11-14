"""S5固定冷调用的原资源引用闭包；不模拟Game、RNG、输入或任意存档身份。"""
from __future__ import annotations
from pathlib import Path
import argparse
from collections import Counter
import json
import struct

import audit_stage_data as audit
from audit_caster_domain import SOURCE_HASHES as CASTER_HASHES
from audit_g4_render_source import OTHER_SHA, rle_writes

# 原S5 baseline.json 51项只读guard；只存身份，不复制资源或生产参数。
EXTRA_HASHES={'FDOTHER.DAT':OTHER_SHA,'FIGANI.DAT':CASTER_HASHES['FIGANI.DAT'],
    'ANI.DAT':'be909c71d0f1121b6632ae931d978e990f6d54c830f4e0509cd6862187c4d963',
    'BG.DAT':'b9fc21d019d6256a4bb7e6da1cefcb0bfe331d8ff74a52a8201570afc98b56de',
    'DATO.DAT':'dff859783606ee95d9811e1e564bcb230ef53c4df9130e8b6d42ad37bc8c27bd',
    'TAI.DAT':'d56fea9c43f8bb59aad89ad76698885d7e07f380d12a4547888a0b60ea5e0410'}
CALLS={'call_init':'stage.source.initialize','call_fade_in':'stage.source.fade.in',
    'call_fade_out':'stage.source.fade.out','call_transition_finish':'stage.palette.transition.finish',
    'call_teleport':'stage.source.teleport','call_cinematic_ending':'stage.cinematic.ending',
    'call_cinematic_credits':'stage.cinematic.credits','call_cinematic_card_first':'stage.cinematic.card_first',
    'call_cinematic_card_normal':'stage.cinematic.card_normal','call_cinematic_card_last':'stage.cinematic.card_last',
    'call_cinematic_montage_round':'stage.cinematic.montage_round'}


def sheet_directory(data):
    """沿原FIGANI byte count与DWORD part目录；图片RLE复用G4严格reader。"""
    if len(data)<8 or not data[0]:raise ValueError('source cinematic sheet is empty/truncated')
    count=data[0];header=8+4*count
    if header>len(data):raise ValueError('source cinematic part directory truncated')
    starts=struct.unpack_from(f'<{count}I',data,8)
    if starts[0]!=header or any(a>=b for a,b in zip(starts,starts[1:])):
        raise ValueError('source cinematic part directory not exact/increasing')
    frames=[]
    for frame,(start,end) in enumerate(zip(starts,(*starts[1:],len(data)))):
        if not header<=start<end<=len(data) or end-start<13:raise ValueError('source cinematic part span invalid')
        width,height=struct.unpack_from('<HH',data,start+9)
        writes=rle_writes(width,height,data[start+13:end])
        frames.append({'frame':frame,'offset':start,'length':end-start,
            'sha256':audit.digest(data[start:end]),'width':width,'height':height,
            'opaque_writes':len(writes),'RLE_verified':True})
    return {'part_count':count,'intro_count':data[2],'frames':frames}


class ResourceReferences:
    def __init__(self,original,core):
        self.original=Path(original);self.data=dict(core);self.snapshots={};self.sheets={}
        for name,expected in EXTRA_HASHES.items():
            path=self.original/name;raw=path.read_bytes()
            if audit.digest(raw)!=expected:raise ValueError('S5 original resource identity differs: '+name)
            self.data[name]=raw;self.snapshots[path]=raw
        self.entries={name:audit.archive_entries(raw) for name,raw in self.data.items() if name.endswith('.DAT')}

    def entry(self,name,index):
        entries=self.entries[name]
        if type(index) is not int or not 0<=index<len(entries):raise ValueError('source resource entry out of range')
        offset,raw=entries[index]
        return raw,{'file':name,'entry':index,'file_sha256':audit.digest(self.data[name]),
            'file_offset':offset,'length':len(raw),'sha256':audit.digest(raw)}

    def sheet(self,entry):
        if entry not in self.sheets:self.sheets[entry]=sheet_directory(self.entry('FDOTHER.DAT',entry)[0])
        return self.sheets[entry]

    def references(self,stage,binding,flows,scripts):
        result=[];seen_refs=set();memo={}
        def append(identity,node,kind,source=None,**fields):
            record={'flow':identity,'node':node,'kind':kind,**fields}
            if source:record['source']=source
            key=str(record)
            if key not in seen_refs:seen_refs.add(key);result.append(record)
        def visit(identity,state,depth=0):
            if depth>8:raise ValueError('fixed source resource call depth exceeded')
            if (identity,state) in memo:return memo[identity,state]
            if identity not in flows:raise ValueError('source resource fixed callee missing: '+identity)
            f=flows[identity];nodes={n['id']:n for n in f['nodes']}
            todo=[(f['entry'],state)];seen=set();ends=set();memo[identity,state]=ends
            while todo:
                pc,state0=todo.pop()
                if (pc,state0) in seen:continue
                seen.add((pc,state0))
                if len(seen)>65536:raise ValueError('source resource path budget exceeded')
                n=nodes[pc];op,args=n['op'],n['args'];text_stage,slots=state0
                if op in ('fail','terminal_wait'):continue
                if op=='end':ends.add(state0);continue
                states={state0}
                if op in CALLS:
                    states=visit(CALLS[op],state0,depth+1)
                elif op=='credits_setup':
                    states={(30,slots)}
                    for entry in (90,91,92):append(identity,pc,'terminal_constructor_resource',self.entry('FDFIELD.DAT',entry)[1],
                        resource_stage=30,hot_stage_not_assigned=True,ordinary_battle_capability=False)
                    metadata=audit.parse_metadata(self.entry('FDFIELD.DAT',91)[0])
                    for entry in (2*metadata['shape'],2*metadata['shape']+1):
                        append(identity,pc,'terminal_constructor_shape',self.entry('FDSHAP.DAT',entry)[1],
                            resource_stage=30,ordinary_battle_capability=False)
                elif op in ('text','story','cin_notice_text'):
                    raw,source=self.entry('FDTXT.DAT',text_stage+1)
                    if not 0<=args[0]<audit.text_fragment_count(raw):raise ValueError('source text fragment out of range after resource rebinding')
                    append(identity,pc,'text_fragment',source,fragment=args[0],resource_stage=text_stage,
                        source_id=f'text:{text_stage+1:03d}:{args[0]:05d}')
                elif op=='move':
                    script=scripts.get(str(args[0]))
                    if not script:raise ValueError('source movement script missing')
                    structure=audit.scan_movement_structure(bytes.fromhex(script['raw_hex']))
                    if not structure['complete'] or structure['consumed_length']!=script['length']:
                        raise ValueError('source movement structure incomplete')
                    actors={a for group in structure['groups'] for a,_ in group['pairs']}
                    append(identity,pc,'movement',{k:script[k] for k in ('canonical_va','length','sha256')},
                        script=args[0],required_raw_prefix=max(actors,default=-1)+1,
                        gate='actual projected identity/cache/count at leaf; not initial count equivalence')
                elif op=='cin_sheet_open':
                    raw,source=self.entry('FDOTHER.DAT',args[1]);sheet=self.sheet(args[1])
                    if not 0<=args[0]<3 or slots[args[0]]!=-1:raise ValueError('source sheet slot already owned')
                    changed=list(slots);changed[args[0]]=args[1];states={(text_stage,tuple(changed))}
                    append(identity,pc,'cinematic_sheet',source,slot=args[0],part_count=sheet['part_count'],
                        intro_count=sheet['intro_count'],all_part_RLE_verified=True)
                elif op=='cin_sheet_close':
                    if not 0<=args[0]<3 or slots[args[0]]<0:raise ValueError('source sheet close lacks owner')
                    changed=list(slots);changed[args[0]]=-1;states={(text_stage,tuple(changed))}
                elif op=='cin_sheet_blit':
                    if not 0<=args[0]<3 or slots[args[0]]<0:raise ValueError('source sheet frame lacks live owner')
                    sheet=self.sheet(slots[args[0]])
                    if not 0<=args[1]<sheet['part_count']:raise ValueError('source cinematic frame out of range')
                    append(identity,pc,'cinematic_frame',self.entry('FDOTHER.DAT',slots[args[0]])[1],
                        part=sheet['frames'][args[1]],destination=args[2])
                elif op in ('cin_image_blit','cin_palette_validate','cin_palette_load','cin_audio_open'):
                    raw,source=self.entry('FDOTHER.DAT',args[0])
                    if op.startswith('cin_palette') and len(raw)!=768:raise ValueError('source palette must be exact768bytes')
                    if op=='cin_image_blit':
                        if len(raw)<4:raise ValueError('source single image header truncated')
                        width,height=struct.unpack_from('<HH',raw)
                        writes=rle_writes(width,height,raw[4:])
                        append(identity,pc,op,source,width=width,height=height,opaque_writes=len(writes),RLE_verified=True)
                    else:append(identity,pc,op,source,gate='literal source identity/extent; actual owner/audio separately')
                elif op=='cin_notice_open':
                    append(identity,pc,'portrait',self.entry('DATO.DAT',args[0])[1],frames=[0,1,2,3],gate='actual C80x80 LMI/RLE and notice UI preflight')
                elif op=='cin_animation':
                    append(identity,pc,'ANI',self.entry('ANI.DAT',args[0])[1],delay_ms=args[1],check_input=args[2],
                           gate='existing animation consumer actual decoding/playback separately')
                elif op=='cin_montage_open':
                    first,second,scene=args
                    if not 0<=first<128 or not 0<=second<128 or not 1<=scene<=63:
                        raise ValueError('source montage literal parameters outside finite family')
                    for entry in (3*first,3*second,3*first+1,3*second+1):
                        raw,source=self.entry('FIGANI.DAT',entry);directory=sheet_directory(raw)
                        attack=entry in (3*first+1,3*second+1)
                        if attack:
                            if raw[1]!=0 or directory['part_count']>22:
                                raise ValueError('source montage attack is not proven bounded nonsplit family')
                            starts=struct.unpack_from(f'<{directory["part_count"]}I',raw,8)
                            if not any(raw[p+4] for p in starts) or any(raw[p+6]>10 for p in starts):
                                raise ValueError('source montage marker/duration outside original20-pair family')
                        append(identity,pc,'montage_attack' if attack else 'montage_idle',source,
                            part_count=directory['part_count'],all_part_RLE_verified=True,
                            actual_frame_timing_and_Game_RNG_checked_separately=True)
                    append(identity,pc,'montage_background',self.entry('BG.DAT',scene)[1])
                    tai=3 if first in (26,54) or second==55 else scene
                    append(identity,pc,'montage_terrain_overlay',self.entry('TAI.DAT',tai)[1])
                elif op in ('teleport_begin','bank80_validate'):
                    entry=81 if op=='teleport_begin' else 80
                    append(identity,pc,'sound_bank',self.entry('FDOTHER.DAT',entry)[1],samples=[0,1] if entry==81 else [11])
                    if entry==81:append(identity,pc,'teleport_LMI_directory',self.entry('FDOTHER.DAT',6)[1],
                        frames=list(range(114,125)),gate='actual C/source LMI decoding and pixel prefixes separately')
                elif op=='cin_credits_roll_text':
                    raw,source=self.entry('FDTXT.DAT',31)
                    if audit.text_fragment_count(raw)<=44:raise ValueError('credits long-page fragment44 missing')
                    append(identity,pc,'credits_long_page',source,fragment=44)
                elif op=='cin_credits_assets_open':
                    append(identity,pc,'credits_terrain',self.entry('TAI.DAT',3)[1])
                elif op in ('story_source','append_group_source','map_replace','raw_animation','cin_credits_actor','cin_credits_card_prepare'):
                    append(identity,pc,op,args=args,resource_stage=text_stage,
                        gate='actual C bounded future-resource/count/raw identity/owner revalidation; not literal proof')
                    if op=='cin_credits_card_prepare':append(identity,pc,'credits_dynamic_references',
                        formulas={'FIGANI':['3*u8(unit_id)','3*u8(unit_id)+1'],'DATO':'u8(unit_id)',
                            'common_text':['u8(text_id)+1','u8(movement_profile@0x20)+150'],
                            'stage31_text':['u8(text_id+12)','frame>=220 ?45:wrapped_fragment']},
                        dynamic_identity_fully_validated_by_this_tool=False)
                for state1 in states:
                    todo.extend((target,state1) for port,target in n['on'].items() if port!='error')
            return ends
        visit(binding['definition_id'],(stage,(-1,-1,-1)))
        return result


def main():
    from stage_source_ledger import Sources
    import s5_stage_transitions as conversion
    from s5_output import validate_output
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('original','s5-extension','output-dir'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args();output=validate_output(args.output_dir,args.original)
    candidate_raw=args.s5_extension.read_bytes();candidate=json.loads(candidate_raw,object_pairs_hook=audit.unique_keys)
    rows,expected,inputs=conversion.validate_candidate(candidate)
    source=Sources(args.original);reader=ResourceReferences(args.original,source.data)
    stage_path=Path(__file__).parents[2]/'data/static/flows/stage.json'
    inputs[stage_path]=stage_path.read_bytes()
    flows={f['id']:f for f in json.loads(inputs[stage_path])['flows']}
    flows.update({identity:f for (_,identity),f in expected.items()})
    scripts={str(index):{'canonical_va':hex(s.canonical_va),'length':len(s.data),
        'sha256':audit.digest(s.data),'raw_hex':s.data.hex()} for index,s in source.streams.items()}
    by_source=[]
    for row in rows:
        stage=row['slot'] if row['definition_domain']=='stage' else 29
        references=reader.references(stage,row,flows,scripts)
        by_source.append({'source_id':row['source_id'],'definition_id':row['definition_id'],
            'disposition':row['disposition'],'resource_fixture_stage':stage,'references':references,
            'scope':'all reachable candidate fixed-call references; dynamic identity and actual execution remain separate C gates'})
    inputs.update(reader.snapshots);inputs.update({path:source.data[name] for name,path in source.paths.items()})
    inputs[args.s5_extension]=candidate_raw;inputs[Path(__file__)]=Path(__file__).read_bytes()
    for name in ('tools/audit_g4_render_source.py','tools/static_data/audit_caster_domain.py'):
        path=Path(__file__).parents[2]/name;inputs[path]=path.read_bytes()
    refs=[r for row in by_source for r in row['references']]
    montage_entries={r['source']['entry']:r['part_count'] for r in refs
                     if r['kind'] in ('montage_attack','montage_idle')}
    report={'schema':'fd2-s5-source-resource-references-v1','by_source':by_source,
        'input_sha256':{str(path):audit.digest(raw) for path,raw in inputs.items()},
        'summary':{'source_slots':len(rows),'reference_records':len(refs),
            'different_archive_entries':len({(r['source']['file'],r['source']['entry']) for r in refs if 'entry' in r.get('source',{})}),
            'by_kind':dict(Counter(r['kind'] for r in refs)),
            'different_full_sheet_entries':len(reader.sheets),
            'full_sheet_parts_RLE_verified':sum(s['part_count'] for s in reader.sheets.values()),
            'full_sheet_summary_archive':'FDOTHER.DAT',
            'different_montage_FIGANI_entries':len(montage_entries),
            'different_montage_FIGANI_parts_RLE_verified':sum(montage_entries.values())},
        'claims':{'original_core_and_extra_resource_identity_verified':True,'reference_count_is_mechanism_count':False,
            'dynamic_roster_FIGANI_portrait_all_states_verified':False,'source_conditions_verified_by_resource_scan':False,
            'actual_C_executed_by_this_tool':False,'native_GUI_executed':False,'new_shipping_bindings_written':False}}
    source.unchanged()
    if any(path.read_bytes()!=raw for path,raw in inputs.items()):raise ValueError('source resource inputs changed')
    output.mkdir(parents=True);(output/'references.json').write_text(conversion.encode(report),encoding='utf8')
    print(conversion.encode(report['summary']),end='')


if __name__=='__main__':main()
