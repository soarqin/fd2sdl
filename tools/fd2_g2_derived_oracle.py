"""G2 派生28的独立来源夹具、FIGANI合同和关联全SAV结果门。

只读原EXE/FDFIELD/FIGANI及真实SAV，不读取SDL冷表或观测终态。
10c50/1b750构造、1cff0/2cf30/1c81f及18d8c/1e292为静态依据；
本模块不启动窗口，不声明RNG同轨或逐part运行写点已经量测。
"""
from __future__ import annotations

from pathlib import Path
import struct
import sys

import fd2_g2_battle_oracle as common

sys.path.insert(0, str(Path(__file__).resolve().parent / "static_data"))
from verify_unit_tables import source_range

SCENARIO = "derived28"
SCENARIOS = (SCENARIO,)
SAVE_SIZE, META, UNITS, STRIDE = common.SAVE_SIZE, common.META, common.UNITS, common.STRIDE
CASTER_RAW, TARGET_RAW, TARGET_DONOR_RAW = 1, 4, 25
CASTER_WORLD, TARGET_WORLD, SAVE_WORLD = (7, 14), (8, 14), (6, 14)
TARGET_SHA256 = "61ae3bd1322a10070466aae511f001382fd2effc9072539a5d9e89de15d7b8cf"
CLIP_SHA256 = "2cdc1fd76f850989c880a0f5321947916252f9b35aec871e0656708a52110d70"
TEMPLATE = bytes((2,19,1,1,20,8,143,255,255,255,255,255,255,0,0,0,16,0,0,0,0,0,3,7,0,0))


def signed16(value):
    return value if value < 0x8000 else value - 0x10000


def signed32(value):
    value &= 0xffffffff
    return value if value < 0x80000000 else value - 0x100000000


