"""原帧协议/完整owner门的纯测；合成源只检结构，不作原义取证。"""
import copy
import hashlib
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tools/ida')]
import verify_s5_montage_frames as oracle


class MontageFrameGates(unittest.TestCase):
    def fixture(self):
        image=bytes(0x64000);rows=[]
        for start,end in oracle.EXTENTS.items():
            raw=image[start:end]
            rows.append({'va':start,'end':end,'blocks':[{'start':start}],
                'chunks':[{'start':start,'end':end,'raw_hex':raw.hex(),'sha256':hashlib.sha256(raw).hexdigest()}],
                'instructions':[{'ea':start,'size':len(raw),'raw_hex':raw.hex()}]})
        return image,{'image_sha256':hashlib.sha256(image).hexdigest(),'functions':rows}

    def test_requires_all_complete_owners_and_rejects_mid_instruction_request(self):
        image,report=self.fixture()
        with patch.object(oracle,'IMAGE_SHA',hashlib.sha256(image).hexdigest()):
            self.assertEqual(len(oracle.validate_source(image,report)),20)
            damaged=[]
            bad=copy.deepcopy(report);bad['functions'].pop();damaged.append(bad)
            bad=copy.deepcopy(report);bad['functions'].append(bad['functions'][0]);damaged.append(bad)
            bad=copy.deepcopy(report);bad['functions'][0]['instructions'][0]['size']-=1;damaged.append(bad)
            bad=copy.deepcopy(report);bad['functions'][0]['blocks']=[];damaged.append(bad)
            bad=copy.deepcopy(report);bad['requested_entries']=[{'owner_va':0x37042,'requested_va':0x37043}];damaged.append(bad)
            for bad in damaged:
                with self.assertRaises(ValueError):oracle.validate_source(image,bad)

    def test_full_64000_pixels_and_exact_HP_rng_branch_required(self):
        pixels=bytes((i*17+9)&255 for i in range(64000))
        expected={'hp':[63680,63680],'rng':2,'yes':-1,'pixels':pixels}
        actual={'status':0,'hp':[63680,63680],'rng':2,'yes':-1,'pixels_hex':pixels.hex()}
        oracle.validate_row(actual,expected)
        for change in ({'status':True},{'hp':[0,63680]},{'hp':[True,63680]},
            {'rng':1},{'rng':True},{'yes':0},{'pixels_hex':pixels.hex()[:-2]},
            {'pixels_hex':'00'+pixels.hex()[2:]},{'sha256':hashlib.sha256(pixels).hexdigest()}):
            with self.assertRaises(ValueError):oracle.validate_row({**actual,**change},expected)

    def test_nonframe_command_cannot_hide_unchecked_pixels(self):
        expected={'hp':[0,0],'rng':0,'yes':-1}
        actual={'status':0,**expected,'pixels_hex':''}
        oracle.validate_row(actual,expected)
        with self.assertRaises(ValueError):oracle.validate_row({**actual,'pixels_hex':'00'},expected)


if __name__=='__main__':unittest.main()
