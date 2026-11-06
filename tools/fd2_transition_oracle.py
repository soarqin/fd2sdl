"""B5准备页／酒店的原版资源及整容器模型，不运行窗口或SDL代码。

依据field_town_prepare_execute @0x26152（原sub_26152）、field_town_view_render
@0x265ec（原sub_265EC）、field_hotel_menu_execute @0x29300（原field_manual_slot_dispatch）、
field_manual_slot_rows_draw @0x29ab2（原sub_29AB2）及field_manual_slot_picker @0x29bcb（原登记同名）。
受控五人夹具直接拼接原档完整记录，不能当自然首关／literal unit1证据。
"""
import hashlib
import struct

from PIL import Image

from audit_fdicon_opaque_zero import audit_fdicon
from fd2_item_list_oracle import EXE_SHA256, START_SHA256, raw_icon
from fd2_magic_list_oracle import glyphs
from fd2_reward_oracle import common_tokens
from fd2_opening_oracle import build_pages
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image
from dump_fd2_fixup_table import collect_internal_offset_fixups
from run_original_comparison import archive_entry, decode_image
from run_original_end_turn import META, STRIDE, decode_lmi
from run_original_player import sheet_frame, exact_tile
from run_original_battle_menu import match_indexed
from run_graphics_tests import decode_save

FILE_SIZE = 0x59cb
SLOT_BASE, SLOT_STRIDE, ROSTER_SIZE = 0x312b, 0xa28, 0xa00
LATE_SAVE_SHA256 = "2efc678871b8b019023f7a22758f6afd78a2aab2ad352a6a4a22a4c7ab15d2eb"


def _checksum(data):
    if len(data) != FILE_SIZE:
        raise ValueError("B5完整存档长度非法")
    result = bytearray(data)
    result[-4:] = sum(result[:-4]).to_bytes(4, "little")
    return bytes(result)


def write_manual_slot(source, state, index):
    """2968d只写整0xa00和meta0..9；slot3的checksum交叠另统一计算。"""
    if (len(source) != FILE_SIZE or
            sum(source[:-4]) != int.from_bytes(source[-4:], "little") or
            not 0 <= index < 4 or state["stage"] != 1 or
            not 0 < state["count"] <= 32 or len(state["roster"]) != ROSTER_SIZE or
            len(state["options"]) != 4 or not 0 <= state["resource"] <= 0xffffffff):
        raise ValueError("B5手工slot模型参数非法或尚未支持该stage")
    changed = bytearray(source)
    start = SLOT_BASE + SLOT_STRIDE * index
    changed[start:start + ROSTER_SIZE] = state["roster"]
    changed[start + ROSTER_SIZE:start + ROSTER_SIZE + 10] = (
        bytes((state["stage"], state["count"])) +
        struct.pack("<I", state["resource"]) + bytes(state["options"]))
    return _checksum(changed)


