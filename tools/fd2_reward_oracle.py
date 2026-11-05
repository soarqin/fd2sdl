"""B4奖励的原版数据及纯资源模型；不启动进程、不改写来源。

依据field_unit_experience_reward_execute @0x1e292（原sub_1E292）、
field_growth_stat_present_apply @0x1e529（原sub_1E529）、
field_unit_magic_bit_set @0x1d79c（原sub_1D79C），以及
field_unit_combat_stats_recompute @0x1b750。
完整成长／学习表由sub_4E821 @0x4e821／sub_4E7F2 @0x4e7f2取得。
只用strict LE原表，不读取SDL表；事件序列与原版WGC运行证据分别登记。
"""
import hashlib
import struct

from PIL import Image, ImageOps

from fd2_item_list_oracle import EXE_SHA256, item_table, use_effect_tables, number
from fd2_magic_list_oracle import glyphs
from fd2_opening_oracle import raw_ui
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image
from run_original_comparison import archive_entry
from run_original_battle_menu import match_indexed
from run_original_item_list import drop_derived
from run_original_player import exact_tile
from run_original_end_turn import META, STRIDE, UNITS, decode_lmi
from run_graphics_tests import decode_save
from run_original_item_list import derive_active_fixture

GROWTH_VA, GROWTH_SIZE = 0x620a1, 68 * 11
LEARNING_VA, LEARNING_SIZE = 0x626b3, 20 * 12
GROWTH_SHA256 = "75bedc7308088625abc0b70c10115d94903008e7fb907a0acaadc22dc45af2a5"
LEARNING_SHA256 = "839200667f837136d3426ab952e455b1a63b3bbba8b9175eb895d4408ddd6fb9"
GROWTH_FIELDS = ((0x37, 490), (0x39, 491), (0x3e, 492), (0x42, 493), (0x46, 494))
SCENARIOS = ("physical-cap", "physical-overwrite", "heal", "learn", "counter-multi", "status")


def reward_tables(executable):
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("B4 oracle仅支持已确认完整FD2.EXE")
    le = parse_le(data)
    objects = parse_objects(data, le)
    for va, size in ((GROWTH_VA, GROWTH_SIZE), (LEARNING_VA, LEARNING_SIZE)):
        if not any(obj.index == 3 and obj.relbase <= va and va + size <= obj.relbase + obj.vsize for obj in objects):
            raise ValueError("B4表不在已确认object3完整范围")
    raw = build_relbase_image(data, le, objects)
    growth = bytes(raw[GROWTH_VA:GROWTH_VA + GROWTH_SIZE])
    learning = bytes(raw[LEARNING_VA:LEARNING_VA + LEARNING_SIZE])
    if hashlib.sha256(growth).hexdigest() != GROWTH_SHA256 or hashlib.sha256(learning).hexdigest() != LEARNING_SHA256:
        raise ValueError("B4完整成长/学习表strict LE哈希不符")
    items, _ = item_table(executable)
    return {"growth": growth, "learning": learning, "items": items}, {
        "executable_sha256": EXE_SHA256,
        "growth": {"getter_va": 0x4e821, "canonical_va": GROWTH_VA, "size": GROWTH_SIZE,
                   "rows": 68, "stride": 11, "sha256": GROWTH_SHA256},
        "learning": {"getter_va": 0x4e7f2, "canonical_va": LEARNING_VA, "size": LEARNING_SIZE,
                     "rows": 20, "stride": 12, "sha256": LEARNING_SHA256},
        "runtime_support": "controlled player IDs0..31; readable68-row data is not a gameplay support claim"}


