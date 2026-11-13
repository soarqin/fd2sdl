#!/usr/bin/env python3
"""S2入场／胜利转换：完整Domain CFG与严格原字节，候选不开放运行绑定。

来源为51d71／51de9各30槽及25bf4／25ebb场景分派。保留表内实际入口、
共享尾、条件、顺序与原owner；不能把owner入口或邻近CALL冒充真实路径。
"""
from __future__ import annotations

import hashlib
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct
import sys
import subprocess
from audit_stage_data import unique_keys

ROOT = Path(__file__).resolve().parents[2]
S1_LEDGER_SHA256 = "1a47357ba1f179db74bd0d99bf2096748596e6fc46c1ab1aabf30950eb3e6202"
TABLES = ("stage_entry", "stage_victory")
SUPPORTING_OWNERS = (0x1088D, 0x134E4, 0x13536, 0x17E0B, 0x1F525, 0x1F882,
                     0x205DA, 0x22AF6, 0x233C6, 0x25BF4, 0x25EBB, 0x33499, 0x34894,
                     0x15311, 0x1B41D, 0x1D4F6, 0x24336, 0x24D22, 0x24618, 0x24B14,
                     0x24BDE, 0x33F78, 0x35B78, 0x35F10, 0x31529, 0x111BA)
IMAGE_SHA256 = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
# 参数数来自已审完整caller的push／清栈，非Hex-Rays推断原型。
LEAVES = {0x205DA: ("stage_setup_source", 0), 0x135DD: ("pan", 2),
          0x1366A: ("move", 1), 0x15F84: ("text", 9),
          0x3790A: ("delay", 1), 0x10B4E: ("append_group", 1),
          0x134E4: ("reset_directions", 0), 0x12D7B: ("focus_actor", 1),
          0x11CAC: ("refresh", 1), 0x11506: ("roster_sync", 0),
          0x112A5: ("roster_append", 1), 0x33499: ("branch_roster_text", 1),
          0x34894: ("branch_any_hidden", 1), 0x233C6: ("layout", 11),
          0x1F882: ("call_fade_out", 0), 0x1F525: ("call_fade_in", 0),
          0x13536: ("clear_acted", 0), 0x11D40: ("palette_subtract", 3),
          0x37910: ("clear_screen", 3), 0x35B78: ("camera_group_flash", 3)}


def source_index(readset, image):
    if readset.get("schema") != "fd2-s2-transition-domain-v1" or readset.get("strict_byte_match") is not True:
        raise ValueError("invalid transition readset schema")
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256 or readset["image_sha256"] != IMAGE_SHA256:
        raise ValueError("strict transition image identity mismatch")
    if readset["ledger_sha256"] != S1_LEDGER_SHA256:
        raise ValueError("transition/S1 source identity mismatch")
    if len(readset['bindings']) != 60 or any(row['table'] not in TABLES or type(row['slot']) is not int for row in readset['bindings']):
        raise ValueError('transition binding scope/type differs')
    for table, base in (("stage_entry", 0x51D71), ("stage_victory", 0x51DE9)):
        bindings = [row for row in readset['bindings'] if row['table'] == table]
        if [row['slot'] for row in bindings] != list(range(30)):
            raise ValueError('transition dispatch scope/order differs')
        for row in bindings:
            va = base + row['slot'] * 4
            if (row['source_id'] != f'dispatch:{table}:{row["slot"]:03d}'
                    or row['source']['canonical_va'] != hex(va)
                    or int.from_bytes(image[va:va + 4], 'little') != int(row['target_va'], 16)):
                raise ValueError('transition slot/fixup/actual entry mismatch')
    owners = {int(owner, 16) for row in readset['bindings'] for owner in row['owners']}
    owners.update(SUPPORTING_OWNERS)
    if len(readset['functions']) != len(owners) or {row['va'] for row in readset['functions']} != owners:
        raise ValueError('transition complete owner scope differs')
    instructions, refs = {}, {}
    for function in readset["functions"]:
        if not function["chunks"] or not function["blocks"]:
            raise ValueError("partial function/CFG readset")
        covered = set()
        for chunk in function["chunks"]:
            start, end = chunk["start"], chunk["end"]
            raw = bytes.fromhex(chunk["raw_hex"])
            if raw != image[start:end] or hashlib.sha256(raw).hexdigest() != chunk["sha256"]:
                raise ValueError("transition function source bytes differ")
            covered.update(range(start, end))
        observed = set()
        for instruction in function["instructions"]:
            ea, size = instruction["ea"], instruction["size"]
            if instruction["raw_hex"] != image[ea:ea + size].hex():
                raise ValueError("transition instruction source bytes differ")
            observed.update(range(ea, ea + size))
            if ea in instructions and instructions[ea] != instruction:
                raise ValueError("shared instruction evidence differs")
            instructions[ea] = instruction
        if observed != covered:
            raise ValueError("instructions do not cover complete function chunks")
        for ref in function["xrefs"]:
            key = (ref["from"], ref["to"], ref["type"])
            refs[key] = ref
    by_from = {}
    for ref in refs.values():
        by_from.setdefault(ref["from"], []).append(ref)
    return instructions, by_from


def linear_path(entry, instructions, xrefs, branch_edges=None):
    """仅接受已证直线控制流／直接共享尾；分支和循环保留完整来源并阻塞。"""
    seen, calls = [], []
    pc = entry
    while len(seen) < 4096:
        if pc in seen:
            return seen, calls, {"family": "bounded_loop_or_private_cfg", "va": hex(pc), "reason": "reachable loop requires a reviewed finite template"}
        instruction = instructions.get(pc)
        if instruction is None:
            return seen, calls, {"family": "external_shared_tail", "va": hex(pc), "reason": "reachable shared tail lacks complete Domain instruction evidence"}
        seen.append(pc)
        mnemonic = instruction["disasm"].split(None, 1)[0].lower()
        refs = xrefs.get(pc, [])
        targets = [ref["to"] for ref in refs if ref["type"] in ("CALL_NEAR", "JUMP_NEAR")]
        if mnemonic == "call":
            if len(targets) != 1:
                return seen, calls, {"family": "indirect_call", "va": hex(pc), "reason": "indirect call is outside the confirmed finite domain"}
            target = targets[0]
            if target != 0x3702F:
                calls.append((pc, target))
                if target not in LEAVES:
                    return seen, calls, {"family": "private_callee", "va": hex(pc), "target": hex(target), "reason": "callee requires a reviewed domain operation or source-table contract"}
        elif mnemonic == "jmp":
            if len(targets) != 1:
                return seen, calls, {"family": "indirect_jump", "va": hex(pc), "reason": "indirect jump has no confirmed target domain"}
            if targets[0] in LEAVES:
                calls.append((pc, targets[0]))
                return seen, calls, None
            pc = targets[0]
            continue
        elif mnemonic.startswith("j") or mnemonic.startswith("loop"):
            if branch_edges and pc in branch_edges:
                choices = {ref['to'] for ref in refs if ref['type'] in ('JUMP_NEAR', 'ORDINARY_FLOW')}
                if branch_edges[pc] not in choices:
                    raise ValueError('reviewed branch edge differs from original CFG')
                pc = branch_edges[pc]
                continue
            return seen, calls, {"family": "conditional_transition", "va": hex(pc), "reason": "source branch requires a reviewed condition/template; never flattened"}
        elif mnemonic in ("ret", "retn"):
            return seen, calls, None
        pc += instruction["size"]
    raise ValueError("transition control-flow budget exceeded")


def recover_caller_cfg(entry, instructions, xrefs):
    """从真实入口遍历全部直接边；调用只记完整目标，不追入未知callee猜语义。"""
    pending, seen, calls, edges, missing, indirect = [entry], set(), [], [], [], []
    while pending:
        pc = pending.pop()
        if pc in seen: continue
        if len(seen) >= 8192: raise ValueError('caller CFG budget exceeded')
        instruction = instructions.get(pc)
        if instruction is None:
            missing.append(hex(pc)); continue
        seen.add(pc)
        mnemonic = instruction['disasm'].split(None, 1)[0].lower()
        refs = xrefs.get(pc, [])
        for ref in refs:
            if ref['type'] == 'CALL_NEAR' and ref['to'] != 0x3702F:
                calls.append({'from': hex(pc), 'target': hex(ref['to'])})
        if mnemonic in ('ret', 'retn'): continue
        if mnemonic == 'call':
            if not any(ref['type'] == 'CALL_NEAR' for ref in refs): indirect.append(hex(pc))
            targets = [pc + instruction['size']]
        elif mnemonic.startswith('j') or mnemonic.startswith('loop'):
            targets = [ref['to'] for ref in refs if ref['type'] in ('JUMP_NEAR', 'ORDINARY_FLOW')]
            if not targets: indirect.append(hex(pc))
        else: targets = [pc + instruction['size']]
        for target in targets:
            edges.append({'from': hex(pc), 'to': hex(target)})
            pending.append(target)
    return {'instruction_vas': [hex(va) for va in sorted(seen)], 'edges': edges, 'calls': calls,
            'missing_shared_tails': sorted(set(missing)), 'indirect_transfers': sorted(set(indirect)),
            'all_direct_caller_edges_recovered': not missing and not indirect}


