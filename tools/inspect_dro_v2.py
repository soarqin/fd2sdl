"""只读解析 DOSBox-X b6abbd5 的 DRO v2；不播放、不调用模拟器、不推断加载的 MDI。

依据 src/hardware/adlib.cpp: RawHeader/MakeTables/AddWrite/WriteCache/DoWrite/CloseFile。
只支持该 writer 的固定转换表、无压缩 interleaved 2.0；不是任意 DRO 播放器。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct


HEADER = struct.Struct('<8sHHIIBBBBBB')
WRITER_COMMIT = 'b6abbd5980a885f5f310a4088c59a8688d1b116c'


def pinned_register_map() -> bytes:
    registers = [1, 4, 5, 8, 0xbd]
    for operator in range(24):
        if (operator & 7) < 6:
            registers.extend(base + operator for base in (0x20, 0x40, 0x60, 0x80, 0xe0))
    for channel in range(9):
        registers.extend(base + channel for base in (0xa0, 0xb0, 0xc0))
    return bytes(registers)


def parse_dro(data: bytes) -> dict:
    """保留每个 pair 的字节位置和顺序；结构错误拒绝，时间声明不一致单列为非通过。"""
    if len(data) < HEADER.size:
        raise ValueError('truncated 26-byte DRO header')
    magic, high, low, count, declared_ms, hardware, layout, compression, short, long, map_size = HEADER.unpack_from(data)
    if magic != b'DBRAWOPL' or (high, low) != (2, 0):
        raise ValueError('expected DBRAWOPL version 2.0')
    if hardware not in (0, 1, 2) or layout != 0 or compression != 0:
        raise ValueError('unsupported hardware label/layout/compression')
    payload_offset = HEADER.size + map_size
    if len(data) < payload_offset:
        raise ValueError('truncated register conversion table')
    table = data[HEADER.size:payload_offset]
    if table != pinned_register_map() or (short, long) != (len(table), len(table) + 1):
        raise ValueError('conversion/delay mapping differs from pinned writer')
    if not count:
        raise ValueError('zero command count: not a finalized pinned-writer capture')
    if len(data) != payload_offset + count * 2:
        raise ValueError('declared pair count does not match exact payload/file boundary')
    commands = []
    elapsed = delay_run = maximum_delay_run = 0
    register_counts: Counter[int] = Counter()
    for index in range(count):
        offset = payload_offset + index * 2
        opcode, value = data[offset:offset + 2]
        row = dict(pair_index=index, file_offset=offset, opcode=opcode, value=value, time_ms=elapsed)
        if opcode == short or opcode == long:
            delta = (value + 1) * (1 if opcode == short else 256)
            elapsed += delta
            delay_run += delta
            maximum_delay_run = max(maximum_delay_run, delay_run)
            row.update(kind='delay', delay_ms=delta, time_after_ms=elapsed)
        else:
            register_index = opcode & 0x7f
            if register_index >= len(table):
                raise ValueError(f'pair {index} at byte {offset}: register command outside conversion map')
            register = table[register_index] | ((opcode & 0x80) << 1)
            row.update(kind='write', register=register)
            register_counts[register] += 1
            delay_run = 0
        commands.append(row)
    issues = []
    if declared_ms != elapsed:
        issues.append('declared_duration_does_not_equal_encoded_delays')
    if maximum_delay_run > 30000:
        issues.append('encoded_delay_run_exceeds_writer_30s_segment_limit')
    if commands[-1]['kind'] != 'write':
        issues.append('trailing_delay_is_not_emitted_by_pinned_writer')
    if not register_counts:
        issues.append('no_register_record')
    return dict(schema='fd2-pinned-dro-v2-inspection-v2', ok=not issues,
        structurally_valid=True, writer_consistent=not issues, consistency_issues=issues,
        writer_commit=WRITER_COMMIT,
        header=dict(magic='DBRAWOPL', version_high=high, version_low=low, command_pairs=count,
            declared_milliseconds=declared_ms, hardware_label=hardware, format=layout, compression=compression,
            short_delay_opcode=short, long_delay_opcode=long, conversion_table_size=map_size),
        header_bytes=data[:HEADER.size].hex(), conversion_table_bytes=table.hex(),
        boundaries=dict(header_end=HEADER.size, table_end=payload_offset, payload_start=payload_offset,
            payload_bytes=count * 2, payload_end=len(data), file_bytes=len(data), trailing_bytes=0),
        encoded_delay_ms=elapsed, declared_minus_encoded_ms=declared_ms - elapsed,
        duration_equal=declared_ms == elapsed, maximum_encoded_delay_run_ms=maximum_delay_run,
        possible_unencoded_30s_close_gap=declared_ms - elapsed > 30000,
        decoded_writes=sum(register_counts.values()),
        register_counts={f'0x{register:03x}': n for register, n in sorted(register_counts.items())},
        bank1_records=sum(row['kind'] == 'write' and row['register'] >= 0x100 for row in commands),
        commands=commands,
        evidence_scope=dict(clock='relative emulated PIC_Ticks integer milliseconds; no QPC mapping',
            initial_nonzero_cache_prefix_synthesized_by_writer=True,
            t0_cache_and_live_boundary_encoded=False, same_value_writes_filtered_by_writer=True,
            register_records_are_complete_port_IO_history=False, header_label_proves_OPL_mode=False,
            loaded_driver_pointer_observed=False, PCM_or_A7_fidelity_proved=False,
            no_trailing_silence_duration_encoded=True),
        notes=['t=0记录可能属于WriteCache合成前缀，不能当原始首次端口I/O。',
            '只记录文件实际编码的105/104及bank写序，不从hardware标签补写或推断模式。',
            'writer在大于30秒写间隔时先累加header时间再关段，可能声明时间大于编码delay；不补造丢失间隔。'])


def inspect(path: Path) -> dict:
    path = Path(path).resolve()
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if before.st_size != len(data) or (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('DRO changed while reading; require a closed stable capture')
    result = parse_dro(data)
    result['source'] = dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(),
        size=len(data), mtime_ns=after.st_mtime_ns, unchanged_during_read=True)
    return result


def output_path(path: Path | None, source: Path) -> Path | None:
    if path is None:
        return None
    target = path.resolve()
    if any(part.casefold() == 'original_game' for part in target.parts):
        raise ValueError('output must not be inside resolved original_game')
    if target == source.resolve() or target.exists():
        raise ValueError('output must be a new file distinct from input')
    if not target.parent.is_dir():
        raise ValueError('output parent must already exist')
    return target


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dro', type=Path)
    parser.add_argument('--output', type=Path, help='new JSON only; no mkdir/overwrite/original_game writes')
    args = parser.parse_args(argv)
    target = None
    try:
        target = output_path(args.output, args.dro)
        result = inspect(args.dro)
    except (OSError, ValueError) as error:
        result = dict(ok=False, structurally_valid=False, error=str(error))
    text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if target is not None:
        try:
            with target.open('x', encoding='utf-8') as stream:
                stream.write(text)
        except OSError as error:
            print(json.dumps(dict(ok=False, error=f'new output failed: {error}'), ensure_ascii=False))
            return 1
    print(text, end='')
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
