"""B5自然首关的资源门及只读规划协议，不推测原版RNG或改写SAV。

复用3231b/117e7/18890/18d8c/115b6已核入口；方向与相机沿12cea。
planner只提操作意图，真实光标、范围、菜单、文本及合法Save才是运行证据。
"""
import hashlib
import struct

from PIL import Image

from fd2_battle_animation_frames import rle_material
from fd2_movement_oracle import movement_profile
from fd2_opening_oracle import build_pages, geometry as opening_geometry
from fd2_reward_oracle import reward_pages, reward_tables
from run_original_battle_menu import MENU_OFFSETS, match_indexed
from run_original_comparison import TOLERANCE, archive_entry, decode_image
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle
from run_original_focus import native_focus_reference
from run_original_magic_list import actor_focus_resources
from run_original_player import acted_sprite, cursor_assets, exact_tile, screen, sheet_frame
from run_original_reward import actor_view

STAGE_DIMENSIONS = {0: (24, 24), 1: (27, 21), 2: (18, 26)}


def actor(source, raw):
    if not 0 <= raw < source[META + 1]: raise ValueError("自然动作raw超出实际snapshot")
    return source[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]


def view(source, world, camera=None):
    """只更新工具本地显示坐标；不编码、写入或回灌游戏存档。"""
    result = bytearray(source)
    if camera is not None: result[META + 3:META + 5] = bytes(camera)
    result[META + 5:META + 9] = bytes((*world, world[0] - result[META + 3], world[1] - result[META + 4]))
    return result


def validate_plan(plan, source, *, stage=0):
    if stage not in (0, 1):
        raise ValueError("自然规划仅已开放stage0/1；stage2初始活动不是整关支持")
    width, height = STAGE_DIMENSIONS[stage]
    if (len(source) != 0x59cb or sum(source[:-4]) != int.from_bytes(source[-4:], "little") or
            source[META + 2] != stage or not (11 if stage == 1 else 1) <= source[META + 1] <= 96 or
            not 1 <= source[META + 9] <= 32):
        raise ValueError("自然规划需要完整checksum有效且私有结果handler安全的实际stage活动Save")
    if (plan.get("planning_only") is not True or plan.get("original_acceptance") is not False or
            plan.get("query_side_effects") is not False or plan.get("source_turn") != source[META] or
            plan.get("source_active_count") != source[META + 1] or plan.get("source_roster_count") != source[META + 9]):
        raise ValueError("planner不是此实际Save的纯查询")
    ordered = "turn_actions" in plan
    if ordered:
        actions = plan["turn_actions"]
        if (plan.get("turn_actions_policy") != "hero_wait_then_effect" or
                "next_action" in plan or "rest_tail" in plan or not isinstance(actions, list) or
                not 2 <= len(actions) <= source[META + 1] or
                any(not isinstance(action, dict) for action in actions) or
                actions[0].get("kind") != "wait" or actions[0].get("actor_raw") != 0 or
                actions[0].get("destination") == list(actor(source, 0)[:2]) or
                "wait_cell_action" in actions[0] or
                actions[1].get("kind") not in ("attack", "magic", "herb")):
            raise ValueError("turn_actions仅开放主角普通撤离WAIT、一次效果和其余原地WAIT")
    else:
        if "turn_actions_policy" in plan:
            raise ValueError("turn_actions_policy不能脱离显式turn_actions")
        actions = [plan["next_action"]] + plan.get("rest_tail", [])
    seen = set()
    for index, action in enumerate(actions):
        if action["kind"] == "end_turn" and index == 0: continue
        raw = action["actor_raw"]
        record = actor(source, raw)
        if raw in seen or bytes.fromhex(action["actor_record_hex"]) != record:
            raise ValueError("planner重复actor或使用陈旧80B记录")
        seen.add(raw)
        if record[6] != 2 or record[5] & 0x81 or record[0x26] or not int.from_bytes(record[0x40:0x42], "little"):
            raise ValueError("planner尝试操控不可行动的实际玩家")
        destination = action["destination"]
        if len(destination) != 2 or any(type(x) is not int or not 0 <= x < limit for x, limit in zip(destination, (width, height))):
            raise ValueError("自然目的地超出此stage原地图")
        if ordered and index >= 2:
            if action["kind"] != "wait" or list(record[:2]) != destination:
                raise ValueError("turn_actions效果后的剩余actor必须原地WAIT")
        if (index and not ordered) or (ordered and index == 0):
            if action["kind"] != "wait":
                raise ValueError("REST-tail必须是同轮剩余actor的WAIT")
            if list(record[:2]) != destination:
                path = action.get("reachable_path_directions")
                if (raw != 0 or record[7:9] != b"\0\0" or
                        action.get("hero_safety_reposition") is not True or
                        not isinstance(path, list) or not path or
                        any(type(direction) is not int or direction not in range(4) for direction in path)):
                    raise ValueError("REST移动仅允许显式标记且有方向路径的真实主角WAIT")
                # 只验证query意图的几何，不将安全估计／路径当原版寻路或运行oracle。
                x, y = record[:2]
                for direction in path:
                    x += (0, -1, 0, 1)[direction]
                    y += (1, 0, -1, 0)[direction]
                    if not 0 <= x < width or not 0 <= y < height:
                        raise ValueError("REST主角query路径越出此stage边界")
                if [x, y] != destination:
                    raise ValueError("REST主角query路径终点不同于目的地")
        disabled = action["command_disabled"]
        if (len(disabled) != 4 or any(x not in (0, 1) for x in disabled) or disabled[3] or
                action["command_first_enabled"] != next(i for i, x in enumerate(disabled) if not x)):
            raise ValueError("planner COMMAND查询返回非法")
        if action["kind"] not in ("wait", "attack", "magic", "herb"):
            raise ValueError("自然driver没有该动作协议")
        if action["kind"] == "magic" and (action["magic_id"] not in (0, 13) or destination != list(record[:2])):
            raise ValueError("自然driver只操作已证0/13且零步施法")
        if action.get("target_raw", -1) >= 0:
            target = actor(source, action["target_raw"])
            if target != bytes.fromhex(action["target_record_hex"]) or target[5] & 1:
                raise ValueError("planner目标不是此实际snapshot的可见记录")
    return actions


