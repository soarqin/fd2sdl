"""B5掉落／特殊格的纯夹具和资源判据；不启动进程，不读取SDL常量。

field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D）、
field_special_cell_execute @0x190ac（原sub_190AC）、
field_completed_chest_tiles_apply @0x12263（原sub_12263）。
完整START是来源；每个派生byte列入manifest，不把受控状态当自然流程。
"""
import hashlib
import struct

from PIL import Image

from fd2_item_list_oracle import START_SHA256, PLAIN_SHA256
from fd2_magic_list_oracle import glyphs
from fd2_opening_oracle import fragments, portraits, raw_ui
from fd2_reward_oracle import common_tokens, derive_fixture as reward_fixture
from run_graphics_tests import decode_save
from run_original_comparison import archive_entry
from run_original_battle_menu import match_indexed
from run_original_end_turn import META, STRIDE, UNITS
from run_original_item_list import phase_end_model
from run_original_player import exact_tile

SCENARIOS = ("chest-empty", "chest-swap", "chest-list-cancel", "chest-no", "chest-full-no",
             "treasure-money", "loot-item", "loot-money", "loot-callback", "loot-story")
CELL_STATE = 0x30a3
CELL_ACTIONS = 0x53


def checksum(data):
    result = bytearray(data)
    result[-4:] = sum(result[:-4]).to_bytes(4, "little")
    return bytes(result)


def inventory_append(actor, item):
    actor = bytearray(actor)
    for slot in range(8):
        if actor[10 + slot * 2] & 0x80:
            actor[10 + slot * 2:12 + slot * 2] = bytes((0, item))
            return bytes(actor)
    raise ValueError("库存已满，不能把append当替换")


def inventory_swap(actor, slot, item):
    """1b8e7→1bb8c：整后缀memmove，末ID stale保留；不重算派生值。"""
    if len(actor) != STRIDE or not 0 <= slot < 8 or actor[10 + 2 * slot] & 0x80:
        raise ValueError("无效替换raw槽")
    result = bytearray(actor)
    result[10 + 2 * slot:24] = result[12 + 2 * slot:26]
    result[24] = 0x80
    return inventory_append(result, item)


def cell_contract(original, world):
    field = (original / "FDFIELD.DAT").read_bytes()
    map_data, metadata = archive_entry(field, 0), archive_entry(field, 1)
    width, height = struct.unpack_from("<HH", map_data)
    if (width, height) != (24, 24) or len(map_data) != 4 + width * height * 4:
        raise ValueError("B5特殊格需要完整已证stage0地图")
    x, y = world
    if not 0 <= x < width or not 0 <= y < height:
        raise ValueError("特殊格坐标越界")
    cell = struct.unpack_from("<I", map_data, 4 + (y * width + x) * 4)[0]
    shape = archive_entry((original / "FDSHAP.DAT").read_bytes(), metadata[0] * 2 + 1)
    tile = cell & 1023
    if tile * 4 + 4 > len(shape):
        raise ValueError("特殊格地形属性越界")
    flags, index = shape[tile * 4], (cell >> 16) & 31
    if not flags & 0x60 or index >= 16:
        raise ValueError("特殊格不在已确认metadata16项范围")
    mode, parameter = struct.unpack_from("<BH", metadata, CELL_ACTIONS + index * 3)
    return {"world": list(world), "cell_word": cell, "tile": tile,
            "terrain_flags": flags, "index": index, "mode": mode, "parameter": parameter}


