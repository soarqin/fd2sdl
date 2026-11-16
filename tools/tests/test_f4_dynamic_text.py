"""Pure contract tests; no C/original execution, resources or GUI required."""
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

FILE=Path(__file__).resolve().parents[1]/'verify_f4_dynamic_text.py'
if not FILE.is_file():FILE=Path.cwd()/'tools/verify_f4_dynamic_text.py'
spec=importlib.util.spec_from_file_location('dynamic_tested',FILE)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)


def fragments():
 ui=[[1,-1]for _ in range(701)]
 for lo,hi in[(1,136),(150,178),(181,395),(441,476)]:
  for n in range(lo,hi+1):ui[n]=[20]*(1+(n-lo)%4)+[-1]
 for n in m.DYNAMIC:ui[n]=[20,-6,-1]if n in m.LIMITS else[20,-4,-1]
 ui[425]=[-4,21,-5,-1];ui[490]=[20,-6,-3,-1];ui[549]=[-6,-1]
 return ui


class Leaf:
 put=m.text.Original.put
 def __init__(self):self.letters=[];self.pictures=[];self.arrows=[]
 def picture(self,page,mask,portrait,phase,opening=False):
  self.pictures.append((portrait,phase,opening));self.put(page,mask,0,phase+10)
 def arrow(self,page,mask,frame,x):
  self.arrows.append((frame,x));self.put(page,mask,1,frame)
 def letter(self,page,mask,index,at,stride,fg,bg):
  self.letters.append((index,at,fg,bg));self.put(page,mask,at,index&255)


def case(fragment=488,value=1,track=0,role='progress'):
 return dict(id=0,role=role,fragment=fragment,value=value,new=181,old=182,x=95,y=119,portrait=0,track=track,seed=0,
             request=[0,fragment,value,181,182,95,119,0,track,0],line='dynamic 0\n')


