"""Synthetic retained-protocol fault gates; no original/C/GUI execution."""
import ast
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import zlib

PATH=Path(__file__).resolve().parents[1]/'replay_f4_montage.py'
SPEC=importlib.util.spec_from_file_location('f4_montage_replay',PATH)
M=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(M)

def fixture(rng=0):
    image=bytearray(0x64000)
    image[0x526e9]=51;image[0x526fd]=103;image[0x52711]=4
    rows=[];packed=bytearray();hp=[0,0];calls=0
    commands=['o 51 103 4','i 8','f','h 0',f'r {rng}','p 1 63680','t','c 1','h 1',f'r {rng}','p 0 63680','d 0','q']
    pc={'o':0x31aee,'i':0x2ea3d,'f':0x2e6bf,'h':0x2ebe1,'r':0x4ebe3,'p':0x2ef9e,'t':0x2efc8,'c':0x25a96,'d':0x2f0c6,'q':0x2e763}
    for command in commands:
        op=command[0]
        if op=='r':calls+=1
        if op=='p':hp[int(command.split()[1])]=63680
        row=dict(command=command,source_pc=pc[op],hp=list(hp),rng=calls,yes=int(rng%100<3) if op=='r' else -1)
        if op in ('i','d'):
            pixels=bytes([len(rows)+1])*64000;data=zlib.compress(pixels)
            row.update(frame_offset=len(packed),frame_size=len(data),pixels_sha256=M.digest(pixels));packed+=data
        rows.append(row)
    case=dict(pair=0,rng_value=rng,actors=[51,103],scene=4,commands=len(rows),full_frames=2)
    return ('\r\n'.join(commands)+'\r\n').encode(),rows,bytes(packed),case,bytes(image)

