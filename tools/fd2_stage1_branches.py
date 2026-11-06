"""B7 三箱与六村民败北的有限输入协议；不管理进程或启动窗口。

依据 field_special_cell_execute @0x190ac、stage1 结果门 @0x206c5。
调用方负责 owned Job、原版先行、两端清理和最终来源守卫；本模块保留每个
检查点的完整 Save/mtime 守卫。受控拾取不代表新物品 Use 或自然整关通过。
"""
import hashlib
from pathlib import Path
import struct
import time

from PIL import Image

import fd2_natural_oracle as natural
import fd2_stage1_complete_oracle as oracle
from fd2_item_list_oracle import inventory_templates
from fd2_loot_oracle import inventory_append, message_pages, message_match
from fd2_movement_oracle import movement_profile
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import archive_entry, title_templates
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets
from run_original_item_list import inventory_detail
from run_original_load import require_source_unchanged, wait_measured_save
from run_original_player import cursor_assets, exact_tile, screen, sheet_frame
from fd2_battle_animation_frames import rle_material
from run_original_result import result_branch, result_templates

SCENARIOS = ("chest-129", "chest-201", "chest-193", "villager-defeat")
COMPLETED = 0x30a3


def sha(data):
    return hashlib.sha256(data).hexdigest()


def _approach(original, source, chest):
    """实际一格入箱；原4e4f6的32项工作区不接纳旧加速预算。"""
    unit = oracle.actor(source, 0)
    # 每帧8B，budget31的单位成本路径至多32帧；第33帧覆盖6017a mode。
    if not 0 < unit[59] < 32:
        raise ValueError("三箱路径预算必须1..31，不能沿用MOV40加速夹具")
    data = archive_entry((original / "FDFIELD.DAT").read_bytes(), 3)
    shape = (original / "FDSHAP.DAT").read_bytes()
    attrs, tiles = archive_entry(shape, 3), archive_entry(shape, 2)
    profile, _ = movement_profile(original / "FD2.EXE", 19 if unit[32] == 19 or unit[31] in (4, 5) else unit[32])
    occupied = {tuple(oracle.actor(source, raw)[:2]) for raw in range(1, source[META + 1])
        if not oracle.actor(source, raw)[5] & 1}
    cx, cy = chest["world"]
    cost = profile[attrs[(chest["cell_word"] & 1023) * 4 + 1]]
    if not 0 < cost <= unit[59]: raise ValueError("主角不能按真实 profile 一步进入此箱格")
    for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
        x, y = cx + dx, cy + dy
        if not 0 <= x < 27 or not 1 <= y < 21 or (x, y) in occupied or (x, y + 1) in occupied: continue
        cell = struct.unpack_from("<I", data, 4 + 4 * (y * 27 + x))[0]
        tile = cell & 1023
        if attrs[4 * tile] or not 0 < profile[attrs[4 * tile + 1]] <= unit[59]: continue
        picture = sheet_frame(tiles, tile)
        if any(px == 0 or 224 <= px < 240 for px in picture.getdata()): continue
        return (x, y)
    raise ValueError("此箱没有本协议可完整取证的一格静态可行相邻格")


