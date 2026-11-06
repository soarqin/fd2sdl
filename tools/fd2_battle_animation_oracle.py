"""B6受控演出夹具与完整SAV有限终态；不启动窗口、不读SDL表。

依据15311/1598a AI分派评分、2ff01/30e9d FULL费用/HP写序、
1c75e/1c81f原伤害、1a30b首END；复用已校准B4 physical-cap/heal。
资源像素/声音比较另由真实捕获建立，本模块不把有限数学当实机通过。
"""
import hashlib
import struct
from pathlib import Path

from fd2_item_list_oracle import START_SHA256, PLAIN_SHA256, item_table, use_effect_tables
from fd2_movement_oracle import movement_profile
from fd2_reward_oracle import derive_fixture as reward_fixture, expected_state, reward_tables
from run_graphics_tests import decode_save
from run_original_comparison import archive_entry
from run_original_end_turn import META, STRIDE, UNITS
from run_original_focus import native_focus_reference
from run_original_item_list import phase_end_model
from run_original_load import check_load_measurement
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image

SCENARIOS = ("physical-full-hit", "player-magic0", "player-heal13") + tuple(
    f"ai-full-{i:02d}" for i in range(10)) + tuple(f"ai-full-{i:02d}-multi" for i in (3,5,6,7,8)) + ("ai-item-score20",)


def actor(source, raw):
    return source[UNITS + raw*STRIDE:UNITS + (raw+1)*STRIDE]

def _word(data, at): return struct.unpack_from("<H", data, at)[0]
def _store(data, at, value): struct.pack_into("<H", data, at, value & 65535)
def _checksum(data): data[-4:] = sum(data[:-4]).to_bytes(4,"little"); return bytes(data)
def _options(data):
    data=bytearray(data);data[META+14]=0;data[META+16]=0;data[META+17]=1
    return _checksum(data)
def _patch_actor(source, raw, record): source[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]=record

def casting_metadata(original, identity):
    data=(original/"FIGANI.DAT").read_bytes()
    index=3*identity+2;entry=archive_entry(data,index)
    if _word(entry,0)==0: index-=1;entry=archive_entry(data,index)
    count=_word(entry,0);intro=entry[2]
    if count==0 or intro>count or 8+4*count>len(entry):
        raise ValueError("caster FIGANI入口不是可预检的完整part目录")
    markers=[]
    for part in range(intro):
        start=struct.unpack_from("<I",entry,8+part*4)[0]
        if start+9>len(entry): raise ValueError("caster part元数据越界")
        if entry[start+4]==1:markers.append(part)
    return {"entry":index,"part_count":count,"intro_end":intro,"cost_marker_parts":markers,
        "cost_count":len(markers),"entry_sha256":hashlib.sha256(entry).hexdigest(),
        "figani_sha256":hashlib.sha256(data).hexdigest(),"basis":"2ff01 fallback+30e9d part+4 before FXphase0"}

