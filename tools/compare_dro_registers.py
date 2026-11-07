"""只读比较 pinned DOSBox-X DRO 与 SDL 原始 OPL 写序；不播放或放宽时间误差。

投影依据 b6abbd5 adlib.cpp WriteCache/DoWrite/CacheWrite，非音频仿真器。
仅判定原文件编码寄存器前缀；不由该门宣称绝对时序、波形或整个 A7。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import sys


PHASES = ('install', 'render', 'retire')


def integer(value, low, high, field):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f'invalid {field}')
    return value


def validate_trace(rows, meta):
    if meta.get('schema') != 'fd2-title18-offline-opl-v1' or type(meta.get('track')) is not int or meta['track'] != 18:
        raise ValueError('require the declared Title18 offline probe protocol')
    rate = integer(meta.get('sample_rate'), 1, 192000, 'sample_rate')
    frames = integer(meta.get('frames'), 1, (1 << 64) - 1, 'frames')
    if meta.get('hook_enabled') is not True or meta.get('overflow') is not False:
        raise ValueError('require enabled non-overflowed raw trace')
    if not rows:
        raise ValueError('empty raw trace')
    if integer(meta.get('writes'), 1, (1 << 64) - 1, 'writes') != len(rows):
        raise ValueError('writes does not match complete raw trace')
    if integer(meta.get('generated_samples'), 1, (1 << 64) - 1, 'generated_samples') != frames:
        raise ValueError('generated_samples does not match frames')
    previous_sample = previous_tick = previous_phase = 0
    for index, row in enumerate(rows):
        if integer(row.get('ordinal'), 0, (1 << 64) - 1, 'ordinal') != index:
            raise ValueError('missing, repeated or unordered raw ordinal')
        sample = integer(row.get('sample'), 0, frames, 'sample')
        tick = integer(row.get('timer_tick'), 0, (1 << 32) - 1, 'timer_tick')
        integer(row.get('register'), 0, 0x1ff, 'register')
        integer(row.get('value'), 0, 255, 'value')
        if row.get('phase') not in PHASES:
            raise ValueError('invalid phase')
        phase = PHASES.index(row['phase'])
        if sample < previous_sample or tick < previous_tick or phase < previous_phase:
            raise ValueError('non-monotonic sample, timer_tick or phase')
        if phase == 0 and (sample != 0 or tick != 0):
            raise ValueError('installation write must have sample/timer_tick zero')
        if phase == 2 and sample != frames:
            raise ValueError('retire write must have final sample index')
        previous_sample, previous_tick, previous_phase = sample, tick, phase
    counts = Counter(row['phase'] for row in rows)
    declared = meta.get('phase_counts')
    if not isinstance(declared, dict) or set(declared) != set(PHASES):
        raise ValueError('invalid phase_counts')
    if any(type(declared.get(phase, 0)) is not int or declared.get(phase, 0) != counts[phase]
           for phase in PHASES):
        raise ValueError('phase_counts do not match complete raw trace')
    if not counts['install'] or not counts['render']:
        raise ValueError('require installation and rendered writes')
    if integer(meta.get('timer_ticks'), 0, (1 << 32) - 1, 'timer_ticks') != max(
            row['timer_tick'] for row in rows if row['phase'] in ('render', 'retire')):
        raise ValueError('timer_ticks does not match rendered/retired trace')
    return rate, frames


def project_capture(rows, mapped_registers):
    """首 trigger 前缓存、bank 交错快照、之后异值过滤；retire 不当原版尾段。"""
    cache = [0] * 512
    output = []
    trigger = None
    prefix_count = None
    ignored = Counter()

    def append(register, value, row, origin):
        if register & 255 in mapped_registers:
            output.append(dict(register=register, value=value, sample=row['sample'],
                timer_tick=row['timer_tick'], raw_ordinal=row['ordinal'], origin=origin))

    for row in rows:
        if row['phase'] == 'retire':
            ignored['retire_raw_writes'] += 1
            continue
        register, value = row['register'], row['value']
        # pinned Module::PortWrite MODE_OPL3 calls Chip::Write before CacheWrite.
        # Chip consumes exact bank0 02/03/04; 102/103/104 are not this timer path.
        if register in (0x02, 0x03, 0x04):
            ignored['module_timer_intercepted_raw_writes'] += 1
            continue
        mask = register & 255
        if trigger is None:
            starts = (0xb0 <= mask <= 0xb8 and value & 0x20) or (mask == 0xbd and value & 0x3f > 0x20)
            if starts:
                # CacheWrite calls DoWrite before changing the triggering register.
                trigger = dict(row)
                for low in range(256):
                    for bank in range(2):
                        current = cache[(bank << 8) | low]
                        if 0xb0 <= low <= 0xb8:
                            current &= ~0x20
                        if bank == 0 and low == 0xbd:
                            current &= ~0x1f
                        if current:
                            append((bank << 8) | low, current, row, 'cache_snapshot')
                prefix_count = len(output)
                append(register, value, row, 'first_trigger')
            else:
                ignored['pre_trigger_raw_writes'] += 1
        elif mask not in mapped_registers:
            ignored['unmapped_raw_writes'] += 1
        elif cache[register] == value:
            ignored['same_value_raw_writes'] += 1
        else:
            append(register, value, row, 'live_change')
        cache[register] = value
    return dict(writes=output, trigger=trigger, model_cache_prefix_count=prefix_count,
                ignored=dict(ignored))


def native_boundary(native):
    """从 writer 合同重建首 trigger，不把所有 t=0 写都误归 cache。"""
    writes = [row for row in native['commands'] if row['kind'] == 'write']
    index = next((i for i, row in enumerate(writes) if
        (0xb0 <= row['register'] & 255 <= 0xb8 and row['value'] & 0x20) or
        ((row['register'] & 255) == 0xbd and row['value'] & 0x3f > 0x20)), None)
    if index is None:
        raise ValueError('no encoded first capture trigger')
    prefix = writes[:index]
    if writes[index]['register'] == 0x1bd:
        raise ValueError('bank1 BD can retain low bits in cache; trigger boundary not uniquely established')
    if (any(row['time_ms'] != 0 or row['value'] == 0 for row in prefix) or
        len({row['register'] for row in prefix}) != len(prefix) or
        prefix != sorted(prefix, key=lambda row: (row['register'] & 255, row['register'] >> 8)) or
        any(row['value'] & 0x20 for row in prefix if 0xb0 <= row['register'] & 255 <= 0xb8) or
        any(row['value'] & 0x1f for row in prefix if row['register'] == 0xbd)):
        raise ValueError('native first-trigger prefix violates pinned cache writer order/value contract')
    if writes[index]['time_ms'] != 0:
        raise ValueError('native first capture trigger must have relative time zero')
    return dict(uniquely_reconstructed_from_writer=True, directly_encoded_split_marker=False,
        cache_prefix_writes=index, first_trigger=writes[index],
        cache_prefix_all_t0_nonzero_unique_and_bank_interleaved=True,
        bank1_BD_ambiguity_present=False,
        evidence='WriteCache clears both-bank KEYON and bank0 BD low5; the first non-ambiguous trigger follows the synthetic prefix.')


def quantization_diagnostic(original, generated, start, common, trigger_sample, rate):
    """整数分子求相位交集；不加误差、不挪动写序，不用结果放宽值比较。"""
    lower, upper = 0, rate
    lower_witness = upper_witness = first_empty = None
    for index in range(start, common):
        elapsed_samples = generated[index]['sample'] - trigger_sample
        current_lower = original[index]['time_ms'] * rate - elapsed_samples * 1000
        current_upper = current_lower + rate
        if current_lower > lower:
            lower, lower_witness = current_lower, index
        if current_upper < upper:
            upper, upper_witness = current_upper, index
        if first_empty is None and lower >= upper:
            first_empty = index
    count = max(0, common - start)
    return dict(status=('not_observed' if not count else 'compatible' if lower < upper else 'incompatible'),
        matched_live_writes=count, denominator=rate,
        phase_interval_lower_inclusive_numerator=lower,
        phase_interval_upper_exclusive_numerator=upper,
        signed_interval_width_numerator=upper - lower,
        nonempty_phase_interval=bool(count and lower < upper),
        lower_bound_write_index=lower_witness, upper_bound_write_index=upper_witness,
        first_empty_prefix_write_index=first_empty,
        meaning='Intersection of [native_ms - candidate_relative_ms, native_ms + 1 - candidate_relative_ms) with [0,1); integer rational arithmetic only.',
        hardware_or_input_timing_validated=False, per_port_IO_latency_modeled=False)


def compare(native, rows, meta):
    rate, frames = validate_trace(rows, meta)
    if native.get('ok') is not True or native.get('writer_consistent') is not True:
        raise ValueError('native DRO must pass the pinned writer inspection')
    boundary = native_boundary(native)
    mapped = set(bytes.fromhex(native['conversion_table_bytes']))
    projected = project_capture(rows, mapped)
    original = [row for row in native['commands'] if row['kind'] == 'write']
    generated = projected['writes']
    common = 0
    for left, right in zip(original, generated):
        if (left['register'], left['value']) != (right['register'], right['value']):
            break
        common += 1
    exact = bool(original) and common == len(original)
    mismatch = None
    if common < min(len(original), len(generated)):
        mismatch = dict(write_index=common, native=original[common], projected=generated[common],
            context_native=original[max(0, common - 3):common + 4],
            context_projected=generated[max(0, common - 3):common + 4])
    status = 'passed_native_register_prefix' if exact else ('register_mismatch' if mismatch else 'inconclusive_trace_too_short')
    clock = quantization_diagnostic(original, generated, boundary['cache_prefix_writes'], common,
        projected['trigger']['sample'] if projected['trigger'] else 0, rate)
    timing = []
    for index in sorted({0, min(common, len(original)) - 1}):
        if index < 0 or index >= common:
            continue
        left, right = original[index], generated[index]
        delta = right['sample'] - projected['trigger']['sample']
        timing.append(dict(write_index=index, native_relative_pic_ms=left['time_ms'],
            candidate_relative_sample_numerator=delta * 1000, candidate_rate_denominator=rate,
            candidate_timer_tick=right['timer_tick']))
    return dict(schema='fd2-dro-ordered-register-prefix-gate-v1', status=status, ok=exact,
        native_register_prefix_passed=exact, native_writes=len(original), candidate_projected_writes=len(generated),
        exact_common_prefix_writes=common, first_mismatch=mismatch,
        candidate_unvalidated_tail_writes=max(0, len(generated) - len(original)),
        candidate_rate=rate, candidate_frames=frames,
        native_capture_boundary=boundary,
        projection={key: value for key, value in projected.items() if key != 'writes'},
        first_last_exact_prefix_clock_observations=timing,
        relative_millisecond_quantization=clock,
        scope=dict(reorder=False, comparison_value_masks=False, source_defined_cache_keyoff_projection=True,
            source_defined_Module_Chip_timer_interception=True,
            dropped_different_value_mapped_writes=False,
            timing_tolerance_applied=False, timing_passed=False,
            native_t0_cache_live_boundary_reconstructed=True,
            native_t0_cache_live_split_marker_observed=False,
            native_post_last_write_duration_observed=False,
            retire_matches_native_stop=False, complete_port_IO_equal=False,
            PCM_or_emulator_core_equal=False, loaded_driver_pointer_observed=False,
            entire_A7_passed=False),
        limitations=['Native PIC_Ticks ms and offline sample clocks have no measured common origin; per-port execution latency is not modeled.',
            'No arbitrary alignment search, edit-distance matching or timing tolerance is used.',
            'Candidate extra writes after the finite native prefix are unvalidated; the native file encodes no silent tail.',
            'Core-dependent timbre/loudness differences are outside this ordered-prefix gate.'])


def fact(path):
    path = Path(path).resolve()
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('input changed while reading')
    return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(), size=len(data), mtime_ns=after.st_mtime_ns)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-dro', required=True, type=Path)
    parser.add_argument('--trace', required=True, type=Path)
    parser.add_argument('--meta', required=True, type=Path)
    parser.add_argument('--dro-parser', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    inputs = {key: getattr(args, key) for key in ('native_dro', 'trace', 'meta', 'dro_parser')}
    target = args.output.resolve() if args.output else None
    if target and (target.exists() or target in [p.resolve() for p in inputs.values()] or
                   any(part.casefold() == 'original_game' for part in target.parts) or not target.parent.is_dir()):
        raise ValueError('output must be new, outside original_game, and distinct from all inputs')
    before = {key: fact(path) for key, path in inputs.items()}
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('sealed_dro_parser', args.dro_parser)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    native = module.inspect(args.native_dro)
    rows = [json.loads(line) for line in args.trace.read_text(encoding='utf-8').splitlines()]
    meta = json.loads(args.meta.read_text(encoding='utf-8'))
    result = compare(native, rows, meta)
    after = {key: fact(path) for key, path in inputs.items()}
    if before != after:
        raise ValueError('an input changed during comparison')
    result.update(inputs=before, input_sha_size_mtime_unchanged=True)
    text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if target:
        with target.open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(text)
    print(text, end='')
    return 0 if result['ok'] else (2 if result['status'].startswith('inconclusive') else 1)


if __name__ == '__main__':
    raise SystemExit(main())
