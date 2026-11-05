"""物品列表原版资源；调用者锁定完整START/精确派生，不运行进程或改存档。

依据field_unit_detail_equipment_draw @0x184c0（原登记同名）、
field_unit_inventory_select @0x1b932（原sub_1B932）和
field_unit_inventory_select_step @0x1b9de（原sub_1B9DE）。
物品表由field_item_record_get @0x4e8bc的object3 VA0x602ad取得；
装备许可来自field_equipment_profile_get @0x4e88e（原sub_4E88E），
玩家效果依据field_item_use_effect_execute @0x20c6f（原sub_20C6F）。
属性条依据field_stat_bar_draw @0x18795（原sub_18795）和
field_stat_bar_columns_draw @0x17d6f（原sub_17D6F）。
复用既有LE loader、字形和数字primitive，不从SDL内置表构造oracle。
"""
import hashlib
import struct

from PIL import Image

from fd2_magic_list_oracle import fragment, glyphs, number as decimal_digits
from rebuild_fd2_analysis import build_relbase_image, parse_le, parse_objects
from run_original_comparison import archive_entry, decode_image
from run_original_end_turn import STRIDE, UNITS, round_assets, validate_initial
from run_original_player import sheet_frame

START_SHA256 = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"
PLAIN_SHA256 = "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975"
EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
TABLE_VA, TABLE_COUNT, RECORD_SIZE = 0x602ad, 215, 23
TABLE_SHA256 = "bb64df69e73dfc4d642d342aac7bad8d7c2f4fb0527bb55d35c153b9f79432ca"
EQUIPMENT_VA, EQUIPMENT_ROWS = 0x6188a, 29
EQUIPMENT_SHA256 = "f1dd24fd3d1045fc0a82f07442a0ef1ef0c650bb102044e1bf564b97206addba"
SLOTS = ((0x40, 0), (0x40, 132), (0, 192))
AFTER_USE_INVENTORY = "40004084800080008000800080c980c9"


def derive_injured(source, encrypted):
    """仅active raw0 HP42→20、EX0→99及checksum；不改站位/roster/能力。"""
    if (hashlib.sha256(encrypted).hexdigest() != START_SHA256 or
            hashlib.sha256(source).hexdigest() != PLAIN_SHA256):
        raise ValueError("受控草药夹具必须从完整已确认START明文及密文派生")
    changed = bytearray(source)
    struct.pack_into("<H", changed, UNITS + 0x40, 20)
    changed[UNITS + 0x3c] = 99
    changed[-4:] = sum(changed[:-4]).to_bytes(4, "little")
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, changed))
    def patches(before, after):
        return [{"offset": index, "before": a, "after": b}
                for index, (a, b) in enumerate(zip(before, after)) if a != b]
    return encoded, bytes(changed), {"scenario": "herb-injured", "controlled_fixture": True,
        "source_sha256": START_SHA256, "source_plain_sha256": PLAIN_SHA256,
        "derived_sha256": hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256": hashlib.sha256(changed).hexdigest(),
        "plaintext_patches": patches(source, changed), "encrypted_patches": patches(encrypted, encoded),
        "checksum_before": source[-4:].hex(), "checksum_after": changed[-4:].hex(),
        "excluded": "natural injury/experience history; runtime RNG/whole-owner equivalence"}


def item_table(executable):
    data = executable.read_bytes()
    # 先锁定完整输入，不能让任意LE尺寸进入分配或当成本作物品表。
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("物品oracle仅支持已确认的完整FD2.EXE")
    le = parse_le(data)
    objects = parse_objects(data, le)
    size = TABLE_COUNT * RECORD_SIZE
    owners = [obj for obj in objects if obj.relbase <= TABLE_VA and
              TABLE_VA + size <= obj.relbase + obj.vsize]
    if len(owners) != 1 or owners[0].index != 3 or owners[0].relbase != 0x60000:
        raise ValueError("物品表不在已确认的object3完整virtual范围内")
    table = bytes(build_relbase_image(data, le, objects)[TABLE_VA:TABLE_VA + size])
    if len(table) != size or hashlib.sha256(table).hexdigest() != TABLE_SHA256:
        raise ValueError("物品表strictLE4945字节不符")
    return table, {"getter_va": 0x4e8bc, "canonical_va": TABLE_VA,
                   "object": 3, "object_offset": 0x2ad, "size": size,
                   "record_count": TABLE_COUNT, "record_size": RECORD_SIZE,
                   "sha256": TABLE_SHA256, "executable_sha256": EXE_SHA256,
                   "source": "parse_le/parse_objects/build_relbase_image raw-unrelocated"}


