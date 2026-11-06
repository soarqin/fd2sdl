"""B7第二关完整流程的原版数据前置与受控夹具；不运行SDL或窗口。

依据stage1结果206c5、结算22f37、1c220奖励首空槽、11506持久同步，
以及32e8c的stage2真实入场脚本/模板。自然源完整保留，不用受控胜利冒充整关；
stage2只覆盖初始活动与存取，未开放turn3/action9及整关。
"""
import hashlib
import json
import struct
from pathlib import Path

from fd2_item_list_oracle import EXE_SHA256
from fd2_loot_oracle import inventory_append
from fd2_opening_oracle import build_pages, fragments
from fd2_transition_oracle import (FILE_SIZE, ROSTER_SIZE, SLOT_BASE, SLOT_STRIDE,
    entry_geometry, entry_initialized_records, check_entry_measurement)
from run_graphics_tests import decode_save
from run_original_comparison import archive_entry
from run_original_end_turn import META, UNITS, STRIDE
from run_original_item_list import drop_derived
from run_original_player import sheet_frame
from run_original_focus import native_focus_reference
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image
from dump_fd2_fixup_table import collect_internal_offset_fixups

SCENARIOS = ("natural-stage1", "victory-all-villagers", "victory-villager-loss",
    "victory-full-inventory", "villager-defeat", "chest-129", "chest-201", "chest-193", "stage2-storage")
CHESTS = {129: ((10, 13), 1), 201: ((6, 20), 2), 193: ((21, 17), 3)}


