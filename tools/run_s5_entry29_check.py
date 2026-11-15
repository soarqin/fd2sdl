#!/usr/bin/env python3
"""S5正式Title LOAD→NoTown29→七次22253→入场Save；默认仅准备。"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import struct
from types import SimpleNamespace

from PIL import Image
from native import s5_entry29_fixture as fixture
from native import s3_fixture as state
import run_s5_stage_check as host
import run_s5_late_check as shared_late
import run_s3_stage_check as shared
import fd2_natural_oracle as natural
import fd2_g3_prepare_pages as prepare_pages
import fd2_g3_deployment_pages as deployment
import fd2_battle_animation_frames as animation
from fd2_transition_oracle import picker_match,picker_image
from run_original_battle_menu import wait_title
from run_original_comparison import archive_entry,normalize
from run_original_end_turn import RoundOracle,decode_lmi
from run_g2_battle_check import series


def teleport_anchors(p,entry):
    sheet=archive_entry((p['original']/'FDOTHER.DAT').read_bytes(),6);result=[]
    for index,call in enumerate(entry['teleports']):
        cx,cy=call['camera'];x,y=call['draw'];at=(4+(x-cx)*24,5+(y-cy)*24)
        occluders=[(4+(u[0]-cx)*24,4+(u[1]-cy-1)*24,
            28+(u[0]-cx)*24,28+(u[1]-cy)*24)
            for u in call['before_records']if not u[5]&1]
        choices=[]
        for frame in range(116,125):
            start,end=struct.unpack_from('<II',sheet,6+4*frame);image=decode_lmi(sheet[start:end])
            if not(4<=at[0] and 4<=at[1] and at[0]+image.width<=316 and at[1]+image.height<=196):
                raise ValueError('七传送源LMI不能靠VGA裁切取得正证')
            mask=Image.new('L',image.size)
            for py in range(image.height):
                for px in range(image.width):
                    value=image.getpixel((px,py));sx,sy=at[0]+px,at[1]+py
                    if value and not 224<=value<240 and not any(a<=sx<c and b<=sy<d for a,b,c,d in occluders):
                        mask.putpixel((px,py),255)
            anchor=animation._anchor(image,mask,at,{'caller':index,'source_pc':call['source_pc'],
                'raw_actor':call['actor'],'FDOTHER':6,'LMI':frame},p['assets']['palette'])
            if anchor and anchor['checked_pixels']>=24:choices.append(anchor)
        if not choices:raise ValueError('传送caller没有足够独立非遮挡原LMI写点')
        result.append(choices)
    return result


def prepare(root):
    p=fixture.derive(Path(root)/'original_game');p['assets']=host.round_assets(p['original'])
    p['titles']=host.title_templates(p['original']/'FDOTHER.DAT');p['prepare_assets']=prepare_pages.load(p['original'])
    with state.resource_domain(p['original'],(29,)):
        p['entry_expected']=fixture.entry(p)
        expected=p['entry_expected']
        p['story_pages']={fragment:shared_late.pages_for(p,records,29,(fragment,),expected['prepared']['roster'])
            for fragment,records in expected['story_records'].items()}
        p['teleport_anchors']=teleport_anchors(p,expected)
        natural.selection_summary(p['original'],expected['view'],p['assets'],0)
        deployment.selection(p['prepare_assets'],p['manual_state'],[],0,19)
    p['manifest']['preflight']={'active_count':27,'deployment_slots':20,'group0_records':7,
        'NoTown_selected':list(range(1,20)),'mandatory_text':9,'source_calls':p['manifest']['teleport_calls'],
        'script87':expected['script'],'story_wait_counts':{str(k):len(v)for k,v in p['story_pages'].items()},
        'observed_LMI_candidates':[[a['identity']['LMI']for a in choices]for choices in p['teleport_anchors']],
        'GUI_executed':False,'whole_stage_natural_claimed':False}
    return p


def teleport_gate(frames,p,indices):
    """每个有序caller一张真实原资源写点帧；不声称完整相位/时长。"""
    next_required=0;matched=[]
    for row in frames:
        if next_required==len(indices):break
        path=Path(row['path'])
        if state.digest(path.read_bytes())!=row['sha256']:raise AssertionError('传送WGC原件SHA变化')
        with Image.open(path)as raw:frame=normalize(raw)
        index=indices[next_required]
        found=next((a for a in p['teleport_anchors'][index]if animation._anchor_match(frame,a)),None)
        if found:
            matched.append({'identity':found['identity'],'checked_pixels':found['checked_pixels'],
                'index':row['index'],'path':row['path'],'sha256':row['sha256']});next_required+=1
    if next_required!=len(indices):raise AssertionError('持续WGC缺少有序传送caller资源正证：'+str(indices[next_required:]))
    return {'source_call_indices':list(indices),'matched':matched,'raw_count':len(frames),
        'scope':'one actual unoccluded source-LMI frame per caller; not every phase/whole animation RGB/audio/timing'}


@contextmanager
def observed_batch(capture,record,label,p,indices,*,interval_ms=16):
    # 唯一WGC/owned实现仍为既有series；只给两个独立目录，不复制采集器或输入driver。
    directory=capture.artifacts/label;directory.mkdir()
    view=SimpleNamespace(driver=capture.driver,process=capture.process,artifacts=directory)
    batch={'label':label,'source_call_indices':list(indices)}
    record.setdefault('teleport_batches',[]).append(batch)
    with series(view,batch,interval_ms=interval_ms):yield
    batch['gate']=teleport_gate(batch['continuous_wgc']['frames'],p,indices)


def exercise(capture,p,save,guard,record):
    proof=p.get('snapshot_import_preflight')
    if not proof or proof.get('passed')is not True or proof.get('encoded_snapshot_sha256')!=state.digest(p['encoded']):
        raise ValueError('entry29缺同SHA真实C20 snapshot预检')
    for path,digest in proof['inputs_sha256'].items():
        if state.digest(Path(path).read_bytes())!=digest:raise ValueError('首窗口前snapshot预检身份失效')
    record['snapshot_import_preflight']=proof
    cp=shared.checkpoint(capture,record,save,guard);assets=p['prepare_assets'];st=p['manual_state']
    titles=p['titles'][record['title_menu_policy']['menu_count']]
    wait_title(capture,titles[0],timeout=90);capture.key('DOWN');capture.wait(titles[1]);capture.key('ENTER')
    cp('s5-29-title-slot0',lambda f:picker_match(f,picker_image(assets,p['source'],0)),allow_low_color_resource=True)
    buttons=RoundOracle(p['assets'],(0,0));notice=prepare_pages.notice(assets,410,portrait=75,xy=(100,119))
    def choice(frame,selected):
        text=prepare_pages.notice_match(frame,notice);button=buttons.buttons(frame,selected)
        return dict(button,text=text)if text and button else None
    capture.key('ENTER');cp('s5-29-NoTown410',lambda f:choice(f,0),timeout=90)
    capture.key('RIGHT');cp('s5-29-NoTown-no',lambda f:choice(f,1));capture.key('ENTER')
    chosen=[]
    for raw in range(1,20):
        expected=deployment.selection(assets,st,chosen,raw-1,19)
        cp('s5-29-choose-'+str(raw),lambda f,e=expected:deployment.match(f,e));chosen.append(raw);capture.key('ENTER')
    confirm=prepare_pages.notice(assets,658,portrait=75,xy=(95,119))
    cp('s5-29-confirm658',lambda f:dict(a,buttons=b)if(a:=prepare_pages.notice_match(f,confirm))and(b:=buttons.buttons(f,0))else None)
    capture.key('ENTER')
    pages=p['story_pages']
    shared_late.acknowledge(capture,p,record,cp,pages[0][:-1],'s5-29-story0')
    final0=pages[0][-1];cp('s5-29-story0-last',lambda f:shared_late.page_match(f,final0,p['assets']),timeout=120)
    with observed_batch(capture,record,'entry29-first4',p,tuple(range(4))):
        capture.key('ENTER');record.setdefault('resource_acknowledgements',[]).append(
            {k:final0[k]for k in('stage','fragment','token_offset','portrait_id')})
        first1=pages[1][0]
        cp('s5-29-story1-first',lambda f:shared_late.page_match(f,first1,p['assets']),timeout=120)
    shared_late.acknowledge(capture,p,record,cp,pages[1],'s5-29-story1')
    shared_late.acknowledge(capture,p,record,cp,pages[2][:-1],'s5-29-story2')
    final2=pages[2][-1];cp('s5-29-story2-last',lambda f:shared_late.page_match(f,final2,p['assets']),timeout=120)
    expected=p['entry_expected'];view=expected['view']
    with observed_batch(capture,record,'entry29-last3',p,(4,5,6)):
        capture.key('ENTER');record.setdefault('resource_acknowledgements',[]).append(
            {k:final2[k]for k in('stage','fragment','token_offset','portrait_id')})
        cp('s5-29-browse',lambda f:natural.focus_match(f,p['original'],view,p['assets']),timeout=120)
    shared.save_activity(capture,p,view,save,guard,record,cp,'s5-29-entry-active',
        lambda before,after,v:state.check_activity(before,after,v,expected['records'],expected['masks'],p['data']['stages'][29]['metadata'],
            kind=record['kind'],fresh_metadata=True,roster=expected['prepared']['roster']),load_available=True)
    record['source_family']={'formal_title_LOAD_NoTown29':True,'actual_selected':chosen,'source_teleport_calls':p['manifest']['teleport_calls'],
        'shared_22253_consumer':'seven source33f78 calls, one original LMI witness each; actualC8/408 covers full prefixes separately',
        'full_activity_container_checked':True,'callback82_naturally_triggered':False,
        'callback82_source_record':'ff5200 turn255/phase0; not reached at entry turn1','all_physical_frames_or_audio_claimed':False}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in('sdl','driver','dosbox','build-dir','snapshot-probe'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--run',action='store_true');args=parser.parse_args(argv)
    if args.run and(not args.build_dir or any(getattr(args,k)is None or not getattr(args,k).is_file()for k in('sdl','driver','dosbox','snapshot_probe'))):
        parser.error('--run需要正式App/driver/DOSBox/build-dir及同SHA实际snapshot probe')
    try:
        p=prepare(args.root);shared_late.snapshot_preflight(args,p)
        report=host.run_prepared(args,p,exercise,(29,),fixture.SCENARIO,
            ['natural whole-stage progression','natural callback82 at turn255','every teleport phase/whole RGB/audio equality','Linux/macOS'])
    except Exception as error:parser.exit(1,'S5 entry29工具准备失败：'+str(error)+'。检查原来源和新输出目录。\n')
    print(json.dumps({k:report[k]for k in('ok','status','scenario')},ensure_ascii=False));return int(args.run and not report['ok'])


if __name__=='__main__':raise SystemExit(main())