def equipment_rows(executable):
    """field_equipment_profile_get @0x4e88e（原sub_4E88E）经
    field_unit_can_equip_item @0x1c1c3（原sub_1C1C3）读取的数据VA0x6188a。

    到下一指针表0x61955的连续29×7B；只前6B为许可，第7B仍未知。
    原getter不校index；这里不把后继数据猜为更多profile。
    """
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("装备许可oracle仅支持已确认的完整FD2.EXE")
    le = parse_le(data)
    objects = parse_objects(data, le)
    end = EQUIPMENT_VA + EQUIPMENT_ROWS * 7
    if end != 0x61955 or not any(obj.index == 3 and obj.relbase <= EQUIPMENT_VA and end <= obj.relbase + obj.vsize for obj in objects):
        raise ValueError("装备许可完整203字节不在object3已确认范围")
    rows = bytes(build_relbase_image(data, le, objects)[EQUIPMENT_VA:end])
    if hashlib.sha256(rows).hexdigest() != EQUIPMENT_SHA256:
        raise ValueError("装备许可29行strictLE哈希不符")
    return rows


def use_effect_tables(executable):
    """20c6f→1c75e/1cd17：原魔法36×7B及profile1..28 DWORD倍率。"""
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("Use效果oracle只支持已确认完整FD2.EXE")
    le = parse_le(data); objects = parse_objects(data, le)
    image = build_relbase_image(data, le, objects)
    result = {}
    for name, va, size in (("magic", 0x619fd, 36 * 7), ("damage_profiles", 0x51f96, 28 * 4)):
        if len([obj for obj in objects if obj.relbase <= va and va + size <= obj.relbase + obj.vsize]) != 1:
            raise ValueError("Use效果表不在唯一已确认canonical对象内")
        result[name] = bytes(image[va:va + size])
        if len(result[name]) != size:
            raise ValueError("Use效果原表截断")
    return result, {name: {"canonical_va": va, "size": len(result[name]),
        "sha256": hashlib.sha256(result[name]).hexdigest()} for name, va in (("magic", 0x619fd), ("damage_profiles", 0x51f96))}


def raw_icon(sheet, index, size, palette):
    if sheet[:4] != b"LMI1" or len(sheet) < 6:
        raise ValueError("物品详情目录无效")
    count = struct.unpack_from("<H", sheet, 4)[0]
    if index + 1 >= count or 6 + count * 4 > len(sheet):
        raise ValueError("物品详情raw帧目录越界")
    start, end = struct.unpack_from("<II", sheet, 6 + index * 4)
    if not 6 + count * 4 <= start < end <= len(sheet):
        raise ValueError("物品详情raw帧边界无效")
    if end - start != 4 + size[0] * size[1] or struct.unpack_from("<HH", sheet, start) != size:
        raise ValueError("物品详情raw帧尺寸或长度不符")
    image = Image.frombytes("P", size, sheet[start + 4:end])
    image.putpalette(palette)
    return image


def rle_icon(sheet, index, size, palette):
    """41为RLE；只在原token无opaque0时按0透明叠加。"""
    if sheet[:4] != b"LMI1" or len(sheet) < 6:
        raise ValueError("物品RLE目录无效")
    count = struct.unpack_from("<H", sheet, 4)[0]
    if not 0 <= index < count or 6 + count * 4 > len(sheet):
        raise ValueError("物品RLE索引越界")
    start = struct.unpack_from("<I", sheet, 6 + index * 4)[0]
    end = struct.unpack_from("<I", sheet, 6 + (index + 1) * 4)[0] if index + 1 < count else len(sheet)
    if not 6 + count * 4 <= start < end <= len(sheet):
        raise ValueError("物品RLE帧边界无效")
    data = sheet[start:end]
    image = decode_image(data)
    if image.size != size:
        raise ValueError("物品RLE尺寸不符")
    cursor = 4
    for _ in range(size[1]):
        x = 0
        while x < size[0]:
            if cursor >= len(data):
                raise ValueError("物品RLE控制截断")
            control = data[cursor]; cursor += 1
            mode, length = control & 192, (control & 63) + 1
            span = length * 2 if mode == 64 else length
            if x + span > size[0]:
                raise ValueError("物品RLE超行")
            if mode != 192:
                needed = length if mode == 128 else 1
                if cursor + needed > len(data) or 0 in data[cursor:cursor + needed]:
                    raise ValueError("物品RLE截断或包含opaque0，不能使用透明缓存")
                cursor += needed
            x += span
    if cursor != len(data):
        raise ValueError("物品RLE尾部不符")
    image.putpalette(palette)
    return image