def semantic_digest(source):
    """只用于有界停滞检测；排除turn/cache/frame/未知malloc字节。"""
    live = []
    for raw in range(source[META + 1]):
        item = actor(source, raw)
        if item[5] & 1: continue
        live.append(bytes((raw, item[0], item[1], item[5] & 1, item[6], item[7], item[8])) +
            item[10:40] + item[0x37:0x3d] + item[0x3e:0x50])
    return hashlib.sha256(b"".join(live) + source[0x30a3:0x30c3] + source[META + 9:META + 14]).hexdigest()


def visible_resource(template, position):
    """17898仅复制page到VGA(4,4)的312x192；资源不移动anchor。"""
    x,y=position;left,top=max(4,x),max(4,y)
    right,bottom=min(316,x+template.width),min(196,y+template.height)
    if left>=right or top>=bottom:return None
    crop=(left-x,top-y,right-x,bottom-y)
    return template.crop(crop),(left,top),crop


def visible_match(frame, template, position):
    visible=visible_resource(template,position)
    if visible is None:return {"ok":False,"checked_pixels":0,"visible_roi":None,"full_resource_compared":False}
    image,at,crop=visible
    return {**match_indexed(frame,image,at),"visible_roi":[*at,image.width,image.height],
        "resource_crop":list(crop),"full_resource_compared":crop==(0,0,template.width,template.height)}


def dialogs_absent(frame, assets):
    return not any(match_indexed(frame, assets["dialog_top"], at)["ok"] for at in ((5, 2), (5, 112)))


def menu_absent(frame, assets, at):
    # 任一实际可见方向都能否定closed；右下角不能仅因right/down都在视区外而通过。
    groups=((0,(0,7,8,12)),(1,(1,5,9,13)),(2,(2,6,10,14)),(3,(3,4,11,15)))
    for item,ids in groups:
        where = (at[0] + MENU_OFFSETS[item][0], at[1] + MENU_OFFSETS[item][1])
        for command in ids:
            for state in (0,1,2):
                image=assets["commands"][3*command+state]
                # 179d5末尾重画actor，固定盖top图标末6行；只用完整未盖的上14行作presence。
                if item==0:image=image.crop((0,0,image.width,14))
                if visible_match(frame,image,where)["ok"]:return False
    return True


def selection_summary(original, source, assets, raw, *, hp_observation=None):
    """field_unit_summary_draw @0x18c6d（原sub_18C6D）的完整149x42前景板。"""
    from fd2_item_list_oracle import number, signed_word, word_bar
    from fd2_magic_list_oracle import fragment, glyphs
    unit=actor(source,raw)
    other=(original/"FDOTHER.DAT").read_bytes();sheet=archive_entry(other,5);font=archive_entry(other,4)
    image=sheet_frame(sheet,22,lmi=True);image.putpalette(assets["palette"])
    if image.size!=(149,42) or 0 in image.getdata():raise ValueError("单位简表需完整不透明149x42原资源")
    background=image.copy()
    hp,maxhp,mp,maxmp=struct.unpack_from("<HHHH",unit,0x40)
    word_bar(image,sheet,hp,maxhp,21,22,23);word_bar(image,sheet,mp,maxmp,21,31,26)
    number(image,sheet,unit[0x21],132,4,31,2)
    number(image,sheet,signed_word(hp),126,21,31 if hp==maxhp else 42,3)
    number(image,sheet,signed_word(mp),126,30,31 if mp==maxmp else 42,3)
    names=fragment(archive_entry((original/"FDTXT.DAT").read_bytes(),0),unit[8]+1)
    glyphs(image,font,names,5,4,205)
    at=(9 if unit[0]-source[META+3]>=7 else 160,9)
    result={"raw_actor":raw,"position":at,"image":image,"fields":{"text_fragment":unit[8]+1,
        "level":unit[0x21],"HP_raw":hp,"HP_max_raw":maxhp,"MP_raw":mp,"MP_max_raw":maxmp,
        "source_record_compared":True,"HP_or_RNG_inferred":False}}
    if hp_observation is not None:
        if (hp_observation.get("raw")!=raw or hp_observation.get("record_hex")!=unit.hex() or
                hp_observation.get("actual_completed_heal") is not True or not 0<maxhp<=999):
            raise ValueError("动态HP简表只允许本轮实际完成治疗且身份未变的目标，最大HP须1..999")
        from fd2_magic_list_oracle import digit
        result["hp_observation"]=dict(hp_observation)
        # 完整不透明6x8字格含原底板；不只检查数字的非零笔画。
        result["hp_digits"]={}
        for bank in (31,42):
            result["hp_digits"][bank]=[]
            for index in range(3):
                cells=[]
                for value in range(10):
                    part=background.crop((126+6*index,21,132+6*index,29))
                    glyph=digit(sheet,bank+value)
                    part.paste(glyph,(0,0),glyph.point(lambda p:255 if p else 0,mode="L"))
                    cells.append(part)
                result["hp_digits"][bank].append(cells)
    return result


