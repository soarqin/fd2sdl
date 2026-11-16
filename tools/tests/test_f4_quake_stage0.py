"""Protocol/owner negatives only; no original/C pixel execution."""
from pathlib import Path
import copy
import io
import json
import sys
import unittest
from unittest.mock import patch
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import verify_f4_quake_stage0 as q

def packet(index=0):
    wanted=bytes((i*7+i//320)&255 for i in range(64000));palette=bytes([12])*768
    view=b''.join(wanted[y*320+4:y*320+316] for y in range(4,196))
    row={'case':index,'q':index%3,'idle':index//3,'rng':0,'game_unchanged':True,
         'input_palette_vga_unchanged':True,'jitter_only':True,'App0_committed':True,
         'vga':wanted.hex(),'app_view':view.hex(),'palette':palette.hex(),
         'dac':(bytes([48])*768).hex()}
    if not index%3:
        app=bytearray((53+i*13+i//456)&255 for i in range(153216))
        for y in range(192):app[(72+y)*456+72:(72+y)*456+384]=view[y*312:(y+1)*312]
        row['App0']=app.hex()
    return row,wanted,palette

class Protocol(unittest.TestCase):
    def test_all_nine_ordered_protocol_shapes(self):
        for index in range(9):
            row,wanted,palette=packet(index);q.compare_case(row,index,wanted,palette)
    def test_first_difference_and_App0_preserve(self):
        row,wanted,palette=packet();raw=bytearray.fromhex(row['vga']);raw[176]=255
        row['vga']=raw.hex()
        with self.assertRaisesRegex(ValueError,'byte176'):q.compare_case(row,0,wanted,palette)
        row,wanted,palette=packet();raw=bytearray.fromhex(row['App0']);raw[0]^=1;row['App0']=raw.hex()
        with self.assertRaisesRegex(ValueError,'App0'):q.compare_case(row,0,wanted,palette)
    def test_owner_and_phase_negatives(self):
        for key,value in [('q',1),('idle',1),('rng',1),('jitter_only',False),('App0_committed',False)]:
            row,wanted,palette=packet();row[key]=value
            with self.assertRaises(ValueError):q.compare_case(row,0,wanted,palette)
        row,wanted,palette=packet(1);row['App0']='00'
        with self.assertRaisesRegex(ValueError,'fresh'):q.compare_case(row,1,wanted,palette)
    def test_complete_App_view_and_palette(self):
        for key in ('app_view','palette','dac'):
            row,wanted,palette=packet();raw=bytearray.fromhex(row[key]);raw[-1]^=1;row[key]=raw.hex()
            with self.assertRaises(ValueError):q.compare_case(row,0,wanted,palette)
    def test_bounded_deflate(self):
        raw=bytes(range(16));self.assertEqual(q.unpack(zlib.compress(raw),16),raw)
        for bad,size in [(zlib.compress(raw)+b'late',16),(zlib.compress(raw),15),(zlib.compress(raw)[:-1],16)]:
            with self.assertRaises(ValueError):q.unpack(bad,size)
    def test_loaded_buffer_not_late_file_hash(self):
        # Fixed minimal DAT protocol; no asset pixels or original resources in tests.
        field=b'field';shape=b'shape';icons=b'icons';plain=b'plain'
        dat=b'LLLLLL'+(14).to_bytes(4,'little')+(19).to_bytes(4,'little')+shape
        resources={'FDFIELD.DAT':field,'FDSHAP.DAT':dat,'FDICON.B24':icons}
        row={'schema':'fd2-f4-quake-stage0-v1','field':field.hex(),'shape':shape.hex(),
             'icon':icons.hex(),'fixture_plain':plain.hex()}
        q.loaded_header(row,resources,plain)
        changed=copy.deepcopy(row);changed['icon']=b'Icons'.hex()
        with self.assertRaisesRegex(ValueError,'loaded bytes'):q.loaded_header(changed,resources,plain)
        changed=copy.deepcopy(row);changed['extra']=True
        with self.assertRaisesRegex(ValueError,'keys'):q.loaded_header(changed,resources,plain)
    def test_bounded_complete_row(self):
        self.assertEqual(q.line(io.BytesIO(b'{"ok":1}\n'),100)[1],{'ok':1})
        for raw,limit in [(b'{}',100),(b'{}\n',2),(b'',100)]:
            with self.assertRaises(ValueError):q.line(io.BytesIO(raw),limit)
    def test_temporary_owner_and_guard_extent_are_separate(self):
        reports=[Path('a').resolve(),Path('b').resolve()]
        qualification={'passed':True,'actual_original_sampler_cases':3,'derived_Q_idle_cases':9,
            'qualification_only_no_new_execution':True,'samplers':[{'report':str(reports[0]),
            'report_sha256':'a','effective_original_owner_bytes':64000,'mapped_not_write_guard_bytes':87648,
            'all_CPU_reads_and_writes_in_effective_owner':True}]}
        with patch.object(q,'freeze',return_value=json.dumps(qualification).encode()):
            with self.assertRaisesRegex(ValueError,'owner'):q.expected_frames(reports,['a','b'],Path('q'),'pin',[])

if __name__=='__main__':unittest.main()