def semantic_blocker(binding, cfg, initial):
    targets = {int(call['target'], 16) for call in cfg['calls']}
    families, next_steps = [], []
    for target, family, needed in (
            (0x24618, 'transition_effect_page_stride_rect_abi', 'complete316B is read; close the four true stack arguments, App456-stride page/mask resources, temporary buffer and audio lifetime before finite cold frame conversion'),
            (0x24336, 'cinematic_101_frame_temporary_page', 'complete384B is read: FDOTHER34 frame0..100, malloc64000 snapshot, page copy/water/tick3/key clear and animation0; needs a bounded Scene temporary-page ABI, not a whole native handler'),
            (0x24D22, 'viewport_312x192_scroll_owner', 'complete208B is read: nonzero BYTE51A10 sets scroll height, zero allocates/cycles312x192 bytes at53AFF; close actual refresh caller and App buffer lifetime, not actor mutation'),
            (0x31529, 'terminal_cinematic_resource_owner', 'complete1718B returns after its cinematic; close FDOTHER54/60/58/57/59, temporary framebuffer, palette save/restore, music and terminal App handoff; do not replace it with end or claim it is a nonreturning loop'),
            (0x111BA, 'temporary_audio_resource_owner', 'trace FDOTHER88 load/free and exact SFX/shake lifetime before source entry presentation'),
            (0x33F78, 'teleport_five_argument_scene_abi', 'complete55B is read: focus_move_to(args1,2), then22253(args0,1,2,1,2); reuse callback82 known ABI isolation rather than guessing a three-argument teleport or re-recovering the helper'),
            (0x35F10, 'defeated_tail_scene_loot_handoff', 'complete56B and existing common_event HP-tail/finalize semantics are known; close this Scene caller handoff to existing defeated/loot continuation and preserve preceding raw writes, rather than inventing a synchronous native wrapper')):
        if target in targets:
            families.append(family); next_steps.append(needed)
    if cfg['missing_shared_tails']:
        families.append('external_shared_tail'); next_steps.append('read complete shared tail spans ' + ','.join(cfg['missing_shared_tails']))
    if cfg['indirect_transfers']:
        families.append('indirect_scene_transfer'); next_steps.append('prove the target domain at ' + ','.join(cfg['indirect_transfers']))
    if not families:
        if binding['table'] == 'stage_victory' and binding['slot'] == 25:
            families = ['id_filtered_position_dynamic_text_source_adapter']
            next_steps = ['full2504d tail is recovered through15311; source16..count actor ID31 x16/y6 filter and state12+5/+8 text are known, but need bounded finite adapters or a legal shared cold expansion within254 nodes; no unknown CFG or new gameplay claimed']
        else:
            families = [initial['family']]
            next_steps = ['recover the flagged typed field/condition family and its finite consumer from this full CFG; never flatten the source branch']
    return {**initial, 'family': families[0], 'semantic_families': sorted(set(families)),
            'reason': 'full reachable caller CFG retained; ' + initial['reason'],
            'stage': 'S2', 'affected_binding': binding['source_id'],
            'known_reusable_dependencies': [{'target': hex(va), 'semantic': semantic} for va, semantic in
                ((0x233C6, 'typed layout/fades/view (closed in this S2)'), (0x24B14, 'first16 item prefix presence, returns1/-1'),
                 (0x24BDE, 'full roster text presence, same33499 meaning'), (0x35B78, 'pan/group/palette_add/waits (closed in this S2)')) if va in targets],
            'next_evidence': '; '.join(next_steps)}


def static_events(path, instructions, xrefs, stage, *, register_overrides=None, actor_writes=None, query_value=None, image=None, inline_events=None):
    """只识别有限调用语法的常量push／register-copy；未知值拒绝，不解释x86运行。"""
    registers = [None] * 8
    pushes, events = [], []
    # 仅追踪caller的栈常量数组复制：不解释运算、跳转或未知指针。
    stack_delta, stack_sources = 0, {}
    force = fade = 0
    for pc in path:
        if inline_events and pc in inline_events:
            events.append(inline_events[pc])
        if actor_writes and pc in actor_writes:
            events.extend(actor_writes[pc])
            continue
        insn = instructions[pc]
        raw = bytes.fromhex(insn['raw_hex'])
        mnemonic = insn['disasm'].split(None, 1)[0].lower()
        if raw[0] == 0x68:
            pushes.append(int.from_bytes(raw[1:5], 'little'))
            stack_delta -= 4
        elif raw[0] == 0x6A:
            pushes.append(int.from_bytes(raw[1:2], 'little', signed=True))
            stack_delta -= 4
        elif 0x50 <= raw[0] <= 0x57:
            pushes.append(registers[raw[0] - 0x50])
            stack_delta -= 4
        elif 0x58 <= raw[0] <= 0x5F:
            registers[raw[0] - 0x58] = None
            stack_delta += 4
        elif raw[:2] == b'\xff\x35':
            pushes.append('current_text_resource' if int.from_bytes(raw[2:6], 'little') == 0x53A79 else None)
            stack_delta -= 4
        elif 0xB8 <= raw[0] <= 0xBF:
            registers[raw[0] - 0xB8] = int.from_bytes(raw[1:5], 'little')
        elif raw == b'\xa1\x45\x3a\x05\0':
            registers[0] = ('actor_base', 0)
        elif raw[0] == 0x05 and isinstance(registers[0], tuple) and registers[0][0] == 'actor_base':
            registers[0] = ('actor_base', registers[0][1] + int.from_bytes(raw[1:5], 'little'))
        elif len(raw) >= 2 and raw[0] in (0x31, 0x33) and raw[1] >= 0xC0:
            a, b = (raw[1] >> 3) & 7, raw[1] & 7
            registers[a if raw[0] == 0x33 else b] = 0 if a == b else None
        elif len(raw) >= 2 and raw[0] in (0x89, 0x8B) and raw[1] >= 0xC0:
            a, b = (raw[1] >> 3) & 7, raw[1] & 7
            src, dst = (a, b) if raw[0] == 0x89 else (b, a)
            registers[dst] = ('stack', stack_delta) if src == 4 else registers[src]
        elif raw[:2] in (b'\x83\xec', b'\x83\xc4'):
            stack_delta += (-1 if raw[1] == 0xEC else 1) * int.from_bytes(raw[2:3], 'little', signed=True)
        elif raw[:2] in (b'\x81\xec', b'\x81\xc4'):
            stack_delta += (-1 if raw[1] == 0xEC else 1) * int.from_bytes(raw[2:6], 'little', signed=True)
        elif raw[:1] == b'\x8d' and len(raw) >= 3 and raw[1] & 0xC7 in (0x04, 0x44, 0x84) and raw[2] == 0x24:
            displacement = int.from_bytes(raw[3:], 'little', signed=True)
            registers[(raw[1] >> 3) & 7] = ('stack', stack_delta + displacement)
        elif raw in (b'\xa5', b'\x66\xa5', b'\xa4', b'\xf3\xa5'):
            if raw == b'\xf3\xa5' and (type(registers[1]) is not int or not 0 <= registers[1] <= 24):
                return None, {'family': 'layout_source_copy', 'va': hex(pc), 'reason': 'REP source count lacks a bounded literal'}
            count = registers[1] * 4 if raw == b'\xf3\xa5' else {b'\xa5': 4, b'\x66\xa5': 2, b'\xa4': 1}[raw]
            src, dst = registers[6], registers[7]
            if not isinstance(src, int) or not isinstance(dst, tuple) or dst[0] != 'stack':
                return None, {'family': 'layout_source_copy', 'va': hex(pc), 'reason': 'local array copy is not from an exact initialized EXE span'}
            for i in range(count): stack_sources[dst[1] + i] = src + i
            registers[6], registers[7] = src + count, ('stack', dst[1] + count)
            if raw == b'\xf3\xa5': registers[1] = 0
        elif raw[:2] in (b'\xc6\x00', b'\xc6\x40', b'\xc6\x80') and isinstance(registers[0], tuple) and registers[0][0] == 'actor_base':
            displacement = 0 if raw[1] == 0 else int.from_bytes(raw[2:-1], 'little', signed=True)
            offset = registers[0][1] + displacement
            if offset < 0 or offset // 80 >= 96 or offset % 80 not in (0, 1, 3, 5, 0x26):
                return None, {'family': 'private_actor_field', 'va': hex(pc), 'reason': 'literal actor store is outside confirmed typed fields'}
            events.append({'kind': 'actor_write', 'actor': offset // 80, 'offset': offset % 80,
                           'size': 1, 'value': raw[-1], 'source_va': hex(pc)})
        elif raw[:3] == b'\xc6\x44\x24':
            # caller局部标志的已证literal初始化；不是Game热字段或新运行变量。
            stack_sources.pop(stack_delta + int.from_bytes(raw[3:4], 'little', signed=True), None)
        elif mnemonic == 'mov' and '[' in insn['disasm'].split(',', 1)[0]:
            return None, {'family': 'private_hot_write', 'va': hex(pc), 'reason': 'indirect state write requires an explicit finite field/owner contract'}
        refs = xrefs.get(pc, [])
        writes = [r['to'] for r in refs if r['type'] == 'WRITE']
        for address in writes:
            if address not in (0x51A83, 0x53AFA, 0x53AFB, 0x53C03, 0x53AA9, 0x53AAD, 0x53AB1, 0x53AB5, 0x53AB9, 0x53ABD):
                return None, {'family': 'private_hot_write', 'va': hex(pc), 'address': hex(address), 'reason': 'global write outside the confirmed finite transition fields'}
            if raw[:2] == b'\xc7\x05':
                value, size = int.from_bytes(raw[6:10], 'little'), 4
            elif raw[:2] == b'\xc6\x05':
                value, size = raw[6], 1
            elif raw[:2] == b'\xff\x05' and address == 0x53C03:
                value, size = stage + 1, 4
            else:
                return None, {'family': 'unconfirmed_state_value', 'va': hex(pc), 'reason': 'nonliteral global write needs a reviewed semantic template'}
            events.append({'kind': 'write', 'source_va': hex(pc), 'address': hex(address), 'size': size, 'value': value})
            if address == 0x53AFA: force = value
            if address == 0x53AFB: fade = value
        if mnemonic in ('call', 'jmp'):
            targets = [r['to'] for r in refs if r['type'] in ('CALL_NEAR', 'JUMP_NEAR')]
            if len(targets) == 1 and targets[0] in LEAVES:
                op, count = LEAVES[targets[0]]
                if len(pushes) < count or any(value is None for value in pushes[-count:] if count):
                    return None, {'family': 'dynamic_call_operand', 'va': hex(pc), 'reason': 'call operand is not a proven literal/current-text reference'}
                args = list(reversed(pushes[-count:])) if count else []
                if targets[0] in (0x33499, 0x34894):
                    if query_value not in (0, 1):
                        return None, {'family': 'roster_condition', 'va': hex(pc), 'reason': 'source expression needs both verified roster predicate domains'}
                    events.append({'kind': 'query', 'target': hex(targets[0]), 'source_va': hex(pc),
                                   'op': op, 'args': args, 'result': query_value})
                else:
                    if targets[0] == 0x233C6:
                        if not image or any(type(value) is not int for value in args[3:]) or not 0 <= args[3] <= args[4] < 96:
                            return None, {'family': 'layout_source_domain', 'va': hex(pc), 'reason': 'layout requires fixed valid raw bounds and exact source arrays'}
                        for i in range(3):
                            if i == 2 and type(args[i]) is int and 0 <= args[i] < 4: continue
                            pointer = args[i]
                            if not isinstance(pointer, tuple) or pointer[0] != 'stack':
                                return None, {'family': 'layout_source_array', 'va': hex(pc), 'reason': 'layout array pointer is not a proven caller-local source copy'}
                            origins = [stack_sources.get(pointer[1] + k) for k in range(args[3], args[4] + 1)]
                            if any(type(va) is not int or not 0x50000 <= va < 0x54000 for va in origins) or origins != list(range(origins[0], origins[0] + len(origins))):
                                return None, {'family': 'layout_source_array', 'va': hex(pc), 'reason': 'layout bytes lack a contiguous initialized source span'}
                            args[i] = {'source_va': hex(origins[0]), 'raw_hex': image[origins[0]:origins[-1] + 1].hex(), 'first_raw': args[3]}
                    events.append({'kind': 'call', 'target': hex(targets[0]), 'source_va': hex(pc),
                                   'op': op, 'args': args, 'force': force, 'fade': fade})
                pushes.clear()
                for register in (0, 1, 2): registers[register] = None
                if targets[0] in (0x33499, 0x34894): registers[0] = query_value
            elif len(targets) == 1 and targets[0] == 0x3702F:
                pushes.clear()
                stack_delta += 4
        if register_overrides and pc in register_overrides:
            register, value = register_overrides[pc]
            registers[register] = value
    if force or fade:
        return None, {'family': 'unbalanced_transient_mode', 'reason': 'source leaves a temporary force/fade mode live; an explicit owner contract is required'}
    return events, None


