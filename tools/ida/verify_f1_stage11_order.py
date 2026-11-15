"""原stage11入场/脚本和首次controller到达玩家action的调用序取证。

构造/绘制/等键均为明确替身；实际执行205da、333f5、1366a、134e4、
117e7。只证时序和原脚本XY，不声称首回合伤害可达或自然输入已经运行。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/static_data'))
from audit_stage_data import archive_entries, parse_metadata, parse_placements, SOURCE_HASHES
from verify_f1_boundaries import verify_source, machine


def footstep_table_boundary(profile, table_base):
    """322a0的实际[ESP+profile-1]；profile1..29是复制的29B，不按96 actor容量签。"""
    if type(profile) is not int or not 0 <= profile <= 255:
        raise ValueError('profile is not original BYTE')
    if 1 <= profile <= 29:
        return None
    return {'pc': '0x322a0', 'operation': 'footstep29[profile-1]', 'profile': profile,
            'copied_table_base': hex(table_base), 'copied_table_size': 29,
            'actual_read_address': hex(table_base + profile - 1),
            'allocation_record_read_is_not_itself_U': True}


def legal_setup_source(image, metadata):
    """固定独立原表坐标；训练/选阵是明确受控前提，不声称已自然运行。"""
    base_at, growth_at = 0x61da1 + 10 * 24, 0x620a1 + 10 * 11
    base = image[base_at:base_at + 24]
    growth = image[growth_at:growth_at + 11]
    selector = growth[10]
    learned_at = 0x626b3 + selector * 12
    learning = image[learned_at:learned_at + 12]
    pairs = list(zip(learning[::2], learning[1::2]))
    if selector != 1 or (30, 23) not in pairs:
        raise ValueError('normal unit10 level30 magic23 source differs')
    magic_at = 0x619fd + 23 * 7
    magic = image[magic_at:magic_at + 7]
    boss = parse_metadata(metadata)['templates'][1]
    enemy_at = 0x61af9 + (boss[1] - 68) * 10
    enemy = image[enemy_at:enemy_at + 10]
    return {'caster_text_id': 10, 'recruitment': 'original stage2 victory @0x230f2, push10 @0x2327d; normal forward roster proof bound04',
            'growth_va': hex(growth_at), 'learning_selector': selector, 'learning_va': hex(learned_at),
            'level_magic_pair': [30, 23], 'magic_record_va': hex(magic_at),
            'magic23_target_range': magic[3], 'magic23_MP_cost': magic[5],
            'caster_level30_base_constructor_MP_lower': struct.unpack_from('<H', base, 5)[0] + growth[8] * 29,
            'drop37_boss': {'template': 1, 'unit_id': boss[1], 'level': boss[4],
                            'enemy_table_va': hex(enemy_at),
                            'HP': struct.unpack_from('<H', enemy, 2)[0] * boss[4],
                            'base_DP': enemy[6] * boss[4]},
            'controlled_prerequisites': ['train normal recruit10 tolevel30 beforestage11, preserving learned23',
                                       'deploy caster within3 of an unacted normal strong player',
                                       'warp player to legal unoccupied tile near(10,6), then lethal physical action beforeEND'],
            'source_semantics_inherited': ['original1e529 growth/learn, player MAP23 target then unrestricted legal destination and caster-only acted commit'],
            'not_proven': ['from-START training input trace', 'chosen RNG/damage input trace', 'heap preload of a naturally trained early37 gameplay trace']}


def execute(image, metadata, placements, valid_pcs):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x800000)
    cpu.mem_write(0, image)
    actors, meta, state, stack, stop = 0x200000, 0x210000, 0x220000, 0x700000, 0x7ffff0
    def rd(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    def put(address, value):
        cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def args(n):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        return [rd(sp + 4 + 4 * i) for i in range(n)]
    def ret(value=0, extra=0):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_EIP, rd(sp))
        cpu.reg_write(UC_X86_REG_ESP, sp + 4 + extra)
    for address, value in ((0x53a45, actors), (0x53a55, meta), (0x53ad5, state), (0x53c03, 11)):
        put(address, value)
    cpu.mem_write(actors, b'\xa5' * (96 * 80))
    meta_parsed = parse_metadata(metadata)
    cpu.mem_write(meta, metadata)
    events, scripts, entry_xy = [], [], None
    phase = 'entry'
    def actor_write(raw, x, y, side, unit=0):
        record = bytearray(80)
        record[0:2] = bytes([x & 255, y & 255])
        record[6:9] = bytes([side, unit, unit])
        record[64:68] = struct.pack('<HH', 100, 100)
        cpu.mem_write(actors + raw * 80, bytes(record))
    def hook(_, pc, size, data):
        if pc in (0x1a30b, 0x1a813, 0x34fcc):
            events.append({'pc': hex(pc), 'phase': phase})
            raise ValueError('unexpected turn event before first player action')
        if pc == 0x3702f:
            ret(cpu.reg_read(UC_X86_REG_EAX), 4)
        elif pc == 0x1088d:
            if args(1) != [11]:
                raise ValueError('unexpected constructor stage')
            for raw in range(14):
                x, y, unit = placements[meta_parsed['unknown_02'] + raw]
                actor_write(raw, x, y, 2)
            put(0x53beb, 14)
            ret()
        elif pc == 0x10b4e:
            if args(1) != [1] or rd(0x53beb) != 14:
                raise ValueError('unexpected entry group/count')
            selected = [(i,t) for i,t in enumerate(meta_parsed['templates'][:meta_parsed['unknown_02']]) if t[21] == 1]
            for raw, (i, template) in enumerate(selected, 14):
                x, y, _ = placements[i]
                actor_write(raw, x, y, template[0], template[1])
            put(0x53beb, 14 + len(selected))
            ret()
        elif pc == 0x37910:
            dst, value, count = args(3)
            cpu.mem_write(dst, bytes([value & 255]) * count)
            ret(dst)
        elif pc == 0x1366a:
            scripts.append(args(1)[0])
        elif pc == 0x4eb48:
            identity = args(1)[0]
            if identity not in (40, 41):
                raise ValueError('unexpected movement script')
            ret(rd(0x627d8 + identity * 4))
        elif pc in (0x11cac, 0x11d40, 0x11eb0, 0x11eee, 0x127e0, 0x129ec,
                    0x135dd, 0x15f84, 0x17aa9, 0x1f525, 0x32230, 0x33faf, 0x3790a, 0x4e381):
            ret()
        elif pc == 0x11aa8:
            events.append({'pc': hex(pc), 'phase': phase, 'stub_key': 28})
            ret(28)
        elif pc == 0x12c0d:
            ret(0)
        elif pc == 0x25a96:
            ret()
        elif pc == 0x18890:
            events.append({'pc': hex(pc), 'phase': phase, 'actor': args(1)[0]})
            cpu.emu_stop()
        elif pc not in valid_pcs:
            raise ValueError(f'PC outside full Domain source09: {pc:#x}')
    cpu.hook_add(UC_HOOK_CODE, hook)
    put(stack, stop)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.emu_start(0x333f5, stop, count=2000000)
    if cpu.reg_read(UC_X86_REG_EIP) != stop or rd(0x53bef) != 1 or scripts != [40, 41]:
        raise ValueError('entry/script/turn boundary differs')
    entry_xy = [[cpu.mem_read(actors + raw * 80, 2)[0], cpu.mem_read(actors + raw * 80, 2)[1]] for raw in range(25)]
    phase = 'first_controller'
    put(stack, stop)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.emu_start(0x117e7, stop, count=100000)
    if cpu.reg_read(UC_X86_REG_EIP) != 0x18890 or events[-1].get('actor') != 0:
        raise ValueError('controller did not first reach active player action')
    return {'entry': '0x333f5', 'turn_after_entry': 1, 'active_count': rd(0x53beb),
            'scripts_actually_executed': scripts, 'entry_xy': entry_xy,
            'controller_prefix': events, 'phase0_callback35_before_first_player_action': False,
            'substitutions': ['1088d deploy constructor and10b4e group1 constructor with original placements/template identity',
                             'movement getter uses original strict relocated pointer table; all drawing/pan/DAC/waits/keyboard/audio leaves', 'first key28 and visible raw0 selection'],
            'not_proven': ['real input prefix', 'actual source constructor bytes/stats in this experiment',
                           'first-turn kill37 reachability', 'pixels/audio/timing']}


def early_drop(image, metadata, placements, valid_pcs, preload, allocation_preload=None):
    """受控早杀37；完整移动/脚步原码区分allocation读和真实29B表边界。"""
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_MEM_READ
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
    parsed = parse_metadata(metadata)
    original_constructor, construct_put, construct, (scratch, _, meta_at, place_at) = machine(image)
    records = {}
    for i, template in enumerate(parsed['templates'][:parsed['unknown_02']]):
        if template[21] not in (1, 3, 4):
            continue
        x, y, unit = placements[i]
        original_constructor.mem_write(meta_at, bytes(0x83) + template)
        original_constructor.mem_write(place_at, struct.pack('<4H', 1, x, y, unit))
        original_constructor.mem_write(0x53afa, b'\1')
        construct_put(0x53beb, 0)
        records[i] = construct(0x10c50, [0, 0], preload=preload)['record']
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x800000)
    cpu.mem_write(0, image)
    actors, stack, stop = 0x200000, 0x700000, 0x7ffff0
    def rd(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    def put(address, value):
        cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def args(n):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        return [rd(sp + 4 + 4 * i) for i in range(n)]
    def ret(value=0, extra=0):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_EIP, rd(sp))
        cpu.reg_write(UC_X86_REG_ESP, sp + 4 + extra)
    put(0x53a45, actors)
    put(0x53beb, 25)
    if allocation_preload is not None and len(allocation_preload) != 96 * 80:
        raise ValueError('captured allocation must contain all96 original80B records')
    cpu.mem_write(actors, allocation_preload if allocation_preload is not None else bytes([preload]) * (96 * 80))
    for raw in range(14):
        record = bytearray(80)
        record[6:9] = bytes([2, 0, 0])
        record[31:33] = bytes([1, 1])
        record[64:68] = struct.pack('<HH', 100, 100)
        cpu.mem_write(actors + raw * 80, bytes(record))
    for raw, i in enumerate((i for i, template in enumerate(parsed['templates']) if template[21] == 1), 14):
        cpu.mem_write(actors + raw * 80, records[i])
    cpu.mem_write(actors + 15 * 80 + 5, b'\1')
    cpu.mem_write(actors + 15 * 80 + 64, b'\0\0')
    accessed, events, table_error = [], [], None
    current_pc = 0
    def hook(_, pc, size, data):
        nonlocal current_pc, table_error
        current_pc = pc
        if pc == 0x322a0:
            profile = cpu.reg_read(UC_X86_REG_EAX)
            sp = cpu.reg_read(UC_X86_REG_ESP)
            if bytes(cpu.mem_read(sp,29)) != image[0x52725:0x52725 + 29]:
                raise ValueError('actual footstep local copy differs from original29B')
            table_error = footstep_table_boundary(profile, sp)
            if table_error:
                cpu.emu_stop()
                return
        if pc == 0x3702f:
            ret(cpu.reg_read(UC_X86_REG_EAX), 4)
        elif pc == 0x10b4e:
            group = args(1)[0]
            if group not in (3, 4):
                raise ValueError('unexpected early drop group')
            before = rd(0x53beb)
            selected = [i for i, template in enumerate(parsed['templates']) if template[21] == group]
            for raw, i in enumerate(selected, before):
                cpu.mem_write(actors + raw * 80, records[i])
            put(0x53beb, before + len(selected))
            events.append({'append_group': group, 'before': before, 'after': before + len(selected),
                           'record_bytes_from_actual_10c50': True})
            ret()
        elif pc == 0x4eb48:
            identity = args(1)[0]
            if identity not in (43, 44):
                raise ValueError('unexpected early drop movement')
            events.append({'movement_script': identity, 'count': rd(0x53beb)})
            ret(rd(0x627d8 + identity * 4))
        elif pc in (0x11cac, 0x11d40, 0x11eb0, 0x11eee, 0x127e0, 0x129ec,
                    0x135dd, 0x15f84, 0x17aa9, 0x25a96, 0x35f78, 0x3790a, 0x4e381):
            ret()
        elif pc not in valid_pcs:
            raise ValueError(f'early37 PC outside full Domain source09: {pc:#x}')
    def access(_, kind, address, size, value, data):
        if actors <= address < actors + 96 * 80:
            raw, offset = divmod(address - actors, 80)
            if raw >= rd(0x53beb):
                accessed.append({'pc': hex(current_pc), 'raw': raw, 'offset': offset, 'size': size,
                                 'kind': 'read' if kind == UC_MEM_READ else 'write'})
    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, access)
    put(stack, stop)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.emu_start(0x35022, stop, count=2000000)
    returned = cpu.reg_read(UC_X86_REG_EIP) == stop
    if not table_error and not returned:
        raise ValueError('early drop37 neither returned nor reached first declared table boundary')
    reads = [row for row in accessed if row['kind'] == 'read']
    return {'inactive_preload': preload if allocation_preload is None else 'captured controlled allocation',
            'allocation_preload_sha256': hashlib.sha256(allocation_preload).hexdigest() if allocation_preload is not None else None,
            'constructor_holes_preload': preload, 'events': events, 'first_inactive_record_reads': reads[:12],
            'all_inactive_read_offsets': sorted({row['offset'] for row in reads}),
            'inactive_read_raw_ids': sorted({row['raw'] for row in reads}),
            'first_declared_table_boundary': table_error, 'returned': returned,
            'active_count_at_stop': rd(0x53beb), 'inactive_accesses': len(accessed),
            'substitutions': ['10b4e append uses actual10c50 constructed80B from this run',
                             'graphics/pan/text/audio/delay/keyboard; movement getter uses original pointer table'],
            'not_claimed': ['uniform preload occurs in original DOS heap' if allocation_preload is None else
                            'composed active/tail state occurred in a natural original early37 trace',
                            'level30 caster10 training and spell23 input sequence already executed']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ('image', 'source', 'original', 'output'):
        parser.add_argument('--' + flag, type=Path, required=True)
    parser.add_argument('--pool-preload', type=Path)
    parser.add_argument('--pool-receipt', type=Path)
    args = parser.parse_args()
    if args.output.exists() or args.output.resolve().is_relative_to(args.original.resolve()):
        raise ValueError('unsafe/existing output')
    if bool(args.pool_preload) != bool(args.pool_receipt):
        raise ValueError('captured pool requires both complete bytes and original runtime receipt')
    guarded = [args.image, args.source, Path(__file__), args.original/'FD2.EXE', args.original/'FDFIELD.DAT',
               Path(__file__).with_name('verify_f1_boundaries.py')]
    if args.pool_preload: guarded += [args.pool_preload, args.pool_receipt]
    guards = {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest() for path in guarded}
    image = args.image.read_bytes()
    source = json.loads(args.source.read_text(encoding='utf8'))
    verify_source(source, image)
    field = (args.original / 'FDFIELD.DAT').read_bytes()
    if hashlib.sha256(field).hexdigest() != SOURCE_HASHES['FDFIELD.DAT']:
        raise ValueError('FDFIELD identity differs')
    entries = archive_entries(field)
    pcs = {i['va'] for f in source['functions'] for i in f['instructions']}
    result = {'schema': 'fd2-f1-stage11-first-action-order-v1',
              'image_sha256': hashlib.sha256(image).hexdigest(),
              'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
              'legal_setup_source': legal_setup_source(image, entries[34][1]),
              'original_CPU': execute(image, entries[34][1], parse_placements(entries[35][1]), pcs),
              'controlled_early_drop37': [early_drop(image, entries[34][1], parse_placements(entries[35][1]), pcs, value)
                                          for value in (0, 0xa5, 4)],
              'natural_input_executed': False, 'full_semantic_gate': False}
    if args.pool_preload:
        pool = args.pool_preload.read_bytes()
        receipt = json.loads(args.pool_receipt.read_text(encoding='utf8'))
        observed = receipt['heap_observation']
        if (receipt.get('schema') != 'fd2-f1-active-load-heap-precondition-v1' or receipt.get('ok') is not True
                or receipt.get('sources_unchanged') is not True or observed.get('active_count') != 25
                or observed.get('capacity_records') != 96 or len(pool) != 96 * 80
                or observed.get('preload_sha256') != hashlib.sha256(pool).hexdigest()
                or observed.get('inactive_tail_unchanged_by_Load') is not True):
            raise ValueError('captured stage11 allocation/receipt identity or scope differs')
        result['controlled_captured_inactive_pool'] = early_drop(
            image, entries[34][1], parse_placements(entries[35][1]), pcs, 0, pool)
        result['controlled_captured_inactive_pool']['scope'] = (
            'composed original CPU experiment: inactive bytes from one controlled native Load allocation; '
            'active roster synthetic and template constructors use declared0 preload for holes; '
            'not actual native early37, natural input, universal heap or full-renderer first-U proof')
    if any(hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest for path,digest in guards.items()):
        raise ValueError('stage11 audit input changed during execution')
    result['input_sha256'] = guards
    with args.output.open('x', encoding='utf8') as stream:
        stream.write(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()}))


if __name__ == '__main__':
    main()