def derive_prepare_fixture(original):
    """START四人＋原FD2 roster raw5的晋升角色1(unit51)，全部80B保留。"""
    start_path, late_path = original / "START.SAV", original / "FD2.SAV"
    encrypted, late_encrypted = start_path.read_bytes(), late_path.read_bytes()
    if (hashlib.sha256(encrypted).hexdigest() != START_SHA256 or
            hashlib.sha256(late_encrypted).hexdigest() != LATE_SAVE_SHA256):
        raise ValueError("受控五人准备需已确认的完整START及原FD2存档，不能用ctor补记录")
    source, late = decode_save(start_path), decode_save(late_path)
    if source[META + 9] != 4 or late[META + 9] != 31:
        raise ValueError("原版独立roster条数与受控来源不符")
    roster = bytearray(source[0x8a3:0x12a3])
    if [(roster[i * STRIDE + 7], roster[i * STRIDE + 8]) for i in range(4)] != [(0, 0), (9, 9), (4, 4), (30, 30)]:
        raise ValueError("START四人完整身份与来源不符")
    extra_start = 0x8a3 + 5 * STRIDE
    extra = late[extra_start:extra_start + STRIDE]
    if (extra[7], extra[8], extra[0x21], extra[5]) != (51, 1, 27, 0):
        raise ValueError("补充记录不是原档角色1的已确认晋升形态")
    roster[4 * STRIDE:5 * STRIDE] = extra
    state = {"stage": 1, "count": 5, "roster": bytes(roster),
             "resource": struct.unpack_from("<I", source, META + 10)[0],
             "options": bytes(source[META + i] for i in (15, 14, 16, 17))}
    changed = write_manual_slot(source, state, 0)
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, changed))
    patches = lambda a, b: [{"offset": i, "before": x, "after": y}
                            for i, (x, y) in enumerate(zip(a, b)) if x != y]
    manifest = {"scenario": "prepare-controlled", "controlled_fixture": True,
        "natural_stage0_completion": False, "literal_unit1_created": False,
        "source_sha256": START_SHA256, "supplement_sha256": LATE_SAVE_SHA256,
        "supplement_span": [extra_start, extra_start + STRIDE],
        "supplement_identity": {"unit_id": 51, "text_id": 1, "level": 27},
        "source_records": "START roster0..3 and FD2 roster5, each complete80B; no SDL ctor",
        "source_plain_sha256": hashlib.sha256(source).hexdigest(),
        "derived_sha256": hashlib.sha256(encoded).hexdigest(),
        "derived_plain_sha256": hashlib.sha256(changed).hexdigest(),
        "plaintext_patches": patches(source, changed), "encrypted_patches": patches(encrypted, encoded),
        "scope": "manual slot0 full roster/meta0..9 and checksum only; active/other slots untouched"}
    return encoded, changed, state, manifest


def _sheet_member(sheet, index):
    if len(sheet) < 6 or sheet[:4] != b"LMI1":
        raise ValueError("酒店LMI目录非法")
    count = struct.unpack_from("<H", sheet, 4)[0]
    if not 0 <= index < count or 6 + 4 * count > len(sheet):
        raise ValueError("酒店资源帧越界")
    start = struct.unpack_from("<I", sheet, 6 + 4 * index)[0]
    end = struct.unpack_from("<I", sheet, 6 + 4 * (index + 1))[0] if index + 1 < count else len(sheet)
    if not 6 + 4 * count <= start < end <= len(sheet):
        raise ValueError("酒店资源span非法")
    return sheet[start:end]


def _text(canvas, font, tokens, x, y, foreground, *, value=None, background=None):
    for token in tokens:
        if token == -1:
            return
        expanded = [int(digit) for digit in str(value)] if token == -6 and value is not None else [token]
        if any(g < 0 or (g + 1) * 32 > len(font) for g in expanded):
            raise ValueError("准备页字段含未确认control或坏字模")
        for identity in expanded:
            if x < 1 or y < 0 or x + 16 > canvas.width or y + 17 > canvas.height:
                raise ValueError("准备页文字越界")
            if background is not None:
                canvas.paste(background, (x, y, x + 16, y + 16))
            glyphs(canvas, font, [identity], x, y, foreground)
            x += 16
    raise ValueError("准备页字段缺结束token")


def resources(original):
    executable = (original / "FD2.EXE").read_bytes()
    if hashlib.sha256(executable).hexdigest() != EXE_SHA256:
        raise ValueError("准备页原始EXE身份不符")
    le = parse_le(executable)
    raw = build_relbase_image(executable, le, parse_objects(executable, le))
    if bytes(raw[0x4e809:0x4e821]) != bytes.fromhex("558bec8b450848ba1f000000f7e28d158d23000003c25dc3"):
        raise ValueError("stage-1 getter原码不符")
    other = (original / "FDOTHER.DAT").read_bytes()
    palette6 = archive_entry(other, 0)
    if len(palette6) != 768 or max(palette6) > 63:
        raise ValueError("FDOTHER0调色板非法")
    palette = [v << 2 for v in palette6]
    hotel = archive_entry(other, 13)
    background = decode_image(archive_entry(other, 11))
    panel = decode_lmi(archive_entry(other, 10))
    hotel_background = decode_image(_sheet_member(hotel, 0))
    picker = decode_lmi(_sheet_member(hotel, 16))
    if (background.size, panel.size, hotel_background.size, picker.size) != ((320, 200), (62, 26), (320, 200), (310, 86)):
        raise ValueError("准备页原资源尺寸改变")
    for image in (background, panel, hotel_background, picker):
        image.putpalette(palette)
        if any(224 <= v <= 239 for v in image.getdata()):
            raise ValueError("静态准备页资源新增水DAC索引，必须重核而非忽略")
    commands = [raw_icon(hotel, i, (24, 20), palette) for i in range(3, 11)]
    return {"palette": palette, "background": background, "panel": panel,
        "hotel_background": hotel_background, "picker": picker, "commands": commands,
        "text": archive_entry((original / "FDTXT.DAT").read_bytes(), 0),
        "font": archive_entry(other, 4), "town_xy": list(zip(raw[0x52363:0x52368], raw[0x52375:0x5237a])),
        "stage_row": bytes(raw[0x6238d:0x623ac])}


