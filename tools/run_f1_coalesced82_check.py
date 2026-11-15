#!/usr/bin/env python3
"""F1受控27→29、inactive29/30 xy-only正式代表；默认只准备。

复用S5 owned/WGC/逐页ACK/全容器Save；原版先行、SDL后行。输入并非自然
击杀轨迹，不以旧entry29七传送代签本次三caller；不验音频或全相位。
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from types import SimpleNamespace

from PIL import Image
from native import f1_coalesced82_fixture as fixture
from native import s3_fixture as state
import run_s5_entry29_check as entry
import run_s5_stage_check as host
import run_s5_late_check as late
import run_s3_stage_check as shared
import fd2_natural_oracle as natural
import fd2_g3_prepare_pages as prepare_pages
import run_original_natural as protocol
from run_original_battle_menu import wait_title,match_indexed
from run_original_comparison import normalize
from run_original_end_turn import META
from field_play_input_test import wait_log
from run_g2_battle_check import series


def prepare(root,save=fixture.SOURCE/'s5-29-entry-active.SAV',report=fixture.SOURCE/'comparison.json'):
    p=fixture.derive(Path(root)/'original_game',save,report)
    p['assets']=host.round_assets(p['original']);p['titles']=host.title_templates(p['original']/'FDOTHER.DAT')
    p['prepare_assets']=prepare_pages.load(p['original'])
    with state.resource_domain(p['original'],(29,)):
        p['expected']={kind:fixture.projection(p,kind)for kind in('original','sdl')}
        p['story_pages']=late.pages_for(p,p['expected']['original']['story_records'],29,(4,),p['source'][0x8a3:0x12a3])
        p['teleport_anchors']=entry.teleport_anchors(p,p['expected']['original'])
        natural.selection_summary(p['original'],p['source'],p['assets'],0)
        natural.cursor_bank(p['original'],p['expected']['sdl']['view'],p['assets'])
    p['manifest']['preflight']={'actual_C_import_checked':False,'GUI_executed':False,
        'fragment4_pages':len(p['story_pages']),'teleport_raw':[22,29,30],
        'source_LMI_candidates':[[a['identity']['LMI']for a in row]for row in p['teleport_anchors']],
        'capacity_semantics_not_original_equivalence':True}
    return p


def round_witness(p,batch,turn):
    candidates=[(frame_id,natural.round_banner(p['original'],p['assets'],turn,frame_id=frame_id))
                for frame_id in (91,90)]
    for row in batch['continuous_wgc']['frames']:
        path=Path(row['path'])
        if state.digest(path.read_bytes())!=row['sha256']:raise ValueError('Round WGC原件改变')
        with Image.open(path)as raw:frame=normalize(raw)
        for frame_id,expected in candidates:
            found=match_indexed(frame,expected,(120,84))
            if found['ok']:return {'turn':turn,'path':row['path'],'sha256':row['sha256'],
                'resource_frame':frame_id,'resource':found,
                'source':'field_turn_cycle_run @0x1a63d..0x1a68d: frames90/91 with number',
                'scope':'actual full numbered Round resource witness; not all phases or absolute timing'}
    raise AssertionError(f'受控END连续WGC缺完整Round{turn:03d}资源正证')


def rebind_current(p,loaded,kind):
    """原135dd保相对focus；重新从本端已验Save绑定所有后续观察模型。"""
    current=fixture.projection({**p,'source':loaded},kind)
    p['expected'][kind]=current
    p['teleport_anchors']=entry.teleport_anchors(p,current)
    p['story_pages']=late.pages_for(p,current['story_records'],29,(4,),loaded[0x8a3:0x12a3])
    return current


def exercise(capture,p,save,guard,record):
    proof=p.get('snapshot_import_preflight')
    if not proof or proof.get('passed')is not True or proof.get('encoded_snapshot_sha256')!=state.digest(p['encoded']):
        raise ValueError('F1容量代表缺同SHA实际App consumer快照预检')
    for path,value in {**proof['inputs_sha256'],**p['manifest']['source_guards']}.items():
        if state.digest(Path(path).read_bytes())!=value:raise ValueError('首窗口前夹具/来源身份改变')
    record['snapshot_import_preflight']=proof
    cp=shared.checkpoint(capture,record,save,guard)
    wait_title(capture,p['titles'][3][0],timeout=90)
    capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    cp('f1-82-formal-CONTINUE',lambda f:natural.focus_match(f,p['original'],p['source'],p['assets']),timeout=90)
    # 首存证明loaded27全已拥有字节；随后真实取消/确认Load，再存完整本端owner。
    loaded=shared.save_activity(capture,p,p['source'],save,guard,record,cp,'f1-82-preload-owned',
        lambda before,after,view:state.check_loaded_save(p['source'],after,view,record['kind']))
    loaded=shared.load_activity(capture,p,loaded,save,guard,record,cp)
    before=bytes(loaded)
    first_source={**p,'source':loaded}
    first=fixture.first_end(first_source,record['kind']);directory=capture.artifacts/'f1-82-first-END-Round002';directory.mkdir()
    first_batch={'label':'first END scans turn1/phase0 without82; Round002'}
    record.setdefault('round_batches',[]).append(first_batch)
    view=SimpleNamespace(driver=capture.driver,process=capture.process,artifacts=directory)
    with series(view,first_batch,interval_ms=33):
        protocol.end_phase(capture,p['original'],loaded,p['assets'],natural.cursor_bank(p['original'],loaded,p['assets']),None,record,cp)
        cp('f1-82-first-END-no-event-browse',lambda f:natural.focus_match(f,p['original'],first['view'],p['assets']),timeout=90)
        if record['kind']=='sdl':wait_log(capture.process,capture.artifacts/'game.log','field phase: turn=2 phase=2 ',timeout=30)
    record['native_round_banners']=[round_witness(p,first_batch,2)]
    loaded=shared.save_activity(capture,p,first['view'],save,guard,record,cp,'f1-82-first-END-count27',
        lambda disk,after,view:fixture.check_first_end(first_source,disk,after,view,record['kind']))
    # 以本端已严格核验的实际Load/首END Save为前态；不借另一端的未知字节。
    current=rebind_current(p,loaded,record['kind'])
    record['phase2_rebind']={'backend':record['kind'],'source_plain_sha256':state.digest(loaded),
        'source_camera_focus':list(loaded[META+3:META+9]),
        'story_pages_rebuilt_from_backend_Save':True,
        'story_records_sha256':state.digest(b''.join(current['story_records'])),
        'pages':[{k:page[k]for k in('stage','fragment','token_offset','portrait_id')}for page in p['story_pages']]}
    protocol.end_phase(capture,p['original'],loaded,p['assets'],natural.cursor_bank(p['original'],loaded,p['assets']),None,record,cp)
    late.acknowledge(capture,p,record,cp,p['story_pages'][:-1],'f1-82-story4')
    final=p['story_pages'][-1]
    cp('f1-82-story4-last',lambda f:late.page_match(f,final,p['assets']),timeout=120)
    # 原82最后ACK之后才有三caller；33ms为有序LMI见证，不称全相位采集。
    with entry.observed_batch(capture,record,'f1-82-three-callers-Round003',p,(0,1,2),interval_ms=33):
        capture.key('ENTER');record.setdefault('resource_acknowledgements',[]).append(
            {k:final[k]for k in('stage','fragment','token_offset','portrait_id')})
        expected=current['view']
        cp('f1-82-phase2-browse',lambda f:natural.focus_match(f,p['original'],expected,p['assets']),timeout=150)
        if record['kind']=='sdl':wait_log(capture.process,capture.artifacts/'game.log','field phase: turn=3 phase=2 ',timeout=30)
    record['native_round_banners'].append(round_witness(p,record['teleport_batches'][-1],3))
    shared.save_activity(capture,p,expected,save,guard,record,cp,'f1-82-count29-active',
        lambda disk,after,view:fixture.check_activity(p,disk,after,view,record['kind']))
    actual=state.decode_save(Path(record['completed_save']['snapshot']))
    if actual[0x12a3+29*80:0x30a3]!=before[0x12a3+29*80:0x30a3]:
        raise AssertionError('原Save count前缀以外的已拥有tail改变')
    record['capacity_domain']={'controlled_prefix':p['manifest']['controlled_prefix'],
        'natural_kills_executed':False,'formal_CONTINUE':True,'formal_active_Load_cancel_confirm':True,
        'formal_END_count':2,'first_END_count27_no82_Save':True,
        'fragment4_complete_ACK':len(record['resource_acknowledgements'])==len(p['story_pages']),
        'turn':3,'count':29,'state16':2,'observed_teleport_raw':[22,29,30],
        'inactive_xy_runtime_dumped':False,'inactive_SAV_tail_preserved':True,
        'whole_RGB_audio_timing_or_natural_campaign_claimed':False}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in('sdl','driver','dosbox','build-dir','snapshot-probe','bootstrap-config'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--save-fixture',type=Path,default=fixture.SOURCE/'s5-29-entry-active.SAV')
    parser.add_argument('--source-report',type=Path,default=fixture.SOURCE/'comparison.json')
    parser.add_argument('--run',action='store_true');args=parser.parse_args(argv)
    if args.run and(not args.build_dir or any(getattr(args,k)is None or not getattr(args,k).is_file()
        for k in('sdl','driver','dosbox','snapshot_probe','bootstrap_config'))):
        parser.error('--run需要当前正式App/driver/DOSBox/build-dir/snapshot-probe/bootstrap-config')
    p=prepare(args.root,args.save_fixture,args.source_report)
    late.snapshot_preflight(args,p)
    p['manifest']['preflight']['actual_C_import_checked']=p.get('snapshot_import_preflight',{}).get('passed')is True
    if args.bootstrap_config:p['source_evidence_paths'].append(args.bootstrap_config.resolve(strict=True))
    report=host.run_prepared(args,p,exercise,(29,),fixture.SCENARIO,
        ['natural kills/campaign','all animation phases or full RGB','runtime inactive29/30 RAM bytes','audio/PIC/absolute timing','Linux/macOS'],
        require_display_on=True)
    print(json.dumps({k:report[k]for k in('ok','status','scenario')},ensure_ascii=False))
    return int(args.run and not report['ok'])


if __name__=='__main__':raise SystemExit(main())
