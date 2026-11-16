"""Candidate-only tests; no assets/C/CPU/GUI. Not run by the preparation lane."""
import importlib.util
from pathlib import Path
import struct
import unittest

PATH=Path(__file__).resolve().parents[1]/'verify_f4_extra_resources.py'
SPEC=importlib.util.spec_from_file_location('fd2_extra',PATH)
M=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(M)

class ExtraResourceContracts(unittest.TestCase):
    def test_raw_zero_is_an_opaque_write_and_outside_bytes_are_preserved(self):
        source=bytes(10)+struct.pack('<HH',2,2)+bytes([0,255,83,172])
        row={'file':'sample','start':10,'end':18,'derived':{'kind':'ui_raw'}}
        raw,stride,height,scope=M.derived_payload(row,{'sample':source})
        page,palette,mask,pmask=raw[:64000],raw[64000:64768],raw[64768:128768],raw[128768:]
        self.assertEqual((stride,height),(320,200))
        self.assertEqual([page[i] for i in (1284,1285,1604,1605)],[0,255,83,172])
        self.assertEqual([i for i,value in enumerate(mask) if value],[1284,1285,1604,1605])
        for i in (0,1283,1286,1603,1606,63999):
            self.assertEqual(page[i],(i*17+i//320+83)&255)
        self.assertEqual((palette,pmask),(bytes([165])*768,bytes(768)))
        self.assertIn('not CPU',scope)

    def test_lut_is_256_loaded_values_not_a_lut_application(self):
        data=bytes(reversed(range(256)));row={'file':'sample','start':0,'end':256,'derived':{'kind':'lut'}}
        raw,width,height,scope=M.derived_payload(row,{'sample':data})
        self.assertEqual((width,height,raw[:256]),(256,1,data))
        self.assertIn('no original RMW/mask comparison',scope)
        self.assertEqual(len(raw),2048)

    def test_wave_and_palette_are_value_contracts_without_original_masks(self):
        data=struct.pack('<HH',320,200)+bytes([0,255])*32000
        row={'file':'sample','start':0,'end':len(data),'derived':{'kind':'wave_raw'}}
        raw,width,height,scope=M.derived_payload(row,{'sample':data})
        self.assertEqual((width,height,raw[:64000]),(320,200,data[4:]))
        self.assertIn('not original pixel-write mask',scope)
        pal=bytes([0,63,31])*256
        row={'file':'sample','start':0,'end':768,'derived':{'kind':'palette'}}
        raw,width,height,scope=M.derived_payload(row,{'sample':pal})
        self.assertEqual(raw[64000:64768],pal)
        self.assertIn('DAC/write-mask not qualified',scope)

    def test_unknown_ui_rows_are_not_promoted_by_directory_count(self):
        self.assertTrue(M.RAW_UI.isdisjoint(M.RLE_UI))
        self.assertTrue(M.RAW_UI.isdisjoint(M.LMI_UI))
        self.assertTrue(M.RLE_UI.isdisjoint(M.LMI_UI))
        self.assertEqual(set(range(138))-(M.RAW_UI|M.RLE_UI|M.LMI_UI),{58,137})
        self.assertIn(92,M.RAW_UI)
        self.assertIn(132,M.RLE_UI)
        self.assertIn(20,M.LMI_UI)

    def test_directory_refuses_overlap_and_owner_escape(self):
        good=b'header'+struct.pack('<II',14,16)+b'abcd'
        self.assertEqual(M.directory(good,2),[(14,b'ab'),(16,b'cd')])
        for offsets in [(14,14),(10,16),(14,18),(18,16)]:
            with self.subTest(offsets=offsets),self.assertRaises(ValueError):
                M.directory(b'header'+struct.pack('<II',*offsets)+b'abcd',2)

if __name__=='__main__':unittest.main()
