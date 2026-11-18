#!/usr/bin/env python3
"""Finite source projection of the fixed 15 XMIDI beat states, not a player.

43160/443d0/43270: fixed normal MDI preference120, tempo-percent100,
first EOT only, zero application callbacks, no special controllers. MIDI
messages/duration/PCM/PIC costs are not modeled. Original EOT flag blocks
the same-callback rewind peek from consuming a second event (433c9/437e3).
The output contains fingerprints/parameters only, no original payload.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

RESOURCE_SHA = '4105ebde543fe1c497e852728f6bc333bda80edeb7fb3671e487504bee74e998'
MASK = (1 << 32) - 1


def signed(value):
    return value if value < 1 << 31 else value - (1 << 32)


def fold(hash_value, values):
    for byte in struct.pack('<' + 'I' * len(values), *values):
        hash_value = ((hash_value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return hash_value


def vlq(data, at):
    value = 0
    for _ in range(4):
        b = data[at]
        at += 1
        value = (value << 7) | (b & 127)
        if b < 128:
            return value, at
    raise ValueError('not a normal finite VLQ')


def evnt_chunk(entry):
    found = []
    def walk(begin, end, depth=0):
        assert depth <= 8
        at = begin
        while at < end:
            assert end - at >= 8
            kind, n = entry[at:at+4], int.from_bytes(entry[at+4:at+8], 'big')
            stop = at + 8 + n
            assert stop <= end
            if kind == b'EVNT':
                found.append(entry[at+8:stop])
            if kind in (b'FORM', b'CAT ', b'LIST'):
                assert n >= 4
                walk(at+12, stop, depth+1)
            at = stop + (n & 1)
        assert at == end
    walk(0, len(entry))
    assert len(found) == 1
    return found[0]


def events(stream):
    at, result = 0, {}
    while at < len(stream):
        begin = at
        st = stream[at]
        at += 1
        kind, payload = 'MIDI', None
        if st < 128:
            kind, payload = 'delay', st
        elif st == 255:
            meta = stream[at]
            n, at = vlq(stream, at+1)
            assert n <= len(stream)-at
            payload = stream[at:at+n]
            at += n
            kind = 'meta' + str(meta)
            if meta == 81:
                assert n >= 3
            if meta == 88:
                assert n >= 2
            if meta == 47:
                assert n == 0
        elif st >> 4 in (8, 9, 10, 11, 14):
            assert at + 2 <= len(stream)
            at += 2
            if st >> 4 == 9:
                _, at = vlq(stream, at)
        elif st >> 4 in (12, 13):
            at += 1
        else:
            raise ValueError('outside fixed15 source family: ' + hex(st))
        assert at <= len(stream)
        result[begin] = (at, kind, payload)
        if kind == 'meta47':
            return result
    raise ValueError('no first EOT')


def projection(stream, loops):
    parsed = events(stream)
    cursor = delay = beat = phase = increment = tick = 0
    measure, numerator, denominator, remaining, status = MASK, 4, 8000000, loops, 4
    result = 14695981039346656037
    signatures, tempos = set(), set()
    while status == 4:
        tick += 1
        assert tick < 100000
        delay = (delay - 1) & MASK
        eot = False
        if signed(delay) <= 0:
            while True:
                nxt, kind, payload = parsed[cursor]
                if kind == 'delay':
                    cursor, delay = nxt, payload
                    break
                if kind == 'meta81':
                    tempo = int.from_bytes(payload[:3], 'big')
                    denominator = (tempo << 4) & MASK
                    tempos.add(tempo)
                elif kind == 'meta88':
                    numerator, exponent = payload[:2]
                    signatures.add((numerator, exponent))
                    # Fixed normal resource exponent2; general SHL/SAR is a
                    # separate literal regression, not needed to project15.
                    assert exponent == 2
                    increment = 16000000 // 120
                    phase = beat = 0
                    measure = (measure + 1) & MASK
                    phase = (phase - increment) & MASK
                elif kind == 'meta47':
                    eot = True
                    remaining -= 1
                    if remaining == 0:
                        status, cursor = 2, nxt
                    else:
                        beat, measure, phase, cursor = 0, MASK, 0, 0
                    break
                cursor = nxt
        if not eot:
            phase = (phase + increment) & MASK
            if signed(phase) >= signed(denominator):
                phase = (phase - denominator) & MASK
                beat = (beat + 1) & MASK
                if signed(beat) >= signed(numerator):
                    beat, measure = 0, (measure + 1) & MASK
        result = fold(result, [tick, 100, 100, 0, 0, 0, beat, measure, numerator,
                               increment, phase, denominator, cursor, delay, remaining, status])
    assert len(tempos) == len(signatures) == 1
    return tick, result, next(iter(tempos)), next(iter(signatures))


def derive(raw):
    assert hashlib.sha256(raw).hexdigest() == RESOURCE_SHA
    assert raw[:6] == b'LLLLLL'
    first = int.from_bytes(raw[6:10], 'little')
    assert (first - 6) // 4 == 21 and (first - 6) % 4 == 0
    offsets = list(struct.unpack_from('<21I', raw, 6))
    assert offsets[0] == first and offsets[-1] == len(raw)
    rows, sentinel = [], []
    for track, (begin, end) in enumerate(zip(offsets, offsets[1:])):
        assert begin < end
        entry = raw[begin:end]
        if entry == b' \r\n':
            sentinel.append(track)
            continue
        stream = evnt_chunk(entry)
        for loops in (1, 2):
            ticks, state, tempo, signature = projection(stream, loops)
            rows.append(dict(track=track, loops=loops, ticks=ticks, hash=f'{state:016x}',
                             raw_tempo=tempo, numerator=signature[0], exponent=signature[1]))
    assert sentinel == [0, 2, 5, 7, 9] and len(rows) == 30
    return rows


def header(rows):
    text = ['/* Generated by tools/testing/sequence_beat_vectors.py from fixed original',
            ' * resource + 43160/443d0/43270 finite source projection; no C/CPU/PCM oracle. */',
            'static const BeatTrackVector beat_track_vectors[] = {']
    for r in rows:
        text.append('    {%d,%d,%d,UINT64_C(0x%s),%d,%d,%d},' %
                    (r['track'], r['loops'], r['ticks'], r['hash'],
                     r['raw_tempo'], r['numerator'], r['exponent']))
    return '\n'.join(text + ['};', ''])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--resource', type=Path, default=Path('original_game/FDMUS.DAT'))
    p.add_argument('--output', type=Path)
    p.add_argument('--json', action='store_true')
    a = p.parse_args()
    rows = derive(a.resource.read_bytes())
    text = json.dumps(dict(schema='fd2-source-beat-vectors-v1', resource_sha256=RESOURCE_SHA,
                           cases=rows, PIC_qualified=False), indent=2)+'\n' if a.json else header(rows)
    if a.output:
        protected = Path(__file__).resolve().parents[2] / 'original_game'
        if (a.output.resolve().is_relative_to(a.resource.resolve().parent) or
                a.output.resolve().is_relative_to(protected.resolve())):
            raise ValueError('output must be outside original resource directory')
        with a.output.open('x', encoding='utf-8', newline='\n') as out:
            out.write(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