def number(canvas, sheet, value, x, y, bank, width):
    """ui_number_draw @0x187d6（原登记同名）：负数夹0，固定宽度使用原溢出帧。"""
    if width not in (2, 3):
        raise ValueError("物品详情数字只覆盖原版已确认的2/3位宽")
    value = max(0, value)
    overflow = 93 if width == 2 and value > 99 else bank + 10 if width == 3 and value > 999 else None
    if overflow is None:
        decimal_digits(canvas, sheet, value, x, y, bank, width)
        return
    # 原FDOTHER[5] frame129（bank119+10）实际17px宽，93/41/52分别12/18/18。
    overflow_width = 12 if width == 2 else 17 if bank == 119 else 18
    image = rle_icon(sheet, overflow, (overflow_width, 8), canvas.getpalette())
    # rle_icon已逐token拒opaque0；这里的0只来自原生transparent skip。
    canvas.paste(image, (x, y), image.point(lambda value: 255 if value else 0, mode="L"))


def signed_word(value):
    return value - 65536 if value & 0x8000 else value


def word_bar(canvas, sheet, raw_current, raw_maximum, x, y, first_frame):
    """field_stat_bar_draw @0x18795（原sub_18795）→
    field_stat_bar_columns_draw @0x17d6f（原sub_17D6F）：signed idiv，负长两cap。
    """
    current, maximum = signed_word(raw_current), signed_word(raw_maximum)
    if maximum == 0:
        return {"signed_current": current, "signed_maximum": maximum, "drawn": False}
    numerator = 101 * current
    quotient = abs(numerator) // abs(maximum)
    if (numerator < 0) != (maximum < 0): quotient = -quotient
    length = quotient + 1 if current else 0
    if length > 102:
        raise ValueError("本B3夹具HP/MP条超出原103px正常区域，未声明该原版越界绘制")
    palette = canvas.getpalette()
    def paste(frame, offset):
        canvas.paste(raw_icon(sheet, frame, (1, 5), palette), (x + offset, y))
    if length:
        paste(first_frame, 0)
        for offset in range(1, length): paste(first_frame + 1, offset)
        paste(first_frame + 2, max(1, length))
    else:
        for offset in range(1, 102): paste(29, offset)
        paste(30, 102)
    return {"signed_current": current, "signed_maximum": maximum, "native_length": length, "drawn": True}


def prepare(original, fixture, *, source=None, after_use=False):
    encrypted = fixture.read_bytes()
    if hashlib.sha256(encrypted).hexdigest() != START_SHA256:
        raise ValueError("物品校准仅支持已确认的只读真实START.SAV来源")
    base, focus = validate_initial(fixture, 0)
    if source is None:
        source = base
    elif bytes(source) not in (base, derive_injured(base, encrypted)[1]):
        raise ValueError("物品资源仅接受原START或HP20/EX99完整exact manifest派生")
    actor = bytearray(source[UNITS:UNITS + STRIDE])
    slots = tuple(tuple(actor[0x0a + i * 2:0x0c + i * 2]) for i in range(3))
    if slots != SLOTS or any(not actor[0x0a + i * 2] & 0x80 for i in range(3, 8)):
        raise ValueError("真实START raw0库存不是已确认的密集三槽")
    if (actor[7:9] != b"\0\0" or struct.unpack_from("<HH", actor, 0x40) not in ((42, 42), (20, 42)) or
            struct.unpack_from("<HH", actor, 0x44) != (0, 0)):
        raise ValueError("物品列表角色身份或HP/MP前置不符")
    if after_use:
        # 1b8e7移除raw slot2：只左移后槽并写末flag80，不能清stale ID。
        actor[0x0e:0x18] = actor[0x10:0x1a]
        actor[0x18] = 0x80
        struct.pack_into("<H", actor, 0x40, 42)
        if actor[0x0a:0x1a].hex() != AFTER_USE_INVENTORY:
            raise ValueError("草药后库存预期模型与已确认1b8e7写集不符")
        slots = slots[:2]
    assets, templates, fields = inventory_templates(original, actor)
    fields["raw_actor"] = 0
    if [(item["category_frame"], item["effect_frame"], item["value"], item["use_code"])
            for item in fields["slots"]] != [(62, 64, 10, 0), (63, 65, 8, 0), (61, 66, 40, 5)][:len(slots)]:
        raise ValueError("真实START物品表字段偏离已确认的三项")
    fields["after_use_model"] = after_use
    return source, focus, assets, templates, fields


