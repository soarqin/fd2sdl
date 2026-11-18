#!/usr/bin/env python3
"""Independent finite Main-profile tape, not measured PIC/CPU initial phase.

Reuses previously independent channel/DIG source models, never C outputs.
IRQ0 input is explicit5966/PIT seconds; source ms ticks start at explicit0.
"""
from pathlib import Path
from fractions import Fraction
import argparse,hashlib,importlib.util,json

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    obj=importlib.util.module_from_spec(spec);spec.loader.exec_module(obj);return obj

def build(leaf,bridge):
    model=bridge.BridgeModel(leaf);model.ch.gain=[8192,8192]
    model.start(0,0,0)
    irq=1;dc=[0,0];pcm=[];rows=[]
    period=Fraction(5966*1000000000,1193182)
    for ms in range(1,111):
        boundary=ms*1000000
        while irq*period<boundary:
            model.service();irq+=1
        assert irq*period!=boundary
        model.mix(48)
        for sb in model.ch.out:
            for side,fm in enumerate((1024*8192,-1536*8192)):
                value=fm+sb[side]+dc[side]
                scale=1 if -245760000<value<245760000 else 64
                dc[side]-=value*349*scale//16777216
                pcm.append(max(-32768,min(32767,value*8192//67108864)))
        model.end()
        if model.pending:
            model.fill_whole=0;model.irq()
        rows.append(dict(ms=ms,state=model.state()[:37],dc=dc[:],
            dma_fnv=bridge.fnv(model.ring),build_fnv=bridge.fnv(bridge.wordbytes(model.build)),
            window_fnv=leaf.fnv_words(model.ch.window)))
    # Closed lattice prefix: first physicalblockends in ms93, no ideal92.8798 IRQ.
    assert rows[91]['state'][35]==2031 and rows[92]['state'][35]==2053
    assert len(pcm)==110*48*2 and rows[-1]['state'][36]==1
    partial=[];partial_dc=[0,0]
    for raw in [8192]*24+[16384]*24:
        for side in range(2):
            value=raw*8192+partial_dc[side]
            partial_dc[side]-=value*349//16777216
            partial.append(value//8192)
    return dict(pcm=pcm,rows=rows,partial=partial,partial_dc=partial_dc)

def header(result):
    lines=['/* Independent given-phase/source/CPU-policy expected, not a game recording. */',
        'static const int16_t original_expected_pcm[] = {']
    for at in range(0,len(result['pcm']),24):lines.append(','.join(map(str,result['pcm'][at:at+24]))+',')
    lines+=['};','typedef struct { int64_t state[37]; int32_t dc[2]; uint64_t dma,build,window; } original_checkpoint;',
        'static const original_checkpoint original_checkpoints[] = {']
    for r in result['rows']:
        lines.append('{{'+','.join(map(str,r['state']))+'},{'+','.join(map(str,r['dc']))+'},'+
            ','.join('UINT64_C('+str(r[k])+')' for k in ('dma_fnv','build_fnv','window_fnv'))+'},')
    lines+=['};','static const int16_t original_partial_pcm[] = {'+
        ','.join(map(str,result['partial']))+'};',
        'static const int32_t original_partial_dc[] = {'+
        ','.join(map(str,result['partial_dc']))+'};']
    return '\n'.join(lines+[''])

def main():
    p=argparse.ArgumentParser();p.add_argument('--channel-model',type=Path,default=Path(__file__).with_name('sb_channel_vectors.py'))
    p.add_argument('--bridge-model',type=Path,default=Path(__file__).with_name('dig_sb_vectors.py'))
    p.add_argument('--header',type=Path,required=True);p.add_argument('--json',type=Path)
    args=p.parse_args();leaf=load('channel_leaf',args.channel_model);bridge=load('bridge_leaf',args.bridge_model)
    if args.json and args.header.resolve()==args.json.resolve():raise ValueError('distinct outputs required')
    result=build(leaf,bridge);out=header(result).encode('utf-8');leaf.new_or_identical(args.header,out)
    if args.json:leaf.new_or_identical(args.json,(json.dumps(result)+'\n').encode('utf-8'))
    print(json.dumps(dict(frames=len(result['pcm'])//2,ms_checkpoints=len(result['rows']),
        sha256=hashlib.sha256(out).hexdigest(),synthetic_policy=True,C_results_used=False)))
if __name__=='__main__':main()
