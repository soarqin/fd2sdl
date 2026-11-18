#!/usr/bin/env python3
"""Source-derived finite DIG/SB/channel event tape; no C output is read.

Time/CPU delivery is explicit test input, not a new clock or a measured phase.
The independently qualified channel Fraction model is reused as a leaf.
DIG spans use closed-form 2R-1 source intervals, not the C fraction loop.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import struct

def module(path):
    spec = importlib.util.spec_from_file_location('fd2_channel_oracle', path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value

def wordbytes(values):
    return b''.join(struct.pack('<i', value) for value in values)

PAYLOADS = [bytes((i * 37 + 7) & 255 for i in range(2500)),
            bytes((255 - i * 19) & 255 for i in range(500)),
            bytes((i * 71 + 91) & 255 for i in range(120)),
            bytes((i * 11 + 3) & 255 for i in range(10000))]

class BridgeModel:
    def __init__(self, leaf):
        self.leaf = leaf
        self.ch = leaf.Model(8192, 4096)
        self.ch.window = [[0, 0] for _ in range(2048)]
        self.ch.out = [[0, 0] for _ in range(48)]
        self.ch.enable(1)
        self.samples = [dict(data=0, length=0, pos=0, loops=1, status=2) for _ in range(2)]
        self.ring = bytearray([128] * 4096)
        self.build = [0] * 2048
        self.playing = self.empty = self.entered = 0
        self.last_half = -1
        self.cursor = self.left = self.total = self.mode = 0
        self.pending = self.half = self.saved = self.driver_state = 0
        self.reads = self.ends = self.fills = 0
        self.fill_whole = 0

    def fill(self):
        self.fills += 1
        self.mix(self.fill_whole)

    def provider_start(self):
        if self.driver_state in (2, 3):
            self.driver_state = 3
            return
        if self.saved:
            self.half, self.saved = self.saved - 1, 0
            self.fill()
            if self.mode == 3:
                self.mode = 2
                self.fill()
                self.mode = 1
            self.driver_state = 3
            return
        self.cursor = 0
        self.mode = 2
        self.fill()
        self.left = self.total = 2048
        self.pending = 0
        self.fill()
        self.mode = 1
        self.half, self.driver_state = 0, 3

    def start(self, handle, payload, loops):
        self.samples[handle] = dict(data=payload, length=len(PAYLOADS[payload]),
                                   pos=0, loops=loops, status=4)
        if not self.playing:
            self.provider_start()
            self.playing = 1

    def stop(self, handle):
        if self.samples[handle]['status'] == 4:
            self.samples[handle]['status'] = 8

    def service(self):
        if not self.playing or self.half == -1 or self.half == self.last_half:
            return
        self.last_half = self.half
        self.build = [0] * 2048
        self.entered = 0
        for sample in self.samples:
            if sample['status'] != 4:
                continue
            self.entered += 1
            offset = 0
            while True:
                remaining = sample['length'] - sample['pos']
                span = min(2048 - offset, max(0, 2 * remaining - 1))
                # Each fresh/loop span emits first point once, then each twice.
                for j in range(span):
                    at = sample['pos'] + (j + 1) // 2
                    self.build[offset + j] += (PAYLOADS[sample['data']][at] - 128) * 202
                sample['pos'] += (span + 1) // 2
                offset += span
                if sample['pos'] < sample['length']:
                    break
                if sample['loops'] == 1:
                    sample['status'] = 2
                    break
                if sample['loops']:
                    sample['loops'] -= 1
                sample['pos'] = 0
                if offset == 2048:
                    break
        base = (self.half ^ 1) * 2048
        self.ring[base:base + 2048] = bytes((max(-32768, min(32767, v)) + 32768) // 256
                                          for v in self.build)
        if self.entered:
            self.empty = 0
        else:
            previous = self.empty
            self.empty += 1
            if previous == 2:
                if self.driver_state == 3:
                    self.driver_state = 2
                self.playing = 0

    def generate(self, request):
        if self.mode != 1:
            return
        count = min(request, self.left, 1024)
        data = bytes(self.ring[(self.cursor + i) % 4096] for i in range(count))
        self.cursor = (self.cursor + count) % 4096
        self.reads += count
        self.ch.add(data)
        self.left -= count
        if not self.left:
            was_pending = self.pending
            self.pending = 1
            self.ends += 1
            self.left = self.total
            if was_pending:
                self.mode = 4

    def mix(self, whole):
        for _ in range(2):
            requested = self.ch.need(whole)
            if not requested:
                break
            self.generate(requested)
        self.ch.render(whole)

    def irq(self):
        assert self.pending
        self.pending = 0
        if self.mode == 4:
            self.fill()
            self.mode = 1
        if self.driver_state == 2:
            self.fill()
            self.mode = 3
            self.saved = (self.half ^ 1) + 1
            self.half, self.driver_state = -1, 0
        elif self.driver_state == 3:
            self.half ^= 1

    def end(self):
        self.ch.end()
        self.ch.out = [[0, 0] for _ in range(48)]

    def state(self):
        samples = [value for sample in self.samples
                   for value in (sample['length'], sample['pos'], sample['loops'], sample['status'])]
        return self.ch.state() + [self.playing, self.empty, self.entered, self.last_half] + samples + [
            self.cursor, self.left, self.total, self.mode, self.pending, 1,
            self.half, self.saved, self.driver_state, self.reads, self.ends, self.fills]

def build(leaf):
    events = []
    model = None
    cases = []

    def event(op, a=0, b=0, c=0, tick=0, fill_whole=0, label=''):
        nonlocal model
        if op == 'INIT':
            model = BridgeModel(leaf)
            cases.append(label)
        model.fill_whole = fill_whole
        if op == 'START': model.start(a, b, c)
        elif op == 'STOP': model.stop(a)
        elif op == 'SERVICE': model.service()
        elif op == 'MIX': model.mix(a)
        elif op == 'IRQ': model.irq()
        elif op == 'END': model.end()
        elif op == 'GAIN':
            model.fill()
            model.ch.gain = [a, b]
        elif op != 'INIT': raise AssertionError(op)
        state = model.state()
        assert len(state) == 38
        events.append(dict(label=label or f'{tick}:{op}', op=op, a=a, b=b, c=c,
            tick=tick, fill_whole=fill_whole, state=state,
            dma_fnv=fnv(model.ring), build_fnv=fnv(wordbytes(model.build)),
            window_fnv=leaf.fnv_words(model.ch.window),
            out=[v for row in model.ch.out for v in row]))

    event('INIT', label='two-P-replace-stop-pause-resume')
    event('START', 0, 0, 1)
    event('START', 1, 1, 2)
    event('SERVICE')
    for ms in range(1, 576):
        if ms == 20:
            event('STOP', 0, tick=ms)
            event('START', 0, 2, 2, tick=ms)
        if ms == 40: event('STOP', 1, tick=ms)
        if ms == 470: event('START', 0, 2, 1, tick=ms)
        if ms % 5 == 0: event('SERVICE', tick=ms)
        event('MIX', 48, tick=ms)
        # Explicit test CPU policy: deliver pending IRQ after this Mix. It is
        # an input tape, not a claim about exact DOSBox or protected CPU latency.
        if model.pending: event('IRQ', tick=ms, fill_whole=48)
        event('END', tick=ms)
    assert model.fills > 4 and model.ends >= 3

    event('INIT', label='delayed-IRQ-ACK-recovery')
    event('START', 0, 3, 0)
    event('SERVICE')
    ack_seen = False
    for ms in range(1, 206):
        if ms % 5 == 0: event('SERVICE', tick=ms)
        event('MIX', 48, tick=ms)
        ack_seen |= model.mode == 4
        if ms >= 200 and model.pending: event('IRQ', tick=ms, fill_whole=48)
        event('END', tick=ms)
    assert ack_seen

    event('INIT', label='source-FillUp-partial-and-gain-epoch')
    event('START', 0, 3, 0)
    event('SERVICE')
    for ms in range(1, 106):
        if ms % 5 == 0: event('SERVICE', tick=ms)
        if ms in (1, 94):
            event('MIX', 12, tick=ms)
            event('GAIN', 4096 if ms == 1 else 8192, 16384,
                  tick=ms, fill_whole=23)
        event('MIX', 48, tick=ms)
        if model.pending: event('IRQ', tick=ms, fill_whole=48)
        event('END', tick=ms)
    return dict(cases=cases, events=events, inputs=[list(p) for p in PAYLOADS])

def header(result):
    ops = ['INIT', 'START', 'STOP', 'SERVICE', 'MIX', 'IRQ', 'END', 'GAIN']
    lines = ['/* Independent finite source event tape; no C results. */',
        'enum { '+','.join('B_'+op for op in ops)+' };',
        'typedef struct { const char *label; int op,a,b,c; uint64_t tick; size_t fill_whole;',
        ' int64_t state[38]; uint64_t dma_fnv,build_fnv,window_fnv; int32_t out[96]; } bridge_vector;']
    for i, payload in enumerate(PAYLOADS):
        lines.append('static const uint8_t bridge_input_'+str(i)+'[] = {')
        for start in range(0, len(payload), 32):
            lines.append(','.join(map(str, payload[start:start+32]))+',')
        lines.append('};')
    lines += ['static const uint8_t *const bridge_inputs[] = {bridge_input_0,bridge_input_1,bridge_input_2,bridge_input_3};',
              'static const uint32_t bridge_lengths[] = {2500,500,120,10000};',
              'static const bridge_vector bridge_vectors[] = {']
    for e in result['events']:
        fields = [json.dumps(e['label']), 'B_'+e['op'], str(e['a']), str(e['b']), str(e['c']),
                  str(e['tick']), str(e['fill_whole'])]
        lines.append('{'+','.join(fields)+',{'+','.join(map(str,e['state']))+'},'+
            ','.join('UINT64_C('+str(e[k])+')' for k in ('dma_fnv','build_fnv','window_fnv'))+
            ',{'+','.join(map(str,e['out']))+'}},')
    return '\n'.join(lines+['};',''])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--channel-model', type=Path, default=Path(__file__).with_name('sb_channel_vectors.py'))
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    targets = [args.header] + ([args.json] if args.json else [])
    if len({str(p.resolve()).casefold() for p in targets}) != len(targets):
        raise ValueError('distinct outputs required')
    leaf = module(args.channel_model)
    result = build(leaf)
    payload = header(result).encode('utf-8')
    leaf.new_or_identical(args.header, payload)
    if args.json: leaf.new_or_identical(args.json, (json.dumps(result)+'\n').encode('utf-8'))
    print(json.dumps(dict(cases=len(result['cases']), checkpoints=len(result['events']),
        header_sha256=hashlib.sha256(payload).hexdigest(),
        explicit_input_policy=True, original_CPU=False, C_results_used=False)))

if __name__ == '__main__': main()