def trace_projection(events):
    keys = {'call': ('kind', 'target', 'op', 'args', 'force', 'fade'),
            'query': ('kind', 'target', 'op', 'args', 'result'),
            'actor_write': ('kind', 'actor', 'offset', 'size', 'value'),
            'write': ('kind', 'address', 'size', 'value')}
    return [{key: event[key] for key in keys[event['kind']]} for event in events]


def compare_original_trace(expected, actual, fixture=None):
    source = json.loads(json.dumps(expected))
    runtime = json.loads(json.dumps(actual))
    expanded = []
    for event in source:
        if event['kind'] == 'filter_position':
            first, identity, x, y = event['args']
            for actor in range(first, fixture['actor_count']):
                if fixture.get('actor_ids', {}).get(actor, 0) == identity:
                    for offset, value in ((0, x), (1, y)):
                        expanded.append({'kind': 'actor_write', 'actor': actor, 'offset': offset, 'size': 1, 'value': value})
        else:
            if event['kind'] == 'call' and event['op'] == 'text' and isinstance(event['args'][1], dict):
                expression = event['args'][1]
                event['args'][1] = fixture.get('event_bytes', {}).get(expression['event_index'], 0) + expression['add']
            expanded.append(event)
    source = expanded
    for events in (source, runtime):
        for event in events:
            if event['kind'] == 'call' and event['op'] == 'text':
                event['args'][0] = 'current_text_resource'
    if trace_projection(source) != trace_projection(runtime):
        raise ValueError('static lowering differs from independent original caller execution')


