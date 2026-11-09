"""G2原版来源夹具和完整SAV有限结果门；不启动窗口、不写原资源。

1cff0/1d1b0/115b6、2d80d/22721/22866/22997与10010是依据。
独立读取原EXE/FIGANI和真实80B donor，不读取SDL冷表或观测终态造期望。
只检验明确列出的语义族，不承诺两端RNG轨迹或逐帧动画相同。
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import sys

SAVE_SIZE, META, UNITS, STRIDE = 0x59CB, 0x30C3, 0x12A3, 80
SCENARIOS = ("composite34", "teleport23-cancel-destination", "teleport23-success")
START_SHA256 = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"
START_PLAIN_SHA256 = "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975"
DONOR_SHA256 = "2efc678871b8b019023f7a22758f6afd78a2aab2ad352a6a4a22a4c7ab15d2eb"
DONOR_RECORD_SHA256 = "a12c83ec3f791f8d8ef1eb71ad2a73982b095bf7ff9ce21eab74a4515b397213"
EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
FIGANI_SHA256 = "c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8"
FIELD_SHA256 = "81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d"
SHAPE_SHA256 = "901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c"
DONOR_MAGIC_IDS = [0, 1, 2, 3, 17, 23, 26, 27, 32, 33, 34, 35]
SAVE_WORLD, CASTER_WORLD = (6, 14), (7, 14)
TELEPORT_DESTINATION = (10, 14)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def crypt(data):
    """已证save_xor_crypt：加9014／ROL3，XOR对称。"""
    state, result = 0xA5, bytearray(data)
    for index in range(len(result)):
        state = (state + 0x9014) & 65535
        state = ((state << 3) | (state >> 13)) & 65535
        result[index] ^= state & 255
    return bytes(result)


def valid_plain(data):
    if not isinstance(data, bytes) or len(data) != SAVE_SIZE or sum(data[:-4]) != int.from_bytes(data[-4:], "little"):
        raise ValueError("G2需要完整checksum有效的22987B活动SAV")
    if not 1 <= data[META+1] <= 96 or data[META+9] > 32:
        raise ValueError("G2活动／持久owner计数越界")
    return data


def checksum(data):
    result = bytearray(data)
    result[-4:] = sum(result[:-4]).to_bytes(4, "little")
    return bytes(result)


def actor(source, raw):
    if type(raw) is not int or not 0 <= raw < source[META+1]:
        raise ValueError("G2 actor raw越出实际活动表")
    return source[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]


def magic_ids(record):
    return [identity for identity in range(40) if record[26+identity//8] & (1 << (identity % 8))]


def _word(data, at):
    return struct.unpack_from("<H", data, at)[0]


def _archives(data):
    # 完整DAT目录／EOF哨兵校验，不扫描字节猜分界。
    sys.path.insert(0, str(Path(__file__).resolve().parent / "static_data"))
    from audit_stage_data import archive_entries
    return [payload for _, payload in archive_entries(data)]


def resource_contract(original, magic_id, *, committed=False):
    """34费用读取52*3+1专用clip；23成功走一次费用，取消不走wrapper。"""
    original = Path(original)
    sys.path.insert(0, str(Path(__file__).resolve().parent / "static_data"))
    from verify_unit_tables import source_range
    executable = (original / "FD2.EXE").read_bytes()
    if digest(executable) != EXE_SHA256 or magic_id not in (23, 34):
        raise ValueError("G2魔法来源／当前有限场景不符")
    record = source_range(executable, 0x619FD + magic_id*7, 7)
    result = {"magic_id": magic_id, "magic_record_hex": record.hex(),
              "magic_record_va": hex(0x619FD+magic_id*7), "magic_record_sha256": digest(record),
              "executable_sha256": EXE_SHA256, "cost": record[5], "cost_count": int(magic_id==23 and committed)}
    if magic_id==23 and committed:
        result["cost_basis"]="2218a -> 1ca89(caster,23), exactly one WORD subtraction"
    if magic_id == 34:
        data = (original / "FIGANI.DAT").read_bytes()
        if digest(data) != FIGANI_SHA256:
            raise ValueError("G2 FIGANI来源身份不符")
        index = 52*3+1
        clip = _archives(data)[index]
        count, intro = _word(clip, 0), clip[2]
        if not count or intro > count or 8+count*4 > len(clip):
            raise ValueError("G2复合clip目录／intro越界")
        markers = []
        for part in range(count):
            begin = struct.unpack_from("<I", clip, 8+part*4)[0]
            end = struct.unpack_from("<I", clip, 12+part*4)[0] if part+1 < count else len(clip)
            if not 8+count*4 <= begin < end <= len(clip) or end-begin < 9:
                raise ValueError("G2 FIGANI part范围不完整")
            if part < intro and clip[begin+4] == 1:
                markers.append(part)
        result.update(figani_sha256=FIGANI_SHA256, figani_entry=index, figani_entry_sha256=digest(clip),
                      intro_end=intro, cost_marker_parts=markers, cost_count=len(markers),
                      cost_basis="2d80d -> intro part+4==1 -> 1ca89; no second 17/18/19 wrapper cost")
    return result


def field_contract(original):
    """caster/测量格的普通地形与无事件门，避免隔离夹具暗触剧情。"""
    original=Path(original)
    field,shape=(original/"FDFIELD.DAT").read_bytes(),(original/"FDSHAP.DAT").read_bytes()
    if digest(field)!=FIELD_SHA256 or digest(shape)!=SHAPE_SHA256:
        raise ValueError("G2原地图／shape来源身份不符")
    entries,shapes=_archives(field),_archives(shape)
    grid,metadata=entries[0],entries[1]
    width,height=struct.unpack_from("<HH",grid)
    attrs=shapes[metadata[0]*2+1]
    if (width,height)!=(24,24):raise ValueError("G2夹具限定原stage0地图")
    cells=[]
    for x,y in (CASTER_WORLD,SAVE_WORLD,(11,14),(12,14),(9,14)):
        cell=struct.unpack_from("<I",grid,4+(y*width+x)*4)[0];terrain=cell&1023
        attribute=attrs[terrain*4:terrain*4+4]
        if len(attribute)!=4 or attribute[0]&0x60 or (cell>>16)&31:
            raise ValueError("G2夹具caster/测量/目标格存在未隔离原事件")
        cells.append({"world":[x,y],"raw_u32":cell,"terrain":terrain,"attribute_bytes":list(attribute)})
    return {"stage":0,"map_entry":0,"metadata_entry":1,"shape_attribute_entry":metadata[0]*2+1,
            "sources":{"FDFIELD.DAT":FIELD_SHA256,"FDSHAP.DAT":SHAPE_SHA256},"ordinary_event_free_cells":cells}


def teleport_effect_contract(original,source):
    """23成功的有限目的地／贡献域；不以选点距离代替原mode6站立门。"""
    original=Path(original);valid_plain(source)
    executable=(original/"FD2.EXE").read_bytes()
    if digest(executable)!=EXE_SHA256:raise ValueError("G2传送成功需要已核原EXE")
    from verify_unit_tables import source_range
    field=(original/"FDFIELD.DAT").read_bytes();shape=(original/"FDSHAP.DAT").read_bytes()
    if digest(field)!=FIELD_SHA256 or digest(shape)!=SHAPE_SHA256:
        raise ValueError("G2传送目的地原地图/shape身份不符")
    entries,shapes=_archives(field),_archives(shape)
    grid,metadata=entries[0],entries[1]
    width,height=struct.unpack_from("<HH",grid)
    if (width,height)!=(24,24):raise ValueError("G2传送目的地限定原stage0地图")
    caster,target=actor(source,1),actor(source,0)
    if tuple(caster[:2])!=CASTER_WORLD or tuple(target[:2])!=(9,14) or target[7]!=0 or target[32]!=1:
        raise ValueError("G2传送只覆盖已核caster7,14与target0/profile1在9,14")
    destination=TELEPORT_DESTINATION
    if any(not actor(source,raw)[5]&1 and tuple(actor(source,raw)[:2])==destination for raw in range(source[META+1])):
        raise ValueError("G2传送目的地被其它可见actor占用")
    cell=struct.unpack_from("<I",grid,4+(destination[1]*width+destination[0])*4)[0]
    terrain=cell&1023;attrs=shapes[metadata[0]*2+1]
    attribute=attrs[terrain*4:terrain*4+4]
    if len(attribute)!=4 or attribute[0]&0x60 or (cell>>16)&31:
        raise ValueError("G2传送目的地含未隔离事件")
    profile=source_range(executable,0x61646+target[32]*20,20)
    cost_class=attribute[1]
    if cost_class>=len(profile) or profile[cost_class]==20:
        raise ValueError("G2传送目标原profile不能站立目的地")
    magic=source_range(executable,0x619fd+23*7,7)
    distance=abs(caster[0]-target[0])+abs(caster[1]-target[1])
    if not 1<=distance<=magic[3] or magic[6]!=3:
        raise ValueError("G2传送首目标不在原最小1/record.range/filter3域")
    weighted=target[33]+(30 if 8<target[32]<25 else 0)
    divisor=caster[33]+(30 if caster[32]>8 else 0)
    if not divisor or (10*weighted)//divisor!=0 or caster[33]!=40:
        raise ValueError("G2传送夹具不再属于无奖励/无升级的已证域")
    from run_original_focus import native_focus_reference
    camera=tuple(source[META+3:META+5])
    paths=((CASTER_WORLD,(9,14)),((9,14),destination),(destination,CASTER_WORLD),
           (CASTER_WORLD,(9,14)),((9,14),destination),(destination,SAVE_WORLD),
           (SAVE_WORLD,destination),(destination,CASTER_WORLD),(CASTER_WORLD,SAVE_WORLD))
    for start,end in paths:
        projection=native_focus_reference(camera,start,end)
        if tuple(projection["camera"])!=camera or any(tuple(step["camera"])!=camera for step in projection["steps"]):
            raise ValueError("G2传送/END/Save导航需未建模卷屏")
    return {"target_raw":0,"target_old_world":[9,14],"destination":list(destination),
        "raw_cell_u32":cell,"terrain":terrain,"attribute_bytes":list(attribute),
        "profile":target[32],"movement_profile_va":hex(0x61646+target[32]*20),
        "movement_profile_sha256":digest(profile),"stand_cost":profile[cost_class],
        "raw_contribution":10*weighted,"caster_divisor":divisor,"normalized_gain":0,
        "focus_before_effect":list(CASTER_WORLD),"focus_after_success":list(destination),
        "basis":"1d270回caster在效果前；2218a随后focus旧target→目的地，1cff0/18d8c尾无再聚焦"}


def derive_fixture(original, scenario):
    if scenario not in SCENARIOS:
        raise ValueError("未知G2有限场景")
    original = Path(original)
    encrypted, donor_encrypted = (original/"START.SAV").read_bytes(), (original/"FD2.SAV").read_bytes()
    if digest(encrypted) != START_SHA256 or digest(donor_encrypted) != DONOR_SHA256:
        raise ValueError("G2 START／真实donor SAV身份不符")
    source, donor = valid_plain(crypt(encrypted)), valid_plain(crypt(donor_encrypted))
    copied = actor(donor, 1)
    if (digest(source) != START_PLAIN_SHA256 or source[META+2] != 0 or source[META+1] != 12 or
            digest(copied) != DONOR_RECORD_SHA256 or copied[7:9] != bytes((52,9)) or
            copied[32:34] != bytes((21,40)) or _word(copied,68) != 1414 or magic_ids(copied) != DONOR_MAGIC_IDS):
        raise ValueError("G2 donor不是已核真实unit52/text9/profile21/level40记录")
    magic_id = 34 if scenario == "composite34" else 23
    resource = resource_contract(original, magic_id,committed=scenario=="teleport23-success")
    field = field_contract(original)
    records = [bytearray(actor(source, raw)) for raw in range(source[META+1])]
    records[1] = bytearray(copied)
    for raw, record in enumerate(records):
        record[3:5] = b"\0\0"  # 明列隔离运动状态，不从截图推断。
        if raw < 4:
            record[5] &= 0x7E
            record[34:40] = bytes(6)
        else:
            record[5] |= 4
            record[:2] = bytes((20+(raw-4)%3, 2+(raw-4)//3))
    records[1][:2] = bytes(CASTER_WORLD)
    records[1][26:31] = bytes(5)
    records[1][26+magic_id//8] = 1 << (magic_id % 8)  # 仅保留donor已拥有的单bit。
    records[0][:2] = bytes((11,14) if magic_id == 34 else (9,14))
    records[2][:2] = bytes((12,14) if magic_id == 34 else (1,21))
    records[3][:2] = bytes((2,21))
    controlled = bytearray(source)
    for raw, record in enumerate(records):
        controlled[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE] = record
    cx, cy = source[META+3:META+5]
    controlled[META+5:META+9] = bytes((*CASTER_WORLD, CASTER_WORLD[0]-cx, CASTER_WORLD[1]-cy))
    plain = checksum(controlled)
    patches = [{"offset": at, "before": a, "after": b} for at,(a,b) in enumerate(zip(source,plain)) if a != b]
    allowed = set(range(UNITS,UNITS+12*STRIDE)) | set(range(META+5,META+9)) | set(range(SAVE_SIZE-4,SAVE_SIZE))
    if any(p["offset"] not in allowed for p in patches):
        raise AssertionError("G2夹具写出了声明的active/focus/checksum域")
    encoded = crypt(plain)
    targets = [0,2] if magic_id == 34 else [0]
    manifest = {"scenario": scenario, "controlled_fixture": True, "natural_action_history": False,
                "source_sha256": START_SHA256, "source_plain_sha256": START_PLAIN_SHA256,
                "donor_sha256": DONOR_SHA256, "donor_actor_raw": 1, "donor_record_sha256": DONOR_RECORD_SHA256,
                "derived_sha256": digest(encoded), "derived_plain_sha256": digest(plain),
                "plaintext_patches": patches, "caster_raw": 1, "caster_world": list(CASTER_WORLD),
                "magic_id": magic_id, "targets": targets, "center": list(records[0][:2]),
                "save_measurement_world": list(SAVE_WORLD), "resource_contract": resource,"field_source":field,
                "roster_manual_slots_metadata_unknown_tail_unchanged": True,
                "full_save_guard": True, "rng_stream_equivalence_claimed": False,
                "unsaved_state_not_measured":["0x53ec8 contribution","RNG","UI modal/cursor owner"],
                "entry": "real title CONTINUE of isolated fixture; no runtime-memory hook"}
    if scenario=="teleport23-success":manifest["teleport_contract"]=teleport_effect_contract(original,plain)
    return encoded, plain, manifest


def source_actors(source):
    valid_plain(source)
    caster = actor(source,1)
    ids = magic_ids(caster)
    if caster[7:9] != bytes((52,9)) or len(ids) != 1 or ids[0] not in (23,34):
        raise ValueError("G2资源helper只接受真实donor派生的单项34／23")
    return {"caster_raw":1, "unit_id":52, "text_id":9, "magic_ids":ids,
            "magic_name_fragment":441+ids[0], "caster_world":list(caster[:2]),
            "mp":_word(caster,68), "mp_max":_word(caster,70), "record_sha256":digest(caster)}


def expected_contract(original, source, scenario):
    valid_plain(source)
    if scenario not in SCENARIOS:
        raise ValueError("未知G2有限结果门")
    _, prepared, manifest = derive_fixture(original, scenario)
    if source != prepared:
        raise ValueError("G2结果门只接受完整已登记夹具，不从任意观测构造前态")
    wanted = 34 if scenario == "composite34" else 23
    if source_actors(source)["magic_ids"] != [wanted] or source[META+2] != 0:
        raise ValueError("G2结果门与夹具场景不符")
    resource = manifest["resource_contract"]
    fixed, finite = [], []
    def write(at, data, basis):
        fixed.append({"offset":at,"hex":bytes(data).hex(),"basis":basis})
    cx,cy=source[META+3:META+5]
    write(META+5,(*SAVE_WORLD,SAVE_WORLD[0]-cx,SAVE_WORLD[1]-cy),"declared empty-cell Save measurement; no camera scroll")
    if wanted == 34:
        center=tuple(actor(source,0)[:2])
        targets=[raw for raw in range(source[META+1]) if not actor(source,raw)[5]&1 and actor(source,raw)[6]==2
                 and abs(actor(source,raw)[0]-center[0])+abs(actor(source,raw)[1]-center[1])<=3]
        if targets != [0,2] or actor(source,1)[33] != 40:
            raise ValueError("G2复合34目标组／奖励gate前置不符")
        caster=actor(source,1);at=UNITS+STRIDE
        write(at+5,(caster[5]|128,),"18890/18d8c successful action marks caster acted")
        write(at+68,struct.pack("<H",(_word(caster,68)-resource["cost"]*resource["cost_count"])&65535),"original intro marker cost count")
        for raw in targets:
            record=actor(source,raw);start=UNITS+raw*STRIDE
            if record[34:37] != bytes(3):
                raise ValueError("G2增益原status必须为0才写；不可用重复施加混淆本门")
            ap,dp,hit,ev=struct.unpack_from("<4H",record,72)
            values=((ap+(15*ap+100)//100)&65535,(dp+(15*dp+100)//100)&65535,(hit+15)&65535,(ev+15)&65535)
            write(start+72,struct.pack("<4H",*values),"22721/22866 direct integer increment; 22997 WORD+15; no post-target1b750")
            for offset in (34,35,36):
                finite.append({"offset":start+offset,"values":[2,3,4,5],"basis":"one original rng%4+2 per field/core; not a shared RNG trajectory"})
    success=scenario=="teleport23-success"
    if success:
        transfer=manifest["teleport_contract"]
        if transfer["normalized_gain"]!=0 or resource["cost_count"]!=1:
            raise ValueError("G2传送成功费用/无奖励合同不符")
        write(UNITS,transfer["destination"],"22547第二段提交target0目的地，原身份/状态/HP/EX保持")
        write(UNITS+STRIDE+5,(actor(source,1)[5]|128,),"18890/18d8c successful action marks caster acted")
        write(UNITS+STRIDE+68,struct.pack("<H",(_word(actor(source,1),68)-resource["cost"])&65535),
              "2218a -> 1ca89 record23 fee once; target weighted contribution10 / caster70 =0")
    return {"scenario":scenario,"fixed_updates":fixed,"finite_bytes":finite,"resource_contract":resource,
            "expected_focus_before_save_navigation":list(TELEPORT_DESTINATION) if success else
                list(actor(source,0)[:2]) if wanted==34 else list(CASTER_WORLD),
            "measurement_focus":list(SAVE_WORLD),
            "turn_stage_roster_slots_unknown_bytes_unchanged":True,"checksum":"exact full-container sum",
            "unsaved_state_not_measured":["0x53ec8 contribution","RNG","UI modal/cursor owner"],
            "formal_save_available":wanted!=34 and not success,
            "measurement_phase":"immediate",
            "boundary":"immediate action/cancel only; no END or phase tick"}


def expected_end_contract(original, source, scenario):
    """34或23成功后的恰好一次正式END；不从测量值推导写集或计时。

    1a30b的idle恢复、1→0→2状态tick与13536清acted均有原版依据。
    本窄夹具全部HP已满、全员初始六状态0、无side1、side0为原04停止，
    且对应phase无turn事件。34的2..5仅递减到1..4，不会归零调用1b750；
    23不新增计时，其raw0已移动到10,14，END末尾聚焦这个新坐标。
    因而四派生保留22721/22866/22997的直接增量，而非装备重算结果。
    """
    if scenario not in ("composite34","teleport23-success"):
        raise ValueError("G2 END结果门只覆盖已证34/23成功窄夹具，23取消不追加END")
    contract=expected_contract(original,source,scenario)
    turn=source[META]
    if turn!=1 or source[META+2]!=0:
        raise ValueError("G2 END仅覆盖stage0的turn1到turn2")
    phases={(turn,1),(turn,0),(turn+1,2)}
    if any((source[3+slot*3],source[5+slot*3]) in phases for slot in range(16)):
        raise ValueError("G2 END夹具有尚未纳入结果门的回合事件")
    visible_enemy=False
    for raw in range(source[META+1]):
        record=actor(source,raw)
        if any(record[34:40]) or not _word(record,64) or _word(record,64)!=_word(record,66):
            raise ValueError("G2 END要求原状态0且HP满，不能隐含恢复/毒伤/归零重算")
        if record[6]==1 or (record[6]==0 and not record[5]&4):
            raise ValueError("G2 END夹具存在未隔离的NPC或敌方AI")
        if record[6]==0 and not record[5]&1:
            visible_enemy=True
    if actor(source,0)[5]&1 or not visible_enemy:
        raise ValueError("G2 END的stage0结果门会提前结束战场")
    # 正式END从空测量格进入，1a30b末尾12d7b(raw0)再回leader；
    # 随后的Save导航同样必须保持本夹具camera，不能抄observed镜头。
    from run_original_focus import native_focus_reference
    camera=tuple(source[META+3:META+5])
    leader=TELEPORT_DESTINATION if scenario=="teleport23-success" else tuple(actor(source,0)[:2])
    for start,end in ((leader,SAVE_WORLD),(SAVE_WORLD,leader),(leader,CASTER_WORLD),(CASTER_WORLD,SAVE_WORLD)):
        projected=native_focus_reference(camera,start,end)
        if tuple(projected["camera"])!=camera or any(tuple(step["camera"])!=camera for step in projected["steps"]):
            raise ValueError("G2 END/Save导航越出已证固定镜头域")
    caster_flag=UNITS+STRIDE+5
    contract["fixed_updates"]=[row for row in contract["fixed_updates"] if row["offset"]!=caster_flag]
    contract["fixed_updates"].append({"offset":META,"hex":bytes((turn+1,)).hex(),
        "basis":"1a30b side0完成后DWORD回合+1，原Save仅写低BYTE；本门只到turn2"})
    for raw in range(source[META+1]):
        contract["fixed_updates"].append({"offset":UNITS+raw*STRIDE+5,
            "hex":bytes((actor(source,raw)[5]&0x7f,)).hex(),
            "basis":"13536全表flags&=7f；04停止和hidden等其它bit保持"})
    for row in contract["finite_bytes"]:
        row["values"]=[value-1 for value in row["values"]]
        row["basis"]="34原rng%4+2后，1a866新side2仅递减一次；1..4不归零，不触发1b750"
    contract.update(formal_save_available=True,measurement_phase="after_one_end",
        expected_focus_before_save_navigation=list(leader),
        boundary=("one 34 action then exactly one END to turn2; finite 1..4 timers, no expiry or AI action" if scenario=="composite34" else
            "one successful teleport23 to10,14 then exactly one END to turn2; no reward, timer or AI action"),
        end_turn={"phase_order":[1,0,2],"source_turn":turn,"measurement_turn":turn+1,
            "end_menu_world":list(SAVE_WORLD),"focus_after_end":list(leader),"camera":list(camera),
            "original_functions":["0x1a30b","0x13536","0x1a866","0x13a9f","0x1d80b","0x1d8ba","0x205b4"],
            "idle_heal":False,"ai_action":False,"status_expiry":False,
            "combat_stats_recompute":False,"rng_stream_equivalence_claimed":False,
            "duration_countdown_exact_pair_observed":False})
    return contract


def check_result(original, source, scenario, measured, *, engine, after_end=False):
    """只有预声明六个2..5 byte可取实测有限值，余下全部22987B独立守卫。"""
    valid_plain(measured)
    if engine not in ("original","sdl"):
        raise ValueError("G2结果必须声明original或sdl")
    if type(after_end) is not bool:
        raise ValueError("G2测量phase必须明确为布尔after_end")
    contract=(expected_end_contract if after_end else expected_contract)(original,source,scenario)
    expected=bytearray(source)
    if engine == "original":
        # 10010的实际Load写+2可独立重算，绝不从observed任意抄cache byte。
        ids=[]
        for raw in range(source[META+1]):
            identity=actor(source,raw)[7]
            if identity not in ids: ids.append(identity)
            expected[UNITS+raw*STRIDE+2]=ids.index(identity)
    for update in contract["fixed_updates"]:
        data=bytes.fromhex(update["hex"]);expected[update["offset"]:update["offset"]+len(data)]=data
    invalid=[]
    for rule in contract["finite_bytes"]:
        at=rule["offset"]
        if measured[at] not in rule["values"]: invalid.append({"offset":at,"actual":measured[at],"allowed":rule["values"]})
        else: expected[at]=measured[at]
    expected=checksum(expected)
    mismatch=[at for at in range(SAVE_SIZE) if expected[at]!=measured[at]]
    return {"ok":not invalid and not mismatch,"scenario":scenario,"engine":engine,
            "finite_byte_violations":invalid,"mismatch_count":len(mismatch),"mismatch_offsets":mismatch[:32],
            "checked_container_bytes":SAVE_SIZE,"finite_duration_bytes":len(contract["finite_bytes"]),
            "measurement_phase":contract["measurement_phase"],
            "rng_stream_equivalence_claimed":False,"scope":contract["boundary"]}


def range_phase_contract(original):
    """原11eee/13b54使用51a97的20步序列；不借SDL现有子集。"""
    sys.path.insert(0,str(Path(__file__).resolve().parent/"static_data"))
    from verify_unit_tables import source_range
    executable=(Path(original)/"FD2.EXE").read_bytes()
    if digest(executable)!=EXE_SHA256:
        raise ValueError("G2范围色周期需要已核原EXE身份")
    raw=source_range(executable,0x51a97,20)
    if list(raw)!=[*range(11),*range(9,0,-1)]:
        raise ValueError("G2原20步范围色序列与已证51a97合同不符")
    return {"canonical_va":"0x51a97","byte_length":20,"raw_hex":raw.hex(),"sha256":digest(raw),
            "sequence":list(raw),"used_lut_frames":sorted(set(raw)),"executable_sha256":EXE_SHA256,
            "sdl_cycle_equivalence_claimed":False}


def phase_probes(original, source, scenario, assets):
    """给串行runner的资源门目录；不含输入动作、不把ENTER次数当阶段证据。"""
    expected_contract(original,source,scenario)  # 资源门也只接受完整已登记前态。
    from run_original_comparison import archive_entry
    from run_original_player import sheet_frame, screen
    from run_original_magic_list import actor_focus_resources
    from run_original_item_list import use_range_probe
    original=Path(original);source_actors(source)
    sheet=archive_entry((original/"FDOTHER.DAT").read_bytes(),1)
    # magic23 global display始终2直到成功/取消，mode6仍frame1，不套item23 frame0。
    cursor=sheet_frame(sheet,1);cursor.putpalette(assets["palette"])
    if scenario in ("teleport23-cancel-destination","teleport23-success"):
        cycle=range_phase_contract(original)
        used=cycle["used_lut_frames"]
        center=tuple(actor(source,0)[:2])
        probe=use_range_probe(original,source,assets,3,0,center,caster_raw=1,require_lut_frames=tuple(used))
        probe=dict(probe,ranges=[(phase,image) for phase,image in probe["ranges"] if phase in used],
                   required_lut_frames=used)
        result={"target":actor_focus_resources(original,source,assets,0,cursor),"target_screen":screen(source,center),
                "first_layer_colored_range":probe,"destination_restored_terrain":probe,
                "required_order":["first_layer_colored_range","destination_full_base_tile_and_target_frame1","Esc"],
                "native_phase_calibrated":False,
                "positive_proof":"same complete ROI: all eleven original used cast LUT frames differ from base; mode6 must restore base",
                "original_range_cycle":cycle,
                "initial_destination_focus":list(center),"after_cancel_focus":list(CASTER_WORLD),
                "basis":"1d188/1d238 global display2,115b6 mode6 local zero-range,1d3c9..1d3e2 cancel"}
        if scenario=="teleport23-success":
            transfer=teleport_effect_contract(original,source)
            field=_archives((original/"FDFIELD.DAT").read_bytes())
            shapes=_archives((original/"FDSHAP.DAT").read_bytes())
            grid,metadata=field[0],field[1];width=struct.unpack_from("<H",grid)[0]
            x,y=TELEPORT_DESTINATION
            cell=struct.unpack_from("<I",grid,4+(y*width+x)*4)[0]
            attrs=shapes[metadata[0]*2+1]
            if cell>>10 or attrs[(cell&1023)*4]:
                raise ValueError("G2传送目的地资源门不接受overlay/事件或未建模地形")
            occupied={tuple(actor(source,raw)[:2]) for raw in range(source[META+1]) if not actor(source,raw)[5]&1}
            if (x,y) in occupied or (x,y+1) in occupied:
                raise ValueError("G2传送目的格或下格actor会遮挡完整base正证")
            base=sheet_frame(shapes[metadata[0]*2],cell&1023);base.putpalette(assets["palette"])
            if any(224<=pixel<240 for pixel in base.getdata()):
                raise ValueError("G2传送目的地不能借动态水色像素作固定正证")
            composite=base.copy()
            composite.paste(cursor,(0,0),cursor.point(lambda pixel:255 if pixel else 0,mode="L"))
            result.update(destination_selected={"world":[x,y],"position":screen(source,(x,y)),
                    "base":base,"cursor":cursor,"composite":composite,"cursor_frame":1,
                    "complete_tile_required":True,"restored_range_probe":probe,
                    "target_old_world":[9,14],"source_contract":transfer,
                    "basis":"mode6仍global display2/frame1；确认前目的格空且无范围着色，target仍在旧格"},
                after_success_focus=list(TELEPORT_DESTINATION),
                required_order=["first_layer_colored_range","destination_full_base_tile_and_target_frame1",
                    "Right once to10,14","destination_selected_complete_base_plus_frame1","Confirm success"],
                unsaved_coordinate_transitions=[[9,14],[255,255],[10,14]],
                four_part_presentation_requires_native_evidence=True)
        return result
    if scenario != "composite34":
        raise ValueError("未知G2阶段资源门")
    icons=(original/"FDICON.B24").read_bytes()
    targets=[]
    for raw in (0,2):
        record=actor(source,raw);x,y=screen(source,tuple(record[:2]))
        images=[sheet_frame(icons,record[7]*12+record[3]*3+phase) for phase in range(3)]
        for image in images:image.putpalette(assets["palette"])
        targets.append({"raw_actor":raw,"unit_id":record[7],"world":list(record[:2]),
                        "position":(x,y-6),"templates":images,"selector_pixels_included":False})
    return {"targets":targets,"center":list(actor(source,0)[:2]),
            "after_success_focus":list(actor(source,0)[:2]),
            "selection_shape_verified":False,
            "scope":"raw actor source frames only; display5/area3 selector composition is a separate runner gate"}