def tables(original):
    original = Path(original)
    data = (original / "FD2.EXE").read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("B7原始EXE不符")
    le = parse_le(data)
    raw = build_relbase_image(data, le, parse_objects(data, le))
    from fd2_item_list_oracle import item_table
    items, item_source = item_table(original / "FD2.EXE")
    fixups = collect_internal_offset_fixups(original / "FD2.EXE")
    field = (original / "FDFIELD.DAT").read_bytes()
    stages = {}
    for stage in (1, 2):
        metadata, placement = archive_entry(field, 3 * stage + 1), archive_entry(field, 3 * stage + 2)
        if len(metadata) != 1171 or metadata[:3] != bytes((stage, stage + 4, 40)):
            raise ValueError("B7 stage1/2完整模板表不符")
        stages[stage] = {"metadata": metadata,
            "templates": [metadata[131 + i * 26:157 + i * 26] for i in range(40)],
            "placement": [list(struct.unpack_from("<HH", placement, 2 + i * 6)) for i in range((len(placement) - 2) // 6)]}
    scripts = {}
    for sid, address in ((14, 0x62afe), (15, 0x62b03), (16, 0x62b10), (17, 0x62b29), (18, 0x62b3e), (19, 0x62b4d)):
        owner, _, target = fixups[0x627d8 + 4 * sid]
        if owner != 3 or target != address:
            raise ValueError("B7 movement script原指针不符")
        end = fixups[0x627d8 + 4 * (sid + 1)][2]
        data = bytes(raw[address:end]); cursor = 1; blocks = []
        for _ in range(data[0]):
            mode, count = data[cursor:cursor + 2]; cursor += 2
            pairs = [tuple(data[cursor + 2 * i:cursor + 2 * i + 2]) for i in range(count)]
            cursor += 2 * count
            if any(direction > 3 for _, direction in pairs):
                raise ValueError("B7 script方向越界")
            blocks.append((mode, pairs))
        if cursor != len(data): raise ValueError("B7 movement script边界不符")
        scripts[sid] = {"blocks": blocks, "va": address, "sha256": hashlib.sha256(data).hexdigest()}
    return {"raw": raw, "items": items, "item_source": item_source, "stages": stages, "scripts": scripts,
        "stage2_dialogue": dict(fragments((original / "FDTXT.DAT").read_bytes(), 2, 4))}


def template_record(data, stage, index, position):
    """10c50精确构造；只排runtime cache及实际未写的9B，不读SDL构造器。"""
    row = data["stages"][stage]["templates"][index]
    raw, identity, level = data["raw"], row[1], row[4]
    unit, known = bytearray(80), bytearray([1] * 80)
    unit[:2] = bytes(position); known[2] = 0; known[0x28:0x31] = bytes(9)
    unit[5:10] = bytes((0, row[0], identity, identity, 0))
    if identity < 32:
        base, growth = raw[0x61da1 + 24 * identity:0x61da1 + 24 * (identity + 1)], raw[0x620a1 + 11 * identity:0x620a1 + 11 * (identity + 1)]
        unit[31:33] = base[:2]
        strength = struct.unpack_from("<H", base, 18)[0] + level * growth[0]
        endurance = struct.unpack_from("<H", base, 20)[0] + level * growth[2]
        speed = struct.unpack_from("<H", base, 22)[0] + level * growth[4]
        hp = struct.unpack_from("<H", base, 3)[0] + (level - 1) * growth[6]
        mp = struct.unpack_from("<H", base, 5)[0] + (level - 1) * growth[8]
        movement = base[7]
    elif 68 <= identity <= 135:
        base = raw[0x61af9 + 10 * (identity - 68):0x61af9 + 10 * (identity - 67)]
        unit[31:33] = base[:2]
        strength, endurance, speed = base[5] * level, base[6] * level, base[7] * level
        hp, mp, movement = struct.unpack_from("<H", base, 2)[0] * level, base[4] * level, base[8]
    else: raise ValueError("B7模板没有已证基础record")
    if row[5] == 255:
        unit[10:13] = bytes((0x40, row[6], 0x80)); known[13] = 0
    else: unit[10:14] = bytes((0x40, row[5], 0x40, row[6]))
    for slot in range(6): unit[14 + slot * 2:16 + slot * 2] = bytes((0x80 if row[7 + slot] == 255 else 0, row[7 + slot]))
    unit[26:31] = row[13:17] + bytes(1); unit[33] = level
    unit[49:52] = row[22:25]; unit[52:55] = row[17:20]
    struct.pack_into("<H", unit, 55, strength & 65535); struct.pack_into("<H", unit, 57, endurance & 65535)
    unit[59:62] = bytes((movement, 0 if row[0] == 2 else 255, row[2]))
    struct.pack_into("<5H", unit, 62, speed & 65535, hp & 65535, hp & 65535, mp & 65535, mp & 65535)
    struct.pack_into("<4H", unit, 72, *(n & 65535 for n in drop_derived(unit, data["items"])))
    return unit, known


def apply_script(units, script):
    for mode, pairs in script["blocks"]:
        for raw, direction in pairs:
            if raw >= len(units): raise ValueError("B7脚本引用尚不存在的raw；不能改为lastactor")
            unit = units[raw]; unit[3] = direction
            if mode < 128:
                unit[4] = 0
                unit[0] = (unit[0] + (0, -1, 0, 1)[direction] * mode) & 255
                unit[1] = (unit[1] + (1, 0, -1, 0)[direction] * mode) & 255


def settlement_projection(source, data):
    """受控turn4源→22f37/11506；只给new raw27的10B明确known mask。"""
    source_state(source, post_turn3=True)
    rewarded, reward = reward_inventory_write(source)
    units = [bytearray(actor(rewarded, raw)) for raw in range(27)]
    index = next(i for i, row in enumerate(data["stages"][1]["templates"]) if row[21] == 4)
    position = data["stages"][1]["placement"][index]
    if any(not u[5] & 1 and list(u[:2]) == position for u in units):
        raise ValueError("group4初格被占；未实现此来源的nearest-free，不能猜位置")
    new, new_mask = template_record(data, 1, index, position)
    units.append(new)
    for sid in (14, 15, 16): apply_script(units, data["scripts"][sid])
    if new[7:9] != bytes((8, 8)) or tuple(new[:2]) != (15, 0) or new[3] != 2:
        raise ValueError("B7原group4/script14..16几何不符")
    roster = bytearray(source[0x8a3:0x12a3]); masks = [bytearray([1] * 80) for _ in range(6)]
    for mask in masks: mask[2] = 0
    # 112a5(8)的构造会被11506匹配active27整80B覆写，不用未用roster假装新malloc字节。
    roster[5 * 80:6 * 80] = new
    masks[5] = new_mask
    for unit in units:
        for index in range(6):
            if unit[8] != roster[index * 80 + 8] or unit[8] == 0 and unit[5] & 1: continue
            fixed = bytearray(unit); fixed[34:40] = bytes(6); fixed[5] &= 1
            if not fixed[5] & 1: fixed[64:66] = fixed[66:68]
            fixed[68:70] = fixed[70:72]
            struct.pack_into("<4H", fixed, 72, *(n & 65535 for n in drop_derived(fixed, data["items"])))
            roster[index * 80:(index + 1) * 80] = fixed
    state = {"stage": 2, "count": 6, "roster": bytes(roster),
        "resource": struct.unpack_from("<I", source, META + 10)[0],
        "options": bytes(source[META + i] for i in (15, 14, 16, 17))}
    return state, masks, reward


def stage1_story_pages(original, assets, source):
    """34778的group3先补raw21..26，22f37的group4再成为raw27。"""
    count = source[META + 1]
    if source[META + 2] != 1 or count not in (21, 27):
        raise ValueError("stage1剧情仅已确认初始21或turn3后27条active，不能猜raw27")
    identities = [actor(source, i)[7] for i in range(count)]
    mapping = {actor(source, i)[8]: actor(source, i)[7] for i in range(count)}
    if count == 21:
        metadata = archive_entry((Path(original) / "FDFIELD.DAT").read_bytes(), 4)
        future = [metadata[131 + i * 26:157 + i * 26] for i in range(40)
            if metadata[131 + i * 26 + 21] == 3]
        if len(future) != 6: raise ValueError("stage1 group3原记录不是六人")
        identities.extend(row[1] for row in future)
        mapping.update((row[1], row[1]) for row in future)
    identities.append(8); mapping[8] = 8
    # 1aa1d mode3使用本stage完整目录；11..16是六村民的必经死亡对白。
    pages = build_pages(Path(original), assets, stage_plan=((1, 17),),
        identities_by_stage={1: identities}, text_identities_by_stage={1: mapping})
    return [page for page in pages if page["fragment"] in (4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)]


def settlement_pages(original, assets, source):
    first = 7 if any(actor(source, i)[5] & 1 for i in range(5, 11)) else 6
    pages = stage1_story_pages(original, assets, source)
    return [page for page in pages if page["fragment"] in (first, 8, 9, 10)]


def stage2_projection(state, data):
    if state["stage"] != 2 or state["count"] != 6 or len(state["roster"]) != ROSTER_SIZE:
        raise ValueError("stage2部署需实际六条持久记录，不补人")
    units, masks = [], []
    camera, focus, ledger = [0, 0], [0, 0], []
    def pan(target):
        # 135dd保持relative，同步absolute focus delta，不是只改camera。
        nonlocal camera, focus
        focus = [focus[i] + target[i] - camera[i] for i in range(2)]
        camera = list(target)
        ledger.append({"kind": "pan", "camera": camera.copy(), "focus": focus.copy(), "va": "0x135dd"})
    def focus_to(point, reason):
        nonlocal camera, focus
        result = native_focus_reference(camera, focus, point, (18, 26))
        ledger.append({"kind": reason, "from_camera": camera.copy(), "from_focus": focus.copy(),
            "target": list(point), "camera": result["camera"], "steps": result["steps"], "va": "0x12cea"})
        camera, focus = result["camera"], list(point)
    def dialogue(fragment):
        tokens = data["stage2_dialogue"][fragment]; offset = 0
        while offset < len(tokens):
            token = tokens[offset]
            if token not in (-17, -18, -19, -20): offset += 1; continue
            argument = tokens[offset + 1]; offset += 2
            if token in (-17, -18):
                raw = next((i for i, unit in enumerate(units) if unit[8] == argument and not unit[5] & 1), None)
                if raw is None:
                    ledger.append({"kind": "hidden_text_speaker_no_focus", "fragment": fragment,
                        "text_id": argument, "va": "0x12c60/0x15f84"})
                    continue
            else: raw = argument
            if raw >= len(units): raise ValueError("stage2词元引用尚不存在raw")
            focus_to(units[raw][:2], f"fragment{fragment}/speaker{token}/{argument}/raw{raw}")
    for raw in range(6):
        unit = bytearray(state["roster"][raw * 80:(raw + 1) * 80]); mask = bytearray([1] * 80); mask[2] = 0
        unit[:2] = bytes(data["stages"][2]["placement"][40 + raw]); unit[3:5] = bytes(2)
        unit[6] = 2; unit[49] = 255; unit[34:40] = bytes(6)
        units.append(unit); masks.append(mask)
    pan((3, 17)); dialogue(0)
    apply_script(units, data["scripts"][18])
    for index, row in enumerate(data["stages"][2]["templates"]):
        if row[21] != 1: continue
        position = data["stages"][2]["placement"][index]
        if any(not u[5] & 1 and list(u[:2]) == position for u in units):
            raise ValueError("stage2初始group1占格，不猜nearest-free")
        unit, mask = template_record(data, 2, index, position); units.append(unit); masks.append(mask)
    pan((3, 6)); apply_script(units, data["scripts"][17]); dialogue(1)
    apply_script(units, data["scripts"][19]); dialogue(2)
    pan((3, 17)); dialogue(3)
    for unit in units: unit[3] = 0
    focus_to(units[0][:2], "final actor0 focus @0x12d7b")
    if len(units) != 15: raise ValueError("stage2真实entry必须15active")
    return {"units": [bytes(u) for u in units], "masks": [bytes(m) for m in masks],
        "first_focus_world": list(units[0][:2]), "camera": camera, "focus_ledger": ledger, "stage": 2, "turn": 1}


def stage2_pages(original, assets, state, data):
    projection = stage2_projection(state, data)
    identities = [u[7] for u in projection["units"]]
    mapping = {u[8]: u[7] for u in projection["units"]}
    return build_pages(Path(original), assets, stage_plan=((2, 4),),
        identities_by_stage={2: identities}, text_identities_by_stage={2: mapping})


def stage2_town(original, data, state):
    from fd2_transition_oracle import resources, _text
    from fd2_reward_oracle import common_tokens
    from run_original_comparison import decode_image
    assets = resources(Path(original))
    raw = data["raw"]
    if bytes(raw[0x52405:0x52408]) != bytes((11, 61, 62)) or raw[0x623ac] != 2:
        raise ValueError("B7 stage2 town类型/真实背景表不符")
    assets["background"] = decode_image(archive_entry((Path(original) / "FDOTHER.DAT").read_bytes(), 62))
    assets["background"].putpalette(assets["palette"])
    if assets["background"].size != (320, 200): raise ValueError("stage2 FDOTHER62背景尺寸不符")
    coordinates = list(zip(raw[0x52363 + 12:0x52363 + 17], raw[0x52375 + 12:0x52375 + 17]))
    if coordinates != [(59, 26), (10, 144), (59, 163), (130, 150), (242, 31)]:
        raise ValueError("B7 stage2 type2 marker六字节stride原表不符")
    icons = (Path(original) / "FDICON.B24").read_bytes()
    from audit_fdicon_opaque_zero import audit_fdicon
    if audit_fdicon(icons)["opaque_zero_pixels"]:
        raise ValueError("stage2 town FDICON出现opaque0，不能用非零透明mask")
    towns = []
    for selected in range(5):
        views = []
        for phase in (0, 1, 2):
            view = assets["background"].crop((0, 0, 312, 192)); view.putpalette(assets["palette"])
            view.paste(assets["panel"], (244, 162))
            _text(view, assets["font"], common_tokens(assets["text"], 495 + selected), 252, 168, 205, background=74)
            marker = sheet_frame(icons, state["roster"][7] * 12 + phase)
            view.paste(marker, coordinates[selected], marker.point(lambda p: 255 if p else 0, mode="L"))
            views.append(view)
        towns.append(views)
    return {"assets": assets, "towns": towns, "city_type": 2, "background_entry": 62,
        "marker_xy_inside_view": coordinates}


def write_stage2_slot(before, state, index):
    if (checksum(before) != before or state["stage"] != 2 or state["count"] != 6 or
            len(state["roster"]) != ROSTER_SIZE or not 0 <= index < 4):
        raise ValueError("B7 stage2手工slot参数不符")
    out = bytearray(before); at = SLOT_BASE + index * SLOT_STRIDE
    out[at:at + ROSTER_SIZE] = state["roster"]
    out[at + ROSTER_SIZE:at + ROSTER_SIZE + 10] = bytes((2, 6)) + struct.pack("<I", state["resource"]) + state["options"]
    return checksum(out)


def check_stage2_manual_save(before, measured, expected_state, roster_masks, index=0):
    if checksum(measured) != measured: raise AssertionError("B7手工实测checksum不符")
    state = dict(expected_state); roster = bytearray(expected_state["roster"])
    opaque = []
    base = SLOT_BASE + SLOT_STRIDE * index
    for raw, mask in enumerate(roster_masks):
        for offset, known in enumerate(mask):
            if not known:
                where = raw * 80 + offset; roster[where] = measured[base + where]
                opaque.append({"raw": raw, "offset": offset, "scope": "native runtime cache or exact9B new ctor holes"})
    state["roster"] = bytes(roster)
    expected = write_stage2_slot(before, state, index)
    mismatch = [i for i, (a, z) in enumerate(zip(expected, measured)) if a != z]
    if mismatch: raise AssertionError(f"B7手工全容器/原22f37已知roster不符：{mismatch[:20]}")
    return state, {"stage": 2, "count": 6, "slot": index, "complete_container_equal_except_specific_ctor_cache": True,
        "known_roster_bytes": [sum(mask) for mask in roster_masks], "native_new_record_opaque": opaque,
        "other_slots_active_resource_unknown_tails_preserved": True}


def check_natural_stage2_manual_save(before, measured, origin, data):
    """自然战果只取本端实际持久表；独立核11506规范化及new8已知70B。"""
    if len(measured) != FILE_SIZE or checksum(measured) != measured:
        raise AssertionError("自然stage2酒店Save不是完整有效容器")
    allowed = set(range(SLOT_BASE, SLOT_BASE + ROSTER_SIZE + 10)) | set(range(FILE_SIZE - 4, FILE_SIZE))
    if any(a != b and at not in allowed for at, (a, b) in enumerate(zip(before, measured))):
        raise AssertionError("自然酒店Save修改slot0/meta0..9/checksum以外字节")
    slot = measured[SLOT_BASE:SLOT_BASE + SLOT_STRIDE]
    if slot[ROSTER_SIZE:ROSTER_SIZE + 2] != bytes((2, 6)):
        raise AssertionError("自然结算未形成stage2六条持久记录")
    state = {"stage": 2, "count": 6, "roster": bytes(slot[:ROSTER_SIZE]),
        "resource": struct.unpack_from("<I", slot, ROSTER_SIZE + 2)[0],
        "options": bytes(slot[ROSTER_SIZE + 6:ROSTER_SIZE + 10])}
    if state["options"] != bytes(origin[META + i] for i in (15, 14, 16, 17)):
        raise AssertionError("自然酒店Save配置偏离只读源；流程未操作Options")
    rows = [state["roster"][raw * 80:(raw + 1) * 80] for raw in range(6)]
    for raw in range(5):
        if rows[raw][7:9] != actor(origin, raw)[7:9] or rows[raw][6] != 2:
            raise AssertionError("自然持久队伍前五条身份/顺序改变")
        if rows[raw][0x28:0x31] != actor(before, raw)[0x28:0x31]:
            raise AssertionError("自然11506没有写旧actor保留9B，持久同步不能丢未知字节")
        if actor(origin, raw)[5] & 1 and not rows[raw][5] & 1:
            raise AssertionError("自然阶段没有复活路径，源阵亡玩家不得自行恢复")
    if rows[0][5] & 1: raise AssertionError("自然胜利必须主角仍存活")
    for row in rows:
        if row[5] & ~1 or any(row[34:40]):
            raise AssertionError("11506结算未清acted/临时状态")
        if not row[5] & 1 and row[64:66] != row[66:68] or row[68:70] != row[70:72]:
            raise AssertionError("11506结算HP/MP恢复合同不符")
        if struct.unpack_from("<4H", row, 72) != tuple(v & 65535 for v in drop_derived(row, data["items"])):
            raise AssertionError("11506结算派生属性不等于独立原装备/基础表公式")
    index = next(i for i, row in enumerate(data["stages"][1]["templates"]) if row[21] == 4)
    new, mask = template_record(data, 1, index, (22, 4))
    new[:2] = bytes((15, 0)); new[3] = 2
    if any(known and wanted != actual for wanted, actual, known in zip(new, rows[5], mask)):
        raise AssertionError("自然新unit8偏离10c50/script14..16的70已知字节")
    if state["roster"][480:] != before[0x8a3 + 480:0x12a3]:
        raise AssertionError("自然结算改写六人之外的持久未知尾")
    return state, {"stage": 2, "count": 6, "source": "this endpoint actual natural settlement manual Save",
        "slot0_full_write_scope_checked": True, "own_persistent_roster_used_for_independent_entry": True,
        "new_unit8_known_bytes": sum(mask), "new_unit8_opaque_offsets": [i for i, known in enumerate(mask) if not known],
        "normalization_and_derived_stats_checked": True, "unused_persistent_tail_preserved": True,
        "battle_HP_XP_resource_random_outcome_not_predicted_from_SDL": True}


def check_stage2_active_save(before, measured, state, projection, measuring_view, data):
    if checksum(measured) != measured: raise AssertionError("B7 stage2活动实测checksum不符")
    expected = bytearray(before)
    expected[:1171] = data["stages"][2]["metadata"]
    # 原19df7固定复制2211B；资源只有1171B。此段明确登记，不扩为整个metadata/单位记录豁免。
    expected[1171:0x8a3] = measured[1171:0x8a3]
    expected[0x8a3:0x12a3] = state["roster"]
    opaque = []
    for raw, (record, mask) in enumerate(zip(projection["units"], projection["masks"])):
        at = UNITS + raw * 80; unit = bytearray(record)
        for offset, known in enumerate(mask):
            if not known:
                unit[offset] = measured[at + offset]; opaque.append([raw, offset])
        expected[at:at + 80] = unit
    expected[0x30a3:0x30c3] = bytes(32)
    expected[META:META + 10] = bytes((1, 15, 2, *measuring_view[META + 3:META + 9], 6))
    struct.pack_into("<I", expected, META + 10, state["resource"])
    for at, value in zip((15, 14, 16, 17), state["options"]): expected[META + at] = value
    expected = checksum(expected)
    mismatch = [i for i, (a, z) in enumerate(zip(expected, measured)) if a != z]
    if mismatch: raise AssertionError(f"B7 stage2活动全容器/初始15actor不符：{mismatch[:20]}")
    return {"stage": 2, "turn": 1, "active_count": 15, "persistent_count": 6,
        "known_actor_bytes": [sum(mask) for mask in projection["masks"]],
        "full_roster_slots_unused_active_meta_tails_checked": True,
        "native_cache_and_exact_ctor_holes": opaque, "native_metadata_heap_tail": [1171, 2211],
        "stage2_battle_or_turn3_handler_claimed": False}


def checksum(plain):
    if len(plain) != FILE_SIZE:
        raise ValueError("B7需要完整0x59cb容器")
    result = bytearray(plain)
    result[-4:] = sum(result[:-4]).to_bytes(4, "little")
    return bytes(result)


def actor(plain, raw):
    if type(raw) is not int or not 0 <= raw < plain[META + 1]:
        raise ValueError("B7 raw actor越出实际活动表")
    return plain[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]


def source_state(plain, *, post_turn3=False):
    """只读取本文件完整持久owner，不从START或构造器补角色。"""
    if len(plain) != FILE_SIZE or checksum(plain) != plain:
        raise ValueError("B7来源长度或checksum不符")
    if (plain[META:META + 3] != (bytes((4, 27, 1)) if post_turn3 else bytes((1, 21, 1))) or
            plain[META + 9] != 5):
        raise ValueError("B7来源必须stage1 turn1/21active或胜利前真实turn4/27active，均5persistent")
    return {"stage": 1, "count": 5, "roster": bytes(plain[0x8a3:0x12a3]),
        "resource": struct.unpack_from("<I", plain, META + 10)[0],
        "options": bytes(plain[META + at] for at in (15, 14, 16, 17))}


def validate_entry_source(original, plain):
    """检查原资源已知写集；未知heap只是本次source所有，不是终态豁免。"""
    original = Path(original)
    state = source_state(plain)
    if hashlib.sha256((original / "FD2.EXE").read_bytes()).hexdigest() != EXE_SHA256:
        raise ValueError("B7原版EXE身份不符")
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata = archive_entry(field, 4)
    data = archive_entry(field, 3)
    width, height = struct.unpack_from("<HH", data)
    if (width, height) != (27, 21) or len(metadata) != 1171 or metadata[:6] != bytes((1, 5, 40, 3, 6, 1)):
        raise ValueError("B7 stage1原资源/turn3 action6前置不符")
    camera = list(plain[META + 3:META + 5])
    if not 0 <= camera[0] <= width - 13 or not 0 <= camera[1] <= height - 8:
        raise ValueError("B7来源camera超出stage1地图")
    geometry = entry_geometry(original, state)
    initialized = entry_initialized_records(original, state, geometry)
    checked = check_entry_measurement(plain, plain, state, geometry,
        {"inferred_camera": camera}, metadata, initialized)
    if actor(plain, 0)[5] & 1 or any(actor(plain, raw)[5] & 1 for raw in range(5, 11)):
        raise ValueError("B7自然起点必须主角与六村民存活；不复活旧阵亡玩家")
    return {"kind": "resource_proven_stage1_entry", "checked": checked,
        "map_size": [width, height], "full_plain_sha256": hashlib.sha256(plain).hexdigest(),
        "persistent_source": "this exact complete source roster, not controlled five-member constructor",
        "player_hidden_raws": [raw for raw in range(5) if actor(plain, raw)[5] & 1],
        "known_constructor_bytes": [sum(mask) for _, mask in initialized],
        "unknown_source_bytes_pinned_not_normalized": True,
        "runtime_provenance_proven_by_semantic_validation_alone": False}


def validate_post_turn3_source(original, plain):
    """22f37脚本14/16会读raw27；禁止用count21提前假造结算前置。"""
    state = source_state(plain, post_turn3=True)
    original = Path(original)
    metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 4)
    if plain[:1171] != metadata or len(metadata) != 1171:
        raise ValueError("B7 turn4来源stage1完整metadata不符")
    templates = [metadata[131 + i * 26:157 + i * 26] for i in range(40)]
    identities = [(state["roster"][i * 80 + 7], state["roster"][i * 80 + 8], 2) for i in range(5)]
    identities += [(row[1], row[1], row[0]) for group in (1, 2, 3) for row in templates if row[21] == group]
    if len(identities) != 27:
        raise ValueError("B7原group1/2/3追加后必须27active")
    for raw, (unit_id, text_id, side) in enumerate(identities):
        record = actor(plain, raw)
        if (record[7], record[8], record[6]) != (unit_id, text_id, side):
            raise ValueError("B7 turn4来源raw身份/组顺序不符")
    if actor(plain, 0)[5] & 1 or any(actor(plain, raw)[5] & 1 for raw in range(5, 11)):
        raise ValueError("B7受控胜利来源应主角/六村民仍活，不复活source")
    return {"kind": "resource_proven_stage1_turn4", "turn3_group3_count": 6,
        "existing_active_count": 27, "full_plain_sha256": hashlib.sha256(plain).hexdigest(),
        "original_source_all_bytes_owned_and_preserved": True,
        "runtime_provenance_proven_by_semantic_validation_alone": False}


def validate_origin_report(report_path, fixture):
    """只读核完整真实来源端报告；不能用SDL模拟或半写JSON伪造自然起点。"""
    path, fixture = Path(report_path), Path(fixture)
    report = json.loads(path.read_text(encoding="utf-8"))
    completed = report.get("completed_save", {})
    processes = report.get("processes", [])
    if (report.get("kind") != "original" or report.get("status") != "passed" or
            report.get("business_sequence_completed") is not True or
            report.get("natural_settlement_reached_town") is not True or
            not processes or any(p.get("exit_code") != 0 or p.get("cleaned") is not True or
                p.get("forced_cleanup") for p in processes)):
        raise ValueError("B7自然来源需已完整完成并正常清理的原版端报告")
    required = ("original_unchanged", "fixture_unchanged", "copy_save_matches_completed_state",
        "cleanup_passed", "original_all_mtimes_unchanged", "planner_unchanged", "binary_tools_unchanged")
    if any(report.get(key) is not True for key in required):
        raise ValueError("B7自然来源报告缺必需晚守卫")
    actual = hashlib.sha256(fixture.read_bytes()).hexdigest()
    if completed.get("sha256") != actual or Path(completed.get("snapshot", "")).resolve() != fixture.resolve():
        raise ValueError("B7自然来源文件不是该原版端实际completed Save")
    acknowledgements = report.get("natural_entry_acknowledgements", [])
    if len(acknowledgements) != 25:
        raise ValueError("B7来源缺stage1完整25页入场剧情ACK")
    captures = []
    for index, page in enumerate(acknowledgements):
        name = f"natural-transition-entry-{index:02d}"
        matches = [row for row in report.get("checkpoints", []) if row.get("name") == name]
        if len(matches) != 1:
            raise ValueError("B7来源入场剧情检查点缺失或重复")
        row = matches[0]
        if ((row.get("fragment"), row.get("token_offset"), row.get("portrait_id")) !=
                (page.get("fragment"), page.get("offset"), page.get("portrait")) or
                row.get("kind") != "resource_proven_opening_wait" or
                any(row.get(part, {}).get("ok") is not True for part in ("text", "border"))):
            raise ValueError("B7来源入场页token/画像或资源门不符")
        filename = name + ".png"
        image = path.parent / filename
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        if report.get("capture_hashes", {}).get(filename) != digest:
            raise ValueError("B7来源入场实际PNG hash不符")
        captures.append({"path": str(image.resolve()), "sha256": digest})
    return {"report": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "fixture_sha256": actual, "native_original_completed_and_cleaned": True,
        "actual_entry_story_pages": 25, "entry_captures": captures}


def stage1_chests(original):
    """按真实map index与metadata mode/param取三箱，不把拾取当Use支持。"""
    field = (Path(original) / "FDFIELD.DAT").read_bytes()
    data, metadata = archive_entry(field, 3), archive_entry(field, 4)
    width, height = struct.unpack_from("<HH", data)
    shapes = archive_entry((Path(original) / "FDSHAP.DAT").read_bytes(), metadata[0] * 2 + 1)
    rows = []
    for identity, (world, index) in CHESTS.items():
        cell = struct.unpack_from("<I", data, 4 + 4 * (world[1] * width + world[0]))[0]
        mode, parameter = struct.unpack_from("<BH", metadata, 0x53 + index * 3)
        flags = shapes[(cell & 1023) * 4]
        if ((cell >> 16) & 31, mode, parameter) != (index, 0, identity) or not flags & 0x60:
            raise ValueError("B7三箱原map/metadata/地形属性合同不符")
        rows.append({"item_id": identity, "world": list(world), "map_cell_index": index,
            "cell_word": cell, "terrain_flags": flags, "mode": mode, "parameter": parameter})
    return rows


def reward_inventory_write(source):
    """22f37→1c220的唯一库存写集；不代替后续剧情/11506/阶段2模型。"""
    if len(source) != FILE_SIZE or checksum(source) != source or source[META + 1] < 11:
        raise ValueError("B7奖励需完整stage1 source及六村民记录")
    changed = bytearray(source)
    all_alive = not any(actor(source, raw)[5] & 1 for raw in range(5, 11))
    recipient = None
    if all_alive:
        for raw in range(source[META + 1]):
            record = actor(source, raw)
            if record[6] != 2:
                continue
            if any(record[10 + slot * 2] & 0x80 for slot in range(8)):
                changed[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE] = inventory_append(record, 198)
                recipient = raw
                break
    return checksum(changed), {"fragment": 6 if all_alive else 7,
        "item198_recipient_raw": recipient, "full_inventory_silent_no_write": all_alive and recipient is None,
        "write_scope": "only 1c220 inventory append, not full settlement or phase transition"}


def derive_fixture(original, fixture, scenario):
    """自然场景原样；受控结果只改明确flags/inventory，不等于整关实战。"""
    if scenario not in SCENARIOS:
        raise ValueError("未知B7 stage1场景")
    fixture = Path(fixture)
    encoded = fixture.read_bytes()
    source = decode_save(fixture)
    validated = (validate_post_turn3_source(original, source) if scenario.startswith("victory-") else
        validate_entry_source(original, source))
    changed = bytearray(source)
    allowed = set()
    def put(offset, data):
        changed[offset:offset + len(data)] = data
        allowed.update(range(offset, offset + len(data)))
    if scenario.startswith("victory-"):
        for raw in range(source[META + 1]):
            if actor(source, raw)[6] == 0:
                at = UNITS + raw * STRIDE + 5
                put(at, bytes((changed[at] | 1,)))
        if scenario == "victory-villager-loss":
            at = UNITS + 5 * STRIDE + 5
            put(at, bytes((changed[at] | 1,)))
        if scenario == "victory-full-inventory":
            # 1c220只看active raw side2；不伪造未知奖励交换UI或改原资源。
            for raw in range(source[META + 1]):
                if actor(source, raw)[6] == 2:
                    put(UNITS + raw * STRIDE + 10, bytes((0, 192)) * 8)
    elif scenario == "villager-defeat":
        for raw in range(5, 11):
            at = UNITS + raw * STRIDE + 5
            put(at, bytes((changed[at] | 1,)))
    elif scenario.startswith("chest-"):
        raise ValueError("三箱输入位置/特殊格前置正在核原资源；未就绪不能创建GUI夹具")
    changed = checksum(changed)
    allowed.update(range(FILE_SIZE - 4, FILE_SIZE))
    patches = [{"offset": at, "before": a, "after": z}
        for at, (a, z) in enumerate(zip(source, changed)) if a != z]
    if any(patch["offset"] not in allowed for patch in patches):
        raise AssertionError("B7 controlled fixture写入未声明字节")
    output = bytes(a ^ z ^ c for a, z, c in zip(encoded, source, changed))
    manifest = {"scenario": scenario, "source_path": str(fixture.resolve()),
        "source_sha256": hashlib.sha256(encoded).hexdigest(), "source_mtime_ns": fixture.stat().st_mtime_ns,
        "source_validation": validated, "source_plain_sha256": hashlib.sha256(source).hexdigest(),
        "derived_sha256": hashlib.sha256(output).hexdigest(), "derived_plain_sha256": hashlib.sha256(changed).hexdigest(),
        "plaintext_patches": patches, "encrypted_patches": [{"offset": at, "before": a, "after": z}
            for at, (a, z) in enumerate(zip(encoded, output)) if a != z],
        "controlled_fixture": scenario != "natural-stage1", "natural_stage1_battle": scenario == "natural-stage1",
        "all_other_container_bytes_preserved": True}
    return output, changed, manifest
