"""只读执行原1db65，核局部地址列表与临时页指针的实际活跃期。

仅签合成参数下的原frame写/回读，不签正常战斗可达、像素或实际callee。
原Domain完整chunk必须与strict relocated镜像相同；callee显式替身。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
from verify_f1_boundaries import verify_source

ROOT = Path(__file__).resolve().parents[2]


def execute(image, function, dead_count):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX,
        UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_EBP,
        UC_X86_REG_EIP, UC_X86_REG_ESP)
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x800000)
    cpu.mem_write(0, image)
    actors, page, temporary, frames = 0x200000, 0x280000, 0x300000, 0x480000
    stack, stop = 0x700000, 0x7ffff0
    local_base = stack - 16 - 0x7c
    def rd(address): return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    def put(address, value): cpu.mem_write(address, struct.pack('<I', value))
    def args(count):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        return [rd(sp + 4 + 4 * index) for index in range(count)]
    def ret(value=0, extra=0):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_EIP, rd(sp))
        cpu.reg_write(UC_X86_REG_ESP, sp + 4 + extra)
    for address, value in ((0x53a45, actors), (0x53a49, page), (0x53a81, frames),
            (0x53beb, 16 + dead_count), (0x53aa9, 10), (0x53aad, 10)):
        put(address, value)
    for raw in range(16 + dead_count):
        record = bytearray(80)
        record[:2] = bytes([10 + raw % 13, 10 + raw // 13])
        struct.pack_into('<H', record, 64, 100 if raw < 16 else 0)
        cpu.mem_write(actors + raw * 80, bytes(record))
    writes, draws, boundary = [], [], None
    pcs = {row['va'] for row in function['instructions']}
    leaves = set(function['callees'])
    def original_call(pc):
        if pc not in pcs or image[pc] != 0xe8:
            raise ValueError('expected original direct-call instruction')
        target = pc + 5 + struct.unpack_from('<i', image, pc + 1)[0]
        if target not in leaves:
            raise ValueError('direct-call target differs from full Domain owner')
        return target
    allocation_call, restore_call = original_call(0x1dd57), original_call(0x1de94)
    def hook(_, pc, size, data):
        nonlocal boundary
        if pc == 0x1dc29:
            index = cpu.reg_read(UC_X86_REG_EBP)
            sp = cpu.reg_read(UC_X86_REG_ESP)
            if sp != local_base:
                raise ValueError('address-list write does not use expected actual local frame')
            offset = index * 4
            if offset >= 0x7c:
                boundary = {'pc': hex(pc), 'zero_based_index': index,
                    'offset': offset, 'local_bytes': 0x7c,
                    'first_slot_outside_locals': 'saved EBP',
                    'write_performed': False}
                cpu.emu_stop()
                return
            writes.append({'index': index, 'offset': offset,
                           'value': cpu.reg_read(UC_X86_REG_ECX)})
        if pc == 0x3702f:
            ret(cpu.reg_read(UC_X86_REG_EAX), 4)
        elif pc == allocation_call:
            if args(1) != [456 * 336]:
                raise ValueError('unexpected original temporary-page allocation')
            ret(temporary)
        elif pc == restore_call:
            destination, source, length = args(3)
            if (destination, source, length) != (page, temporary, 456 * 336):
                raise ValueError('unexpected original page restore')
            cpu.mem_write(destination, bytes(cpu.mem_read(source, length)))
            ret(destination)
        elif pc == 0x4ebab:
            draws.append(args(3))
            ret()
        elif pc in leaves:
            ret()
        elif pc not in pcs:
            raise ValueError(f'PC outside the full original owner: {pc:#x}')
    cpu.hook_add(UC_HOOK_CODE, hook)
    put(stack, stop)
    saved = {UC_X86_REG_EBP: 0x11223344, UC_X86_REG_EBX: 0x22334455,
             UC_X86_REG_ESI: 0x33445566, UC_X86_REG_EDI: 0x44556677}
    for register, value in saved.items(): cpu.reg_write(register, value)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.emu_start(0x1db65, stop, count=2000000)
    returned = cpu.reg_read(UC_X86_REG_EIP) == stop
    if dead_count <= 31:
        if not returned or boundary or len(writes) != dead_count or len(draws) != dead_count * 12:
            raise ValueError('original frame vector did not return with expected bounded draw calls')
        if any(cpu.reg_read(register) != value for register, value in saved.items()):
            raise ValueError('in-frame vector did not preserve original callee-saved registers')
        expected = [row['value'] for row in writes[:30]]
        if dead_count == 31: expected.append(temporary)
        for frame in range(12):
            if [row[0] for row in draws[frame*dead_count:(frame+1)*dead_count]] != expected:
                raise ValueError('actual address-list readback differs from collected/aliased values')
    elif returned or boundary is None or boundary['zero_based_index'] != 31:
        raise ValueError('expected first write beyond original local frame was not observed')
    return {'dead_on_screen': dead_count, 'active_count': 16 + dead_count,
        'returned': returned, 'list_writes': writes, 'draw_calls': len(draws),
        'frame0_destinations': [row[0] for row in draws[:dead_count]],
        'temporary_page': temporary, 'first_outside_local_write': boundary,
        'callee_saved_preserved': returned and all(cpu.reg_read(r) == v for r,v in saved.items())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ('image', 'source', 'output'):
        parser.add_argument('--' + flag, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT/'build').resolve()) or output.exists():
        raise ValueError('output must be new evidence below project build/')
    paths = [args.image, args.source, Path(__file__), Path(__file__).with_name('verify_f1_boundaries.py')]
    inputs = {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    image = args.image.read_bytes()
    source = json.loads(args.source.read_text(encoding='utf8'))
    verify_source(source, image)
    function = next(f for f in source['functions'] if f['va'] == 0x1db65)
    # Actual original prologue: four callee-saved pushes, then sub esp,7ch.
    if image[0x1db6f:0x1db76] != bytes.fromhex('5356575583ec7c'):
        raise ValueError('original frame prologue identity differs')
    result = {'schema': 'fd2-f1-defeat-local-frame-v1', 'inputs': inputs,
        'vectors': [execute(image, function, count) for count in (0, 1, 29, 30, 31, 32)],
        'substitutions': ['all renderer/upload/wait/sfx/free leaves; malloc returns declared isolated page',
            'memmove performs actual bytes; FX table is synthetic and blit records arguments only'],
        'claims': {'actual_original_owner_executed': True, 'normal_reachability_proved': False,
            'original_native_executed': False, 'pixels_or_ABI_after_first_outside_write_proved': False,
            'first_U_is_not_inferred_from_pseudocode_array_length': True, 'whole_F1_complete': False}}
    if any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != digest for p,digest in inputs.items()):
        raise ValueError('input changed while executing source vectors')
    with output.open('x', encoding='utf8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps({'output': str(output), 'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
                      'vectors': 6, 'whole_F1_complete': False}))


if __name__ == '__main__': main()
