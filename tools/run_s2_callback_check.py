#!/usr/bin/env python3
"""S2 callback70/35e5b三次palette加白的原版/SDL机制代表。

原版真实CONTINUE→物理败亡→mode2 WORD70，SDL只增加非shipping源图别名。
受控START扩至46人以满足AI41..45，hidden说话人97/68有显式原记录复制；
metadata.byte2=0是声明的空group边界，本例不验证追加构造或自然stage22绑定。
复用已有owned WGC、后台输入、END/UI Save整容器及来源守卫。
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image
from fd2_opening_oracle import build_pages, page_match
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import normalize
from run_original_item_list import measure_inventory_save
from run_original_load import require_source_unchanged
from run_g2_battle_check import series, sha
from static_data import audit_stage_data
from static_data.verify_unit_tables import source_range
import run_g4_common_event_check as host
import run_original_loot as base

SCENARIO = 'callback70-three-flashes'
ALIAS = 'S2 mechanism alias: stage.000.action.070 -> common.callback.070'


def prepare(root, scenario, **unused):
    if scenario != SCENARIO: raise ValueError('未知S2 callback机制代表')
    original=(root/'original_game').resolve()
    p=base.prepare(original,'loot-item');initial=decode_save(original/'START.SAV')
    source=bytearray(p['source']);source[2]=0;source[base.META+1]=46
    # 完整复制真实原raw4，未知字节不从零猜。新增全隐藏/不可AI行动，仍有正HP。
    template=bytearray(initial[base.UNITS+4*80:base.UNITS+5*80]);template[5]|=5
    template[0x31:0x34]=b'\xff\0\0'
    for raw in range(12,46):source[base.UNITS+raw*80:base.UNITS+(raw+1)*80]=template
    for raw,identity in ((39,68),(40,97)):
        source[base.UNITS+raw*80+7:base.UNITS+raw*80+9]=bytes((identity,identity))
    source[base.UNITS+11*80+0x31:base.UNITS+11*80+0x34]=bytes((2,70,0))
    source=base.checksum(source);items,_=base.item_table(original/'FD2.EXE')
    model=base.expected_state(source,'loot-story',items)
    immediate=bytearray(model['immediate'])
    for raw in range(41,46):immediate[base.UNITS+raw*80+0x34]&=0xf0
    immediate=base.checksum(immediate)
    from run_original_item_list import phase_end_model
    model.update(immediate=immediate,final=phase_end_model(immediate,items))
    identities=[source[base.UNITS+i*80+7] for i in range(46)]
    mapping={i:i for i in range(256)}
    for text in (0,4,68,97):mapping[text]=host.story_speaker(source,text)['unit_id']
    pages=[page for page in build_pages(original,p['assets'],stage_plan=((0,7),),
        identities_by_stage={0:identities},text_identities_by_stage={0:mapping}) if page['fragment'] in (5,6)]
    changes=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(initial,source)) if a!=b]
    allowed={2,base.META+1,*range(len(source)-4,len(source)),*range(base.UNITS,base.UNITS+46*80)}
    if any(row['offset'] not in allowed for row in changes):raise AssertionError('S2夹具越出声明写集')
    pointers=audit_stage_data.collect_internal_offset_fixups(original/'FD2.EXE')
    if pointers[0x51b91+70*4][2]!=0x35e5b:raise ValueError('callback70原表target不同')
    raw=source_range((original/'FD2.EXE').read_bytes(),0x35e5b,0x35ec1-0x35e5b)
    import hashlib
    p.update(source=source,encoded=encode(source),model=model,stories=pages,
        source_contract={'ok':True,'native_WGC_acceptance':False,
            'source':'original callback70 fixup and complete 35e5b entry bytes; separate full CFG/CPU receipts required',
            'entry_sha256':hashlib.sha256(raw).hexdigest(),'entry_length':len(raw),
            'domain_claim':'empty group, valid AI raw41..45, real stage0 text5/6, explicit mode2 mechanism'},
        manifest={'scenario':scenario,'controlled_fixture':True,'natural_history_claimed':False,
            'original_dispatch':'1aa1d(mode2, full WORD70) -> 35e5b -> 35b78 -> 11df2',
            'SDL_alias':ALIAS,'production_binding_added':False,'plaintext_patches':changes,
            'source_sha256':sha(original/'START.SAV'),'count':46,'metadata_effective_template_count':0,
            'new_records':'raw12..45 copy original START raw4, hidden+AI-ineligible; raw39 ID/text68, raw40 ID/text97',
            'empty_group_boundary':'group3/4/5 has no templates; not arrival/constructor evidence',
            'story_pages':[{k:page[k] for k in ('fragment','token_offset','reason','speaker_argument','portrait_id')} for page in pages]} )
    p['focused'],p['focus_reference']=base.focus_view(source,p['initial'],p['hero'])
    p['target_view'],_=base.focus_view(p['focused'],p['hero'],(8,14))
    return p


def story_state(state,p,fragment):
    for page in (row for row in p['stories'] if row['fragment']==fragment):
        speaker=host.story_speaker(state,page['speaker_argument'])
        if speaker['visible']:
            raw=speaker['raw_actor'];world=tuple(state[base.UNITS+raw*80:base.UNITS+raw*80+2])
            state,_=base.focus_view(state,tuple(state[base.META+5:base.META+7]),world)
    return state


def flash_gate(frames):
    groups=[];inside=False
    for row in frames:
        frame=normalize(Image.open(row['path']))
        white=all(lo>=252 for lo,hi in frame.getextrema())
        if white and not inside:groups.append([])
        if white:groups[-1].append({k:row[k] for k in ('index','sha256','wgc_timestamp_100ns')})
        inside=white
    if len(groups)!=3:raise AssertionError(f'应观察三段原palette全白反馈，实际{len(groups)}段')
    return {'ok':True,'white_runs':groups,'pixel_count_per_frame':64000,
        'channel_minimum':252,'nominal_200ms_not_absolute_timing_claimed':True,
        'full_intermediate_RGB_equivalence_claimed':False}


def exercise(capture,p,save,record,args):
    saved,stamp=save.read_bytes(),save.stat().st_mtime_ns;assets=p['assets'];source=p['source']
    def checkpoint(name,predicate,source_guard=True,**kwargs):
        value=wait_state(capture,name,predicate,record['checkpoints'],**kwargs)
        if source_guard:require_source_unchanged(save,saved,stamp)
        return value
    wait_title(capture,p['titles'][3][0]);capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    checkpoint('s2-continued',base.RoundOracle(assets,base.screen(source,p['initial'])).initial_browse,timeout=45)
    capture.key('Z');checkpoint('s2-actor0',lambda f:base.focus_match(f,p,p['focused']))
    capture.key('ENTER');checkpoint('s2-selection',lambda f:base.selection_match(f,p,p['focused']))
    capture.key('ENTER');checkpoint('s2-command',lambda f:base.command(f,p,0))
    capture.key('ENTER');checkpoint('s2-target',lambda f:base.selection_match(f,p,p['target_view'],11))
    capture.key('ENTER');record['physical_confirmations']=1
    state=base.positioned(p['model']['immediate'],(8,14),p['target_view'][base.META+3:base.META+5])
    first=[page for page in p['stories'] if page['fragment']==5]
    for i,page in enumerate(first):
        checkpoint(f's2-story5-{i:02d}',lambda f,page=page:page_match(f,page,assets),timeout=90)
        if i+1<len(first):capture.key('ENTER')
    state=story_state(state,p,5)
    # DAC扫描中的混合帧可能紧邻唯一完整白新帧；16ms保留60Hz相邻帧，
    # 不把原33ms丢弃的帧补造出来，也不放宽三段／全帧252门。
    with series(capture,record,interval_ms=16):
        capture.key('ENTER')
        page=next(page for page in p['stories'] if page['fragment']==6)
        checkpoint('s2-story6-00',lambda f:page_match(f,page,assets),timeout=90)
    record['palette_gate']=flash_gate(record['continuous_wgc']['frames'])
    second=[page for page in p['stories'] if page['fragment']==6]
    for i,page in enumerate(second):
        if i:checkpoint(f's2-story6-{i:02d}',lambda f,page=page:page_match(f,page,assets),timeout=60)
        capture.key('ENTER')
    # 三次135dd依原camera delta同步focus；均是stage0合法camera。
    for target in ((8,7),(4,7),(0,7)):
        old=state[base.META+3:base.META+5];focus=state[base.META+5:base.META+7]
        state=base.positioned(state,(focus[0]+target[0]-old[0],focus[1]+target[1]-old[1]),target)
    state=story_state(state,p,6)
    checkpoint('s2-callback-complete',base.quiet_focus(p,state,record,'s2-callback-complete'),timeout=90)
    state=base.navigate(capture,p,state,tuple(state[base.META+5:base.META+7]),(6,14),checkpoint,'s2-end-anchor')
    ui=base.RoundOracle(assets,base.screen(state,(6,14)))
    capture.key('ENTER');checkpoint('s2-end-parent',lambda f:ui.menu(f,0))
    capture.key('DOWN');checkpoint('s2-end-selected',lambda f:ui.menu(f,3))
    capture.key('ENTER');checkpoint('s2-end-yes',lambda f:ui.buttons(f,0));capture.key('ENTER')
    record['END_confirmations']=1
    final,_=base.focus_view(base.positioned(p['model']['final'],(6,14),state[base.META+3:base.META+5]),(6,14),p['hero'])
    checkpoint('s2-round2',base.quiet_focus(p,final,record,'s2-round2'),timeout=120)
    final=base.navigate(capture,p,final,p['hero'],(6,14),checkpoint,'s2-save-anchor')
    step=base.save_empty_step(p,final);base.require_save_enabled(final)
    measure_inventory_save(capture,assets,{'expected_source':final,'steps':[(step['key'],tuple(step['position']))]},
        save,saved,stamp,record,checkpoint,prefix='s2',measurement_key='whole_save')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--run',action='store_true');args=parser.parse_args();args.scenario=SCENARIO
    args.capture_defeat=False  # 本代表只采三次palette反馈；复用host的SDL启动分支仍需此显式配置。
    if args.run and any(getattr(args,n) is None or not getattr(args,n).is_file() for n in ('sdl','driver','dosbox')):
        parser.error('--run需要实际机制exe/driver/dosbox')
    report=host.run(args,prepare_case=prepare,exercise_case=exercise,alias_message=ALIAS,
        fixture_source=args.root/'src/tests/s2_callback_native_fixture.c',
        scope='S2 source callback70: three real palette-add flashes/story5+6/AI prefix/END/UI Save; explicit empty-group mechanism',
        excluded=['自然新stage绑定','constructor/arrival','全部palette中间RGB/绝对时序','RNG整轨','其它平台'])
    print(json.dumps({'ok':report['ok'],'status':report['status']},ensure_ascii=False))
    return 0 if not args.run or report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