def _derive_chest(original, fixture, scenario):
    """只改本次来源的已知字段；原 roster、槽、未知记录字节均保留。"""
    encrypted = fixture.read_bytes()
    source = oracle.decode_save(fixture)
    validation = oracle.validate_post_turn3_source(original, source)
    identity = int(scenario.split("-")[1])
    row = next(row for row in oracle.stage1_chests(original) if row["item_id"] == identity)
    hero = oracle.actor(source, 0)
    if hero[6:9] != bytes((2, 0, 0)) or hero[5] & 0x85 or hero[3:5] != bytes(2):
        raise ValueError("三箱需真实可操作、静止且朝下的 raw0 主角")
    # sub_4E838 @0x4e838返回此24B；10c50复制+7到actor+59，1b750不写MOV。
    # 仅三箱控制夹具的同unit0/profile1/装备0+132，不作为通用actor默认值。
    base = bytes(oracle.tables(original)["raw"][0x61da1:0x61da1 + 24])
    if (base.hex() != "0101012a00000004000000000084c0ffffff000000000000" or
            hero[31:33] != base[:2] or hero[10:14] != bytes((0x40, 0, 0x40, 132)) or not 0 < base[7] < 32):
        raise ValueError("三箱MOV恢复需原unit0 base、profile1及装备0/132的确证身份")
    if source[COMPLETED + row["map_cell_index"]]:
        raise ValueError("三箱来源的目标 completed 必须尚未写入")
    visible = [oracle.actor(source, raw) for raw in range(source[META + 1])
        if not oracle.actor(source, raw)[5] & 1]
    if any(unit[3:5] != bytes(2) or unit[5] & 0x80 or any(unit[34:40]) or
            struct.unpack_from("<H", unit, 64)[0] != struct.unpack_from("<H", unit, 66)[0]
            for unit in visible):
        raise ValueError("三箱有限 END 模型需原表静止、未行动、无状态且满 HP")
    if sum(unit[6] == 2 for unit in visible) < 2:
        raise ValueError("三箱需另一个仍可行动的玩家，以便业务后显式 END")
    if any(list(unit[:2]) in (row["world"], [row["world"][0], row["world"][1] + 1])
            for raw in range(1, source[META + 1])
            if not (unit := oracle.actor(source, raw))[5] & 1):
        raise ValueError("三箱目标或下格被其他可见 actor 占用")
    inventory_append(hero, identity)  # 原版空槽门，满库存不改造为替换场景。
    changed = bytearray(source)
    owners = {}
    def put(at, value, owner):
        changed[at:at + len(value)] = value
        owners.update((offset, owner) for offset in range(at, at + len(value)))
    put(UNITS + 59, bytes((base[7],)), "controlled Hero0 MOV restored from canonical player base61da1+7, not accelerated source")
    approach = _approach(original, changed, row)
    put(UNITS, bytes(approach), "controlled raw0 x/y at a resource-proven adjacent approach")
    for raw in range(source[META + 1]):
        unit = oracle.actor(source, raw)
        if unit[6] != 2:
            put(UNITS + raw * STRIDE + 5, bytes((unit[5] | 4,)), "13a9f AI-ineligible bit04; no hiding or killing")
    x, y = approach
    camera = (max(0, min(27 - 13, x - 6)), max(0, min(21 - 8, y - 3)))
    put(META + 3, bytes((*camera, x, y, x - camera[0], y - camera[1])),
        "controlled initial camera/focus derived from this 27x21 map")
    changed = oracle.checksum(changed)
    owners.update((offset, "checksum") for offset in range(len(changed) - 4, len(changed)))
    patches = [{"offset": at, "before": a, "after": z, "owner": owners[at]}
        for at, (a, z) in enumerate(zip(source, changed)) if a != z]
    encoded = bytes(a ^ b ^ c for a, b, c in zip(encrypted, source, changed))
    return encoded, changed, {"scenario": scenario, "controlled_fixture": True,
        "natural_stage1_battle": False, "source_path": str(fixture.resolve()),
        "source_sha256": sha(encrypted), "source_mtime_ns": fixture.stat().st_mtime_ns,
        "source_validation": validation, "source_plain_sha256": sha(source),
        "derived_sha256": sha(encoded), "derived_plain_sha256": sha(changed),
        "plaintext_patches": patches, "encrypted_patches": [
            {"offset": at, "before": a, "after": z} for at, (a, z) in enumerate(zip(encrypted, encoded)) if a != z],
        "all_other_container_bytes_preserved": True, "chest": row, "approach_world": list(approach),
        "action_trigger": "one-step move then WAIT; exit and re-enter completed cell",
        "movement": {"source_raw": hero[59], "restored_raw": base[7], "base_canonical_va": "0x61da1",
            "base_24b_hex": base.hex(), "actor_offset": 59, "path_budget_limit": 31,
            "identity": {"unit_id": 0, "race": hero[31], "profile": hero[32], "equipped_slots_hex": hero[10:14].hex()},
            "natural_corroboration_sha256": "82b8302a78d63ce03d10872e24525fcf861cf2c0a4f088708babc0d1086ab158",
            "scope": "this controlled Hero0 only; full source/roster retained, not a generic actor default"},
        "closed_origin_stationary_path_anomaly": "C9 failed retained; original-code probe reproduced MOV40 recursion overwriting6017a mode and returningpath0d; MOV4 normal; corrected fixture GUI pending",
        "acquired_item_use_effect_claimed": False}


