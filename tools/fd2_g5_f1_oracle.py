"""2000a/1f558/1f6ef：原map/shape来源的F1两尺寸域及精确3槽view alias。

不读取SDL冷表或运行结果；alias只在隔离副本使用，非晚关支持/剧情夹具。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import struct

import fd2_stage2_storage as stage2_storage
from audit_fdicon_opaque_zero import audit_fdicon
from run_graphics_tests import decode_save
from run_original_comparison import archive_entry, decode_image
from run_original_end_turn import META, UNITS, STRIDE, round_assets, validate_initial
from run_original_f1 import scaled_base, marker_frames, tile_payload, prepare as stage0_prepare
from run_original_player import cursor_assets

SCENARIOS=('stage1','stage2','tall-alias7')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def dat_entries(data):
    if len(data)<14 or data[:6]!=b'LLLLLL':raise ValueError('alias DAT头不符')
    first=struct.unpack_from('<I',data,6)[0]
    if first<14 or first>=len(data) or (first-6)%4:raise ValueError('alias DAT目录长度不符')
    offsets=struct.unpack_from('<'+'I'*((first-6)//4),data,6)
    if offsets[0]!=first or offsets[-1]!=len(data) or any(a>=b for a,b in zip(offsets,offsets[1:])):
        raise ValueError('alias DAT目录／EOF不符')
    return [data[a:b] for a,b in zip(offsets,offsets[1:])]


def repack_dat(entries):
    if not entries or any(not row for row in entries):raise ValueError('alias DAT不接受空entry')
    at=6+4*(len(entries)+1);offsets=[at]
    for row in entries:at+=len(row);offsets.append(at)
    output=b'LLLLLL'+struct.pack('<'+'I'*len(offsets),*offsets)+b''.join(entries)
    if dat_entries(output)!=entries:raise AssertionError('alias重建目录往返不符')
    return output


def view_alias(original):
    """仅三个payload换源；元数据/EXE/SAV保持原件，全部未换entry逐byte核。"""
    original=Path(original);outputs={};slots=[]
    for name,mapping in (('FDFIELD.DAT',{0:21}),('FDSHAP.DAT',{0:14,1:15})):
        before=(original/name).read_bytes();entries=dat_entries(before);after=list(entries)
        for target,source in mapping.items():
            after[target]=entries[source]
            slots.append({'file':name,'target_entry':target,'source_entry':source,
                'before_sha256':sha(entries[target]),'source_sha256':sha(entries[source]),
                'source_size':len(entries[source])})
        packed=repack_dat(after);actual=dat_entries(packed)
        if any(actual[i]!=entries[mapping.get(i,i)] for i in range(len(entries))):
            raise AssertionError('alias声明外entry变化')
        outputs[name]=packed
    return outputs,{'kind':'three_slot_resource_view_alias','source_stage':7,'runtime_stage':0,
        'slots':slots,'all_other_entries_byte_identical':True,
        'directory_offsets_rebuilt_and_roundtrip_verified':True,
        'EXE_metadata_SAV_units_RNG_modified':False,'shipping_capability_changed':False,
        'battle_END_move_story_allowed':False,
        'oracle_source':'readonly original FDFIELD[21], FDSHAP[14/15], not generated alias'}


def validate_stage2_origin(original,fixture,report_path):
    """只接B7原端已完成的15页entry，不把模型/SDL或旧top flags当替代。"""
    if original is None:raise ValueError('F1 stage2来源需要只读原资源路径')
    fixture,report_path=Path(fixture),Path(report_path)
    report=json.loads(report_path.read_text(encoding='utf-8'))
    required=('source_fixture_and_report_unchanged','original_bytes_size_mtime_unchanged',
        'tools_and_binaries_unchanged','save_bytes_mtime_expected','owned_cleanup','business_complete',
        'snapshot_files_unchanged','isolated_static_resources_unchanged')
    guards=report.get('guards',{});processes=report.get('processes',[])
    if (report.get('kind')!='original' or report.get('status')!='passed' or
            report.get('model_only') is True or report.get('business_sequence_completed') is not True or
            any(guards.get(key) is not True for key in required) or
            len(processes)!=1 or type(processes[0].get('pid')) is not int or processes[0]['pid']<=0 or
            processes[0].get('exit_code')!=0 or processes[0].get('cleaned') is not True or
            processes[0].get('forced_cleanup') is not False):
        raise ValueError('F1 stage2需B7原版完整business/8guard及唯一正常owned来源；模型不可替代')
    source=stage2_storage.prepare(original,fixture,'stage2-storage',report_path)
    expected=[(page['fragment'],page['token_offset'],page['portrait_id']) for page in source['entry_pages']]
    actual=[(ack.get('fragment'),ack.get('offset'),ack.get('portrait'))
        for ack in report.get('stage2_entry_acknowledgements',[])]
    if actual!=expected:raise ValueError('F1 stage2来源15页ACK不是原资源生成的entry序列')
    points=[point for point in report['checkpoints'] if point.get('name') in
        {f'b7-entry-{index:02d}' for index in range(15)}]
    if any(point.get('pid')!=processes[0]['pid'] for point in points):
        raise ValueError('F1 stage2来源15页检查点不是同一原版owned PID')
    manual=report.get('manual_save',{});path=Path(manual.get('snapshot',''))
    if manual.get('slot')!=0 or not path.is_file() or sha(path.read_bytes())!=manual.get('sha256'):
        raise ValueError('F1 stage2来源manual0实际snapshot缺失或SHA不同')
    manual_plain=decode_save(path)
    at=stage2_storage.SLOT_BASE;end=at+stage2_storage.ROSTER_SIZE+10
    if manual_plain[at:end]!=source['source'][at:end]:
        raise ValueError('F1 stage2实际manual0与后继entry同槽完整2570B不同')
    files=[source['origin']['fixture'],source['origin']['report'],*source['origin']['stage2_entry_captures'],
        {'path':str(path.resolve()),'sha256':manual['sha256'],'size':path.stat().st_size,'mtime_ns':path.stat().st_mtime_ns}]
    return {'schema':'fd2-f1-b7-stage2-origin-v1','path':str(report_path.resolve()),
        'sha256':sha(report_path.read_bytes()),'actual_encoded_Save_sha256':sha(fixture.read_bytes()),
        'native_original_endpoint_only':True,'source_status':report['status'],
        'stage2_entry_ACKs_checked_against_original_pages':15,'strict_B7_guards':list(required),
        'entry_source_measurement':source['origin']['entry_source_measurement'],
        'source_files':files,'source_overall_ok':report.get('ok')}


def stage2_sources_unchanged(origin):
    if not origin or origin.get('schema')!='fd2-f1-b7-stage2-origin-v1':return True
    files=origin.get('source_files',[])
    return bool(files) and all(Path(row['path']).is_file() and
        Path(row['path']).stat().st_size==row['size'] and Path(row['path']).stat().st_mtime_ns==row['mtime_ns'] and
        sha(Path(row['path']).read_bytes())==row['sha256'] for row in files)


def validate_origin(fixture,report_path,stage,*,original=None):
    if stage==2:return validate_stage2_origin(original,fixture,report_path)
    fixture=Path(fixture);report_path=Path(report_path)
    report=json.loads(report_path.read_text(encoding='utf-8'))
    if report.get('kind')!='original' or report.get('status')!='passed' or \
            report.get('original_unchanged') is not True or report.get('fixture_unchanged') is not True:
        raise ValueError('F1 stage来源需正常通过原版端／来源守卫的实际Save报告')
    processes=report.get('processes',[])
    if not processes or any(p.get('exit_code')!=0 or p.get('cleaned') is not True or p.get('forced_cleanup') for p in processes):
        raise ValueError('F1 stage来源缺正常owned清理')
    candidates=[]
    def find(value):
        if isinstance(value,dict):
            if 'snapshot' in value and 'sha256' in value:candidates.append(value)
            for child in value.values():find(child)
        elif isinstance(value,list):
            for child in value:find(child)
    find(report)
    digest=sha(fixture.read_bytes())
    if not any(Path(r['snapshot']).resolve()==fixture.resolve() and r['sha256']==digest for r in candidates):
        raise ValueError('F1来源不是报告绑定的真实完整Save')
    if stage==1 and report.get('probe_only'):
        if report.get('probe_cases_passed') is not True or report.get('business_sequence_completed') is not True or \
                report.get('copy_save_matches_completed_state') is not True or \
                report.get('cleanup_passed') is not True or len(report.get('entry_acknowledgements',[]))!=25:
            raise ValueError('F1受控stage1来源缺完整25页/实际entrySave/晚守卫')
    return {'path':str(report_path.resolve()),'sha256':sha(report_path.read_bytes()),
        'actual_encoded_Save_sha256':digest,'native_original_endpoint_only':True,
        'source_overall_ok':report.get('ok'),'source_status':report['status']}


def return_tiles(original,source,cells,width,height,sheet,attrs,palette):
    """两个完整干地源tile＋cursor核返回，不把整页runtime相位说成已测。"""
    cx,cy,fx,fy=source[META+3:META+7]
    occupied=[tuple(source[UNITS+i*STRIDE:UNITS+i*STRIDE+2])
        for i in range(source[META+1]) if not source[UNITS+i*STRIDE+5]&1]
    result=[]
    for row in range(1,7):
        for col in range(1,12):
            x,y=cx+col,cy+row
            if x>=width or y>=height or (x,y)==(fx,fy) or any(abs(x-a)<=1 and abs(y-b)<=2 for a,b in occupied):continue
            tid=cells[y*width+x]&0x3ff
            if attrs[tid*4]&0x1d:continue
            payload=tile_payload(sheet,tid)
            audit=audit_fdicon(struct.pack('<HHHI',24,24,1,10)+payload,expected_frames=1)
            if audit['counts']['explicit_skip_pixels'] or audit['counts']['alternating_skip_pixels']:continue
            image=decode_image(struct.pack('<HH',24,24)+payload)
            if any(224<=p<240 for p in image.getdata()):continue
            image.putpalette(palette)
            result.append({'world':[x,y],'position':[4+24*col,4+24*row],'terrain_id':tid,'image':image})
            if len(result)==2:return result
    raise ValueError('F1返回页缺两个无遮挡/无动画/无水/完整opaque源tile，未启动窗口')


def prepare(original,scenario,fixture,report_path=None):
    original=Path(original);fixture=Path(fixture)
    if scenario not in SCENARIOS:raise ValueError('未知F1代表')
    source=decode_save(fixture);resource_stage=7 if scenario=='tall-alias7' else int(scenario[-1])
    aliases={};alias=None
    if scenario=='tall-alias7':
        validate_initial(fixture,0)
        if fixture.read_bytes()!=(original/'START.SAV').read_bytes():raise ValueError('高图alias只借原START整档，不接受额外热字段patch')
        aliases,alias=view_alias(original);origin=None
    else:
        if report_path is None:raise ValueError('shipping stage代表需--source-report绑定原版实际entry Save')
        origin=validate_origin(fixture,report_path,resource_stage,original=original) if resource_stage==2 else \
            validate_origin(fixture,report_path,resource_stage)
    field=(original/'FDFIELD.DAT').read_bytes();meta=archive_entry(field,resource_stage*3+1)
    data=archive_entry(field,resource_stage*3);width,height=struct.unpack_from('<HH',data)
    if not 1<=width<=64 or not 1<=height<=64 or len(data)!=4+width*height*4:raise ValueError('F1原地图域不符')
    cells=struct.unpack_from('<'+'I'*(width*height),data,4)
    shape=meta[0];shape_data=(original/'FDSHAP.DAT').read_bytes()
    sheet=archive_entry(shape_data,shape*2);attrs=archive_entry(shape_data,shape*2+1)
    frame_count=struct.unpack_from('<H',sheet,4)[0]
    if scenario!='tall-alias7' and (source[META+2]!=resource_stage or source[0]!=shape or source[:len(meta)]!=meta):
        raise ValueError('F1 shipping Save stage/shape/完整已知metadata不符')
    count=source[META+1]
    if not 1<=count<=96 or source[META+9]>32:raise ValueError('F1来源active/roster数量越界')
    cx,cy,fx,fy,rx,ry=source[META+3:META+9]
    if cx>max(0,width-13) or cy>max(0,height-8) or not 0<=fx<width or not 0<=fy<height or \
            (fx-cx,fy-cy)!=(rx,ry) or rx>=13 or ry>=8:raise ValueError('F1来源镜头/焦点域不符')
    tiles={};used_skips={}
    for index in sorted({v&0x3ff for v in cells}):
        if index*4+4>len(attrs):raise ValueError('F1 map terrain attr越界')
        payload=tile_payload(sheet,index)
        audit=audit_fdicon(struct.pack('<HHHI',24,24,1,10)+payload,expected_frames=1)
        skips=audit['counts']['explicit_skip_pixels']+audit['counts']['alternating_skip_pixels']
        if skips:used_skips[index]=skips
        tiles[index]=bytes(decode_image(struct.pack('<HH',24,24)+payload).getdata())
    # 复用已验的原EXE颜色表/DAC完整源门；其stage0图不作为新场景期望。
    baseline=stage0_prepare(original,original/'START.SAV');assets=baseline[2]
    base=scaled_base(width,height,cells,tiles,assets['palette'])
    frames,markers=marker_frames(base,source,(fx,fy),(32,80,72),width,height)
    _,composites=cursor_assets(original/'FDICON.B24',source,assets)
    dry=return_tiles(original,source,cells,width,height,sheet,attrs,assets['palette'])
    manifest={'scenario':scenario,'resource_stage':resource_stage,'runtime_stage':source[META+2],
        'map':[width,height],'shape_source':shape,'shape_frames':frame_count,'attributes':len(attrs)//4,
        'scale':3 if height>40 else 4,'final_step':1024 if height>40 else 768,'markers':markers,
        'map_used_tiles_with_RLE_skip':used_skips,'full_frame_pixels':64000,
        'fixture':{'path':str(fixture.resolve()),'sha256':sha(fixture.read_bytes()),'mtime_ns':fixture.stat().st_mtime_ns},
        'source_report':origin,'view_alias':alias,'return_tiles':[{k:v for k,v in x.items() if k!='image'} for x in dry],
        'source_objects':{'map_entry':resource_stage*3,'map_sha256':sha(data),'shape_sheet_entry':shape*2,
            'shape_sheet_sha256':sha(sheet),'shape_attr_entry':shape*2+1,'shape_attr_sha256':sha(attrs)},
        'scope':'F1 read-only view, no END/move/story/new stage battle; SAV disk guard, not native full runtime/RNG measurement'}
    return {'source':source,'encoded':fixture.read_bytes(),'frames':frames,'assets':assets,'composites':composites,
        'return_tiles':dry,'aliases':aliases,'manifest':manifest,'focus':(4+24*rx,4+24*ry)}
