#!/usr/bin/env python3
"""S1 全量来源账／批量夹具；复用严格 DAT、LE、fixup 与审计，不生成玩法。

依据 field_turn_event_check @0x1a813、field_cell_event_lookup @0x13a44、
190ac／1aa1d caller 和已审查的 indexed initialization／1366a 计数格式。
原字节仅存本地 build；规范化不是第二套生产数据，不推导未知 CFG 边。
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct

import audit_stage_data as audit
import audit_common_scope as scope
from verify_unit_tables import source_range
from audit_fdicon_opaque_zero import audit_fdicon

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "fd2-stage-source-ledger-v1"
FIXTURE_SCHEMA = "fd2-stage-source-fixtures-v1"
DISPOSITIONS = ("confirmed", "excluded", "rejected", "unresolved")


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n"


class Sources:
    """DAT 绝对文件坐标与 EXE canonical raw 视图区分；不应用 relocation。"""
    def __init__(self, original):
        self.paths = {name: original / name for name in audit.SOURCE_HASHES}
        self.data = {name: path.read_bytes() for name, path in self.paths.items()}
        self.hashes = {name: audit.digest(raw) for name,raw in self.data.items()}
        for name, expected in audit.SOURCE_HASHES.items():
            if audit.digest(self.data[name]) != expected:
                raise ValueError(f"{name}: source identity mismatch")
        self.entries = {name: audit.archive_entries(raw) for name, raw in self.data.items() if name.endswith('.DAT')}
        self.entry_hashes = {(name,i):audit.digest(raw) for name,entries in self.entries.items() for i,(_,raw) in enumerate(entries)}
        exe = self.data['FD2.EXE']
        le = audit.parse_le(exe)
        self.objects = audit.parse_objects(exe, le)
        self.fixups = audit.collect_internal_offset_fixups(self.paths['FD2.EXE'])
        self.streams = audit.source_scripts(self.paths['FD2.EXE'])
        # object2的热BSS不在源文件；只缓存已初始化文件映射，仍由严格reader校验页。
        self.raw_objects = {obj.index: source_range(exe, obj.relbase, min(obj.vsize,obj.page_count*le.page_size))
                            if obj.page_count else b'' for obj in self.objects}

    def unchanged(self):
        if any(path.read_bytes() != self.data[name] for name, path in self.paths.items()):
            raise ValueError("original source changed during ledger generation")

    def read(self, locator):
        name, length = locator['file'], locator['length']
        if type(length) is not int or length <= 0 or name not in self.data:
            raise ValueError("invalid source length/file")
        if locator.get('file_sha256') != self.hashes[name]:
            raise ValueError("source file digest mismatch")
        if name == 'FD2.EXE':
            va = int(locator['canonical_va'], 16)
            owners = [obj for obj in self.objects if obj.relbase <= va < va + length <= obj.relbase + len(self.raw_objects[obj.index])]
            if len(owners) != 1 or locator.get('view') != 'raw-unrelocated':
                raise ValueError("canonical span has no unique initialized object")
            obj = owners[0]
            if locator['object'] != obj.index or locator['object_offset'] != va - obj.relbase:
                raise ValueError("canonical/object coordinate mismatch")
            return self.raw_objects[obj.index][va - obj.relbase:va - obj.relbase + length]
        at = locator['file_offset']
        if type(at) is not int or not 0 <= at < at + length <= len(self.data[name]):
            raise ValueError("DAT source offset out of range")
        if 'entry' in locator:
            entry, relative = locator['entry'], locator['entry_offset']
            if type(entry) is not int or not 0 <= entry < len(self.entries[name]):
                raise ValueError("DAT entry out of range")
            start, raw = self.entries[name][entry]
            if (type(relative) is not int or not 0 <= relative < relative + length <= len(raw)
                    or start + relative != at or locator['entry_sha256'] != self.entry_hashes[name,entry]):
                raise ValueError("DAT entry span/digest mismatch")
        return self.data[name][at:at + length]

    def dat(self, name, entry, offset=0, length=None):
        start, raw = self.entries[name][entry]
        return {'file': name, 'file_sha256': self.hashes[name], 'entry': entry,
                'entry_sha256': self.entry_hashes[name,entry], 'entry_offset': offset,
                'file_offset': start + offset, 'length': len(raw) if length is None else length}

    def exe(self, va, length):
        obj = next(obj for obj in self.objects if obj.relbase <= va < va + length <= obj.relbase + obj.vsize)
        return {'file': 'FD2.EXE', 'file_sha256': audit.SOURCE_HASHES['FD2.EXE'],
                'view': 'raw-unrelocated', 'canonical_va': hex(va), 'object': obj.index,
                'object_offset': va - obj.relbase, 'length': length}


def record(identity, kind, source, sources, *, family, caller, decoded=None, refs=(),
           disposition='confirmed', reason='source structure verified', stage=None,
           definition=None, parent=None, evidence=None):
    raw = sources.read(source)
    source = {**source, 'read_sha256': audit.digest(raw), 'raw_hex': raw.hex()}
    blocked = disposition in ('unresolved', 'rejected')
    return {'source_id': identity, 'kind': kind, 'stage': stage, 'parent_id': parent,
            'source': source, 'caller': caller, 'family': family, 'decoded': decoded or {},
            'references': list(refs), 'definition': definition,
            'evidence': evidence or ['strict_original_bytes', caller],
            'evidence_level': 'existing_definition_reference' if definition else 'source_structure_or_caller_rule',
            'disposition': disposition, 'reason': reason,
            'control_flow': {'status': 'existing_definition_reference' if definition else 'not_recovered' if blocked else 'not_applicable',
                             'ordered_successors': [], 'owner': 'existing Game/Scene/App; no new state'},
            'blocker': {'stage': 'S2', 'affected_binding': f'stage.{stage:03d}' if stage is not None else identity,
                        'family': family, 'next_evidence': 'recover full caller/CFG and parameter/resource closure before binding',
                        'reason': reason} if blocked else None,
            'new_semantic_conversion': False, 'new_runtime_binding': False}


def validate_records(records, sources):
    ids = {row['source_id'] for row in records}
    if len(ids) != len(records):
        raise ValueError('duplicate source ID')
    for ordinal, row in enumerate(records):
        if row.get('ordinal') != ordinal or row['disposition'] not in DISPOSITIONS:
            raise ValueError('source order/disposition mismatch')
        raw = sources.read(row['source'])
        validate_identity(row,sources)
        if row['source']['raw_hex'] != raw.hex() or row['source']['read_sha256'] != audit.digest(raw):
            raise ValueError('source raw bytes/read digest mismatch')
        validate_decoded(row,raw,sources)
        if row['parent_id'] and row['parent_id'] not in ids:
            raise ValueError('unknown source parent')
        if any(ref not in ids for ref in row['references']):
            raise ValueError('unresolved normalized source reference')
        if row['control_flow']['ordered_successors']:
            raise ValueError('S1 does not infer CFG edges')
        if row['new_semantic_conversion'] or row['new_runtime_binding']:
            raise ValueError('S1 cannot enable gameplay')
        if row['disposition'] in ('unresolved', 'rejected'):
            blocker = row['blocker']
            if not blocker or any(not blocker.get(key) for key in ('stage','affected_binding','family','next_evidence','reason')):
                raise ValueError('unresolved source has no actionable blocker')


def validate_identity(row,sources):
    """ID决定槽位坐标；相邻同字节也不允许换成另一个来源。"""
    parts=row['source_id'].split(':');source=row['source'];kind=row['kind']
    expected=None
    if parts[0]=='stage':
        stage,index=int(parts[1]),int(parts[3])
        if row['stage']!=stage or not 0<=stage<33:raise ValueError('source ID stage mismatch')
        layouts={'turn':(1,3,3),'lookup':(1,0x33,2),'cell_action':(1,0x53,3),
                 'template':(1,0x83,26),'drop':(1,0x83+22,26),'placement':(2,2,6),'map_cell':(0,4,4),
                 'map_header':(0,0,0),'metadata_header':(1,0,0),'placement_header':(2,0,0)}
        group,base,stride=layouts[parts[2]]
        size={'drop':3,'map_header':4,'metadata_header':3,'placement_header':2}.get(parts[2],stride)
        expected=sources.dat('FDFIELD.DAT',stage*3+group,base+index*stride,size)
        if kind in ('turn','lookup','cell_action') and (not 0<=index<16 or row['decoded']['slot']!=index):
            raise ValueError('source slot/index mismatch')
    elif parts[0]=='dispatch' and kind=='dispatch':
        name,index=parts[1],int(parts[2]);table=next(t for t in audit.DISPATCH if t[0]==name)
        if not 0<=index<table[2] or row['decoded']['slot']!=index:raise ValueError('dispatch slot mismatch')
        expected=sources.exe(table[1]+index*4,4)
    elif parts[0]=='movement' and parts[1] in ('pointer','stream'):
        index=int(parts[2]);stream=sources.streams[index]
        expected=sources.exe(audit.POINTER_TABLE_VA+index*4,4) if parts[1]=='pointer' else sources.exe(stream.canonical_va,len(stream.data))
    elif parts[0]=='movement' and parts[1] in ('group','pair'):
        index,group=int(parts[2]),int(parts[3]);stream=sources.streams[index];at=1
        if not 0<=group<stream.data[0]:raise ValueError('movement group identity outside source')
        for _ in range(group):at+=2+stream.data[at+1]*2
        size=2+stream.data[at+1]*2
        if parts[1]=='pair':
            pair=int(parts[4])
            if not 0<=pair<stream.data[at+1]:raise ValueError('movement pair identity outside source')
            at+=2+pair*2;size=2
        expected=sources.exe(stream.canonical_va+at,size)
    elif parts[0]=='archive':expected=sources.dat(parts[1],int(parts[2]))
    elif parts[0]=='attribute':expected=sources.dat('FDSHAP.DAT',int(parts[1]),int(parts[2])*4,4)
    elif parts[0] in ('text','shape_frame'):
        name='FDTXT.DAT' if parts[0]=='text' else 'FDSHAP.DAT';entry,index=int(parts[1]),int(parts[2]);raw=sources.entries[name][entry][1]
        count=audit.text_fragment_count(raw) if parts[0]=='text' else int.from_bytes(raw[4:6],'little')
        if not 0<=index<count:raise ValueError('resource fragment identity outside directory')
        offsets=struct.unpack_from(f'<{count}{"H" if parts[0]=="text" else "I"}',raw,0 if parts[0]=='text' else 6)
        at=offsets[index];end=offsets[index+1] if index+1<count else len(raw)
        expected=sources.dat(name,entry,at,end-at)
    if expected and any(source.get(k)!=v for k,v in expected.items()):
        raise ValueError('source ID/slot coordinate mismatch')


def validate_decoded(row,raw,sources):
    """复读原字节的字段契约；夹具不是原义oracle，caller原义另由完整读集证明。"""
    kind,decoded=row['kind'],row['decoded'];expected={}
    if kind=='turn':expected=dict(zip(('turn','action','phase'),raw))
    elif kind=='lookup':expected={'action':raw[0],'match_arg':raw[1]}
    elif kind in ('cell_action','drop_descriptor'):expected={'mode':raw[0],'param_u16':int.from_bytes(raw[1:3],'little')}
    elif kind=='map_cell':
        value=int.from_bytes(raw,'little');expected={'raw_u32':value,'terrain':value&1023,'index':value>>16&31}
    elif kind=='placement':
        x,y,unit=struct.unpack('<HHH',raw);expected={'x_u16':x,'y_u16':y,'unit_u16':unit,'x_low_byte':x&255,'y_low_byte':y&255}
    elif kind=='template':expected={'unit_id':raw[1],'group':raw[21]}
    elif kind=='terrain_attribute':expected={'flags_u8':raw[0],'raw_u32':int.from_bytes(raw,'little')}
    elif kind=='movement_pair':expected={'actor_raw_index':raw[0],'direction':raw[1]}
    elif kind=='movement_group':expected={'mode':raw[0],'pair_count':raw[1]}
    elif kind=='movement_stream':expected={'group_count':raw[0],'counted_length':len(raw)}
    elif kind=='map_header':expected={'width':int.from_bytes(raw[:2],'little'),'height':int.from_bytes(raw[2:4],'little')}
    elif kind=='metadata_header':expected={'shape':raw[0],'unknown_01':raw[1],'effective_count':raw[2]}
    elif kind=='placement_header':expected={'count':int.from_bytes(raw,'little')}
    elif kind=='shape_directory':expected={'width':int.from_bytes(raw[:2],'little'),'height':int.from_bytes(raw[2:4],'little'),'frame_count':int.from_bytes(raw[4:6],'little')}
    elif kind=='text_directory':expected={'fragment_count':len(raw)//2}
    elif kind=='text_fragment':expected={'word_count':len(raw)//2}
    elif kind in ('dispatch','movement_pointer'):
        target=sources.fixups.get(int(row['source']['canonical_va'],16))
        if not target:raise ValueError('pointer has no independent fixup')
        expected={'target_va':hex(target[2])}
        expected['fixup_target_object' if kind=='dispatch' else 'target_object']=target[0]
        if kind=='dispatch':expected['fixup_target_offset']=hex(target[1])
    if any(decoded.get(key)!=value for key,value in expected.items()):
        raise ValueError('decoded field disagrees with independent original byte read')


def normalize(inventory, sources, common):
    if (inventory['structural_success'] is not True or inventory['semantic_complete'] is not False
            or [s['stage'] for s in inventory['stages']] != list(range(33))
            or inventory['sources'] != {n: {'sha256': audit.digest(b), 'length': len(b)} for n,b in sources.data.items()}):
        raise ValueError('full source inventory identity/scope mismatch')
    rows = []
    add = lambda identity, kind, source, **kw: rows.append(record(identity, kind, source, sources, **kw))
    rid = lambda name, entry: f'archive:{name}:{entry:03d}'
    sid = lambda stage, kind, index: f'stage:{stage:02d}:{kind}:{index:05d}'
    did = lambda table, slot: f'dispatch:{table}:{slot:03d}'
    families = {r['source_id']: r.get('current_semantic',r['semantic']) for r in common['rows']}
    for name, entries in sources.entries.items():
        first = entries[0][0]
        add(f'directory:{name}', 'archive_directory', {'file':name,'file_sha256':audit.digest(sources.data[name]),'file_offset':0,'length':first},
                family='archive_directory', caller='archive_entries', decoded={'entry_count':len(entries)})
        for index, (_,raw) in enumerate(entries):
            add(rid(name,index), 'archive_entry', sources.dat(name,index), family='resource', caller='archive_entries',
                decoded={'entry':index,'byte_length':len(raw)})
    for stage in inventory['stages']:
        st = stage['stage']; meta = stage['metadata']; me = meta['entry']; mp = stage['map']
        map_raw = sources.entries['FDFIELD.DAT'][mp['entry']][1]
        width,height,cells = audit.parse_grid(map_raw)
        add(sid(st,'map_header',0),'map_header',sources.dat('FDFIELD.DAT',mp['entry'],0,4),family='map',caller='field_map_open_stage',
            stage=st,parent=rid('FDFIELD.DAT',mp['entry']),decoded={'width':width,'height':height})
        shape = meta['shape_index']; attr_entry = shape*2+1
        attrs = sources.entries['FDSHAP.DAT'][attr_entry][1]
        frame_count=int.from_bytes(sources.entries['FDSHAP.DAT'][shape*2][1][4:6],'little')
        add(sid(st,'metadata_header',0),'metadata_header',sources.dat('FDFIELD.DAT',me,0,3),family='indexed_initialization',caller='metadata_reader',
            stage=st,parent=rid('FDFIELD.DAT',me),decoded={'shape':shape,'unknown_01':meta['unknown_01'],'declared_deployment_capacity':meta['unknown_01'],'effective_count':meta['effective_template_count']},
            refs=[rid('FDSHAP.DAT',shape*2),rid('FDSHAP.DAT',attr_entry),rid('FDTXT.DAT',st+1)])
        for index,cell in enumerate(cells):
            terrain,ref = cell&1023,(cell>>16)&31
            if terrain*4+4>len(attrs): raise ValueError('map terrain reference outside original attributes')
            special = attrs[terrain*4]&0x60
            refs=[f'attribute:{attr_entry:03d}:{terrain:05d}',rid('FDSHAP.DAT',shape*2)]
            flags=attrs[terrain*4]
            # 已证11eee/12ac6的有限资源索引；不模拟clock或声称自然帧轨迹。
            frames={terrain,terrain+(2 if flags&8 else 1 if flags&0x14 else 0)}
            if flags&128:frames.update({terrain+1,terrain+1+(2 if flags&8 else 0)})
            if any(frame>=frame_count for frame in frames):raise ValueError('decoded map frame reference outside source sheet')
            refs.extend(f'shape_frame:{shape*2:03d}:{frame:05d}' for frame in sorted(frames))
            if special:
                if ref>=16: raise ValueError('special cell outside sixteen source actions')
                refs.append(sid(st,'cell_action',ref))
            elif ref:
                if ref>16: raise ValueError('normal cell outside sixteen source lookup slots')
                refs.append(sid(st,'lookup',ref-1))
            add(sid(st,'map_cell',index),'map_cell',sources.dat('FDFIELD.DAT',mp['entry'],4+index*4,4),family='map_reference',caller='13a44/190ac',
                stage=st,parent=rid('FDFIELD.DAT',mp['entry']),refs=refs,decoded={'x':index%width,'y':index//width,'raw_u32':cell,'terrain':terrain,'index':ref,'index_base':0 if special else 1,'flags60':special,'potential_frame_indices':sorted(frames)})
        for kind,key,size in [('turn','turn_slots',3),('lookup','cell_lookup_slots',2),('cell_action','cell_action_slots',3)]:
            for slot in meta[key]:
                index=slot['slot']; classification=slot.get('classification'); action=slot.get('action')
                caller={'turn':'field_turn_event_check @0x1a813','lookup':'field_cell_event_lookup @0x13a44','cell_action':'cell_execute @0x190ac'}[kind]
                decoded={k:slot[k] for k in ['slot','turn','action','phase','lookup_id','match_arg','mode','param_u16','default_fill','referenced_by_map'] if k in slot}
                refs=[]; disp='confirmed'; reason='fixed slot preserved; no reachability claim'; family='cell_lookup' if kind=='lookup' else kind
                if kind=='lookup' and action==255:
                    disp='excluded';reason='lookup255 sentinel excluded only from current lookup caller'
                elif kind=='cell_action':
                    decoded['caller_semantics']=audit.descriptor_semantics(slot['mode'],slot['param_u16'],caller='cell_execute')
                    action=decoded['caller_semantics'].get('callback_u16')
                    family='cell_descriptor'
                if action is not None and not (kind=='lookup' and action==255):
                    if 0<=action<90:refs.append(did('event_callback',action))
                    else:disp='rejected';reason='callback index outside confirmed 90-slot table; original out-of-table execution not simulated'
                callback_needed = action is not None or kind=='turn'
                if classification in (audit.UNKNOWN,audit.UNOPENED) and disp=='confirmed' and callback_needed:
                    disp='unresolved';reason=slot.get('unresolved_reason') or 'source parameter has no closed formal stage binding'
                if kind=='turn' and slot['default_fill']:
                    disp='rejected';reason='default turn255/phase0 can match; callback255 outside confirmed table, never silently skipped'
                add(sid(st,kind,index),kind,sources.dat('FDFIELD.DAT',me,slot['entry_offset'],size),family=family,caller=caller,stage=st,
                    parent=rid('FDFIELD.DAT',me),refs=refs,decoded={**decoded,'audit_classification':classification,'legacy_classification':slot.get('legacy_classification',classification)},
                    disposition=disp,reason=reason,definition=slot.get('definition'))
        for template in meta['templates']:
            index=template['index'];identity=sid(st,'template',index);effective=template['effective_template']
            refs=[sid(st,'placement',index)] if effective else []
            add(identity,'template',sources.dat('FDFIELD.DAT',me,template['entry_offset'],26),family='indexed_initialization',caller='1088d/10b4e indexed prefix',
                stage=st,parent=rid('FDFIELD.DAT',me),refs=refs,decoded={k:template[k] for k in ['index','unit_id','group','effective_template','placement_index','placement_relation']},
                disposition='confirmed' if effective else 'excluded',reason='same-index low BYTE coordinates' if effective else 'outside original effective prefix; tail bytes retained')
            drop=template['drop']; sem=audit.descriptor_semantics(drop['mode'],drop['param_u16'],caller='defeated_collector')
            callback=sem.get('callback_u16'); refs=[]
            if callback is not None and callback<90:refs.append(did('event_callback',callback))
            if sem['semantic_kind']=='story' and not drop['reference_errors']:
                refs.append(f'text:{st+1:03d}:{drop["param_u16"]:05d}')
            disp='excluded' if not effective or drop['mode']==255 else 'rejected' if callback is not None and callback>=90 else 'unresolved' if callback is not None and drop['classification'] in (audit.UNKNOWN,audit.UNOPENED) else 'rejected' if drop['reference_errors'] else 'confirmed'
            add(sid(st,'drop',index),'drop_descriptor',sources.dat('FDFIELD.DAT',me,template['entry_offset']+22,3),family='drop_descriptor',caller='defeated_collector -> drop_execute @0x1aa1d',
                stage=st,parent=identity,refs=refs,decoded={'mode':drop['mode'],'param_u16':drop['param_u16'],'caller_semantics':sem,'audit_classification':drop['classification'],
                    'legacy_classification':drop.get('legacy_classification',drop['classification'])},disposition=disp,
                reason='resource tail or collector255 excluded before descriptor execution' if disp=='excluded' else drop.get('classification_reason','descriptor caller/parameter binding requires closure'),definition=drop.get('definition'))
        pe=stage['placements']['entry']; placements=stage['placements']['records']
        if len(placements)!=meta['effective_template_count']+meta['unknown_01']:
            raise ValueError('placement prefix/deployment capacity source partition mismatch')
        add(sid(st,'placement_header',0),'placement_header',sources.dat('FDFIELD.DAT',pe,0,2),family='indexed_initialization',caller='placement_reader',stage=st,
            parent=rid('FDFIELD.DAT',pe),decoded={'count':len(placements)})
        for index,(x,y,unit) in enumerate(placements):
            add(sid(st,'placement',index),'placement',sources.dat('FDFIELD.DAT',pe,2+index*6,6),family='indexed_initialization',caller='1088d/10b4e same-index low BYTE',
                stage=st,parent=rid('FDFIELD.DAT',pe),decoded={'index':index,'x_u16':x,'y_u16':y,'unit_u16':unit,'x_low_byte':x&255,'y_low_byte':y&255,
                    'source_role':'indexed_template_position' if index<meta['effective_template_count'] else 'declared_deployment_capacity',
                    'deployment_ordinal':index-meta['effective_template_count'] if index>=meta['effective_template_count'] else None,
                    'runtime_consumption_claimed':False})
    for entry,(_,raw) in enumerate(sources.entries['FDSHAP.DAT']):
        if entry%2==0:
            # 复用现有只读RLE审计；terrain允许opaque0，不施加FDICON CLI的零色门。
            shape_audit=audit_fdicon(raw,expected_frames=None)
            count=shape_audit['validated_frames']; offsets=struct.unpack_from(f'<{count}I',raw,6)
            add(f'shape_directory:{entry:03d}','shape_directory',sources.dat('FDSHAP.DAT',entry,0,6+count*4),family='shape_resource',caller='shape_sheet_open_mem',
                parent=rid('FDSHAP.DAT',entry),decoded={'width':24,'height':24,'frame_count':count,'rle_syntax_verified':True})
            for index,(at,end) in enumerate(zip(offsets,(*offsets[1:],len(raw)))):
                add(f'shape_frame:{entry:03d}:{index:05d}','shape_frame',sources.dat('FDSHAP.DAT',entry,at,end-at),family='shape_resource',caller='shape_frame RLE source',
                    parent=rid('FDSHAP.DAT',entry),decoded={'id':index,'rle_syntax_verified':True,'pixel_equivalence_claimed':False})
            continue
        if len(raw)%4:raise ValueError('incomplete terrain attribute record')
        for index in range(len(raw)//4):
            add(f'attribute:{entry:03d}:{index:05d}','terrain_attribute',sources.dat('FDSHAP.DAT',entry,index*4,4),family='terrain',caller='shape_attribute_reader',
                parent=rid('FDSHAP.DAT',entry),decoded={'flags_u8':raw[index*4],'raw_u32':struct.unpack_from('<I',raw,index*4)[0]})
    for entry,(_,raw) in enumerate(sources.entries['FDTXT.DAT']):
        count=audit.text_fragment_count(raw);offsets=struct.unpack_from(f'<{count}H',raw)
        add(f'text_directory:{entry:03d}','text_directory',sources.dat('FDTXT.DAT',entry,0,count*2),family='text_directory',caller='text_reader',parent=rid('FDTXT.DAT',entry),decoded={'fragment_count':count})
        for index,(at,end) in enumerate(zip(offsets,(*offsets[1:],len(raw)))):
            add(f'text:{entry:03d}:{index:05d}','text_fragment',sources.dat('FDTXT.DAT',entry,at,end-at),family='text_resource',caller='text_reader; token/glyph/portrait validation outside S1',
                parent=rid('FDTXT.DAT',entry),decoded={'id':index,'word_count':(end-at)//2,'token_semantics_verified':False})
    existing_callbacks=defaultdict(list)
    for alias,definition in inventory['flow_bindings'].items():
        parts=alias.split('.')
        if len(parts)==4 and parts[0]=='stage' and parts[2]=='action':
            stage,action=int(parts[1]),int(parts[3])
            if inventory['stages'][stage]['battle_enabled']:
                existing_callbacks[action].append({'runtime_id':alias,'definition':definition,'stage':stage})
    for table,data in inventory['dispatch_tables'].items():
        for slot in data['records']:
            identity=did(table,slot['slot']);semantic=families[identity]
            refs=[f'function:{owner}:{chunk_index:03d}' for owner in slot['ida_owners'] for chunk_index,_ in enumerate(inventory['function_catalog'][owner]['chunks'])]
            kind={'stage_runtime_result':'result','stage_entry':'entry','stage_victory':'victory'}.get(table)
            binding=inventory['stages'][slot['slot']]['dispatch'][kind] if kind else {}
            callbacks=existing_callbacks[slot['slot']] if table=='event_callback' else []
            confirmed=binding.get('classification')==audit.SUPPORTED or bool(callbacks)
            callers={'stage_runtime_result':'117e7 callsite1197b','event_callback':'117e7 callsite1199c / descriptor1aa1d / cell190ac',
                     'adjacent_magic':'magic dispatch1541f','stage_entry':'stage entry25e3a','stage_victory':'stage victory25e23'}
            add(identity,'dispatch',sources.exe(int(slot['source_va'],16),4),family='dispatch_control_flow',caller=callers[table],
                refs=refs,stage=slot['slot'] if kind else None,decoded={'slot':slot['slot'],'table':table,'target_va':slot['target_va'],'ida_owners':slot['ida_owners'],
                    'reviewed_source_families':semantic,'fixup_target_object':slot['target_object'],'fixup_target_offset':slot['target_offset']},
                disposition='confirmed' if confirmed else 'unresolved',
                reason='existing exact stage/caller definitions only; unbound references retain their own blockers' if confirmed else 'full target CFG or exact stage/caller binding remains a S2 input',definition=binding.get('definition') or (callbacks[0]['definition'] if callbacks else None))
            rows[-1]['decoded']['existing_exact_bindings']=callbacks
    for owner,function in inventory['function_catalog'].items():
        for index,chunk in enumerate(function['chunks']):
            add(f'function:{owner}:{index:03d}','function_chunk',sources.exe(int(chunk['canonical_va'],16),chunk['length']),family='function_source',caller='published IDA function/chunk owner',
                decoded={'owner_va':owner,'name':function['name'],'chunk_index':index,'semantic_recovery_claimed':False})
    movement=inventory['movement_pointer_table']
    add('movement:payload','movement_candidate_payload',sources.exe(int(movement['payload']['canonical_va'],16),movement['payload']['length']),family='movement_source',caller='raw initialized object3 payload',
        decoded={'allocation_boundary_confirmed':False,'overlapping_counted_streams_preserved':True})
    for item in movement['records']:
        index=item['id'];stream=inventory['movement_sources'][str(index)];raw=bytes.fromhex(stream['raw_hex']);streamid=f'movement:stream:{index:03d}'
        add(f'movement:pointer:{index:03d}','movement_pointer',sources.exe(int(item['pointer_va'],16),4),family='movement_known',caller='field_movement_script_ptr @0x4eb48',refs=[streamid],decoded={'id':index,'target_va':item['target_va'],'target_object':3})
        add(streamid,'movement_stream',sources.exe(int(stream['canonical_va'],16),stream['length']),family='movement_known',caller='field_movement_script_play @0x1366a',parent='movement:payload',
            decoded={'id':index,'candidate_relation':item['candidate_relation'],'counted_length':len(raw),'group_count':raw[0],'actor_constraint':'raw index < current runtime actor count, not character ID','native_binding_verified':False})
        at=1
        for group,observation in enumerate(item['counted_structure_observation']['groups']):
            count=observation['pair_count'];gid=f'movement:group:{index:03d}:{group:03d}'
            add(gid,'movement_group',sources.exe(int(stream['canonical_va'],16)+at,2+count*2),family='movement_known',caller='1366a counted group order',parent=streamid,decoded={'order':group,'mode':observation['mode'],'pair_count':count})
            for pair,(actor,direction) in enumerate(observation['pairs']):
                add(f'movement:pair:{index:03d}:{group:03d}:{pair:03d}','movement_pair',sources.exe(int(stream['canonical_va'],16)+at+2+pair*2,2),family='movement_operand',caller='1366a raw actor/direction',parent=gid,
                    decoded={'order':pair,'actor_raw_index':actor,'direction':direction,'direction5_preserves_cache':observation['mode']==128 and direction==5})
            at+=2+count*2
        if at!=len(raw):raise ValueError('counted movement stream length mismatch')
    covered=set()
    base=int(movement['payload']['canonical_va'],16)
    for stream in sources.streams.values():covered.update(range(stream.canonical_va,stream.canonical_va+len(stream.data)))
    gaps=[]
    for va in range(base,base+movement['payload']['length']):
        if va not in covered:
            if gaps and gaps[-1][0]+gaps[-1][1]==va:gaps[-1][1]+=1
            else:gaps.append([va,1])
    for index,(va,length) in enumerate(gaps):
        add(f'movement:unconsumed:{index:03d}','movement_unconsumed_bytes',sources.exe(va,length),family='movement_source',caller='complete counted-stream source union',
            parent='movement:payload',disposition='excluded',reason='not consumed by any of the106 counted streams; original bytes retained, not padding semantics')
    gap=inventory['adjacent_nonpointer_bytes']
    add('dispatch:nonpointer_gap','adjacent_nonpointer_bytes',sources.exe(int(gap['canonical_va'],16),gap['length']),family='dispatch_boundary',caller='independent callback/magic table boundary',
        disposition='excluded',reason='two nonpointer DWORD values18; not callback90/91')
    source_join={};by_semantic_source=defaultdict(set)
    for old in common['rows']:
        parts=old['source_id'].split(':')
        if parts[0]=='stage':
            st=int(parts[1]);kind=parts[2]
            identity=(did({'entry':'stage_entry','result':'stage_runtime_result','victory':'stage_victory'}[kind],st)
                      if kind in ('entry','result','victory') else sid(st,'cell_action' if kind=='cell' else kind,int(parts[3])))
        elif parts[0]=='movement':identity=f'movement:stream:{int(parts[1]):03d}'
        else:identity=old['source_id']
        source_join[old['source_id']]=identity
        by_semantic_source[identity].update(old.get('current_semantic',old['semantic']).get('families',[]))
    confirmed_targets=defaultdict(list)
    for row in rows:
        if row['kind']=='dispatch' and row['disposition']=='confirmed':
            confirmed_targets[row['decoded']['target_va']].append(row['source_id'])
    reverse=defaultdict(list)
    for row in rows:
        for ref in row['references']:reverse[ref].append(row['source_id'])
    for ordinal,row in enumerate(rows):
        row['ordinal']=ordinal
        row['semantic_families']=sorted(by_semantic_source[row['source_id']] | set().union(*(by_semantic_source[ref] for ref in row['references'])))
        if row['blocker']:
            row['blocker']['semantic_families']=row['semantic_families']
            row['blocker']['affected_sources']=reverse[row['source_id']]
            row['blocker']['source_evidence']=list(row['references']) or [row['source_id']]
            if row['kind']=='dispatch':
                row['blocker']['next_evidence']='recover full target CFG/chunks at '+row['decoded']['target_va']+'; preserve this exact caller and all referencing raw parameters'
                reuse=confirmed_targets[row['decoded']['target_va']]
                if reuse:
                    row['blocker']['source_evidence']+=reuse
                    row['blocker']['next_evidence']='reuse confirmed target evidence '+','.join(reuse)+'; close this exact caller/parameter/resource binding without automatic target-based approval'
            elif row['disposition']=='rejected':
                row['blocker']['next_evidence']='check full '+row['caller']+' and source domain/reachability; retain refusal, never alias low BYTE or execute table overrun'
            else:
                row['blocker']['next_evidence']='recover the referenced callback full CFG and this source parameter/caller binding; reuse confirmed primitive semantics'
    validate_records(rows,sources)
    by_target=defaultdict(list);by_family=defaultdict(list);destinations=defaultdict(list);semantic_families=defaultdict(list)
    for row in rows:
        by_family[row['family']].append(row['source_id']);destinations[row['disposition']].append(row['source_id'])
        if row['kind']=='dispatch':by_target[row['decoded']['target_va']].append(row['source_id'])
    for row in rows:
        for family in row['semantic_families']:semantic_families[family].append(row['source_id'])
    exception_index=defaultdict(lambda:defaultdict(list))
    for row in rows:
        if row['blocker']:
            for family in row['semantic_families'] or [row['family']]:
                exception_index[family][row['disposition']].append(row['source_id'])
    summary={'resource_groups':len(inventory['stages']),'record_count':len(rows),'record_kind_counts':dict(Counter(r['kind'] for r in rows)),
             'disposition_counts':dict(Counter(r['disposition'] for r in rows)),'source_family_count':len(by_family),'reviewed_semantic_family_count':len(semantic_families),
             'dispatch_slots':sum(r['kind']=='dispatch' for r in rows),'different_dispatch_targets':len(by_target),
             'new_semantic_conversions':0,'new_runtime_bindings':0,'source_requirements_current_counts':inventory['summary']['active_classification_counts'],
             'source_requirements_legacy_counts':inventory['summary']['legacy_active_classification_counts'],
             'resource_groups_without_battle_dispatch_slots':[30,31,32], 'different_dispatch_owners':len(inventory['function_catalog']),
             'existing_definition_references':sum(bool(row['definition']) for row in rows),
             'movement_counted_bytes_sum':sum(len(stream.data) for stream in sources.streams.values()),
             'movement_counted_bytes_union':len(covered),'movement_unconsumed_bytes':sum(length for _,length in gaps)}
    return {'schema':SCHEMA,'schema_version':1,'sources':inventory['sources'],'summary':summary,'records':rows,
            'target_index':dict(sorted(by_target.items())),'family_index':dict(sorted(by_family.items())),
            'reviewed_semantic_family_index':dict(sorted(semantic_families.items())),
            'common_source_join':source_join,'exception_index':dict(exception_index),
            'semantic_index_scope':'reviewed common-source families joined to normalized source IDs and direct references; not implementation completion or CFG recovery',
            'destination_index':dict(destinations),'claims':{'new_gameplay_enabled':False,'original_runtime_executed':False,'all_game_semantic_complete':False,
                'same_target_approves_binding':False,'raw_overlap_is_not_unique_byte_coverage':True},
            'not_checked':['shape rendered pixel equivalence','all text token/glyph/portrait semantics','new callback CFG edges','dynamic reachability and hot-state projections','S2/S3 binding enablement']}


def current_consumer_overlay(inventory, sources, rows, exclusion_proof=None):
    """F1当前消费视图；保留此前分类器视图，不把source-only提升为运行资格。"""
    import convert_stage_callbacks as callbacks
    import verify_common_events as common_source
    import verify_magic_contracts as magic_source

    flows, aliases, _ = audit.load_definitions(ROOT / 'data/static')
    tables = json.loads((ROOT / 'data/static/tables/magic.json').read_bytes(), object_pairs_hook=audit.unique_keys)
    magic_receipt = magic_source.check(tables, sources.paths['FD2.EXE'])
    values = {table['name']: table['values'] for table in tables['tables']}
    changed = []
    for row in rows:
        if row['kind'] != 'dispatch' and row['disposition'] != 'unresolved':
            continue
        current = {'disposition': row['disposition'], 'status': 'formal_binding' if row['disposition'] == 'confirmed' else 'unresolved',
                   'original_target_va': row['decoded'].get('target_va'),
                   'definition': row['definition'], 'formal_bindings': row['decoded'].get('existing_exact_bindings', []),
                   'original_runtime_executed': False, 'all_normal_paths_proven': False}
        if row['kind'] == 'dispatch':
            table, slot = row['decoded']['table'], row['decoded']['slot']
            if table == 'adjacent_magic':
                current.update(disposition='confirmed', status='existing_primitive_caller_domain',
                    definition='magic.effect.023' if slot == 23 else None,
                    projection={name: values[name][slot] for name in magic_source.NAMES},
                    consumer={'profile': 'src/field_magic.c::fd2_field_magic_map_profile_get',
                              'effect': 'src/field_game.c::execute_teleport_magic' if slot == 23 else 'src/field_game.c::execute_map_magic'},
                    caller_scope={'player': 'FULL before map table' if slot < 9 or slot == 24 else 'dedicated teleport selection' if slot == 23 else 'map wrapper',
                                  'AI': 'existing normal-source eligibility; FULL iff id<10 and SHORT option0, otherwise map',
                                  'alias24': 'map24 consumes record16; player24 uses FULL' if slot == 24 else None},
                    evidence=['tools/static_data/verify_magic_contracts.py', 'docs/reverse-engineering/common-battle-semantics.md#2-28-槽地图法术分派与-36-条法术记录'])
                if slot == 23 and audit.resolve(current['definition'], flows, aliases) is None:
                    raise ValueError('current teleport consumer lacks its cold definition')
            elif table == 'event_callback' and slot in (58, 85, 86, 87, 88, 89):
                target = int(row['decoded']['target_va'], 16)
                identity = 'common.cell.grant' if slot == 58 else 'common.callback.085'
                flow = audit.resolve(identity, flows, aliases)
                if flow is None:
                    raise ValueError('reviewed source-only callback lacks its definition')
                if slot == 58:
                    end, expected = common_source.RAW_FUNCTIONS[0x35854]
                    if target != 0x35854 or audit.digest(source_range(sources.data['FD2.EXE'], target, end-target)) != expected:
                        raise ValueError('source-only callback58 original full function differs')
                    if callbacks.digest(callbacks.encode(flow).encode()) != callbacks.EXISTING_FLOW_SHA[58]:
                        raise ValueError('source-only callback58 definition differs from its reviewed full-source graph')
                    source_evidence = {'canonical_va': hex(target), 'end': hex(end), 'raw_unrelocated_sha256': expected}
                else:
                    # 原完整39B：85执行stack-check后ret；86..89均跳同一序言尾。
                    expected = bytes.fromhex('6804000000e8f70b0000c36804000000ebf36804000000ebec6804000000ebe56804000000ebde')
                    if (source_range(sources.data['FD2.EXE'], 0x3642e, len(expected)) != expected or
                            target != (0x3642e, 0x36439, 0x36440, 0x36447, 0x3644e)[slot-85] or
                            [(n['op'], n['args'], n['on']) for n in flow['nodes']] != [('end', [], {})]):
                        raise ValueError('original shared no-op callback/definition differs')
                    source_evidence = {'canonical_va': '0x3642e', 'length': len(expected), 'raw_unrelocated_sha256': audit.digest(expected)}
                current.update(disposition='confirmed', status='source_only_definition', definition=identity,
                    consumer='src/field_game.c::fd2_field_game_common_event_begin', source_evidence=source_evidence,
                    incoming_sources=[other['source_id'] for other in rows if row['source_id'] in other['references']],
                    evidence=['tools/static_data/convert_stage_callbacks.py', 'docs/reverse-engineering/common-event-semantics.md'])
                if current['incoming_sources'] or current['formal_bindings']:
                    raise ValueError('source-only callback acquired a caller; close its exact binding before claiming current coverage')
            elif row['disposition'] == 'confirmed':
                kind = {'stage_runtime_result':'result', 'stage_entry':'entry', 'stage_victory':'victory'}.get(table)
                if kind:
                    binding = inventory['stages'][slot]['dispatch'][kind]
                    current['formal_bindings'] = [{key: binding.get(key) for key in ('runtime_id','runtime_domain','definition','classification')}]
                current['consumer'] = ('src/field_game.c::fd2_field_game_battle_result_query' if table == 'stage_runtime_result' else
                                       'src/scene.c::scene_flow_run' if table in ('stage_entry', 'stage_victory') else
                                       'src/field_game.c::known_common_event_flow')
                current['evidence'] = ['exact source slot/fixup', 'current unique cold binding; parameter/runtime evidence separate']
        else:
            # 无初始引用不证明动态不可达；专用opening不等于普通battle分派。
            current['status'] = 'no_initial_map_reference' if row['kind'] == 'cell_action' and not row['decoded']['referenced_by_map'] else 'opening_resource_without_battle_consumer' if row['stage'] == 31 else 'unresolved'
            current['consumer'] = 'src/scene.c::opening_stage_open / scene.new_game' if row['stage'] == 31 else row['caller']
            current['next_evidence'] = ('prove original3231b opening never dispatches these turn/drop records; do not create stage31 battle aliases'
                                        if row['stage'] == 31 else 'prove no normal map replacement/runtime writer can expose this unreferenced action slot')
            current['exclusion_scope'] = 'initial resource/current formal consumer only; normal dynamic reachability remains unresolved'
            if exclusion_proof is not None and row['source_id'] in exclusion_proof['excluded_source_ids']:
                current.update(disposition='excluded', status='normal_original_consumer_excluded',
                    exclusion_scope=exclusion_proof['qualified_scope'], normal_source_consumer_exclusion_proven=True,
                    exclusion_proof_sha256=exclusion_proof['sha256'])
                current.pop('next_evidence')
        row['current_consumer'] = current
        if current['disposition'] != row['disposition']:
            changed.append(row['source_id'])
    counts = Counter(row.get('current_consumer', {}).get('disposition', row['disposition']) for row in rows)
    return {'schema': 'fd2-current-source-consumers-v1', 'historical_dispositions_preserved': True,
            'current_disposition_counts': {name:counts[name] for name in DISPOSITIONS}, 'refined_source_ids': changed,
            'current_unresolved_source_ids': [row['source_id'] for row in rows if row.get('current_consumer', {}).get('disposition', row['disposition']) == 'unresolved'],
            'magic_source_check': magic_receipt, 'exclusion_proof':exclusion_proof, 'original_runtime_executed': False,
            'all_game_semantic_complete': False, 'new_runtime_bindings': False}


def fixtures(ledger):
    """每来源一例，按共享族聚合；只检查来源与已证规则，不执行剧情。"""
    cases=[{'case_id':r['source_id'],'source_id':r['source_id'],'read_sha256':r['source']['read_sha256'],
            'expected_decoded':r['decoded'],'expected_disposition':r['disposition'],
            'expected_references':r['references'],'family':r['family'],
            **({'expected_current_consumer':r['current_consumer']} if 'current_consumer' in r else {})} for r in ledger['records']]
    return {'schema':FIXTURE_SCHEMA,'ledger_sha256':audit.digest(encode(ledger).encode()),'cases':cases,
            'families':ledger['family_index'],'runtime_executed':False,
            'oracle_boundary':'source-derived fixtures; caller semantics are supported by independent complete original readsets, not by this projection'}


def validate_fixtures(document,ledger):
    if encode(document)!=encode(fixtures(ledger)):
        raise ValueError('fixture/source/schema/coverage mismatch')


def validate_ledger(ledger,sources):
    if ledger.get('schema')!=SCHEMA or type(ledger.get('schema_version')) is not int or ledger['schema_version']!=1:
        raise ValueError('unknown normalized source schema')
    if ledger['claims']!={'new_gameplay_enabled':False,'original_runtime_executed':False,'all_game_semantic_complete':False,
                         'same_target_approves_binding':False,'raw_overlap_is_not_unique_byte_coverage':True}:
        raise ValueError('normalized source claims exceed S1')
    rows=ledger['records'];validate_records(rows,sources)
    kinds=dict(Counter(row['kind'] for row in rows))
    # 固定原资源身份对应的结构分母；函数chunk另受发布IDA身份约束。
    expected={'archive_directory':3,'archive_entry':199,'map_header':33,'metadata_header':33,'map_cell':43734,
              'turn':528,'lookup':528,'cell_action':528,'template':1899,'drop_descriptor':1899,
              'placement_header':33,'placement':2322,'shape_directory':33,'shape_frame':8256,
              'terrain_attribute':9366,'text_directory':34,'text_fragment':1016,'dispatch':208,
              'movement_candidate_payload':1,'movement_unconsumed_bytes':1,'movement_pointer':106,'movement_stream':106,
              'movement_group':350,'movement_pair':1045,'adjacent_nonpointer_bytes':1}
    if {k:v for k,v in kinds.items() if k!='function_chunk'}!=expected or kinds.get('function_chunk',0)<=0:
        raise ValueError('normalized record scope/count mismatch')
    summary=ledger['summary']
    if summary['record_kind_counts']!=kinds or summary['record_count']!=len(rows) or summary['disposition_counts']!=dict(Counter(row['disposition'] for row in rows)):
        raise ValueError('normalized summary does not close')
    targets=defaultdict(list);families=defaultdict(list);destinations=defaultdict(list)
    for row in rows:
        families[row['family']].append(row['source_id']);destinations[row['disposition']].append(row['source_id'])
        if row['kind']=='dispatch':targets[row['decoded']['target_va']].append(row['source_id'])
    if dict(targets)!=ledger['target_index'] or dict(families)!=ledger['family_index'] or dict(destinations)!=ledger['destination_index']:
        raise ValueError('normalized index loses a source or changes order')
    ids={r['source_id'] for r in rows}
    if len(ledger['common_source_join'])!=1497 or not set(ledger['common_source_join'].values())<=ids:
        raise ValueError('historical semantic source join is incomplete')
    semantic=defaultdict(list)
    for row in rows:
        for family in row['semantic_families']:semantic[family].append(row['source_id'])
    if dict(semantic)!=ledger['reviewed_semantic_family_index']:
        raise ValueError('semantic index loses a source or changes order')
    current = ledger.get('current_consumer_summary')
    if current is None and (any('current_consumer' in row for row in rows) or
            'current_consumer_disposition_counts' in summary or
            ledger.get('inputs',{}).get('f1_exclusion_proof') is not None or
            ledger.get('inputs',{}).get('f1_exclusion_inputs_sha256')):
        raise ValueError('current consumer exclusion/view lacks its required summary')
    if current is not None:
        counts = Counter(row.get('current_consumer', {}).get('disposition', row['disposition']) for row in rows)
        refined = [row['source_id'] for row in rows if row.get('current_consumer', {}).get('disposition', row['disposition']) != row['disposition']]
        unresolved = [row['source_id'] for row in rows if row.get('current_consumer', {}).get('disposition', row['disposition']) == 'unresolved']
        if (current.get('schema') != 'fd2-current-source-consumers-v1' or
                current.get('historical_dispositions_preserved') is not True or
                current.get('current_disposition_counts') != {name:counts[name] for name in DISPOSITIONS} or
                summary.get('current_consumer_disposition_counts') != {name:counts[name] for name in DISPOSITIONS} or
                current.get('refined_source_ids') != refined or current.get('current_unresolved_source_ids') != unresolved or
                any(current.get(key) is not False for key in ('original_runtime_executed', 'all_game_semantic_complete', 'new_runtime_bindings'))):
            raise ValueError('current consumer summary/claims do not close')
        for row in rows:
            consumer = row.get('current_consumer')
            if consumer is not None and (consumer.get('disposition') not in DISPOSITIONS or
                    consumer.get('original_runtime_executed') is not False or consumer.get('all_normal_paths_proven') is not False):
                raise ValueError('current consumer exceeds its static evidence scope')
        exclusions = [row for row in rows if row.get('current_consumer',{}).get('disposition') == 'excluded' and row['disposition'] != 'excluded']
        marked = [row for row in rows if row.get('current_consumer',{}).get('normal_source_consumer_exclusion_proven') or
                  row.get('current_consumer',{}).get('status') == 'normal_original_consumer_excluded' or
                  'exclusion_proof_sha256' in row.get('current_consumer',{})]
        proof = current.get('exclusion_proof')
        inputs = ledger.get('inputs',{})
        if proof is None:
            if exclusions or marked or inputs.get('f1_exclusion_proof') is not None or inputs.get('f1_exclusion_inputs_sha256'):
                raise ValueError('current consumer exclusions require explicit verified F1 proof')
        else:
            import f1_consumer_exclusion
            if (not isinstance(proof,dict) or {row['source_id'] for row in exclusions} != set(f1_consumer_exclusion.EXCLUDED_IDS) or
                    {row['source_id'] for row in marked} != set(f1_consumer_exclusion.EXCLUDED_IDS)):
                raise ValueError('current consumer exclusion source IDs differ from F1 scope')
            verified, proof_inputs = f1_consumer_exclusion.validate(proof['path'],sources)
            if (proof != verified or inputs.get('f1_exclusion_proof') != verified or
                    inputs.get('f1_exclusion_inputs_sha256') != {str(path):audit.digest(raw) for path,raw in proof_inputs.items()}):
                raise ValueError('current consumer exclusion proof/input identities differ')
            for row in exclusions:
                consumer = row['current_consumer']
                if (consumer.get('status') != 'normal_original_consumer_excluded' or
                        consumer.get('normal_source_consumer_exclusion_proven') is not True or
                        consumer.get('exclusion_proof_sha256') != verified['sha256'] or
                        consumer.get('exclusion_scope') != verified['qualified_scope']):
                    raise ValueError('current consumer exclusion row/proof join differs')


def build(original,review_path=scope.DEFAULT_REVIEW,f1_proof=None):
    paths=[Path(__file__),Path(audit.__file__),Path(scope.__file__),review_path,
           *sorted((ROOT/'data/static').rglob('*.json')),
           *[ROOT/'tools'/name for name in ['dump_fd2_fixup_table.py','rebuild_fd2_analysis.py','audit_fdicon_opaque_zero.py',
                'static_data/verify_unit_tables.py','static_data/verify_movement_scripts.py','static_data/generate.py',
                'static_data/verify_magic_contracts.py','static_data/verify_common_events.py','static_data/convert_stage_callbacks.py',
                'static_data/f1_consumer_exclusion.py']],
           *[ROOT/'src'/name for name in ('field_magic.c','field_game.c','scene.c','field_common_event_runtime.h')]]
    snapshots={path:path.read_bytes() for path in paths}
    sources=Sources(original)
    inventory=audit.audit(original)
    review_raw=review_path.read_bytes();review=json.loads(review_raw,object_pairs_hook=audit.unique_keys)
    common=scope.build_ledger(inventory,review)
    ledger=normalize(inventory,sources,common)
    proof=None;proof_inputs={}
    if f1_proof is not None:
        import f1_consumer_exclusion
        proof,proof_inputs=f1_consumer_exclusion.validate(f1_proof,sources)
    ledger['current_consumer_summary']=current_consumer_overlay(inventory,sources,ledger['records'],proof)
    ledger['summary']['current_consumer_disposition_counts']=ledger['current_consumer_summary']['current_disposition_counts']
    ledger['summary']['disposition_scope']='disposition_counts retains the prior classifier; current_consumer_disposition_counts includes source-only/primitive consumer joins, never runtime approval'
    ledger['inputs']={'semantic_review_sha256':audit.digest(review_raw),'definition_sources':inventory['definition_sources'],
                      'ida_evidence_sources':inventory['ida_evidence_sources'],'audit_schema':inventory['schema_version'],
                      'descriptor_source_evidence':inventory['descriptor_source_evidence'],
                      'pipeline_inputs_sha256':{str(path.relative_to(ROOT)):audit.digest(raw) for path,raw in snapshots.items()},
                      'f1_exclusion_proof':proof,
                      'f1_exclusion_inputs_sha256':{str(path):audit.digest(raw) for path,raw in proof_inputs.items()}}
    validate_ledger(ledger,sources)
    sources.unchanged()
    if any(path.read_bytes()!=raw for path,raw in {**snapshots,**proof_inputs}.items()):
        raise ValueError('pipeline input changed during ledger generation')
    return inventory,common,ledger,fixtures(ledger)


def verify_bundle(directory,original,f1_proof=None):
    manifest=json.loads((directory/'manifest.json').read_text(encoding='utf8'),object_pairs_hook=audit.unique_keys)
    names=('inventory.json','common-scope.json','ledger.json','fixtures.json')
    if manifest.get('schema')!=SCHEMA or set(manifest['files'])!=set(names):
        raise ValueError('unknown/incomplete source bundle schema')
    actual=[]
    for name in names:
        raw=(directory/name).read_bytes()
        if audit.digest(raw)!=manifest['files'][name]:raise ValueError('bundle file digest mismatch')
        actual.append(json.loads(raw,object_pairs_hook=audit.unique_keys))
    expected=build(original,f1_proof=f1_proof)
    if any(encode(a)!=encode(e) for a,e in zip(actual,expected)):
        raise ValueError('bundle differs from fresh original source/caller/input derivation')
    if manifest['sources']!=expected[2]['sources'] or manifest['summary']!=expected[2]['summary']:
        raise ValueError('bundle manifest scope mismatch')
    return expected[2]['summary']


def validate_output(path,root=ROOT,original=None):
    output=path.resolve()
    if output.is_relative_to((original or root/'original_game').resolve()):
        raise ValueError('source evidence output must not write inside original resource directory')
    if not output.is_relative_to((root/'build').resolve()) or output==(root/'build').resolve():
        raise ValueError('source evidence output must be a new directory below project build/')
    if output.exists():raise ValueError('refusing to overwrite existing output/evidence')
    return output


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original',type=Path,default=ROOT/'original_game')
    parser.add_argument('--f1-proof',type=Path,help='explicit frozen F1 source/CPU exclusion manifest; absent retains8 unresolved')
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--output-dir',type=Path)
    mode.add_argument('--verify',type=Path)
    args=parser.parse_args()
    try:
        if args.verify:
            print(json.dumps({'verified':True,**verify_bundle(args.verify,args.original.resolve(),args.f1_proof)},ensure_ascii=False))
            return
        output=validate_output(args.output_dir,original=args.original)
        inventory,common,ledger,cases=build(args.original.resolve(),f1_proof=args.f1_proof)
        validate_fixtures(cases,ledger)
        output.mkdir(parents=True,exist_ok=False)
        hashes={}
        for name,value in [('inventory.json',inventory),('common-scope.json',common),('ledger.json',ledger),('fixtures.json',cases)]:
            raw=encode(value).encode();(output/name).write_bytes(raw);hashes[name]=audit.digest(raw)
        (output/'manifest.json').write_text(encode({'schema':SCHEMA,'files':hashes,'sources':ledger['sources'],'summary':ledger['summary']}),encoding='utf8')
        print(json.dumps(ledger['summary'],ensure_ascii=False))
    except (OSError,ValueError,KeyError,TypeError,StopIteration,struct.error) as error:
        parser.exit(1,f'全量来源账生成失败：{error}\n')


if __name__=='__main__':main()
