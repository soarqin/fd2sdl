#!/usr/bin/env python3
"""S5正式24 NoTown/shake与20 sixhits/101帧代表；默认只准备，不开窗口。

原/SDL共用run_s5_stage_check的owned/source/SAV/显示守卫。实际WGC只签
有限资源姿态，原码/C逐帧解码单独登记；不声称全帧/声音/自然整关相等。
"""
from __future__ import annotations
import argparse
import json
import hashlib
from pathlib import Path
import struct

from PIL import Image
from native import s5_cinematic_fixture as fixture
from native import s3_fixture as state
import run_s5_stage_check as host
import run_s3_stage_check as shared
import fd2_natural_oracle as natural
import fd2_g3_prepare_pages as prepare_pages
import fd2_g3_deployment_pages as deployment
import fd2_battle_animation_frames as animation
from fd2_opening_oracle import page_match,build_pages
from fd2_transition_oracle import picker_image,picker_match
from run_original_battle_menu import wait_title
from run_original_comparison import normalize,archive_entry
from run_original_end_turn import RoundOracle,META
from run_original_player import exact_tile
from run_g2_battle_check import series


def sheet_anchors(original,palette):
    parts,identity=animation.figani_parts((original/'FDOTHER.DAT').read_bytes(),34)
    if len(parts)!=101:raise ValueError('FDOTHER34原101part目录不同')
    result=[]
    for index in (1,35,68,69,85,100):
        image=Image.new('P',(320,200));mask=Image.new('L',(320,200))
        animation._paste(image,mask,parts[0]);animation._paste(image,mask,parts[index])
        # 4e31c动态DAC224..239不充当静态RGB原资源证据；其余真实写点含0。
        for y in range(200):
            for x in range(320):
                if 224<=image.getpixel((x,y))<240:mask.putpixel((x,y),0)
        anchor=animation._anchor(image,mask,(0,0),{'FDOTHER':34,'part':index},palette)
        if not anchor or anchor['checked_pixels']<256:raise ValueError('101代表缺完整资源写点')
        result.append(anchor)
    return result,identity


def validate_frames(frames):
    if len(frames)<2:raise AssertionError('持续WGC至少两真实帧')
    previous_index=previous_stamp=-1
    for row in frames:
        index,stamp=row['index'],row['wgc_timestamp_100ns']
        if type(index) is not int or type(stamp) is not int or index<=previous_index or stamp<=previous_stamp:
            raise AssertionError('持续WGC index/time不是原始严格顺序')
        if hashlib.sha256(Path(row['path']).read_bytes()).hexdigest()!=row['sha256']:
            raise AssertionError('持续WGC原PNG在资源分类前改变')
        previous_index,previous_stamp=index,stamp


def sheet_gate(frames,anchors):
    validate_frames(frames)
    matches=[];last=-1
    for anchor in anchors:
        found=None
        for row in frames:
            if row['index']<=last:continue
            with Image.open(row['path']) as image:valid=animation._anchor_match(normalize(image),anchor)
            if valid:found=row;break
        if found is None:raise AssertionError('101持续WGC缺原资源有序姿态'+str(anchor['identity']))
        last=found['index'];matches.append({**anchor['identity'],'index':last,
            'sha256':found['sha256'],'wgc_timestamp_100ns':found['wgc_timestamp_100ns'],
            'checked_resource_pixels':anchor['checked_pixels']})
    return {'ok':True,'ordered_resource_witnesses':matches,'all101_runtime_frames_claimed':False,
        'full_frame_pair_equality_claimed':False,'decoder_all_parts_proved_separately':True}


def shake_anchors(p,view):
    original=p['original'];map_data,meta,w,h=natural._stage_map(original,view)
    shapes=(original/'FDSHAP.DAT').read_bytes();tiles=archive_entry(shapes,2*meta[0]);attrs=archive_entry(shapes,2*meta[0]+1)
    records=state.units(view);cx,cy=view[META+3:META+5];found=[]
    for ry in range(1,7):
        for rx in range(1,12):
            x,y=cx+rx,cy+ry
            if x>=w or y>=h or any(not u[5]&1 and u[0]==x and abs(u[1]-y)<=1 for u in records):continue
            cell=struct.unpack_from('<I',map_data,4+4*(y*w+x))[0];tile=cell&1023
            if attrs[4*tile]:continue
            image=animation.sheet_frame(tiles,tile)
            if image.size!=(24,24) or 0 in image.getdata() or any(224<=v<240 for v in image.getdata()):continue
            image.putpalette(p['assets']['palette']);a=image.crop((0,0,24,23)).convert('RGB');b=image.crop((0,1,24,24)).convert('RGB')
            if sum(max(abs(x-y) for x,y in zip(v1,v2))>6 for v1,v2 in zip(a.getdata(),b.getdata()))<24:continue
            found.append({'world':[x,y],'tile':tile,'position':[4+24*rx,4+24*ry],'images':[a,b]})
            if len(found)==3:return found
    raise ValueError('stage24 shake缺三个完整静态/无遮挡/可区分行移的原地形ROI')