def execute_linear(image, entry, stage, allowed, *, actor_count=96, roster_count=2, cursor=0,
                   actor_writes=False, roster_text_ids=(), native_args=(), roster_flags=0, actor_flags=None,
                   native_helpers=(), extra_leaves=None, raw_actors=None, inline_queries=None,
                   event_bytes=None, turn=1, actor_hpmax=None, view_writes=False, actor_ids=None):
    """执行原机器码；已证叶显式替身，只取调用顺序、参数与本函数写点。"""
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX

    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(0x100000, 0x20000)
    cpu.mem_map(0x200000, 0x20000)
    stop, stack = 0x400000, 0x210000
    word = lambda at: struct.unpack("<I", cpu.mem_read(at, 4))[0]
    def put(at, value):
        cpu.mem_write(at, struct.pack("<I", value))
    for at, value in ((0x53A45, 0x101000), (0x53BF7, 0x104000), (0x53AD5, 0x108000),
                      (0x53A79, 0x109000), (0x53BEB, actor_count), (0x53BFB, roster_count),
                      (0x53C03, stage), (0x51A83, cursor), (0x53AFB, 0), (0x53BEF, turn)):
        put(at, value)
    cpu.mem_write(0x53AFA, b"\0")
    if raw_actors is not None:
        if len(raw_actors) != 96 * 80: raise ValueError('native actor fixture must preserve all raw slots')
        cpu.mem_write(0x101000, raw_actors)
    for actor, flags in (actor_flags or {}).items():
        if not 0 <= actor < 96 or not 0 <= flags <= 255: raise ValueError('invalid actor flag fixture')
        cpu.mem_write(0x101000 + actor * 80 + 5, bytes([flags]))
    for actor, hp in (actor_hpmax or {}).items(): cpu.mem_write(0x101000 + actor * 80 + 0x42, struct.pack('<H', hp))
    for index, value in (event_bytes or {}).items(): cpu.mem_write(0x108000 + index, bytes([value]))
    for actor, value in (actor_ids or {}).items(): cpu.mem_write(0x101000 + actor * 80 + 7, bytes([value]))
    for i, text_id in enumerate(roster_text_ids):
        if i >= roster_count or not 0 <= text_id <= 255: raise ValueError('invalid predicate fixture')
        cpu.mem_write(0x104000 + i * 80 + 8, bytes([text_id]))
        cpu.mem_write(0x104000 + i * 80 + 5, bytes([roster_flags]))
    put(stack, stop)
    for i, value in enumerate(native_args): put(stack + 4 + i * 4, value)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    events, unknown, pending_queries = [], [], {}
    stack_origins, source_reads = {}, {}
    active = False
    def hook(machine, address, size, _):
        nonlocal active
        active = True
        sp = cpu.reg_read(UC_X86_REG_ESP)
        if address in pending_queries:
            pending_queries.pop(address)['result'] = cpu.reg_read(UC_X86_REG_EAX)
        if inline_queries and address in inline_queries:
            query = dict(inline_queries[address]);capture = query.pop('capture')
            value = cpu.reg_read(UC_X86_REG_EAX) if capture[0] == 'eax' else word(capture[1])
            comparison, threshold = capture[-2:]
            query['result'] = int({'eq': value == threshold, 'gt': value > threshold, 'ge': value >= threshold}[comparison])
            events.append(query)
        if address == 0x3702F:
            cpu.reg_write(UC_X86_REG_EIP, word(sp))
            cpu.reg_write(UC_X86_REG_ESP, sp + 8)
            return
        if address in (0x33499, 0x34894) and address != entry and address not in native_helpers:
            event = {'kind': 'query', 'target': hex(address), 'op': LEAVES[address][0],
                     'args': [word(sp + 4)], 'result': None}
            events.append(event)
            pending_queries[word(sp)] = event
        elif address in {**LEAVES, **(extra_leaves or {})} and address != entry and address not in native_helpers:
            op, count = {**LEAVES, **(extra_leaves or {})}[address]
            args = [word(sp + 4 + i * 4) for i in range(count)]
            if address == 0x233C6:
                if not 0 <= args[3] <= args[4] < 96: raise ValueError('native layout invalid source bounds')
                for i in range(3):
                    if i == 2 and args[i] < 4: continue
                    start, length = args[i] + args[3], args[4] - args[3] + 1
                    origins = [stack_origins.get(start + k) for k in range(length)]
                    if any(va is None for va in origins) or origins != list(range(origins[0], origins[0] + length)):
                        raise ValueError('native caller local array lacks original EXE read provenance')
                    args[i] = {'source_va': hex(origins[0]), 'raw_hex': bytes(cpu.mem_read(start, length)).hex(), 'first_raw': args[3]}
            events.append({"kind": "call", "target": hex(address), "op": op,
                           "args": args, "return_va": hex(word(sp)),
                           "force": cpu.mem_read(0x53AFA, 1)[0], "fade": word(0x53AFB)})
            cpu.reg_write(UC_X86_REG_EIP, word(sp))
            cpu.reg_write(UC_X86_REG_ESP, sp + 4)
            cpu.reg_write(UC_X86_REG_EAX, 0)
            return
        if address not in allowed:
            unknown.append({"va": hex(address), "reason": "execution escaped complete reviewed straight-line path"})
            machine.emu_stop()
    def write(machine, access, address, size, value, _):
        if not active or 0x200000 <= address < address + size <= 0x220000:
            read = source_reads.get(cpu.reg_read(UC_X86_REG_EIP))
            if read and read[1] == size:
                for i in range(size): stack_origins[address + i] = read[0] + i
            return
        if actor_writes and 0x101000 <= address < address + size <= 0x101000 + 96 * 80:
            offset = address - 0x101000
            events.append({'kind': 'actor_write', 'source_va': hex(cpu.reg_read(UC_X86_REG_EIP)),
                           'actor': offset // 80, 'offset': offset % 80, 'size': size, 'value': value})
        elif address in (0x51A83, 0x53AFA, 0x53AFB, 0x53C03) or ((entry == 0x233C6 or view_writes) and address in (0x53AA9, 0x53AAD, 0x53AB1, 0x53AB5, 0x53AB9, 0x53ABD)):
            events.append({"kind": "write", "source_va": hex(cpu.reg_read(UC_X86_REG_EIP)),
                           "address": hex(address), "size": size, "value": value})
        else:
            unknown.append({"va": hex(cpu.reg_read(UC_X86_REG_EIP)), "address": hex(address),
                            "size": size, "reason": "private hot write lacks a finite operation contract"})
            machine.emu_stop()
    def read(machine, access, address, size, value, _):
        if 0x50000 <= address < address + size <= 0x54000:
            source_reads[cpu.reg_read(UC_X86_REG_EIP)] = (address, size)
    cpu.hook_add(unicorn.UC_HOOK_CODE, hook)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write)
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ, read)
    cpu.emu_start(entry, stop, count=100000)
    if not unknown and cpu.reg_read(UC_X86_REG_EIP) != stop:
        raise ValueError("linear source did not reach its return boundary")
    return {"events": events, "blocker": unknown[0] if unknown else None,
            "eax": cpu.reg_read(UC_X86_REG_EAX),
            "actors_hex": bytes(cpu.mem_read(0x101000, 96 * 80)).hex() if entry == 0x233C6 else None,
            "view_fields": [word(va) for va in (0x51A83, 0x53AA9, 0x53AAD, 0x53AB1, 0x53AB5, 0x53AB9, 0x53ABD)] if entry == 0x233C6 else None,
            "scope": "original caller execution with explicit leaf substitutions; no UI/resources/RNG execution"}


def lower_events(events, stage, kind):
    actions, origins = [], []
    view_prefix = []
    def add(op, args, source, views=('logic', 'present')):
        actions.append({'id': f'n{len(actions):03d}', 'op': op, 'args': args,
                        'on': {'next': f'n{len(actions) + 1:03d}'}, 'views': list(views)})
        origins.append(source)
    pending_actor = None
    def flush_actor():
        nonlocal pending_actor
        if pending_actor:
            first, last, field, value, source = pending_actor
            add('unit_field_write', [first, last, field, 0, value], source)
            pending_actor = None
    for event in events:
        source = event.get('source_va')
        if event['kind'] == 'filter_position':
            flush_actor();add('unit_id_position', event['args'], source)
            continue
        if event['kind'] == 'write' and event['address'] in ('0x53aa9', '0x53aad', '0x53ab1', '0x53ab5', '0x53ab9', '0x53abd'):
            view_prefix.append(event)
            if len(view_prefix) == 6:
                if [e['address'] for e in view_prefix] != ['0x53aa9', '0x53aad', '0x53ab1', '0x53ab5', '0x53ab9', '0x53abd'] or [e['value'] for e in view_prefix[-2:]] != [0, 0]:
                    raise ValueError('inline view write order/domain differs')
                add('view_set', [e['value'] for e in view_prefix[:4]], view_prefix[0]['source_va'])
                view_prefix.clear()
            continue
        if view_prefix: raise ValueError('inline view prefix has an intervening observable leaf')
        if event['kind'] == 'actor_write':
            if event['size'] != 1 or event['offset'] not in (0, 1, 3, 5, 0x26):
                raise ValueError('actor write is outside the reviewed typed field family')
            field = {0: 0, 1: 1, 3: 2, 5: 4, 0x26: 12}[event['offset']]
            if pending_actor and pending_actor[1] + 1 == event['actor'] and pending_actor[2:4] == [field, event['value']]:
                pending_actor[1] = event['actor']
            else:
                flush_actor()
                pending_actor = [event['actor'], event['actor'], field, event['value'], source]
            continue
        flush_actor()
        if event['kind'] == 'write':
            if event['address'] == '0x51a83': add('display_mode', [event['value']], source)
            elif event['address'] == '0x53c03': add('next_stage', [event['value']], source, ('logic',))
            elif event['address'] not in ('0x53afa', '0x53afb'):
                raise ValueError('unlowered original state write')
            continue
        op, args = event['op'], event['args']
        if op == 'stage_setup_source':
            # 固定源helper的编排只保一份cold子图，不在native handler藏整段。
            add('call_init', [], source, ('present',))
            continue
        elif op == 'text':
            if args[0] != 'current_text_resource' or args[2:] != [655360, 320, 205, 76, 74, 19, 1]:
                raise ValueError('text call has an unconfirmed source context/geometry')
            if isinstance(args[1], dict):
                add('text_state_offset', [args[1]['event_index'], args[1]['add']], source, ('present',))
                continue
            args = [args[1]]
        elif op == 'move': args = [args[0], event['fade']]
        elif op == 'append_group': args = [args[0], event['force'], 0]
        elif op == 'clear_screen':
            if args != [655360, 0, 64000]: raise ValueError('source memset is not the confirmed VGA page clear')
            args = []
        elif op == 'camera_group_flash':
            for leaf, values in [('pan', args[:2]), ('append_group', [args[2] & 255, event['force'], 0]),
                                 ('delay', [300]), ('palette_add', [0, 255, 255]), ('delay', [200]),
                                 ('palette_add', [0, 255, 0]), ('refresh', [0]), ('delay', [400])]:
                add(leaf, values, '0x35b78:full_source_leaf_order', ('logic', 'present') if leaf == 'append_group' else ('present',))
            continue
        elif op == 'layout':
            first, last, special, sx, sy, direction, cx, cy = args[3:]
            add('call_fade_out', [], source, ('present',))
            add('clear_acted', [], '0x233e5:0x13536')
            for actor in range(first, last + 1):
                for field in range(3):
                    array = args[field]
                    value = bytes.fromhex(array['raw_hex'])[actor - first] if isinstance(array, dict) else array
                    origin = f"{array['source_va']}+{actor - first}" if isinstance(array, dict) else source
                    add('unit_field_write', [actor, actor, field, 0, value], origin)
            if special:
                for field, value in enumerate((sx, sy, direction)):
                    add('unit_field_write', [special, special, field, 0, value & 255], source)
            add('display_mode', [0], '0x2345b')
            add('view_set', [cx, cy, cx, cy], '0x23469..0x23493')
            add('refresh', [1], '0x2349f', ('present',))
            add('call_fade_in', [], '0x234a7', ('present',))
            add('delay', [200], '0x234b1', ('present',))
            continue
        add(op, args, source, ('present',) if op in ('text', 'delay', 'refresh') else ('logic', 'present'))
        if op == 'reset_directions': add('sleep_ms', [20], '0x134e4:confirmed_internal_wait', ('present',))
    flush_actor()
    if view_prefix: raise ValueError('incomplete inline view writes')
    actions.append({'id': f'n{len(actions):03d}', 'op': 'end', 'args': [], 'on': {}})
    origins.append('original_return_boundary')
    return {'id': f'stage.source.{kind}.{stage:03d}', 'entry': 'n000', 'nodes': actions}, origins