def inventory_templates(original, actor, assets=None):
    """纯资源构图：调用者先验证完整来源/派生manifest，不接受任意SAV。

    依184c0按原23B表构造最多8项、两列；名称使用actor.text_id。
    来源/写集由场景完整manifest约束，此纯构图不接收任意SAV。空库存只供删空后的
    COMMAND/无列表判据，不把空页当原版会打开的列表。
    """
    if len(actor) != STRIDE:
        raise ValueError("物品构图需要完整80字节原单位")
    slots = [tuple(actor[0x0a + i * 2:0x0c + i * 2]) for i in range(8)]
    count = sum(not flags & 0x80 for flags, _ in slots)
    if any(bool(flags & 0x80) != (i >= count) for i, (flags, _) in enumerate(slots)):
        raise ValueError("物品操作夹具仅支持已确认密集库存，不修整原始槽")
    slots = slots[:count]
    if any(item_id >= TABLE_COUNT for _, item_id in slots):
        raise ValueError("物品构图包含原表范围之外的ID")
    table, evidence = item_table(original / "FD2.EXE")
    if assets is None:
        assets = round_assets(original)
    other = (original / "FDOTHER.DAT").read_bytes()
    sheet, font = archive_entry(other, 5), archive_entry(other, 4)
    text = archive_entry((original / "FDTXT.DAT").read_bytes(), 0)
    bottom, top = sheet_frame(sheet, 21, lmi=True), sheet_frame(sheet, 20, lmi=True)
    if bottom.size != (310, 99) or top.size != (223, 86) or 0 in bottom.getdata():
        raise ValueError("物品详情底板尺寸或不透明性不符")
    bottom.putpalette(assets["palette"])
    top.putpalette(assets["palette"])
    fields = []
    templates = {}
    for selected in (-1, *range(len(slots))):
        canvas = bottom.copy()
        for slot, (flags, item_id) in enumerate(slots):
            record = table[item_id * RECORD_SIZE:(item_id + 1) * RECORD_SIZE]
            category = record[0]
            icon = (59 if category < 0x15 else 60 if category < 0x20 else 61) + (3 if flags & 0x40 else 0)
            has_effect = category < 0x20 or category == 0x20 and record[13] in (5, 11)
            effect_icon = 64 if category < 0x15 else 65 if category < 0x20 else 67 if record[13] == 11 else 66
            value = struct.unpack_from("<h", record, 1 if category < 0x15 else 5 if category < 0x20 else 14)[0]
            x, y = 42 + 150 * (slot // 4), 101 + 22 * (slot % 4)
            # 1685c→4ed0b完整raw复制，包括0；不使用透明mask。
            canvas.paste(raw_icon(sheet, icon, (24, 20), assets["palette"]), (x - 29 - 5, y - 94))
            name = fragment(text, item_id + 181)
            glyphs(canvas, font, name, x - 5, y + 2 - 94, 201 if selected == slot else 205)
            if has_effect:
                canvas.paste(raw_icon(sheet, effect_icon, (21, 8), assets["palette"]), (x + 68 - 5, y + 6 - 94))
                number(canvas, sheet, value, x + 93 - 5, y + 6 - 94, 42, 3)
            else:
                fallback = rle_icon(sheet, 41, (18, 8), assets["palette"])
                canvas.paste(fallback, (x + 68 - 5, y + 6 - 94), fallback.point(lambda c: 255 if c else 0, mode="L"))
            if selected == 0:
                fields.append({"raw_slot": slot, "flags": flags, "item_id": item_id,
                               "record_hex": record.hex(), "use_code": record[13],
                               "name_fragment": item_id + 181, "name_tokens": list(name),
                               "category_frame": icon, "effect_frame": effect_icon if has_effect else 41, "has_effect": has_effect, "value": value})
        templates["unselected" if selected == -1 else f"selected_{selected}"] = ((5, 94), canvas)
    name = fragment(text, actor[8] + 1)
    glyphs(top, font, name, 99 - 92, 13 - 7, 205)
    hp, maximum = struct.unpack_from("<HH", actor, 0x40)
    number(top, sheet, signed_word(hp), 267 - 92, 41 - 7, 31 if hp == maximum else 42, 3)
    number(top, sheet, signed_word(maximum), 293 - 92, 41 - 7, 31, 3)
    mp, mp_max = struct.unpack_from("<HH", actor, 0x44)
    number(top, sheet, signed_word(mp), 267 - 92, 59 - 7, 31 if mp == mp_max else 42, 3)
    number(top, sheet, signed_word(mp_max), 293 - 92, 59 - 7, 31, 3)
    hp_bar = word_bar(top, sheet, hp, maximum, 198 - 92, 33 - 7, 23)
    templates["hp_bar"] = ((198, 33), top.crop((106, 26, 209, 31)))
    bar = word_bar(top, sheet, mp, mp_max, 198 - 92, 52 - 7, 26)
    templates["mp_bar"] = ((198, 52), top.crop((106, 45, 209, 50)))
    number(top, sheet, actor[0x21], 157 - 92, 33 - 7, 42, 2)
    number(top, sheet, actor[0x3c], 157 - 92, 44 - 7, 42, 2)
    dexterity = struct.unpack_from("<H", actor, 0x3e)[0]
    number(top, sheet, signed_word(dexterity), 117 - 92, 55 - 7, 42, 3)
    number(top, sheet, actor[0x3b], 157 - 92, 55 - 7, 42, 2)
    templates["dexterity"] = ((117, 55), top.crop((25, 48, 43, 56)))
    templates["movement"] = ((157, 55), top.crop((65, 48, 77, 56)))
    # 17fc0的+25..27三个状态图，0时也比较底板，不能仅接受图标消失。
    for index in range(3):
        image = raw_icon(sheet, 55 + index, (24, 20), assets["palette"])
        x, y = 194 + index * 35, 68
        if actor[0x25 + index]:
            top.paste(image, (x - 92, y - 7))
        templates[f"status_{0x25 + index:02x}"] = ((x, y), top.crop((x - 92, y - 7, x - 92 + image.width, y - 7 + image.height)))
    # field_unit_detail_stats_draw @0x17fc0（原登记同名）：
    # 21597/25437/21557/25397分别为四个目标地址偏移，
    # 无+22..24状态时bank42；Drop装备必须同时验当前页四派生数字。
    for label, offset, x, y, status in (("attack", 0x48, 157, 67, 0x22),
            ("defense", 0x4a, 157, 79, 0x23), ("accuracy", 0x4c, 117, 67, 0x24),
            ("evasion", 0x4e, 117, 79, 0x24)):
        number(top, sheet, struct.unpack_from("<h", actor, offset)[0], x - 92, y - 7,
               119 if actor[status] else 42, 3)
        templates[label] = ((x, y), top.crop((x - 92, y - 7, x - 92 + 18, y - 7 + 8)))
    templates["actor_name"] = ((98, 13), top.crop((6, 6, 7 + 16 * len(name), 23)))
    templates["current_max_hp"] = ((267, 41), top.crop((175, 34, 219, 42)))
    templates["current_max_mp"] = ((267, 59), top.crop((175, 52, 219, 60)))
    templates["level"] = ((157, 33), top.crop((65, 26, 77, 34)))
    templates["experience"] = ((157, 44), top.crop((65, 37, 77, 45)))
    return assets, templates, {"unit_id": actor[7], "text_id": actor[8],
        "slots": fields, "item_table": evidence, "list_bottom": [5, 94, 310, 99],
        "actor_fragment": actor[8] + 1, "actor_tokens": list(name), "hp": hp, "max_hp": maximum, "mp": mp, "max_mp": mp_max,
        "level": actor[0x21], "experience": actor[0x3c],
        "dexterity": dexterity, "movement": actor[0x3b], "status_bytes": list(actor[0x22:0x28]),
        "derived_values": list(struct.unpack_from("<4H", actor, 0x48)),
        "expected_inventory_hex": actor[0x0a:0x1a].hex(), "after_use_model": False,
        "runtime_raw_inventory_measured": False,
        "list_is_equipment_page": True, "use_confirmed": False,
        "signed_word_display": {name: {"expected_raw_u16": struct.unpack_from("<H", actor, offset)[0],
            "signed_input": struct.unpack_from("<h", actor, offset)[0],
            "nonnegative_number_input": max(0, struct.unpack_from("<h", actor, offset)[0])}
            for name, offset in (("hp", 0x40), ("max_hp", 0x42), ("mp", 0x44), ("max_mp", 0x46),
                ("dexterity", 0x3e), ("attack", 0x48), ("defense", 0x4a), ("accuracy", 0x4c), ("evasion", 0x4e))},
        "hp_bar": hp_bar, "mp_bar": bar, "runtime_ui_rng_alignment": "unverified; 16c57 wait consumes shared RNG"}