def observed_summary(frame, original, source, assets, summary):
    """治疗后只读取三位显示观察，再重建整6258；不更新任何80B来源状态。"""
    candidates=set();px,py=summary["position"];maximum=summary["fields"]["HP_max_raw"]
    for bank,columns in summary["hp_digits"].items():
        digits=[]
        for index,cells in enumerate(columns):
            hits=[value for value,cell in enumerate(cells) if match_indexed(frame,cell,(px+126+6*index,py+21))["ok"]]
            if len(hits)!=1:break
            digits.append(hits[0])
        if len(digits)==3:
            value=100*digits[0]+10*digits[1]+digits[2]
            if value<=maximum and (bank==31)==(value==maximum):candidates.add(value)
    if len(candidates)!=1:return None
    value=candidates.pop();local=bytearray(source)
    struct.pack_into("<H",local,UNITS+summary["raw_actor"]*STRIDE+0x40,value)
    result=selection_summary(original,local,assets,summary["raw_actor"])
    fields=result["fields"];fields.pop("HP_raw")
    fields.update(source_HP_raw=summary["fields"]["HP_raw"],observed_HP=value,
        source_HP_expected_compared=False,source_record_compared=False,other_source_fields_compared=True,
        observation_scope="display only; no Save/plan/HP effect or RNG inference",
        actual_completed_heal=dict(summary["hp_observation"]))
    return result


def actor_foreground_resources(original, source, assets, raw, cursor):
    """129ec/12ac6：静止actor本格、上格的原RLE前景，保持完整30行。

    无前景仍走旧资源门。这里不猜行走附加格、相邻actor遮挡或动态水色。
    RLE写点包括写入的颜色0，不能用非零颜色猜前景透明区。
    """
    unit = actor(source, raw)
    # field_unit_ignores_terrain_combat_modifier @0x1f183：28例外，19/4/5免遮挡。
    if unit[7] != 28 and (unit[32] == 19 or unit[31] in (4, 5)):
        return None
    stage = source[META + 2]
    key = (str(original.resolve()), stage)
    cache = assets.setdefault("_actor_foreground_maps", {})
    if key not in cache:
        field = (original / "FDFIELD.DAT").read_bytes()
        data = archive_entry(field, 3 * stage)
        metadata = archive_entry(field, 3 * stage + 1)
        shape = (original / "FDSHAP.DAT").read_bytes()
        cache[key] = (data, archive_entry(shape, 2 * metadata[0]),
                      archive_entry(shape, 2 * metadata[0] + 1))
    data, tiles, attrs = cache[key]
    width, height = struct.unpack_from("<HH", data)
    if STAGE_DIMENSIONS.get(stage) != (width, height):
        raise ValueError("actor前景地图尺寸不符已确认stage")
    x, y = unit[:2]
    cells = []
    for cy in (y - 1, y):
        if not 0 <= x < width or not 0 <= cy < height:
            return None  # 此处无完整上下两格，保留既有边缘资源判据。
        cell = struct.unpack_from("<I", data, 4 + 4 * (cy * width + x))[0]
        tile = cell & 1023
        index = (cell >> 16) & 31
        if attrs[4 * tile] & 0x60 == 0x20 and source[0x30a3 + index]:
            tile = ((cell & 0xffff) + 1) & 1023
        cells.append((cy, tile))
    if not any(attrs[4 * tile] & 0x80 for _, tile in cells):
        return None
    if unit[4] or any(i != raw and not actor(source, i)[5] & 1 and
            actor(source, i)[0] == x and abs(actor(source, i)[1] - y) <= 1
            for i in range(source[META + 1])):
        raise ValueError("actor前景完成门仅覆盖独立静止actor，不猜相邻/同格绘制顺序")
    bases = []
    overlays = []
    for cy, tile in cells:
        image = sheet_frame(tiles, tile)
        image.putpalette(assets["palette"])
        if attrs[4 * tile] & 0x1c or any(224 <= p < 240 for p in image.getdata()):
            raise ValueError("actor前景完成门未覆盖动画/水DAC地形")
        bases.append(image)
        if attrs[4 * tile] & 0x80:
            count = struct.unpack_from("<H", tiles, 4)[0]
            if tile + 1 >= count:
                raise ValueError("actor前景资源索引越界")
            begin = struct.unpack_from("<I", tiles, 6 + 4 * (tile + 1))[0]
            end = struct.unpack_from("<I", tiles, 6 + 4 * (tile + 2))[0] if tile + 2 < count else len(tiles)
            part, mask = rle_material(struct.pack("<HH", 24, 24) + tiles[begin:end])
            part.putpalette(assets["palette"])
            if any(224 <= p < 240 for p, written in zip(part.getdata(), mask.getdata()) if written):
                raise ValueError("actor前景含动态水DAC，不裁掉该区域")
            overlays.append((cy, tile + 1, part, mask))
    isolated = bytearray(source)
    isolated[META + 1] = 1
    isolated[UNITS:UNITS + STRIDE] = unit
    isolated[UNITS + 5] &= 0x7f
    normal, _ = cursor_assets(original / "FDICON.B24", isolated, assets)
    def compose(sprite):
        image = Image.new("P", (24, 30), 0)
        image.putpalette(assets["palette"])
        image.paste(bases[0].crop((0, 18, 24, 24)), (0, 0))
        image.paste(bases[1], (0, 6))
        image.paste(cursor, (0, 6), cursor.point(lambda p: 255 if p else 0, mode="L"))
        image.paste(sprite, (0, 0), sprite.point(lambda p: 255 if p else 0, mode="L"))
        for cy, _, part, mask in reversed(overlays):
            image.paste(part, (0, 6 + 24 * (cy - y)), mask)
        # 完整底图已恢复，颜色0也是真实像素；RGB使match_indexed不再跳过索引0。
        return image.convert("RGB")
    return {"templates": [compose(acted_sprite(image) if unit[5] & 0x80 else image) for image in normal],
            "unacted_templates": [compose(image) for image in normal],
            "foreground_entries": [entry for _, entry, _, _ in reversed(overlays)],
            "template_rows": 30, "actor_tile": cells[1][1], "upper_tile": cells[0][1]}