def _hero_templates(prepared, source):
    """129ec/12ac6：只合成静止主角本格/上格前景，不删被挡像素。"""
    unit = oracle.actor(source, 0)
    isolated = bytearray(source); isolated[META + 1] = 1
    sprites, _ = cursor_assets(prepared["original"] / "FDICON.B24", isolated, prepared["assets"])
    images = []
    data, tiles, attrs, width = prepared["map_assets"]
    cells = []
    for x, y in ((unit[0], unit[1] - 1), (unit[0], unit[1])):
        cell = struct.unpack_from("<I", data, 4 + 4 * (y * width + x))[0]
        tile, index = cell & 1023, (cell >> 16) & 31
        if attrs[4 * tile] & 0x60 == 0x20 and source[COMPLETED + index]:
            tile = ((cell & 0xffff) + 1) & 1023
        if attrs[4 * tile] & 0x1c:
            raise ValueError("三箱完整焦点模板仅已确认静态非水色地形")
        base = sheet_frame(tiles, tile)
        if any(224 <= px < 240 for px in base.getdata()):
            raise ValueError("三箱焦点地形含动态水 DAC，不裁掉该区域")
        cells.append((y, tile, base))
    for phase in range(3):
        image = Image.new("P", (24, 30), 0); image.putpalette(prepared["assets"]["palette"])
        image.paste(cells[0][2].crop((0, 18, 24, 24)), (0, 0))
        image.paste(cells[1][2], (0, 6))
        for part, at in ((prepared["assets"]["cursor"], (0, 6)), (sprites[phase], (0, 0))):
            image.paste(part, at, part.point(lambda px: 255 if px else 0, mode="L"))
        if unit[7] == 28 or unit[32] != 19 and unit[31] not in (4, 5):
            for y, tile, _ in reversed(cells):
                flags = attrs[4 * tile]
                if flags & 8:
                    raise ValueError("三箱焦点前景不扩展未确认动画相位")
                if flags & 0x80:
                    count = struct.unpack_from("<H", tiles, 4)[0]
                    begin = struct.unpack_from("<I", tiles, 6 + 4 * (tile + 1))[0]
                    end = struct.unpack_from("<I", tiles, 6 + 4 * (tile + 2))[0] if tile + 2 < count else len(tiles)
                    part, mask = rle_material(struct.pack("<HH", 24, 24) + tiles[begin:end])
                    part.putpalette(image.getpalette())
                    image.paste(part, (0, 6 + 24 * (y - unit[1])), mask)
        images.append(image)
    return images


def hero_focus(frame, prepared, source):
    at = screen(source, oracle.actor(source, 0)[:2])
    if not natural.dialogs_absent(frame, prepared["assets"]) or not natural.menu_absent(frame, prepared["assets"], at):
        return None
    templates = prepared["hero_templates"][(tuple(oracle.actor(source, 0)[:2]), bool(oracle.actor(source, 0)[5] & 0x80), bool(source[COMPLETED + prepared["chest"]["map_cell_index"]]))]
    hit = next((dict(phase=phase, **found) for phase, image in enumerate(templates)
        if (found := exact_tile(frame, image, (at[0], at[1] - 6)))), None)
    return {"kind": "stage1_chest_hero_closed", "raw_actor": 0, "world": list(oracle.actor(source, 0)[:2]),
        "camera": list(source[META + 3:META + 5]), "focus_screen": list(at),
        "actor_cursor_foreground": hit, "template_size": [24, 30],
        "acted": bool(oracle.actor(source, 0)[5] & 0x80)} if hit else None