def derive_fixture(original, scenario):
    if scenario not in SCENARIOS:
        raise ValueError("未知B5掉落场景")
    encrypted = (original / "START.SAV").read_bytes()
    base = decode_save(original / "START.SAV")
    if hashlib.sha256(encrypted).hexdigest() != START_SHA256 or hashlib.sha256(base).hexdigest() != PLAIN_SHA256:
        raise ValueError("B5夹具来源不是完整已确认START")
    physical = scenario.startswith("loot-")
    controlled = bytearray(reward_fixture(original, "physical-cap")[1] if physical else base)
    for raw in range(controlled[META + 1]):
        at = UNITS + raw * STRIDE
        if controlled[at + 6] != 2:
            controlled[at + 5] |= 4  # 仅隔离测量END的无关AI，不作自然行动声明。
    cell = None
    if physical:
        mode, parameter = {"loot-item": (0, 192), "loot-money": (1, 1000),
                           "loot-callback": (2, 4), "loot-story": (3, 8)}[scenario]
        struct.pack_into("<BH", controlled, UNITS + 11 * STRIDE + 0x31, mode, parameter)
        if scenario == "loot-story":
            controlled[UNITS + 11 * STRIDE + 7:UNITS + 11 * STRIDE + 9] = b"\x61\x61"
        if scenario == "loot-callback":
            # callback34738直接访问raw13。明确复制真实START记录，不猜构造器默认。
            controlled[META + 1] = 14
            for raw, position, hidden, source_raw, identity in ((12, (3, 3), True, 0, 1), (13, (3, 5), False, 3, 3)):
                actor = bytearray(base[UNITS + source_raw * STRIDE:UNITS + (source_raw + 1) * STRIDE])
                actor[:2], actor[5] = bytes(position), 5 if hidden else 4
                actor[6], actor[7], actor[8] = 0, identity, identity
                actor[0x31:0x34] = b"\xff\0\0"
                controlled[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE] = actor
    else:
        world = (0, 10) if scenario == "treasure-money" else (7, 13)
        cell = cell_contract(original, world)
        if (cell["index"], cell["mode"], cell["parameter"], cell["terrain_flags"] & 0x60) != (
                (2, 1, 5000, 0x40) if scenario == "treasure-money" else (0, 0, 192, 0x20)):
            raise ValueError("真实stage0箱／财宝数据与合同不同")
        controlled[UNITS:UNITS + 2] = bytes(world)
        if scenario in ("chest-swap", "chest-list-cancel", "chest-full-no"):
            controlled[UNITS + 10:UNITS + 26] = bytes((0x40, 0, 0x40, 132, 0, 192, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5))
    controlled = checksum(controlled)
    encoded = bytes(old ^ before ^ after for old, before, after in zip(encrypted, base, controlled))
    patches = lambda a, b: [{"offset": i, "before": x, "after": y} for i, (x, y) in enumerate(zip(a, b)) if x != y]
    manifest = {"scenario": scenario, "controlled_fixture": True, "natural_history_claimed": False,
        "source_sha256": START_SHA256, "source_plain_sha256": PLAIN_SHA256,
        "derived_sha256": hashlib.sha256(encoded).hexdigest(), "derived_plain_sha256": hashlib.sha256(controlled).hexdigest(),
        "plaintext_patches": patches(base, controlled), "encrypted_patches": patches(encrypted, encoded),
        "cell": cell, "physical_target_raw": 11 if physical else None,
        "extra_records": {"12": {"copied_start_raw": 0, "identity_override": 1, "hidden": True}, "13": {"copied_start_raw": 3, "identity_override": 3, "hidden": False}} if scenario == "loot-callback" else {},
        "nonplayer_ai_ineligible_bit": 4, "roster_manual_unknown_regions_preserved": True}
    allowed = set(range(UNITS, UNITS + controlled[META + 1] * STRIDE)) | {META + 1} | set(range(len(base) - 4, len(base)))
    if any(change["offset"] not in allowed for change in manifest["plaintext_patches"]):
        raise AssertionError("B5派生夹具触及未声明容器区域")
    return encoded, controlled, manifest


def expected_state(source, scenario, items):
    """只预测已声明确定性写集；native未知heap尾不作为视觉或构造器默认。"""
    state = bytearray(source)
    actor = bytes(state[UNITS:UNITS + STRIDE])
    if scenario in ("chest-empty", "loot-item"):
        state[UNITS:UNITS + STRIDE] = inventory_append(actor, 192)
    if scenario == "chest-swap":
        state[UNITS:UNITS + STRIDE] = inventory_swap(actor, 0, 192)
        struct.pack_into("<H", state, CELL_ACTIONS + 1, actor[11])
    if scenario in ("chest-empty", "treasure-money"):
        state[CELL_STATE + (2 if scenario == "treasure-money" else 0)] = 1
    if scenario in ("treasure-money", "loot-money"):
        old = struct.unpack_from("<I", state, META + 10)[0]
        struct.pack_into("<I", state, META + 10, (old + (5000 if scenario == "treasure-money" else 1000)) & 0xffffffff)
    state[UNITS + 5] |= 0x80
    if scenario.startswith("loot-"):
        enemy = UNITS + 11 * STRIDE
        struct.pack_into("<H", state, enemy + 0x40, 0)
        state[enemy + 5], state[enemy + 3], state[UNITS + 0x3c] = 1, 0, 99
        for raw in range(source[META + 1]):
            state[UNITS + raw * STRIDE + 3:UNITS + raw * STRIDE + 5] = b"\0\0"
        if scenario == "loot-callback":
            state[UNITS + 13 * STRIDE + 6] = 1
    immediate = checksum(state)
    final = phase_end_model(immediate, items)
    return {"immediate": immediate, "final": final,
            "inventory_hex": immediate[UNITS + 10:UNITS + 26].hex(),
            "completed": list(immediate[CELL_STATE:CELL_STATE + 32]),
            "resource": struct.unpack_from("<I", immediate, META + 10)[0],
            "stat_recompute_claimed": False}