def focus_match(frame, original, source, assets, *, require_closed=True, cursor=None, summary=None):
    world, at = tuple(source[META + 5:META + 7]), screen(source, source[META + 5:META + 7])
    if not 4 <= at[0] <= 292 or not 4 <= at[1] <= 172: return None
    if not dialogs_absent(frame, assets) or require_closed and not menu_absent(frame, assets, at): return None
    actual_cursor = assets["cursor"] if cursor is None else cursor
    raw = next((i for i in range(source[META + 1]) if not actor(source, i)[5] & 1 and tuple(actor(source, i)[:2]) == world), None)
    summary_hit=None
    if summary is not None:
        if raw!=summary["raw_actor"]:return None
        if "hp_observation" in summary:
            summary=observed_summary(frame,original,source,assets,summary)
            if summary is None:return None
        summary_hit=match_indexed(frame,summary["image"],summary["position"])
        if not summary_hit["ok"]:return None
    foreground = None
    if raw is not None:
        foreground = actor_foreground_resources(original, source, assets, raw, actual_cursor) if summary is None else None
        candidates = foreground["templates"] if foreground else actor_focus_resources(original, source, assets, raw, actual_cursor)["templates"]
        if summary is not None:
            # 18b84明确先actor/cursor再18c6d；完整板是前景，不把盖住的头部当透明mask略过。
            x,y=at[0],at[1]-6;px,py=summary["position"];panel=summary["image"]
            composed=[]
            for template in candidates:
                image=template.copy();left,top=max(x,px),max(y,py)
                right,bottom=min(x+image.width,px+panel.width),min(y+image.height,py+panel.height)
                if left<right and top<bottom:
                    image.paste(panel.crop((left-px,top-py,right-px,bottom-py)),(left-x,top-y))
                composed.append(image)
            candidates=composed
        hit = next((found for image in candidates if (found := visible_match(frame, image, (at[0], at[1] - 6)))["ok"]), None)
    else:
        lower = next((i for i in range(source[META + 1]) if not actor(source, i)[5] & 1 and tuple(actor(source, i)[:2]) == (world[0], world[1] + 1)), None)
        if lower is None: hit = visible_match(frame, actual_cursor, at)
        else:
            isolated = bytearray(source); isolated[META + 1] = 1; isolated[UNITS:UNITS + STRIDE] = actor(source, lower)
            _, composites = cursor_assets(original / "FDICON.B24", isolated, dict(assets, cursor=actual_cursor))
            hit = next((found for image, offset in composites if offset == 0 and at[1] + image.height <= 200
                and (found := visible_match(frame, image, at))["ok"]), None)
    if not hit or not hit["ok"]:return None
    result={"kind":"natural_resource_focus", "world":list(world), "focus_screen":list(at),
        "camera":list(source[META + 3:META + 5]), "raw_actor":raw, "cursor_actor":hit,
        "full_actor_compared":summary is None and hit["full_resource_compared"] if raw is not None else None}
    if foreground:
        result["actor_local_foreground"] = {key: foreground[key] for key in
            ("foreground_entries", "template_rows", "actor_tile", "upper_tile")}
        if actor(source, raw)[5] & 0x80:
            rejected = [visible_match(frame, image, (at[0], at[1] - 6)) for image in foreground["unacted_templates"]]
            if any(item["ok"] or item.get("bad_pixels", 0) < 24 for item in rejected):
                return None
            result["foreground_acted_witness"] = {**hit, "all_unacted_candidates_rejected": len(rejected),
                "minimum_unacted_bad_pixels": min(item["bad_pixels"] for item in rejected),
                "scope": "complete terrain/cursor/actor/foreground visible resource; no occlusion mask"}
    if summary_hit is not None:
        result["unit_summary"]={"position":list(summary["position"]),"full_resource":summary_hit,
            "fields":summary["fields"],"foreground_composited_over_actor":True}
    return result


def actor_browse_anywhere(frame, original, source, assets, raw=0, *, acted=None):
    """只推断已知world的玩家焦点；不把相同敌军sprite匹配当world证据。"""
    state = bytearray(source)
    if acted is not None:
        state[UNITS + raw * STRIDE + 5] = (state[UNITS + raw * STRIDE + 5] & 0x7f) | (0x80 if acted else 0)
    world = tuple(actor(state, raw)[:2]); found = []
    dimensions = STAGE_DIMENSIONS.get(source[META + 2])
    if dimensions is None: raise ValueError("焦点资源只已确认stage0/1及stage2初图")
    width, height = dimensions
    for ry in range(8):
        for rx in range(13):
            camera = (world[0] - rx, world[1] - ry)
            if not 0 <= camera[0] <= width - 13 or not 0 <= camera[1] <= height - 8: continue
            candidate = view(state, world, camera)
            hit = focus_match(frame, original, candidate, assets)
            if hit and acted:
                witness=hit.get("foreground_acted_witness")
                if witness:
                    hit.update(visible_acted_witness=witness)
                else:
                    at=screen(candidate,world)
                    proof=visible_acted_resources(original,assets,actor(state,raw),(at[0],at[1]-6))
                    witness=next(({"frame":phase,**got} for phase,image in enumerate(proof["sprites"])
                        if (got:=match_indexed(frame,image,proof["position"]))["ok"]),None)
                    if witness is None:continue
                    hit.update(visible_acted_witness=witness,full_actor_compared=proof["full_actor_compared"])
            if hit: found.append((candidate, hit))
    return found[0] if len(found) == 1 else None