def _after_action(prepared, source, ordinal):
    result = bytearray(source); unit = bytearray(oracle.actor(result, 0))
    if ordinal == 0:
        unit = bytearray(inventory_append(unit, prepared["chest"]["item_id"]))
        result[COMPLETED + prepared["chest"]["map_cell_index"]] = 1
    unit[5] |= 0x80; result[UNITS:UNITS + STRIDE] = unit
    return oracle.checksum(result)


def _after_end(source):
    result = bytearray(source)
    for raw in range(result[META + 1]): result[UNITS + raw * STRIDE + 5] &= 0x7f
    result[META] += 1
    steps = natural.navigation(result, oracle.actor(result, 0)[:2])
    return oracle.checksum(steps[-1][1] if steps else result)


def check_chest_save(prepared, before, measured, expected):
    """只允许现已证 active+2 cache 派生；其余整个容器逐字严格核。"""
    if oracle.checksum(measured) != measured or len(measured) != len(before):
        raise AssertionError("三箱实际 Save checksum/长度不符")
    wanted = bytearray(expected); cache = []
    for raw in range(expected[META + 1]):
        at = UNITS + raw * STRIDE + 2
        if measured[at] != wanted[at]: cache.append({"raw": raw, "offset": at, "expected": wanted[at], "actual": measured[at]})
        wanted[at] = measured[at]
    wanted = oracle.checksum(wanted)
    diff = [at for at, (a, z) in enumerate(zip(wanted, measured)) if a != z]
    if diff: raise AssertionError(f"三箱完整 Save 超出已证写集：{diff[:20]}")
    return {"complete_container_equal_except_actor_cache_byte2": True, "actor_cache_differences": cache,
        "raw_equal": expected == measured, "mask_equal": True, "turn": measured[META],
        "item_id": prepared["chest"]["item_id"], "completed_index": prepared["chest"]["map_cell_index"],
        "repeat_completed_cell_did_not_append_again": True, "acquired_item_use_effect_claimed": False,
        "persistent_manual_unknown_bytes_preserved": True}


