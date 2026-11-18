#!/usr/bin/env python3
"""Source-derived finite AIL fade projections; no C/guest/PIC execution.

449e0/43270/43230/42980: active ordinary no-EOT fade slices, fixed120Hz
logical services, unchanged raw cache/map, and no conflicting sequence/hook.
No signed overflow; service/busy/lock gates already permit this slice. Physical
CC7 is projected only after a completed service. These are not port timestamps.
The six inherited hash literals match the independent source relations; the
historical CPU harness/stub location has not been recovered.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

CASES = (
    (0, 127, 2000, 248, 127, 0x421af229187b2ed7, 0x493badc3112561e0),
    (127, 0, 4000, 488, 127, 0xd36f5e1dff985d53, 0xad8eaef7cea8d226),
    (10, 13, 1, 16, 100, 0xd012e1d49fc93155, 0x646e006e555f6e88),
    (100, 20, 0, 8, 91, 0x46257875b1145cad, 0x04934c33f9e74691),
    (42, 42, 2000, 8, 127, 0x8bbc44a4c1c8f32d, 0x4d25767f9dce13f5),
    (126, 127, 10, 16, 127, 0xf66ebc35e4194961, 0x023e8fd9b169c601),
)


def fold(hash_value, word):
    for byte in word.to_bytes(4, 'little', signed=False):
        hash_value = ((hash_value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return hash_value


def projection(case):
    initial, target, milliseconds, services, raw, old_state, old_messages = case
    current, accumulator, interval = initial, 0, 0
    physical, messages, states = raw * initial // 127, [], []
    # 449f7 target first; equality does not call43230. Positive operands make
    # Python floor equal source signed IDIV. This finite set cannot wrap.
    if initial != target:
        if milliseconds:
            interval = milliseconds * 1000 // abs(target - initial)
        else:
            current = target
        messages.append([0, 0xbc, 7, raw * current // 127])
    state_hash = 14695981039346656037
    for service in range(1, services + 1):
        if current != target:
            accumulator += 1000000 // 120
            steps = min(accumulator // interval, abs(target - current))
            accumulator -= steps * interval
            current += steps * (1 if target > current else -1)
            if service % 8 == 0:
                messages.append([service, 0xbc, 7, raw * current // 127])
        if messages:
            physical = messages[-1][3]
        state = [service, current, accumulator, physical]
        states.append(state)
        for value in state:
            state_hash = fold(state_hash, value)
    message_hash = fold(14695981039346656037, len(messages))
    for message in messages:
        for value in message:
            message_hash = fold(message_hash, value)
    if (state_hash, message_hash) != (old_state, old_messages):
        raise AssertionError('source projection differs from inherited finite literals')
    return dict(initial=initial, target=target, milliseconds=milliseconds,
                services=services, raw_CC7=raw, logical_channel=3, mapped_channel=12,
                state_hash=f'{state_hash:016x}', logical_message_hash=f'{message_hash:016x}',
                states=states, messages=messages)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='new JSON file; parent must exist')
    args = parser.parse_args()
    value = dict(schema='fd2-source-fade-projection-v1', cases=[projection(c) for c in CASES],
                 historical_stub_location_recovered=False, physical_PIC_qualified=False)
    text = json.dumps(value, indent=2) + '\n'
    if args.output:
        original = (Path(__file__).resolve().parents[2] / 'original_game').resolve()
        if args.output.resolve().is_relative_to(original):
            raise ValueError('output must be outside original_game')
        with args.output.open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