def command_match(frame, original, source, assets, raw, selected, disabled=None):
    view_assets = actor_view(original, source, assets, raw)
    at = screen(source, actor(source, raw)[:2])
    # 17898的共享53C13每4 BIOS tick在0/1间闪烁，fresh并不保证是1。
    # capability query仍逐项严格；缺少亮帧时不得把预期default写成已观测selected。
    observable=[];matches=[]
    for item in range(4):
        where=tuple(a+b for a,b in zip(at,MENU_OFFSETS[item]))
        geometry=visible_resource(assets["commands"][3*item],where)
        if geometry is None:
            matches.append({"frame":None,"position":list(where),"observable":False,"checked_pixels":0})
            continue
        observable.append(item)
        states=(0,1,2) if disabled is not None or selected is None else (1,) if item==selected else (0,2)
        found=None
        for state in states:
            index=3*item+state
            templates=view_assets["upper_commands"][index] if item==0 else [assets["commands"][index]]
            for phase,template in enumerate(templates):
                hit=visible_match(frame,template,where)
                if hit["ok"]:
                    found={"frame":index,"position":list(where),"observable":True,
                        "actor_frame_candidate":phase if item==0 else None,**hit};break
            if found:break
        if found is None:return None
        matches.append(found)
    if disabled is None and selected is not None and selected not in observable:return None
    result={"kind":"natural_command_visible_resources","focus_screen":list(at),"selected":selected,
        "matches":matches,"observable_indices":observable,"full_four_icons_compared":len(observable)==4,
        "full_resource_height_checked":len(observable)==4 and all(row["full_resource_compared"] for row in matches),
        "capability_gates_compared":False,"default_selection_compared":False}
    if result and disabled is not None:
        if len(disabled)!=4 or any(value not in (0,1) for value in disabled) or selected not in range(4) or disabled[selected]:return None
        allowed = [[3*i+2] if disabled[i] else [3*i,3*i+1] if i==selected else [3*i] for i in range(4)]
        if any(result["matches"][i]["frame"] not in allowed[i] for i in observable):return None
        observed = selected if result["matches"][selected]["frame"]==3*selected+1 else None
        result.update(capability_gates_compared=len(observable)==4,capability_observed_indices=observable,
            observed_disabled=[disabled[i] if i in observable else None for i in range(4)],
            expected_disabled=list(disabled),allowed_frames=allowed,
            expected_first_enabled=selected,selected=observed,default_unobserved=observed is None,
            default_selection_compared=observed is not None)
    return result


def navigation(source, target):
    dimensions = STAGE_DIMENSIONS.get(source[META + 2])
    if dimensions is None: raise ValueError("逐格焦点只已确认stage0/1及stage2初图")
    reference = native_focus_reference(tuple(source[META + 3:META + 5]), tuple(source[META + 5:META + 7]), target, dimensions)
    previous = tuple(source[META + 5:META + 7]); result = []
    for step in reference["steps"]:
        world = tuple(step["world"])
        key = "RIGHT" if world[0] > previous[0] else "LEFT" if world[0] < previous[0] else "DOWN" if world[1] > previous[1] else "UP"
        result.append((key, view(source, world, step["camera"])))
        previous = world
    return result


def _range_rectangle(position, panels):
    """只避固定资源板，保留最大的完整矩形；不从实际坏像素推遮挡。"""
    x, y = position
    boxes = [(x, y, x + 24, y + 24)]
    for px, py, width, height in panels:
        remaining = []
        for left, top, right, bottom in boxes:
            il, it = max(left, px), max(top, py)
            ir, ib = min(right, px + width), min(bottom, py + height)
            if il >= ir or it >= ib:
                remaining.append((left, top, right, bottom))
            else:
                remaining.extend(box for box in ((left, top, il, bottom),
                    (ir, top, right, bottom), (left, top, right, it),
                    (left, ib, right, bottom)) if box[0] < box[2] and box[1] < box[3])
        boxes = remaining
    return max(boxes, key=lambda box: (box[2]-box[0])*(box[3]-box[1]), default=None)


def _stage_map(original, source):
    stage = source[META + 2]
    if stage not in STAGE_DIMENSIONS:
        raise ValueError("自然资源门只已确认stage0/1及stage2初始活动图")
    field = (original / "FDFIELD.DAT").read_bytes()
    data, meta = archive_entry(field, 3 * stage), archive_entry(field, 3 * stage + 1)
    width, height = struct.unpack_from("<HH", data)
    if (width, height) != STAGE_DIMENSIONS[stage] or len(data) != 4 + width * height * 4:
        raise ValueError("自然资源门地图尺寸/完整长度不等于原stage记录")
    return data, meta, width, height


