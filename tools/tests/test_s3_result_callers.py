"""S3独立caller读集拒绝门；不把工具自测视为原CPU或C执行证据。"""
import copy
import hashlib
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'ida'))
import verify_s3_result_callers as oracle

class CallerReadsetTests(unittest.TestCase):
    def setUp(self):
        self.image=bytes(range(64));self.sha=hashlib.sha256(self.image).hexdigest()
        self.report={'schema':'fd2-s3-result-callers-v1','functions':[],
            'table_refs':[{'from':at} for at in oracle.CALLS]}
        for index,va in enumerate(oracle.TARGETS):
            raw=self.image[index:index+1]
            self.report['functions'].append({'va':va,'blocks':[{'start':index}],
                'chunks':[{'start':index,'end':index+1,'raw_hex':raw.hex(),'sha256':hashlib.sha256(raw).hexdigest()}],
                'instructions':[{'ea':at,'disasm':'call    funcs_1197B[eax*4]'} for at,owner in oracle.CALLS.items() if owner==va] or [{'ea':index,'disasm':'ret'}]})
    def check(self,report=None,image=None):
        with patch.object(oracle,'RELOCATED_SHA256',self.sha):
            return oracle.validate(self.image if image is None else image,self.report if report is None else report)
    def test_complete_readset(self):
        self.assertEqual(set(self.check()),set(oracle.TARGETS))
    def test_missing_caller_or_site_or_xref_rejected(self):
        for which in range(3):
            row=copy.deepcopy(self.report)
            if which==0:row['functions'].pop()
            elif which==1:row['functions'][0]['instructions']=[{'ea':0,'disasm':'ret'}]
            else:row['table_refs'].pop()
            with self.assertRaises(ValueError):self.check(row)
    def test_wrong_image_chunk_or_incomplete_cfg_rejected(self):
        with self.assertRaises(ValueError):self.check(image=self.image+b'!')
        for field in ('raw_hex','sha256','blocks'):
            row=copy.deepcopy(self.report)
            if field=='blocks':row['functions'][0][field]=[]
            else:row['functions'][0]['chunks'][0][field]='01' if field=='raw_hex' else '0'*64
            with self.assertRaises(ValueError):self.check(row)
    def test_fixture_raw_gates_are_distinct(self):
        for side in (0,1):
            raw=oracle.records(side)
            self.assertEqual([raw[i*80+5] for i in range(5)],[0,1,128,0,4])
            self.assertEqual([raw[i*80+6] for i in range(5)],[2,side,side,side,side])
            self.assertEqual(raw[3*80+38],1)
if __name__=='__main__':unittest.main()
