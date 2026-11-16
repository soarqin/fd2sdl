"""Protocol only; no resource/C/original execution."""
import copy,sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import verify_f4_magic_work as m

class Protocol(unittest.TestCase):
    def setUp(self):
        self.spec=dict(request=[0,27,16,0,18880,1,-1,-1,18880],owner_bytes=172800,idle_part=16,idle_ticks=0)
        self.row=dict(schema='fd2-f4-magic-work-actual-v1',case=0,**self.spec,payload_bytes=m.PAYLOAD,other_owners_unchanged=True)
    def test_valid(self):m.check_header(self.row,self.spec,0)
    def test_wrong_owner(self):
        self.row['owner_bytes']=128000
        with self.assertRaises(ValueError):m.check_header(self.row,self.spec,0)
    def test_wrong_idle(self):
        self.row['idle_ticks']=1
        with self.assertRaises(ValueError):m.check_header(self.row,self.spec,0)
    def test_wrong_request(self):
        self.row=copy.deepcopy(self.row);self.row['request'][5]=0
        with self.assertRaises(ValueError):m.check_header(self.row,self.spec,0)
    def test_short_payload(self):
        self.row['payload_bytes']-=1
        with self.assertRaises(ValueError):m.check_header(self.row,self.spec,0)
    def test_unexpected_key(self):
        self.row['ignored']=True
        with self.assertRaises(ValueError):m.check_header(self.row,self.spec,0)

if __name__=='__main__':unittest.main()
