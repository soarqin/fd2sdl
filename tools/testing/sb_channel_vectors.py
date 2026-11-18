#!/usr/bin/env python3
"""Independent selected-profile integer/Fraction oracle, not C-generated data.

Reference: DOSBox-X b6abbd5980a885f5f310a4088c59a8688d1b116c,
mixer.cpp SHA256 9ba65864257b93af65056aa862db2d4af67647636b238e95fd8d2e15681a6c7b.
Uses mathematical intervals rather than the C emitter loop. It neither imports
the converter nor executes original code, a device, a driver or a clock.
"""
from fractions import Fraction
from pathlib import Path
import argparse
import hashlib
import json
import struct

N, D, ALPHA, SIZE = 22050, 48000, 38711, 2048
FRAME = 48

def ceil_fraction(value):
    return -(-value.numerator // value.denominator)

def trunc(value):
    return int(value)

def fnv_words(values):
    result = 14695981039346656037
    for row in values:
        for value in row:
            for byte in struct.pack('<i', value):
                result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return result

class Model:
    def __init__(self, left, right):
        self.window = [[0x13570000 + i * 17, 0x13570001 + i * 17] for i in range(SIZE)]
        self.out = [[-123456789 + i * 31, -123456788 + i * 31] for i in range(FRAME)]
        self.written = self.read = self.rendered = 0
        self.last = self.current = self.delta = self.phase = self.slew = 0
        self.loaded = self.enabled = 0
        self.gain = [left, right]
        self.low = [0, 0]

    def load(self, byte):
        self.last, self.current = self.current, (byte - 128) * 256
        self.delta = self.current - self.last
        self.loaded = 1

    def put(self, sample):
        self.window[self.written] = [sample * gain for gain in self.gain]
        self.written += 1

    def interval(self, limit):
        # Number of still-valid interpolation positions is derived as a rational
        # interval length. No call or generated result from the C implementation.
        ramp = min(limit - self.written, max(0, ceil_fraction(Fraction(D - self.slew, N))))
        for j in range(ramp):
            self.put(self.last + trunc(Fraction(self.delta * (self.slew + j * N), D)))
        self.phase += ramp * N
        self.slew += ramp * N
        hold = min(limit - self.written, max(0, ceil_fraction(Fraction(D - self.phase, N))))
        for _ in range(hold):
            self.put(self.current)
        self.phase += hold * N

    def add(self, data):
        if self.written == SIZE:
            return 1, 0
        used = 0
        if not self.loaded:
            if not data:
                return 0, 0
            self.load(data[0]); used = 1
            if len(data) == 1:
                self.phase = self.slew = D
                return 0, 1
            self.load(data[1]); used = 2
            self.phase = self.slew = 0
        while self.written < SIZE:
            if self.phase >= D:
                if used == len(data):
                    break
                self.load(data[used]); used += 1
                self.phase -= D
                self.slew = self.phase
            self.interval(SIZE)
        return int(used < len(data)), used

    def need(self, whole):
        if not self.enabled or whole <= self.rendered or whole <= self.written:
            return 0
        return ceil_fraction(Fraction((whole - self.written) * N + self.phase, D)) + (not self.loaded)

    def enable(self, value):
        value = int(bool(value))
        if value != self.enabled:
            self.enabled = value
            if not value:
                self.phase = 0

    def render(self, whole):
        if whole <= self.rendered:
            return
        if not self.enabled:
            self.rendered = whole
            return
        if self.written < whole:
            if self.loaded:
                self.interval(whole)
            if self.written < whole:
                self.phase = min(self.phase, D)
                while self.written < whole:
                    self.window[self.written] = [self.current, self.current]  # raw pad, no Q13
                    self.written += 1
        amount = min(whole - self.rendered, whole - self.read)
        for i in range(amount):
            at = self.read + i
            for side in (0, 1):
                filtered = (self.window[at][side] * ALPHA + self.low[side] * (65536 - ALPHA)) // 65536
                self.window[at][side] = self.low[side] = filtered
                self.out[self.rendered + i][side] = filtered
        self.read += amount
        self.rendered = whole

    def end(self):
        assert self.rendered == FRAME
        if self.written <= FRAME:
            self.written = self.read = 0
        else:
            remaining = self.written - FRAME
            # Slice copies source prefix first, like the original overlapping move.
            self.window[:remaining] = [row[:] for row in self.window[FRAME:self.written]]
            self.written = remaining
            self.read = max(0, self.read - FRAME)
        self.rendered = 0

    def state(self):
        return [self.written, self.read, self.rendered, self.last, self.current, self.delta,
                self.phase, self.slew, *self.gain, *self.low, self.loaded, self.enabled]

def build():
    data = []
    events = []
    model = None
    case_names = []

    def action(label, op, a=0, b=0, payload=()):
        nonlocal model
        offset = len(data); data.extend(payload)
        result = used = needed = 0
        if op == 'INIT':
            model = Model(a, b); case_names.append(label)
        elif op == 'ENABLE': model.enable(a)
        elif op == 'GAIN': model.gain = [a, b]
        elif op == 'ADD': result, used = model.add(payload)
        elif op == 'NEED': needed = model.need(a)
        elif op == 'RENDER': model.render(a)
        elif op == 'END': model.end()
        else: raise AssertionError(op)
        events.append(dict(label=label, op=op, a=a, b=b, offset=offset, count=len(payload),
                           result=result, loaded_bytes=used, needed=int(needed),
                           state=model.state(), window_fnv=fnv_words(model.window),
                           out=[value for row in model.out for value in row]))

    def begin(label, left=8192, right=8192):
        action(label, 'INIT', left, right); action(label + ':enable', 'ENABLE', 1)

    begin('empty-pad'); action('empty-request24', 'NEED', 48); action('empty-zero', 'RENDER', 48); action('empty-end', 'END')
    for byte, label in [(255, 'single-plus'), (0, 'single-minus')]:
        begin(label); action(label + ':load', 'ADD', payload=[byte]); action(label + ':need', 'NEED', 48)
        action(label + ':raw-pad', 'RENDER', 48); action(label + ':end', 'END')
    for seq, label in [([255, 0], 'falling-extreme'), ([0, 255], 'rising-extreme'), ([128, 128], 'zero-pair')]:
        begin(label); action(label + ':load', 'ADD', payload=seq); action(label + ':render', 'RENDER', 48)

    begin('all-u8-cycle', 8192, 4096); action('all-u8-load256', 'ADD', payload=list(range(256)))
    for whole in (12, 23, 23, 12, 48): action('partial-' + str(whole), 'RENDER', whole)
    action('all-u8-end', 'END')
    begin('split-u8-cycle', 8192, 4096)
    cursor = 0
    for size in (1, 1, 3, 7, 13, 64, 167):
        action('split-' + str(size), 'ADD', payload=list(range(cursor, cursor + size))); cursor += size
    assert cursor == 256
    for whole in (12, 23, 23, 12, 48): action('split-partial-' + str(whole), 'RENDER', whole)
    action('split-u8-end', 'END')

    ramp = [(i * 37 + 19) & 255 for i in range(1200)]
    begin('explicit-24-23', 8192, 16384)
    action('first-need24', 'NEED', 48); action('given-24', 'ADD', payload=ramp[:24])
    action('first-prefetch-enough', 'NEED', 48); action('first48', 'RENDER', 48); action('first-end', 'END')
    action('next-need', 'NEED', 48); action('given-23', 'ADD', payload=ramp[24:47])
    action('next48', 'RENDER', 48); action('next-end', 'END')

    begin('given66-then-paused', 8192, 4096); action('given66', 'ADD', payload=ramp[:66])
    for i in range(4):
        action(f'pause-{i}:request', 'NEED', 48)
        action(f'pause-{i}:render', 'RENDER', 48); action(f'pause-{i}:end', 'END')
    action('resume-pair', 'ADD', payload=[0, 255]); action('resume-render', 'RENDER', 48); action('resume-end', 'END')

    begin('disable-keeps-prefetch'); action('disable-load66', 'ADD', payload=ramp[:66])
    action('before-disable23', 'RENDER', 23); action('disable', 'ENABLE', 0)
    action('disabled-demand', 'NEED', 48); action('disabled-out-unchanged', 'RENDER', 48); action('disabled-end', 'END')
    action('already-disabled', 'ENABLE', 0); action('enable-preserves-slew', 'ENABLE', 1)
    for i in range(2): action(f'enabled-{i}', 'RENDER', 48); action(f'enabled-{i}-end', 'END')
    action('resumed-input3', 'ADD', payload=[0, 128, 255]); action('resumed48', 'RENDER', 48)

    begin('gain-only-future-generated'); action('gain-load66', 'ADD', payload=ramp[:66])
    action('gain-first', 'RENDER', 48); action('gain-first-end', 'END')
    action('gain-change', 'GAIN', 0, 32768); action('gain-old-prefetch', 'RENDER', 48); action('gain-old-end', 'END')
    action('gain-new24', 'ADD', payload=[0, 255] * 12)
    for i in range(3): action(f'gain-{i}', 'RENDER', 48); action(f'gain-{i}-end', 'END')

    begin('max-and-zero-gain', 32768, 0); action('max-load', 'ADD', payload=[0, 255] * 4)
    action('max-render', 'RENDER', 48); action('max-end', 'END')

    begin('window-capacity'); action('large-input', 'ADD', payload=ramp)
    action('full-no-read', 'ADD', payload=[7, 9, 11]); action('full-need0', 'NEED', 48)
    action('full-first48', 'RENDER', 48); action('full-first-end', 'END')
    action('after-room-input8', 'ADD', payload=ramp[:8]); action('after-room48', 'RENDER', 48)

    action('disabled-add-is-not-enable', 'INIT', 8192, 8192)
    action('disabled-add2', 'ADD', payload=[1, 254]); action('disable-no-transition', 'ENABLE', 0)
    action('disabled-consume', 'RENDER', 48); action('disabled-consume-end', 'END')
    action('later-enable', 'ENABLE', 1); action('later-enabled-render', 'RENDER', 48)

    # Independent literals expose pad-without-gain and arithmetic negative floor.
    first=lambda label: next(e for e in events if e['label']==label)
    assert first('empty-request24')['needed'] == 24
    assert first('single-plus:load')['state'][:3] == [0,0,0]
    assert first('single-plus:raw-pad')['out'][0] == 19204
    assert first('single-minus:raw-pad')['out'][0] == -19356
    assert first('rising-extreme:render')['out'][0] == -158560256
    assert first('given66')['state'][0] == 142
    assert first('given-24')['state'][0] == 51
    # 51 outputs and 22 further source loads leave phase=51*22050-22*48000=68550.
    # EndFrame removes48 output points only. ceil((45*22050+68550)/48000)=23.
    assert first('given-24')['state'][6] == 68550
    assert first('next-need')['needed'] == 23
    return dict(cases=case_names,events=events,data=data)

def header(result):
    ops=['INIT','ENABLE','GAIN','ADD','NEED','RENDER','END']
    lines=['/* Synthetic source-derived vectors; generated by tools/testing/sb_channel_vectors.py. */',
           '#ifndef FD2_SB_CHANNEL_VECTORS_H','#define FD2_SB_CHANNEL_VECTORS_H',
           'enum { '+', '.join('V_'+op for op in ops)+' };',
           'typedef struct { const char *label; int op,a,b; size_t offset,count; int result; size_t loaded_bytes,needed; int64_t state[14]; uint64_t window_fnv; int32_t out[96]; } sb_vector;',
           'static const uint8_t sb_input[] = {']
    for start in range(0,len(result['data']),24):lines.append('    '+','.join(map(str,result['data'][start:start+24]))+',')
    lines += ['};','static const sb_vector sb_vectors[] = {']
    for row in result['events']:
        fields=[json.dumps(row['label']),'V_'+row['op'],str(row['a']),str(row['b']),str(row['offset']),str(row['count']),str(row['result']),str(row['loaded_bytes']),str(row['needed'])]
        lines.append('    {'+','.join(fields)+', {'+','.join(map(str,row['state']))+'}, UINT64_C('+str(row['window_fnv'])+'),')
        lines.append('     {'+','.join(map(str,row['out']))+'}},')
    lines += ['};','#endif','']
    return '\n'.join(lines)

def new_or_identical(path,data):
    path=path.resolve()
    if any(part.lower()=='original_game' for part in path.parts):
        raise ValueError('expected vectors must not be written into original_game')
    if path.exists():
        if path.read_bytes()!=data:raise FileExistsError('refuse replacing different expected data: '+str(path))
    else:
        path.parent.mkdir(parents=True,exist_ok=True)
        with path.open('xb') as stream:stream.write(data)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--header',type=Path,required=True);parser.add_argument('--json',type=Path)
    args=parser.parse_args()
    targets=[args.header]+([args.json] if args.json else [])
    if len({str(path.resolve()).lower() for path in targets})!=len(targets):
        raise ValueError('header and diagnostic JSON need distinct output paths')
    for path in targets:
        if any(part.lower()=='original_game' for part in path.resolve().parts):
            raise ValueError('expected vectors must not be written into original_game')
    result=build();payload=header(result).encode('utf-8');new_or_identical(args.header,payload)
    if args.json:new_or_identical(args.json,(json.dumps(result,indent=2)+'\n').encode('utf-8'))
    print(json.dumps(dict(cases=len(result['cases']),checkpoints=len(result['events']),synthetic_input_bytes=len(result['data']),header_sha256=hashlib.sha256(payload).hexdigest())))
if __name__=='__main__':main()