def range_probe(original, source, assets, raw, *, radius=None, minimum=0, allow_empty=False, physical_third_actors=()):
    """干燥地形的完整可见矩形正门；移动只用可达首步，cast沿profile0全1。"""
    unit = actor(source, raw); world = tuple(unit[:2])
    data, meta, width, height = _stage_map(original, source)
    shape = (original / "FDSHAP.DAT").read_bytes(); tiles, attrs = archive_entry(shape, 2*meta[0]), archive_entry(shape, 2*meta[0]+1)
    profile, _ = movement_profile(original / "FD2.EXE", unit[0x20] if radius is None else 0)
    if radius is not None and profile != bytes([1])*20: raise ValueError("自然cast需已证全1 profile0")
    lut_sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 3)
    count = struct.unpack_from("<H", lut_sheet, 4)[0]
    panels = []
    if radius is None:
        sheet = archive_entry((original / "FDOTHER.DAT").read_bytes(), 5)
        start, end = struct.unpack_from("<II", sheet, 6 + 130*4)
        if not 6 + struct.unpack_from("<H",sheet,4)[0]*4 <= start < end <= len(sheet):
            raise ValueError("自然移动信息板目录边界无效")
        if (sheet_frame(sheet, 22, lmi=True).size != (149, 42) or
                decode_image(sheet[start:end]).size != (69, 34)):
            raise ValueError("自然移动信息板尺寸不同于原资源")
        # sub_18B84 @0x18b84：相对X>=7时page偏移2285，否则2436；
        # page(0,0)复制到VGA(4,4)。sub_18C6D @0x18c6d绘frame22。
        # field_cell_info_panel_draw @0x1acf3的frame130侧别沿旧owner，
        # 本工具不猜侧别，保守避开两种已证位置，而非模拟当前HP/名字。
        panels = [(9 if world[0]-source[META+3] >= 7 else 160, 9, 149, 42),
                  (5, 161, 69, 34), (246, 161, 69, 34)]
    occupied = {tuple(actor(source, i)[:2]) for i in range(source[META + 1]) if not actor(source, i)[5] & 1}
    # sub_127E0 @0x127e0：24x24 sprite在单位格上移6px；上邻格只被头6行遮住。
    # 移动范围沿用固定几何矩形裁剪，不用实际坏像素/透明洞拟合裁剪位置。
    heads = [[i,*screen(source,actor(source,i)[:2]),24,6]
        for i in range(source[META+1]) if not actor(source,i)[5]&1]
    for head in heads:head[2] -= 6
    candidates = [];occupied_candidates=[]
    for y in range(max(0,source[META+4]),min(height,source[META+4]+8)):
        for x in range(max(0,source[META+3]),min(width,source[META+3]+13)):
            distance = abs(x-world[0])+abs(y-world[1])
            if radius is None and distance != 1 or radius is not None and not minimum <= distance <= radius: continue
            occupant=next((i for i in physical_third_actors if i!=raw and
                not actor(source,i)[5]&1 and tuple(actor(source,i)[:2])==(x,y)),None)
            occupied_strip=(x,y) in occupied
            if occupied_strip and (radius is None or occupant is None or
                    (x,y)==tuple(source[META+5:META+7]) or (x,y+1) in occupied):continue
            at=screen(source,(x,y))
            if not (4<=at[0]<=292 and 4<=at[1]<=172):continue
            rectangle = _range_rectangle(at, panels+[head[1:] for head in heads])
            if rectangle is None: continue
            if occupied_strip:
                # 127e0的完整24x24 actor上移6px，本格底18..23行没有own sprite。
                # 仅明确不受普通物理动作影响的第三actor；下邻空故无其头6行，
                # focus/目标cursor不在本格。不按实际坏像素或sprite透明孔裁剪。
                rectangle=(at[0],at[1]+18,at[0]+24,at[1]+24)
                if any(max(rectangle[0],px)<min(rectangle[2],px+w) and
                       max(rectangle[1],py)<min(rectangle[3],py+h)
                       for px,py,w,h in panels+[head[1:] for head in heads]):continue
            left, top, right, bottom = rectangle
            crop = (left-at[0], top-at[1], right-at[0], bottom-at[1])
            cell=struct.unpack_from("<I",data,4+(y*width+x)*4)[0];tile=cell&1023
            if cell>>10 or tile*4+4>len(attrs) or attrs[tile*4]:continue
            category=attrs[tile*4+1]
            if category>=len(profile):raise ValueError("自然地形movement category越界")
            cost=profile[category]
            if radius is None and (not cost or cost>unit[0x3b]):continue
            image=sheet_frame(tiles,tile);pixels=bytes(image.getdata())
            if 0 in pixels or any(224<=p<240 for p in pixels):continue
            image.putpalette(assets["palette"]);ranges=[]
            for phase in range(count):
                start=struct.unpack_from("<I",lut_sheet,6+phase*4)[0]
                end=struct.unpack_from("<I",lut_sheet,10+phase*4)[0] if phase+1<count else len(lut_sheet)
                lut=lut_sheet[start:end]
                if len(lut)!=256:raise ValueError("自然范围LUT不完整")
                transformed=Image.frombytes("P",(24,24),bytes(lut[p] for p in pixels));transformed.putpalette(assets["palette"])
                visible, plain = transformed.crop(crop), image.crop(crop)
                changed = sum(max(abs(a-b) for a,b in zip(want,got)) > TOLERANCE
                    for want,got in zip(visible.convert("RGB").getdata(),plain.convert("RGB").getdata()))
                if changed >= 24:ranges.append((phase,visible))
            if ranges:
                candidate={"world":[x,y],"position":(left,top),"base":image,"ranges":ranges,
                "range_roi":[left,top,right-left,bottom-top],"tile_roi":[*at,24,24],
                "excluded_information_panels":[list(panel) for panel in panels],
                "excluded_actor_heads":heads}
                if occupied_strip:
                    candidate.update(occupied_actor_raw=occupant,actor_pixels_compared=False,
                        scope="physical unaffected third-actor tile bottom24x6 terrain only; actor not compared")
                    occupied_candidates.append(candidate)
                else:candidates.append(candidate)
    if not candidates:candidates=occupied_candidates
    if not candidates and not allow_empty:raise ValueError("自然动作无避开固定信息板/无水色的完整矩形正向range资源门；未发送选择键")
    return candidates


def range_match(frame, probes):
    for probe in probes:
        for phase, image in probe["ranges"]:
            hit=exact_tile(frame,image,probe["position"])
            if hit:return {"world":probe["world"],"position":list(probe["position"]),"lut_phase":phase,
                "range_roi":probe["range_roi"],"tile_roi":probe["tile_roi"],
                "excluded_information_panels":probe["excluded_information_panels"],
                "excluded_actor_heads":probe["excluded_actor_heads"],
                "scope":probe.get("scope","visible range LUT rectangle only; information panels not compared"),
                **({"occupied_actor_raw":probe["occupied_actor_raw"],"actor_pixels_compared":False} if "occupied_actor_raw" in probe else {}),**hit}
    return None


