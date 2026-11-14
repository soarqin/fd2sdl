"""S5实际C像素协议、完整原码范围和原资源输出保护；无GUI/原档修改。"""
from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import tempfile
import unittest
import sys
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/ida'))
spec=importlib.util.spec_from_file_location('s5_cinematic',ROOT/'tools/ida/verify_s5_cinematic.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
spec=importlib.util.spec_from_file_location('s5_montage',ROOT/'tools/ida/verify_s5_montage.py')
montage=importlib.util.module_from_spec(spec);spec.loader.exec_module(montage)
import verify_s5_ani_palette as ani


class CinematicSourceTest(unittest.TestCase):
    def fixture(self,ranges):
        image=bytes(0x64000)
        rows=[]
        for start,end in ranges:
            raw=image[start:end]
            rows.append({'va':start,'end':end,'blocks':[{'start':start}],
                'chunks':[{'start':start,'end':end,'raw_hex':raw.hex(),
                    'sha256':hashlib.sha256(raw).hexdigest()}],
                'instructions':[{'ea':start,'size':end-start,'raw_hex':raw.hex()}]})
        return image,{'image_sha256':hashlib.sha256(image).hexdigest(),'functions':rows}

    def test_decoder_whole_extent_and_strict_instruction_source(self):
        image,row=self.fixture(((0x2eb9f,0x2ebe1),(0x4e98d,0x4eb48)))
        with patch.object(oracle,'IMAGE_SHA',hashlib.sha256(image).hexdigest()):
            self.assertEqual(len(oracle.validate_source(image,row)),2)
            broken=[]
            bad=copy.deepcopy(row);bad['functions'].pop();broken.append(bad)
            bad=copy.deepcopy(row);bad['functions'][0]['instructions']=[];broken.append(bad)
            bad=copy.deepcopy(row);bad['functions'][0]['instructions'][0]['size']-=1;broken.append(bad)
            bad=copy.deepcopy(row);bad['functions'][0]['chunks'][0]['raw_hex']='ff';broken.append(bad)
            for bad in broken:
                with self.assertRaises(ValueError):oracle.validate_source(image,bad)

    def test_montage_complete_function_not_sparse_instructions(self):
        image,row=self.fixture(((0x2ebe1,0x2f4d4),))
        with patch.object(montage,'IMAGE_SHA',hashlib.sha256(image).hexdigest()):
            self.assertEqual(montage.validate(image,row)['va'],0x2ebe1)
            bad=copy.deepcopy(row);bad['functions'][0]['instructions'][0]['size']-=1
            with self.assertRaises(ValueError):montage.validate(image,bad)

    def test_ANI_complete_caller_tail_and_ten_opcode_readset(self):
        image,row=self.fixture(tuple((va,va+1) for va in sorted(ani.REQUIRED)))
        with patch.object(ani,'IMAGE_SHA',hashlib.sha256(image).hexdigest()):
            self.assertEqual(len(ani.validate(image,row)),14)
            for target in (0x190ac,0x20421,0x36fd3,0x36ff4,0x36e57):
                bad=copy.deepcopy(row);bad['functions']=[f for f in bad['functions'] if f['va']!=target]
                with self.assertRaises(ValueError):ani.validate(image,bad)
            bad=copy.deepcopy(row);bad['functions'][0]['instructions']=[]
            with self.assertRaises(ValueError):ani.validate(image,bad)
            bad=copy.deepcopy(row);bad['functions'][0]['chunks']=[]
            with self.assertRaises(ValueError):montage.validate(image,bad)

    def test_actual_c_full_pixels_not_digest_or_shape_only(self):
        expected=bytes((i*13)&255 for i in range(64000))
        row={'status':0,'pixels_hex':expected.hex()}
        oracle.validate_probe([row],[expected])
        for change in ({'status':True},{'status':-1},{'pixels_hex':expected.hex()[:-2]},
                       {'pixels_hex':'ff'+expected.hex()[2:]},{'extra':'unchecked'}):
            with self.assertRaises(ValueError):oracle.validate_probe([{**row,**change}],[expected])
        with self.assertRaises(ValueError):oracle.validate_probe([], [expected])

    def test_duplicate_json_status_does_not_hide_failure(self):
        with self.assertRaises(ValueError):
            json.loads('{"status":-1,"status":0}',object_pairs_hook=oracle.unique_keys)

    def test_output_never_overwrites_original_or_inputs(self):
        with tempfile.TemporaryDirectory() as name:
            dest=Path(name)/'new.json';oracle.validate_output(dest)
            with self.assertRaises(ValueError):oracle.validate_output(dest,(dest,))
            dest.write_text('{}')
            with self.assertRaises(ValueError):oracle.validate_output(dest)
        with self.assertRaises(ValueError):oracle.validate_output(ROOT/'original_game'/'s5-new.json')


if __name__=='__main__':unittest.main()