def reward_plan(actor, gain, tables):
    """原1e292的完整80B终态及逐提示写序；caller负责归一和cap99。

    不随机成长；只支持本阶段夹具的无AP/DP/AC状态重算。返回的before/after
    指事件提示之前与之后已发生的字段写入，不用终态一次覆盖冒充原写序。
    """
    actor = bytes(actor)
    if len(actor) != 80 or not isinstance(gain, int) or not 0 <= gain <= 0x7fffffff - actor[0x3c]:
        raise ValueError("B4奖励需要完整单位及已证非负signed贡献")
    if actor[7] >= 32:
        raise ValueError("B4受控奖励夹具仅支持已确认玩家ID0..31")
    if actor[0x22:0x25] != b"\0\0\0":
        raise ValueError("B4奖励模型尚未验证带派生buff的成长夹具")
    level_cap = 99 if actor[7] in (30, 31) else 40
    if not gain or actor[5] & 1 or actor[0x21] == level_cap:
        return {"entered": False, "events": [], "final_record": actor,
                "raw_contribution_cleared": False, "gain": gain}
    current = bytearray(actor)
    row = tables["growth"][actor[7] * 11:(actor[7] + 1) * 11]
    if len(row) != 11:
        raise ValueError("B4成长row缺失")
    events = [{"kind": "reward", "fragment": 488, "value": gain, "line": 0,
               "requires_key": False, "record_before": bytes(current), "record_after": bytes(current)}]
    total, line, levels = actor[0x3c] + gain, 2, 0
    while total >= 100:
        if levels >= 256:
            raise ValueError("B4模型拒绝超过一整轮byte等级的异常奖励")
        current[0x21] = (current[0x21] + 1) & 255
        events.append({"kind": "level", "fragment": 489, "value": 0, "line": 1,
                       "level": current[0x21], "requires_key": True,
                       "record_before": bytes(current), "record_after": bytes(current)})
        for index, (offset, fragment_id) in enumerate(GROWTH_FIELDS):
            delta = row[2 * index + 1]
            if not delta:
                continue
            scroll = line == 3
            if scroll:
                line = 2
            before = bytes(current)
            old = struct.unpack_from("<H", current, offset)[0]
            struct.pack_into("<H", current, offset, (old + delta) & 0xffff)
            events.append({"kind": "growth", "fragment": fragment_id, "value": delta,
                           "line": line, "scroll_before": scroll, "offset": offset,
                           "level": current[0x21], "requires_key": True,
                           "record_before": before, "record_after": bytes(current)})
            line += 1
        selector = row[10]
        if selector != 255:
            if selector >= 20:
                raise ValueError("B4学习selector不在完整已证20行表")
            for slot in range(6):
                learned_level, magic_id = tables["learning"][selector * 12 + slot * 2:selector * 12 + slot * 2 + 2]
                if learned_level != current[0x21]:
                    continue
                if magic_id >= 40:
                    raise ValueError("B4学习命中未知魔法位；不假称原版跳过ff哨兵")
                current[0x1a + magic_id // 8] |= 1 << (magic_id % 8)
                events.append({"kind": "learning", "fragment": 587, "value": 0,
                               "line": line, "magic_id": magic_id, "level": current[0x21],
                               "requires_key": False, "record_before": bytes(current), "record_after": bytes(current)})
        struct.pack_into("<4H", current, 0x48, *drop_derived(current, tables["items"]))
        total -= 100
        levels += 1
        if current[0x21] == 30 or actor[7] in (30, 31) and current[0x21] == 99:
            total = 0
    current[0x3c] = total
    return {"entered": True, "events": events, "final_record": bytes(current),
            "levels_gained": levels, "gain": gain, "final_experience": total,
            "raw_contribution_cleared": True,
            "current_hp_mp_preserved": current[0x40:0x42] == actor[0x40:0x42] and current[0x44:0x46] == actor[0x44:0x46]}


def common_tokens(text, index):
    count = struct.unpack_from("<H", text)[0] // 2
    if not 0 <= index < count:
        raise ValueError("B4文字索引超出目录")
    begin = struct.unpack_from("<H", text, index * 2)[0]
    end = struct.unpack_from("<H", text, (index + 1) * 2)[0] if index + 1 < count else len(text)
    if not 2 * count <= begin < end <= len(text) or (end - begin) % 2:
        raise ValueError("B4文字span非法")
    tokens = list(struct.unpack("<" + "h" * ((end - begin) // 2), text[begin:end]))
    if tokens[-1] != -1:
        raise ValueError("B4文字缺结束token")
    return tokens


def derive_fixture(original, scenario):
    """受控START仅改active记录/checksum；每byte由共用exact-manifest记录。"""
    if scenario not in SCENARIOS:
        raise ValueError("未知B4场景")
    fixture = original / "START.SAV"
    source, encrypted = decode_save(fixture), fixture.read_bytes()
    records = [bytearray(source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE]) for i in range(source[META + 1])]
    def word(record, at, value): struct.pack_into("<H", record, at, value)
    for actor in records:
        if actor[6] != 2:
            actor[5] |= 4  # 仅隔离本场无关AI；不是hidden，也不是自然遭遇。
    if scenario in ("heal", "learn"):
        word(records[0], 0x40, 20)
        records[1][0x1b] |= 0x20  # 仅教原记录13；0原能力保持。
        if scenario == "learn":
            records[1][0x21], records[1][0x3c] = 4, 99
    elif scenario in ("physical-cap", "physical-overwrite", "counter-multi"):
        enemy, hero = records[11], records[0]
        if enemy[7] != 96 or enemy[0x31] != 255 or enemy[0x34] & 15:
            raise ValueError("B4物理夹具必须使用原raw11无掉落/普通AI enemy96")
        enemy[:2] = bytes((8, 14))
        if scenario != "physical-overwrite": enemy[0x21] = 30
        for offset, value in ((0x40, 28 if scenario == "physical-overwrite" else 1), (0x4a, 0), (0x4e, 0)):
            word(enemy, offset, value)
        word(hero, 0x4c, 200)
        if scenario == "physical-overwrite":
            hero[0x0b] = 71
            word(hero, 0x48, 3)
            word(enemy, 0x4c, 0)
        if scenario == "counter-multi":
            enemy[5] &= ~4
            enemy[0x0b], enemy[0x3b] = 71, 0
            word(enemy, 0x48, 3); word(enemy, 0x4c, 100)
            word(hero, 0x4a, 0); word(hero, 0x4e, 0)
    else:
        records[0][0x22:0x28] = bytes((1,)) * 6
        # 只为已确认16/12这两个非边界数建立原生1.15初态，不宣称x87全域等价。
        struct.pack_into("<4H", records[0], 0x48, 18, 13, 112, 17)
    changes = [(UNITS + i * STRIDE, record) for i, record in enumerate(records)
               if record != source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE]]
    encoded, controlled, manifest = derive_active_fixture(source, encrypted, "b4-" + scenario, changes)
    manifest.update(b4_scenario=scenario, natural_action_history=False,
                    nonplayer_ai_ineligible_bit=4, untouched_roster_and_manual_slots=True,
                    entry="real title CONTINUE; no test hook or runtime memory patch")
    return encoded, controlled, manifest


def finish_first_end(source, items):
    """B4夹具的第一轮END；所有无关AI04、turn2无事件，不猜未知尾。

    growth/奖励由此前动作或本轮counter模型已应用；本helper只处理原1a30b
    的idle heal、各side一次tick、clear acted及turn2。毒及到期必须返回逐提示事件。
    """
    expected = bytearray(source)
    statuses = []
    for raw in range(source[META + 1]):
        at = UNITS + raw * STRIDE
        actor = bytearray(source[at:at + STRIDE])
        hp, maximum = struct.unpack_from("<HH", actor, 0x40)
        if actor[6] == 2 and not actor[5] & 0x81 and not actor[0x25] and not actor[0x26] and hp != maximum:
            struct.pack_into("<H", actor, 0x40, min(maximum, hp + maximum // 5))
            actor[5] |= 0x80
        expected[at:at + STRIDE] = actor
    # 1a866的毒扫描在所有到期之前；不同阵营按真实1→0→2分别处理。
    for side in (1, 0, 2):
        for raw in range(source[META + 1]):
            at = UNITS + raw * STRIDE
            actor = bytearray(expected[at:at + STRIDE])
            if actor[5] & 1 or actor[6] != side or not actor[0x25]:
                continue
            hp, maximum = struct.unpack_from("<HH", actor, 0x40)
            damage = maximum // 10
            if hp <= damage:
                raise ValueError("本B4状态夹具不覆盖毒杀/掉落；不得放宽全容器")
            struct.pack_into("<H", actor, 0x40, hp - damage)
            expected[at:at + STRIDE] = actor
            statuses.append({"kind": "poison", "fragment": 487, "value": damage, "line": 0,
                             "x": 99, "y": 127, "clear_before": True, "requires_key": False,
                             "raw_actor": raw, "record_before": bytes(actor), "record_after": bytes(actor)})
        for raw in range(source[META + 1]):
            at = UNITS + raw * STRIDE
            actor = bytearray(expected[at:at + STRIDE])
            if actor[5] & 1 or actor[6] != side:
                continue
            for index in range(6):
                offset = 0x22 + index
                if not actor[offset]:
                    continue
                actor[offset] -= 1
                if actor[offset]:
                    continue
                event = {"kind": "status_expiry", "fragment": 481 + index, "value": 0, "line": 0,
                                 "x": 99, "y": 127, "clear_before": True, "requires_key": False,
                                 "raw_actor": raw, "record_before": bytes(actor)}
                plain = bytearray(actor)
                plain[0x22:0x25] = b"\0\0\0"
                # 本场全部buff同时到期，只对低正数精确建立每次原1b750结果。
                values = drop_derived(plain, items)
                if actor[0x22]: values[0] = _stat_scale(values[0])
                if actor[0x23]: values[1] = _stat_scale(values[1])
                if actor[0x24]: values[2] += 15; values[3] += 15
                struct.pack_into("<4H", actor, 0x48, *values)
                event["record_after"] = bytes(actor)
                statuses.append(event)
            expected[at:at + STRIDE] = actor
    for raw in range(source[META + 1]):
        expected[UNITS + raw * STRIDE + 5] &= 0x7f
    expected[META] = 2
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    return bytes(expected), statuses


def _stat_scale(value):
    """1b750 FMUL binary64(1.15)→__CHP朝0；不得改成整数115/100。

    原块+真实__CHP已局部执行[-65536,65535]全域；本B4低正数夹具
    在该范围内，100的结果114而非115。超读集拒绝，不推广未验边界。
    """
    if not -65536 <= value <= 65535:
        raise ValueError("B4状态模型超出原块局部执行读集")
    return int(float(value) * 1.15)


def expected_state(original, source, scenario, tables):
    expected = bytearray(source)
    raw, gain = (1, 20 if scenario == "heal" else 5) if scenario in ("heal", "learn") else (0, 630 if scenario == "counter-multi" else 3 if scenario == "physical-overwrite" else 99)
    events = []
    if scenario in ("heal", "learn"):
        struct.pack_into("<H", expected, UNITS + 0x40, 42)
        struct.pack_into("<H", expected, UNITS + STRIDE + 0x44, 5)
        expected[UNITS + STRIDE + 5] |= 0x80
    elif scenario in ("physical-cap", "physical-overwrite", "counter-multi"):
        at = UNITS + 11 * STRIDE
        struct.pack_into("<H", expected, at + 0x40, 24 if scenario == "physical-overwrite" else 0)
        if scenario != "physical-overwrite": expected[at + 5] = 1
        for index in range(source[META + 1]):
            expected[UNITS + index * STRIDE + 3:UNITS + index * STRIDE + 5] = b"\0\0"
        if scenario != "counter-multi":
            expected[UNITS + 5] |= 0x80
        else:
            # 原AI动手前END不会恢复满HP hero；两击各2，反击首击击杀。
            struct.pack_into("<H", expected, UNITS + 0x40, 38)
    if scenario != "status":
        at = UNITS + raw * STRIDE
        reward = reward_plan(expected[at:at + STRIDE], gain, tables)
        expected[at:at + STRIDE] = reward["final_record"]
        events = reward["events"]
    else:
        reward = None
    expected[-4:] = sum(expected[:-4]).to_bytes(4, "little")
    immediate = bytes(expected)
    if scenario == "counter-multi":
        # idle heal已在敌攻击之前执行；不能在成长max114之后错误地补22HP。
        final = bytearray(expected)
        for index in range(source[META + 1]): final[UNITS + index * STRIDE + 5] &= 0x7f
        final[META] = 2; final[-4:] = sum(final[:-4]).to_bytes(4, "little")
        status_events = []
    else:
        final, status_events = finish_first_end(immediate, tables["items"])
    return {"immediate": immediate, "final": bytes(final), "events": events,
            "status_events": status_events, "reward": reward, "reward_actor": raw,
            "effect_gain": 0 if scenario == "status" else gain,
            "first_end_already_executed": scenario in ("status", "counter-multi")}


def magic_page(original, actor, assets, selected=-1):
    """1ceed列表纯资源；调用者区别F2 selected=-1与0/13正式施法选择。"""
    ids = [8 * byte + bit for byte in range(5) for bit in range(8) if actor[0x1a + byte] & (1 << bit)]
    if not 1 <= len(ids) <= 12 or any(identity >= 36 for identity in ids):
        raise ValueError("B4 F2法术页超出已证普通36记录/12项布局")
    if selected != -1 and not 0 <= selected < len(ids):
        raise ValueError("B4法术资源选择索引非法")
    other = (original / "FDOTHER.DAT").read_bytes()
    font, sheet = archive_entry(other, 4), archive_entry(other, 5)
    text = archive_entry((original / "FDTXT.DAT").read_bytes(), 0)
    from run_original_player import sheet_frame
    from fd2_item_list_oracle import raw_icon
    canvas = sheet_frame(sheet, 21, lmi=True)
    canvas.putpalette(assets["palette"])
    magic, _ = use_effect_tables(original / "FD2.EXE")
    cost_icon = raw_icon(sheet, 92, (20, 8), assets["palette"])
    for index, magic_id in enumerate(ids):
        x, y = 18 + 100 * (index // 4), 103 + 22 * (index % 4)
        tokens = common_tokens(text, 441 + magic_id)
        if any(token < 0 for token in tokens[:-1]): raise ValueError("B4普通魔法名含未证control")
        glyphs(canvas, font, tokens[:-1], x - 5, y - 94, 201 if selected == index else 205)
        canvas.paste(cost_icon, (x + 45, y - 89))
        number(canvas, sheet, magic["magic"][magic_id * 7 + 5], x + 68, y - 89, 42, 2)
    return ((5, 94), canvas), {"magic_ids": ids, "selected": selected, "learned_cast_supported": False}


def reward_pages(original, assets, actor_id, events):
    """1e292/1e529的bottom对话文字/卷动；每个-3保留严格箭头门。

    仅普通玩家DATO ID；1956b的36887为reverse右端(87,115)，
    4ec31先写再--故80px实际覆盖x8..87。正式battle map非NULL，
    16c57的箭头左x191，不接受非战场的另一个x143分支。
    """
    text = archive_entry((original / "FDTXT.DAT").read_bytes(), 0)
    other = (original / "FDOTHER.DAT").read_bytes()
    font, sheet = archive_entry(other, 4), archive_entry(other, 5)
    canvas = Image.new("P", (320, 200), 0)
    canvas.putpalette(assets["palette"])
    arrows = [raw_ui(sheet, index) for index in (18, 19)]
    for arrow in arrows:
        arrow.putpalette(assets["palette"])
    result = []
    portrait_entry = archive_entry((original / "DATO.DAT").read_bytes(), actor_id)
    portrait_images = []
    for frame_id in range(4):
        start = struct.unpack_from("<I", portrait_entry, frame_id * 4)[0]
        end = struct.unpack_from("<I", portrait_entry, (frame_id + 1) * 4)[0] if frame_id < 3 else len(portrait_entry)
        if not 16 <= start < end <= len(portrait_entry):
            raise ValueError("B4 DATO四帧span非法")
        image = decode_lmi(portrait_entry[start:end])
        if image.size != (80, 80): raise ValueError("B4普通portrait不是80x80")
        image = ImageOps.mirror(image); image.putpalette(assets["palette"])
        portrait_images.append(image)
    def draw(tokens, value, learned, x, y):
        origin_x = x
        for token in tokens:
            if token == -1:
                return
            if token == -3:
                continue  # 全部已证B4片段仅以-3结束一行；不绘制新行字符。
            if token == -2:
                x, y = origin_x, y + 19
                continue
            if token == -6:
                expanded = [int(c) for c in str(value)]
            elif token == -4:
                if learned is None:
                    raise ValueError("B4动态学习名缺少已证magic ID")
                expanded = common_tokens(text, 441 + learned)[:-1]
            elif 0 <= token < len(font) // 32:
                expanded = [token]
            else:
                raise ValueError(f"B4未建模文字control {token}")
            for glyph in expanded:
                if glyph < 0 or (glyph + 1) * 32 > len(font) or x + 16 > 303 or y + 17 > 200:
                    raise ValueError("B4文字字模/显示边界非法")
                canvas.paste(74, (x, y, x + 16, y + 16))
                glyphs(canvas, font, [glyph], x, y, 205)
                x += 16
    for ordinal, event in enumerate(events):
        if event.get("clear_before"):
            canvas.paste(0, (0, 0, 320, 200))
        if event.get("scroll_before"):
            for delta in (3, 3, 3, 3, 3, 4):
                region = canvas.crop((94, 119 + delta, 302, 119 + delta + 72))
                canvas.paste(region, (94, 119))
                canvas.paste(74, (95, 191, 303, 192))
        tokens = common_tokens(text, event["fragment"])
        if (-3 in tokens) != event["requires_key"] or any(token < 0 and token not in (-1, -2, -3, -4, -6) for token in tokens):
            raise ValueError("B4片段control与已确认等待合同不同")
        draw(tokens, event["value"], event.get("magic_id"), event.get("x", 95), event.get("y", 119 + 19 * event["line"]))
        expected = canvas.copy()
        # 所有普通bottom对话共用16×16箭头擦除区；另独立完整匹配18/19。
        if event["requires_key"]:
            expected.paste(0, (191, 176, 207, 192))
        if sum(p != 0 for p in expected.getdata()) < 128:
            raise ValueError("B4页面已知文字像素不足")
        # 只有-3等待会blink0/3。自动状态和学习无16559收尾，保留逐字嘴型0..2；
        # 488之后1e292明确16559(0)，不能给自动页误套0/3。
        frame_ids = (0, 3) if event["requires_key"] else (0,) if event["fragment"] == 488 else (0, 1, 2)
        result.append({**{key: value for key, value in event.items() if key not in ("record_before", "record_after")},
                       "ordinal": ordinal, "text": expected, "arrows": arrows,
                       "portraits": [portrait_images[i] for i in frame_ids], "portrait_frames": frame_ids,
                       "portrait_position": (8, 115), "arrow_position": (191, 181)})
        # 原16c57返回时frame13擦除16×16；后续scroll只携带已知写集。
        if event["requires_key"]:
            canvas.paste(0, (191, 176, 207, 192))
    return result


def page_match(frame, page, assets):
    found = match_indexed(frame, page["text"], (0, 0))
    border = match_indexed(frame, assets["dialog_top"], (5, 112))
    if not found["ok"] or not border["ok"]:
        return None
    portrait = next(({"frame": index, **hit} for index, image in zip(page["portrait_frames"], page["portraits"])
                     if (hit := exact_tile(frame, image, page["portrait_position"]))), None)
    if portrait is None:
        return None
    arrow = None
    if page["requires_key"]:
        arrow = next(({"frame": 18 + index, **hit} for index, image in enumerate(page["arrows"])
                      if (hit := exact_tile(frame, image, page["arrow_position"]))), None)
        if arrow is None:
            return None
    return {"kind": "reward_resource_wait" if page["requires_key"] else "reward_resource_automatic",
            "fragment": page["fragment"], "ordinal": page["ordinal"], "requires_key": page["requires_key"],
            "text": found, "portrait": portrait, "dialog_border": border, "arrow": arrow}
