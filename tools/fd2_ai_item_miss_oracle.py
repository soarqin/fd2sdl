"""item56精确AI夹具的合法miss观察：原MAP0七帧及完整四glyph两姿态。

13a9f/14237/14ef0/15055/1567e/15880限定唯一item候选；20c6f→2111a→
1c75e返回0→1e1dc/1df58给出MISS。5202c/5204a及51f33/54/75是canonical
relbase VA，经既有strict LE source_range读取，均不是EXE file offset。
只读已关闭WGC/SAV/音频；不是完整FX、声音身份、RNG同轨或通用OCR。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import wave

from PIL import Image

import fd2_battle_animation_oracle as business
from fd2_save_checks import decode_save
from run_original_battle_menu import match_indexed
from run_original_comparison import archive_entry, normalize
from run_original_end_turn import META, STRIDE, UNITS
from run_original_player import screen, sheet_frame
from static_data.verify_unit_tables import source_range

EXE_SHA = 'bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
OTHER_SHA = '882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667'
MAP_FRAMES = tuple(range(50, 57))  # 源49..56；本门不补签未捕获的首49。


class MissingActionWitness(AssertionError):
    """合法全SAV存在，但已采帧不能证明动作；不能把它当作命中或无动作。"""


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _require(value, message):
    if not value:
        raise AssertionError('AI item56 miss见证：' + message)


def _exclusive(source, item, magic, expected_plan):
    _require(len(source) == 22987 and source[META+1] == 12, 'fixture长度/count')
    eligible = [raw for raw in range(source[META+1]) if source[UNITS+raw*STRIDE+6] == 0
                and not source[UNITS+raw*STRIDE+5] & 5]
    caster = business.actor(source, 11)
    _require(eligible == [11] and not any(caster[10+slot*2] & 64 for slot in range(8)),
             '另一可执行hostile或装备物理候选')
    plan = business.ai_item_plan(source, item, magic)
    _require(plan == expected_plan, '唯一中心/目标/评分边界改变')
    return eligible


def resources(original, prepared):
    original = Path(original)
    exe, other = (original/'FD2.EXE').read_bytes(), (original/'FDOTHER.DAT').read_bytes()
    _require(hashlib.sha256(exe).hexdigest() == EXE_SHA and hashlib.sha256(other).hexdigest() == OTHER_SHA,
             '原EXE/FDOTHER身份')
    expected = business.prepare(original, 'ai-item-score20')
    _require(prepared['source'] == expected['source'] and prepared['contract'] == expected['contract'], '非本精确夹具')
    _exclusive(prepared['source'], expected['items'][56*23:57*23], expected['magic'], expected['contract']['ai_plan'])
    curve = list(source_range(exe, 0x5202c, 25))
    glyphs = list(source_range(exe, 0x5204a, 4))
    _require(glyphs == [116,117,118,118] and list(source_range(exe,0x51f33,1)) == [49]
        and list(source_range(exe,0x51f54,1)) == [8] and list(source_range(exe,0x51f75,1)) == [6], '原表来源')
    palette = prepared['assets']['palette']
    glyph_bank, fx_bank = archive_entry(other,5), archive_entry(other,6)
    def material(bank, identity):
        value = sheet_frame(bank,identity,lmi=True); value.putpalette(palette)
        return value
    words = []
    for phase in range(22):
        image = Image.new('P',(32,40)); image.putpalette(palette)
        for slot, identity in enumerate(glyphs):
            value = material(glyph_bank,identity)
            image.paste(value,(5*slot+(3 if slot==1 else 2),curve[phase+slot]+7),
                value.point(lambda i:255 if i else 0,mode='L'))
        words.append(image)
    return {'expected':expected,'curve':curve,'glyphs':glyphs,'words':words,
        'fx':{identity:material(fx_bank,identity) for identity in MAP_FRAMES}}


def check_visual(rows, material, action_qpc):
    """只在固定Hero/camera原公式坐标检查完整资源写点，不搜索任意位置。"""
    fx_hits = []; word_hits = []; next_fx = 0
    for row in rows:
        if row['wgc_timestamp_100ns'] <= action_qpc:
            continue
        with Image.open(row['path']) as image:
            frame = normalize(image)
        if next_fx < len(MAP_FRAMES):
            identity = MAP_FRAMES[next_fx]
            found = match_indexed(frame,material['fx'][identity],(52,70))
            if found['ok']:
                fx_hits.append({'frame':identity,'source':row,**found}); next_fx += 1
            continue  # 同一PNG不能作为其后的浮字相位。
        phases = [phase for phase in range(22) if match_indexed(frame,material['words'][phase],(52,66))['ok']]
        if not word_hits and 9 in phases:
            word_hits.append({'source':row,'source_curve_phases':[9],
                **match_indexed(frame,material['words'][9],(52,66))})
        elif word_hits and row['index'] > word_hits[0]['source']['index'] and any(i in phases for i in (20,21)):
            word_hits.append({'source':row,'source_curve_phases':[i for i in phases if i in (20,21)],
                **match_indexed(frame,material['words'][20],(52,66))})
            break
    if next_fx != len(MAP_FRAMES) or len(word_hits) != 2:
        raise MissingActionWitness('缺少有序七MAP帧或完整MISS两不同姿态')
    return {'MAP0_source_frames':fx_hits,'MISS_complete_word_poses':word_hits,
        'source_first49_not_required_or_claimed':True,'all22_trajectory_frames_claimed':False}


def observe_case(original, prepared, case, work):
    """游戏、WGC和音频已经正常关闭；外层Job及producer最终守卫仍由原流程校验。"""
    # 延迟导入避免battle runner→此独立oracle→battle runner循环；这些源已在run前绑定。
    from run_original_battle_animation import series_index, parse_job
    work = Path(work).resolve()
    material = resources(original,prepared)
    _require(case.get('business_sequence_completed') is True and case.get('collection_completed') is True, '业务/采集未完成')
    processes = case.get('processes',[])
    _require(len(processes)==1 and processes[0].get('cleaned') is True and processes[0].get('forced_cleanup') is False
        and processes[0].get('exit_code')==0, '游戏尚未正常清理')
    pid = processes[0]['pid']
    if 'guards' in case:
        _require(bool(case['guards']) and all(value is True for value in case['guards'].values()), '已有最终守卫失败')
    helper = case['series_helper']
    _require(helper.get('cleaned') is True and helper.get('forced_cleanup') is False and helper.get('exit_code')==0, '采集helper未正常清理')
    _require(parse_job(Path(helper['stderr']).read_text(encoding='utf-8')) == helper['job'], '采集Job收据')
    series = case['animation_capture']; summary_path = Path(series['summary_path']).resolve()
    _require(summary_path.is_relative_to(work) and sha(summary_path)==series['summary_sha256'], 'series summary来源')
    summary = json.loads(summary_path.read_text(encoding='utf-8'))
    ready = json.loads((work/'animation-series/ready.json').read_text(encoding='utf-8'))
    _require(summary == series['summary'] == helper['summary'] and ready == series['ready'], 'closed summary/ready绑定')
    rows = series_index(work/'animation-series',summary,ready,pid)
    _require(rows == series['frames'], 'closed帧目录/owner/SHA改变')
    action = case['animation_action']; delivery = action['delivery']
    _require(action.get('key')=='ENTER' and action.get('confirmations')==1 and delivery.get('ok') is True
        and delivery.get('pid')==pid and delivery.get('hwnd')==ready['hwnd'], '唯一END投递/owner')
    _require(sum(row.get('delivery')==delivery for row in case['inputs'])==1, 'END投递重复/缺失')
    completed = case['completed_save']; snapshot = Path(completed['snapshot']).resolve()
    _require(snapshot.is_relative_to(work) and sha(snapshot)==completed['sha256'], '完整Save来源')
    measured = decode_save(snapshot)
    _, finite = business.check_measurement(prepared,measured,[('LEFT',screen(prepared['base_final'],(6,14)))])
    _require(struct.unpack_from('<H',measured,UNITS+64)[0]==50, '不是合法miss HP50域')
    _require(case['ai_item_action_positive_evidence']['passed'] is False, '不能改写HP_positive门')
    live = work/'game/FD2.SAV' if case['kind']=='original' else work/'FD2.SAV'
    _require(live.read_bytes()==snapshot.read_bytes() and live.stat().st_mtime_ns==completed['mtime_ns'], 'owned Save bytes/mtime')
    audio = case['audio_capture']['wav']; wav_path = Path(audio['path']).resolve()
    _require(wav_path.is_relative_to(work) and sha(wav_path)==audio['sha256'], '封闭音频来源')
    with wave.open(str(wav_path),'rb') as wav:
        _require(wav.getnframes()==audio['frames'] and wav.getnchannels()==2 and wav.getsampwidth()==2
            and wav.getframerate()==audio['sample_rate'] and len(wav.readframes(wav.getnframes()))==audio['data_bytes'], 'WAV未完整封包')
    if case['kind']=='sdl' and 'app_mix_capture' in case:
        appmix = case['app_mix_capture']
        for key in ('ready_fact','summary_fact'):
            _require(sha(appmix[key]['path'])==appmix[key]['sha256'], 'AppMix封包来源')
            value = appmix['ready' if key=='ready_fact' else 'summary']
            _require(json.loads(Path(appmix[key]['path']).read_text(encoding='utf-8'))==value, 'AppMix封包/收据不同')
        _require(appmix['summary']['ok'] is True and appmix['summary']['pid']==pid and appmix['summary']['callback_quiescent']
            and appmix['summary']['wave_finalized'] and appmix['summary']['stop_reason']=='stop', 'AppMix尚未关闭')
    elif case['kind']=='original':
        _require(case['native_audio']['started'] and case['native_audio']['stopped'], '原WAV未停止')
    else:
        audio_helper = case['audio_helper']
        _require(audio_helper['cleaned'] and not audio_helper['forced_cleanup'] and audio_helper['exit_code']==0, '音频helper未清理')
        parse_job(Path(audio_helper['stderr']).read_text(encoding='utf-8'))
    witness = check_visual(rows,material,action['key_call_qpc_bracket']['before']['qpc'])
    return {'passed':True,'kind':'exclusive_item56_executed_legal_miss','HP_positive_passed':False,
        'scope':'exact AI item56/code20/value0 fixture;seven complete MAP0 resources then two complete MISS poses;not hit/fullFX/SFX/RNG',
        'pid':pid,'hwnd':ready['hwnd'],'closed_frames_checked':len(rows),'Save_SHA256':completed['sha256'],
        'full22987B_native_finite_Save_verified':finite['source_equal_except_declared_cache_and_measuring_focus'],
        'source_address_view':'canonical relbase VA,strict LE raw-unrelocated,not file offsets',
        'source_EXE_SHA256':EXE_SHA,'source_FDOTHER_SHA256':OTHER_SHA,
        'unique_fixture':{'source_plain_SHA256':hashlib.sha256(prepared['source']).hexdigest(),
            'eligible_hostile_raws':[11],'learned_magic_bitmap':[0]*5,'equipped_weapon_slots':[],
            'item_id':56,'item_slot':0,'code':20,'value':0,'accuracy':90,'power':50,
            'plan':prepared['contract']['ai_plan'],'source_score18_runtime_RAM_read':False},
        'source_MISS_glyphs':material['glyphs'],'source_curve':material['curve'],
        'game_and_capture_cleanup_verified':True,'outer_Job_or_final_producer_guards_claimed':False,**witness}