def prepare(original, fixture, scenario, source_report):
    original, fixture = Path(original), Path(fixture)
    if scenario not in SCENARIOS: raise ValueError("未知 B7 三箱/村民败北分支")
    assets = round_assets(original)
    if scenario == "villager-defeat":
        origin = oracle.validate_origin_report(source_report, fixture)
        encoded, source, manifest = oracle.derive_fixture(original, fixture, scenario)
        steps = natural.navigation(source, oracle.actor(source, 0)[:2])
        focused = steps[-1][1] if steps else source
        summary = natural.selection_summary(original, focused, assets, 0)
        probes = natural.range_probe(original, focused, assets, 0)
        if not probes: raise ValueError("六村民败北的 controller 取消入口缺原移动范围正门")
        return {"original": original, "scenario": scenario, "encoded": encoded, "source": source,
            "manifest": manifest, "origin": origin, "assets": assets,
            "titles": title_templates(original / "FDOTHER.DAT"), "result": result_templates(original, assets),
            "defeat_navigation": steps, "defeat_summary": summary, "defeat_probes": probes,
            "expected_acknowledgements": {}, "required_snapshots": (), "result_capture_required": True}
    # 延迟 import，central 同样可调用本模块；避免创建第二个 provenance checker。
    from run_original_stage1_complete import source_origin
    origin = source_origin(source_report, fixture)
    encoded, source, manifest = _derive_chest(original, fixture, scenario)
    field = (original / "FDFIELD.DAT").read_bytes(); shape = (original / "FDSHAP.DAT").read_bytes()
    data = archive_entry(field, 3); metadata = archive_entry(field, 4)
    prepared = {"original": original, "scenario": scenario, "encoded": encoded, "source": source,
        "manifest": manifest, "origin": origin, "assets": assets, "chest": manifest["chest"],
        "titles": title_templates(original / "FDOTHER.DAT"),
        "map_assets": (data, archive_entry(shape, 2 * metadata[0]), archive_entry(shape, 2 * metadata[0] + 1), 27),
        "expected_acknowledgements": {}, "required_snapshots": ("completed_save",), "result_capture_required": False}
    prepared["hero_templates"] = {}
    for world in (tuple(manifest["approach_world"]), tuple(prepared["chest"]["world"])):
        for acted in (False, True):
            for completed in (False, True):
                state = bytearray(source); state[UNITS:UNITS + 2] = bytes(world)
                state[UNITS + 5] = state[UNITS + 5] & 0x7f | (0x80 if acted else 0)
                state[COMPLETED + prepared["chest"]["map_cell_index"]] = int(completed)
                prepared["hero_templates"][(world, acted, completed)] = _hero_templates(prepared, state)
    fragments = (421, 422) if prepared["chest"]["terrain_flags"] & 0x20 else (428, 429)
    prepared["pages"] = message_pages(original, assets, [
        {"fragment": fragments[0], "open": True, "choice": True},
        {"fragment": fragments[1], "y": 146, "key": True, "item": prepared["chest"]["item_id"]}])
    prepared["bank"] = natural.cursor_bank(original, source, assets)
    prepared["actions"] = []
    state = source
    for ordinal, destination in enumerate((prepared["chest"]["world"], manifest["approach_world"], prepared["chest"]["world"])):
        if sum(abs(a - z) for a, z in zip(oracle.actor(state, 0)[:2], destination)) != 1:
            raise ValueError("三箱各次移动必须是已确认的非零一步")
        probe = natural.range_probe(original, state, assets, 0)
        if not probe: raise ValueError("三箱无独立移动范围资源正门")
        summary = natural.selection_summary(original, state, assets, 0)
        prepared["actions"].append({"destination": tuple(destination), "summary": summary, "probe": probe})
        steps = natural.navigation(state, destination); state = bytearray(steps[-1][1])
        state[UNITS:UNITS + 2] = bytes(destination)
        state = _after_action(prepared, state, ordinal)
        inventory_templates(original, oracle.actor(state, 0), assets)
        state = _after_end(state)
    return prepared


def _quiet_hero(prepared, state):
    start = None
    def match(frame):
        nonlocal start
        result = hero_focus(frame, prepared, state)
        if not result: start = None; return None
        now = time.monotonic()
        if start is None: start = now
        return dict(result, host_resource_quiet_seconds=2.0, native_timing_claimed=False) if now - start >= 2 else None
    return match


def _navigate(capture, prepared, state, destination, cp, name):
    for index, (key, step) in enumerate(natural.navigation(state, destination)):
        capture.key(key)
        cp(f"{name}-{index}", lambda frame, s=step: natural.cursor_match(frame, prepared["assets"], prepared["bank"], screen(s, s[META + 5:META + 7])))
        state = step
    return state