def shake_gate(frames,anchors):
    validate_frames(frames)
    matches=[];counts=[0,0]
    for row in frames:
        with Image.open(row['path']) as raw:image=normalize(raw)
        phases=[phase for phase in (0,1) if all(exact_tile(image,a['images'][phase],a['position']) for a in anchors)]
        if len(phases)>1:raise AssertionError('shake资源行相位不可区分')
        phase=phases[0] if phases else None
        if phase is not None:counts[phase]+=1
        matches.append({'index':row['index'],'sha256':row['sha256'],'wgc_timestamp_100ns':row['wgc_timestamp_100ns'],'row':phase})
    sequence=[r['row'] for r in matches if r['row'] is not None]
    changes=sum(a!=b for a,b in zip(sequence,sequence[1:]))
    if min(counts)<2 or changes<2:raise AssertionError('shake持续WGC未证两源行各两帧/至少两次变化')
    return {'ok':True,'raw_frame_classification':matches,'row_counts':counts,'observed_changes':changes,
        'anchors':[{k:v for k,v in a.items() if k!='images'} for a in anchors],
        'all120_frames_or_20ms_accuracy_claimed':False,'compared_pixels_per_frame':3*24*23}


def prepare(root,scenario):
    p=fixture.derive(Path(root)/'original_game',scenario)
    p['assets']=host.round_assets(p['original']);p['titles']=host.title_templates(p['original']/'FDOTHER.DAT')
    with state.resource_domain(p['original'],(20,24)):
        if scenario==fixture.SCENARIOS[0]:
            view,records,masks=fixture.entry24(p['source'],p['manual_state'],p['data'],p['original'])
            p.update(view=view,records=records,masks=masks,prepare_assets=prepare_pages.load(p['original']))
            p['pages']=pages_for(p,view,24,(1,2))
            camera,focus=fixture.speech_view(p['original'],records,24,(1,),[5,0],[5,0])
            quake_view=state.as_view(p['source'],records,24,1,camera,focus,17)
            p['shake_anchors']=shake_anchors(p,quake_view)
            deployment.selection(p['prepare_assets'],p['manual_state'],[],0,15)
            p['manifest']['preflight']={'stage':24,'active_count':len(records),'NoTown':True,'selected_raw':list(range(1,16)),
                'retained_roster_count':17,'entry_pages':len(p['pages']),'camera_during_shake':camera,
                'shake_ROIs':[{k:v for k,v in a.items() if k!='images'} for a in p['shake_anchors']]}
        else:
            natural.selection_summary(p['original'],p['source'],p['assets'],0)
            p['range']=natural.range_probe(p['original'],p['source'],p['assets'],0)
            records=fixture.victory20_records(p)
            view=state.as_view(p['source'],records,20,1,[14,8],[14,8],16)
            p['pages']=pages_for(p,view,20,(5,7,8,9,10))
            p['sheet_anchors'],identity=sheet_anchors(p['original'],p['assets']['palette'])
            p['settlement_expected']={kind:fixture.victory20_settlement(p,kind) for kind in ('original','sdl')}
            tail=p['settlement_expected']['original'];town=shared.preparation_town(p,tail)
            if tail['stage']!=21 or tail['count']!=18 or town['source_prepare_type']!=1 or p['data']['raw'][0x523e7+21]:
                raise ValueError('240fa尾部不是append24/23后18人/stage21 type1准备')
            p['manifest']['preflight']={'stage':20,'active_count':75,'sixpresence':6,'story_pages':len(p['pages']),
                'sheet_source':identity,'ordered_WGC_parts':[a['identity']['part'] for a in p['sheet_anchors']],
                'tail_source':'complete240fa:112a5(24)→112a5(23)→11506→stage21',
                'prepared_stage':21,'prepared_roster_count':18,'Town_prepare_type':1,
                'expected_roster_sha256':{kind:state.digest(value['roster']) for kind,value in p['settlement_expected'].items()},
                'scope_end':'all story10 ACK→stage21 Town(type1)→four full manual slots; sheet series stops at first story10 wait'}
    return p


