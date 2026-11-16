"""C14 independent source-role, typed owner and qualification guard properties."""
from pathlib import Path
import importlib.util,json,tempfile,unittest,zlib
from unittest.mock import patch
ROOT=Path.cwd();p=Path(__file__).resolve().parents[1]/'verify_f4_title_composition.py'
if not p.exists():p=ROOT/'tools/verify_f4_title_composition.py'
s=importlib.util.spec_from_file_location('title_reviewed',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class MathematicalOracle(unittest.TestCase):
 def test_skip_and_stride_holes_use_proven_zero_not_retained_seed(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);source=b'\2\0\2\0raw';pixels=bytes([11,100,22,135]);mask=bytes([1,0,1,0]);raw=pixels+bytes([165])*768+mask+bytes(768);packed=zlib.compress(raw)
   (root/'leaf.zbin').write_bytes(packed)
   o=m.Original.__new__(m.Original);o.cache={};o.other=[source];o.nested=[];o.d={'cases':[{'request':['scene','other',0,0,0],'source_sha256':m.sha(source),'width':2,'height':2,'original_payload':{'path':'leaf.zbin','sha256':m.sha(packed)}}]}
   with patch.object(m,'REPORT',root/'report.json'):self.assertEqual(o.image('other',0),(2,2,bytes([11,0,22,0])))
 def test_corrupt_unwritten_retained_pixel_is_not_accepted(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);source=b'\2\0\1\0raw';raw=bytes([11,99])+bytes([165])*768+bytes([1,0])+bytes(768);packed=zlib.compress(raw);(root/'leaf.zbin').write_bytes(packed)
   o=m.Original.__new__(m.Original);o.cache={};o.other=[source];o.nested=[];o.d={'cases':[{'request':['scene','other',0,0,0],'source_sha256':m.sha(source),'width':2,'height':1,'original_payload':{'path':'leaf.zbin','sha256':m.sha(packed)}}]}
   with patch.object(m,'REPORT',root/'report.json'),self.assertRaises(ValueError):o.image('other',0)
 def test_signed_idiv_truncates_negative_toward_zero(self):
  self.assertEqual(m.trunc(-56,40),-1);self.assertEqual(m.fade(bytes([0])*768,1)[0],(55<<2)|(55>>4))
  self.assertEqual(m.fade(bytes([63])*768,1)[0],(56<<2)|(56>>4))
  self.assertEqual(m.fade(bytes([0])*768,0),bytes([227,243,255])*256)
 def test_all536_windows_end_inside_the735row_owner(self):
  owner=bytes(y&255 for y in range(735) for _ in range(320))
  for y in range(536):
   window=owner[y*320:(y+200)*320];self.assertEqual(len(window),64000);self.assertEqual(window[0],y&255);self.assertEqual(window[-1],(y+199)&255)
 def test_interleaving_and_overlap_are_not_14_times11(self):
  t=(520,430,410,340,310,300,240,180,150,130,110,87,64,22,1000);states=list(m.scroll_operators(t));banks={y:(bank,count) for y,bank,count in states}
  self.assertEqual(len(states),536);self.assertEqual(sum(bank==102 for _,bank,_ in states),152)
  self.assertEqual(banks[330],(101,10));self.assertEqual(banks[329],(101,11));self.assertEqual(banks[300],(102,0));self.assertEqual(banks[310],(102,0))
 def test_absolute_menu_coordinates_no_viewport_shift(self):
  o=m.Original.__new__(m.Original);o.base=lambda:bytes([83])*64000
  def image(op,i):return (61 if i<3 else 62,8 if i>=5 else 7,bytes([i])*(61 if i<3 else 62)*(8 if i>=5 else 7))
  o.image=image
  page=o.menu(3,2,True)
  self.assertEqual((page[164*320+129],page[173*320+129],page[182*320+129]),(1,3,6));self.assertEqual(page[164*320+128],83)
  pages={o.menu(n,s,h) for n in (2,3) for s in range(n) for h in (0,1)};self.assertEqual(len(pages),7) # all-normal repeats within each count
  with self.assertRaises(ValueError):o.menu(2,2,True)
class Qualification(unittest.TestCase):
 def test_missing_producer_dependency_blocks_before_launch(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t);r=p/'producer.json';r.write_text(json.dumps({'schema':'fd2-f4-title-producer-v1','source_root':str(p),'sources':{}}))
   with self.assertRaisesRegex(ValueError,'dependency'),patch.object(m.core.subprocess,'Popen') as launch:m.producer(r,p)
   launch.assert_not_called()
 def test_disallowed_mode_is_rejected_before_any_output(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t)
   with self.assertRaises(ValueError),patch.object(m.core.subprocess,'Popen') as launch:m.core.Probe(p/'no.exe',p,p,'invalid',1,{},mode='--title-from-guess')
   launch.assert_not_called();self.assertEqual(list(p.iterdir()),[])
if __name__=='__main__':unittest.main()