def trunc_div(value, divisor):
    if divisor <= 0:
        raise ValueError("派生结果除数必须为正")
    return (-1 if value < 0 else 1) * (abs(value) // divisor)


def word(record, at):
    return struct.unpack_from("<H", record, at)[0]


def clip_contract(data):
    """固定entry59，完整有序offset及全部part头；不借普通slot回退。"""
    if common.digest(data) != common.FIGANI_SHA256:
        raise ValueError("derived28 FIGANI身份不符")
    clip = common._archives(data)[59]
    if common.digest(clip) != CLIP_SHA256:
        raise ValueError("derived28 固定slot2条目身份不符")
    count, intro = word(clip, 0), clip[2]
    if count != 19 or intro != 7 or 8 + count * 4 > len(clip):
        raise ValueError("derived28 clip目录不符")
    parts = []
    for index in range(count):
        start = struct.unpack_from("<I", clip, 8 + index * 4)[0]
        end = struct.unpack_from("<I", clip, 12 + index * 4)[0] if index + 1 < count else len(clip)
        if not 8 + count * 4 <= start < end <= len(clip) or end - start < 9:
            raise ValueError("derived28 part偏移或头被截断")
        parts.append({"index": index, "offset": start, "end": end,
                      "marker": clip[start + 4], "duration": clip[start + 6]})
    costs = [p["index"] for p in parts[:intro] if p["marker"] == 1]
    effects = [p["index"] for p in parts[intro:] if p["marker"] == 1]
    if costs != [6] or effects != list(range(7, 15)):
        raise ValueError("derived28 实际费用／效果marker合同不同")
    return {"figani_sha256": common.FIGANI_SHA256, "entry": 59, "slot": 2,
            "entry_sha256": CLIP_SHA256, "part_count": count, "intro_end": intro,
            "parts": parts, "cost_marker_parts": costs, "effect_marker_parts": effects,
            "divisor": 8, "ordinary_fallback": False}


def _caster_overlay(seed, template, base, growth, item_records):
    """仅10c50/1b750已证写集；未知字节继续保留START raw1，不填猜值。"""
    if (len(seed), template, len(base), len(growth)) != (80, TEMPLATE, 24, 11):
        raise ValueError("derived28只支持已登记的完整unit19模板")
    result = bytearray(seed)
    level = template[4]
    result[0:2] = bytes(CASTER_WORLD)
    result[3:10] = bytes((0, 0, 0, template[0], template[1], template[1], 0))
    result[10:14] = bytes((64, template[5], 64, template[6]))
    for i, item in enumerate(template[7:13]):
        result[14 + 2*i:16 + 2*i] = bytes((128 if item == 255 else 0, item))
    result[26:31] = template[13:17] + b"\0"
    result[31:34] = bytes((base[0], base[1], level))
    result[34:40] = bytes(6)
    result[49:55] = template[22:25] + template[17:20]
    result[59:62] = bytes((base[7], 0, template[2]))
    ap = word(base,18) + level*growth[0]
    dp = word(base,20) + level*growth[2]
    ac = word(base,22) + level*growth[4]
    for at,value in ((55,ap),(57,dp),(62,ac),
                     (64,word(base,3)+(level-1)*growth[6]),
                     (66,word(base,3)+(level-1)*growth[6]),
                     (68,word(base,5)+(level-1)*growth[8]),
                     (70,word(base,5)+(level-1)*growth[8])):
        struct.pack_into("<H",result,at,value & 65535)
    values = [signed16(word(result,55)), signed16(word(result,57)),
              signed16(word(result,62)), signed16(word(result,62))]
    for index in range(8):
        if not result[10 + index*2] & 64:
            continue
        item = item_records[result[11 + index*2]]
        for slot,at in enumerate((1,5,3,7)):
            values[slot] += signed16(word(item,at))
    struct.pack_into("<4H",result,72,*(v & 65535 for v in values))
    return bytes(result)


def derive_fixture(original, scenario=SCENARIO):
    if scenario != SCENARIO:
        raise ValueError("未知派生夹具")
    original = Path(original)
    encrypted = (original/"START.SAV").read_bytes()
    donor_encrypted = (original/"FD2.SAV").read_bytes()
    if common.digest(encrypted) != common.START_SHA256 or common.digest(donor_encrypted) != common.DONOR_SHA256:
        raise ValueError("derived28 START或真实目标donor身份不符")
    source, donor = common.valid_plain(common.crypt(encrypted)), common.valid_plain(common.crypt(donor_encrypted))
    if common.digest(source) != common.START_PLAIN_SHA256 or source[META+1] != 12 or source[META+2] != 0:
        raise ValueError("derived28需要已登记stage0 START")
    executable = (original/"FD2.EXE").read_bytes()
    field = (original/"FDFIELD.DAT").read_bytes()
    if common.digest(field) != common.FIELD_SHA256:
        raise ValueError("derived28 FDFIELD身份不符")
    entries = common._archives(field)
    metadata, placement = entries[67], entries[68]
    template = metadata[0x83+26:0x83+52]
    if metadata[2] <= 1 or template != TEMPLATE or struct.unpack_from("<3H",placement,8) != (21,30,19):
        raise ValueError("derived28真实有效模板／同索引placement不符")
    base = source_range(executable,0x61da1+19*24,24)
    growth = source_range(executable,0x620a1+19*11,11)
    items = source_range(executable,0x602ad,215*23)
    if items[8*23+11:8*23+13] != bytes((1,1)) or source_range(executable,0x61646,20) != bytes((1,))*20:
        raise ValueError("derived28初始COMMAND的邻格攻击来源不符")
    records = [bytearray(common.actor(source,raw)) for raw in range(12)]
    records[1] = bytearray(_caster_overlay(records[1],template,base,growth,
        [items[i*23:(i+1)*23] for i in range(215)]))
    target = common.actor(donor,TARGET_DONOR_RAW)
    if common.digest(target) != TARGET_SHA256 or target[6:8] != bytes((0,116)) or any(target[34:40]):
        raise ValueError("derived28真实unit116目标donor不符")
    records[4] = bytearray(target)
    for raw,record in enumerate(records):
        record[3:5] = bytes(2)
        if record[6] != 2:
            record[5] |= 4  # 有明示来源的AI停止位，非hidden；END仍保留活敌。
            record[:2] = bytes((20+(raw-4)%3,2+(raw-4)//3))
    records[0][:2], records[2][:2], records[3][:2] = b"\x0b\x0e", b"\x01\x15", b"\x02\x15"
    records[4][:2] = bytes(TARGET_WORLD)
    controlled = bytearray(source)
    for raw,record in enumerate(records):
        controlled[UNITS+raw*80:UNITS+(raw+1)*80] = record
    cx,cy = source[META+3:META+5]
    controlled[META+5:META+9] = bytes((*CASTER_WORLD,CASTER_WORLD[0]-cx,CASTER_WORLD[1]-cy))
    plain = common.checksum(controlled)
    magic = source_range(executable,0x619fd+28*7,7)
    coefficient = source_range(executable,0x61af9+(116-68)*10,10)[9]
    if magic != bytes((0,0,0,1,0,22,0)) or coefficient != 140:
        raise ValueError("derived28原record或目标经验系数不符")
    resource = clip_contract((original/"FIGANI.DAT").read_bytes())
    source_contract = common.field_contract(original)
    patches = [{"offset":i,"before":a,"after":b} for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    allowed = set(range(UNITS,UNITS+12*80)) | set(range(META+5,META+9)) | set(range(SAVE_SIZE-4,SAVE_SIZE))
    if any(p["offset"] not in allowed for p in patches):
        raise AssertionError("derived28写出声明的active/focus/checksum域")
    manifest = {"scenario":SCENARIO,"controlled_fixture":True,"natural_action_history":False,
        "source_sha256":common.START_SHA256,"source_plain_sha256":common.START_PLAIN_SHA256,
        "field_sha256":common.FIELD_SHA256,"executable_sha256":common.EXE_SHA256,
        "template":{"stage":22,"entry":67,"index":1,"effective_count":metadata[2],
            "hex":template.hex(),"sha256":common.digest(template),"placement_entry":68,
            "placement_index":1,"placement":[21,30,19],"side":2,"unit_id":19,"level":20},
        "target_donor_sha256":common.DONOR_SHA256,"target_donor_raw":TARGET_DONOR_RAW,
        "target_record_sha256":TARGET_SHA256,"target_raw":TARGET_RAW,"caster_raw":CASTER_RAW,
        "caster_world":list(CASTER_WORLD),"target_world":list(TARGET_WORLD),
        "nonplayer_ai_stop_bit":4,"unknown_caster_bytes":"preserved from START raw1 outside confirmed constructor writes",
        "caster_readsets":[{"va":hex(0x61da1+19*24),"length":24,"sha256":common.digest(base)},
            {"va":hex(0x620a1+19*11),"length":11,"sha256":common.digest(growth)},
            {"va":"0x602ad","length":len(items),"sha256":common.digest(items)}],
        "magic_record_hex":magic.hex(),"enemy_reward_coefficient":coefficient,
        "command_gate":{"weapon_item":8,"weapon_min_range":1,"weapon_max_range":1,
            "attack_profile":0,"adjacent_enemy_raw":4,"default_selected":0,
            "initial_frames":[1,3,6,9],"select_magic_key":"LEFT","magic_selected":1,
            "magic_frames":[0,4,6,9],
            "basis":["item8+0x0b/+0x0c","0x61646 profile0 all-one costs",
                "0x173e7 first enabled","0x17898 LEFT selects slot1","0x179d5 command draw"]},
        "resource_contract":resource,"field_source":source_contract,
        "derived_sha256":common.digest(common.crypt(plain)),"derived_plain_sha256":common.digest(plain),
        "plaintext_patches":patches,"roster_manual_slots_metadata_unknown_tail_unchanged":True,
        "entry":"title CONTINUE; player 1cff0 single-target28 -> 2ff01 -> fixed slot2 2cf30",
        "runtime_memory_patch":False,"rng_stream_equivalence_claimed":False,
        "unsaved_state_not_measured":["0x53ec8 contribution","RNG","per-part HP write timing","UI owner"]}
    return common.crypt(plain),plain,manifest


def derived_outcomes(caster,target,coefficient,markers,cost):
    """1c81f signed core→2cf30 signed封顶/marker→18d8c归一；同roll关联。"""
    if len(caster)!=80 or len(target)!=80 or not word(target,66) or not caster[33]:
        raise ValueError("derived28结果需要完整记录及非零除数")
    power = trunc_div(signed16(word(caster,72))*20,10)-signed16(word(target,74))
    divisor = caster[33]+(30 if caster[32]>8 else 0)
    rows = []
    for roll in range(100):
        damage = trunc_div(9*power,10)+trunc_div(power*roll,1000)
        core_hp = max(0,word(target,64)-damage)
        reward_full = target[33]*coefficient
        raw = 0 if target[7]<68 else reward_full if not core_hp else trunc_div(signed32(damage*reward_full),word(target,66))
        gain = min(99,trunc_div(signed32(raw),divisor))
        capped = min(damage,signed16(word(target,64)))
        hp_steps = [(signed16(word(target,64))-trunc_div(capped*((i+1)&255),8))&65535 for i in range(markers)]
        rows.append({"spread":roll,"power":power,"raw_damage":damage,"signed_capped_damage":capped,
            "marker_hp":hp_steps,"hp_after":hp_steps[-1] if hp_steps else word(target,64),
            "raw_contribution":raw&0xffffffff,"gain":gain,
            "experience_after":(caster[60]+gain)&255,"mp_after":(word(caster,68)-cost)&65535})
    return rows


def expected_contract(original,source,scenario=SCENARIO,*,after_end=False):
    if scenario != SCENARIO:
        raise ValueError("未知派生结果合同")
    if type(after_end) is not bool:
        raise ValueError("derived28测量phase必须显式为bool")
    _,confirmed,manifest = derive_fixture(original)
    if source != confirmed:
        raise ValueError("derived28结果门只接受完整已登记前态")
    caster,target = common.actor(source,1),common.actor(source,4)
    plan = manifest["resource_contract"]
    outcomes = derived_outcomes(caster,target,manifest["enemy_reward_coefficient"],
        len(plan["effect_marker_parts"]),22*len(plan["cost_marker_parts"]))
    if any(not row["hp_after"] or not 0<row["gain"]<100 or caster[60]+row["gain"]>=100 for row in outcomes):
        raise ValueError("derived28夹具出现死亡、零奖励或升级，需独立新合同")
    end = None
    if after_end:
        phases={(1,1),(1,0),(2,2)}
        if source[META]!=1 or any((source[3+i*3],source[5+i*3]) in phases for i in range(16)):
            raise ValueError("derived28 END只覆盖stage0 turn1且无phase事件")
        for raw in range(source[META+1]):
            record=common.actor(source,raw)
            if any(record[34:40]) or not word(record,64) or record[6]==1 or (record[6]==0 and not record[5]&4):
                raise ValueError("derived28 END有未隔离的状态、死者或AI")
            if record[6]==2 and word(record,64)!=word(record,66):
                raise ValueError("derived28 END不隐含未行动友方休息恢复")
        if common.actor(source,0)[5]&1 or not any(common.actor(source,i)[6]==0 and not common.actor(source,i)[5]&1 for i in range(source[META+1])):
            raise ValueError("derived28 stage0会提前胜败")
        from run_original_focus import native_focus_reference
        camera=tuple(source[META+3:META+5]);leader=tuple(common.actor(source,0)[:2])
        for begin,destination in ((TARGET_WORLD,SAVE_WORLD),(SAVE_WORLD,leader),(leader,CASTER_WORLD),(CASTER_WORLD,SAVE_WORLD)):
            focus=native_focus_reference(camera,begin,destination)
            if tuple(focus["camera"])!=camera or any(tuple(s["camera"])!=camera for s in focus["steps"]):
                raise ValueError("derived28 END/Save需要未建模镜头变化")
        end={"phase_order":[1,0,2],"source_turn":1,"measurement_turn":2,"focus_after_end":list(leader),
            "camera":list(camera),
            "end_menu_world":list(SAVE_WORLD),"idle_heal":False,"ai_action":False,
            "poison_or_expiry":False,"new_effect_rng":False,"all_acted_cleared":True,
            "basis":["0x1a30b","0x13536","0x1a866","0x13a9f","0x205b4","0x12cea"]}
    gains=sorted({row["gain"] for row in outcomes})
    cx,cy=source[META+3:META+5]
    fixed=[{"offset":META+5,"hex":bytes((*SAVE_WORLD,SAVE_WORLD[0]-cx,SAVE_WORLD[1]-cy)).hex(),
            "basis":"declared final Save focus, fixed source camera"},
        {"offset":UNITS+80+68,"hex":struct.pack("<H",outcomes[0]["mp_after"]).hex(),
            "basis":"one original intro marker6, record28 cost22"}]
    if after_end:
        fixed.append({"offset":META,"hex":"02","basis":"exactly one source-projected END to turn2"})
        fixed.extend({"offset":UNITS+raw*80+5,"hex":bytes((common.actor(source,raw)[5]&127,)).hex(),
            "basis":"13536 clears acted in all active records, preserves other bits"} for raw in range(source[META+1]))
    else:
        fixed.append({"offset":UNITS+80+5,"hex":bytes((caster[5]|128,)).hex(),
            "basis":"successful 18890 action sets caster acted"})
    return {"scenario":SCENARIO,"outcomes":outcomes,"resource_contract":plan,
        "fixed_updates":fixed,"finite_bytes":[],
        "correlated_fields":[{"offset":UNITS+4*80+64,"length":2,"meaning":"target HP"},
            {"offset":UNITS+80+60,"length":1,"meaning":"caster EX; same spread as target HP"}],
        "correlated_fields_must_not_be_accepted_independently":True,
        "gains":gains,"reward_pages":[{"kind":"reward","fragment":488,"value":gain,"line":0,
            "requires_key":False,"final_wait_ticks":11,"portrait_actor_id":19} for gain in gains],
        "reward_page_relation":"one alternative matching the same spread as target HP; not three sequential pages",
        "selection":{"cast":1,"area":0,"filter":0,"display":2,"cursor_frame":1,"target_raw":4},
        "expected_focus_before_save_navigation":list(common.actor(source,0)[:2]) if after_end else list(TARGET_WORLD),
        "measurement_focus":list(SAVE_WORLD),"measurement_phase":"after_one_end" if after_end else "immediate_after_action",
        "formal_save_available":after_end,"end_turn":end,"full_save_bytes":SAVE_SIZE,
        "boundary":"one derived28 action, no death/loot/level; one isolated END to turn2" if after_end else
            "one derived28 action; immediate predicted state, formal Save still requires source-projected END",
        "rng_stream_equivalence_claimed":False,"per_part_runtime_write_trace_claimed":False}


def expected_end_contract(original,source,scenario=SCENARIO):
    return expected_contract(original,source,scenario,after_end=True)


def expected_sources(original,source,*,engine,after_end=False):
    if engine not in ("original","sdl"):
        raise ValueError("derived28必须声明original或sdl")
    contract=expected_contract(original,source,after_end=after_end)
    candidates={}
    for row in contract["outcomes"]:
        result=bytearray(source)
        if engine=="original":
            identities=[]
            for raw in range(source[META+1]):
                identity=common.actor(source,raw)[7]
                if identity not in identities:identities.append(identity)
                result[UNITS+raw*80+2]=identities.index(identity)
        result[UNITS+80+5]|=128
        result[UNITS+80+60]=row["experience_after"]
        struct.pack_into("<H",result,UNITS+80+68,row["mp_after"])
        struct.pack_into("<H",result,UNITS+4*80+64,row["hp_after"])
        if after_end:
            result[META]=2
            for raw in range(source[META+1]):result[UNITS+raw*80+5]&=127
        cx,cy=source[META+3:META+5]
        result[META+5:META+9]=bytes((*SAVE_WORLD,SAVE_WORLD[0]-cx,SAVE_WORLD[1]-cy))
        final=common.checksum(result)
        candidates.setdefault(final,[]).append(row["spread"])
    return list(candidates),contract


def check_result(original,source,scenario,measured,*,engine,after_end=False):
    if scenario != SCENARIO:
        raise ValueError("未知派生测量场景")
    common.valid_plain(measured)
    candidates,contract=expected_sources(original,source,engine=engine,after_end=after_end)
    nearest=min(candidates,key=lambda expected:sum(a!=b for a,b in zip(expected,measured)))
    mismatch=[i for i,(a,b) in enumerate(zip(nearest,measured)) if a!=b]
    return {"ok":not mismatch,"scenario":SCENARIO,"engine":engine,
        "measurement_phase":contract["measurement_phase"],"finite_correlated_states":len(candidates),
        "checked_container_bytes":SAVE_SIZE,"mismatch_count":len(mismatch),"mismatch_offsets":mismatch[:32],
        "raw_damage_hp_experience_linked":True,"rng_stream_equivalence_claimed":False,
        "per_part_runtime_write_trace_claimed":False}


def phase_probes(original,source,scenario,assets):
    """正式range1/area0的首层正证；不把若干帧数等同完成marker。"""
    expected_contract(original,source,scenario)
    from run_original_comparison import archive_entry
    from run_original_player import sheet_frame, screen
    from run_original_magic_list import actor_focus_resources
    from run_original_item_list import use_range_probe
    original=Path(original)
    sheet=archive_entry((original/"FDOTHER.DAT").read_bytes(),1)
    cursor=sheet_frame(sheet,1);cursor.putpalette(assets["palette"])
    cycle=common.range_phase_contract(original)
    probe=use_range_probe(original,source,assets,1,0,TARGET_WORLD,caster_raw=1)
    probe=dict(probe,ranges=[(phase,image) for phase,image in probe["ranges"]
        if phase in cycle["used_lut_frames"]])
    if not probe["ranges"]:
        raise ValueError("derived28没有实际原序列内的正向range资源")
    return {"target":actor_focus_resources(original,source,assets,TARGET_RAW,cursor),
        "target_screen":screen(source,TARGET_WORLD),"cast_range":probe,
        "original_range_cycle":cycle,"display":2,"cursor_frame":1,
        "scope":"one positive colored cast tile plus focused target; no claim that every LUT differs from base",
        "after_action_focus":list(TARGET_WORLD),"reward_portrait_actor_id":19}


def movement_probe(original,source,assets):
    """4e390起点不套进入ZOC门；仅已登记源从(7,14)可走到(6,14)。"""
    expected_contract(original,source)
    from run_original_comparison import archive_entry
    from run_original_item_list import use_range_probe
    from run_original_player import screen
    original=Path(original);caster=common.actor(source,1);destination=(6,14)
    executable=(original/"FD2.EXE").read_bytes()
    profile=source_range(executable,0x61646+caster[32]*20,20)
    field=common._archives((original/"FDFIELD.DAT").read_bytes())
    grid,metadata=field[0],field[1]
    if struct.unpack_from("<HH",grid)!=(24,24):
        raise ValueError("derived28移动门只支持stage0的24×24地图")
    shape=(original/"FDSHAP.DAT").read_bytes()
    if common.digest(shape)!=common.SHAPE_SHA256:
        raise ValueError("derived28移动门shape身份不符")
    attributes=archive_entry(shape,metadata[0]*2+1)
    cell=struct.unpack_from("<I",grid,4+(destination[1]*24+destination[0])*4)[0]
    terrain=cell&1023;flags,cost_class=attributes[terrain*4:terrain*4+2]
    units=[common.actor(source,i) for i in range(source[META+1])]
    visible=[u for u in units if not u[5]&1]
    enemies=[u for u in visible if u[6]==0]
    if (tuple(caster[:2])!=CASTER_WORLD or caster[32]!=9 or not caster[59] or
            flags or (cell>>16)&31 or cost_class>=len(profile) or
            not 0<profile[cost_class]<=min(caster[59],19)):
        raise ValueError("derived28左邻移动成本／事件／profile前置不符")
    if any(tuple(u[:2]) in (destination,(6,15)) for u in visible) or any(
            abs(u[0]-destination[0])+abs(u[1]-destination[1])<=1 for u in enemies):
        raise ValueError("derived28左邻或下格被占用，或目标格处在敌方ZOC")
    if not any(abs(u[0]-caster[0])+abs(u[1]-caster[1])<=1 for u in enemies):
        raise ValueError("derived28窄移动门要求已登记的起点邻敌前置")
    # 这里只复用完整地形/LUT材料解码；profile9的单步可达性已在上面独立核定。
    # area4参数避免材料helper人为排除邻格，不给游戏写area或目标状态。
    probe=use_range_probe(original,source,assets,1,4,CASTER_WORLD,caster_raw=1)
    if tuple(probe["world"])!=destination or any(
            pixel==0 or 224<=pixel<240 for pixel in probe["base"].getdata()):
        raise ValueError("derived28移动正证必须为左邻完整非零／非水色静态tile")
    used=common.range_phase_contract(original)["used_lut_frames"]
    probe=dict(probe,ranges=[(i,image) for i,image in probe["ranges"] if i in used],
        actor_position=screen(source,CASTER_WORLD),
        movement_source={"profile":9,"profile_va":hex(0x61646+9*20),
            "profile_sha256":common.digest(profile),"terrain":terrain,"cost_class":cost_class,
            "one_step_cost":profile[cost_class],"movement_points":caster[59],
            "start_adjacent_enemy":True,"destination_zoc":False,"actor_actually_moved":False,
            "basis":"4e390/4e680 start seed bypasses destination ZOC entry; explicit one-step left"})
    if not probe["ranges"]:
        raise ValueError("derived28移动门没有原序列内的完整有色tile")
    return probe