def shared_flows():
    def build(identity, actions):
        nodes = [{'id': f'n{i:03d}', 'op': op, 'args': args,
                  'on': {'next': f'n{i + 1:03d}'}, 'views': ['logic', 'present']}
                 for i, (op, args) in enumerate(actions)]
        nodes.append({'id': f'n{len(nodes):03d}', 'op': 'end', 'args': [], 'on': {}})
        return {'id': identity, 'entry': 'n000', 'nodes': nodes}
    initialization = [('display_mode', [0]), ('entry_result_reset', []), ('stage_setup_source', []),
                      ('append_group', [0, 0, 0]), # 1088d@10b3c..10b43；不藏进整段native handler
                      ('event_byte_write', [0, 31, 0, 0]), ('entry_view_reset', []), ('refresh', [1]),
                      ('display_mode', [1]), ('call_fade_in', []), ('entry_turn_reset', []), ('clear_keys', [])]
    fade = lambda values: [(op, args) for dark in values for op, args in
                           [('palette_subtract', [0, 255, dark]), ('delay', [2])]]
    return [build('stage.source.initialize', initialization),
            build('stage.source.fade.in', fade(range(64, -1, -1))),
            build('stage.source.fade.out', fade(range(64)))]


SHARED_CALLS = {'call_init': 'stage.source.initialize', 'call_fade_in': 'stage.source.fade.in',
                'call_fade_out': 'stage.source.fade.out'}


def validate_shared_calls(flows):
    """固定源helper调用只借现Scene深度／预算，不引入任意callee或热执行栈。"""
    by_id = {flow['id']: flow for flow in flows}
    if len(by_id) != len(flows): raise ValueError('duplicate transition flow')
    def visit(identity, ancestry=()):
        if identity in ancestry or len(ancestry) >= 8: raise ValueError('shared source call cycle/depth')
        flow = by_id.get(identity)
        if flow is None: raise ValueError('shared source helper missing')
        total = len(flow['nodes'])
        for node in flow['nodes']:
            if node['op'] in SHARED_CALLS:
                if node['args']: raise ValueError('fixed shared source call takes no operands')
                total += visit(SHARED_CALLS[node['op']], ancestry + (identity,))
        if total > 65536: raise ValueError('expanded transition flow exceeds existing source budget')
        return total
    return {identity: visit(identity) for identity in by_id}