def _defeat(capture, prepared, save, guard, record, cp):
    source, assets = prepared["source"], prepared["assets"]
    branch = lambda frame: result_branch(frame, "defeat", assets, prepared["result"])
    _, seen = cp("b7-defeat-continued", lambda f: branch(f) or natural.focus_match(f, prepared["original"], source, assets), timeout=90)
    if seen["kind"] != "native_defeat_resource":
        for index, (key, state) in enumerate(prepared["defeat_navigation"]):
            capture.key(key)
            _, seen = cp(f"b7-defeat-to-hero-{index}", lambda f, s=state: branch(f) or natural.focus_match(f, prepared["original"], s, assets))
            source = state
            if seen["kind"] == "native_defeat_resource": break
        if seen["kind"] != "native_defeat_resource":
            summary, probes = prepared["defeat_summary"], prepared["defeat_probes"]
            capture.key("ENTER")
            _, seen = cp("b7-defeat-selection", lambda f: branch(f) or (
                {"kind": "b7_defeat_selection", "summary": hit, "range": probe}
                if (hit := exact_tile(f, summary["image"], summary["position"])) and (probe := natural.range_match(f, probes)) else None))
            if seen["kind"] != "native_defeat_resource":
                capture.key("ESC"); _, seen = cp("b7-defeat-controller-result", branch, timeout=90)
    frame, final = cp("b7-defeat-game-over", lambda f: hit if (hit := branch(f)) and hit["frame"] == 1 else None,
        timeout=30, allow_low_color_resource=True)
    path = capture.artifacts / "b7-defeat-game-over.png"
    record["result_capture"] = {"path": str(path.resolve()), "sha256": sha(path.read_bytes()),
        "size": path.stat().st_size, "mtime_ns": path.stat().st_mtime_ns}
    record["defeat_measurement"] = final
    capture.wait(prepared["titles"][3][0], blank_transition=True, timeout=45)
    cp("b7-defeat-returned-title", lambda f: {"kind": "full_title_resource", **hit}
        if (hit := exact_tile(f, prepared["titles"][3][0], (0, 0))) else None)
    require_source_unchanged(save, *guard)
    record["business_sequence_completed"] = True