class Contract(unittest.TestCase):
 def test_names_are_plain_and_request_denominator_is_thirty(self):
  ui=fragments();rows=m.requests(ui)
  self.assertEqual({r['fragment']for r in rows},m.DYNAMIC)
  self.assertEqual(len(m.DYNAMIC),30)
  self.assertEqual(len({tuple(r['request'])for r in rows}),len(rows))
  ui[181]=[-3,-1]
  with self.assertRaisesRegex(ValueError,'plain'):m.requests(ui)
 def test_length_classes_not_name_cartesian_and_two_histories(self):
  rows=m.requests(fragments());pair=[r for r in rows if r['fragment']==425]
  self.assertLessEqual(len(pair),2*4*4)
  self.assertEqual({r['track']for r in pair},{0,1})
  slots=[r for r in rows if r['fragment']==549]
  self.assertEqual([r['value']for r in slots],[0,1,2,3]);self.assertTrue(all(r['track']==0 for r in slots))
 def test_scalar_decimal_edges_and_signed_dword(self):
  self.assertEqual(m.scalar_bounds(30,True),[1,9,10,30])
  self.assertIn(100000000,m.scalar_bounds(0x7fffffff))
  self.assertEqual(m.signed_digits(0xffffffff),[65519,1])
  self.assertEqual(m.signed_digits(0x80000000),[65519,2,1,4,7,4,8,3,6,4,8])
 def test_nested_styles_are_source_fixed_not_parent_arbitrary(self):
  ui=fragments();ui[422]=[20,-4,21,-5,-1]
  oracle=Leaf();_,tokens,meta=m.source_expected(oracle,ui,b'',case(422))
  self.assertEqual(tokens,[20]+ui[181][:-1]+[21]+ui[182][:-1])
  self.assertTrue(all(t[2:]==(205,74)for t in oracle.letters));self.assertEqual(meta['ticks'],len(tokens))
 def test_queued_peek_has_no_helper_and_keeps_same_fifo(self):
  ui=fragments();ui[488]=[20,21,-1]
  _,_,meta=m.source_expected(Leaf(),ui,b'',case(track=1))
  self.assertEqual((meta['ticks'],meta['pcm'],meta['rng'],meta['takes'],meta['queued_after']),(0,0,0,0,1))
 def test_page_ack_consumes_early_key_or_injects_at_real_take(self):
  ui=fragments();ui[490]=[20,-3,-1]
  for track in(0,1):
   oracle=Leaf();_,_,meta=m.source_expected(oracle,ui,b'',case(490,1,track,'prepare'))
   self.assertEqual((meta['takes'],meta['rng'],meta['queued_after']),(1,1,0))
   self.assertEqual(meta['injected'],1-track)
   self.assertEqual(oracle.arrows,[(18,143),(13,143)])
 def test_negative_only_defined_clear_prefix_never_negative_font(self):
  ui=fragments();ui[488]=[20,21,22,23,-6,24,-1]
  for track in(0,1):
   oracle=Leaf();payload,_,meta=m.source_expected(oracle,ui,b'',case(value=0xffffffff,track=track))
   self.assertEqual([t[0]for t in oracle.letters],[20,21,22,23]);self.assertTrue(meta['source_prefix_only'])
   at=119*320+159
   self.assertTrue(all(payload[at+y*320+x]==74 for y in range(16)for x in range(16)))
   self.assertEqual(meta['ticks'],0 if track else 4)
 def test_wide_positive_uses_linear_owner_and_true_newline(self):
  ui=fragments();ui[488]=[20,21,22,23,-6,24,25,-1]
  oracle=Leaf();m.source_expected(oracle,ui,b'',case(value=0x7fffffff))
  self.assertGreater(oracle.letters[-1][1]-119*320,319)
  self.assertLess(oracle.letters[-1][1]+16*320+16,64000)
 def test_validate_rejects_any_page_palette_mask_byte_and_metadata(self):
  expected=m.text.payload(bytearray(64000),bytearray(64000));meta={'ticks':1,'source_prefix_only':False}
  header=dict(consumer_result=0,business_unchanged=True,deadline_before=0,deadline_after=0,tokens=[20],**meta)
  self.assertIsNone(m.validate_actual(header,expected,case(),[20],meta,expected))
  for at in[0,63999,64000,64768,128768]:
   bad=bytearray(expected);bad[at]^=1
   with self.assertRaisesRegex(ValueError,'owner/palette'):m.validate_actual(header,bad,case(),[20],meta,expected)
  for key,value in[('ticks',True),('deadline_after',1),('business_unchanged',False),('tokens',[21])]:
   bad={**header,key:value}
   with self.assertRaises(ValueError):m.validate_actual(bad,expected,case(),[20],meta,expected)
 def test_snapshot_first_read_cannot_be_overwritten_by_later_identity(self):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d)/'source';p.write_bytes(b'a');guards={};m.snapshot(p,guards);p.write_bytes(b'b')
   with self.assertRaisesRegex(ValueError,'first qualification'):m.snapshot(p,guards)


class Producer(unittest.TestCase):
 def test_real_identity_validator_rejects_closure_omission_and_byte_drift(self):
  with tempfile.TemporaryDirectory()as d:
   root=Path(d);files=[];sources={};executables={}
   for n in m.MINIMUM:
    p=root/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'synthetic source');sources[n]=m.sha(p.read_bytes())
   for role in('build_log','tlog','static_library','producer_source','executable'):
    for sub in(('prepare','progress')if role=='executable'else(role,)):
     p=root/sub;p.write_bytes(role.encode());st=p.stat();item=dict(path=str(p),role=role,sha256=m.sha(p.read_bytes()),size=st.st_size,mtime_ns=st.st_mtime_ns);files.append(item)
     if role=='executable':executables[sub]=dict(path=str(p),sha256=item['sha256'])
   receipt=dict(schema='fd2-f4-dynamic-producer-v1',source_root=str(root),sources=sources,files=files,executables=executables)
   path=root/'receipt.json';path.write_text(json.dumps(receipt))
   self.assertEqual(set(m.producer(path,root,{})),{'prepare','progress'})
   broken=copy.deepcopy(receipt);broken['files']=[f for f in files if f['role']!='tlog'];path.write_text(json.dumps(broken))
   with self.assertRaisesRegex(ValueError,'closure'):m.producer(path,root,{})
   path.write_text(json.dumps(receipt));(root/'prepare').write_bytes(b'changed')
   with self.assertRaisesRegex(ValueError,'identity'):m.producer(path,root,{})