def picker_image(assets, plain, selected):
    if len(plain) != FILE_SIZE or not 0 <= selected < 4:
        raise ValueError("四槽绘制参数非法")
    canvas = assets["picker"].copy()
    for index in range(4):
        foreground = 201 if selected == index else 205
        y = 7 + index * 19
        _text(canvas, assets["font"], common_tokens(assets["text"], 549), 5, y, foreground, value=index + 1)
        stage = plain[SLOT_BASE + index * SLOT_STRIDE + ROSTER_SIZE]
        if stage == 255:
            _text(canvas, assets["font"], common_tokens(assets["text"], 514), 83, y, foreground)
        elif 0 <= stage <= 29:
            # START原档其它两槽是stage6，不能为了校准slot0把它们清空。
            # 29ab2按stage直接索引公共标签；这里预检每个实际字模/终止符。
            _text(canvas, assets["font"], common_tokens(assets["text"], 514 + stage), 35, y, foreground)
            _text(canvas, assets["font"], common_tokens(assets["text"], 550 + stage), 125, y, foreground)
        else:
            raise ValueError("四槽stage不在已确认0..29标签或空槽范围")
    return canvas


def town_images(original, assets, state, selected):
    if state["stage"] != 1 or not 0 <= selected < 5:
        raise ValueError("本阶段town oracle仅支持stage1五项")
    icons = (original / "FDICON.B24").read_bytes()
    if audit_fdicon(icons)["opaque_zero_pixels"]:
        raise ValueError("FDICON出现opaque0，不能用非零mask")
    identity = state["roster"][7]
    views = []
    for phase in (0, 1, 2):
        view = assets["background"].crop((0, 0, 312, 192))
        view.putpalette(assets["palette"])
        view.paste(assets["panel"], (244, 162))
        _text(view, assets["font"], common_tokens(assets["text"], 495 + selected), 252, 168, 205, background=74)
        marker = sheet_frame(icons, identity * 12 + phase)
        view.paste(marker, assets["town_xy"][selected], marker.point(lambda v: 255 if v else 0, mode="L"))
        views.append(view)
    return views


def picker_match(frame, expected):
    found = exact_tile(frame, expected, (5, 112))
    return {"kind": "manual_slot_picker", "position": [5, 112], "size": [310, 86],
            "checked_pixels": 26660, "opaque_black_included": True, **found} if found else None


def hotel_menu_match(frame, assets, selected):
    if not 0 <= selected < 4:
        raise ValueError("酒店菜单索引非法")
    matches = [match_indexed(frame, assets["commands"][2 * i + (i == selected)], (201 + 26 * i, 169)) for i in range(4)]
    background = exact_tile(frame, assets["hotel_background"].crop((220, 8, 316, 88)), (220, 8))
    return {"kind": "hotel_menu", "selected": selected, "matches": matches,
            "background_roi": [220, 8, 96, 80]} if background and all(row["ok"] for row in matches) else None


def notice_resources(original, assets, fragment, *, portrait=129, origin=(12, 119)):
    canvas = Image.new("P", (320, 200), 0)
    canvas.putpalette(assets["palette"])
    _text(canvas, assets["font"], common_tokens(assets["text"], fragment), *origin, 205, background=74)
    entry = archive_entry((original / "DATO.DAT").read_bytes(), portrait)
    images = []
    for index in (0, 3):
        start = struct.unpack_from("<I", entry, 4 * index)[0]
        end = struct.unpack_from("<I", entry, 4 * (index + 1))[0] if index < 3 else len(entry)
        image = decode_lmi(entry[start:end])
        if image.size != (80, 80): raise ValueError("B5 notice portrait尺寸改变")
        if portrait == 75:
            from PIL import ImageOps
            image = ImageOps.mirror(image)
        image.putpalette(assets["palette"])
        images.append(image)
    return {"fragment": fragment, "text": canvas, "portraits": images,
            "portrait_position": (107, 5) if portrait == 129 else (8, 115),
            "portrait_id": portrait, "text_origin": list(origin)}


