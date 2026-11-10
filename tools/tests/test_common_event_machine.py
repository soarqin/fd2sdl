"""原码/C事件差分输入门；正例只是结构夹具，不冒称机码或C执行。"""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('common_event_machine',ROOT/'tools/ida/verify_fd2_common_events.py')
machine=importlib.util.module_from_spec(spec);spec.loader.exec_module(machine)

def cases():
    result=[]
    for name in sorted(machine.EXPECTED_IDS):
        kind=name.split('-')[0];units=bytearray(7680);state=bytearray(32)
        c={'case_id':name,'kind':kind,'count':96,'actor':0,'turn':255,'seed':0x7a18,
            'rng_after':0,'rng_calls':int(kind=='pair'),'result':0,'width':0,'height':0,
            'input_cells_hex':'','finish_cells_hex':''}
        if kind=='relative':
            _,side,gate,enabled=name.split('-');c['actor']=1
            units[86]=2 if int(side) else 0;state[17]=int(gate);state[18]=int(enabled)
        elif kind=='pair':
            _,count,seed=name.split('-');c['count']=int(count);c['seed']=int(seed);state[21]=c['count']-3
        elif kind=='turn':
            _,turn,hidden=name.split('-');c['turn']=int(turn);c['actor']=64;units[5125]=0xa0|int(hidden)
        else:
            c['width']=3;c['height']=2;c['input_cells_hex']='a5'*24;c['finish_cells_hex']='ff'*24
        for boundary in ('input','finish'):
            c[boundary+'_units_hex']=units.hex();c[boundary+'_state_hex']=state.hex();c[boundary+'_turn_slots_hex']='09'*48
        for field,raw in [('units',units),('state',state),('slots',bytes([9])*48)]:
            c['rng_input_'+field+'_hex']=raw.hex() if kind=='pair' else ''
        result.append(c)
    return result

def encode(rows):return ('\n'.join(json.dumps(c) for c in rows)+'\n').encode()

class CommonEventMachineTest(unittest.TestCase):
    def test_complete_structural_cover(self):
        self.assertEqual(len(machine.load_cases(encode(cases()))),30)
    def test_missing_duplicate_unknown_kind_and_count_rejected(self):
        for mutate in (lambda r:r.pop(),lambda r:r.__setitem__(1,r[0]),
            lambda r:r[0].update(kind='arbitrary_callback'),lambda r:r[0].update(count=97),
            lambda r:r[0].update(actor=96),lambda r:r[0].update(seed=True)):
            rows=cases();mutate(rows)
            with self.assertRaises(ValueError):machine.load_cases(encode(rows))
    def test_full_owners_and_actual_rng_boundary_required(self):
        for field in ('input_units_hex','finish_state_hex','finish_turn_slots_hex','rng_input_units_hex'):
            rows=cases();pair=next(c for c in rows if c['kind']=='pair');pair[field]=pair[field][:-2]
            with self.assertRaises(ValueError):machine.load_cases(encode(rows))
        rows=cases();next(c for c in rows if c['kind']=='pair')['rng_calls']=2
        with self.assertRaises(ValueError):machine.load_cases(encode(rows))
    def test_mismatched_trigger_and_signed_class_refused(self):
        rows=cases();c=next(c for c in rows if c['case_id']=='relative-0-0-1')
        raw=bytearray.fromhex(c['input_units_hex']);raw[86]=2;c['input_units_hex']=raw.hex()
        with self.assertRaises(ValueError):machine.load_cases(encode(rows))
        rows=cases();next(c for c in rows if c['case_id']=='turn-4294967295-1')['turn']=2147483647
        with self.assertRaises(ValueError):machine.load_cases(encode(rows))
    def test_machine_sources_do_not_accept_unrelated_image(self):
        with self.assertRaisesRegex(ValueError,'身份'):machine.validate_sources(b'not image',b'not exe',[])

if __name__=='__main__':unittest.main()
