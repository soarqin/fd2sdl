#!/usr/bin/env python3
"""原版-only MAGIC列表/取消与受控ID0单次施法校准；不开放SDL正式MAGIC。

依据field_player_magic_command_execute @0x1cff0、field_player_command_execute
@0x18d8c（原登记同名）；field_unit_magic_list_draw @0x1ceed（原sub_1CEED）
仅叠名字/费用，field_player_magic_list_select_step @0x1d51d（原sub_1D51D）处理选择。
damage依据field_magic_battle_animation @0x2ff01、field_defeated_units_finalize
@0x1db65、field_unit_detail_stats_draw @0x17fc0（原登记同名）与sub_1E292
@0x1e292（原登记同名）；只改active站位，不声称自然一回合移动后可施法。
复用fd2_magic_list_oracle纯资源构造；仅由driver run-job串行运行，owned PID清理。
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
from fd2_magic_list_oracle import prepare as list_templates,damage_detail as detail_templates
from audit_fdicon_opaque_zero import audit_fdicon
from run_graphics_tests import decode_save
from run_original_comparison import Capture,archive_entry,hashes,running,title_templates
from run_original_battle_menu import match_indexed,wait_state,wait_title
from run_original_end_turn import META,UNITS,STRIDE,RoundOracle,round_assets,validate_initial
from run_original_load import (check_load_measurement,focus_cycle_assets,focused_actor,
                               require_source_unchanged,wait_measured_save)
from run_original_focus import native_focus_reference
from fd2_movement_oracle import movement_profile
from run_original_player import (PlayerOracle,acted_sprite,cursor_assets,exact_tile,screen,
                                 sheet_frame,upper_command_composite,visual_focus)
from run_original_result import selection_probe,selection_state

START_SHA256 = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"
START_PLAIN_SHA256 = "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975"
EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"


def derive_controls(source,encrypted):
    """只教raw1五项以测输入；保留persistent roster，不声称自然学习或施法通过。"""
    if (len(source)!=0x59cb or len(encrypted)!=len(source) or
            hashlib.sha256(source).hexdigest()!=START_PLAIN_SHA256 or
            hashlib.sha256(encrypted).hexdigest()!=START_SHA256):
        raise ValueError("controls仅支持已确认真实START完整来源，拒绝其它或已修改的存档")
    plain = bytearray(source)
    bitmap = UNITS+STRIDE+0x1a
    plain[bitmap:bitmap+2] = b"\x0f\x20"
    plain[-4:] = sum(plain[:-4]).to_bytes(4,"little")
    encoded = bytes(a^b^c for a,b,c in zip(encrypted,source,plain))
    changes = [{"offset":i,"before":a,"after":b,
                "owner":"raw1 active magic bitmap" if i in (bitmap,bitmap+1) else "checksum"}
               for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    if {change["offset"] for change in changes}!={bitmap,bitmap+1,len(plain)-4}:
        raise AssertionError("controls派生改动超出已确认位图/checksum")
    return encoded,bytes(plain),{"synthetic_magic_bitmap":True,"natural_learning_covered":False,
        "magic_ids":[0,1,2,3,13],"changed_plain_bytes":changes,
        "changed_encrypted_bytes":[{"offset":i,"before":a,"after":b}
                                    for i,(a,b) in enumerate(zip(encrypted,encoded)) if a!=b],
        "source_encrypted_sha256":hashlib.sha256(encrypted).hexdigest(),
        "source_plain_sha256":hashlib.sha256(source).hexdigest(),
        "derived_encrypted_sha256":hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256":hashlib.sha256(plain).hexdigest()}


def derive_damage(source,encrypted):
    """受控站位而非游戏内移动；18890中unit9移动后不能本回合使用MAGIC。"""
    if (len(source)!=0x59cb or len(encrypted)!=len(source) or
            hashlib.sha256(source).hexdigest()!=START_PLAIN_SHA256 or
            hashlib.sha256(encrypted).hexdigest()!=START_SHA256):
        raise ValueError("damage仅支持已确认真实START完整来源")
    plain = bytearray(source)
    position = UNITS+STRIDE
    plain[position:position+2] = bytes((7,16))
    plain[-4:] = sum(plain[:-4]).to_bytes(4,"little")
    encoded = bytes(a^b^c for a,b,c in zip(encrypted,source,plain))
    changes = [{"offset":i,"before":a,"after":b,
                "owner":"raw1 active x/y" if i in (position,position+1) else "checksum"}
               for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    if {change["offset"] for change in changes}!={position,position+1,len(plain)-4}:
        raise AssertionError("damage派生改动超出坐标/checksum")
    return encoded,bytes(plain),{"synthetic_coordinate_fixture":True,"natural_one_turn_route_covered":False,
        "actor":1,"source_world":[10,15],"controlled_world":[7,16],"command_move_length":0,
        "hp_mp_ex_abilities_unchanged":True,"changed_plain_bytes":changes,
        "changed_encrypted_bytes":[{"offset":i,"before":a,"after":b}
                                    for i,(a,b) in enumerate(zip(encrypted,encoded)) if a!=b],
        "source_encrypted_sha256":hashlib.sha256(encrypted).hexdigest(),
        "source_plain_sha256":hashlib.sha256(source).hexdigest(),
        "derived_encrypted_sha256":hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256":hashlib.sha256(plain).hexdigest()}


def actor_focus_resources(original,source,assets,raw,cursor):
    record = source[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]
    isolated = bytearray(source)
    isolated[META+1] = 1
    isolated[UNITS:UNITS+STRIDE] = record
    _,composites = cursor_assets(original/"FDICON.B24",isolated,dict(assets,cursor=cursor))
    return {"raw_actor":raw,"unit_id":record[7],"world":list(record[:2]),
            "templates":[image.crop((0,0,24,24)) for image,offset in composites if offset==-6]}


def prepare(original,*,source=None):
    fixture = original/"START.SAV"
    encrypted = fixture.read_bytes()
    if hashlib.sha256(encrypted).hexdigest()!=START_SHA256:
        raise ValueError("列表校准仅支持已确认的只读真实START来源；未修改或生成夹具")
    base,focus = validate_initial(fixture,0)
    if source is None:
        source = base
    else:
        if source not in (derive_controls(base,encrypted)[1],derive_damage(base,encrypted)[1]):
            raise ValueError("资源仅接受完整controls/damage exact manifest派生")
        focus = screen(source,tuple(source[META+5:META+7]))
    templates,fields = list_templates(original,fixture,source=source)
    assets = round_assets(original)
    actors = focus_cycle_assets(original,source,assets)["actors"]
    actor = source[UNITS+STRIDE:UNITS+STRIDE*2]
    sprites = (original/"FDICON.B24").read_bytes()
    actor_sprites = [sheet_frame(sprites,actor[7]*12+phase) for phase in range(12)]
    for image in actor_sprites:
        image.putpalette(assets["palette"])
    assets["upper_commands"] = {index:[upper_command_composite(assets["commands"][index],sprite)
                                       for sprite in actor_sprites] for index in range(3)}
    damage = tuple(actor[:2])==(7,16)
    # 7,16的四邻受actor/视口边缘限制；沿用原source的9,15完整地形模板，
    # 后续按受控站位另验7,15→8,15→9,15三格可达，不冒称它是相邻格。
    probe = selection_probe(original,base if damage else source,1,assets)
    if damage:
        probe = dict(probe,actor_position=screen(source,tuple(actor[:2])))
    if fields["ids"]==[0,1,2,3,13] or damage:
        # 1cff0→14818→115b6：普通0/13分支，按native filter与raw顺序取首目标。
        raw,spell,budget = (11,0,5) if damage else (0,13,4)
        candidates = [i for i in range(source[META+1])
            if not source[UNITS+i*STRIDE+5]&1 and (source[UNITS+i*STRIDE+6]==0 if damage else source[UNITS+i*STRIDE+6]!=0) and
            abs(source[UNITS+i*STRIDE]-actor[0])+abs(source[UNITS+i*STRIDE+1]-actor[1])<=budget]
        if candidates!=([11] if damage else [0,1,2,3]) or abs(probe["world"][0]-actor[0])+abs(probe["world"][1]-actor[1])>budget:
            raise ValueError("目标候选或静态范围ROI超出已确认START场景")
        target_world = tuple(source[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2])
        reference = native_focus_reference(tuple(source[META+3:META+5]),tuple(actor[:2]),target_world)
        if not damage and reference["camera"]!=list(source[META+3:META+5]):
            raise ValueError("controls目标聚焦需要未校准卷屏")
        # 122dc mode2先画frame1，再127a9画actor（同格上移6px）；不将LUT套actor。
        selection_sheet = archive_entry((original/"FDOTHER.DAT").read_bytes(),1)
        begin,end = struct.unpack_from("<II",selection_sheet,10)
        audit = audit_fdicon(struct.pack("<HHHI",24,24,1,10)+selection_sheet[begin:end],expected_frames=1)
        if audit["opaque_zero_pixels"]:
            raise ValueError("目标frame1含实际opaque0，现合成资源门不能把它当透明")
        target_cursor = sheet_frame(selection_sheet,1)
        target_cursor.putpalette(assets["palette"])
        target_actor = actor_focus_resources(original,source,assets,raw,target_cursor)
        # 仅改变纯几何model用于ROI计算，不改夹具或把推导值冒充运行时读数。
        geometry = bytearray(source)
        geometry[META+3:META+5] = bytes(reference["camera"])
        target_probe = dict(probe,position=screen(geometry,probe["world"]))
        assets["magic_target"] = {"actor":target_actor,"range":target_probe,"screen":reference["screen"]}
        fields["target_contract"] = {"spell":spell,"cast":budget,"area":0,"filter":0 if damage else 1,
            "raw_candidates":candidates,"initial_raw_actor":raw,"initial_focus_world":list(target_world),
            "initial_focus_screen":reference["screen"],"selection_frame":1,
            "expected_camera":reference["camera"],"range_world":list(probe["world"]),"range_screen":list(target_probe["position"]),
            "selection_opaque_zero_pixels":audit["opaque_zero_pixels"]}
        if damage:
            prepare_damage_resources(original,source,assets,fields,geometry,target_world)
    return source,focus,assets,actors,probe,templates,fields


def prepare_damage_resources(original,source,assets,fields,geometry,target_world):
    """只服务受控unit9→raw11击杀；不推导其它魔法、成长或整屏演出。"""
    if hashlib.sha256((original/"FD2.EXE").read_bytes()).hexdigest()!=EXE_SHA256:
        raise ValueError("damage数值依据只支持已确认原版EXE")
    victim = source[UNITS+11*STRIDE:UNITS+12*STRIDE]
    if victim[7]!=96 or victim[0x21]!=2 or victim[0x31:0x34]!=b"\xff\0\0":
        raise ValueError("damage目标的等级/无掉落前置不符")
    field = (original/"FDFIELD.DAT").read_bytes()
    map_data,metadata = archive_entry(field,0),archive_entry(field,1)
    if struct.unpack_from("<HH",map_data)!=(24,24) or len(map_data)!=4+24*24*4:
        raise ValueError("damage地图不是已确认stage0布局")
    shape = (original/"FDSHAP.DAT").read_bytes()
    tiles,attributes = archive_entry(shape,metadata[0]*2),archive_entry(shape,metadata[0]*2+1)
    def dry_tile(world):
        value = struct.unpack_from("<I",map_data,4+(world[1]*24+world[0])*4)[0]
        tile = value&1023
        image = sheet_frame(tiles,tile)
        if value>>10 or attributes[tile*4] or 0 in image.getdata() or any(224<=p<240 for p in image.getdata()):
            raise ValueError("damage清场/站位ROI含动态、遮挡或未确认地形")
        image.putpalette(assets["palette"])
        return image,attributes[tile*4+1]
    caster = tuple(source[UNITS+STRIDE:UNITS+STRIDE+2])
    _,cost_class = dry_tile(caster)
    profile,_ = movement_profile(original/"FD2.EXE",source[UNITS+STRIDE+0x20])
    if profile[cost_class]!=1 or any(i!=1 and not source[UNITS+i*STRIDE+5]&1 and
            tuple(source[UNITS+i*STRIDE:UNITS+i*STRIDE+2])==caster for i in range(source[META+1])):
        raise ValueError("damage受控站位不可站立或被占用")
    route = ((7,15),(8,15),(9,15))
    if assets["magic_target"]["range"]["world"]!=(9,15) or sum(profile[dry_tile(cell)[1]] for cell in route)>source[UNITS+STRIDE+0x3b] or any(
            not source[UNITS+i*STRIDE+5]&1 and tuple(source[UNITS+i*STRIDE:UNITS+i*STRIDE+2]) in route
            for i in range(source[META+1])):
        raise ValueError("damage独立范围ROI没有已确认的三格可达路径")
    upper,_ = dry_tile((target_world[0],target_world[1]-1))
    lower,_ = dry_tile(target_world)
    empty = Image.new("P",(24,30))
    empty.putpalette(assets["palette"])
    empty.paste(upper.crop((0,18,24,24)),(0,0))
    empty.paste(lower,(0,6))
    cursor = assets["cursor"]
    empty.paste(cursor,(0,6),cursor.point(lambda c:255 if c else 0,mode="L"))
    icons = (original/"FDICON.B24").read_bytes()
    acted = [acted_sprite(sheet_frame(icons,9*12+phase)) for phase in range(12)]
    _,composites = cursor_assets(original/"FDICON.B24",source,assets)
    for sprite in acted:
        sprite.putpalette(assets["palette"])
        composite = Image.new("P",(24,30))
        composite.putpalette(assets["palette"])
        for image,at in ((cursor,(0,6)),(sprite,(0,0))):
            composite.paste(image,at,image.point(lambda c:255 if c else 0,mode="L"))
        composites.append((composite,-6))
    reference = native_focus_reference(tuple(geometry[META+3:META+5]),target_world,caster)
    returning = []
    previous = target_world
    for step in reference["steps"]:
        world = tuple(step["world"])
        if step["camera"]!=list(geometry[META+3:META+5]):
            raise ValueError("damage返回施法者需要额外卷屏")
        key = "RIGHT" if world[0]>previous[0] else "LEFT" if world[0]<previous[0] else "DOWN" if world[1]>previous[1] else "UP"
        returning.append((key,screen(geometry,world)))
        previous = world
    assets["damage_result"] = {"empty":empty,"empty_at":(assets["magic_target"]["screen"][0],assets["magic_target"]["screen"][1]-6),
        "acted":acted,"caster_screen":screen(geometry,caster),"composites":composites,"returning":returning,
        "alive_actor":actor_focus_resources(original,source,assets,11,cursor)}
    assets["damage_detail"] = detail_templates(original,original/"START.SAV",source)
    detail = archive_entry((original/"FDOTHER.DAT").read_bytes(),5)
    assets["detail_edges"] = [(sheet_frame(detail,index,lmi=True).crop((0,0,width,2)),at)
        for index,width,at in ((20,223,(92,7)),(21,310,(5,94)))]
    for image,_ in assets["detail_edges"]:
        image.putpalette(assets["palette"])
    fields["damage_contract"] = {"spell":0,"caster":1,"victim":11,"expected_mp":6,"max_mp":8,
        "expected_level":1,"expected_experience":42,"initial_experience":0,
        "enemy_base_record_va":"0x61c11","enemy_base_record_hex":"01070e00000701010415",
        "kill_experience":"enemy LV2 * record[9]21 / caster LV1 = 42; no level up",
        "expected_camera":list(geometry[META+3:META+5]),"expected_final_focus_world":list(target_world),
        "caster_screen":list(screen(geometry,caster)),"target_hidden_raw_flag_not_read":True,
        "hidden_visual_roi":[*assets["damage_result"]["empty_at"],24,30],
        "experience_notice":"sub_1E292 @0x1e292 fragment488 -> sub_1E5C0(11), automatic; no skip key"}


def list_page(frame,templates,fields,selected=0):
    # 三块都是明确矩形；exact_tile包含palette0，不产生新的像素排除。
    checks = {}
    if not 0<=selected<len(fields["ids"]):
        raise ValueError("列表选项超出已确认资源")
    selected_key = "selected" if selected==0 else f"selected_{selected}"
    for name in (selected_key,"actor_name","current_max_mp"):
        position,image = templates[name]
        found = exact_tile(frame,image,position)
        if found is None:
            return None
        checks[name] = {"position":list(position),"size":list(image.size),**found}
    return {"kind":"native_magic_list","selected":selected,"magic_ids":fields["ids"],
            "cost":fields["costs"][selected],
            "current_mp":fields["current_mp"],"max_mp":fields["max_mp"],"rectangles":checks,
            "checked_pixels":sum(check["checked_pixels"] for check in checks.values())}


def command_page(frame,command,position,templates,selected=None,*,after_cancel=False):
    detail = command.command(frame,position,selected)
    if detail is None:
        return None
    indices = [match["frame"] for match in detail["matches"]]
    # 真实raw1的weapon52 range1无敌目标、magic有效、两装备槽可计数、WAIT有效。
    if indices[0]!=2 or indices[1] not in ((4,) if selected==1 else (3,4)) or indices[2:]!=[6,9]:
        return None
    if after_cancel:
        at,image = templates["selected"]
        if exact_tile(frame,image,at) is not None:
            return None
    return {**detail,"capability_gates_compared":True,"list_resource_absent":after_cancel}


def compare_reopened(first,second,templates):
    checks = {}
    for name in ("selected","actor_name","current_max_mp"):
        at,image = templates[name]
        x,y = at
        expected = first.crop((x,y,x+image.width,y+image.height))
        found = exact_tile(second,expected,at)
        if found is None:
            raise AssertionError(f"reopened list changed the proven {name} rectangle")
        checks[name] = found
    return {"rectangles":checks,"name_mp_cost_selection_unchanged":True,
            "runtime_acted_pending_and_rng_not_measured":True}


def target_state(frame,assets,templates,fields):
    """识别本场首次普通目标，不发送确认；115b6/122dc，原登记同名。"""
    target = assets["magic_target"]
    focused = focused_actor(frame,assets,target["actor"])
    if not focused or focused["focus_screen"]!=target["screen"] or list_page(frame,templates,fields):
        return None
    for phase,image in target["range"]["ranges"]:
        found = exact_tile(frame,image,target["range"]["position"])
        if found:
            return {"kind":f"magic{fields['target_contract']['spell']}_target_selection","selection_mode":2,"selection_frame":1,
                    "raw_actor":target["actor"]["raw_actor"],"focus":focused,"range_lut":phase,
                    "range_world":list(target["range"]["world"]),"range_roi":list(target["range"]["position"])+[24,24],
                    "range":found,"target_confirmation_sent":False}
    return None


def damage_result(frame,assets):
    plan = assets["damage_result"]
    x,y = plan["caster_screen"]
    actor = next((phase for phase,sprite in enumerate(plan["acted"])
                  if match_indexed(frame,sprite,(x,y-6))["ok"]),None)
    if actor is None:
        return None
    position = tuple(assets["magic_target"]["screen"])
    closed = RoundOracle(assets,position).closed_browse(frame)
    empty = exact_tile(frame,plan["empty"],plan["empty_at"])
    if closed and empty:
        return {"kind":"magic0_acted_kill_browse","outcome":"kill","acted_caster_frame":actor,
                "hidden_visual_roi":[*plan["empty_at"],24,30],"empty_target":empty,
                "browse":closed,"target_raw_flags_not_read":True}
    alive = focused_actor(frame,assets,plan["alive_actor"])
    if alive and tuple(alive["focus_screen"])==position:
        return {"kind":"magic0_acted_miss_browse","outcome":"miss","acted_caster_frame":actor,"alive_target":alive}
    return None


def caster_detail(frame,source,assets):
    if PlayerOracle(source,assets,[],{}).detail(frame) is None:
        return None
    checks = {}
    for name,(at,image) in assets["damage_detail"].items():
        found = exact_tile(frame,image,at)
        if found is None:
            return None
        checks[name] = {"roi":[*at,*image.size],**found}
    return {"kind":"magic0_caster_detail_values","mp":6,"max_mp":8,"level":1,"experience":42,
            "rectangles":checks,"checked_pixels":sum(c["checked_pixels"] for c in checks.values()),
            "full_page_visual_equivalence":False,"rng_alignment_verified":False}


def cancelled_browse(frame,assets,actor,probe):
    focused = focused_actor(frame,assets,actor)
    base = exact_tile(frame,probe["base"],probe["position"])
    if focused and tuple(focused["focus_screen"])==probe["actor_position"] and base:
        return {"kind":"cancelled_browse","actor":focused,"untranslated_terrain":base}
    return None


def exercise(capture,source,focus,assets,actors,probe,templates,fields,titles,save,data,stamp,report,*,scenario="single"):
    def checkpoint(name,predicate,**kwargs):
        frame,detail = wait_state(capture,name,predicate,report["checkpoints"],**kwargs)
        require_source_unchanged(save,data,stamp)
        return frame,detail
    wait_title(capture,titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    checkpoint("continued-source-browse",RoundOracle(assets,focus).initial_browse,timeout=45)
    for actor in actors[:2]:
        capture.key("Z")
        _,detail = checkpoint(f"z-raw{actor['raw_actor']}",lambda frame,a=actor:focused_actor(frame,assets,a))
        if tuple(detail["focus_screen"])!=screen(source,tuple(actor["world"])):
            raise AssertionError("focus cycle moved the camera outside the confirmed source geometry")
    capture.key("ENTER")
    checkpoint("raw1-movement-selection",lambda frame:selection_state(frame,assets,actors[1],probe))
    capture.key("ENTER")
    command = PlayerOracle(source,assets,[],{})
    position = screen(source,tuple(actors[1]["world"]))
    checkpoint("raw1-zero-move-command",lambda frame:command_page(frame,command,position,templates))
    capture.key("LEFT")
    checkpoint("raw1-magic-selected",lambda frame:command_page(frame,command,position,templates,1))
    capture.key("ENTER")
    first,_ = checkpoint("magic-list-first",lambda frame:list_page(frame,templates,fields))
    if scenario=="controls":
        exercise_controls(capture,source,assets,actors,probe,templates,fields,save,data,stamp,report,first,checkpoint)
        return
    if scenario=="damage":
        capture.key("ENTER")  # 只打开唯一ID0的施法中心选择。
        checkpoint("damage-magic0-target",lambda frame:target_state(frame,assets,templates,fields))
        report["input_scope"] = {"controlled_coordinate_fixture":True,"natural_one_turn_route_covered":False,
            "cast_confirmation_count":1,"post_cast_skip_keys":[],"save_attempted":False}
        capture.key("ENTER")  # 本轮唯一施法提交；其后直到稳定browse只观察。
        _,outcome = checkpoint("damage-cast-outcome",lambda frame:damage_result(frame,assets),timeout=120)
        report["damage_outcome"] = outcome
        if outcome["outcome"]!="kill":
            raise AssertionError("ID0本次未命中，击杀/EX42尚未验收；未重试或发送补键")
        for index,(key,expected) in enumerate(assets["damage_result"]["returning"],1):
            capture.key(key)
            def reached(frame,at=expected):
                found = visual_focus(frame,assets,assets["damage_result"]["composites"])
                return found if found and tuple(found["focus_screen"])==tuple(at) else None
            checkpoint(f"damage-return-caster-{index}",reached)
        capture.key("F2")
        _,detail = checkpoint("damage-caster-detail",lambda frame:caster_detail(frame,source,assets))
        report["damage_measurement"] = detail
        return  # 首详情稳定页即hostclose；不关页、不Save、不END。
    capture.key("ESC")
    checkpoint("list-cancel-back-command",lambda frame:command_page(frame,command,position,templates,1,after_cancel=True))
    capture.key("ENTER")
    second,_ = checkpoint("magic-list-reopened",lambda frame:list_page(frame,templates,fields))
    report["reopened_comparison"] = compare_reopened(first,second,templates)
    report["input_scope"] = {"list_input":["ESC"],"second_list_input":[],"cast_confirmation_sent":False}


def exercise_controls(capture,source,assets,actors,probe,templates,fields,save,data,stamp,report,first,checkpoint):
    # 1d51d的上下回绕、左右±4与不足MP返回0；每个单键后都核对完整三块资源。
    navigation = (("up-wrap","UP",4),("right-edge","RIGHT",4),
        ("left-column","LEFT",0),("left-edge","LEFT",0),("down-first","DOWN",1),
        ("down-expensive","DOWN",2),("insufficient-enter","ENTER",2),
        ("insufficient-space","SPACE",2),("right-partial-edge","RIGHT",2),
        ("down-fourth","DOWN",3),("down-fifth","DOWN",4),("down-wrap","DOWN",0),
        ("right-column","RIGHT",4))
    report["controls_navigation"] = []
    for name,key,selected in navigation:
        capture.key(key)
        _,detail = checkpoint("controls-"+name,lambda frame,s=selected:list_page(frame,templates,fields,s))
        report["controls_navigation"].append({"key":key,**detail})
    capture.key("ENTER")  # 列表确认ID13，只进入target；不确认施法中心。
    checkpoint("controls-magic13-target",lambda frame:target_state(frame,assets,templates,fields))
    capture.key("ESC")
    reopened,_ = checkpoint("controls-target-cancel-list0",lambda frame:list_page(frame,templates,fields))
    report["reopened_comparison"] = compare_reopened(first,reopened,templates)
    capture.key("ESC")
    position = screen(source,tuple(actors[1]["world"]))
    command = PlayerOracle(source,assets,[],{})
    checkpoint("controls-list-cancel-command-first-enabled",lambda frame:command_page(
        frame,command,position,templates,1,after_cancel=True))
    capture.key("ESC")
    checkpoint("controls-command-cancel-selection",lambda frame:selection_state(frame,assets,actors[1],probe))
    capture.key("ESC")
    checkpoint("controls-selection-cancel-browse",lambda frame:cancelled_browse(frame,assets,actors[1],probe))
    report["input_scope"] = {"list_navigation_and_low_mp":True,"list_confirmation_spell":13,
        "target_input":["ESC"],"target_confirmation_sent":False,"cast_confirmation_sent":False,
        "runtime_pending_and_rng_not_measured":True}
    # 原版19df7：取消后仍无acted，才可用Save量测；保持既有严格Load测量写集。
    save_cell = actors[1]["world"][0]-1,actors[1]["world"][1]
    occupied = {tuple(source[UNITS+i*STRIDE:UNITS+i*STRIDE+2]) for i in range(source[META+1])
                if not source[UNITS+i*STRIDE+5]&1}
    if save_cell in occupied or not (2<=save_cell[0]-source[META+3]<=10 and
                                    2<=save_cell[1]-source[META+4]<=5):
        raise ValueError("controls没有已确认不卷屏的左邻Save空格")
    capture.key("LEFT")
    oracle = RoundOracle(assets,screen(source,save_cell))
    checkpoint("controls-measurement-left-empty",oracle.closed_browse)
    capture.key("ENTER")
    checkpoint("controls-measurement-parent",lambda frame:oracle.menu(frame,0))
    capture.key("UP")
    checkpoint("controls-measurement-parent-upper",lambda frame:oracle.menu(frame,0))
    capture.key("ENTER")
    checkpoint("controls-measurement-secondary",lambda frame:oracle.secondary(frame,0))
    capture.key("LEFT")
    checkpoint("controls-measurement-save-enabled",lambda frame:oracle.secondary(frame,1))
    capture.key("ENTER")
    checkpoint("controls-measurement-save-default-yes",lambda frame:oracle.buttons(frame,0))
    report["measurement_save_requested"] = True
    capture.key("ENTER")
    measured = wait_measured_save(save,data,stamp)
    # 保留真实输出后再严格检查；未知diff会失败并留下文件，不增加allowlist或恢复键。
    measured_bytes = save.read_bytes()
    measured_stamp = save.stat().st_mtime_ns
    (capture.artifacts/"controls-measured.SAV").write_bytes(measured_bytes)
    report["measured_save_sha256"] = hashlib.sha256(measured_bytes).hexdigest()
    report["measurement"] = check_load_measurement(source,measured,[("LEFT",screen(source,save_cell))])
    wait_state(capture,"controls-measurement-saved-browse",oracle.closed_browse,report["checkpoints"])
    require_source_unchanged(save,measured_bytes,measured_stamp)


def run(args):
    root,output = args.root.resolve(),args.artifacts.resolve()
    original = (root/"original_game").resolve()
    if output==original or original in output.parents:
        raise ValueError("测试产物不能写入original_game")
    output.mkdir(parents=True,exist_ok=False)
    before = hashes(original)
    fixture = original/"START.SAV"
    data,fixture_stamp = fixture.read_bytes(),fixture.stat().st_mtime_ns
    scenario = getattr(args,"scenario","single")
    if scenario not in ("single","controls","damage"):
        raise ValueError("未知法术列表校准场景")
    report = {"ok":False,"probe_only":True,"probe_cases_passed":False,
              "scenario":scenario,
              "capture":"Windows.Graphics.Capture HWND only","scope":"real START raw1 fire0 list -> Esc COMMAND -> reopened list; no cast",
              "excluded":"SDL pairing, cast/target/effect, runtime acted/pending snapshot, UI RNG alignment, animation/timing/audio, healing13",
              "ui_rng_static_evidence":"1d51d calls16c57(0); that wait consumes shared RNG; actual count/alignment not measured",
              "original_before":before,"fixture":{"path":str(fixture),"sha256":hashlib.sha256(data).hexdigest(),"mtime_ns":fixture_stamp},
              "processes":[],"inputs":[],"checkpoints":[]}
    save = None
    save_stamp = None
    save_data = data
    completed_bytes,completed_stamp = None,None
    try:
        report["binaries"] = {name:{"path":str(path.resolve()),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
                              for name,path in (("driver",args.driver),("dosbox",args.dosbox))}
        if scenario in ("controls","damage"):
            derive = derive_controls if scenario=="controls" else derive_damage
            save_data,derived,manifest = derive(decode_save(fixture),data)
            (output/(scenario+"-start.SAV")).write_bytes(save_data)
            (output/"fixture-manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf-8")
            report["derived_fixture"] = manifest
            source,focus,assets,actors,probe,templates,fields = prepare(original,source=derived)
            if scenario=="controls":
                report["scope"] = "synthetic five-spell START controls: navigation, insufficient MP, target/list/command/selection cancel, explicit UI Save; no cast"
                report["excluded"] = "SDL pairing, spell effects, natural learning, runtime pending/RNG, animation/timing/audio"
            else:
                report["scope"] = "controlled raw1 coordinates only: zero-move ID0 cast, acted/hidden visual evidence and caster MP/LV/EX detail; no Save or END"
                report["excluded"] = "natural one-turn route, SDL pairing, raw runtime flags/other owners, exact RNG stream, complete FIGANI visuals/timing/audio, other spell/caster"
                for name,(at,image) in assets["damage_detail"].items():
                    image.save(output/("expected-damage-"+name+".png"))
                assets["damage_result"]["empty"].save(output/"expected-damage-empty-target.png")
        else:
            source,focus,assets,actors,probe,templates,fields = prepare(original)
        report["resource_fields"] = fields
        report["plan"] = {"actors":[{key:actor[key] for key in ("raw_actor","unit_id","world")} for actor in actors[:2]],
                          "selection_probe_world":list(probe["world"]),"selection_probe_screen":list(probe["position"])}
        for name,(at,image) in templates.items():
            image.save(output/("expected-"+name+".png"))
        game_copy = output/"game"
        shutil.copytree(original,game_copy)
        save = game_copy/"FD2.SAV"
        save.write_bytes(save_data)
        save_stamp = save.stat().st_mtime_ns
        config = output/"dosbox.conf"
        config.write_text("[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
            "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
            f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
        env = os.environ.copy()
        for name in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER"):
            env.pop(name,None)
        with running([str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],output,env,
                     output/"game.log",args.driver.resolve(),report["processes"]) as process:
            capture = Capture(args.driver.resolve(),process,output,report["inputs"])
            arguments = (capture,source,focus,assets,actors,probe,templates,fields,title_templates(original/"FDOTHER.DAT"),
                         save,save_data,save_stamp,report)
            if scenario=="single":
                exercise(*arguments)  # 保持默认单项协议与输入不变。
            else:
                exercise(*arguments,scenario=scenario)
                if ("measurement" if scenario=="controls" else "damage_measurement") not in report:
                    raise AssertionError("场景未完成其严格量测")
            completed_bytes,completed_stamp = save.read_bytes(),save.stat().st_mtime_ns
        require_source_unchanged(save,completed_bytes,completed_stamp)
        report["probe_cases_passed"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_after"] = hashes(original)
        report["original_unchanged"] = before==report["original_after"]
        report["fixture_unchanged"] = fixture.read_bytes()==data and fixture.stat().st_mtime_ns==fixture_stamp
        report["copy_save_unchanged_after_cleanup"] = (save is not None and save.exists() and
            save.read_bytes()==save_data and save.stat().st_mtime_ns==save_stamp)
        report["copy_save_matches_completed_state_after_cleanup"] = (save is not None and save.exists() and
            completed_bytes is not None and save.read_bytes()==completed_bytes and save.stat().st_mtime_ns==completed_stamp)
        report["capture_hashes"] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()
                                  if p.is_file() and p.suffix.lower() in (".png",".bmp")}
        report["probe_cases_passed"] = (report["probe_cases_passed"] and report["original_unchanged"] and
            report["fixture_unchanged"] and report["copy_save_matches_completed_state_after_cleanup"] and
            (scenario=="controls" or report["copy_save_unchanged_after_cleanup"]))
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root","driver","dosbox","artifacts"):
        parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--scenario",choices=("single","controls","damage"),default="single")
    args = parser.parse_args()
    if sys.platform!="win32":
        parser.error("原版法术列表校准要求Windows WGC；未启动窗口")
    try:
        report = run(args)
        print(json.dumps(report,ensure_ascii=False,indent=2))
        return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"法术列表校准无法启动：{error}。检查参数并使用新产物目录重跑。",file=sys.stderr)
        return 1


if __name__=="__main__":
    sys.exit(main())