def notice_match(frame, notice, assets):
    text = match_indexed(frame, notice["text"], (0, 0))
    border = match_indexed(frame, assets["dialog_top"], (5, 112))
    portrait = next(({"frame": index * 3, **found} for index, image in enumerate(notice["portraits"])
        if (found := exact_tile(frame, image, notice["portrait_position"]))), None)
    return {"kind": "prepare_notice", "fragment": notice["fragment"], "text": text,
            "portrait": portrait, "border": border, "text_origin": notice["text_origin"]} if text["ok"] and border["ok"] and portrait else None


def entry_browse(frame, assets, expected):
    from run_original_end_turn import browse_anywhere
    found = browse_anywhere(frame, assets, leader=True)
    if not found or any(match_indexed(frame, assets["dialog_top"], at)["ok"] for at in ((5, 2), (5, 112))):
        return None
    screen = found["focus_screen"]
    world = expected["first_focus_world"]
    camera = [world[i] - (screen[i] - 4) // 24 for i in range(2)]
    if not 0 <= camera[0] <= 14 or not 0 <= camera[1] <= 13:
        return None
    return dict(found, kind="stage1_first_browse", inferred_camera=camera, leader_world=world,
                geometry_source="strict LE script9..12 + original stage1 placement")


def entry_initialized_records(original, state, geometry):
    """1088d/10c50确切写集；新NPC malloc洞不伪造为0或源档值。"""
    from fd2_item_list_oracle import item_table
    from run_original_item_list import drop_derived
    data = (original / "FD2.EXE").read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256: raise ValueError("entry基础表EXE不符")
    le = parse_le(data); raw = build_relbase_image(data, le, parse_objects(data, le))
    table, _ = item_table(original / "FD2.EXE")
    metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 4)
    templates = [metadata[131 + 26 * i:131 + 26 * (i + 1)] for i in range(40)
                 if metadata[131 + 26 * i + 21] in (1, 2)]
    result = []
    for index, actor in enumerate(geometry["actors"]):
        unit = bytearray(state["roster"][index * 80:(index + 1) * 80]) if index < 5 else bytearray(80)
        known = bytearray([1] * 80)
        known[2] = 0  # SDL按unit_id索引；native为本轮FDICON cache class。
        unit[:2] = bytes(actor["position"]); unit[3:5] = b"\0\0"
        if index < 5:
            unit[6] = 2; unit[0x31] = 255; unit[0x22:0x28] = bytes(6)
        else:
            row = templates[index - 5]; identity, level = row[1], row[4]
            if not 68 <= identity <= 135: raise ValueError("本entry只已核敌军基础表构造NPC")
            base = raw[0x61af9 + 10 * (identity - 68):0x61af9 + 10 * (identity - 67)]
            unit[5:10] = bytes((0, row[0], identity, identity, 0))
            if row[5] == 255:
                unit[0x0a:0x0d] = bytes((0x40, row[6], 0x80)); known[0x0d] = 0
            else: unit[0x0a:0x0e] = bytes((0x40, row[5], 0x40, row[6]))
            for slot in range(6): unit[0x0e + 2 * slot:0x10 + 2 * slot] = bytes((0x80 if row[7 + slot] == 255 else 0, row[7 + slot]))
            unit[0x1a:0x1f] = row[13:17] + bytes(1)
            unit[0x1f:0x22] = bytes((base[0], base[1], level))
            known[0x28:0x31] = bytes(9)
            unit[0x31:0x34] = row[22:25]; unit[0x34:0x37] = row[17:20]
            struct.pack_into("<H", unit, 0x37, base[5] * level)
            struct.pack_into("<H", unit, 0x39, base[6] * level)
            unit[0x3b:0x3e] = bytes((base[8], 0 if row[0] == 2 else 255, row[2]))
            struct.pack_into("<H", unit, 0x3e, base[7] * level)
            hp, mp = struct.unpack_from("<H", base, 2)[0] * level, base[4] * level
            struct.pack_into("<4H", unit, 0x40, hp, hp, mp, mp)
        struct.pack_into("<4H", unit, 0x48, *drop_derived(unit, table))
        result.append((bytes(unit), bytes(known)))
    return result


