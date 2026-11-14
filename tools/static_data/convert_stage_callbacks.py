#!/usr/bin/env python3
"""S2 callback 冷图转换；原90槽、完整Domain owner与显式入口分别保留。

FD2.EXE canonical 0x51b91 为90槽独立callback表。只生成有来源的候选图，
不修改正式绑定、支持门或原资源；未知CFG不降成空图。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import audit_stage_data as audit
from verify_unit_tables import source_range

IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def byte_guard(raw):
    value = 14695981039346656037
    for byte in raw: value = (value ^ byte) * 1099511628211 & 0xffffffffffffffff
    return f"{value:016x}"


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n"


def collect_domain(db, original, output, strict_image):
    """由技能driver调用；完整owner含shared chunk，入口不强造新函数。"""
    sys.path.insert(0, str(ROOT / "tools/ida"))
    from g1_preparation_evidence import function_record
    original, output = Path(original), Path(output)
    image = Path(strict_image).read_bytes()
    exe = (original / "FD2.EXE").read_bytes()
    if digest(image) != IMAGE_SHA or digest(exe) != audit.EXE_SHA256:
        raise ValueError("原EXE/strict relocated镜像身份不同")
    fixups = audit.collect_internal_offset_fixups(original / "FD2.EXE")
    slots = []
    owners = {}
    for slot in range(90):
        target = fixups[0x51b91 + slot * 4][2]
        function = db.functions.get_at(target)
        if function is None:
            raise ValueError(f"callback{slot}没有Domain owner")
        owners[function.start_ea] = function
        slots.append({"source_id": f"dispatch:event_callback:{slot:03d}",
                      "slot": slot, "target": target, "owner": function.start_ea,
                      "entry_kind": "function_entry" if target == function.start_ea else "explicit_shared_chunk_entry"})
    # 原语/共享helper的范围独立于候选图；不能从邻近CALL拼接剧情。
    for va in (0x10b4e, 0x112a5, 0x11df2, 0x134e4, 0x13512,
               0x34894, 0x344f2, 0x35b78, 0x35f10, 0x361b0,
               0x33f78, 0x22253, 0x17aa9, 0x3790a, 0x35f78):
        function = db.functions.get_at(va)
        if function is None or function.start_ea != va:
            raise ValueError(f"helper不是完整入口: {va:#x}")
        owners[va] = function
    functions = []
    total = 0
    for va in sorted(owners):
        row = function_record(db, va)
        row["instructions"] = []
        # Domain functions.get_instructions只遍历主块；显式按全部chunks读取shared tail。
        instructions = [instruction for chunk in row["chunks"]
                        for instruction in db.instructions.get_between(chunk["start"], chunk["end"])]
        for instruction in instructions:
            raw = bytes(db.bytes.get_bytes_at(instruction.ea, size=instruction.size))
            if raw != image[instruction.ea:instruction.ea + instruction.size]:
                raise ValueError(f"Domain/strict原指令不等: {instruction.ea:#x}")
            row["instructions"].append({"ea": instruction.ea, "raw_hex": raw.hex(),
                                        "disasm": db.instructions.get_disassembly(instruction)})
        if sum(len(bytes.fromhex(i["raw_hex"])) for i in row["instructions"]) != sum(c["end"] - c["start"] for c in row["chunks"]):
            raise ValueError(f"Domain指令未完整覆盖所有chunk: {va:#x}")
        for chunk in row["chunks"]:
            if bytes.fromhex(chunk["raw_hex"]) != image[chunk["start"]:chunk["end"]]:
                raise ValueError("Domain/strict chunk不等")
            total += chunk["end"] - chunk["start"]
        row["target_xrefs"] = [{"from": x.from_ea, "to": x.to_ea, "type": x.type.name,
                                "is_call": x.is_call, "is_jump": x.is_jump}
                               for x in db.xrefs.to_ea(va) if not x.is_flow]
        functions.append(row)
    report = {"schema": "fd2-s2-callback-domain-v1", "database_saved": False,
              "address_view": "canonical relbase VA; strict loader-relocated bytes",
              "exe_sha256": digest(exe), "strict_image_sha256": digest(image),
              "slots": slots, "functions": functions, "function_bytes_sum": total,
              "scope": "complete owner functions plus explicit shared-chunk entries; not automatic CFG semantics",
              "original_machine_execution_claimed": False}
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise ValueError("不覆盖旧Domain收据")
    output.write_text(encode(report), encoding="utf8")
    return {"callbacks": len(slots), "owners_and_helpers": len(functions), "bytes_sum": total}


# 这里只定义规范节点；期望轨迹来自下方真正原指令，不以这些模板自证。
def action(op, *args):
    return (op, list(args))


def branch(op, args, yes, no=()):
    return {"op": op, "args": list(args), "yes": list(yes), "no": list(no)}


def cold_flow(identity, program):
    nodes = [{"id": "done", "op": "end", "args": [], "on": {}},
             {"id": "failed", "op": "fail", "args": [], "on": {}}]

    def compile_sequence(sequence, following):
        for item in reversed(sequence):
            label = f"n{len(nodes):04d}"
            if isinstance(item, dict):
                yes = compile_sequence(item["yes"], following)
                no = compile_sequence(item["no"], following)
                node = {"id": label, "op": item["op"], "args": item["args"],
                        "on": {"yes": yes, "no": no, "error": "failed"}}
            else:
                op, args = item
                node = {"id": label, "op": op, "args": args,
                        "on": {"ack" if op in ("story", "story_source", "wait_key", "loot_child") else "next": following,
                               "error": "failed"}}
            # 分支递归先增加子节点，入口名必须在递归后取唯一值。
            node["id"] = f"n{len(nodes):04d}"
            following = node["id"]
            nodes.append(node)
        return following

    entry = compile_sequence(program, "done")
    if not program:
        nodes = nodes[:1]
    return {"id": identity, "entry": entry, "nodes": list(reversed(nodes))}


FIELD_OFFSETS = (0, 1, 3, 4, 5, 6, 7, 8, 0x22, 0x23, 0x24,
                 0x25, 0x26, 0x27, 0x31, 0x34, 0x35, 0x36, 0x40, 0x44)

EXISTING = {**{i: f"common.legacy.action.{i:03d}" for i in (0, 1, 2, 3, 4, 6)},
            8: "common.inventory.once", 12: "common.state.once",
            39: "common.loot.child.0",
            53: "common.defeat.tail", 55: "common.loot.physical", 58: "common.cell.grant",
            61: "common.key.consume", 69: "common.turn.relative", 79: "common.random.pair"}
EXISTING_FLOW_SHA = {
    0: "2e5862b063679a4bff063170ce0bcd8fa257f9df2501e48f6de069e906d8db3a",
    1: "05cea2cdd1e480e6a2eef016fa76c51ebf6ff292234b62c788888c9ebf32d92f",
    2: "16ec45f1ff1881417fc72b59f453281768d38e2e985570547110e35095680f5a",
    3: "e83d7577fb80ee1c021cbeae126b2c04a2558d89c1934e7dc61d08a6e269130f",
    4: "1f2a99bbb6a0f85c174182bb989b9298d8d0fbad4cbc475fa36f64edae2d269d",
    6: "465f3419edddb141f944dcdcaeeccbf62934eb6e503f3acf231e0de6b096c42d",
    8: "95e4f50ed02bc6f2faeca70fcdca6dc23be95052c517fde7c70f4cbf71d19ee4",
    12: "ac9391d34a8e8da80934ed8b9111206167d17a5f96ba95b52e3d69bdd9595a42",
    39: "70a7b2593c9b8652032e428fa077d62c999309ddbfd7c824b4fef63564d951bd",
    53: "283c38a31e5d29a9a51aaca04e22ed644cc50d043c48231e136367139ff8d96a",
    55: "d7379705aa3f12def4ee308f45ef4acc62714b79eae83b97b5a7fcd4c7353a5c",
    58: "bebe3ab7947ce6048708db3060f79e50f9f4d6478473c843458305cf88b4594e",
    61: "fa2288e394f7fa0abeb98f42fb580666b243a12b6dee81021e201d9462835f08",
    69: "7e1467d572d843e0cf8b3d505e2598ee0f3e895c83d2033d36db75db1a6275dc",
    79: "3a306d4510b324cc7282ba703724f3e83653a0ad7276dc4a6ca68900f2945f65",
}


def programs():
    """按完整原callback及显式共享尾审查的有限组合；不按关卡分C handler。"""
    a = action
    story = lambda n: a("story", n)
    group = lambda n, force=0: a("append_group", n, force)
    gsource = lambda n: a("append_group_source", n, 0)
    ai = lambda first, last, value: a("unit_ai_write", first, last, value)
    write = lambda first, last, field, value, op=0: a("unit_field_write", first, last, field, op, value)
    state = lambda index, value, add=0: a("event_byte_write", index, index, value, add)
    turn = lambda slot, delta: a("turn_relative", slot, delta)
    gate = lambda index, value, yes, no=(): branch("branch_event_byte", [index, 0, value], yes, no)
    caller_side = lambda yes: branch("branch_actor", [-1, 1, 1, 0], yes)
    directions = [a("directions_reset"), a("delay_ms", 20)]
    def flash(x, y, value=None, source=None):
        return [a("pan", x, y), group(value) if source is None else gsource(source),
                a("delay_ms", 300), a("palette_add", 0, 255, 255), a("delay_ms", 200),
                a("palette_add", 0, 255, 0), a("refresh_map", 0), a("delay_ms", 400)]
    def pulse():
        sequence = []
        for level in range(64): sequence += [a("palette_add", 0, 255, level), a("delay_ms", 8)]
        sequence += [a("delay_ms", 400)]
        for level in range(62, -1, -1): sequence += [a("palette_add", 0, 255, level), a("delay_ms", 8)]
        return sequence
    p = {
        5: [group(1), story(1)],
        7: [a("pan", 27, 5), group(2, 1), a("move", 46, 0), *directions, story(8)],
        9: [branch("branch_actor", [6, 0, 0, 0], [group(2), a("pan", 3, 0), a("delay_ms", 800), a("pan", 3, 17), a("delay_ms", 200), story(4)])],
        10: [gate(16, 0, [ai(16, 71, 0), story(1), state(16, 1)])],
        11: [group(2), story(2)],
        13: [story(6), write(64, 73, 16, 0), ai(64, 73, 3), ai(35, 49, 0)],
        14: [ai(37, 40, 0), ai(13, 24, 0), story(3)],
        15: [a("display_mode", 0), group(2, 1), a("pan", 14, 0), a("move", 23, 0), *directions, ai(7, 12, 0), ai(33, 35, 0), story(4)],
        16: [group(3), story(5)],
        17: [ai(48, 51, 7), story(6), a("move", 24, 0), story(7)],
        18: [story(8), ai(16, 34, 0)],
        19: [ai(7, 36, 7), story(8), branch("branch_hidden_count", [7, 36, 2, 30], [story(11)])],
        20: [story(1)],
        21: [branch("branch_actor", [8, 0, 0, 0], [story(2)])],
        22: [branch("branch_actor", [8, 0, 0, 0], [group(1), story(3)])],
        23: [ai(8, 28, 0), story(4), branch("branch_turn", [2, 15], [group(2), a("pan", 5, 17), a("move", 25, 0), story(5), a("pan", 5, 17), a("move", 26, 0), write(33, 33, 4, 1, 2), a("display_mode", 1)])],
        24: [story(3)],
        25: [gate(16, 1, [group(2, 1), a("pan", 16, 10), a("move", 30, 0), story(2), state(17, 1)])],
        26: [caller_side([ai(9, 27, 0), state(16, 1)])],
        27: [a("pan", 8, 2), a("delay_ms", 100), gsource(0), a("delay_ms", 100)],
        28: [write(10, 27, 15, 128, 1)],
        29: [story(2), write(10, 27, 15, 128, 1)],
        30: [write(12, 33, 15, 0), turn(0, 1), turn(1, 2),
             write(11, 11, 4, 0), write(11, 11, 5, 1), write(11, 11, 6, 6),
             write(11, 11, 7, 6), write(11, 11, 14, 255), write(11, 11, 15, 128),
             write(11, 11, 18, 1), story(2), group(1), story(3), a("contribution_clear"), state(16, 2)],
        31: [gsource(5), state(16, 1, 1), a("pan", 0, 0), a("delay_ms", 200), a("pan", 12, 0), a("delay_ms", 200), a("pan", 12, 11), a("delay_ms", 200), a("pan", 0, 11), a("delay_ms", 200)],
        32: [group(1), story(1)],
        33: [story(2), write(12, 13, 15, 0)],
        34: [story(3)],
        35: [a("pan", 12, 5), group(2, 1), a("move", 42, 0), *directions],
        36: [write(14, 14, 15, 131)],
        37: [story(1), a("pan", 15, 34), group(3, 1), a("move", 43, 0), *directions,
             a("pan", 0, 26), group(4, 1), a("move", 44, 0), *directions, a("display_mode", 1)],
        38: [group(1), story(10)],
        40: [group(2), a("pan", 17, 37), story(1)],
        42: [group(1), story(6)],
        43: [ai(16, 16, 3)],
        44: [ai(29, 59, 3)],
        45: [ai(16, 31, 3)],
        46: [group(1), story(1), a("roster_append", 27)],
        47: [gsource(2), a("delay_ticks", 1), a("pan", 0, 0), a("delay_ticks", 8),
             a("pan", 28, 0), a("delay_ticks", 8), a("pan", 28, 32), a("delay_ticks", 8),
             a("pan", 0, 32), a("delay_ticks", 8), branch("branch_turn", [0, 2], [story(3)])],
        48: [ai(35, 42, 3), ai(67, 74, 3)],
        49: [gsource(2), a("pan", 32, 35), a("delay_ticks", 8), a("pan", 0, 35), a("delay_ticks", 8), branch("branch_turn", [0, 3], [story(1)])],
        50: [group(2), a("pan", 16, 42), a("delay_ticks", 8), a("roster_append", 20), story(2)],
        # 355f0@35631 push3→3530d；不经过352ca自身push11，旧G child.2不改。
        51: [a('loot_child',2,-1),story(3)],
        52: [*flash(2, 11, source=3), *flash(26, 11, source=4)],
        54: [gsource(0), a("pan", 0, 4), a("delay_ms", 400), a("pan", 0, 22), a("delay_ms", 400), a("pan", 26, 24), a("delay_ms", 400), a("pan", 26, 2), a("delay_ms", 400)],
        56: [a("pan", 6, 40), group(1), a("move", 74, 0), story(5), *directions],
        57: [gsource(0), a("pan", 9, 0), a("delay_ms", 400)],
        59: [caller_side([ai(39, 44, 0)])],
        60: [caller_side([ai(23, 24, 0), ai(53, 56, 0)])],
        62: [gate(17, 0, [turn(0, 1), state(17, 1)])],
        63: [*flash(3, 27, 1), *flash(15, 27, 2)],
        64: [gate(16, 1, [story(1), *flash(9, 44, 3), *flash(0, 9, 4), *flash(17, 9, 5), a("display_mode", 1)],
                        [gate(16, 2, [story(2), a("unit_hp_zero_tail", 16), a("finalize_defeated")])]), state(16, 1, 1)],
        65: [gate(16, 0, [turn(0, 0), state(16, 1)])],
        66: [story(3), *flash(17, 18, 1), story(6)],
        67: [turn(1, 0)],
        68: [story(4), *flash(14, 7, 2), story(6), state(18, 1)],
        70: [ai(41, 45, 0), story(5), *flash(8, 7, 3), *flash(4, 7, 4), *flash(0, 7, 5), story(6)],
        71: [branch("branch_event_byte", [19, 1, 0], [story(2), a("unit_hp_zero_tail", 20), a("finalize_defeated")]), state(19, 1, 1)],
        72: [*flash(4, 35, 2), *flash(14, 35, 3), a("display_mode", 1)],
        73: [state(18, 1)],
        74: [*flash(10, 29, source=5), branch("branch_event_byte", [16, 1, 7], [turn(0, 1)]), state(16, 1, 1)],
        75: [caller_side([gate(17, 0, [branch("branch_actor", [-1, 2, 0, 9],
             [story(1), state(17, 1), turn(1, 1), state(16, 4), turn(0, 0)],
             [a("notice_actor", -1), a("message", 0, 1, 95, 119), a("portrait_frame", 0), a("wait_key", 0), a("close_box")])])])],
        76: [gate(17, 4, [story(2), group(1), a("event_byte_from_count", 21, 3), turn(2, 0),
                         *pulse(), a("delay_ms", 400), *pulse(), a("delay_ms", 400),
                         *[item for fragment in range(3, 7) for item in [*pulse(), story(fragment)]]],
                        [write(1, 1, 4, 128, 2), state(17, 1, 1), turn(1, 1)])],
        77: [state(19, 1)],
        78: [state(20, 1)],
        80: [ai(20, 20, 11)],
        81: [state(16, 1, 1), turn(0, 1)],
        83: [story(8), a("unit_hp_zero_tail", 20), a("finalize_defeated")],
        84: [ai(16, 79, 0)],
        **{i: [] for i in range(85, 90)},
    }
    # 35346在353ae先push4再跳3530d，未执行352ca的push11。
    # 旧G机制图保原身份；新caller必须使用自己的完整共享尾参数。
    p[41] = [story(3), a("loot_child", 1, -1), story(4)]
    if set(p) | set(EXISTING) | {82} != set(range(90)):
        raise ValueError("callback转换去向不完整")
    return p


def source_trace(image, domain, slot, case, groups):
    """真x86 caller与写点oracle；已证领域callee明确stub，不声称整游戏执行。"""
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(0x100000, 0x20000)
    cpu.mem_map(0x200000, 0x10000)
    units, state, metadata, stack, stop = 0x100000, 0x102000, 0x103000, 0x208000, 0x400000
    result, visited, unsupported, branch_edges, checkpoints = [], set(), [], set(), []
    pending_branch = [None]
    prefix_guard = [None]
    instructions = {i["ea"]: i for f in domain["functions"] for i in f["instructions"]}
    def word(at): return struct.unpack("<I", cpu.mem_read(at, 4))[0]
    def put(at, value): cpu.mem_write(at, struct.pack("<I", value & 0xffffffff))
    records = bytearray(case["units"])
    cpu.mem_write(units, bytes(records)); cpu.mem_write(state, bytes(case["state"]))
    cpu.mem_write(metadata, bytes(case["metadata"]))
    for at, value in ((0x53a45, units), (0x53ad5, state), (0x53a55, metadata),
                      (0x53bef, case["turn"]), (0x53beb, case["count"]),
                      (0x53a79, 0x11111111), (0x53a7d, 0x22222222),
                      (0x53bfb, 0), (0x51a83, case["display"]), (0x53ec8, case["contribution"])):
        put(at, value)
    cpu.mem_write(0x53afa, b"\0"); put(stack, stop); put(stack + 4, case["caller"])
    cpu.reg_write(UC_X86_REG_ESP, stack)
    # 单个callee合同的oracle边界，与冷图生成逻辑无关。
    nargs = {0x10b4e: 1, 0x112a5: 1, 0x135dd: 2, 0x11cac: 1, 0x3790a: 1,
             0x17aa9: 1, 0x1366a: 1, 0x4e381: 0, 0x15f84: 9, 0x32999: 1,
             0x344f2: 3, 0x34894: 1, 0x32975: 1, 0x13512: 1, 0x11df2: 3,
             0x134e4: 0, 0x35f10: 1, 0x1956b: 1, 0x16559: 1, 0x16c57: 1, 0x196cb: 0,
             0x1aa1d: 3, 0x1b8a6: 1, 0x1bb8c: 2}
    leaves = {"story", "pan", "refresh_map", "delay_ms", "delay_ticks", "move", "clear_keys",
              "palette_add", "arrival_prepare", "arrival_frames", "arrival_close", "notice_actor",
              "message", "portrait_frame", "wait_key", "close_box"}
    def snapshot():
        if prefix_guard[0] is None:
            prefix_guard[0] = byte_guard(cpu.mem_read(units, case["count"] * 80))
        return {"count": word(0x53beb), "roster_count": word(0x53bfb),
                "unit_prefix_guard": prefix_guard[0],
                "state_hex": bytes(cpu.mem_read(state, 32)).hex(),
                "turn_slots_hex": bytes(cpu.mem_read(metadata + 3, 48)).hex(),
                "display": word(0x51a83), "contribution": word(0x53ec8)}
    def code(machine, address, size, userdata):
        if pending_branch[0] is not None:
            branch_edges.add((pending_branch[0], address)); pending_branch[0] = None
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if address == 0x3702f:
            cpu.reg_write(UC_X86_REG_EIP, word(sp)); cpu.reg_write(UC_X86_REG_ESP, sp + 8); return
        if address in nargs:
            before = snapshot(); trace_start = len(result)
            args = [word(sp + 4 + i * 4) for i in range(nargs[address])]
            retval = 0
            if address == 0x10b4e:
                result.append(("append_group", [args[0], cpu.mem_read(0x53afa, 1)[0]]))
                put(0x53beb, word(0x53beb) + groups.get(args[0], 0))
            elif address == 0x112a5:
                result.append(("roster_append", args)); put(0x53bfb, word(0x53bfb) + 1)
            elif address == 0x135dd: result.append(("pan", args))
            elif address == 0x11cac: result.append(("refresh_map", args))
            elif address == 0x3790a: result.append(("delay_ms", args))
            elif address == 0x17aa9: result.append(("delay_ticks", args))
            elif address == 0x1366a: result.append(("move", [args[0], 0]))
            elif address == 0x4e381: result.append(("clear_keys", []))
            elif address == 0x15f84:
                if args[2] == 0xa0000: result.append(("story", [args[1]]))
                elif args[2] == 0xa951f: result.append(("message", [args[1], 1, 95, 119]))
                else: raise ValueError(f"未审text参数 {args}")
            elif address == 0x32999:
                result.extend([("arrival_prepare", []), ("append_group", [args[0], 0]),
                               ("arrival_frames", []), ("arrival_close", [])])
                put(0x53beb, word(0x53beb) + groups.get(args[0], 0))
            elif address == 0x344f2:
                prefix_guard[0] = None
                result.append(("unit_ai_write", args))
                for index in range(args[0], args[1] + 1):
                    at = units + index * 80 + 52; old = cpu.mem_read(at, 1)[0]
                    cpu.mem_write(at, bytes([(old & 0xf0) | args[2]]))
            elif address == 0x34894: retval = cpu.mem_read(units + args[0] * 80 + 5, 1)[0] & 1
            elif address in (0x32975, 0x13512):
                prefix_guard[0] = None
                offset, mask = (5, 1) if address == 0x32975 else (5, 128)
                at = units + args[0] * 80 + offset; value = cpu.mem_read(at, 1)[0] | mask
                cpu.mem_write(at, bytes([value])); result.append(("unit_write", [args[0], offset, 1, value]))
            elif address == 0x11df2: result.append(("palette_add", args))
            elif address == 0x134e4:
                prefix_guard[0] = None
                for index in range(word(0x53beb)): cpu.mem_write(units + index * 80 + 3, b"\0")
                result.extend([("directions_reset", []), ("delay_ms", [20])])
            elif address == 0x35f10:
                prefix_guard[0] = None
                result.extend([("unit_hp_zero_tail", args), ("finalize_defeated", [])])
                for index in range(args[0], word(0x53beb)):
                    cpu.mem_write(units + index * 80 + 64, b"\0\0")
                    cpu.mem_write(units + index * 80 + 5, b"\1")
                    cpu.mem_write(units + index * 80 + 3, b"\0")
            elif address == 0x1956b: result.append(("notice_actor", [case["caller"]]))
            elif address == 0x16559: result.append(("portrait_frame", args))
            elif address == 0x16c57: result.append(("wait_key", args))
            elif address == 0x196cb: result.append(("close_box", []))
            elif address == 0x1b8a6:
                raw=cpu.mem_read(units+args[0]*80,80)
                retval=sum(not(raw[10+i*2]&128) for i in range(8))
            elif address in (0x1aa1d,0x1bb8c):
                recipient=args[0]
                if recipient>=word(0x53beb):raise ValueError("inventory/child recipient outside original count")
                if address==0x1aa1d:
                    descriptor=bytes(cpu.mem_read(args[2],3))
                    children=bytes(cpu.mem_read(0x52742,12))
                    matches=[i for i in range(4) if children[i*3:i*3+3]==descriptor]
                    if args[1]!=1 or descriptor[0]!=0 or len(matches)!=1:
                        raise ValueError("unreviewed child descriptor/count")
                    item=int.from_bytes(descriptor[1:],"little")
                    result.append(("loot_child",[matches[0],-1]))
                    allowed=cpu.mem_read(units+recipient*80+6,1)[0]==2
                else:
                    item=args[1];allowed=True
                    result.append(("event_item_append",[recipient,item]))
                if item>255:raise ValueError("item WORD outside reviewed byte consumer")
                if allowed:
                    for slot_index in range(8):
                        at=units+recipient*80+10+slot_index*2
                        if cpu.mem_read(at,1)[0]&128:
                            prefix_guard[0]=None;cpu.mem_write(at,bytes([0,item]));break
                    # 满包的callee呈现/选择替身明确Cancel；不编造替换slot。
            for op, values in result[trace_start:]:
                if op in leaves:
                    checkpoints.append({"op": op, "args": values,
                        **(before if op == "arrival_prepare" else snapshot())})
            cpu.reg_write(UC_X86_REG_EAX, retval)
            cpu.reg_write(UC_X86_REG_EIP, word(sp)); cpu.reg_write(UC_X86_REG_ESP, sp + 4); return
        if address not in instructions:
            unsupported.append(hex(address)); raise ValueError(f"未声明原指令域 {address:#x}")
        visited.add(address)
        raw = bytes.fromhex(instructions[address]["raw_hex"])
        if 0x70 <= raw[0] <= 0x7f or raw[:1] == b"\x0f" and 0x80 <= raw[1] <= 0x8f:
            pending_branch[0] = address
    def write(machine, access, address, size, value, userdata):
        if address == 0x51a83: result.append(("display_mode", [value]))
        elif units <= address < units + 96 * 80:
            prefix_guard[0] = None
            actor, offset = divmod(address - units, 80)
            result.append(("unit_write", [actor, offset, size, value & ((1 << (size * 8)) - 1)]))
        elif state <= address < state + 32:
            result.append(("state_write", [address - state, size, value & ((1 << (size * 8)) - 1)]))
        elif metadata + 3 <= address < metadata + 51:
            result.append(("turn_write", [(address - metadata - 3) // 3, size, value & ((1 << (size * 8)) - 1)]))
        elif address == 0x53ec8: result.append(("contribution_clear", []))
    cpu.hook_add(unicorn.UC_HOOK_CODE, code); cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)
    target = struct.unpack_from("<I", image, 0x51b91 + slot * 4)[0]
    cpu.emu_start(target, stop, count=200000)
    if cpu.reg_read(UC_X86_REG_EIP) != stop: raise ValueError("原callback未正常返回")
    return {"trace": result, "visited": sorted(visited), "unsupported": unsupported,
            "conditional_edges": sorted(branch_edges), "checkpoints": checkpoints, "finish": snapshot()}


def compare(value, comparison, expected):
    return (value == expected, value != expected, value < expected, value <= expected,
            value > expected, value >= expected)[comparison]


def source_value(source, turn, state):
    signed = turn if turn < 0x80000000 else turn - 0x100000000
    return (turn, turn & 255, (abs(signed) // 2 * (-1 if signed < 0 else 1)) & 0xffffffff,
            ((turn & 255) - 14) * 2 & 255, (((turn & 255) - 14) * 2 + 1) & 255,
            state[16], (24 - state[16]) & 0xffffffff, 2 * state[16] + 25,
            2 * state[16] + 26, state[16] + 2)[source]


def graph_trace(flow, case, groups, stop_after=None):
    """只解释有限已证原语，用于和原caller指令轨迹比较；不是正式consumer替身。"""
    nodes = {node["id"]: node for node in flow["nodes"]}
    pc, records, state, count = flow["entry"], bytearray(case["units"]), bytearray(case["state"]), case["count"]
    result, branches = [], []
    def actor(value): return case["caller"] if value == -1 else value
    for _ in range(65536):
        node = nodes[pc]; op, args = node["op"], node["args"]; port = "next"
        if op == "end": return {"trace": result, "branches": branches}
        if op == "fail": raise ValueError("正常轨迹到失败叶")
        if op.startswith("branch_"):
            if op == "branch_event_byte": yes = compare(state[args[0]], args[1], args[2])
            elif op == "branch_turn":
                turn = case["turn"] if case["turn"] < 0x80000000 else case["turn"] - 0x100000000
                yes = compare(turn, args[0], args[1])
            elif op == "branch_caller": yes = compare(case["caller"], args[0], args[1])
            elif op == "branch_actor":
                at = actor(args[0]) * 80
                values = (records[at + 5] & 1, records[at + 6], records[at + 8], records[at + 7],
                          int.from_bytes(records[at + 64:at + 66], "little"),
                          int.from_bytes(records[at + 66:at + 68], "little"), bool(records[at + 5] & 128), 0)
                yes = compare(values[args[1]], args[2], args[3])
            elif op == "branch_hidden_count":
                yes = compare(sum(bool(records[i * 80 + 5] & 1) for i in range(args[0], args[1] + 1)), args[2], args[3])
            else: raise ValueError("该轨迹验证器没有已证predicate: " + op)
            port = "yes" if yes else "no"; branches.append([node["id"], port])
        elif op == "unit_field_write":
            first, last, field, operation, value = args; offset = FIELD_OFFSETS[field]; width = 2 if field in (18, 19) else 1
            for i in range(first, last + 1):
                at = i * 80 + offset; old = int.from_bytes(records[at:at + width], "little")
                changed = value if operation == 0 else old & value if operation == 1 else old | value
                records[at:at + width] = changed.to_bytes(width, "little")
                result.append(("unit_write", [i, offset, width, changed]))
        elif op == "ai_anchor":
            for i in range(args[0], args[1] + 1):
                for offset, value in ((53, args[2]), (54, args[3])):
                    result.append(("unit_write", [i, offset, 1, value])); records[i * 80 + offset] = value
        elif op == "event_byte_write":
            for index in range(args[0], args[1] + 1):
                state[index] = ((state[index] if args[3] else 0) + args[2]) & 255
                result.append(("state_write", [index, 1, state[index]]))
        elif op == "event_byte_from_count":
            state[args[0]] = (count - args[1]) & 255
            result.append(("state_write", [args[0], 1, state[args[0]]]))
        elif op == "turn_relative": result.append(("turn_write", [args[0], 1, (case["turn"] + args[1]) & 255]))
        elif op in ("append_group", "append_group_source"):
            group = args[0] if op == "append_group" else source_value(args[0], case["turn"], state)
            result.append(("append_group", [group, args[1]])); count += groups.get(group, 0)
        else:
            if op == "story_source": op, args = "story", [source_value(args[0], case["turn"], state)]
            if op == "notice_actor": args = [actor(args[0])]
            result.append((op, args))
            if op in ("story", "wait_key", "loot_child"): port = "ack"
        if stop_after is not None and len(result) >= stop_after:
            return {"trace": result[:stop_after], "branches": branches}
        pc = node["on"][port]
    raise ValueError("候选图轨迹未在有限预算内终止")


def mechanism_cases(metadata):
    units = bytearray(96 * 80)
    for i in range(96):
        units[i * 80 + 5] = 0x40
        units[i * 80 + 8] = i % 32
        units[i * 80 + 0x34] = 0xa5
        units[i * 80 + 0x35] = 0xa1
        units[i * 80 + 0x36] = 0xb2
        units[i * 80 + 49] = 255
        units[i * 80 + 64:i * 80 + 68] = b"\x64\0\x64\0"
        for item in range(8): units[i * 80 + 10 + item * 2] = 128
    base = {"units": list(units), "state": [0] * 32, "metadata": list(metadata[:131]),
            "turn": 2, "count": 80, "caller": 0, "display": 1, "contribution": 0x8a55}
    cases = []
    def add(name, *, turn=None, caller=0, byte=None, side=0, text_id=None, hidden=()):
        case = {**base, "id": name, "units": list(base["units"]), "state": list(base["state"]), "caller": caller}
        if turn is not None: case["turn"] = turn
        if byte is not None: case["state"][byte[0]] = byte[1]
        case["units"][caller * 80 + 6] = side
        if text_id is not None: case["units"][caller * 80 + 8] = text_id
        for actor in hidden: case["units"][actor * 80 + 5] |= 1
        cases.append(case)
    add("baseline")
    for index, values in ((16, (1, 2, 4, 7, 255)), (17, (1, 4, 255)), (19, (1, 255))):
        for value in values: add(f"state{index}-{value}", byte=(index, value))
    for turn in (0, 1, 3, 14, 15, 255, 256, 0x7fffffff, 0x80000000, 0xffffffff): add(f"turn-{turn}", turn=turn)
    add("hidden6", hidden=[6]); add("hidden8", hidden=[8]); add("all-hidden7-36", hidden=range(7, 37))
    add("caller1-side1", caller=1, side=1)
    add("caller1-side1-text9", caller=1, side=1, text_id=9)
    add("caller1-side1-state17-1", caller=1, side=1, byte=(17, 1))
    return cases


def fit_mechanism_case(flow, case, groups, scripts):
    """原96actor域与原script raw引用共同限定机制输入；不借实际关卡初值。"""
    required = case["caller"] + 1
    for node in flow["nodes"]:
        op, args = node["op"], node["args"]
        if op in ("unit_ai_write", "unit_field_write", "ai_anchor", "branch_hidden_count"):
            required = max(required, args[1] + 1)
        elif op == "branch_actor" and args[0] >= 0: required = max(required, args[0] + 1)
    added = 0
    for op, args in graph_trace(flow, case, groups)["trace"]:
        if op == "append_group": added += groups.get(args[0], 0)
        elif op == "move":
            raw = scripts[args[0]].data; at = 1; maximum = -1
            for _ in range(raw[0]):
                count = raw[at + 1]; at += 2
                for pair in range(count): maximum = max(maximum, raw[at + pair * 2])
                at += count * 2
            required = max(required, maximum + 1 - added)
    # event_byte_from_count(21,3)只在有追加的分支到达；保守为所有机制输入保3。
    if any(node["op"] == "event_byte_from_count" for node in flow["nodes"]): required = max(required, 3)
    if not 0 < required <= 96 - added:
        raise ValueError("该源路径没有同时满足raw引用和96容量的机制fixture；保留阻塞，不截断group")
    return {**case, "count": required}


def source_cfg_branches(domain, slot):
    """按真指令rel32/rel8及显式callee边界求完整入口可达条件，不搜索近CALL。"""
    instructions = {i["ea"]: i for f in domain["functions"] for i in f["instructions"]}
    target = next(row["target"] for row in domain["slots"] if row["slot"] == slot)
    primitives = {0x3702f, 0x10b4e, 0x112a5, 0x135dd, 0x11cac, 0x3790a,
                  0x17aa9, 0x1366a, 0x4e381, 0x15f84, 0x32999, 0x344f2,
                  0x34894, 0x32975, 0x13512, 0x11df2, 0x134e4, 0x35f10,
                  0x1956b, 0x16559, 0x16c57, 0x196cb, 0x1aa1d, 0x1b8a6, 0x1bb8c}
    pending, seen, branches = [(target, ())], set(), {}
    while pending:
        at, stack = pending.pop()
        if (at, stack) in seen: continue
        seen.add((at, stack))
        if len(seen) > 20000 or len(stack) > 8: raise ValueError("原入口CFG越出有界callee闭包")
        if at in primitives:
            if stack: pending.append((stack[-1], stack[:-1]))
            continue
        if at not in instructions: raise ValueError(f"原入口CFG缺指令 {at:#x}")
        raw = bytes.fromhex(instructions[at]["raw_hex"]); following = at + len(raw)
        if raw[0] in (0xc3, 0xc2):
            if stack: pending.append((stack[-1], stack[:-1]))
        elif raw[0] == 0xe8:
            callee = following + int.from_bytes(raw[1:5], "little", signed=True)
            pending.append((following, stack)) if callee in primitives else pending.append((callee, (*stack, following)))
        elif raw[0] in (0xe9, 0xeb):
            pending.append((following + int.from_bytes(raw[1:], "little", signed=True), stack))
        elif 0x70 <= raw[0] <= 0x7f or raw[0] == 0x0f and 0x80 <= raw[1] <= 0x8f:
            relative = raw[1:] if raw[0] != 0x0f else raw[2:]
            target = following + int.from_bytes(relative, "little", signed=True)
            branches[at] = sorted({following, target}); pending.extend([(following, stack), (target, stack)])
        elif raw[0] == 0xff and (raw[1] >> 3) & 7 in (2, 3, 4, 5):
            raise ValueError(f"未闭合间接控制流 {at:#x}")
        else: pending.append((following, stack))
    return branches


def load_inputs(original, ledger_directory, domain_path, image_path):
    original, ledger_directory = Path(original), Path(ledger_directory)
    manifest = json.loads((ledger_directory / "manifest.json").read_bytes(), object_pairs_hook=audit.unique_keys)
    if manifest.get("schema") != "fd2-stage-source-ledger-v1" or set(manifest["files"]) != {"inventory.json", "common-scope.json", "ledger.json", "fixtures.json"}:
        raise ValueError("S1完整来源包manifest缺失或schema不同")
    snapshots = {Path(__file__): Path(__file__).read_bytes(), Path(domain_path): Path(domain_path).read_bytes(),
                 Path(image_path): Path(image_path).read_bytes(),
                 ROOT / "data/static/flows/common_event.json": (ROOT / "data/static/flows/common_event.json").read_bytes()}
    for name, expected in manifest["files"].items():
        path = ledger_directory / name; snapshots[path] = path.read_bytes()
        if digest(snapshots[path]) != expected: raise ValueError("S1来源包摘要不等: " + name)
    snapshots[ledger_directory / "manifest.json"] = (ledger_directory / "manifest.json").read_bytes()
    sources = {name: (original / name).read_bytes() for name in audit.SOURCE_HASHES}
    if any(digest(sources[name]) != sha for name, sha in audit.SOURCE_HASHES.items()):
        raise ValueError("原资源身份不同")
    ledger = json.loads(snapshots[ledger_directory / "ledger.json"], object_pairs_hook=audit.unique_keys)
    from stage_source_ledger import Sources, validate_ledger
    validate_ledger(ledger, Sources(original))
    domain = json.loads(snapshots[Path(domain_path)], object_pairs_hook=audit.unique_keys)
    image = snapshots[Path(image_path)]
    if digest(image) != IMAGE_SHA or domain.get("strict_image_sha256") != IMAGE_SHA or domain.get("exe_sha256") != audit.EXE_SHA256:
        raise ValueError("Domain/strict镜像身份不同")
    if domain.get("schema") != "fd2-s2-callback-domain-v1" or domain.get("database_saved") is not False:
        raise ValueError("Domain来源schema/保存边界不同")
    pointers = audit.collect_internal_offset_fixups(original / "FD2.EXE")
    callbacks = [r for r in ledger["records"] if r["kind"] == "dispatch" and r["decoded"].get("table") == "event_callback"]
    if len(callbacks) != 90 or [r["decoded"]["slot"] for r in callbacks] != list(range(90)):
        raise ValueError("callback90槽来源不完整")
    for slot, (row, observed) in enumerate(zip(callbacks, domain["slots"])):
        source = row["source"]; va = 0x51b91 + slot * 4; target = pointers[va][2]
        raw = source_range(sources["FD2.EXE"], va, 4)
        if (row["source_id"] != f"dispatch:event_callback:{slot:03d}" or source["canonical_va"] != hex(va)
                or source["file"] != "FD2.EXE" or source["file_sha256"] != audit.EXE_SHA256
                or source["object"] != 2 or source["object_offset"] != va - 0x50000
                or source["view"] != "raw-unrelocated" or source["length"] != 4 or source["raw_hex"] != raw.hex()
                or source["read_sha256"] != digest(raw) or int(row["decoded"]["target_va"], 16) != target
                or observed["slot"] != slot or observed["target"] != target or observed["source_id"] != row["source_id"]
                or hex(observed["owner"]) not in row["decoded"]["ida_owners"]):
            raise ValueError("callback原ID/槽位/指针/owner不等")
    if len(domain["slots"]) != 90: raise ValueError("Domain槽数不同")
    for function in domain["functions"]:
        expected = {a for chunk in function["chunks"] for a in range(chunk["start"], chunk["end"])}
        observed = set()
        for chunk in function["chunks"]:
            raw = image[chunk["start"]:chunk["end"]]
            if raw.hex() != chunk["raw_hex"] or digest(raw) != chunk["sha256"]:
                raise ValueError("Domain完整chunk与严格原镜像不同")
        for instruction in function["instructions"]:
            at = instruction["ea"]; raw = bytes.fromhex(instruction["raw_hex"])
            if raw != image[at:at + len(raw)]: raise ValueError("Domain指令与原镜像不同")
            span = set(range(at, at + len(raw)))
            if span & observed or not span <= expected: raise ValueError("Domain指令重叠/越chunk")
            observed.update(span)
        if observed != expected: raise ValueError("Domain未完整读取shared chunk")
    if any(ledger["sources"][name] != {"length": len(raw), "sha256": digest(raw)} for name, raw in sources.items()):
        raise ValueError("S1原来源身份不同")
    return ledger, callbacks, domain, image, sources, snapshots


def validate_candidates(flows, operations):
    """结构校验与原码oracle分开；端口保持当前有限consumer的精确合同。"""
    identities = set()
    for flow in flows:
        if flow["id"] in identities: raise ValueError("候选图ID重复")
        identities.add(flow["id"]); nodes = {n["id"]: n for n in flow["nodes"]}
        if len(nodes) != len(flow["nodes"]) or flow["entry"] not in nodes: raise ValueError("候选图节点/入口重复或缺失")
        reached, pending = set(), [flow["entry"]]
        while pending:
            label = pending.pop()
            if label in reached: continue
            reached.add(label)
            if label not in nodes: raise ValueError("候选图边悬空")
            n = nodes[label]; op = n["op"]
            if op not in operations: raise ValueError("未声明有限op: " + op)
            spec = operations[op]["args"]
            if len(n["args"]) != len(spec) or any(type(v) is not int or not s["min"] <= v <= s["max"] for v, s in zip(n["args"], spec)):
                raise ValueError("候选图参数位宽/值域不同")
            expected = set() if op in ("end", "fail") else {"yes", "no", "error"} if op.startswith("branch_") else {"ack", "error"} if op in ("story", "story_source", "wait_key", "loot_child") else {"next", "error"}
            if set(n["on"]) != expected: raise ValueError("候选图等待/错误端口不同")
            pending.extend(n["on"].values())
        if reached != set(nodes): raise ValueError("候选图有不可达节点")


NEW_OPERATIONS = {
    "palette_add": {"args": [{"name": "first", "min": 0, "max": 255}, {"name": "last", "min": 0, "max": 255}, {"name": "raw_add", "min": 0, "max": 255}]},
    "delay_ticks": {"args": [{"name": "ticks", "min": 0, "max": 65535}]},
}


def build(original, ledger_directory, domain_path, image_path):
    ledger, callbacks, domain, image, sources, snapshots = load_inputs(original, ledger_directory, domain_path, image_path)
    document = json.loads(snapshots[ROOT / "data/static/flows/common_event.json"], object_pairs_hook=audit.unique_keys)
    existing = {flow["id"]: flow for flow in document["flows"]}
    if not set(EXISTING.values()) <= set(existing): raise ValueError("已证既有common图缺失")
    if any(digest(encode(existing[identity]).encode()) != EXISTING_FLOW_SHA[slot] for slot, identity in EXISTING.items()):
        raise ValueError("既有完整图改变，需要重新核原entry/参数/轨迹")
    candidate_programs = programs(); canonical, flows, decisions, cases_out = {}, [], [], []
    field = audit.archive_entries(sources["FDFIELD.DAT"])
    scripts = audit.source_scripts(Path(original) / "FD2.EXE")
    incoming = {row["source_id"]: [] for row in callbacks}
    for row in ledger["records"]:
        for reference in row["references"]:
            if reference in incoming: incoming[reference].append(row["source_id"])
    # S1引用亦从原metadata独立复核；manifest摘要不能给任意caller提权。
    expected_incoming = {identity: [] for identity in incoming}
    for stage in range(33):
        meta = field[stage * 3 + 1][1]
        for kind, base, stride in (("turn", 3, 3), ("lookup", 51, 2), ("cell_action", 83, 3)):
            for index in range(16):
                raw = meta[base + index * stride:base + (index + 1) * stride]
                target = raw[1] if kind == "turn" else raw[0] if kind == "lookup" else int.from_bytes(raw[1:3], "little") if raw[0] not in (0, 1) else None
                if target is not None and target < 90:
                    expected_incoming[f"dispatch:event_callback:{target:03d}"].append(f"stage:{stage:02d}:{kind}:{index:05d}")
        for index in range((len(meta) - 131) // 26):
            raw = meta[131 + index * 26 + 22:131 + index * 26 + 25]
            target = int.from_bytes(raw[1:3], "little")
            if raw[0] == 2 and target < 90: expected_incoming[f"dispatch:event_callback:{target:03d}"].append(f"stage:{stage:02d}:drop:{index:05d}")
    if expected_incoming != incoming: raise ValueError("S1 callback caller引用不等于原完整metadata")
    source_by_id = {row["source_id"]: row for row in ledger["records"]}
    all_conditionals, reused_slots = 0, []
    for slot, row in enumerate(callbacks):
        identity = row["source_id"]; callers = incoming[identity]
        decision = {"source_id": identity, "slot": slot, "target_va": row["decoded"]["target_va"],
                    "source_pointer": row["source"], "caller_sources": callers,
                    "caller_domains": sorted({source_by_id[x]["caller"] for x in callers}),
                    "semantic_families": row["semantic_families"],
                    "parameter_width_boundary": "turn/lookup action BYTE; descriptor callback full WORD; never low-BYTE alias",
                    "source_entry": domain["slots"][slot], "new_runtime_binding": False}
        owner = next(f for f in domain["functions"] if f["va"] == decision["source_entry"]["owner"])
        decision["complete_owner_chunks"] = [{key: chunk[key] for key in ("start", "end", "sha256")} for chunk in owner["chunks"]]
        if slot == 82:
            decision.update({"disposition": "blocked", "definition": None,
                "blocker": {"family": "teleport_actor_source", "target": "0x33f78 -> 0x22253",
                    "reason": "dynamic actor expressions and full teleport present/write/ABI contract not closed by current54ops",
                    "next_evidence": "verify complete22253 instructions/stack ABI and existing finite effect consumer; preserve all362e8 branches before binding",
                    "affected_sources": callers}})
        elif slot in EXISTING:
            flow = existing[EXISTING[slot]]
            decision.update({"disposition": "existing_definition_reference", "definition": flow["id"],
                             "definition_sha256": digest(encode(flow).encode()),
                             "original_evidence": "G4 verify_common_events full-source rules / G5 six complete caller original-CPU receipts; exact original target verified here"})
            reused_slots.append(slot)
        else:
            program = candidate_programs[slot]; key = encode(program)
            if key not in canonical:
                canonical[key] = f"common.callback.{slot:03d}"
                flows.append(cold_flow(canonical[key], program))
            flow = next(f for f in flows if f["id"] == canonical[key])
            decision.update({"disposition": "converted_candidate", "definition": flow["id"],
                             "definition_sha256": digest(encode(flow).encode()), "blocker": None})
            stage = next((source_by_id[x]["stage"] for x in callers if source_by_id[x]["stage"] is not None), 0)
            meta = field[stage * 3 + 1][1]
            groups = Counter(meta[131 + i * 26 + 21] for i in range(meta[2]))
            seen_edges, expected_edges, graph_branches = {}, source_cfg_branches(domain, slot), {}
            for original_case in mechanism_cases(meta):
                case = fit_mechanism_case(flow, original_case, groups, scripts)
                original_result = source_trace(image, domain, slot, case, groups)
                graph_result = graph_trace(flow, case, groups)
                if original_result["trace"] != graph_result["trace"]:
                    raise ValueError(f"原callback与冷图轨迹不等: {slot}/{case['id']}")
                for at, target in original_result["conditional_edges"]: seen_edges.setdefault(at, set()).add(target)
                for label, port in graph_result["branches"]: graph_branches.setdefault(label, set()).add(port)
                cases_out.append({"source_id": identity, "case_id": case["id"], "resource_fixture_stage": stage,
                                  "input": case, "source_trace": original_result["trace"],
                                  "source_trace_sha256": digest(encode(original_result["trace"]).encode()),
                                  "source_conditional_edges": original_result["conditional_edges"],
                                  "source_checkpoints": original_result["checkpoints"], "source_finish": original_result["finish"],
                                  "candidate_definition": flow["id"], "candidate_trace_matches": True})
            if any(sorted(seen_edges.get(at, ())) != targets for at, targets in expected_edges.items()):
                raise ValueError(f"原callback可达条件的两边未覆盖: {slot}")
            if any(ports != {"yes", "no"} for ports in graph_branches.values()):
                raise ValueError(f"候选图分支两边未覆盖: {slot}")
            decision["original_conditionals"] = {hex(at): list(map(hex, targets)) for at, targets in expected_edges.items()}
            decision["original_conditionals_all_edges_executed"] = True
            decision["mechanism_case_count"] = len(mechanism_cases(meta)); all_conditionals += len(expected_edges)
        decisions.append(decision)
    validate_candidates(flows, {**document["operations"], **NEW_OPERATIONS})
    result = {"schema": "fd2-s2-callback-candidates-v1", "domain": "common_event", "flows": flows,
              "required_new_operations": NEW_OPERATIONS, "decisions": decisions,
              "existing_definition_references": reused_slots, "claims": {"new_gameplay_enabled": False,
                  "new_runtime_bindings": 0, "native_WGC_acceptance": False, "all_callbacks_closed": False},
              "summary": {"slots": len(decisions), "different_targets": 90, "different_IDA_owners": 88,
                  "converted_candidate_slots": len(candidate_programs), "new_unique_flows": len(flows),
                  "existing_definition_slots": len(reused_slots), "blocked_slots": [82], "source_cpu_cases": len(cases_out),
                  "source_conditionals_with_both_edges": all_conditionals,
                  "new_flow_nodes": sum(len(f["nodes"]) for f in flows)},
              "inputs_sha256": {str(p): digest(raw) for p, raw in snapshots.items()},
              "oracle_scope": "real callback/explicit shared-tail and35b78/361b0 original x86; finite primitive callees stubbed, no SDL/native/whole-game equivalence"}
    for name, raw in sources.items():
        if (Path(original) / name).read_bytes() != raw: raise ValueError("原资源在转换期间改变")
    if any(path.read_bytes() != raw for path, raw in snapshots.items()): raise ValueError("转换输入在生成期间改变")
    return result, {"schema": "fd2-s2-callback-original-traces-v1", "cases": cases_out,
                    "original_cpu_executed": True, "C_implementation_compared": False,
                    "scope": result["oracle_scope"]}


def verify_c_probe(probe, original, trace_document):
    """正式C begin/peek/reply，期望仅来自独立真x86 caller检查点。"""
    requests = []
    for row in trace_document["cases"]:
        requests.append((row, 0, 0))
    # 每个去重图一次等待保护；正常批已覆盖每个源predicate两边。
    unique = {}
    for row in trace_document["cases"]:
        if row["source_checkpoints"]:
            previous = unique.get(row["candidate_definition"])
            if previous is None or len(row["source_checkpoints"]) > len(previous["source_checkpoints"]):
                unique[row["candidate_definition"]] = row
    for row in unique.values():
        checkpoints = row["source_checkpoints"]
        cut = next((i for i, point in enumerate(checkpoints) if point["op"] in ("story", "wait_key") and i), 0)
        for mode in (1, 2, 3): requests.append((row, mode, cut))
    capacity = next(row for row in trace_document["cases"] if row["source_id"] == "dispatch:event_callback:027" and row["case_id"] == "turn-0")
    capacity = {**capacity, "input": {**capacity["input"], "count": 96}}
    requests.append((capacity, 4, 0))
    wire = []
    for index, (row, mode, cut) in enumerate(requests):
        case = row["input"]
        wire.append(" ".join(map(str, (index, row["candidate_definition"], row["resource_fixture_stage"],
            case["count"], case["caller"], case["turn"], case["display"], case["contribution"], mode, cut))) + " " +
            bytes(case["units"]).hex() + " " + bytes(case["state"]).hex() + " " + bytes(case["metadata"][3:51]).hex())
    probe_hash = digest(Path(probe).read_bytes())
    process = subprocess.run([str(Path(probe).resolve()), str((Path(original) / "FDFIELD.DAT").resolve())],
                             input="\n".join(wire) + "\n", text=True, capture_output=True, timeout=180)
    if process.returncode:
        raise ValueError(f"C正式consumer probe退出{process.returncode}: {process.stderr[-1000:]}")
    if digest(Path(probe).read_bytes()) != probe_hash: raise ValueError("C probe二进制在比较期间改变")
    observed = [json.loads(line, object_pairs_hook=audit.unique_keys) for line in process.stdout.splitlines()]
    if len(observed) != len(requests): raise ValueError("C probe case缺失/额外")
    results = []
    for index, ((row, mode, cut), actual) in enumerate(zip(requests, observed)):
        expected = [] if mode == 4 else row["source_checkpoints"] if not mode else row["source_checkpoints"][:cut + 1]
        if actual["case_id"] != index or actual["status"] != 0 or (actual["begin_result"] >= 0 if mode == 4 else actual["begin_result"] != 0) or len(actual["checkpoints"]) != len(expected):
            raise ValueError(f"C probe正式入口/叶数不同: {row['source_id']}/{row['case_id']}/mode{mode}: {actual}")
        for at, (point, original_point) in enumerate(zip(actual["checkpoints"], expected)):
            point["args"] = point["args"][:len(original_point["args"])]
            # peek保留caller占位；正式Play只对此已证叶用同一peek actor解析-1。
            if point["op"] == "notice_actor" and point["args"] == [-1]: point["args"] = [row["input"]["caller"]]
            if point != original_point: raise ValueError(f"C/原caller检查点不等: {row['source_id']}/{row['case_id']}/mode{mode}/leaf{at}: {point} != {original_point}")
        if mode == 4:
            case = row["input"]
            finish = {"count": 96, "roster_count": 0, "unit_prefix_guard": byte_guard(case["units"]),
                      "state_hex": bytes(case["state"]).hex(), "turn_slots_hex": bytes(case["metadata"][3:51]).hex(),
                      "display": case["display"], "contribution": case["contribution"]}
        else: finish = row["source_finish"] if not mode else {k: v for k, v in expected[-1].items() if k not in ("op", "args")}
        if actual["finish"] != finish or (not mode and (not actual["repeat_ack_guard"] or actual["active"])) or mode and not actual["guarded"]:
            raise ValueError(f"C尾/重复ACK或等待前缀守卫不等: {row['source_id']}/{row['case_id']}/mode{mode}")
        results.append({"source_id": row["source_id"], "case_id": row["case_id"], "mode": mode,
                        "checked_leaf_prefix": len(expected), "passed": True})
    return {"schema": "fd2-s2-callback-C-original-comparison-v1", "probe_sha256": probe_hash,
            "cases": results, "normal_cases": len(trace_document["cases"]), "wait_guard_cases": len(requests) - len(trace_document["cases"]),
            "full_initial_actor_prefix_counts": sorted({row["input"]["count"] for row in trace_document["cases"]}),
            "capacity_refusal_cases": 1, "source_original_primitives_stubbed": ["constructors/group newly appended80B region", "stats", "renderer/text/keyboard/delay/DAC", "already-proven directions/finalize helper state contract"],
            "C_implementation": "real Game event_resume begin/peek/reply and business primitives; presentation leaves explicitly ACKed; no SDL/native WGC"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--ledger-dir", type=Path, required=True)
    parser.add_argument("--domain", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--c-probe", type=Path)
    args = parser.parse_args()
    try:
        from stage_source_ledger import validate_output
        output = validate_output(args.output_dir, original=args.original)
        result, traces = build(args.original, args.ledger_dir, args.domain, args.image)
        comparison = verify_c_probe(args.c_probe, args.original, traces) if args.c_probe else None
        output.mkdir(parents=True)
        hashes = {}
        outputs = [("candidates.json", result), ("original-traces.json", traces)]
        if comparison is not None: outputs.append(("C-original-comparison.json", comparison))
        for name, value in outputs:
            raw = encode(value).encode(); (output / name).write_bytes(raw); hashes[name] = digest(raw)
        (output / "manifest.json").write_text(encode({"schema": result["schema"], "files": hashes, "summary": result["summary"]}), encoding="utf8")
        print(encode(result["summary"]), end="")
    except (ValueError, KeyError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
