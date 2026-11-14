"""S5五个完整victory caller的关键分支/写点，真x86对真实Scene C前缀。

原图与C冷定义不作expected。呈现、移动/构造/资源I/O在原端明确替身；
库存查询/删除/奖励、signed turn分支、35f10/1db65死亡及直接写点实际执行。
"""
from pathlib import Path
import argparse
import json
import struct
import subprocess
from verify_s5_transition import ROOT, IMAGE_SHA, output_guard, sha
from audit_stage_data import unique_keys

READSET_SHA = 'e2756ff8f9ac885f27c8539362d297ab2bce7d1e795dd71e9f942108953e71a8'
ACTORS, ROSTER, PAGE, TEMP, STACK, RET = 0x200000, 0x210000, 0x300000, 0x400000, 0x700000, 0x7ffff0
ENTRIES = {20: 0x240fa, 22: 0x24754, 26: 0x250cc, 28: 0x2548c, 29: 0x25757}
VECTORS = ([(20, v, 1) for v in (0, 5, 6, 7, 8, 9)] +
           [(22, v, t) for v, t in ((0, 14), (0, 15), (0, -1), (1, 15), (2, 15), (8, 14), (9, 15))] +
           [(26, v, 1) for v in (0, 1, 3)] + [(28, 0, 1), (28, 1, 1), (29, 0, 1)])