def check_entry_measurement(measured, before, state, expected, first, metadata_prefix, initialized):
    from run_original_end_turn import UNITS
    if (measured[META:META + 3] != bytes((1, 21, 1)) or measured[META + 9] != 5 or
            measured[0] != 1 or measured[0x30a3:0x30c3] != bytes(32)):
        raise AssertionError("stage1回合/条数/shape/特殊格初态不同")
    if measured[0x8a3:0x12a3] != state["roster"]:
        raise AssertionError("entry改写了完整persistent表或未用尾")
    if measured[SLOT_BASE:-4] != before[SLOT_BASE:-4]:
        raise AssertionError("活动Save改写了四槽或未知metadata")
    if len(metadata_prefix) != 1171 or measured[:1171] != metadata_prefix:
        raise AssertionError("stage1已用1171B metadata不等于原FDFIELD[4]")
    if measured[UNITS + 21 * STRIDE:0x30a3] != before[UNITS + 21 * STRIDE:0x30a3]:
        raise AssertionError("活动Save改写21个actor以外未用尾")
    if measured[META + 18:SLOT_BASE] != before[META + 18:SLOT_BASE]:
        raise AssertionError("活动Save改写未知meta尾")
    actors = []
    for expected_actor in expected["actors"]:
        offset = UNITS + expected_actor["raw_actor"] * STRIDE
        record = measured[offset:offset + STRIDE]
        actual = {"raw_actor": expected_actor["raw_actor"], "unit_id": record[7], "text_id": record[8],
            "side": record[6], "position": list(record[:2]), "direction": record[3],
            "hidden": bool(record[5] & 1), "acted": bool(record[5] & 128)}
        if actual != expected_actor:
            raise AssertionError(f"stage1 actor{actual['raw_actor']}身份/几何/方向不同")
        wanted, mask = initialized[actual["raw_actor"]]
        if any(flag and a != b for a, b, flag in zip(record, wanted, mask)):
            raise AssertionError(f"stage1 actor{actual['raw_actor']}已初始化byte不同")
        actors.append(actual)
    camera = first["inferred_camera"]
    focus = [expected["first_focus_world"][0] - 1, expected["first_focus_world"][1]]
    if (list(measured[META + 3:META + 5]) != camera or list(measured[META + 5:META + 7]) != focus or
            list(measured[META + 7:META + 9]) != [focus[i] - camera[i] for i in range(2)]):
        raise AssertionError("Save与WGC推导镜头/显式LEFT测量焦点不一致")
    if (struct.unpack_from("<I", measured, META + 10)[0] != state["resource"] or
            bytes(measured[META + i] for i in (15, 14, 16, 17)) != state["options"]):
        raise AssertionError("entry改变资源/手工配置")
    return {"stage": 1, "turn": 1, "active_count": 21, "actors": actors,
            "camera": camera, "first_focus": expected["first_focus_world"], "measuring_focus": focus,
            "complete_persistent_roster_equal": True, "manual_slots_and_unused_active_meta_tails_preserved": True,
            "initialized_actor_bytes_checked": [sum(mask) for _, mask in initialized],
            "metadata_prefix_sha256": hashlib.sha256(metadata_prefix).hexdigest(),
            "native_metadata_heap_tail": {"span": [1171, 2211], "compared": False,
                "changed_from_previous_file": sum(a != b for a, b in zip(measured[1171:2211], before[1171:2211])),
                "reason": "19df7 copies all2211B; new1171B resource has no confirmed bytes beyond its allocation"},
            "excluded": "metadata heap tail1171..2210; native cache_index; newly allocated NPC constructor holes; full FIGANI/audio/RNG timing"}