def reviewed_cases(binding, instructions, xrefs, image=None):
    """全原码身份固定后的有限模板；只恢复已读清的条件/字段，不猜私有CALL。"""
    entry, stage, table = int(binding['target_va'], 16), binding['slot'], binding['table']
    if table == 'stage_victory' and stage == 25:
        path, _, blocker = linear_path(entry, instructions, xrefs, {0x24ECC: 0x24EF2})
        if blocker: raise ValueError('ID-filter/dynamic-text caller tail changed')
        fixed = {0x24EC6: '3b15eb3b0500', 0x24EE4: '83fb1f', 0x24EE9: 'c60010',
                 0x24EEC: 'c6400106', 0x24F35: '0fb6400c', 0x24F39: '83c005',
                 0x24FB6: '0fb6400c', 0x24FBA: '83c008'}
        if any(instructions[pc]['raw_hex'] != raw for pc, raw in fixed.items()):
            raise ValueError('filtered position/dynamic source operands differ')
        events, blocker = static_events(path, instructions, xrefs, stage, image=image,
            actor_writes={0x24ECC: [{'kind': 'filter_position', 'args': [16, 31, 16, 6], 'source_va': '0x24ec6..0x24ef0'}]},
            register_overrides={0x24F39: (0, {'event_index': 12, 'add': 5}), 0x24FBA: (0, {'event_index': 12, 'add': 8})})
        if blocker: raise ValueError(f'dynamic source caller operands differ: {blocker}')
        allowed = {int(va, 16) for va in recover_caller_cfg(entry, instructions, xrefs)['instruction_vas']}
        return [{'events': events, 'blocker': None, 'actor_writes': True, 'query': None, 'allowed': allowed,
                 'fixtures': [{'actor_count': count, 'actor_ids': {15: 31, 16: 31, 18: 30, 23: 31, 95: 31},
                    'actor_flags': {16: 1, 23: 0x80}, 'event_bytes': {12: value}}
                    for count, value in ((24, 0), (24, 1), (96, 255))]}]
    def inline(pc, op, args, result, capture):
        return {'kind': 'query', 'target': f'inline:{hex(pc)}', 'source_va': hex(pc),
                'op': op, 'args': args, 'result': result, 'capture': capture}
    if table == 'stage_victory' and stage in (9, 19):
        groups = ([(0, 11, 0x52113, 0x5211E, 2, 0x23657)] if stage == 9 else
                  [(0, 16, 0x521F6, 0x52206, 1, 0x23EEF), (52, 9, 0x52216, 0x5221F, 3, 0x23F1F)])
        branches = {pc: (0x23659 if stage == 9 else 0x23EF1 if pc == 0x23EEF else 0x23F21)
                    for _, _, _, _, _, pc in groups}
        writes = {}
        for first, count, x, y, direction, pc in groups:
            # source memcpy operands + loop compare/store are strict-readset bytes, not nearby data guesses.
            for address in (x, y):
                if not any(i['raw_hex'] == (b'\xbe' + struct.pack('<I', address)).hex() for i in instructions.values()):
                    raise ValueError('inline source array copy identity differs')
            if bytes.fromhex(instructions[pc - 3]['raw_hex']) != bytes((0x83, 0xF8, count)):
                raise ValueError('inline layout loop count differs')
            writes[pc] = [{'kind': 'actor_write', 'actor': first + i, 'offset': field, 'size': 1,
                           'value': image[address + i] if address else direction, 'source_va': hex(address + i) if address else hex(pc)}
                          for i in range(count) for field, address in ((0, x), (1, y), (3, 0))]
        cases = []
        for late in ((0,) if stage == 9 else (0, 1)):
            source_branches = {**branches, **({0x24005: 0x240C6 if late else 0x2400B} if stage == 19 else {})}
            path, _, blocker = linear_path(entry, instructions, xrefs, source_branches)
            if blocker: raise ValueError('inline layout source CFG changed')
            inline_rows = {0x24005: inline(0x24005, 'branch_turn', [4, 15], late, ('memory', 0x53BEF, 'gt', 15))} if stage == 19 else {}
            events, blocker = static_events(path, instructions, xrefs, stage, image=image, actor_writes=writes,
                inline_events={pc: {k: v for k, v in row.items() if k != 'capture'} for pc, row in inline_rows.items()})
            if blocker: raise ValueError(f'inline layout state writes differ: {blocker}')
            allowed = {int(va, 16) for va in recover_caller_cfg(entry, instructions, xrefs)['instruction_vas']}
            cases.append({'events': events, 'blocker': None, 'actor_writes': True, 'view_writes': True,
                'query': next((e for e in events if e['kind'] == 'query'), None), 'allowed': allowed,
                'inline_queries': {pc: {k: v for k, v in row.items() if k != 'result'} for pc, row in inline_rows.items()},
                'fixtures': [{'actor_count': 53 if stage == 9 else 61, 'turn': 16 if late else 15},
                             {'actor_count': 96, 'turn': 0x7FFFFFFF if late else 1}]})
        return cases
    if table == 'stage_victory' and stage in (6, 15):
        cases = []
        combinations = ((0, 0, 0), (1, 1, 0), (1, 0, 0)) if stage == 6 else ((1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, 0))
        for a, b, c in combinations:
            if stage == 6:
                branches = {0x2332A: 0x2332C if a else 0x23393,
                            0x23338: 0x23393 if b else 0x2333A}
                inline_rows = {0x23327: inline(0x23327, 'branch_event_byte', [17, 0, 1], a, ('eax', 'eq', 1))}
                fixtures = [{'actor_count': count, 'event_bytes': {17: value}, 'actor_flags': {43: b}}
                            for count, value in ((44, 1 if a else 0), (96, 1 if a else 2), (96, 1 if a else 255))]
                native_helpers = ()
                query_value = b
            else:
                # 原8次hidden调用仍在oracle完整执行；cold复用一次typed范围计数。
                branches = {0x23A6E: 0x23A86, 0x23A8E: 0x23A90 if a else 0x23A95,
                            0x23AA1: 0x23ABD if b else 0x23AA3,
                            0x23AAB: 0x23ABD if a else 0x23AAD,
                            0x23ABB: 0x23B21 if c else 0x23ABD}
                inline_rows = {0x23A8E: inline(0x23A8E, 'branch_hidden_count', [66, 73, 4, 4], a, ('eax', 'gt', 4))}
                if not a:
                    inline_rows[0x23AA1] = inline(0x23AA1, 'branch_turn', [4, 18], b, ('memory', 0x53BEF, 'gt', 18))
                    if not b: inline_rows[0x23ABB] = inline(0x23ABB, 'branch_actor', [0, 5, 5, 320], c, ('eax', 'ge', 320))
                fixtures = [{'actor_count': count, 'actor_flags': {66 + i: int(i < hidden_count) for i in range(8)},
                             'turn': 19 if b else 18, 'actor_hpmax': {0: 320 if c else 319}}
                            for count, hidden_count in ((74, 5 if a else 0), (96, 8 if a else 4))]
                native_helpers = (0x34894,)
                query_value = None
            path, _, blocker = linear_path(entry, instructions, xrefs, branches)
            if blocker: raise ValueError('reviewed composed transition CFG changed')
            events, blocker = static_events(path, instructions, xrefs, stage, image=image,
                query_value=query_value, inline_events={pc: {k: v for k, v in row.items() if k != 'capture'} for pc, row in inline_rows.items()})
            if blocker: raise ValueError(f'reviewed composed transition operands differ: {blocker}')
            actual_inline = {pc: {k: v for k, v in row.items() if k != 'result'} for pc, row in inline_rows.items() if pc in path}
            allowed = set(recover_caller_cfg(entry, instructions, xrefs)['instruction_vas'])
            allowed = {int(va, 16) for va in allowed} | {va for va in instructions if 0x34894 <= va < 0x348BB}
            cases.append({'events': events, 'blocker': None, 'actor_writes': False,
                          'query': next(e for e in events if e['kind'] == 'query'), 'allowed': allowed,
                          'fixtures': fixtures, 'inline_queries': actual_inline, 'native_helpers': native_helpers})
        return cases
    if table == 'stage_victory' and stage == 2:
        cases = []
        for hidden in (1, 0):
            path, _, blocker = linear_path(entry, instructions, xrefs, {0x23137: 0x23139 if hidden else 0x23165})
            if blocker: raise ValueError('victory2 full source branch changed')
            events, blocker = static_events(path, instructions, xrefs, stage, query_value=hidden, image=image)
            query = next((event for event in events or () if event['kind'] == 'query'), None)
            if not query: raise ValueError('victory2 hidden predicate missing')
            cases.append({'events': events, 'blocker': blocker, 'actor_writes': False, 'query': query,
                          'allowed': set(path) | {va for va in instructions if 0x34894 <= va < 0x348BB}})
        return cases
    if table == 'stage_entry' and stage == 8:
        suffix, _, blocker = linear_path(0x332AC, instructions, xrefs)
        if blocker: raise ValueError('fixed direction template tail changed')
        prefix = [va for va in sorted(instructions) if entry <= va < 0x3328D]
        count = bytes.fromhex(instructions[0x332A7]['raw_hex'])[-1]
        store = bytes.fromhex(instructions[0x332A1]['raw_hex'])
        if count != 11 or store[-2:] != b'\x03\x02':
            raise ValueError('fixed direction source loop differs')
        writes = [{'kind': 'actor_write', 'actor': actor, 'offset': store[-2], 'size': 1,
                   'value': store[-1], 'source_va': '0x332a1'} for actor in range(count)]
        events, blocker = static_events(prefix + [0x3328D] + suffix, instructions, xrefs, stage,
                                       actor_writes={0x3328D: writes})
        return [{'events': events, 'blocker': blocker, 'actor_writes': True, 'query': None,
                 'allowed': {va for va in instructions if entry <= va < 0x3332B}}]
    if table == 'stage_entry' and stage == 9:
        path, _, blocker = linear_path(entry, instructions, xrefs)
        if blocker: return None
        writes = {}
        for add_va, store_va in ((0x3334B, 0x33350), (0x33359, 0x3335E)):
            actor_offset = int.from_bytes(bytes.fromhex(instructions[add_va]['raw_hex'])[-4:], 'little')
            raw = bytes.fromhex(instructions[store_va]['raw_hex'])
            if actor_offset % 80 or raw[:2] != b'\xc6\x40': raise ValueError('literal actor field template differs')
            writes[store_va] = [{'kind': 'actor_write', 'actor': actor_offset // 80,
                                'offset': raw[2], 'size': 1, 'value': raw[3], 'source_va': hex(store_va)}]
        events, blocker = static_events(path, instructions, xrefs, stage, actor_writes=writes)
        return [{'events': events, 'blocker': blocker, 'actor_writes': True, 'query': None, 'allowed': set(path)}]
    if (table, stage) not in (('stage_entry', 14), ('stage_entry', 16), ('stage_victory', 14), ('stage_victory', 16)):
        return None
    cases = []
    native_predicate = {va for va in instructions if 0x33499 <= va < 0x334D9}
    for present in (1, 0):
        branch = ({0x335C5: 0x3344D if present else 0x335CB} if table == 'stage_entry' and stage == 16 else
                  {0x23B9D: 0x23B9F if present else 0x23BEA} if table == 'stage_victory' and stage == 16 else None)
        path, _, blocker = linear_path(entry, instructions, xrefs, branch)
        if blocker: raise ValueError('reviewed roster template control flow changed')
        overrides = {}
        if table == 'stage_entry' and stage == 14:
            scale = bytes.fromhex(instructions[0x334F5]['raw_hex'])[-1]
            base = (present ^ 1) * scale
            overrides = {0x33510: (3, base), 0x33545: (0, base + 1), 0x33582: (3, base + 2)}
        elif table == 'stage_victory' and stage == 14:
            # 239bd 的xor1/add12/movzx：只用0/1返回域，不增通用表达式操作。
            fragment = (present ^ 1) + 12
            for va in path:
                raw = bytes.fromhex(instructions[va]['raw_hex'])
                if instructions[va]['disasm'].startswith('movzx'):
                    overrides[va] = ((raw[-1] >> 3) & 7, fragment)
        events, blocker = static_events(path, instructions, xrefs, stage,
                                       register_overrides=overrides, query_value=present, image=image)
        query = next((event for event in events or () if event['kind'] == 'query'), None)
        if not query: raise ValueError('reviewed source lacks its native roster predicate')
        cases.append({'events': events, 'blocker': blocker, 'actor_writes': False, 'query': query,
                      'allowed': set(path) | native_predicate})
    return cases


def lower_cases(cases, stage, kind):
    """有限已证条件树：保持叶写序，只合并共同前缀，不新增变量或任意表达式。"""
    nodes, origins = [], []
    def append_events(events):
        lowered, sources = lower_events(events, stage, kind)
        for old, source in zip(lowered['nodes'][:-1], sources[:-1]):
            number = len(nodes)
            nodes.append({**old, 'id': f'n{number:03d}', 'on': {'next': f'n{number+1:03d}'}})
            origins.append(source)
    def emit(paths):
        first = paths[0]
        common = 0
        while common < min(map(len, paths)) and first[common]['kind'] != 'query':
            if any(trace_projection([path[common]]) != trace_projection([first[common]]) for path in paths[1:]):
                raise ValueError('source cases differ before their explicit condition')
            common += 1
        entry = f'n{len(nodes):03d}'
        append_events(first[:common])
        tails = [path[common:] for path in paths]
        if not tails[0]:
            if any(tail for tail in tails): raise ValueError('source path returned before an unmodeled condition')
            nodes.append({'id': f'n{len(nodes):03d}', 'op': 'end', 'args': [], 'on': {}})
            origins.append('original_return_boundary')
            return entry
        query = tails[0][0]
        if query['kind'] != 'query' or any(tail[0]['kind'] != 'query' or tail[0]['op'] != query['op'] or tail[0]['args'] != query['args'] for tail in tails):
            raise ValueError('source cases lack a common explicit predicate')
        query_args = query['args'] * 2 if query['op'] == 'branch_any_hidden' else query['args']
        branch = {'id': f'n{len(nodes):03d}', 'op': query['op'], 'args': query_args, 'on': {}, 'views': ['logic', 'present']}
        nodes.append(branch);origins.append(query['source_va'])
        for result, port in ((1, 'yes'), (0, 'no')):
            group = [tail[1:] for tail in tails if tail[0]['result'] == result]
            if not group: raise ValueError('reviewed predicate lacks one complete source arm')
            branch['on'][port] = emit(group)
        if query['op'] != 'branch_any_hidden':
            branch['on']['error'] = f'n{len(nodes):03d}'
            nodes.append({'id': f'n{len(nodes):03d}', 'op': 'fail', 'args': [], 'on': {}, 'views': ['logic', 'present']})
            origins.append('invalid_runtime_domain_explicit_rejection')
        return entry
    entry = emit([case['events'] for case in cases])
    return {'id': f'stage.source.{kind}.{stage:03d}', 'entry': entry, 'nodes': nodes}, origins