def message_pages(original, assets, steps):
    """190ac/1aa1d的99,127/146。文字、原DATO、箭头/按钮分别资源比对。"""
    other = (original / "FDOTHER.DAT").read_bytes()
    text = archive_entry((original / "FDTXT.DAT").read_bytes(), 0)
    font, sheet = archive_entry(other, 4), archive_entry(other, 5)
    canvas = Image.new("P", (320, 200), 0); canvas.putpalette(assets["palette"])
    arrows = [raw_ui(sheet, i) for i in (18, 19)]
    for image in arrows: image.putpalette(assets["palette"])
    interior = raw_ui(sheet, 13)
    if interior.size != (16, 16): raise ValueError("掉落底框填充资源不是16x16")
    result = []
    for step in steps:
        if step.get("open", False): canvas.paste(0, (0, 0, 320, 200))
        x, y, origin = 99, step.get("y", 127), 99
        def expand(fragment, depth=0):
            if depth > 2: raise ValueError("掉落动态文本嵌套过深")
            out = []
            for token in common_tokens(text, fragment):
                if token == -1: break
                if token == -4: out += expand(181 + step["item"], depth + 1)
                elif token == -5: out += expand(181 + step["old"], depth + 1)
                elif token == -6: out += [int(c) for c in str(step["value"])]
                elif token == -2 or 0 <= token < len(font) // 32: out.append(token)
                else: raise ValueError(f"未确认掉落文本control {token}")
            return out
        for token in expand(step["fragment"]):
            if token == -2:
                x, y = origin, y + 19
                continue
            if x + 16 > 320 or y + 17 > 200: raise ValueError("掉落文字超出VGA")
            canvas.paste(74, (x, y, x + 16, y + 16))
            glyphs(canvas, font, [token], x, y, 205); x += 16
        expected = canvas.copy()
        if step.get("choice"): expected.paste(0, (232, 168, 288, 184))
        if sum(p != 0 for p in expected.getdata()) < 100: raise ValueError("掉落文字门为空")
        no_arrow = Image.new("P", (14, 10)); no_arrow.putpalette(assets["palette"])
        for py in range(10):
            for px in range(14):
                x, y = 191 + px, 181 + py
                known = canvas.getpixel((x, y))
                no_arrow.putpixel((px, py), known or interior.getpixel(((x - 8) % 16, (y - 115) % 16)))
        result.append({**step, "wait_key_mode": 0, "text": expected,
            "portraits": portraits(original, 0, assets["palette"], True),
            "arrows": arrows, "no_arrow_background": no_arrow})
    return result


def message_match(frame, page, assets, selected=0):
    text = match_indexed(frame, page["text"], (0, 0))
    border = match_indexed(frame, assets["dialog_top"], (5, 112))
    portrait = next((dict(frame=i * 3, **found) for i, image in enumerate(page["portraits"])
                    if (found := exact_tile(frame, image, (8, 115)))), None)
    if not text["ok"] or not border["ok"] or portrait is None: return None
    detail = {"kind": "loot_message", "fragment": page["fragment"], "text": text, "portrait": portrait,
              "dialog_border": border, "requires_key": bool(page.get("key")), "choice": bool(page.get("choice"))}
    if page.get("choice"):
        # 两种相位：选项高亮闪烁时也可暂呈普通图，但非选项不得反向高亮。
        pairs = ((48, 49) if selected == 0 else (48,), (51, 52) if selected == 1 else (51,))
        buttons = []
        for indices, at in zip(pairs, ((232, 168), (264, 168))):
            found = next((dict(frame=i, **hit) for i in indices if (hit := match_indexed(frame, assets["commands"][i], at))["ok"]), None)
            if found is None: return None
            buttons.append(found)
        # 只普通/普通不能证明Right真正改变owner；至少目标高亮一帧。
        if buttons[selected]["frame"] != (49 if selected == 0 else 52): return None
        detail.update(kind="loot_choice", selected=selected, buttons=buttons)
    if page.get("key"):
        # 190ac/1aa1d均dialog_wait_key(0)，不能把外部等键误认文字-3。
        # 仍严格核对原填充资源+累计文字覆盖的完整140px，不用删ROI掩盖多画箭头。
        unchanged = exact_tile(frame, page["no_arrow_background"], (191, 181))
        if unchanged is None: return None
        detail.update(wait_key_mode=0, no_arrow_background=unchanged)
    return detail