def visible_acted_resources(original, assets, unit, position):
    """完整可见部分仍须对全部12未acted帧有独立正差，边界不依赖等待秒数。"""
    icons = (original / "FDICON.B24").read_bytes()
    normal = [sheet_frame(icons, unit[7] * 12 + phase) for phase in range(12)]
    for image in normal:
        if image.size != (24, 24): raise ValueError("自然完成锚需要完整24x24角色sprite")
        image.putpalette(assets["palette"])
    acted = [acted_sprite(image) for image in normal]
    clipped=visible_resource(normal[0],position)
    if clipped is None:raise ValueError("自然动作caster无可见sprite，当前完成门不覆盖；未发送动作键")
    _,visible_position,crop=clipped
    full_actor=crop==(0,0,24,24)
    normal=[image.crop(crop) for image in normal];acted=[image.crop(crop) for image in acted]
    for image in acted:
        pixels, colors = list(image.getdata()), list(image.convert("RGB").getdata())
        if sum(bool(pixel) for pixel in pixels)<24 or any(sum(bool(pixel) and max(abs(a-b) for a,b in zip(want,got)) > TOLERANCE
                   for pixel,want,got in zip(pixels,colors,other.convert("RGB").getdata())) < 24
               for other in normal):
            raise ValueError("自然caster可见acted资源不能独立区别全部未acted帧；未发送动作键")
    return {"position":visible_position,"original_position":position,
            "full_actor_compared":full_actor,"sprite_crop":list(crop),
            "sprites":acted,"unacted_sprites":normal}


def action_completion_resources(original, source, assets, raw, bank, probes):
    """18d8c成功尾acted是提交锚；普通physical/0/13/herb保留确认target焦点。"""
    unit=actor(source,raw);at=screen(source,unit[:2])
    proof=visible_acted_resources(original,assets,unit,(at[0],at[1]-6))
    if not probes: raise ValueError("自然目标完成门缺少原确认range的静态资源")
    return {**proof,"raw_actor":raw,"world":list(source[META+5:META+7]),
            "camera":list(source[META+3:META+5]),"bank":bank,"probes":probes}


def action_completion_match(frame, source, assets, resources):
    """裸tile只作恢复事实；另见可见caster acted正门，完整性由full_actor_compared标明。"""
    cursor = cursor_match(frame, assets, resources["bank"], screen(source, resources["world"]))
    if not cursor: return None
    bare = None
    for probe in resources["probes"]:
        left,top,width,height = probe["range_roi"]; x,y,_,_ = probe["tile_roi"]
        image = probe["base"].crop((left-x,top-y,left-x+width,top-y+height))
        hit = exact_tile(frame,image,(left,top))
        if hit:
            bare = {"world":probe["world"],"roi":probe["range_roi"],**hit}; break
    if bare is None: return None
    acted = next(({"frame":phase,**hit} for phase,image in enumerate(resources["sprites"])
                  if (hit:=match_indexed(frame,image,resources["position"]))["ok"]),None)
    unacted = None if acted else next(({"frame":phase,**hit} for phase,image in enumerate(resources["unacted_sprites"])
                  if (hit:=match_indexed(frame,image,resources["position"]))["ok"]),None)
    return {"kind":"natural_declared_target_browse","world":resources["world"],
            "camera":resources["camera"],"cursor":cursor,"restored_bare_probe":bare,
            "bare_is_commit_witness":False,"caster_raw":resources["raw_actor"],
            "caster_sprite_position":list(resources["position"]),"caster_acted_sprite":acted,
            "caster_original_sprite_position":list(resources.get("original_position",resources["position"])),
            "full_actor_compared":resources.get("full_actor_compared",True),
            "caster_unacted_sprite":unacted,
            "commit_witness":("full caster acted sprite" if resources.get("full_actor_compared",True) else "visible caster acted sprite") if acted else None,
            "HP_or_hidden_inferred":False}


def stage0_pages(original, assets):
    """34531先group3/script7/fragment11，再group7/script8/fragment3。
    112a5(1)只追加persistent，不占active raw；后续3460b/34673/346cd
    依次追加group4/5/6。-19/-20必须按此顺序解释，不能用模板表顺序。
    34738/1aa1d及22ef6另用7/8/9；fragment10不在本已确认调用集。
    """
    metadata=archive_entry((original/"FDFIELD.DAT").read_bytes(),1)
    if len(metadata)<0x83 or (len(metadata)-0x83)%26:
        raise ValueError("stage0剧情需要完整26B模板表")
    templates=[metadata[i:i+26] for i in range(0x83,len(metadata),26)]
    ids=[unit["unit_id"] for unit in opening_geometry(original)["actors"]]
    for group in (3,7,4,5,6):
        ids.extend(record[1] for record in templates if record[0x15]==group)
    return [page for page in build_pages(original,assets,stage_plan=((0,12),),
        identities_by_stage={0:ids},text_identities_by_stage={0:{identity:identity for identity in ids}})
        if page["fragment"] in (3,4,5,6,7,8,9,11)]


def required_reward_pages(original, assets, source):
    """只匹配本次新写的一行+原portrait+箭头，不把未知随机gain猜作数字。"""
    tables,_=reward_tables(original/"FD2.EXE");pages=[]
    identities={actor(source,i)[7] for i in range(source[META+1]) if actor(source,i)[6] in (1,2) and actor(source,i)[7]<68}
    metadata=archive_entry((original/"FDFIELD.DAT").read_bytes(),1)
    identities.update(metadata[i+1] for i in range(0x83,len(metadata),26) if metadata[i] in (1,2) and metadata[i+1]<68)
    for identity in sorted(identities):
        if identity not in (0,1,3,4,9,30):
            raise ValueError("自然奖励等待序列仅已核六种portrait")
        row=tables["growth"][identity*11:(identity+1)*11]
        # 1e529第二个非零成长先scroll19，移走y138的旧LEVEL行。
        # 本首关六人各至少四项，故末成长后新489可与旧页资源区分。
        if sum(bool(row[2*i+1]) for i in range(5))<4:
            raise ValueError("自然奖励成长项不足，不能沿用跨级页面可区分合同")
        events=[{"fragment":489,"kind":"level","value":0,"line":1,"requires_key":True}]
        events += [{"fragment":490+i,"kind":"growth","value":row[2*i+1],"line":2,"requires_key":True} for i in range(5) if row[2*i+1]]
        for event in events:
            page=reward_pages(original,assets,identity,[event])[0];page["portrait_id"]=identity;pages.append(page)
    return pages