def layout_oracles(image, instructions, flows, probes):
    """完整233c6/13536原指令效果，供真实C consumer逐字节比较；呈现叶另账。"""
    allowed = {va for va in instructions if 0x233C6 <= va < 0x234BB or 0x13536 <= va < 0x13565 or 0x22BBE <= va < 0x22BC6}
    actual_layouts = {}
    for probe in probes:
        for event in probe['events']:
            if event['kind'] == 'call' and event['op'] == 'layout':
                key = json.dumps(event['args'], sort_keys=True)
                actual_layouts[key] = event['args']
    vectors = []
    for args in actual_layouts.values():
        matching = None
        for flow in flows:
            for i, node in enumerate(flow['nodes']):
                if node['op'] != 'call_fade_out': continue
                # exact lowered node slice identifies its source input, not actor values guessed nearby.
                lowered, _ = lower_events([{'kind': 'call', 'op': 'layout', 'args': args, 'source_va': None}], 0, 'victory')
                expected = [(n['op'], n['args']) for n in lowered['nodes'][:-1]]
                if [(n['op'], n['args']) for n in flow['nodes'][i:i + len(expected)]] == expected:
                    matching = (flow['id'], i); break
            if matching: break
        if not matching: raise ValueError('source layout has no exact cold node consumer')
        native = [int(a['source_va'], 16) - a['first_raw'] if isinstance(a, dict) else a for a in args]
        for count in sorted({max(args[4] + 1, args[5] + 1), 96}):
            for seed in (0, 1):
                raw = bytearray((i * 17 + seed * 97) & 255 for i in range(96 * 80))
                for i in range(96):
                    raw[i * 80 + 5] = (0x80 if seed else 0) | (i % 8)
                    raw[i * 80 + 0x40:i * 80 + 0x42] = b'\0\0' # HP0也不跳过clear/layout
                actual = execute_linear(image, 0x233C6, 0, allowed, actor_count=count,
                    actor_writes=True, native_args=native, raw_actors=bytes(raw),
                    native_helpers=(0x13536,),
                    extra_leaves={0x1F882: ('call_fade_out', 0), 0x1F525: ('call_fade_in', 0)})
                if actual['blocker']: raise ValueError('full native layout escaped strict source domain')
                vectors.append({'flow_id': matching[0], 'start_node': matching[1], 'actor_count': count,
                                'input_actors_hex': raw.hex(), 'source_args': args, **actual})
    return vectors


def flash_oracles(image, readset):
    function = next(row for row in readset['functions'] if row['va'] == 0x35B78)
    allowed = {row['ea'] for row in function['instructions']}
    vectors = []
    for args in ([0, 0, 0], [14, 17, 1], [255, 255, 255], [4, 11, 0x106]):
        native = execute_linear(image, 0x35B78, 0, allowed, native_args=args,
                                extra_leaves={0x11DF2: ('palette_add', 3)})
        lowered, _ = lower_events([{'kind': 'call', 'op': 'camera_group_flash', 'args': args,
                                   'source_va': '0x35b78', 'force': 0}], 0, 'entry')
        expected = [(n['op'], n['args'][:1] if n['op'] == 'append_group' else n['args']) for n in lowered['nodes'][:-1]]
        actual = [(event['op'], event['args']) for event in native['events']]
        if native['blocker'] or expected != actual:
            raise ValueError('cold shared camera/group/flash order differs from full original35b78')
        vectors.append({'arguments': args, 'native_events': native['events'], 'cold_operations': expected})
    return vectors


def deduplicate_source_flows(flows):
    """只共用整图全等的已证源定义；caller地址/域仍留在各row，不按target猜别名。"""
    unique, bindings, signatures = [], {}, {}
    for flow in flows:
        signature = json.dumps({key: value for key, value in flow.items() if key != 'id'}, sort_keys=True)
        if signature in signatures:
            bindings[flow['id']] = signatures[signature]
        else:
            signatures[signature] = flow['id'];unique.append(flow)
    return unique, bindings


def convert(readset, image):
    instructions, xrefs = source_index(readset, image)
    rows, flows, probes = [], [], []
    for binding in readset['bindings']:
        stage, entry = binding['slot'], int(binding['target_va'], 16)
        kind = 'entry' if binding['table'] == 'stage_entry' else 'victory'
        row = {**binding, 'disposition': 'unresolved', 'flow_id': None,
               'new_runtime_binding': False, 'caller_domain': {'stage_equals_table_slot': stage,
                    'initial_group_force': 0, 'initial_movement_fade': 0},
               'blocker': None, 'reachable_instruction_vas': []}
        row['caller_cfg'] = recover_caller_cfg(entry, instructions, xrefs)
        # 复杂但既有完整原义／正式图继续引用；不以窄解析器重新宣布未知。
        if (kind, stage) in (('entry', 0), ('victory', 0), ('victory', 1)):
            if not binding['existing_definition']:
                raise ValueError('expected exact existing transition definition missing')
            row['disposition'] = 'existing_definition_reference'
            rows.append(row)
            continue
        cases = reviewed_cases(binding, instructions, xrefs, image)
        blocker = None
        if cases is None:
            path, calls, blocker = linear_path(entry, instructions, xrefs)
            row['reachable_instruction_vas'] = [hex(va) for va in path]
            if not blocker:
                events, blocker = static_events(path, instructions, xrefs, stage, image=image)
                cases = [{'events': events, 'blocker': blocker, 'actor_writes': any(e['kind'] == 'actor_write' for e in events or ()),
                          'query': None, 'allowed': set(path)}]
        else:
            row['reachable_instruction_vas'] = [hex(va) for va in sorted(set().union(*(case['allowed'] for case in cases)))]
            row['reviewed_template'] = 'typed_actor_range' if cases[0]['actor_writes'] else ('hidden_layout_two_paths' if kind == 'victory' and stage == 2 else 'roster_presence_two_paths')
        for case in cases or ():
            blocker = case['blocker']
            if blocker: break
            query = case['query']
            default_fixtures = [{'actor_count': count, 'roster_count': roster, 'cursor': cursor} for count, roster, cursor in
                ((7 if query and query['op'] == 'branch_any_hidden' else 1, 1 if query and query['result'] else 0, 0), (32, 2, 1), (96, 32, 0))]
            for fixture in case.get('fixtures', default_fixtures):
                count, roster, cursor = fixture['actor_count'], fixture.get('roster_count', 0), fixture.get('cursor', 0)
                ids = ([0] * (roster - 1) + [query['args'][0]]) if query and query['op'] == 'branch_roster_text' and query['result'] else []
                flags = {query['args'][0]: query['result']} if query and query['op'] == 'branch_any_hidden' else {}
                actual = execute_linear(image, entry, stage, case['allowed'], actor_count=count,
                                        roster_count=roster, cursor=cursor, actor_writes=case['actor_writes'],
                                        roster_text_ids=ids, actor_flags=fixture.get('actor_flags', flags),
                                        event_bytes=fixture.get('event_bytes'), turn=fixture.get('turn', 1),
                                        actor_hpmax=fixture.get('actor_hpmax'), inline_queries=case.get('inline_queries'),
                                        native_helpers=case.get('native_helpers', ()), view_writes=case.get('view_writes', False),
                                        actor_ids=fixture.get('actor_ids'))
                if actual['blocker']:
                    blocker = {'family': 'private_hot_write', **actual['blocker']}
                    break
                compare_original_trace(case['events'], actual['events'], fixture)
                probes.append({'source_id': row['source_id'], 'actor_count': count,
                               'roster_count': roster, 'cursor': cursor, 'roster_text_ids': ids,
                               'fixture': fixture, 'actor_flags': fixture.get('actor_flags', flags), **actual})
            if blocker: break
        if blocker:
            row['blocker'] = semantic_blocker(binding, row['caller_cfg'], blocker)
        else:
            flow, origins = lower_cases(cases, stage, kind)
            row.update(disposition='converted_candidate', flow_id=flow['id'], node_sources=origins,
                       original_caller_trace_verified=True, original_leaf_effects_executed=False)
            flows.append(flow)
        rows.append(row)
    flows, source_bindings = deduplicate_source_flows(flows)
    for row in rows:
        row['canonical_definition'] = source_bindings.get(row['flow_id'], row['flow_id'])
        row['source_only_alias'] = row['flow_id'] in source_bindings
    targets, exceptions, templates = defaultdict(list), defaultdict(list), defaultdict(list)
    for row in rows:
        targets[row['target_va']].append(row['source_id'])
        if row['blocker']: exceptions[row['blocker']['family']].append(row['source_id'])
    for flow in flows:
        signature = {'entry': flow['entry'], 'nodes': [(node['op'], node['args'], node.get('views'), node['on']) for node in flow['nodes']]}
        templates[hashlib.sha256(json.dumps(signature, sort_keys=True).encode()).hexdigest()].append(flow['id'])
    predicate_vectors = []
    predicate_path = {va for va in instructions if 0x33499 <= va < 0x334D9}
    for ids, argument, expected in (([], 12, 0), ([12], 12, 1), ([0] * 31 + [12], 12, 1),
                                    ([12] * 32, 13, 0), ([255], 255, 1), ([12], 0x10C, 0), ([255], 0xFFFFFFFF, 0)):
        for hidden in (0, 1):
            actual = execute_linear(image, 0x33499, 0, predicate_path, roster_count=len(ids),
                                    roster_text_ids=ids, native_args=(argument,), roster_flags=hidden)
            if actual['blocker'] or actual['eax'] != expected:
                raise ValueError('full original roster predicate disagrees with its confirmed BYTE/full-argument domain')
            predicate_vectors.append({'roster_text_ids': ids, 'argument': argument, 'flags': hidden,
                                      'hp_words': 0, 'result': actual['eax']})
    helpers = shared_flows()
    validate_shared_calls(flows + helpers)
    layout_vectors = layout_oracles(image, instructions, flows, probes)
    flash_vectors = flash_oracles(image, readset)
    return {'schema': 'fd2-s2-transition-conversion-v1', 'source_ledger_sha256': S1_LEDGER_SHA256,
            'rows': rows, 'flows': flows + helpers, 'source_bindings': source_bindings,
            'shared_flow_ids': [flow['id'] for flow in helpers], 'target_index': dict(targets), 'exception_index': dict(exceptions),
            'template_index': dict(templates), 'summary': {'source_slots': len(rows), 'different_targets': len(targets),
                'converted_different_targets': len({row['target_va'] for row in rows if row['disposition'] == 'converted_candidate'}),
                'historical_definition_links': sum(bool(row['existing_definition']) for row in rows),
                'disposition_counts': dict(Counter(row['disposition'] for row in rows)), 'candidate_flows': len(flows),
                'shared_helper_flows': len(helpers),
                'converted_source_bindings': sum(row['disposition'] == 'converted_candidate' for row in rows),
                'source_only_aliases': len(source_bindings),
                'different_parameterized_templates': len(templates), 'new_runtime_bindings': 0,
                'original_caller_vectors': len(probes), 'original_full_predicate_vectors': len(predicate_vectors),
                'original_full_layout_vectors': len(layout_vectors), 'original_full_flash_vectors': len(flash_vectors)},
            'oracle': {'method': 'static push/write lowering versus independent original x86 caller execution',
                       'template_count_scope': 'complete literal graph/edge identities, not independent gameplay mechanism count or percentage coverage',
                       'vectors': probes, 'full_roster_predicate_vectors': predicate_vectors,
                       'full_layout_effect_vectors': layout_vectors,
                       'full_camera_group_flash_vectors': flash_vectors,
                       'layout_leaf_substitutions': ['1f882/1f525 (their expanded fade sequence verified separately)', '11cac refresh', '3790a real-time wait'],
                       'native_predicate': '0x33499 complete original instructions, not a substitution',
                       'leaf_substitutions': {hex(va): name for va, (name, _) in LEAVES.items() if va != 0x33499},
                       'not_checked': ['all caller leaf business/presentation execution (explicit substitutions remain)', 'whole-caller SDL scene equivalence (C layout probe separately recorded)', 'natural stage reachability', 'original GUI']}}