def pages_for(p,view,stage,fragments):
    records=state.units(view);mapping={}
    # 原15f84先首visible active；无visible时从持久/模板完整来源保肖像资格。
    for unit in records:
        if not unit[5]&1:mapping.setdefault(unit[8],unit[7])
    roster=p['manual_state']['roster']
    for i in range(p['manual_state']['count']):mapping.setdefault(roster[i*80+8],roster[i*80+7])
    return build_pages(p['original'],p['assets'],stage_plan=((stage,max(fragments)+1),),
        identities_by_stage={stage:[u[7] for u in records]},text_identities_by_stage={stage:mapping},
        fragment_indices_by_stage={stage:fragments})


def story_pages(capture,p,save,guard,record,cp,fragments,*,stop_last=False):
    for page in p['pages']:
        if page['fragment'] not in fragments:continue
        cp('s5-story-'+str(page['fragment'])+'-'+str(page['token_offset']),
            lambda f,page=page:page_match(f,page,p['assets']),timeout=120)
        record.setdefault('resource_acknowledgements',[]).append({k:page[k] for k in ('fragment','token_offset','portrait_id')})
        if stop_last and page['fragment']==fragments[-1]:return
        capture.key('ENTER')


def entry24(capture,p,save,guard,record):
    cp=shared.checkpoint(capture,record,save,guard);assets=p['prepare_assets'];st=p['manual_state']
    titles=p['titles'][record['title_menu_policy']['menu_count']]
    wait_title(capture,titles[0],timeout=90);capture.key('DOWN');capture.wait(titles[1]);capture.key('ENTER')
    cp('s5-24-title-slot0',lambda f:picker_match(f,picker_image(assets,p['source'],0)),allow_low_color_resource=True)
    buttons=RoundOracle(p['assets'],(0,0))
    notice=prepare_pages.notice(assets,410,portrait=75,xy=(100,119))
    def choice(f,selected):
        text=prepare_pages.notice_match(f,notice);button=buttons.buttons(f,selected)
        return dict(button,text=text) if text and button else None
    capture.key('ENTER');cp('s5-24-NoTown410',lambda f:choice(f,0),timeout=90)
    capture.key('RIGHT');cp('s5-24-NoTown-no',lambda f:choice(f,1));capture.key('ENTER')
    chosen=[]
    for raw in range(1,16):
        expected=deployment.selection(assets,st,chosen,raw-1,15)
        cp('s5-24-choose-'+str(raw),lambda f,e=expected:deployment.match(f,e))
        chosen.append(raw);capture.key('ENTER')
    confirm=prepare_pages.notice(assets,658,portrait=75,xy=(95,119))
    cp('s5-24-658',lambda f:dict(a,b=b) if (a:=buttons.buttons(f,0)) and
        (b:=prepare_pages.notice_match(f,confirm)) else None)
    capture.key('ENTER')
    pages1=[page for page in p['pages'] if page['fragment']==1]
    for page in pages1[:-1]:
        cp('s5-24-story1-'+str(page['token_offset']),lambda f,page=page:page_match(f,page,p['assets']),timeout=90);capture.key('ENTER')
    final=pages1[-1];cp('s5-24-story1-last',lambda f:page_match(f,final,p['assets']),timeout=90)
    with series(capture,record,interval_ms=33):
        capture.key('ENTER')
        first=next(page for page in p['pages'] if page['fragment']==2)
        cp('s5-24-story2-first',lambda f:page_match(f,first,p['assets']),timeout=90)
    record['shake_wgc']=shake_gate(record['continuous_wgc']['frames'],p['shake_anchors'])
    story_pages(capture,p,save,guard,record,cp,(2,))
    cp('s5-24-real-browse',lambda f:natural.focus_match(f,p['original'],p['view'],p['assets']),timeout=90)
    shared.save_activity(capture,p,p['view'],save,guard,record,cp,'s5-24-entry-active',
        lambda before,after,v:state.check_activity(before,after,v,p['records'],p['masks'],p['data']['stages'][24]['metadata'],
            kind=record['kind'],fresh_metadata=True,roster=st['roster']),
        load_available=host.initial_load_availability(record))
    record['source_family']={'stage':24,'NoTown_actual_selection':chosen,'bank88_calls_source_count':4,
        'audio_waveform_or_unmuted_output_claimed':False,'source_120_shake_frames_proved_separately':True}


