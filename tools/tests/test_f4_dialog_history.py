"""Independent source-shaped history properties; no original/C/GUI execution."""
from pathlib import Path
import importlib.util,json,os,subprocess,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
if not (ROOT/'tools/verify_f4_dialog_history.py').exists():ROOT=Path.cwd()
p=Path(__file__).resolve().parents[1]/'verify_f4_dialog_history.py'
if not p.exists():p=ROOT/'tools/verify_f4_dialog_history.py'
s=importlib.util.spec_from_file_location('dialog_under_review',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class RawFixture:
 strict_image=bytes(0x60003)+bytes(range(48))+bytes(range(45))
 raw=m.Original.raw;raw_at=m.Original.raw_at;box=m.Original.box;stages=m.Original.stages
 def __init__(self):
  dims=[(24,24),(3,3),(3,3),(3,3),(3,3),(16,3),(16,3),(16,3),(16,3),(16,3),(3,16),(3,16),(16,3),(16,16),(3,16),(3,16),(3,16),(3,16),(14,10),(14,10)]
  self.ui_blocks=[m.struct.pack('<HH',w,h)+bytes([74 if i in (0,13) else i])*(w*h) for i,(w,h) in enumerate(dims)]
 def portrait(self,p,identity,frame,area):
  x,y=(8,115) if area else (232,5)
  for row in range(y,y+80):p[row*320+x:row*320+x+80]=bytes([100+frame])*80
 def glyph_at(self,p,glyph,x,y):p[y*320+x]=205
class Geometry(unittest.TestCase):
 def test_complete_box_extent_and_retained_outside(self):
  o=RawFixture();p=bytearray([199])*64000;o.box(p,0,4,2)
  self.assertEqual({i for i,x in enumerate(p) if x!=199},{y*320+x for y in range(2,40) for x in range(5,75)})
  self.assertEqual([p[2*320+5],p[2*320+74],p[39*320+5],p[39*320+74],p[5*320+8]],[1,2,3,4,74])
 def test_five_levels_and_closing_history(self):
  o=RawFixture()
  for area in (0,1):
   states=o.stages(m.seed(),area);self.assertEqual(len(set(states)),5)
   for level in range(5):
    packet,meta=m.expected(o,['scene','box',area,level,1]);wanted=states[3-level] if level<4 else m.seed()
    self.assertEqual(packet[:64000],wanted);self.assertEqual((meta['trace'],meta['queued'],meta['clears']),('UD'*4,1,0))
 def test_signed_reverse_is_not_reversed_open_array(self):
  self.assertEqual(m.marker_xy(0,2,1,False),(17,15));self.assertEqual(m.marker_xy(0,2,1,True),(16,15))
  self.assertEqual(m.marker_xy(1,2,1,True),(16,70));self.assertEqual(m.marker_xy(0,2,0,False),(28,28))
 def test_marker74_write_and_n0_no_phase(self):
  o=RawFixture();p,h=m.expected(o,['scene','marker',0,0,0]);self.assertEqual(p[28*320+28:28*320+52],bytes([74])*24);self.assertEqual(h['clears'],3)
  p,h=m.expected(o,['scene','marker',1,0,2]);self.assertEqual(p[:64000],m.seed());self.assertEqual((h['trace'],h['queued']),('',1))
 def test_scroll_19_rows_and_asymmetric_clear(self):
  p=bytearray(y for y in range(200) for _ in range(320));m.scroll(p,0)
  self.assertEqual([p[9*320+14],p[81*320+14],p[81*320+15],p[81*320+222],p[81*320+223],p[82*320+15]],[28,81,74,74,81,82])
 def test_page_cleans16_not14_by10(self):
  p,h=m.expected(RawFixture(),['scene','page',0,0,0]);self.assertEqual(p[66*320+111:66*320+127],bytes([74])*16)
  self.assertEqual(p[81*320+126],74);self.assertEqual(p[81*320+127],m.seed()[81*320+127]);self.assertEqual((h['trace'],h['rng'],h['last_key'],h['water']),('RK',1,0x1579,0))
class Prefix(unittest.TestCase):
 def test_open_cutoff_fresh_ack_not_old_z(self):
  _,h=m.expected(RawFixture(),['scene','box',0,0,3]);self.assertEqual(h['trace'],'UDUDUDUDCcRK');self.assertEqual((h['queued'],h['last_key'],h['clears'],h['rng']),(0,0x1579,1,1))
 def test_host_prefix_has_no_new_delay_or_clear(self):
  _,h=m.expected(RawFixture(),['scene','box',1,2,2]);self.assertEqual(h['trace'],'UDUDU');self.assertEqual((h['delay_ms'],h['queued'],h['clears'],h['captured']),(20,1,0,3))
 def test_mouth_shared_phase_and_nonconsuming_early_key(self):
  o=RawFixture();phases=[]
  for n in range(1,9):
   p,h=m.expected(o,['scene','mouth',0,n,0]);phases.append(p[5*320+232]);self.assertEqual((h['ticks'],h['audio']),(n,n))
  self.assertEqual(phases,[100,101,101,102,102,101,101,100])
  p,h=m.expected(o,['scene','mouth',0,8,1]);self.assertEqual(p[5*320+232],100);self.assertEqual((h['mouth'],h['glyph_counter'],h['queued'],h['ticks'],h['audio']),(0,0,1,0,0))
 def test_glyph_host_audio_error_prefix(self):
  _,h=m.expected(RawFixture(),['scene','mouth',0,2,2]);_,f=m.expected(RawFixture(),['scene','mouth',0,2,3])
  self.assertEqual((h['trace'],h['skip'],h['ticks'],h['audio']),('IU',1,0,0));self.assertEqual((f['trace'],f['source_result'],f['ticks'],f['audio'],f['mouth']),('IUATIUUA',-1,1,2,1))
 def test_two_full_pages_not_fake_history_mask(self):
  p,h=m.expected(RawFixture(),['scene','scroll',1,0,0]);self.assertEqual(len(p),257536);self.assertEqual(p[128768:],bytes(128768));self.assertNotEqual(p[:64000],p[64000:128000]);self.assertEqual(h['rng'],0)
 def test_unique_finite_denominator(self):
  c=m.cases();self.assertEqual(len(c),155);self.assertEqual(len({tuple(x['request']) for x in c}),155);self.assertTrue(all(x['stride']==320 and x['height']==400 for x in c))
 def test_idle_deadline_and_arrow19_prefix_are_separate(self):
  o=RawFixture();prefix,h=m.expected(o,['scene','idle',0,2,0]);final,z=m.expected(o,['scene','idle',0,2,1])
  self.assertEqual(prefix[71*320+111],19);self.assertEqual(final[71*320+111],74)
  self.assertEqual(h['trace'],'RHIUEHIUEHIUEK');self.assertEqual((h['water'],h['water_phase'],h['water_anchor'],h['rng']),(3,3,106,1));self.assertEqual(h,z)
 def test_idle_host_does_not_consume_key_or_cleanup_arrow(self):
  p,h=m.expected(RawFixture(),['scene','idle',1,3,0]);self.assertEqual(p[181*320+191],18)
  self.assertEqual((h['trace'],h['skip'],h['last_key'],h['host_stopped']),('RHIUE',1,0,1))
class Boundaries(unittest.TestCase):
 def test_original_output_alias(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t);(p/'original_game').mkdir();alias=p/'evidence'
   if os.name=='nt':
    r=subprocess.run(['cmd','/c','mklink','/J',str(alias),str(p/'original_game')],capture_output=True)
    if r.returncode:self.skipTest('temporary junction unavailable')
   else:os.symlink(p/'original_game',alias,target_is_directory=True)
   with self.assertRaisesRegex(ValueError,'original'):m.core.checked_output(p,alias/'new',p)
   if os.name=='nt':os.rmdir(alias)
 def test_unqualified_producer_never_launches(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t);f=p/'producer.json';f.write_text(json.dumps({'schema':'fd2-f4-dialog-producer-v1','source_root':str(p),'sources':{}}))
   with self.assertRaisesRegex(ValueError,'direct dependencies'),patch.object(m.core.subprocess,'Popen') as launch:m.producer(f,p,{})
   launch.assert_not_called()
if __name__=='__main__':unittest.main()