def cursor_bank(original, source, assets, cursor=None):
    """导航只证光标所在格；敌军未知新位置不从SDL模拟猜测。"""
    metadata=archive_entry((original/"FDFIELD.DAT").read_bytes(),1)
    identities={actor(source,i)[7] for i in range(source[META+1])}
    identities.update(metadata[i+1] for i in range(0x83,len(metadata),26))
    if 2*len(identities)>96:raise ValueError("自然光标资源read-set超出首关已证单位容量")
    isolated=bytearray(source);isolated[META+1]=2*len(identities)
    for i,identity in enumerate(sorted(identities)):
        for acted in (0,1):
            at=UNITS+(2*i+acted)*STRIDE
            isolated[at:at+STRIDE]=bytes(STRIDE);isolated[at+7]=identity;isolated[at+5]=0x80*acted
    chosen=assets["cursor"] if cursor is None else cursor
    _,composites=cursor_assets(original/"FDICON.B24",isolated,dict(assets,cursor=chosen))
    unique={}
    for image,offset in composites:
        # 只比较预先定义的光标格24×24，保留该格全部光标及角色遮挡。
        # 当前格actor上方6行、下邻actor格外身体不属于导航门；后者还会
        # 被1acf3固定地形板覆盖，不能用42行模板跨进另一个UI owner。
        picture=image.crop((0,-offset,24,24-offset))
        unique[picture.tobytes()]=(picture,0)
    return [(chosen,0)]+list(unique.values())


def cursor_match(frame, assets, bank, at):
    if not dialogs_absent(frame,assets) or not menu_absent(frame,assets,at):return None
    for index,(image,offset) in enumerate(bank):
        where=(at[0],at[1]+offset)
        hit=visible_match(frame,image,where)
        if hit["ok"]:return {"kind":"natural_navigation_cursor","focus_screen":list(at),"resource_candidate":index,**hit}
    if not bank or any(image.size!=(24,24) or offset!=0 for image,offset in bank):return None
    # sub_127E0固定上移6px：本格actor只占上18行，下邻头只占下6行。
    # 两者可同时出现；分别匹配完整资源带，避免组合所有actor帧的笛卡尔积。
    # 不能只过上带，也不能按实际坏像素重新裁剪。
    bands=[]
    for top,bottom in ((0,18),(18,24)):
        seen=set();found=None
        for index,(image,_) in enumerate(bank):
            part=image.crop((0,top,24,bottom));pixels=part.tobytes()
            if pixels in seen:continue
            seen.add(pixels)
            hit=visible_match(frame,part,(at[0],at[1]+top))
            if hit["ok"]:
                found={"resource_candidate":index,**hit};break
        if found is None:return None
        bands.append(found)
    visible,where,crop=visible_resource(bank[0][0],at)
    return {"kind":"natural_navigation_cursor","focus_screen":list(at),"resource_candidate":None,
        "resource_bands":bands,"ok":True,"bad_pixels":0,
        "checked_pixels":sum(part["checked_pixels"] for part in bands),
        "max_channel_delta":max(part["max_channel_delta"] for part in bands),
        "visible_roi":[*where,visible.width,visible.height],"resource_crop":list(crop),
        "full_resource_compared":all(part["full_resource_compared"] for part in bands)}


def round_banner(original, assets, turn):
    if not 1<=turn<=255:raise ValueError("自然回合编号超出SAV byte范围")
    sheet=archive_entry((original/"FDOTHER.DAT").read_bytes(),5)
    image=sheet_frame(sheet,91,lmi=True);image.putpalette(assets["palette"])
    if image.size!=(87,20):raise ValueError("回合横幅不是原87x20")
    for digit,x in zip(f"{turn:03d}",(51,57,63)):
        start,end=struct.unpack_from("<II",sheet,6+4*(42+int(digit)))
        part=decode_image(sheet[start:end]);part.putpalette(assets["palette"])
        if part.width>6 or part.height!=8:raise ValueError("回合数字尺寸不同于原资源")
        image.paste(part,(x,7),part.point(lambda p:255 if p else 0,mode="L"))
    return image


def empty_tile(original, source, assets, frame):
    """从本端当前WGC选择可见干燥空格，随后才导航/开菜单，不猜AI位置。"""
    data,meta,width,height=_stage_map(original,source)
    shape=(original/"FDSHAP.DAT").read_bytes();tiles,attrs=archive_entry(shape,2*meta[0]),archive_entry(shape,2*meta[0]+1)
    origin=tuple(source[META+5:META+7]);choices=[]
    for ry in range(2,6):
        for rx in range(2,11):
            world=(source[META+3]+rx,source[META+4]+ry)
            if not 0<=world[0]<width or not 0<=world[1]<height:continue
            cell=struct.unpack_from("<I",data,4+(width*world[1]+world[0])*4)[0];index=cell&1023
            if cell>>10 or index*4+4>len(attrs) or attrs[index*4]:continue
            image=sheet_frame(tiles,index);pixels=bytes(image.getdata())
            if 0 in pixels or any(224<=p<240 for p in pixels):continue
            image.putpalette(assets["palette"]);at=screen(source,world)
            hit=exact_tile(frame,image,at)
            if hit:choices.append((abs(world[0]-origin[0])+abs(world[1]-origin[1]),world,image,hit))
    if not choices:raise ValueError("当前实际画面没有可完整证明的干燥空菜单格；未发送ENTER")
    _,world,image,hit=min(choices,key=lambda row:(row[0],row[1]))
    composite=image.copy();composite.paste(assets["cursor"],(0,0),assets["cursor"].point(lambda p:255 if p else 0,mode="L"))
    return {"world":world,"position":screen(source,world),"base":image,"focused":composite,
        "observed_empty_tile":hit,"source":"current endpoint WGC full576 dry pixels"}