def derive_fixture(original, scenario):
    if scenario not in SCENARIOS: raise ValueError("未知B6演出场景")
    original=Path(original);encrypted=(original/"START.SAV").read_bytes();base=decode_save(original/"START.SAV")
    if hashlib.sha256(encrypted).hexdigest()!=START_SHA256 or hashlib.sha256(base).hexdigest()!=PLAIN_SHA256:
        raise ValueError("B6夹具仅从完整已证START派生")
    plain=bytearray(base);magic_id=None;multi=False;caster_raw=0;targets=[11]
    if scenario in ("physical-full-hit","player-heal13"):
        b4="physical-cap" if scenario=="physical-full-hit" else "heal"
        _,controlled,_=reward_fixture(original,b4);plain=bytearray(controlled)
        if b4=="heal":caster_raw=1;magic_id=13;targets=[0]
    elif scenario=="player-magic0":
        caster_raw=1;magic_id=0
        for raw in range(base[META+1]):
            at=UNITS+raw*STRIDE
            if plain[at+6]!=2:
                plain[at+5]|=4
                if raw!=11:plain[at:at+2]=bytes((20+(raw-4)%3,2+(raw-4)//3))
        plain[UNITS+11*STRIDE:UNITS+11*STRIDE+2]=bytes((8,14))
        _store(plain,UNITS+11*STRIDE+0x40,999);_store(plain,UNITS+11*STRIDE+0x42,999)
    else:
        item_action=scenario=="ai-item-score20"
        magic_id=0 if item_action else int(scenario.split("-")[2]);multi=scenario.endswith("-multi");caster_raw=11;targets=[0,2] if multi else [0]
        if magic_id==9 and multi:raise ValueError("ID9 multi原图包越界，不是合法演出夹具")
        for raw in range(base[META+1]):
            at=UNITS+raw*STRIDE
            if plain[at+6]!=2:plain[at+5]|=4
        caster=bytearray(actor(base,1));caster[:2]=bytes((8,14));caster[5]=0;caster[6]=0
        caster[0x1a:0x1f]=bytes(5)
        if not item_action:caster[0x1a+magic_id//8]=1<<(magic_id%8)
        # side0需要真实13a9f behavior0；玩家记录原+34=CE不能当普通AI。
        caster[0x34]=0
        caster[0x0a:0x1a]=bytes((0x80,0))*8  # 明列隔离库存，不据此解释旧C5未扣MP。
        if item_action:caster[0x0a:0x0c]=bytes((0,56))  # 原item56，唯一可见且未装备的非消耗攻击道具。
        caster[0x3b]=0;caster[0x31]=255;_store(caster,0x44,250);_store(caster,0x46,250);_store(caster,0x48,0)
        _patch_actor(plain,11,caster)
        for raw,where in ((1,(1,21)),(2,(2,21)),(3,(3,21))):plain[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]=bytes(where)
        if multi:plain[UNITS+2*STRIDE:UNITS+2*STRIDE+2]=bytes((7,14 if magic_id==8 else 15))
        for raw in targets:
            _store(plain,UNITS+raw*STRIDE+0x40,50 if item_action else 999)
            _store(plain,UNITS+raw*STRIDE+0x42,50 if item_action else 999)
    controlled=_options(plain)
    encoded=bytes(a^b^c for a,b,c in zip(encrypted,base,controlled))
    patches=[{"offset":i,"before":a,"after":b} for i,(a,b) in enumerate(zip(base,controlled)) if a!=b]
    allowed=set(range(UNITS,UNITS+base[META+1]*STRIDE))|{META+14,META+16,META+17}|set(range(len(base)-4,len(base)))
    if any(row["offset"] not in allowed for row in patches):raise AssertionError("B6 fixture写集越出active/config/checksum")
    if controlled[0:UNITS]!=base[0:UNITS] or controlled[META+1]!=base[META+1]:raise AssertionError("B6 fixture改了roster/slots/count")
    manifest={"scenario":scenario,"controlled_fixture":True,"source_sha256":START_SHA256,
        "source_plain_sha256":PLAIN_SHA256,"derived_sha256":hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256":hashlib.sha256(controlled).hexdigest(),"plaintext_patches":patches,
        "caster_raw":caster_raw,"targets":targets,"magic_id":magic_id,"multi":multi,
        "options":{"effect":0,"camera":controlled[META+15],"music":0,"sfx":1},
        "unmodified_roster_slots_unknown_container":True,"natural_acquisition_or_battle_history":False,
        "controlled_AI_dispatch":None if not scenario.startswith("ai-") else {
            "canonical_dispatcher":"field_ai_unit_execute @0x13a9f",
            "copied_player_behavior_before":actor(base,1)[0x34],"behavior_after":0,
            "inventory_before_hex":actor(base,1)[0x0a:0x1a].hex(),"inventory_after_hex":actor(controlled,11)[0x0a:0x1a].hex(),
            "inventory_policy":"eight explicit (flag80,id0) slots; native1b8a6 count0 excludes item candidates",
            "old_C5_explanation":"player behavior CE low14 takes13a9f default common tail, not magic; no MP formula relaxation"}}
    if scenario=="ai-item-score20":
        manifest.update(action="item",item_id=56,item_slot=0)
        manifest["controlled_AI_dispatch"]["inventory_policy"]="one explicit (flag0,id56) slot followed by seven (flag80,id0); no learned magic"
    return encoded,controlled,manifest


def ai_item_plan(source, record, magic):
    """1567e/15880：仅本场item56的单目标等号边界，不扩成通用AI评分器。"""
    caster,target=actor(source,11),actor(source,0)
    if (record.hex()!="0678005a000000000000000101140000020000e0150000" or
            magic!=bytes((50,0,90,5,0,2,0))):
        raise ValueError("AI item equality必须真实item56/code20/value0及magic0原记录")
    if (caster[0x34]!=0 or caster[0x1a:0x1f]!=bytes(5) or caster[0x0a:0x1a]!=bytes((0,56))+bytes((0x80,0))*7 or
            caster[0x3b]!=0 or _word(caster,0x48)!=0 or caster[6]!=0):
        raise ValueError("AI item equality必须普通behavior0、无法术/MOV/AP且只有单item56")
    if target[5]&1 or target[6]!=2 or target[0x20]!=1 or _word(target,0x40)!=50 or _word(target,0x42)!=50:
        raise ValueError("AI item equality目标必须可见side2/profile1/HP=max50")
    centers=[]
    for raw in range(source[META+1]):
        u=actor(source,raw)
        if not u[5]&1 and u[6]!=0 and abs(u[0]-caster[0])+abs(u[1]-caster[1])<=record[16]:centers.append(raw)
    if centers!=[0]:raise ValueError("AI item equality必须只有Hero一个合法目标且无需移动")
    return {"action":"item","item_id":56,"slot":0,"center":list(target[:2]),"targets":[0],
        "score":18,"score_basis":"15880 unsigned magicPower50 >= targetHP50; old missing18 selects no action",
        "accuracy":90,"guaranteed_hit":False,"hit_hp_after":[1,2,3,4,5],
        "legal_miss_hp_after":50,"positive_action_evidence_required":"each endpoint actual HP<50; legal miss alone inconclusive"}

def ai_plan(source, magic):
    """1598a固定夹具：profile0全1、单magic、无移动/物品/物理主优先级。
    首个最高分按y/x扫描；不引用SDL planner作为验收oracle。
    """
    caster=actor(source,11);best=None
    if caster[0x34]!=0:raise ValueError("AI fixture必须显式普通behavior0；不能仅查询magic绕过真实dispatcher")
    if any(not caster[0x0a+2*slot]&0x80 for slot in range(8)):
        raise ValueError("AI fixture要求八槽明确空库存，排除独立item候选")
    power=_word(magic,0)
    for y in range(24):
        for x in range(24):
            if abs(x-caster[0])+abs(y-caster[1])>magic[3]:continue
            targets=[raw for raw in range(source[META+1]) if not actor(source,raw)[5]&1 and actor(source,raw)[6]!=0
                and abs(actor(source,raw)[0]-x)+abs(actor(source,raw)[1]-y)<=magic[4]]
            if not targets:continue
            score=sum((8 if _word(actor(source,raw),0x40)>=power else 24)*(3 if actor(source,raw)[8]==0 else 2)//2 for raw in targets)
            if best is None or score>best["score"]:best={"center":[x,y],"targets":targets,"score":score}
    if not best or best["score"]<6:raise ValueError("受控AI魔法未达到原score>=6")
    return best

def prepare(original, scenario):
    encoded,source,manifest=derive_fixture(original,scenario)
    effects,evidence=use_effect_tables(Path(original)/"FD2.EXE");items,item_evidence=item_table(Path(original)/"FD2.EXE")
    caster_raw=manifest["caster_raw"];caster=actor(source,caster_raw);magic_id=manifest["magic_id"]
    b4=None
    if scenario in ("physical-full-hit","player-heal13"):
        from run_original_reward import prepare as reward_prepare
        b4=reward_prepare(Path(original),"physical-cap" if scenario=="physical-full-hit" else "heal")
        b4.update(source=source,encoded=encoded)
        for key in ("immediate","final"):b4["model"][key]=_options(b4["model"][key])
        final=b4["model"]["final"]
    else:final=phase_end_model(source,items)
    final=bytearray(final);final[META+5:META+9]=bytes((7,14,2,3));final=_checksum(final)
    contract={"scenario":scenario,"caster_raw":caster_raw,"caster_id":caster[7],"caster_side":caster[6],
        "magic_id":magic_id,"targets":[{"raw":raw,"unit_id":actor(source,raw)[7],"world":list(actor(source,raw)[:2]),"record_hex":actor(source,raw).hex()} for raw in manifest["targets"]],
        "source_records_hex":[actor(source,raw).hex() for raw in range(source[META+1])],"effect":0,"music":0,"sfx":1}
    item_action=scenario=="ai-item-score20"
    caster_meta=casting_metadata(Path(original),caster[7]) if magic_id is not None and magic_id!=13 and not item_action else None
    magic=effects["magic"][magic_id*7:(magic_id+1)*7] if magic_id is not None else None
    if manifest["scenario"].startswith("ai-"):
        costs,cost_evidence=movement_profile(Path(original)/"FD2.EXE",0)
        if costs!=bytes((1,))*20:raise ValueError("AI fixture需要已证profile0全1；不猜blocked-range")
        chosen=ai_item_plan(source,items[56*23:57*23],magic) if item_action else ai_plan(source,magic)
        if chosen["targets"]!=manifest["targets"]:raise ValueError("AI实际最高分目标集不等于该场contract")
        if caster[0x3b]!=0 or _word(caster,0x48)!=0 or caster[6]!=0 or caster[7]!=9 or (not item_action and caster_meta["cost_count"]!=1):
            raise ValueError("AI caster/费用/物理优先级前置不满足")
        contract.update(ai_plan=chosen,profile0=cost_evidence)
        if item_action:
            contract.update(action="item",item_id=56,item_slot=0,item_record_hex=items[56*23:57*23].hex(),
                no_MP_cost_or_slot_consumption=True,full_FIGANI_frame_or_sound_equivalence_claimed=False)
        camera=list(source[META+3:META+5]);focus=list(source[META+5:META+7])
        for target in ((6,14),tuple(caster[:2]),tuple(chosen["center"]),(7,14)):
            projected=native_focus_reference(camera,focus,target);camera,focus=projected["camera"],list(target)
        if camera!=[5,11]:raise ValueError("本B6 fixture超出已准备的固定camera资源")
    return {"scenario":scenario,"encoded":encoded,"source":source,"manifest":manifest,"base_final":final,
        "effects":effects,"tables":evidence,"items":items,"item_table":item_evidence,"contract":contract,
        "caster_metadata":caster_meta,"magic":magic,"b4":b4,"original":Path(original)}

def project_final(prepared, measured):
    """只根据原有限数域取本端HP分支；其余字节从fixture模型保留。
    随后仍须check_load_measurement全容器，不读其它measured字节填期望。
    """
    if len(measured)!=0x59cb or sum(measured[:-4])!=int.from_bytes(measured[-4:],"little"):
        raise AssertionError("B6真实SAV长度/checksum错误")
    source,scenario=prepared["source"],prepared["scenario"]
    expected=bytearray(prepared["base_final"]);selected=[]
    if scenario not in ("physical-full-hit","player-heal13"):
        identity=prepared["manifest"]["magic_id"];magic=prepared["magic"]
        for index,raw in enumerate(prepared["manifest"]["targets"]):
            target=actor(source,raw);before=_word(target,0x40);profile=target[0x20]
            if not 1<=profile<=28:raise ValueError("B6目标profile超出原倍率表")
            scale=struct.unpack_from("<i",prepared["effects"]["damage_profiles"],4*(profile-1))[0]
            power=struct.unpack_from("<h",magic)[0]*scale//10
            losses={power*9//10+spread*power//1000 for spread in range(100)}
            if max(losses)>=before:raise ValueError("B6 fixture必须非杀伤，不能掩盖loot/成长")
            allowed={before-loss for loss in losses}
            if magic[2]<100:allowed.add(before)
            # ID8 phase3不重置，后续target没有phase5 impact，HP恢复before后不再写。
            if identity==8 and index>0:allowed={before}
            actual=_word(measured,UNITS+raw*STRIDE+0x40)
            if actual not in allowed:raise AssertionError(f"raw{raw} HP={actual}不属于本端原FULL有限结果")
            _store(expected,UNITS+raw*STRIDE+0x40,actual)
            selected.append({"raw":raw,"hp_before":before,"hp_after":actual,"allowed_hp":sorted(allowed),
                "hit_inferred_from_HP":None if identity==8 and index>0 else actual!=before})
        caster_raw=prepared["manifest"]["caster_raw"]
        cost=0 if scenario=="ai-item-score20" else prepared["caster_metadata"]["cost_count"]*magic[5]
        _store(expected,UNITS+caster_raw*STRIDE+0x44,_word(actor(source,caster_raw),0x44)-cost)
        if scenario=="player-magic0":
            target=actor(source,11);loss=_word(target,0x40)-selected[0]["hp_after"]
            data=(prepared["original"]/"FD2.EXE").read_bytes();le=parse_le(data);raw=build_relbase_image(data,le,parse_objects(data,le))
            coefficient=raw[0x61af9+(target[7]-68)*10+9]
            gain=min(99,(loss*target[0x21]*coefficient//_word(target,0x42))//actor(source,caster_raw)[0x21])
            if actor(source,caster_raw)[0x3c]+gain>=100:raise ValueError("B6player0夹具不覆盖升级")
            expected[UNITS+caster_raw*STRIDE+0x3c]=actor(source,caster_raw)[0x3c]+gain
    return _checksum(expected),{"independent_native_finite_targets":selected,"cross_endpoint_rng_aligned":False,
        "source_unknown_roster_manual_bytes_not_taken_from_measurement":True}


def item_action_coverage(prepared, measured, steps):
    """有限SAV允许miss，但本轮评分回归须另有实际动作正证，不能把无动作当miss通过。"""
    if prepared["scenario"]!="ai-item-score20":raise ValueError("动作正证只用于AI item equality场景")
    _,detail=check_measurement(prepared,measured,steps)
    target=detail["independent_native_finite_targets"][0]
    positive=target["hp_after"]<target["hp_before"]
    return {"passed":positive,"status":"actual_nonlethal_HP_loss" if positive else "inconclusive_legal_miss_or_no_action",
        "raw":0,"hp_before":target["hp_before"],"hp_after":target["hp_after"],
        "legal_miss_allowed_by_finite_SAV":True,"no_reroll_or_additional_input":True,
        "whole_FX_or_SFX_claimed":False}

def check_measurement(prepared, measured, steps):
    expected,detail=project_final(prepared,measured)
    return expected,{**check_load_measurement(expected,measured,steps),**detail}