def entry_pages(original, assets, state):
    """32d18的FDTXT[2]四片段，-18按text_id而非单位图号解析。"""
    metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 4)
    if metadata[:3] != bytes((1, 5, 40)) or len(metadata) != 1171:
        raise ValueError("stage1模板/五个persistent槽布局不符")
    records = [state["roster"][i * STRIDE:(i + 1) * STRIDE] for i in range(state["count"])]
    if len(records) != 5:
        raise ValueError("受控entry资源门需要五份完整来源record")
    templates = [metadata[0x83 + 26 * i:0x83 + 26 * (i + 1)] for i in range(40)]
    identities = [row[7] for row in records] + [row[1] for row in templates if row[21] == 1] + [row[1] for row in templates if row[21] == 2]
    if len(identities) != 21:
        raise ValueError("stage1 group1/2追加后不是21个raw actor")
    mapping = {row[8]: row[7] for row in records}
    return build_pages(original, assets, stage_plan=((1, 4),),
        identities_by_stage={1: identities}, text_identities_by_stage={1: mapping})


def entry_geometry(original, state):
    """原placement+9..12脚本的身份/站位，拒绝未建模的占格冲突。"""
    executable = original / "FD2.EXE"
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("stage1几何需要已确认FD2.EXE")
    le = parse_le(data)
    raw = build_relbase_image(data, le, parse_objects(data, le))
    fixups = collect_internal_offset_fixups(executable)
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata, placement = archive_entry(field, 4), archive_entry(field, 5)
    if (metadata[:3] != bytes((1, 5, 40)) or
            struct.unpack_from("<H", placement)[0] != 45 or len(placement) != 272):
        raise ValueError("stage1 metadata/placement严格前置不符")
    points = [list(struct.unpack_from("<HH", placement, 2 + 6 * i)) for i in range(45)]
    actors = [{"raw_actor": i, "unit_id": state["roster"][i * STRIDE + 7],
               "text_id": state["roster"][i * STRIDE + 8], "side": 2,
               "position": points[40 + i].copy(), "direction": 0,
               "hidden": bool(state["roster"][i * STRIDE + 5] & 1), "acted": False} for i in range(5)]
    scripts = []
    def append(group):
        for index in range(40):
            row = metadata[0x83 + 26 * index:0x83 + 26 * (index + 1)]
            if row[21] != group:
                continue
            point = points[index].copy()
            # group1为nearest-free，真实本夹具目标格未被占；distance0唯一，
            # 不另造通用placement算法。group2原53AFA=1强制放置。
            if group == 1 and any(a["position"] == point and not a["hidden"] for a in actors):
                raise ValueError("受控stage1 group1发生占格，需按原nearest-free补合同")
            actors.append({"raw_actor": len(actors), "unit_id": row[1], "text_id": row[1],
                "side": row[0], "position": point, "direction": 0, "hidden": False, "acted": False})
    def movement(identity):
        obj, offset, address = fixups[0x627d8 + identity * 4]
        if obj != 3:
            raise ValueError("stage1 movement pointer非object3")
        cursor = address + 1
        for _ in range(raw[address]):
            mode, count = raw[cursor:cursor + 2]
            cursor += 2
            pairs = raw[cursor:cursor + 2 * count]
            if len(pairs) != 2 * count:
                raise ValueError("stage1 movement pair截断")
            cursor += len(pairs)
            for actor, direction in zip(pairs[::2], pairs[1::2]):
                if actor >= len(actors) or direction > 3:
                    raise ValueError("stage1 movement引用尚未存在actor")
                actors[actor]["direction"] = direction
                if mode < 128:
                    dx, dy = ((0, 1), (-1, 0), (0, -1), (1, 0))[direction]
                    actors[actor]["position"][0] += mode * dx
                    actors[actor]["position"][1] += mode * dy
        scripts.append({"id": identity, "canonical_va": address, "object_offset": offset,
                        "size": cursor - address, "sha256": hashlib.sha256(raw[address:cursor]).hexdigest()})
    movement(9)
    movement(10)
    append(1)
    movement(11)
    for actor in actors: actor["direction"] = 0
    append(2)
    movement(12)
    for actor in actors: actor["direction"] = 0
    return {"actors": actors, "active_count": len(actors), "stage": 1, "turn": 1,
            "roster_count": state["count"], "first_focus_world": actors[0]["position"].copy(),
            "scripts": scripts, "camera_source": "infer from actual native leader cursor; no SDL/captured default"}