def validate_output(path):
    output = Path(path).resolve()
    if (not output.is_relative_to((ROOT / 'build/s2').resolve())
            or output.is_relative_to((ROOT / 'original_game').resolve()) or output.exists()):
        raise ValueError('transition output must be a new local build/s2 directory outside original resources')
    return output


def compare_layout_c(report, executable):
    vectors = report['oracle']['full_layout_effect_vectors']
    request = ''.join(f"{v['flow_id']} {v['start_node']} {v['actor_count']} {v['input_actors_hex']}\n" for v in vectors)
    result = subprocess.run([str(Path(executable).resolve()), '--layout-vectors'], input=request,
                            text=True, capture_output=True, cwd=ROOT, timeout=60, check=False)
    lines = result.stdout.splitlines()
    if result.returncode or len(lines) != len(vectors):
        raise ValueError(f'actual C layout probe failed: rc={result.returncode}, lines={len(lines)}/{len(vectors)}, stderr={result.stderr[:1000]}')
    for vector, line in zip(vectors, lines):
        values = line.split()
        if len(values) != 8 or values[0] != vector['actors_hex'] or [int(value) for value in values[1:]] != vector['view_fields']:
            raise ValueError(f"actual C layout consumer differs from full original233c6: {vector['flow_id']} raw_count{vector['actor_count']}")
    return {'verified': True, 'vectors': len(vectors), 'executable_sha256': hashlib.sha256(Path(executable).read_bytes()).hexdigest(),
            'scope': 'actual Game stage source node consumer versus full original233c6/13536 raw80B and six view fields; rendering/time excluded'}


def verify_definitions(report, path):
    raw = Path(path).read_bytes()
    document = json.loads(raw, object_pairs_hook=unique_keys)
    actual = {flow['id']: flow for flow in document['flows'] if flow['id'].startswith('stage.source.')}
    expected = {flow['id']: flow for flow in report['flows']}
    aliases = {key: value for key, value in document.get('bindings', {}).items() if key.startswith('stage.source.')}
    if actual != expected or aliases != report['source_bindings']:
        raise ValueError('current static source definitions/aliases differ from converted exact source domain')
    return {'verified': True, 'sha256': hashlib.sha256(raw).hexdigest(), 'definitions': len(expected), 'source_only_aliases': len(aliases)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readset', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--c-probe', type=Path)
    parser.add_argument('--definition', type=Path)
    args = parser.parse_args()
    try:
        output = validate_output(args.output_dir)
        readset_raw, image = args.readset.read_bytes(), args.image.read_bytes()
        report = convert(json.loads(readset_raw, object_pairs_hook=unique_keys), image)
        report['formal_definition_comparison'] = verify_definitions(report, args.definition) if args.definition else {'verified': False, 'reason': 'formal input not supplied'}
        report['oracle']['actual_c_layout_consumer'] = compare_layout_c(report, args.c_probe) if args.c_probe else {'verified': False, 'reason': 'C probe not supplied'}
        report['readset_sha256'] = hashlib.sha256(readset_raw).hexdigest()
        report['converter_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        output.mkdir(parents=True, exist_ok=False)
        (output / 'conversion.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(report['summary'], ensure_ascii=False))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'入场／胜利转换失败：{error}\n')


if __name__ == '__main__':
    main()


def collect_domain(db, ledger_path, image_path, output):
    """由已配置Domain执行器调用；只读隔离库，不保存推断类型。"""
    sys.path.insert(0, str(ROOT / "tools/ida"))
    from g1_preparation_evidence import function_record, RELOCATED_SHA256

    ledger_raw = Path(ledger_path).read_bytes()
    image = Path(image_path).read_bytes()
    if hashlib.sha256(ledger_raw).hexdigest() != S1_LEDGER_SHA256:
        raise ValueError("S1 frozen ledger identity mismatch")
    if hashlib.sha256(image).hexdigest() != RELOCATED_SHA256:
        raise ValueError("strict relocated source image identity mismatch")
    ledger = json.loads(ledger_raw)
    bindings = [row for row in ledger["records"] if row["kind"] == "dispatch" and row["decoded"]["table"] in TABLES]
    owners = {int(owner, 16) for row in bindings for owner in row["decoded"]["ida_owners"]}
    owners.update(SUPPORTING_OWNERS)
    functions = []
    total = 0
    for owner in sorted(owners):
        row = function_record(db, owner)
        for chunk in row["chunks"]:
            raw = bytes.fromhex(chunk["raw_hex"])
            if raw != image[chunk["start"]:chunk["end"]]:
                raise ValueError(f"Domain/strict source mismatch: {chunk['start']:#x}")
            total += len(raw)
        # 旧helper的get_instructions只枚举主块；共享尾须按完整chunk重取。
        instructions = {instruction.ea: instruction for chunk in row["chunks"]
                        for instruction in db.instructions.get_between(chunk["start"], chunk["end"])}
        row["instructions"] = [{"ea": ea, "size": instruction.size,
                                "raw_hex": db.bytes.get_bytes_at(ea, size=instruction.size).hex(),
                                "disasm": db.instructions.get_disassembly(instruction)}
                               for ea, instruction in sorted(instructions.items())]
        row["xrefs"] = [{"from": instruction["ea"], "to": ref.to_ea, "type": ref.type.name}
                        for instruction in row["instructions"] for ref in db.xrefs.from_ea(instruction["ea"])]
        functions.append(row)
    report = {"schema": "fd2-s2-transition-domain-v1", "database_saved": False,
              "address_view": "canonical relbase VA; loader-relocated bytes",
              "ledger_sha256": S1_LEDGER_SHA256, "image_sha256": RELOCATED_SHA256,
              "bindings": [{"source_id": row["source_id"], "source": row["source"],
                            "table": row["decoded"]["table"], "slot": row["decoded"]["slot"],
                            "target_va": row["decoded"]["target_va"], "owners": row["decoded"]["ida_owners"],
                            "existing_definition": row["definition"]} for row in bindings],
              "functions": functions, "function_bytes": total, "strict_byte_match": True,
              "original_machine_code_executed": False, "new_runtime_bindings": 0}
    output = Path(output)
    if output.exists():
        raise ValueError("transition readset already exists")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {"output": str(output), "functions": len(functions), "bytes": total,
            "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