def story_pages(original, source, assets, fragment):
    """复用已发布opening token/字模/portrait primitive，只构当前story片段。

    不能先构0..fragment的其它剧情：受控active14没有那些剧情的raw15/16。
    只收原1aa1d/34738所到7、8中已确认的普通字、换行和等键控制。
    """
    if fragment not in (7, 8): raise ValueError("loot工具只确认stage0 story7/8")
    text_data = (original / "FDTXT.DAT").read_bytes()
    tokens = dict(fragments(text_data, 0, fragment + 1))[fragment]
    other = (original / "FDOTHER.DAT").read_bytes()
    font, sheet = archive_entry(other, 4), archive_entry(other, 5)
    arrows = [raw_ui(sheet, i) for i in (18, 19)]
    for image in arrows: image.putpalette(assets["palette"])
    canvas = Image.new("P", (320, 200), 0); canvas.putpalette(assets["palette"])
    result, speaker = [], None
    x = y = ox = oy = row = 0
    def pause(reason, offset, continuation=False):
        expected = canvas.copy()
        if continuation: expected.paste(0, (ox + 96, oy + 57, ox + 112, oy + 73))
        control, argument = speaker
        candidates = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE]
            for i in range(source[META + 1]) if source[UNITS + i * STRIDE + 8] == argument]
        if not candidates: raise ValueError("story说话人没有显式active记录")
        actor = next((actor for actor in candidates if not actor[5] & 1), candidates[-1])
        bottom = control == -18
        result.append({"stage": 0, "fragment": fragment, "token_offset": offset, "reason": reason,
            "speaker_control": control, "speaker_argument": argument,
            "portrait_id": actor[7], "portrait_position": (8, 115) if bottom else (232, 5),
            "portraits": portraits(original, actor[7], assets["palette"], bottom),
            "border_position": (5, 112) if bottom else (5, 2), "text": expected,
            "known_text_pixels": sum(p != 0 for p in expected.getdata()),
            "arrow_position": (ox + 96, oy + 62) if continuation else None, "arrows": arrows})
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token in (-17, -18):
            if speaker is not None: pause("speaker change", index)
            if index + 1 >= len(tokens): raise ValueError("story缺portrait参数")
            speaker = token, tokens[index + 1]
            ox, oy = (15, 9) if token == -17 else (95, 119)
            x, y, row = ox, oy, 0
            canvas.paste(0, (0, 0, 320, 200)); index += 1
        elif token in (-2, -3):
            if row == 3:
                for delta in (3, 3, 3, 3, 3, 4):
                    canvas.paste(canvas.crop((ox - 1, oy + delta, ox + 207, oy + delta + 72)), (ox - 1, oy))
                    canvas.paste(74, (ox, oy + 72, ox + 208, oy + 73))
                row -= 1
            row += 1; x, y = ox, oy + row * 19
            if token == -3:
                pause("continuation -3", index, True)
                canvas.paste(0, (ox + 96, oy + 57, ox + 112, oy + 73))
        elif token == -1:
            pause("fragment end", index)
            if index != len(tokens) - 1: raise ValueError("story结束后仍有token")
        elif 0 <= token < len(font) // 32:
            canvas.paste(74, (x, y, x + 16, y + 16))
            glyphs(canvas, font, [token], x, y, 205); x += 16
        else: raise ValueError("未确认loot story控制")
        index += 1
    if not result: raise ValueError("story无资源等键门")
    return result
