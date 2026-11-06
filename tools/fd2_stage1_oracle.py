"""B5 stage1新增事件／私有败北的独立原版数据模型，不运行SDL或窗口。

依据field_stage1_turn3_execute @0x34778（原sub_34778）、
field_stage1_battle_result_check @0x206c5（原sub_206C5）、
field_actor_group_append @0x10b4e和field_movement_script_play @0x1366a。
真实C10入场SAV为只读来源；既有AI04隔离和六人hidden派生逐byte登记。
"""
import hashlib
import struct

from fd2_item_list_oracle import EXE_SHA256, item_table
from fd2_opening_oracle import build_pages
from fd2_transition_oracle import (derive_prepare_fixture, entry_geometry,
    entry_initialized_records, check_entry_measurement)
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image
from dump_fd2_fixup_table import collect_internal_offset_fixups
from run_original_comparison import archive_entry
from run_original_end_turn import META, UNITS, STRIDE
from run_original_item_list import drop_derived
from run_graphics_tests import decode_save

SCENARIOS = ("turn3", "villager-defeat")
NEW_UNKNOWN = tuple(range(0x28, 0x31))


def checksum(plain):
    if len(plain) != 0x59cb:
        raise ValueError("stage1完整容器长度错误")
    result = bytearray(plain)
    result[-4:] = sum(result[:-4]).to_bytes(4, "little")
    return bytes(result)


def validate_entry_source(original, source):
    """接受本次真实入场Save；只锁已证资源/构造写集，不锁native heap整档SHA。"""
    if source[META:META + 3] != bytes((1, 21, 1)) or source[:3] != bytes((1, 5, 40)):
        raise ValueError("stage1工具需受控准备页的turn1/21actor真实入场Save，不是通用加载器")
    _, _, state, _ = derive_prepare_fixture(original)
    geometry = entry_geometry(original, state)
    initialized = entry_initialized_records(original, state, geometry)
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata = archive_entry(field, 4)
    width, height = struct.unpack_from("<HH", archive_entry(field, 3))
    camera = list(source[META + 3:META + 5])
    if not 0 <= camera[0] <= width - 13 or not 0 <= camera[1] <= height - 8:
        raise ValueError("stage1来源镜头越界")
    # unknown区以本次source为owner；复用entry checker只校逐record已证初始化写集。
    checked = check_entry_measurement(source, source, state, geometry,
        {"inferred_camera": camera}, metadata, initialized)
    return {"kind": "resource_proven_controlled_stage1_entry", "checked": checked,
        "whole_source_sha_pinned": False, "source_unknown_bytes_owned_and_preserved": True,
        "constructor_known_bytes": [sum(mask) for _, mask in initialized]}


def derive_fixture(fixture, scenario, *, original):
    if scenario not in SCENARIOS:
        raise ValueError("未知stage1场景")
    encoded = fixture.read_bytes()
    source = decode_save(fixture)
    validation = validate_entry_source(original, source)
    changed = bytearray(source)
    targets = range(5, 21) if scenario == "turn3" else range(5, 11)
    bit = 4 if scenario == "turn3" else 1
    for raw in targets:
        if source[UNITS + raw * STRIDE + 6] == 2:
            raise ValueError("stage1受控非玩家范围出现玩家记录")
        changed[UNITS + raw * STRIDE + 5] |= bit
    changed = checksum(changed)
    output = bytes(a ^ b ^ c for a, b, c in zip(encoded, source, changed))
    patches = [{"offset": i, "before": a, "after": b,
                "owner": "checksum" if i >= len(source) - 4 else "active flags"}
               for i, (a, b) in enumerate(zip(source, changed)) if a != b]
    allowed = {UNITS + raw * STRIDE + 5 for raw in targets} | set(range(len(source) - 4, len(source)))
    if any(row["offset"] not in allowed for row in patches):
        raise AssertionError("stage1派生写入了未知字段")
    return output, changed, {"scenario": scenario, "source_sha256": hashlib.sha256(encoded).hexdigest(),
        "source_path": str(fixture.resolve()), "source_mtime_ns": fixture.stat().st_mtime_ns,
        "source_validation": validation,
        "source_plain_sha256": hashlib.sha256(source).hexdigest(),
        "derived_sha256": hashlib.sha256(output).hexdigest(),
        "derived_plain_sha256": hashlib.sha256(changed).hexdigest(), "plaintext_patches": patches,
        "encrypted_patches": [{"offset": i, "before": a, "after": b}
            for i, (a, b) in enumerate(zip(encoded, output)) if a != b],
        "controlled_fixture": True, "natural_stage1_battle": False,
        "flag_bit": bit, "changed_raw_actors": list(targets),
        "all_other_plaintext_bytes_preserved": True}