def exercise(capture, prepared, save, guard, record):
    """每次实际键后须资源 ACK；首错冒泡交 central 正常关闭/晚守卫。"""
    def cp(name, predicate, **options):
        answer = wait_state(capture, name, predicate, record["checkpoints"], **options)
        require_source_unchanged(save, *guard)
        return answer
    wait_title(capture, prepared["titles"][3][0], timeout=90)
    require_source_unchanged(save, *guard)
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); require_source_unchanged(save, *guard)
    capture.key("ENTER")
    if prepared["scenario"] == "villager-defeat":
        return _defeat(capture, prepared, save, guard, record, cp)
    source, assets, original = prepared["source"], prepared["assets"], prepared["original"]
    state = source
    cp("b7-chest-continued", lambda f: hero_focus(f, prepared, state), timeout=90)
    record["chest_actions"] = []
    for ordinal, action in enumerate(prepared["actions"]):
        prefix = f"b7-chest-{ordinal}"
        summary = action["summary"]
        capture.key("ENTER")
        cp(prefix + "-selection", lambda f: {"kind": "b7_chest_selection", "unit_summary": hit, "range": probe}
            if (hit := exact_tile(f, summary["image"], summary["position"])) and (probe := natural.range_match(f, action["probe"])) else None)
        state = _navigate(capture, prepared, state, action["destination"], cp, prefix + "-move-destination")
        cp(prefix + "-destination-cursor", lambda f: natural.cursor_match(f, assets, prepared["bank"], screen(state, action["destination"])))
        capture.key("ENTER")
        state = bytearray(state); state[UNITS:UNITS + 2] = bytes(action["destination"])
        cp(prefix + "-command", lambda f: natural.command_match(f, original, state, assets, 0, None))
        capture.key("DOWN")
        wait_at = screen(state, action["destination"]); wait_at = (wait_at[0], wait_at[1] + 24)
        geometry = natural.visible_resource(assets["commands"][10], wait_at)
        if geometry is not None and geometry[2] == (0, 0, 24, 20):
            cp(prefix + "-wait-selected", lambda f: natural.command_match(f, original, state, assets, 0, 3))
        else:
            # 177fc 的 DOWN 是绝对索引3；此处只核可见图标，绝不声称已看到屏外亮帧。
            # 首轮后继421/422和次轮acted+完整库存/Save才是业务完成的正证。
            def bottom_wait(frame):
                hit = natural.command_match(frame, original, state, assets, 0, None)
                if not hit or hit["matches"][3]["observable"] or any(
                        row.get("observable") and row["frame"] % 3 == 1 for row in hit["matches"]): return None
                return dict(hit, kind="b7_bottom_wait_request", selected=None,
                    wait_highlight_observed=False, requested_command_index=3,
                    static_key_mapping="177fc DOWN selects3", requires_downstream_business_witness=True)
            cp(prefix + "-bottom-wait-request", bottom_wait)
        capture.key("ENTER")
        if ordinal == 0:
            for page in prepared["pages"]:
                cp(prefix + f"-message-{page['fragment']}", lambda f, p=page: message_match(f, p, assets), timeout=90)
                capture.key("ENTER")
        state = _after_action(prepared, state, ordinal)
        cp(prefix + "-acted-closed", _quiet_hero(prepared, state), timeout=120)
        # 完整详情页同时证明首份物品和第二次没有追加；不单以闭框推测业务。
        _, templates, fields = inventory_templates(original, oracle.actor(state, 0), assets)
        fields["require_word_bars"] = True
        capture.key("F2"); cp(prefix + "-inventory", lambda f: inventory_detail(f, templates, fields))
        capture.key("ESC"); cp(prefix + "-detail-closed", _quiet_hero(prepared, state), timeout=90)
        empty = natural.empty_tile(original, state, assets, capture.last)
        state = _navigate(capture, prepared, state, empty["world"], cp, prefix + "-end-empty")
        menu = RoundOracle(assets, screen(state, empty["world"]))
        cp(prefix + "-end-empty-proven", lambda f: {"kind": "b7_static_empty", **hit}
            if (hit := exact_tile(f, empty["focused"], menu.focus)) else None)
        capture.key("ENTER"); cp(prefix + "-end-parent", lambda f: menu.menu(f, 0))
        capture.key("DOWN"); cp(prefix + "-end-selected", lambda f: menu.menu(f, 3))
        capture.key("ENTER"); cp(prefix + "-end-yes", lambda f: menu.buttons(f, 0))
        capture.key("ENTER"); state = _after_end(state)
        cp(prefix + "-next-round", _quiet_hero(prepared, state), timeout=120)
        record["chest_actions"].append({"ordinal": ordinal, "item": prepared["chest"]["item_id"],
            "destination": list(action["destination"]), "completed_before": bool(ordinal),
            "at_chest": tuple(action["destination"]) == tuple(prepared["chest"]["world"]),
            "append_count": int(ordinal == 0), "end_confirmations": 1})
    empty = natural.empty_tile(original, state, assets, capture.last)
    state = _navigate(capture, prepared, state, empty["world"], cp, "b7-chest-save-empty")
    menu = RoundOracle(assets, screen(state, empty["world"]))
    cp("b7-chest-save-empty-proven", lambda f: {"kind": "b7_static_empty", **hit}
        if (hit := exact_tile(f, empty["focused"], menu.focus)) else None)
    capture.key("ENTER"); cp("b7-chest-save-parent", lambda f: menu.menu(f, 0))
    capture.key("ENTER"); cp("b7-chest-save-secondary", lambda f: menu.secondary(f, 0))
    capture.key("LEFT"); cp("b7-chest-save-selected", lambda f: menu.secondary(f, 1))
    capture.key("ENTER"); cp("b7-chest-save-yes", lambda f: menu.buttons(f, 0))
    capture.key("ENTER"); measured = wait_measured_save(save, *guard)
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    snapshot = capture.artifacts / "b7-chest-completed.SAV"; snapshot.write_bytes(guard[0])
    record["completed_save"] = {"snapshot": str(snapshot), "sha256": sha(guard[0]), "mtime_ns": guard[1]}
    record["completed_save"]["measurement"] = check_chest_save(prepared, source, measured, oracle.checksum(state))
    cp("b7-chest-save-returned-browse", menu.closed_browse)
    record["business_sequence_completed"] = True
