"""Pure bounded feedback contract tests; no binary/window/resources required."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

FILE=Path(__file__).resolve().parents[1]/'run_f4_dialog_feedback_check.py'
if not FILE.is_file():FILE=Path.cwd()/'tools/run_f4_dialog_feedback_check.py'
spec=importlib.util.spec_from_file_location('feedback_tested',FILE)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Pack:
 def __init__(self):
  self.marker=bytes((i*13+i//72*7)%256 for i in range(24*24*3));self.anchor_pixels=list(range(252))
 def final_models(self,base):
  p=bytearray(base);p[0:3]=b'END';return[(0,bytes(p))]

def sample(pack,index,k=None,base=None,final=False):
 p=bytearray(base or bytes([17,19,23])*64000)
 if k is not None:
  x,y=m.marker_xy(k)
  for r in range(24):p[((y+r)*320+x)*3:((y+r)*320+x+24)*3]=pack.marker[r*72:(r+1)*72]
 if final:p[0:3]=b'END'
 return{'RGB':bytes(p),'index':index,'path':str(index)+'.png','sha256':'synthetic-'+str(index)}

class Contract(unittest.TestCase):
 def setUp(self):self.pack=Pack()
 def rows(self):return[sample(self.pack,0,11),sample(self.pack,1,2),sample(self.pack,2,5),sample(self.pack,3,7),sample(self.pack,4,11),sample(self.pack,5,final=True)]
 def test_original_parameter_vectors_signed_endpoints(self):
  self.assertEqual([m.marker_xy(k)for k in(0,2,5,7,11)],[(148,124),(122,122),(83,119),(57,117),(5,112)])
 def test_full_marker_plus_final_positive_and_close_endpoint_ignored(self):
  q=m.qualify(self.rows(),self.pack);self.assertTrue(q['whole_before_relation_qualified']);self.assertEqual([w['k']for w in q['markers']],[2,5,7,11]);self.assertEqual(q['final16'][0]['index'],5)
  self.assertFalse(q['absolute_compositor_qualified']);self.assertFalse(q['early_key_qualified']);self.assertFalse(q['all_five_open_stages_observed'])
 def test_shared_close_endpoint_cannot_qualify_opening(self):
  with self.assertRaisesRegex(ValueError,'disjoint'):m.qualify([sample(self.pack,0,11),sample(self.pack,1,final=True)],self.pack)
 def test_missing_disjoint_before_refused(self):
  with self.assertRaisesRegex(ValueError,'disjoint'):m.qualify([sample(self.pack,0,2),sample(self.pack,1,2)],self.pack)
 def test_one_raw74_pixel_changed_is_not_discarded(self):
  rows=self.rows();x,y=m.marker_xy(2);p=bytearray(rows[1]['RGB']);t=300;p[((y+t//24)*320+x+t%24)*3]^=1;rows[1]['RGB']=bytes(p)
  with self.assertRaisesRegex(ValueError,'raw74'):m.qualify(rows,self.pack)
 def test_marker_outside_pixel_change_rejects_whole_page(self):
  rows=self.rows();p=bytearray(rows[2]['RGB']);p[0]^=1;rows[2]['RGB']=bytes(p)
  with self.assertRaisesRegex(ValueError,'full64000'):m.qualify(rows,self.pack)
 def test_full_final_requires_outside_page_not_only_text_roi(self):
  rows=self.rows();p=bytearray(rows[-1]['RGB']);p[-1]^=1;rows[-1]['RGB']=bytes(p)
  with self.assertRaisesRegex(ValueError,'final16 whole64000'):m.qualify(rows,self.pack)
 def test_final_before_marker_not_accepted(self):
  rows=[sample(self.pack,0,final=True),sample(self.pack,1,2),sample(self.pack,2,5)]
  with self.assertRaisesRegex(ValueError,'final16 whole64000'):m.qualify(rows,self.pack)
 def test_ambiguous_marker_params_reject(self):
  self.pack.marker=bytes([0])*1728
  with self.assertRaisesRegex(ValueError,'ambiguous'):m.qualify([{'index':0,'RGB':bytes(192000)}],self.pack)
 def test_two_valid_opening_groups_not_silently_chosen(self):
  rows=[sample(self.pack,i,k)for i,k in enumerate([2,5,2,5])]
  with self.assertRaisesRegex(ValueError,'one ordered'):m.qualify(rows,self.pack)
 def test_source_snapshot_detects_later_byte_change(self):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d)/'source';p.write_bytes(b'a');g={};m.read(p,g,m.sha(b'a'));p.write_bytes(b'b')
   with self.assertRaisesRegex(ValueError,'first read'):m.read(p,g)
 def test_only_one_exact_current_sdl_command(self):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d)/'fd2sdl.exe';m.current_command([str(p)],p,0)
   with self.assertRaisesRegex(ValueError,'only one'):m.current_command([str(p)],p,1)
   with self.assertRaisesRegex(ValueError,'only one'):m.current_command([str(Path(d)/'dosbox.exe')],p,0)

if __name__=='__main__':unittest.main()
