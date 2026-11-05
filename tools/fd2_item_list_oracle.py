"""真实START raw0物品列表资源；不运行进程或改写存档。

依据field_unit_detail_equipment_draw @0x184c0（原登记同名）、
field_unit_inventory_select @0x1b932（原sub_1B932）和
field_unit_inventory_select_step @0x1b9de（原sub_1B9DE）。
物品表由field_item_record_get @0x4e8bc的object3 VA0x602ad取得；
复用既有LE loader、字形和数字primitive，不从SDL内置表构造oracle。
"""
import hashlib
import struct

from PIL import Image

from fd2_magic_list_oracle import fragment, glyphs, number
from rebuild_fd2_analysis import build_relbase_image, parse_le, parse_objects
from run_original_comparison import archive_entry
from run_original_end_turn import STRIDE, UNITS, round_assets, validate_initial
from run_original_player import sheet_frame

START_SHA256 = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"
PLAIN_SHA256 = "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975"
EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
TABLE_VA, TABLE_COUNT, RECORD_SIZE = 0x602ad, 215, 23
TABLE_SHA256 = "bb64df69e73dfc4d642d342aac7bad8d7c2f4fb0527bb55d35c153b9f79432ca"
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
    if [(item["category_frame"], item["effect_frame"], item["value"], item["use_code"])
            for item in fields["slots"]] != [(62, 64, 10, 0), (63, 65, 8, 0), (61, 66, 40, 5)][:len(slots)]:
        raise ValueError("真实START物品表字段偏离已确认的三项")
    fields["after_use_model"] = after_use
    return source, focus, assets, templates, fields


def inventory_templates(original, actor, assets=None):
    """纯资源构图：调用者先验证完整来源/派生manifest，不接受任意SAV。

    只覆盖已核0/1/132/192的最多三项重排。空库存只供删空后的
    COMMAND/无列表判据，不把空页当原版会打开的列表。
    """
    if len(actor) != STRIDE or actor[7:9] != b"\0\0":
        raise ValueError("物品构图仅覆盖已确认raw0角色")
    slots = [tuple(actor[0x0a + i * 2:0x0c + i * 2]) for i in range(8)]
    count = sum(not flags & 0x80 for flags, _ in slots)
    if count > 3 or any(bool(flags & 0x80) != (i >= count) for i, (flags, _) in enumerate(slots)):
        raise ValueError("物品构图仅覆盖最多三项的已确认密集库存")
    slots = slots[:count]
    if any(item_id not in (0, 1, 132, 192) for _, item_id in slots):
        raise ValueError("物品构图包含本轮未校准的物品资源")
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
            effect_icon = 64 if category < 0x15 else 65 if category < 0x20 else 66
            value = struct.unpack_from("<h", record, 1 if category < 0x15 else 5 if category < 0x20 else 14)[0]
            y = 101 + 22 * slot
            # 1685c→4ed0b完整raw复制，包括0；不使用透明mask。
            canvas.paste(raw_icon(sheet, icon, (24, 20), assets["palette"]), (13 - 5, y - 94))
            name = fragment(text, item_id + 181)
            glyphs(canvas, font, name, 42 - 5, y + 2 - 94, 201 if selected == slot else 205)
            canvas.paste(raw_icon(sheet, effect_icon, (21, 8), assets["palette"]), (110 - 5, y + 6 - 94))
            number(canvas, sheet, value, 135 - 5, y + 6 - 94, 42, 3)
            if selected == 0:
                fields.append({"raw_slot": slot, "flags": flags, "item_id": item_id,
                               "record_hex": record.hex(), "use_code": record[13],
                               "name_fragment": item_id + 181, "name_tokens": list(name),
                               "category_frame": icon, "effect_frame": effect_icon, "value": value})
        templates["unselected" if selected == -1 else f"selected_{selected}"] = ((5, 94), canvas)
    name = fragment(text, 1)
    glyphs(top, font, name, 99 - 92, 13 - 7, 205)
    hp, maximum = struct.unpack_from("<HH", actor, 0x40)
    number(top, sheet, hp, 267 - 92, 41 - 7, 31 if hp == maximum else 42, 3)
    number(top, sheet, maximum, 293 - 92, 41 - 7, 31, 3)
    mp, mp_max = struct.unpack_from("<HH", actor, 0x44)
    number(top, sheet, mp, 267 - 92, 59 - 7, 31 if mp == mp_max else 42, 3)
    number(top, sheet, mp_max, 293 - 92, 59 - 7, 31, 3)
    number(top, sheet, actor[0x21], 157 - 92, 33 - 7, 42, 2)
    number(top, sheet, actor[0x3c], 157 - 92, 44 - 7, 42, 2)
    # field_unit_detail_stats_draw @0x17fc0（原登记同名）：
    # 21597/25437/21557/25397分别为四个目标地址偏移，
    # 无+22..24状态时bank42；Drop装备必须同时验当前页四派生数字。
    for label, offset, x, y, status in (("attack", 0x48, 157, 67, 0x22),
            ("defense", 0x4a, 157, 79, 0x23), ("accuracy", 0x4c, 117, 67, 0x24),
            ("evasion", 0x4e, 117, 79, 0x24)):
        number(top, sheet, struct.unpack_from("<H", actor, offset)[0], x - 92, y - 7,
               119 if actor[status] else 42, 3)
        templates[label] = ((x, y), top.crop((x - 92, y - 7, x - 92 + 18, y - 7 + 8)))
    templates["actor_name"] = ((98, 13), top.crop((6, 6, 7 + 16 * len(name), 23)))
    templates["current_max_hp"] = ((267, 41), top.crop((175, 34, 219, 42)))
    templates["current_max_mp"] = ((267, 59), top.crop((175, 52, 219, 60)))
    templates["level"] = ((157, 33), top.crop((65, 26, 77, 34)))
    templates["experience"] = ((157, 44), top.crop((65, 37, 77, 45)))
    return assets, templates, {"raw_actor": 0, "unit_id": 0,
        "slots": fields, "item_table": evidence, "list_bottom": [5, 94, 310, 99],
        "actor_fragment": 1, "actor_tokens": list(name), "hp": hp, "max_hp": maximum, "mp": mp, "max_mp": mp_max,
        "level": actor[0x21], "experience": actor[0x3c],
        "derived_values": list(struct.unpack_from("<4H", actor, 0x48)),
        "expected_inventory_hex": actor[0x0a:0x1a].hex(), "after_use_model": False,
        "runtime_raw_inventory_measured": False,
        "list_is_equipment_page": True, "use_confirmed": False,
        "runtime_ui_rng_alignment": "unverified; 16c57 wait consumes shared RNG"}