def execute(image, vector):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
    stage, variant, turn = vector
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(0, 0x800000)
    u.mem_write(0, image)

    def rd(a): return struct.unpack('<I', u.mem_read(a, 4))[0]
    def wr(a, value): u.mem_write(a, struct.pack('<I', value & 0xffffffff))
    def args(n): return [rd(u.reg_read(UC_X86_REG_ESP) + 4 + 4*i) for i in range(n)]
    def ret(value=0, extra=0):
        sp = u.reg_read(UC_X86_REG_ESP)
        u.reg_write(UC_X86_REG_EIP, rd(sp))
        u.reg_write(UC_X86_REG_ESP, sp + 4 + extra)
        u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)

    records = bytearray(64*80)
    for i in range(64):
        records[80*i:80*i+9] = bytes((3+i % 5, 3+i//5, 0, 0, 0, 0, 2, 0, i))
        records[80*i+64:80*i+68] = struct.pack('<HH', 100, 100)
        for slot in range(8): records[80*i+10+2*slot:80*i+12+2*slot] = b'\x80\xff'

    def item(actor, slot, value): records[80*actor+10+2*slot:80*actor+12+2*slot] = bytes((0, value))
    if stage == 20:
        if variant <= 7:
            for i in range(variant): item(i, 0, 209)
        elif variant == 8:
            for i in range(6): item(0, i, 209+i)
        elif variant == 9:
            for i in range(6): item(0, i, 209)
        else: raise ValueError('unknown controlled item fixture')
    elif stage in (22, 26) and variant & 1: item(0, 0, 100)
    roster = bytearray(32*80)
    for i in range(20): roster[80*i+8] = i
    if stage == 22 and not variant & 2: roster[80*18+8] = 70
    u.mem_write(ACTORS, bytes(records)); u.mem_write(ROSTER, bytes(roster))
    for address, value in ((0x53a45, ACTORS), (0x53beb, 64), (0x53bf7, ROSTER),
                           (0x53bfb, 20), (0x53bef, turn), (0x53c03, stage),
                           (0x53a49, PAGE), (0x53aa9, 0), (0x53aad, 0),
                           (0x53ab1, 4), (0x53ab5, 4), (0x53ab9, 4), (0x53abd, 4)):
        wr(address, value)
    u.mem_write(0x53ef1, b'\0')  # 设备静音；不宣称音频验收。
    wr(STACK, RET); u.reg_write(UC_X86_REG_ESP, STACK)
    stories, events, visited, substitutes, visual_timing = [], [], set(), set(), []
    stopped = visual_started = False

    def hook(uc, pc, size, data):
        nonlocal stopped, visual_started
        visited.add(pc)
        if pc == 0x3702f: substitutes.add(pc); ret(u.reg_read(UC_X86_REG_EAX), 4)
        elif pc == 0x15f84:
            substitutes.add(pc); fragment = args(9)[1]; stories.append(fragment)
            stop = ((stage == 20 and fragment in (6, 7)) or
                    (stage == 22 and (fragment == 14 if variant & 8 else fragment in (10, 12, 13))) or
                    (stage == 26 and not variant & 2 and fragment in (9, 13)) or
                    (stage == 28 and fragment == (15 if variant & 1 else 11)) or (stage == 29 and fragment == 0))
            if stop: stopped = True; u.emu_stop()
            else: ret()
        elif pc == 0x112a5:
            substitutes.add(pc); identity = args(1)[0]; count = rd(0x53bfb)
            if count >= 32: raise ValueError('controlled roster capacity exceeded')
            # 只构造本门会读取的text BYTE/count；58B constructor另有独立原码门。
            u.mem_write(ROSTER+80*count+8, bytes([identity])); wr(0x53bfb, count+1)
            events.append(['roster_append_boundary', identity]); ret()
        elif pc in (0x233c6, 0x1366a, 0x25052, 0x24618, 0x1f882, 0x11506, 0x25089, 0x1088d,
                    0x10b4e, 0x22253, 0x24b4d):
            substitutes.add(pc)
            n = {0x233c6: 11, 0x1366a: 1, 0x25052: 2, 0x24618: 4, 0x1088d: 1,
                 0x10b4e: 1, 0x22253: 5, 0x24b4d: 1}.get(pc, 0)
            events.append([hex(pc), *args(n)]); ret()
        elif pc == 0x32975:
            substitutes.add(pc); actor = args(1)[0]; u.mem_write(ACTORS+80*actor+5, b'\x01'); ret()
        elif pc in (0x135dd, 0x12cea):
            substitutes.add(pc); x, y = args(2)
            if pc == 0x135dd:
                dx, dy = x-rd(0x53aa9), y-rd(0x53aad)
                wr(0x53ab1, rd(0x53ab1)+dx); wr(0x53ab5, rd(0x53ab5)+dy)
                wr(0x53aa9, x); wr(0x53aad, y)
            else:
                wr(0x53ab1, x); wr(0x53ab5, y)
                wr(0x53ab9, x-rd(0x53aa9)); wr(0x53abd, y-rd(0x53aad))
            ret()
        elif pc == 0x3771c:
            substitutes.add(pc); dst, src, count = args(3)
            u.mem_write(dst, bytes(u.mem_read(src, count))); ret(dst)
        elif pc == 0x37910:
            substitutes.add(pc); dst, value, count = args(3)
            u.mem_write(dst, bytes([value & 255])*count); ret(dst)
        elif pc == 0x3706e: substitutes.add(pc); ret(TEMP)
        elif pc == 0x11df2:
            substitutes.add(pc)
            if stage == 28 and variant & 1:
                visual_started = True; visual_timing.append([1, *args(3)])
            ret()
        elif pc == 0x3790a:
            substitutes.add(pc)
            if visual_started: visual_timing.append([2, args(1)[0], 0, 0])
            ret()
        elif pc in (0x129ec, 0x11eb0, 0x17aa9, 0x11eee, 0x127e0, 0x127a9,
                    0x25a96, 0x4ebab, 0x3776e, 0x11cac):
            substitutes.add(pc); ret()
        elif pc == 0x1b5ea and stage == 26 and variant & 2:
            # 原caller的共享return tail边界；未执行菜单draw或伪造成功函数调用。
            substitutes.add(pc); stopped = True; events.append(['shared_return_tail', hex(pc)]); u.emu_stop()

    u.hook_add(UC_HOOK_CODE, hook)
    u.emu_start(ENTRIES[stage], RET, count=2000000)
    if not stopped: raise ValueError('original caller did not reach its declared prefix')
    raw20 = bytes(u.mem_read(ACTORS+20*80, 80))
    return {'stage': stage, 'variant': variant, 'turn': turn, 'stories': stories,
            'hot_stage': rd(0x53c03), 'roster_count': rd(0x53bfb),
            'raw20': [raw20[7], raw20[8], raw20[5], raw20[2]],
            'events': events, 'visual_timing': visual_timing, 'source_instruction_vas': [hex(pc) for pc in sorted(visited)],
            'substituted_entry_vas': [hex(pc) for pc in sorted(substitutes)]}


def validate_probe(rows, runs):
    if len(rows) != len(runs): raise ValueError('C caller vector count differs')
    for row, run in zip(rows, runs):
        required = ('stage', 'variant', 'turn', 'result', 'stories', 'hot_stage', 'count',
                    'roster_count', 'rng_calls', 'raw20', 'raw20_cache_before', 'NoTown',
                    'export_preparation', 'prepared_stage', 'text_entries', 'visual_timing')
        if set(row) != set(required): raise ValueError('C caller result schema differs')
        for name in required:
            value = row[name]
            if name == 'visual_timing':
                if type(value) is not list or any(type(e) is not list or len(e) != 4 or
                    any(type(n) is not int for n in e) for e in value):
                    raise ValueError('C visual timing is not exact four-integer records')
            elif isinstance(value, list):
                if any(type(n) is not int for n in value): raise ValueError('C list is not exact integers')
            elif type(value) is not int: raise ValueError('C scalar is not an exact integer')
        for name in ('stage', 'variant', 'turn', 'stories', 'hot_stage', 'roster_count'):
            if row[name] != run[name]: raise ValueError(f'C/original caller {name} differs: {row[name]} != {run[name]}')
        good26 = run['stage'] == 26 and run['variant'] == 3
        if row['result'] != (0 if good26 else -1) or row['rng_calls']:
            raise ValueError('C prefix result/RNG differs')
        if row['visual_timing'] != run['visual_timing']:
            raise ValueError('C/original full pulse/caller wait sequence differs')
        if run['stage'] == 28 and (row['raw20'] != run['raw20'] or row['raw20_cache_before'] != row['raw20'][3]):
            raise ValueError('death/ID writes unexpectedly changed cached sprite class')
        expected_text = [30, 31] if run['stage'] == 29 else [run['stage']+1]
        if row['text_entries'] != expected_text: raise ValueError('C text bank lifetime differs')
        if good26 and (row['hot_stage'] != 27 or row['NoTown'] != 1 or row['export_preparation'] not in (-1, 0)):
            raise ValueError('C good26 NoTown source handoff differs')
        if good26 and row['export_preparation'] == 0 and row['prepared_stage'] != 27:
            raise ValueError('C approved preparation has wrong stage')
    return {'vectors': len(rows), 'all_declared_branch_write_and_timing_observations_equal': True, 'rows': rows,
            'source_only_export_gate_is_reported_not_bypassed': True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image', type=Path, required=True)
    p.add_argument('--readset', type=Path, required=True)
    p.add_argument('--original', type=Path, default=ROOT/'original_game')
    p.add_argument('--probe', type=Path)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    inputs = [args.image, args.readset, Path(__file__), Path(__file__).with_name('verify_s5_transition.py')]
    if args.probe:
        if args.original.resolve() != (ROOT/'original_game').resolve():
            raise ValueError('actual C probe uses the project original_game resource owner')
        inputs.append(args.probe)
        inputs.extend(args.original/name for name in
            ('FDOTHER.DAT', 'FDFIELD.DAT', 'FDSHAP.DAT', 'FDICON.B24', 'DATO.DAT', 'FDTXT.DAT'))
    output_guard(args.output, args.original, inputs)
    identity = {str(path.resolve()): sha(path.read_bytes()) for path in inputs}
    image = args.image.read_bytes(); raw = args.readset.read_bytes()
    if sha(image) != IMAGE_SHA or sha(raw) != READSET_SHA: raise ValueError('original caller input identity differs')
    domain = json.loads(raw, object_pairs_hook=unique_keys); spans = []
    for function in domain['functions']:
        for chunk in function['chunks']:
            a, b = chunk['start'], chunk['end']; data = bytes.fromhex(chunk['raw_hex'])
            if data != image[a:b] or sha(data) != chunk['sha256']: raise ValueError('caller complete Domain bytes differ')
            spans.append((a, b))
    runs = [execute(image, vector) for vector in VECTORS]
    for run in runs:
        permitted = {int(pc, 16) for pc in run['substituted_entry_vas']}
        missing = [pc for pc in map(lambda s: int(s, 16), run['source_instruction_vas'])
                   if pc not in permitted and not any(a <= pc < b for a, b in spans)]
        if missing: raise ValueError('executed native PC missing complete Domain owner: '+str(list(map(hex, missing))))
    consumer = None
    if args.probe:
        process = subprocess.run([str(args.probe.resolve()), '--s5-stage-vectors'], cwd=ROOT,
            input=''.join(f'{s} {v} {t}\n' for s, v, t in VECTORS), capture_output=True, text=True, timeout=120)
        if process.returncode: raise ValueError('actual Scene C caller probe failed: '+process.stderr[-2000:])
        consumer = validate_probe([json.loads(line, object_pairs_hook=unique_keys) for line in process.stdout.splitlines()], runs)
    for path in inputs:
        if sha(path.read_bytes()) != identity[str(path.resolve())]: raise ValueError('caller source/probe changed')
    report = {'schema': 'fd2-s5-stage-caller-prefix-v1', 'original_CPU_executed': True,
              'actual_C_executed': consumer is not None, 'inputs_sha256': identity, 'runs': runs, 'C_consumer': consumer,
              'native_executed': ['five complete caller branch/inline instructions until declared prefix',
                  '2aedb/1b8a6/1b722 inventory count and raw-prefix lookup',
                  '1b8e7/1c220/1bb8c exact inventory removal/reward before story7',
                  '24b14/24bde full predicates', '35f10/1db65 complete hot death writes; graphics I/O substituted',
                  '361b0 complete three pulses plus original2548c three caller tail300ms waits; DAC/wait I/O recorded'],
              'limits': ['controlled64actor/20roster source-only Scene states, not natural deployment or shipping App',
                  'equality covers the explicitly reported branch/story/stage/roster/death/cache/text-owner/pulse-wait observations, not complete Game memory',
                  'original layout/movement/text/constructor/roster-sync/restoration/resource-open are explicit boundary substitutes',
                  'C side real Scene/Game/map/text/movement/transition consumers; host upload/wait/clock substituted',
                  '1088d(30) caller argument/hotstage and actual C resource/text31 checked; original constructor separately qualified',
                  'prefix rejection is deliberately injected at named story; does not sign full caller success or ending cinematic',
                  'preparation export remains governed by current published support/binding; no source-only bypass',
                  'no original GUI, audible output or physical timing claim']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps({'output': str(args.output), 'sha256': sha(args.output.read_bytes()), 'vectors': len(runs),
                      'actual_C_executed': consumer is not None}))


if __name__ == '__main__': main()