def victory20(capture,p,save,guard,record):
    cp=shared.checkpoint(capture,record,save,guard)
    wait_title(capture,p['titles'][3][0],timeout=90);capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    cp('s5-20-real-CONTINUE',lambda f:natural.focus_match(f,p['original'],p['source'],p['assets']),timeout=90)
    capture.key('Z')
    cp('s5-20-focus-raw0',lambda f:{'focus':found[1]} if
        (found:=natural.actor_browse_anywhere(f,p['original'],p['source'],p['assets'],0)) else None)
    view,_=natural.actor_browse_anywhere(capture.last,p['original'],p['source'],p['assets'],0)
    summary=natural.selection_summary(p['original'],view,p['assets'],0)
    ranges=natural.range_probe(p['original'],view,p['assets'],0)
    capture.key('ENTER');cp('s5-20-real-selection',lambda f:natural.focus_match(f,p['original'],view,p['assets'],summary=summary)
        if natural.range_match(f,ranges) else None)
    capture.key('ESC')
    story_pages(capture,p,save,guard,record,cp,(5,7,8))
    pages9=[page for page in p['pages'] if page['fragment']==9]
    for page in pages9[:-1]:
        cp('s5-20-story9-'+str(page['token_offset']),lambda f,page=page:page_match(f,page,p['assets']),timeout=90);capture.key('ENTER')
        record.setdefault('resource_acknowledgements',[]).append({k:page[k] for k in ('fragment','token_offset','portrait_id')})
    final=pages9[-1];cp('s5-20-story9-last',lambda f:page_match(f,final,p['assets']),timeout=90)
    # 原端67ms曾主动跳过124个WGC事件；SDL保67ms以守21s演出的600帧上限。
    interval={'original':16,'sdl':67}[record['kind']]
    record['sheet_sampling_policy']={'interval_ms':interval,'max_ms':45000,'max_frames':600,
        'ordered_parts':[1,35,68,69,85,100],'max_channel_difference':3,
        'basis':'original denser acquisition; SDL bounded continuous presentation; same strict resource gate',
        'skipped_native_frames_recovered_or_part35_previously_present_claimed':False}
    with series(capture,record,interval_ms=interval):
        capture.key('ENTER')
        record.setdefault('resource_acknowledgements',[]).append({k:final[k] for k in ('fragment','token_offset','portrait_id')})
        first10=next(page for page in p['pages'] if page['fragment']==10)
        cp('s5-20-story10-first',lambda f:page_match(f,first10,p['assets']),timeout=120)
        record.setdefault('resource_observations',[]).append({k:first10[k] for k in ('fragment','token_offset','portrait_id')})
    record['sheet101_wgc']=sheet_gate(record['continuous_wgc']['frames'],p['sheet_anchors'])
    # 首wait只结束连续采集，完整story10另逐页ACK，避免增加series帧数/等待。
    story_pages(capture,p,save,guard,record,cp,(10,))
    expected=[{k:page[k] for k in ('fragment','token_offset','portrait_id')} for page in p['pages']]
    if record['resource_acknowledgements']!=expected:
        raise AssertionError('source20五个story的ACK不完整、重复或错序')
    prepared=fixture.victory20_settlement(p,record['kind'])
    shared.manual_slots(capture,p,save,guard,record,prepared,label_prefix='s5-stage21')
    record['source_family']={'stage':20,'sixpresence':6,'real_controller_selection_cancel':True,
        'first_story10_wait_reached':True,'complete_story10_ACK':True,'final_story10_ACK_or_nextstage_claimed':True,
        'tail_append_ids':[24,23],'tail_sync_source':'11506 active×18 after both112a5 calls',
        'next_stage':21,'prepared_roster_count':18,'Town_prepare_type':1,'full_manual_slots_verified':[0,1,2,3],
        'sheet_series_ends_at_first_story10_wait':True,
        'all101_runtime_frames_or_full_pair_RGB_claimed':False}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,required=True,type=Path)
    for name in ('sdl','driver','dosbox','build-dir'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',required=True,choices=fixture.SCENARIOS);parser.add_argument('--run',action='store_true')
    args=parser.parse_args(argv)
    if args.run and (not args.build_dir or any(getattr(args,n) is None or not getattr(args,n).is_file() for n in ('sdl','driver','dosbox'))):
        parser.error('--run需正式App/driver/DOSBox及实际构建目录')
    try:
        p=prepare(args.root,args.scenario)
        exercise=entry24 if args.scenario==fixture.SCENARIOS[0] else victory20
        report=host.run_prepared(args,p,exercise,(24,) if exercise==entry24 else (20,),args.scenario,
            ['natural whole-stage combat','unselected bindings','whole shared RNG','full RGB/timing/audio equivalence','Linux/macOS'])
    except Exception as error:parser.exit(1,'S5cin工具未能准备：'+str(error)+'。检查来源/资源与新输出目录。\n')
    print(json.dumps({k:report[k] for k in ('ok','status','scenario')},ensure_ascii=False))
    return int(args.run and not report['ok'])

if __name__=='__main__':raise SystemExit(main())
