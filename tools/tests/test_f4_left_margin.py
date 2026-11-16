"""只测新opcode分区/linear header/协议，合成bytes不算原像素执行。"""
import importlib.util,sys,struct,unittest
from pathlib import Path
from unittest import mock
root=Path(__file__).resolve().parents[2]
if '--project-root' in sys.argv:
    i=sys.argv.index('--project-root');root=Path(sys.argv[i+1]);del sys.argv[i:i+2]
spec=importlib.util.spec_from_file_location('left',Path(__file__).resolve().parents[1]/'verify_f4_left_margin.py')
left=importlib.util.module_from_spec(spec)
with mock.patch.object(sys,'argv',['test','--root',str(root)]):spec.loader.exec_module(left)

class Protocol(unittest.TestCase):
    def test_original_opcode_partition_not_all_mask_zero(self):
        row=bytes([0xc3,0x83,0,1,128,255,3,37,0x43,73,0xc3]);k=left.kinds(row*24)
        self.assertEqual(list(k[:24]),[2]*4+[1]*8+[0,1]*4+[2]*4)
    def test_rmw_skip_but_not_stride_hole(self):
        page=bytearray([17])*left.P;mask=bytearray(left.P);kind=bytearray(576)
        kind[0]=2;kind[1]=0;kind[2]=1;pixels=bytes([99])*576;lut=bytes((i+1)&255 for i in range(256))
        left.draw(page,mask,48,72,pixels,kind,lut,True);at=72*456+48
        self.assertEqual(list(page[at:at+3]),[18,17,100]);self.assertEqual(list(mask[at:at+3]),[1,0,1])
    def test_overlay_skip_never_reads_old_lut(self):
        page=bytearray([17])*left.P;mask=bytearray(left.P);kind=bytearray(576);kind[0]=2
        left.draw(page,mask,0,0,bytes(576),kind,bytes(256),False)
        self.assertEqual(page[0],17);self.assertEqual(mask[0],0)
    def test_truncated_source(self):
        with self.assertRaises(ValueError):left.kinds(bytes([0xd7])*23)
    def test_row_overflow(self):
        with self.assertRaises(ValueError):left.kinds(bytes([0xff]))
    def test_tail_is_not_ignored(self):
        with self.assertRaises(ValueError):left.kinds(bytes([0xd7])*24+b'\0')
    def test_wh_header_is_real_four_bytes(self):
        data=struct.pack('<HH',13,8)+b'\0'*(13*8*4)
        self.assertEqual(left.cell_at(data,-1,0),13|(8<<16))
        self.assertEqual(left.cell_at(data,-1,0)>>24,0)
    def test_previous_row_last_and_reset_mask(self):
        data=bytearray(struct.pack('<HH',13,8)+b'\0'*(13*8*4));struct.pack_into('<I',data,4+12*4,0x3faaffab)
        self.assertEqual(left.cell_at(data,-1,1),(0x3faaffab&0x001f03ff)|0xff000000)
    def test_before_header_source_rejected(self):
        data=struct.pack('<HH',13,8)+b'\0'*(13*8*4)
        with self.assertRaises(ValueError):left.cell_at(data,-2,0)
    def test_destination_owner_bound(self):
        with self.assertRaises(ValueError):left.draw(bytearray(left.P),bytearray(left.P),-1,0,bytes(576),bytearray(576))

if __name__=='__main__':unittest.main()