def stage_tables(original):
    data = (original / "FD2.EXE").read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("stage1原始EXE身份不符")
    le = parse_le(data)
    raw = build_relbase_image(data, le, parse_objects(data, le))
    field = (original / "FDFIELD.DAT").read_bytes()
    metadata, placement = archive_entry(field, 4), archive_entry(field, 5)
    if len(metadata) != 1171 or metadata[:6] != bytes((1, 5, 40, 3, 6, 1)):
        raise ValueError("stage1事件必须是turn3/action6/side1；不能改为turn3可Save")
    rows = [metadata[131 + i * 26:157 + i * 26] for i in range(40)]
    points = [struct.unpack_from("<HH", placement, 2 + 6 * i) for i in range(45)]
    group = [(i, row) for i, row in enumerate(rows) if row[21] == 3]
    if len(group) != 6 or any(row[1] != 96 or row[4] != 3 or row[5:7] != bytes((0, 128)) for _, row in group):
        raise ValueError("script13新增六人不是已确认的真实group3")
    obj, _, address = collect_internal_offset_fixups(original / "FD2.EXE")[0x627d8 + 13 * 4]
    if obj != 3 or address != 0x62ad1:
        raise ValueError("script13严格LE指针不符")
    positions, directions = [list(points[i]) for i, _ in group], [0] * 6
    cursor = address + 1
    for _ in range(raw[address]):
        mode, count = raw[cursor:cursor + 2]; cursor += 2
        for _ in range(count):
            actor, direction = raw[cursor:cursor + 2]; cursor += 2
            if not 21 <= actor < 27 or direction > 3:
                raise ValueError("script13引用未确认actor／方向")
            directions[actor - 21] = direction
            if mode < 128:
                dx, dy = ((0, 1), (-1, 0), (0, -1), (1, 0))[direction]
                positions[actor - 21][0] += mode * dx
                positions[actor - 21][1] += mode * dy
    items, _ = item_table(original / "FD2.EXE")
    records = []
    for index, (_, row) in enumerate(group):
        base = raw[0x61af9 + 10 * (row[1] - 68):0x61af9 + 10 * (row[1] - 67)]
        unit = bytearray(80)
        unit[:2] = bytes(positions[index]); unit[3] = directions[index]
        unit[5:10] = bytes((0, row[0], row[1], row[1], 0))
        unit[10:14] = bytes((0x40, row[5], 0x40, row[6]))
        for slot in range(6):
            unit[14 + 2 * slot:16 + 2 * slot] = bytes((0x80 if row[7 + slot] == 255 else 0, row[7 + slot]))
        unit[0x1a:0x1f] = row[13:17] + bytes(1)
        unit[0x1f:0x22] = bytes((base[0], base[1], row[4]))
        unit[0x31:0x34] = row[22:25]; unit[0x34:0x37] = row[17:20]
        struct.pack_into("<H", unit, 0x37, base[5] * row[4])
        struct.pack_into("<H", unit, 0x39, base[6] * row[4])
        unit[0x3b:0x3e] = bytes((base[8], 255, row[2]))
        struct.pack_into("<H", unit, 0x3e, base[7] * row[4])
        hp, mp = struct.unpack_from("<H", base, 2)[0] * row[4], base[4] * row[4]
        struct.pack_into("<4H", unit, 0x40, hp, hp, mp, mp)
        struct.pack_into("<4H", unit, 0x48, *drop_derived(unit, items))
        records.append(bytes(unit))
    return {"metadata": metadata, "records": records, "script13_positions": positions,
        "script13_directions": directions, "script13_sha256": hashlib.sha256(raw[address:cursor]).hexdigest(),
        "constructor_unknown_offsets": list(NEW_UNKNOWN)}


def turn3_pages(original, assets, source, tables):
    identities = [source[UNITS + i * STRIDE + 7] for i in range(21)] + [96] * 6
    mapping = {source[UNITS + i * STRIDE + 8]: source[UNITS + i * STRIDE + 7] for i in range(21)}
    pages = build_pages(original, assets, stage_plan=((1, 5),),
        identities_by_stage={1: identities}, text_identities_by_stage={1: mapping})
    return [page for page in pages if page["fragment"] == 4]


def check_measurement(source, measured, tables, observation):
    """原21记录/所有未知区严格保留；新构造洞仅列确切offset，几何另双端比较。"""
    if len(measured) != 0x59cb or checksum(measured) != measured:
        raise AssertionError("stage1实测SAV容器校验失败")
    if measured[META:META + 3] != bytes((4, 27, 1)):
        raise AssertionError("script13后必须合法完成到turn4/27actor/stage1")
    expected = bytearray(source)
    expected[META:META + 3] = bytes((4, 27, 1))
    camera, focus = observation["camera"], observation["measuring_focus"]
    expected[META + 3:META + 9] = bytes((*camera, *focus, focus[0] - camera[0], focus[1] - camera[1]))
    for raw in range(5, 11): expected[UNITS + raw * STRIDE + 0x35:UNITS + raw * STRIDE + 0x37] = bytes((26, 15))
    for index, record in enumerate(tables["records"]):
        at = UNITS + (21 + index) * STRIDE
        wanted = bytearray(record)
        # 真正phase0正常执行后的XY/方向由原版与SDL各自实测并精确配对，非捕获默认。
        for offset in (0, 1, 2, 3, *NEW_UNKNOWN): wanted[offset] = measured[at + offset]
        if measured[at + 4] != 0 or measured[at + 5] != 0:
            raise AssertionError("新增group3未完成真实phase尾／清acted")
        expected[at:at + STRIDE] = wanted
    expected = checksum(expected)
    mismatch = [i for i, (a, b) in enumerate(zip(expected, measured)) if a != b]
    if mismatch:
        raise AssertionError(f"stage1改写未知区或不符已确认写集：{mismatch[:20]}")
    return {"stage": 1, "turn": 4, "active_count": 27,
        "camera": list(camera), "measuring_focus": list(focus),
        "new_actors": [{"raw": raw, "position": list(measured[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2]),
            "direction": measured[UNITS + raw * STRIDE + 3]} for raw in range(21, 27)],
        "exact_preserved_existing_records": 21, "villager_anchor": [26, 15],
        "new_constructor_unknown_offsets": list(NEW_UNKNOWN), "whole_container_checked_with_named_exceptions": True}