class MainLoop(unittest.TestCase):
 def run_loop(self,*,fault=None,cleanup=False,late=False):
  with tempfile.TemporaryDirectory()as d:
   root=Path(d);out=root/'result';game=root/'original_game';game.mkdir()
   for n in('FDOTHER.DAT','FDTXT.DAT','DATO.DAT','START.SAV'):(game/n).write_bytes(b'synthetic')
   raw=m.text.payload(bytearray(64000),bytearray(64000));header=dict(consumer_result=0,business_unchanged=True,deadline_before=0,deadline_after=0,tokens=[20])
   if fault:header[fault]=1
   class FakeProbe:
    def __init__(self,*a,**k):pass
    def dynamic(self,c):return header,raw
    def close(self,failure):
     if late:(game/'DATO.DAT').write_bytes(b'changed after case')
     if cleanup:raise ValueError('secondary cleanup error')
   class FakeOriginal:
    glyphs={};portraits={}
    def __init__(self,*a,**k):pass
   def fake_snapshot(p,guards,expected=None):return m.text.snapshot(p,guards)
   def fake_decode(p):return b''
   args=['--root',str(root),'--out',str(out),'--output-root',str(root),'--producer-receipt',str(root/'receipt')]
   for n in('glyph-report','ui-report','remainder-report','image','source07','pixel-source','slot-source'):args+=['--'+n,str(root/n)]
   with patch.object(m,'snapshot',fake_snapshot),patch.object(m.text,'BOOT',{}),patch.object(m,'source_contract'),patch.object(m,'decode_save',fake_decode),patch.object(m.audit,'archive_entries',return_value=[b'']),patch.object(m.audit,'text_fragments',return_value=[]),patch.object(m,'Original',FakeOriginal),patch.object(m,'requests',return_value=[case()]),patch.object(m,'source_expected',return_value=(raw,[20],{})),patch.object(m,'producer',return_value={'prepare':root/'p','progress':root/'q'}),patch.object(m,'Probe',FakeProbe),patch('sys.stdout',new_callable=io.StringIO),patch('sys.stderr',new_callable=io.StringIO):
    result=m.main(args)
   report=json.loads((out/'report.json').read_text());log=[json.loads(x)for x in(out/'cases.jsonl').read_text().splitlines()]
   return result,report,log
 def test_actual_main_positive_counts_one_real_mocked_protocol_case(self):
  result,r,rows=self.run_loop();self.assertEqual(result,0);self.assertTrue(r['ok']);self.assertEqual(r['actual_C_cases'],1);self.assertEqual(len(rows),1)
 def test_actual_main_preserves_first_failure_case_and_cleanup_secondary(self):
  result,r,rows=self.run_loop(fault='deadline_after',cleanup=True)
  self.assertEqual(result,1);self.assertFalse(r['ok']);self.assertIn('deadline',r['primary_error']);self.assertIn('header',r['failed_case']);self.assertEqual(len(rows),1)
  self.assertEqual(len(r['cleanup_errors']),2);self.assertEqual(r['actual_C_cases'],0)
 def test_actual_main_late_guard_rejects_even_after_case_matched(self):
  result,r,rows=self.run_loop(late=True);self.assertEqual(result,1);self.assertFalse(r['ok']);self.assertFalse(r['guards_unchanged']);self.assertEqual(r['actual_C_cases'],1)


if __name__=='__main__':unittest.main()