class MontageReplayContracts(unittest.TestCase):
    def test_bounded_frame_exact_extent_no_tail(self):
        pixels=bytes([17])*64000;packed=zlib.compress(pixels)
        self.assertEqual(M.frame(packed),pixels)
        for data in [zlib.compress(pixels[:-1]),zlib.compress(pixels+b'x'),packed[:-1],packed+b'x',packed+packed]:
            with self.assertRaises(ValueError):M.frame(data)

    def test_retained_protocol_both_rng_branches_and_canonical_commands(self):
        for rng in [0,50]:
            commands,rows,packed,case,image=fixture(rng)
            events=list(M.case_events(commands,json.dumps(rows),packed,case,image))
            self.assertEqual(len(events),13);self.assertEqual(sum('pixels'in e for e in events),2)
            self.assertEqual([e['yes'] for e in events if e['command'][0]=='r'],[int(rng==0)]*2)
            self.assertEqual(events[-1]['hp'],[63680,63680])
            self.assertEqual([e['command'] for e in events],commands.decode().splitlines())

    def test_retained_hp_rng_branch_pc_and_command_tamper(self):
        commands,rows,packed,case,image=fixture()
        for field,value in [('hp',[1,0]),('rng',1),('yes',0),('source_pc',0x12345)]:
            changed=copy.deepcopy(rows);changed[0][field]=value
            with self.assertRaises(ValueError):list(M.case_events(commands,json.dumps(changed),packed,case,image))
        for changed in [commands+b'q\r\n',commands.replace(b'h 1',b'h 0')]:
            with self.assertRaises(ValueError):list(M.case_events(changed,json.dumps(rows),packed,case,image))

    def test_retained_spans_hashes_and_unmatched_payload(self):
        commands,rows,packed,case,image=fixture()
        for field,value in [('frame_offset',1),('frame_offset',False),('frame_size',0),('pixels_sha256','0'*64)]:
            changed=copy.deepcopy(rows);changed[1][field]=value
            with self.assertRaises(ValueError):list(M.case_events(commands,json.dumps(changed),packed,case,image))
        for data in [packed[:-1],packed+b'x']:
            with self.assertRaises(ValueError):list(M.case_events(commands,json.dumps(rows),data,case,image))

    def test_exact_actual_row_hp_rng_branch_and_full_pixels(self):
        commands,rows,packed,case,image=fixture();event=list(M.case_events(commands,json.dumps(rows),packed,case,image))[1]
        actual=dict(status=0,hp=event['hp'],rng=event['rng'],yes=event['yes'],pixels_hex=event['pixels'].hex())
        M.compare(actual,event)
        for field,value in [('status',False),('hp',[0,1]),('rng',1),('yes',0),('pixels_hex','00')]:
            changed={**actual,field:value}
            with self.assertRaises(ValueError):M.compare(changed,event)
        for at in (0,63999):
            pixels=bytearray(event['pixels']);pixels[at]^=1
            with self.assertRaises(ValueError):M.compare({**actual,'pixels_hex':pixels.hex()},event)
        with self.assertRaises(ValueError):M.compare({**actual,'extra':1},event)
        with self.assertRaises(ValueError):M.unique('{"status":0,"status":1}')

    def test_shared_external_output_guard_and_retained_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);root=base/'project';(root/'original_game').mkdir(parents=True)
            evidence=base/'evidence';evidence.mkdir();retained=evidence/'old';retained.mkdir()
            args=SimpleNamespace(root=root,out=evidence/'new',output_root=evidence,retained=retained)
            self.assertEqual(M.output_path(args),(evidence/'new').resolve())
            for bad in [evidence,retained,retained/'new',root/'original_game/new',base/'escaped']:
                args.out=bad
                with self.assertRaises(ValueError):M.output_path(args)

    def test_real_actual_stream_rejects_omission_extra_and_corruption(self):
        commands,rows,packed,case,image=fixture()
        events=list(M.case_events(commands,json.dumps(rows),packed,case,image))
        actual=[dict(status=0,hp=e['hp'],rng=e['rng'],yes=e['yes'],pixels_hex=e.get('pixels',b'').hex()) for e in events]
        raw=b''.join(json.dumps(row).encode()+b'\n' for row in actual)
        M.compare_stream(io.BytesIO(raw),events,'fixture')
        for bad in [b'',b'\n'.join(raw.splitlines()[:-1])+b'\n',raw+b'{}\n',raw+b'\n']:
            with self.assertRaises(ValueError):M.compare_stream(io.BytesIO(bad),events,'fixture')
        changed=copy.deepcopy(actual);changed[1]['pixels_hex']='00'*64000
        bad=b''.join(json.dumps(row).encode()+b'\n' for row in changed)
        with self.assertRaisesRegex(ValueError,'command1 i 8'):
            M.compare_stream(io.BytesIO(bad),events,'fixture')

    def test_late_source_binary_guard_preserves_missing_and_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'source';path.write_bytes(b'one');_,info=M.frozen(path)
            self.assertTrue(M.late([info])['unchanged'])
            path.write_bytes(b'two');self.assertFalse(M.late([info])['unchanged'])
            path.unlink();result=M.late([info]);self.assertFalse(result['unchanged']);self.assertFalse(result['after'][0]['available'])

    def test_no_original_execution_entry_and_index_source_gate(self):
        tree=ast.parse(PATH.read_text(encoding='utf8'))
        forbidden=[n for n in ast.walk(tree) if isinstance(n,ast.Attribute) and isinstance(n.value,ast.Name) and n.value.id=='legacy' and n.attr in {'execute','main'}]
        self.assertFalse(forbidden)
        with mock.patch.object(M,'source_inputs',side_effect=ValueError('source gate')),\
            mock.patch.object(M.legacy,'execute') as original,mock.patch.object(M.subprocess,'run') as actual:
            with self.assertRaisesRegex(ValueError,'source gate'):M.index(SimpleNamespace())
            original.assert_not_called();actual.assert_not_called()

if __name__=='__main__':unittest.main()
