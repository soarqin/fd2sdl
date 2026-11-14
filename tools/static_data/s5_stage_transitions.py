#!/usr/bin/env python3
"""S5共享来源候选：继承S2完整80-owner门，新增读集与冷编排不开放玩法。

依据22253/22470/22547/22656完整Domain；所有帧、分支、等待留在冷图。
原始读集和产物只留本地，不复制DAT或生成第二份可编辑玩法定义。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import convert_stage_transitions as base
from convert_stage_callbacks import action, branch, cold_flow


SCHEMA = 'fd2-s5-transition-candidates-v1'
NATURAL_MONTAGE_SHA256 = 'eb63210f61b62676e040f7973d11cc84a7fa0c1c0b4e935945575b1df70f81e2'
NATURAL_MONTAGE_READSET_SHA256 = '4cd565bc6e1c245ab5e6bc1d7d2ee45901c47b987bcf92490d4378c184d02a46'


def digest(raw): return hashlib.sha256(raw).hexdigest()
def encode(value): return json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n'


def teleport_sequence():
    """22470→22547→22253提交/行擦除→22656；begin五参由原caller提供。"""
    a = action
    wipe = lambda count: [node for row in range(count)
                          for node in (a('teleport_wipe', row), a('delay', 10))]
    return [*[node for frame in range(114, 125)
              for node in (a('teleport_sprite', frame), a('delay_ticks', 1))],
            a('teleport_sound'), a('teleport_icon'),
            *[node for i in range(5, -1, -1)
              for node in (a('teleport_contract', i, i), a('delay', 10))],
            a('delay_ticks', 1), a('delay_ticks', 1), a('teleport_commit'),
            branch('teleport_top', [], wipe(18), wipe(24)),
            *[node for i in range(10)
              for node in (a('teleport_expand', i), a('delay_ticks', 1))],
            a('teleport_release')]


def teleport_helper():
    return cold_flow('stage.source.teleport', teleport_sequence())


def transition(dx, dy, radius, step):
    a = action
    return [a('bank80_validate'), a('transition_begin', dx, dy), a('transition_sfx'),
            *[node for i in range(9) for node in
              (a('transition_frame', 9-i, radius+i*step), a('delay', 5))],
            a('transition_release'), a('call_transition_finish'), a('bank80_validate')]


def pulse():
    return [*[node for i in range(64) for node in
              (action('palette_add', 0, 255, i), action('delay', 8))], action('delay', 400),
            *[node for i in range(62, -1, -1) for node in
              (action('palette_add', 0, 255, i), action('delay', 8))]]


def shade(values, wait=4, operation='palette_add'):
    return [node for value in values for node in
            (action(operation, 0, 255, value), action('delay', wait))]


def shake(count):
    return [action('cin_shake_prepare'), *[node for i in range(count) for node in
            (action('cin_shake_frame', i & 1), action('cin_present'), action('delay', 20))]]


def circle(actor, radius, step):
    return [action('cin_circle_prepare', actor),
            *[node for i in range(10) for node in
              (action('cin_circle_frame', actor, radius+i*step, i), action('cin_present'))],
            action('cin_circle_close'), action('refresh', 0)]


def group_flash(x, y, group):
    return [action('pan', x, y), action('append_group', group, 0, 0), action('delay', 300),
            action('palette_add', 0, 255, 255), action('delay', 200),
            action('palette_add', 0, 255, 0), action('refresh', 0), action('delay', 400)]


def teleport(actor, x, y, draw_x=None, draw_y=None, *, focus=True):
    prefix = [action('focus_cell', x, y)] if focus else []
    return prefix + [action('teleport_begin', actor, x, y,
                            x if draw_x is None else draw_x, y if draw_y is None else draw_y),
                     action('call_teleport')]


def layout(image, first, last, arrays, special, sx, sy, direction, cx, cy):
    # 数组仍只读原EXE源；复用S2已独立原233c6/C核过的同一typed lowering。
    values = [{'source_va': hex(va), 'raw_hex': image[va:va+last-first+1].hex(), 'first_raw': first}
              if va >= 0x50000 else va for va in arrays]
    args = [*values, first, last, special, sx, sy, direction, cx, cy]
    event = {'kind': 'call', 'op': 'layout', 'args': args,
             'source_va': '0x233c6:complete_original_layout', 'force': 0, 'fade': 0}
    graph, _ = base.lower_events([event], 0, 'candidate')
    return [(n['op'], n['args']) for n in graph['nodes'] if n['op'] != 'end']


def sheet101():
    a = action
    # 原APP页前64000B不是独立scratch；slot5明确借真实App backing。
    copy = lambda src, dst: a('cin_copy', src, dst, 0, 0, 320, 200, 320, 320)
    frames = lambda first, last, water: [node for i in range(first, last) for node in
        [copy(1, 5), a('cin_sheet_blit', 0, i, 5, 0, 0, 320), copy(5, 0),
         a('cin_present'), *([a('cin_water')] if water else []), a('delay_ticks', 3), a('clear_keys')]]
    return [a('pan', 14, 8), a('cin_page_alloc', 1, 64000), copy(0, 1),
            a('cin_sheet_open', 0, 34), a('cin_sheet_blit', 0, 0, 1, 0, 0, 320),
            *frames(1, 69, True), a('cin_animation', 0, 15, 0),
            a('palette_add', 0, 255, 63), a('delay', 100),
            a('palette_add', 0, 255, 0), a('delay', 500), *frames(69, 101, False),
            a('cin_page_free', 1), a('cin_sheet_close', 0)]


class FixedGraph:
    """只在生成期展开原有界序列；固定credits外层回边不新增执行语言。"""
    def __init__(self, identity):
        self.identity = identity
        self.nodes = [{'id':'done','op':'end','args':[],'on':{}},
                      {'id':'failed','op':'fail','args':[],'on':{}}]

    def node(self, op, args, on):
        label = f'n{len(self.nodes):05d}'
        self.nodes.append({'id':label,'op':op,'args':list(args),'on':dict(on)})
        return label

    def test(self, op, args, yes, no):
        return self.node(op, args, {'yes':yes,'no':no,'error':'failed'})

    def sequence(self, sequence, following='done'):
        for item in reversed(sequence):
            if isinstance(item, dict):
                following = self.test(item['op'],item['args'],
                    self.sequence(item['yes'],following),self.sequence(item['no'],following))
            else:
                op,args = item
                following = self.node(op,args,{'next':following,'error':'failed'})
        return following

    def finish(self, entry):
        by_id = {n['id']:n for n in self.nodes}; todo, seen = [entry],set()
        while todo:
            pc=todo.pop()
            if pc in seen: continue
            if pc not in by_id: raise ValueError('fixed source graph has dangling edge: '+pc)
            seen.add(pc);todo.extend(by_id[pc]['on'].values())
        nodes = [dict(n,views=['logic','present']) for n in reversed(self.nodes) if n['id'] in seen]
        return {'id':self.identity,'entry':entry,'nodes':nodes}


def copy_page(source, destination, *, source_offset=0, destination_offset=0,
              width=320, height=200, source_stride=320, destination_stride=320):
    return action('cin_copy',source,destination,source_offset,destination_offset,
                  width,height,source_stride,destination_stride)


def notice(portrait, fragment):
    a=action
    # 31bdf→1956b/26996完整滑框；共享1974c/16f04尾没有等待。
    return [a('clear_keys'),a('cin_notice_open',portrait),
            *[n for phase in range(5,-1,-1) for n in (a('cin_notice_slide',phase),a('cin_present'))],
            a('clear_keys'),a('cin_notice_text',fragment),a('cin_notice_portrait',0),a('cin_notice_wait',0),
            *[n for phase in range(1,6) for n in (a('cin_notice_slide',phase),a('cin_present'))],
            a('cin_notice_close'),a('clear_keys')]


def card_frames(first, stop):
    a=action
    card=[]
    for frame in range(first,stop):
        card.extend([a('cin_credits_card',frame),copy_page(1,0),a('cin_present'),a('delay_ticks',1),
                     branch('branch_cin_input',[],[a('clear_keys'),a('cin_credits_skip_set',1)])])
    return card


def credits_first():
    """31c49两卡共同的原有完整前缀；不藏循环，返回前没有fade/close。"""
    a=action;g=FixedGraph('stage.cinematic.card_first')
    card_entry=g.sequence([a('cin_credits_card_prepare'),*card_frames(0,220)])
    following=card_entry
    # BYTE intro_count最多255；每个NO直接到卡片，不能继续读取未存在的part。
    for part in range(254,-1,-1):
        yes=g.sequence([copy_page(2,1),a('cin_credits_part',part),copy_page(1,0),
                        a('cin_present'),a('cin_credits_part_wait',part)],following)
        following=g.test('branch_cin_credits_part',[part],yes,card_entry)
    intro=[a('cin_credits_skip_set',0),
           *[n for phase in range(8,-1,-1) for n in
             (a('cin_credits_intro',phase),a('palette_subtract',0,255,6*phase))],
           a('cin_credits_intro_finish'),a('cin_credits_idle_reset'),
           *[n for _ in range(20) for n in (copy_page(2,1),a('cin_credits_idle',1),
               copy_page(1,0),a('cin_present'),a('delay_ticks',1))]]
    return g.finish(g.sequence(intro,following))


def credits_card(identity,frames):
    a=action
    return compile_program(identity,[a('call_cinematic_card_first'),
        *(card_frames(220,440) if frames==440 else []),a('call_fade_out'),a('cin_credits_actor_close')])


def credits_helper():
    a=action;g=FixedGraph('stage.cinematic.credits')
    cleanup=g.sequence([a('cin_page_free',2),a('cin_page_free',3),
                        a('cin_credits_assets_close'),a('cin_page_free',1)])
    last_repeat=g.test('branch_cin_credits_skip',[],'last_card',cleanup)
    last=g.sequence([a('cin_credits_epoch'),a('cin_credits_actor',0),
                     a('call_cinematic_card_last')],last_repeat)
    g.nodes[-1]['id']='last_card'  # 只此原j=0重播回边；固定命名由生成器检查。
    if last != g.nodes[-1]['id']: last='last_card'
    following=last
    for outer in range(1,32):
        after=g.test('branch_cin_credits_skip',[],last,following)
        yes=g.sequence([a('cin_credits_epoch'),a('cin_credits_actor',outer),
                        a('call_cinematic_card_normal')],after)
        following=g.test('branch_cin_roster_count',[outer],yes,following)
    cards=g.test('branch_cin_roster_count',[0],following,cleanup)
    dark=40;roll=[]
    for i in range(500):
        roll.extend([a('palette_subtract',0,255,dark),copy_page(4,0,source_offset=320*i),
                     a('cin_present'),a('delay_ticks',1)])
        if i<200 and dark and i%5==0: dark-=1
        if i>300 and i%5==0: dark+=1
    prefix=[a('credits_setup'),a('cin_credits_blink_reset'),a('cin_page_alloc',4,224000),
            a('cin_page_clear',4,0),a('cin_credits_roll_text'),*roll,a('cin_page_free',4),
            a('cin_page_alloc',1,128000),a('cin_page_alloc',2,64000),a('cin_page_alloc',3,64000),
            a('cin_credits_assets_open'),a('cin_image_blit',56,2,0,0,320),a('bgm',4)]
    return g.finish(g.sequence(prefix,cards))


def montage_round():
    """2ebe1非split原part循环；原DAT给出存在/marker/duration，不抄字段值。"""
    a=action;g=FixedGraph('stage.cinematic.montage_round');following='done'
    for part in range(21,-1,-1):
        # 原override1在整part（包括duration0）之后才检查最后marker返回。
        stop=g.test('branch_cin_montage_half',[1],
            g.test('branch_cin_montage_last_marker',[part],'done',following),following)
        ticks=stop
        for tick in range(9,-1,-1):
            frame=g.sequence([a('cin_montage_frame',part),a('cin_present'),a('delay_ticks',1)],ticks)
            ticks=g.test('branch_cin_montage_tick',[part,tick],frame,stop)
        cue=g.sequence([a('cin_montage_cue',part)],ticks)
        hit=g.sequence([branch('branch_cin_montage_half',[0],
            [a('unit_field_write',1,1,18,0,63680)],
            [a('unit_field_write',0,0,18,0,63680)]),a('cin_montage_hit')],cue)
        marker=g.test('branch_cin_montage_marker',[part],hit,cue)
        following=g.test('branch_cin_montage_part',[part],marker,'done')
    return g.finish(following)


def montage_pair(first,second,scene):
    """31529唯一20组caller有效域；first重复不重置idle/抖动，第二RNG仍消费。"""
    a=action
    return [a('unit_field_write',0,0,5,0,0 if first>=76 else 2),
        a('unit_field_write',0,0,6,0,first),
        a('unit_field_write',1,1,5,0,0 if second>=76 else 2),
        a('unit_field_write',1,1,6,0,second),a('cin_montage_open',first,second,scene),
        a('call_fade_out'),*[n for phase in range(8,-1,-1) for n in
            (a('cin_montage_intro',phase),a('palette_subtract',0,255,6*phase))],
        a('cin_montage_intro_finish'),a('cin_montage_half',0),
        branch('branch_cin_montage_rng',[3],[a('call_cinematic_montage_round')]),
        a('call_cinematic_montage_round'),a('cin_montage_half',1),
        branch('branch_cin_montage_rng',[3],[]),a('call_cinematic_montage_round'),
        a('cin_montage_close')]


def ending_program(image):
    a=action
    def by_stage(yes,no): return branch('branch_cin_stage',[26],yes,no)
    def notices(values): return [n for portrait,fragment in values for n in notice(portrait,fragment)]
    prefix=[a('cin_page_alloc',1,128000),a('cin_page_alloc',2,64000),a('cin_page_clear',2,0),
        a('cin_sheet_open',0,54),a('cin_sheet_blit',0,0,2,0,0,320),a('call_fade_out'),
        copy_page(2,0),a('call_fade_in'),a('delay',1000),a('cin_animation',2,100,0),
        a('palette_add',0,255,63),copy_page(2,0),a('cin_sheet_blit',0,9,0,0,0,320),
        *shade(range(63,-1,-1)),a('delay',2000),
        by_stage(notices([(4,17)]),notices([(37,2),(21,3),(26,4),(105,5),(32,6)])),
        a('delay',500),*[n for _ in range(3) for n in [*shade(range(63,-1,-1)),a('delay',200)]],
        *[n for frame in range(12,109) for n in
          (a('cin_sheet_blit',0,frame,0,0,0,320),a('cin_present'),a('delay',20))],copy_page(2,0),
        by_stage(notices([(21,18),(24,19),(26,20)]),notices([(45,7)])),a('delay',2000)]
    x1,x2=290,80
    for n in range(40):
        prefix.extend([copy_page(2,1,destination_offset=160,destination_stride=640),
            a('cin_sheet_blit',0,n%4+1,1,x1,0,640),a('cin_sheet_blit',0,n%4+5,1,x2,0,640),
            a('delay',20),copy_page(1,0,source_offset=160,source_stride=640),a('cin_present')])
        x2+=2;x1-=4 if n<25 else 2
    prefix.extend([copy_page(2,1),a('cin_sheet_blit',0,1,1,0,0,320),a('cin_sheet_blit',0,5,1,0,0,320),
        copy_page(1,0),a('cin_present'),
        by_stage(notices([(32,21),(36,22),(32,23)]),notices([(32,8),(36,9)]))])
    for i in range(200):
        prefix.extend([copy_page(2,1),a('cin_sheet_blit',0,i%4+1,1,0,0,320),
            a('cin_sheet_blit',0,i%4+5,1,0,0,320),a('delay',20),copy_page(1,0),
            a('palette_subtract',0,255,max(0,i-135))])
    # BASE没有原free；此处在最后一次源读取后现代owned清理，独立标注不充原ABI。
    prefix.extend([a('cin_page_free',1),a('clear_screen'),a('cin_page_free',2),a('cin_sheet_close',0),
        a('call_cinematic_credits'),a('clear_screen'),a('bgm',-1),a('delay_ticks',50),
        a('cin_image_blit',60,0,0,0,320),a('call_fade_in'),a('bgm',18),a('delay_ticks',80),
        a('call_fade_out'),a('clear_screen'),a('cin_sheet_open',1,58),
        a('cin_palette_validate',57),a('cin_palette_save')])
    # 三张20B只读原EXE表，不成为第二份可编辑表。
    for i in range(20):
        if i: prefix.extend([a('cin_palette_restore'),a('cin_palette_save')])
        prefix.extend([*montage_pair(image[0x526E9+i],image[0x526FD+i],image[0x52711+i]),
            a('cin_palette_load',57),a('palette_subtract',0,255,0),a('delay_ticks',20),
            a('cin_sheet_blit',1,i,0,0,0,320),a('cin_present'),a('delay_ticks',78),
            a('call_fade_out'),a('clear_screen')])
    prefix.extend([a('cin_palette_restore'),a('cin_sheet_close',1),a('delay_ticks',50),
                   a('cin_image_blit',59,0,0,0,320),a('call_fade_in')])
    return prefix


def cinematic_helpers(image):
    return [compile_program('stage.cinematic.ending',ending_program(image)),credits_helper(),
            credits_first(),credits_card('stage.cinematic.card_normal',220),credits_card('stage.cinematic.card_last',440),
            montage_round()]


def caller_programs(image):
    a = action
    text = lambda n: a('text', n)
    write = lambda first, last, field, value: a('unit_field_write', first, last, field, 0, value)
    has100 = lambda yes, no=(): branch('branch_item_occurrences', [0, 15, 100, 100, 4, 0], yes, no)
    reset = [a('reset_directions'), a('sleep_ms', 20)]
    p = {}
    p['stage.source.entry.022'] = [a('call_init'), write(0, 15, 4, 1), a('pan', 14, 32),
        *transition(6, 5, 10, 8),
        *[branch('branch_actor', [raw, 4, 1, 0], [write(raw, raw, 4, 0), write(raw, raw, 2, 2)])
          for raw in range(16)], a('refresh', 0), a('palette_add', 0, 255, 0), a('delay', 500),
        text(0), a('pan', 14, 29), a('move', 68, 0), text(1), a('move', 69, 0), text(2),
        a('move', 70, 0), text(3), a('display_mode', 0), a('pan', 14, 13),
        a('append_group', 1, 0, 0), a('delay', 200), a('palette_add', 0, 255, 255),
        a('delay', 100), a('refresh', 0), a('palette_add', 0, 255, 0), a('delay', 500),
        write(16, 17, 2, 2), text(4), *reset, a('focus_actor', 0)]
    p['stage.source.entry.024'] = [a('call_init'), a('cin_audio_open', 88), a('pan', 5, 0),
        text(1), a('cin_work_clear', 0),
        *[node for _ in range(3) for node in [a('cin_audio_play', 1), *shake(20), a('delay', 600)]],
        a('cin_audio_play', 1), *shake(60), text(2), a('focus_actor', 0), a('cin_audio_close')]
    p['stage.source.entry.026'] = [a('call_init'), a('pan', 9, 49), a('move', 76, 0), text(0),
        has100([text(3)]), text(4), a('pan', 9, 49), *transition(0, 3, 2, 2),
        a('palette_add', 0, 255, 0), text(5), *transition(0, 0, 2, 2),
        a('palette_add', 0, 255, 0), a('move', 81, 0), text(6), *transition(2, 0, 2, 2),
        a('palette_add', 0, 255, 0), text(7), *reset, a('focus_actor', 0)]
    p['stage.source.entry.027'] = [a('call_init'), write(0, 19, 4, 1), a('pan', 29, 15),
        *transition(6, 5, 10, 8),
        *[branch('branch_actor', [raw, 4, 1, 0], [write(raw, raw, 4, 0)]) for raw in range(20)],
        a('refresh', 0), a('palette_add', 0, 255, 0), a('delay', 500),
        a('move', 85, 0), a('move', 85, 0), a('move', 85, 0), *reset, text(0),
        a('display_mode', 0), *group_flash(0, 16, 6), *group_flash(7, 16, 7),
        a('display_mode', 1), a('focus_actor', 0)]
    p['stage.source.entry.029'] = [a('call_init'), a('move', 87, 0), a('pan', 16, 19),
        text(0), a('display_mode', 0), a('pan', 16, 1),
        *teleport(21, 21, 5), *teleport(22, 23, 5), *teleport(23, 20, 5), *teleport(24, 24, 5),
        text(1), *pulse(), text(2), a('display_mode', 0), a('pan', 16, 14),
        *teleport(24, 22, 18), *teleport(25, 21, 18), *teleport(26, 23, 18),
        a('display_mode', 1), *reset, a('focus_actor', 0)]
    p['stage.source.victory.020'] = [
        *layout(image, 0, 24, (0x52228, 0x52241, 0x5225A), 25, 23, 14, 1, 14, 10), text(5),
        branch('branch_item_occurrences', [0, 15, 209, 214, 0, 6],
            [a('event_item_consume', 0, 15, 209, 214), a('grant_party_item', 100), text(7),
             a('display_mode', 0), a('move', 63, 0), text(8), a('display_mode', 0),
             a('move', 64, 0), text(9), *sheet101(), text(10)], [text(6)]),
        a('roster_append', 24), a('roster_append', 23), a('roster_sync'), a('next_stage', 21)]
    p['stage.source.victory.021'] = [
        *layout(image, 0, 15, (0x52273, 0x52283, 0x52293), 72, 22, 25, 2, 16, 18),
        text(4), a('move', 65, 0), text(5), a('pan', 16, 16), a('move', 66, 0),
        text(6), a('pan', 16, 14), *transition(0, 3, 10, 8), a('delay', 500),
        a('cin_page_clear', 0, 255), a('call_fade_out'), a('clear_screen'),
        a('roster_sync'), a('next_stage', 22)]
    p['stage.source.victory.022'] = [
        *layout(image, 0, 16, (0x522A3, 0x522B4, 0x522C5), 17, 21, 21, 2, 14, 14),
        has100([text(8), a('roster_append', 22)], [text(9), a('display_mode', 0), a('move', 71, 0)]),
        branch('branch_roster_text', [18], [text(10), a('display_mode', 0), a('move', 72, 0),
            write(17, 17, 4, 1), text(11)],
            [branch('branch_turn', [2, 15], [text(13), a('roster_append', 19)],
             [text(12), a('display_mode', 0), a('move', 72, 0), write(17, 17, 4, 1)])]),
        a('roster_sync'), a('next_stage', 23), text(14), a('delay', 400), *circle(1, 15, 10),
        *shake(30), text(15), a('delay', 400), *circle(1, 15, 10), *shake(30),
        text(16), a('delay', 400), *circle(1, 30, 16), *shade(range(0, 64, 2)),
        a('map_replace', 69, 23), a('map_reset_cells'), a('background_rebuild'), a('pan', 14, 29),
        a('palette_add', 0, 255, 0), a('move', 73, 0), a('pan', 14, 14),
        a('move', 73, 0), a('move', 73, 0), text(17)]
    p['stage.source.victory.023'] = [text(2), a('display_mode', 0),
        *[node for speed in range(2, 10) for node in
          [a('background_scroll', speed), *[leaf for _ in range(30) for leaf in
                                           (a('refresh', 1), a('delay_ticks', 1))]]],
        text(3), a('display_mode', 0),
        *[node for speed in range(10, 15) for node in
          [a('background_scroll', speed), *[leaf for k in range(12) for leaf in
            (a('palette_subtract', 0, 255, (speed-10)*12+k), a('refresh', 0), a('delay_ticks', 1))]]],
        a('clear_screen'), a('roster_sync'), a('next_stage', 24)]
    # 25052的每级无内部等待；caller各次等待不能混进渐变helper。
    ramp = lambda step: [a('palette_add', 0, 255, value) for value in range(80, -1, -step)]
    present26 = [text(9), a('display_mode', 0), a('move', 83, 0), text(10), a('display_mode', 0),
        a('pan', 9, 8), a('move', 84, 0), text(11), *ramp(5), text(12), *ramp(4), a('delay', 500),
        *ramp(3), a('delay', 250), *ramp(2), a('delay', 100), *ramp(2), a('delay', 50), *ramp(2),
        *transition(0, -1, 10, 10), a('delay', 500), a('cin_page_clear', 0, 255),
        a('call_fade_out'), a('clear_screen'), a('roster_sync'), a('next_stage', 27), a('roster_restore')]
    absent26 = [text(13), a('display_mode', 0), a('move', 84, 0), text(14), a('display_mode', 0),
        a('move', 82, 0), text(15), a('display_mode', 0), a('flash_actor', 1),
        *teleport(1, 255, 255, -1, -1, focus=False), text(16), a('display_mode', 0),
        a('roster_restore'), a('call_cinematic_ending'), a('terminal_wait')]
    p['stage.source.victory.026'] = [write(0, 15, 4, 0),
        *layout(image, 0, 15, (0x52306, 0x52316, 2), 0, 15, 0, 0, 9, 8),
        text(8), a('display_mode', 0), a('move', 82, 0), has100(present26, absent26)]
    p['stage.source.victory.028'] = [text(10), a('unit_hp_zero_tail', 20), a('defeated_finalize'),
        write(20, 20, 6, 126), write(20, 20, 7, 126), text(11), a('append_group', 9, 0, 0),
        a('pan', 9, 8), a('focus_cell', 15, 10), *teleport(-4, 15, 10, focus=False),
        text(12), a('display_mode', 0), *shake(20), a('delay', 600), *shake(20), a('delay', 600),
        *shake(20), text(13), *shake(20), a('delay', 200), *shake(20), a('delay', 200),
        *shake(20), text(14), *shake(20), a('delay', 200), *shake(20), a('delay', 100),
        *shake(40), a('delay', 200), *pulse(), a('delay', 300), *pulse(), a('delay', 300),
        *pulse(), a('delay',300), text(15), *shade(range(64)), a('clear_screen'), a('delay', 800),
        *shade(range(62, -1, -1)), a('roster_sync'), a('next_stage', 29)]
    p['stage.source.victory.029'] = [
        *layout(image, 0, 19, (0x52327, 0x5233B, 0x5234F), 0, 0, 0, 0, 16, 18),
        text(9), a('display_mode', 0), a('move', 88, 0), text(10), a('pan', 16, 18),
        a('focus_cell', 22, 23), *transition(0, 1, 10, 8), a('roster_restore'), a('next_stage', 30),
        a('roster_restore'), a('display_mode', 0), a('credits_setup'), a('view_set', 11, 5, 11, 5),
        a('refresh', 1), *shade(range(62, -1, -1)),
        *[node for _ in range(40) for node in (a('refresh', 0), a('delay_ticks', 1))],
        text(0), a('display_mode', 0), a('pan', 11, 12), a('move', 89, 0), text(1),
        a('call_cinematic_ending'), a('terminal_wait')]
    p['common.callback.082'] = [a('pan', 16, 1), a('story_source', 9), a('display_mode', 0),
        a('pan', 16, 14), *teleport(-1, 22, 18),
        branch('branch_event_byte', [16, 0, 4], [a('unit_ai_write', 20, 20, 11)],
               [a('append_group_source', 5, 0), a('unit_ai_write_source', 6, 0),
                *teleport(-2, 21, 18), *teleport(-3, 23, 18)]), a('display_mode', 1)]
    return p


def compile_program(identity, program):
    flow = cold_flow(identity, program)
    for node in flow['nodes']:
        if node['op'] == 'terminal_wait': node['on'] = {}
        if identity.startswith('stage.'): node['views'] = ['logic', 'present']
    by_id = {node['id']: node for node in flow['nodes']}; pending, seen = [flow['entry']], set()
    while pending:
        pc = pending.pop()
        if pc in seen: continue
        seen.add(pc); pending.extend(by_id[pc]['on'].values())
    flow['nodes'] = [node for node in flow['nodes'] if node['id'] in seen]
    return flow


def trace_program(program, choices=()):
    """仅供模板/反例检查；真正expected必须来自独立原指令执行。"""
    result, choices = [], iter(choices)
    def visit(sequence):
        for item in sequence:
            if isinstance(item, dict):
                choice = next(choices)
                result.append((item['op'], item['args']))
                visit(item['yes'] if choice else item['no'])
            else: result.append(item)
    visit(program)
    return result


def required_operations(flows, document):
    declared = document['operations']
    required = {node['op'] for flow in flows for node in flow['nodes']}
    return {'required': sorted(required), 'not_registered': sorted(required - declared.keys()),
            'consumer_semantics_proven': False}


def operation_patch(flows, documents, cinematic_contract):
    args = lambda *values: [{'name': name, 'min': low, 'max': high} for name, low, high in values]
    extra = {
        'bank80_validate': [], 'transition_begin': args(('dx',-255,255),('dy',-255,255)),
        'transition_sfx': [], 'transition_frame': args(('lut',1,9),('radius',0,255)),
        'transition_release': [], 'call_transition_finish': [], 'call_teleport': [],
        'focus_cell': args(('x',0,255),('y',0,255)),
        'teleport_begin': args(('actor_or_source',-4,95),('destination_x',0,255),
                               ('destination_y',0,255),('draw_x',-1,255),('draw_y',-1,255)),
        'teleport_sprite': args(('frame',114,124)), 'teleport_sound': [], 'teleport_icon': [],
        'teleport_contract': args(('lut',0,5),('row_multiplier',0,5)),
        'teleport_commit': [], 'teleport_top': [], 'teleport_wipe': args(('row',0,23)),
        'teleport_expand': args(('lut',0,9)), 'teleport_release': [],
        'delay_ticks': args(('ticks',0,65535)), 'credits_setup': [], 'roster_restore': [],
        'defeated_finalize': [], 'flash_actor': args(('actor',0,95)),
        'background_scroll': args(('speed',0,255)), 'terminal_wait': [],
        'bgm': args(('track',-1,255)),
        'unit_ai_write_source': args(('source',5,8),('behavior',0,255)),
        'cin_water': [], 'cin_animation': args(('animation',0,255),('delay_ms',0,65535),('check_input',0,1)),
    }
    result = {}
    manifest = documents['manifest']
    for domain, domain_flows in flows.items():
        current = documents[domain]['operations']; operations, ports = {}, {}
        used = {node['op'] for flow in domain_flows for node in flow['nodes']}
        for name in sorted(used-current.keys()):
            if name in cinematic_contract['operations']:
                contract = cinematic_contract['operations'][name]
                operations[name] = {'args': contract['args']}; ports[name] = [sorted(contract['ports'])]
            elif name in extra:
                operations[name] = {'args': json.loads(json.dumps(extra[name]))}
                if domain == 'common_event' and name == 'teleport_begin':
                    operations[name]['args'][0]['min'] = -3
                ports[name] = [[]] if name == 'terminal_wait' else [
                    ['error','no','yes'] if name == 'teleport_top' else ['error','next']]
            elif name in documents['common_event']['operations']:
                operations[name] = documents['common_event']['operations'][name]
                observed = {tuple(sorted(node['on'])) for flow in domain_flows for node in flow['nodes'] if node['op']==name}
                ports[name] = [list(value) for value in sorted(observed)]
            else: raise ValueError('candidate operation has no finite consumer ABI: '+name)
        # S5 ANI有限域只有0/2；schema06把旧粗范围255收窄2，1另由generator/consumer拒。
        if 'cin_animation' in used:
            wanted={'args':cinematic_contract['operations']['cin_animation']['args']}
            if current.get('cin_animation')!=wanted:operations['cin_animation']=wanted
        old_ports = manifest['flow_contracts'][domain+'.json']['ports']
        for name in used:
            variants = {tuple(sorted(value)) for value in old_ports.get(name, [])}
            for flow in domain_flows:
                nodes = {node['id']:node for node in flow['nodes']}
                for node in flow['nodes']:
                    if node['op'] != name: continue
                    if 'error' in node['on'] and nodes[node['on']['error']]['op'] != 'fail':
                        raise ValueError('new source error edge may only terminate at fail')
                    variants.add(tuple(sorted(node['on'])))
            ports[name] = [list(value) for value in sorted(variants)]
        merged = {**current, **operations}
        result[domain] = {'operations': operations, 'flows': domain_flows, 'bindings': {},
            'manifest_patch': {'ports': ports, 'required_ids_add': [flow['id'] for flow in domain_flows],
                'operation_abi_sha256': digest(json.dumps(merged,sort_keys=True).encode())}}
    return result


def source_package(base_path, image_path, extensions):
    paths = [Path(base_path), Path(image_path), *map(Path, extensions)]
    snapshots = {path: path.read_bytes() for path in paths}
    if digest(snapshots[paths[0]]) != base.S2_DOMAIN_SHA256:
        raise ValueError('frozen S2 complete Domain identity differs')
    documents = [json.loads(snapshots[path], object_pairs_hook=base.unique_keys) for path in paths[2:]]
    readset = json.loads(snapshots[paths[0]], object_pairs_hook=base.unique_keys)
    instructions, refs, receipt = base.extend_source_index(readset, snapshots[paths[1]], documents)
    return readset, instructions, refs, receipt, snapshots


def natural_montage_source(path,readset_path):
    """固定原caller读集资格；只闭合63680来源，不替代本次C/原生门。"""
    path=Path(path);raw=path.read_bytes()
    if digest(raw)!=NATURAL_MONTAGE_SHA256:
        raise ValueError('natural montage original caller proof identity differs')
    report=json.loads(raw,object_pairs_hook=base.unique_keys)
    if (report.get('schema')!='fd2-s5-montage-natural-stack-v1' or
            report.get('image_sha256')!=base.IMAGE_SHA256 or
            report.get('original_intro_vectors')!=60 or report.get('derived_word_body_vectors')!=80 or
            len(report.get('connected',[]))!=40):
        raise ValueError('natural montage original caller proof domain differs')
    readset_path=Path(readset_path);readset_raw=readset_path.read_bytes()
    if digest(readset_raw)!=NATURAL_MONTAGE_READSET_SHA256:
        raise ValueError('natural montage corrected complete readset identity differs')
    snapshots={path:raw,readset_path:readset_raw}
    for name,expected in report['source_sha256'].items():
        source=Path(name);data=source.read_bytes();snapshots[source]=data
        if digest(data)!=expected:raise ValueError('natural montage original source changed')
    # CPU03以domain07重新执行。CPU02/domain01及37040误入口红件保留，不能改签。
    return [readset_path],snapshots


def build(base_path, image_path, extensions, cinematic_contract_path, montage_source_proof,montage_readset):
    additional,proof_inputs=natural_montage_source(montage_source_proof,montage_readset)
    extensions=list(dict.fromkeys([*map(Path,extensions),*additional]))
    readset, instructions, refs, receipt, snapshots = source_package(base_path, image_path, extensions)
    snapshots.update(proof_inputs)
    image = snapshots[Path(image_path)]
    cinematic_raw = Path(cinematic_contract_path).read_bytes()
    snapshots[Path(cinematic_contract_path)] = cinematic_raw
    cinematic = json.loads(cinematic_raw,object_pairs_hook=base.unique_keys)
    if cinematic.get('schema') not in ('fd2-s5-cinematic-op-contract-v1','fd2-s5-cinematic-op-contract-v3',
            'fd2-s5-cinematic-op-contract-v4','fd2-s5-cinematic-op-contract-v5','fd2-s5-cinematic-op-contract-v6',
            'fd2-s5-cinematic-op-contract-v7'):
        raise ValueError('cinematic finite operation contract missing')
    programs = caller_programs(image)
    domains = {}
    for name in ('stage','common_event'):
        path = base.ROOT/f'data/static/flows/{name}.json'
        snapshots[path] = path.read_bytes()
        domains[name] = json.loads(snapshots[path],object_pairs_hook=base.unique_keys)
    for path in (Path(__file__),Path(base.__file__),base.ROOT/'tools/static_data/convert_stage_callbacks.py',base.ROOT/'data/static/manifest.json'):
        snapshots[path] = path.read_bytes()
    domains['manifest']=json.loads(snapshots[base.ROOT/'data/static/manifest.json'],object_pairs_hook=base.unique_keys)
    flows = [teleport_helper(), *cinematic_helpers(image), *[compile_program(identity, program) for identity,program in programs.items()
                                 if identity.startswith('stage.')]]
    common_flows = [compile_program(identity,program) for identity,program in programs.items()
                    if identity.startswith('common.')]
    patch = operation_patch({'stage': flows, 'common_event': common_flows}, domains, cinematic)
    cfg = {}
    for entry in (0x22253, 0x22470, 0x22547, 0x22656, 0x33F78, 0x362E8):
        observed = base.recover_caller_cfg(entry, instructions, refs)
        cfg[hex(entry)] = observed
    formal = domains['stage']
    rows = []
    binding_index = {row['source_id']:row for row in readset['bindings']}
    for identity, program in programs.items():
        if identity.startswith('common.'):
            row = {'source_id':'dispatch:event_callback:082','target_va':'0x362e8','slot':82}
        else:
            _,_,kind,slot = identity.split('.')
            row = binding_index[f'dispatch:stage_{kind}:{slot}']
        domain = 'common_event' if identity.startswith('common.') else 'stage'
        flow = next(f for f in (common_flows if domain=='common_event' else flows) if f['id']==identity)
        caller = base.recover_caller_cfg(int(row['target_va'],16), instructions, refs)
        rows.append({**row,'definition_domain':domain,'definition_id':identity,
                     'definition_sha256':digest(encode(flow).encode()),'disposition':'converted_candidate',
                     'new_runtime_binding':False,'original_caller_trace_verified':False,
                     'source_CFG_complete':caller['all_direct_caller_edges_recovered'],'caller_cfg':caller,
                     'required_independent_gates':['full original caller trace and each source condition','actual C finite consumer','formal binding/resource closure','few original/native representatives']})
    report = {'schema': SCHEMA, 'source_index': receipt,
              'source_inputs': {'base_readset':str(base_path),'image':str(image_path),
                  'extensions':list(map(str,extensions)),'cinematic_contract':str(cinematic_contract_path),
                  'montage_source_proof':str(montage_source_proof),'montage_readset':str(montage_readset)},
              'input_sha256': {str(path): digest(raw) for path, raw in snapshots.items()},
              'flows': flows, 'source_bindings': {}, 'shared_helper_ids': ['stage.source.teleport',
                  'stage.cinematic.ending','stage.cinematic.credits','stage.cinematic.card_first',
                  'stage.cinematic.card_normal','stage.cinematic.card_last','stage.cinematic.montage_round'],
              'rows':rows,'s5_extension':patch,
              'helper_contract': {'stage.source.teleport': {
                  'caller_prefix': 'teleport_begin[selector,dst_x,dst_y,draw_x,draw_y]',
                  'fixed_call': 'call_teleport[]', 'original_entries': ['0x22253','0x22470','0x22547','0x22656'],
                  'preserves': 'Game/SAV/RNG owner; commit changes xy then prepares mask without present; ERROR/HOST retain arrived prefix',
                  'precondition': 'one valid active Scene teleport scope owned by this source caller'}},
              'complete_source_CFG': cfg, 'operation_gate': required_operations(flows, formal),
              'explicit_blockers':[],
              'montage_original_caller':{'source_entry':'0x31529','callee':'0x2e2b0→0x2ebe1',
                  'proof':str(montage_source_proof),'proof_sha256':NATURAL_MONTAGE_SHA256,
                  'corrected_complete_readset':str(montage_readset),'corrected_complete_readset_sha256':NATURAL_MONTAGE_READSET_SHA256,
                  'proof_original_identity_preserved':'CPU03 freshly executed with full07 owner identity; CPU02 and exploratory37040 rejection unchanged',
                  'HP_DWORD':719040,'marker_HP_WORD':63680,'original_intro_vectors':60,
                  'connected_original_callers':40,'derived_word_body_vectors':80,
                  'scope':'only original20 nonsplit pairs under31529, not every2ebe1 caller',
                  'actual_C_or_native_proven_by_conversion':False,
                  'source_tables':[{'va':hex(va),'width':20,'sha256':digest(image[va:va+20])} for va in (0x526e9,0x526fd,0x52711)],
                  'source_pair_parameters':[[image[va+i] for va in (0x526e9,0x526fd,0x52711)] for i in range(20)]},
              'modern_cleanup':[{'flow':'stage.cinematic.ending','op':'cin_page_free','args':[2],
                  'reason':'BASE snapshot最后源读取后释放现代owned scratch以复用固定槽；不是原free ABI或原调用数'}],
              'summary': {'source_slots':len(rows),'source_CFG_complete':sum(r['source_CFG_complete'] for r in rows),
                          'candidate_flows': len(flows)+len(common_flows), 'nodes': sum(len(f['nodes']) for f in flows+common_flows),
                          'different_operations': len({n['op'] for f in flows for n in f['nodes']}),
                          'new_runtime_bindings': 0},
              'claims': {'new_runtime_bindings': False, 'original_CPU_executed': False,
                         'actual_C_compared': False, 'native_GUI_executed': False,
                         'all13_isolated_sources_converted': True},
              'unclosed_candidate_dependencies':['independent full caller/C/required native gates not signed by producer']}
    if any(path.read_bytes() != raw for path, raw in snapshots.items()):
        raise ValueError('source package changed during candidate generation')
    return report


def validate_candidate(candidate, catalog=None):
    """逐字节源门和生成内容防篡改；不作为独立语义/CPU/C/原生证明。

    安装后static/manifest本就改变，故只重核冻结source/code及图内容，
    不把生成前的static摘要强加到安装后的唯一输入。
    """
    if candidate.get('schema')!=SCHEMA: raise ValueError('S5 extension schema differs')
    source=candidate['source_inputs']
    _,proof_inputs=natural_montage_source(source['montage_source_proof'],source['montage_readset'])
    readset,instructions,refs,receipt,snapshots=source_package(source['base_readset'],source['image'],source['extensions'])
    snapshots.update(proof_inputs)
    contract_path=Path(source['cinematic_contract']);snapshots[contract_path]=contract_path.read_bytes()
    for path,raw in snapshots.items():
        if candidate['input_sha256'].get(str(path))!=digest(raw):
            raise ValueError('S5 frozen source input changed')
    for path in (Path(__file__),Path(base.__file__),base.ROOT/'tools/static_data/convert_stage_callbacks.py'):
        if candidate['input_sha256'].get(str(path))!=digest(path.read_bytes()):
            raise ValueError('S5 converter changed; regenerate new candidate, never resign old')
    if receipt!=candidate['source_index']:raise ValueError('S5 complete source index differs')
    image=snapshots[Path(source['image'])];programs=caller_programs(image)
    expected=[teleport_helper(),*cinematic_helpers(image),
              *[compile_program(identity,program) for identity,program in programs.items()]]
    expected={(('common_event' if f['id'].startswith('common.') else 'stage'),f['id']):f for f in expected}
    actual={(domain,f['id']):f for domain,patch in candidate['s5_extension'].items() for f in patch['flows']}
    if len(actual)!=sum(len(p['flows']) for p in candidate['s5_extension'].values()) or actual!=expected:
        raise ValueError('S5 candidate full cold graph differs from frozen source producer')
    rows=candidate['rows']
    required={f'dispatch:stage_{kind}:{slot:03d}':(f'stage.source.{kind}.{slot:03d}','stage')
              for kind,slots in [('entry',(22,24,26,27,29)),('victory',(20,21,22,23,26,28,29))] for slot in slots}
    required['dispatch:event_callback:082']=('common.callback.082','common_event')
    targets={r['source_id']:r['target_va'] for r in readset['bindings']}
    targets['dispatch:event_callback:082']='0x362e8'
    if len(rows)!=13 or len({r['source_id'] for r in rows})!=13 or {r['source_id'] for r in rows}!=set(required):
        raise ValueError('S5 isolated13 exact source set differs')
    for row in rows:
        identity,domain=required[row['source_id']]
        if (row.get('definition_id'),row.get('definition_domain'))!=(identity,domain):
            raise ValueError('S5 source ID→cold definition join differs')
        if row.get('target_va')!=targets[row['source_id']]:raise ValueError('S5 actual original dispatch target differs')
        caller=base.recover_caller_cfg(int(row['target_va'],16),instructions,refs)
        if not caller['all_direct_caller_edges_recovered'] or row.get('caller_cfg')!=caller:
            raise ValueError('S5 actual complete caller CFG differs')
        if (row.get('definition_sha256')!=digest(encode(expected[domain,identity]).encode()) or
                row.get('source_CFG_complete') is not True or row.get('new_runtime_binding') is not False):
            raise ValueError('S5 candidate definition/source/approval identity differs')
        if row.get('disposition')!='converted_candidate':raise ValueError('S5 complete original source disposition differs')
    if catalog is not None:
        for key,flow in expected.items():
            if key in catalog and catalog[key]!=flow:
                raise ValueError('S5 installed same-ID cold graph differs')
    if any(path.read_bytes()!=raw for path,raw in snapshots.items()):raise ValueError('S5 source changed during revalidation')
    return rows,expected,snapshots


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base-readset','image','output-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--extension', type=Path, action='append', required=True)
    parser.add_argument('--cinematic-contract',type=Path,required=True)
    parser.add_argument('--montage-source-proof',type=Path,required=True)
    parser.add_argument('--montage-readset',type=Path,required=True)
    args = parser.parse_args(argv)
    output = args.output_dir.resolve()
    if output.exists() or output.is_relative_to((base.ROOT/'original_game').resolve()):
        raise ValueError('candidate requires a new directory outside original sources')
    report = build(args.base_readset, args.image, args.extension,args.cinematic_contract,args.montage_source_proof,args.montage_readset)
    output.mkdir(parents=True)
    (output/'conversion.json').write_text(encode(report), encoding='utf8')
    print(encode(report['summary']), end='')
    return 0


if __name__ == '__main__': raise SystemExit(main())
