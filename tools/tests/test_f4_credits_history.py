import io
import importlib.util
import json
from pathlib import Path
import queue
import struct
import sys
import unittest
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

HERE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(HERE))
ROOT=Path.cwd()
sys.path.insert(1,str(ROOT/'tools'))
import verify_f4_credits_history as credits

def archive(block):return b'LLLLLL'+struct.pack('<I',10)+block

class FiniteOriginal:
    resources={'FDOTHER.DAT':archive(bytes([63])*768)}
    base=bytes(64000)
    def literal(self,*args):pass
    def raw_border(self,*args):pass
    def clip(self,entry):
        return {'intro_end':1,'parts':[{'x':0,'y':0,'width':1,'height':1,'mask':b'\1','pixels':bytes([entry]),'ticks':3}]}
    def portrait(self,actor,frame):return bytes([40+actor+frame])*6400

def all_events(case):
    texts=[[(-1,)]*46 for _ in range(32)];texts[0]=[(-1,)]*180
    image=bytearray(0x620a1)
    for actor in range(3):image[0x61da1+actor*24+1]=1
    history=credits.History(FiniteOriginal(),texts,bytes(image),case)
    return history,list(history.events())

def input_bytes(name,raw):
    return json.dumps({'schema':'fd2-f4-input-v1','input_name':name,'payload_bytes':len(raw)}).encode()+b'\n'+raw

def output_bytes():
    return json.dumps({'schema':'fd2-f4-reader-v1','status':0,'payload_bytes':1}).encode()+b'\nX'

class TestSourceHistory(unittest.TestCase):
    def test_source_outer_assignment_does_not_break_current_card(self):
        self.assertEqual(credits.actor_path(0),([2,1,0],[]))
        self.assertEqual(credits.actor_path(1),([2,0,0],[(0,17),(1,218)]))
        for bad in(-1,2,True,'1'):
            with self.assertRaises(ValueError):credits.actor_path(bad)

    def test_roll_darkness_is_post_copy_update(self):
        actual=list(credits.roll_darkness())
        self.assertEqual(len(actual),500)
        self.assertEqual([actual[i]for i in(0,1,195,196,200,300,305,306,495,496,499)],
                         [40,39,1,0,0,0,0,1,38,39,39])

    def test_blink_is_single_source_epoch_across_actors(self):
        history,events=all_events(0)
        cards=[c for c,_ in events if c['stage']=='card']
        self.assertEqual(len(cards),880)
        self.assertEqual([cards[i]['blink']for i in(0,38,39,40,41,219,220,439,440,879)],
                         [40,2,1,0,40,26,25,11,10,22])
        self.assertEqual(history.rng,22)
        self.assertEqual([cards[i]['outer']for i in(0,219,220,439,440,879)],[2,2,1,1,0,0])
        self.assertEqual(cards[220]['idle_ticks'],0) # 20 prefix draws plus card1, duration3.
        self.assertEqual(sum(c['stage']=='roll'for c,_ in events),500)
        self.assertEqual(sum(c['stage']=='fade'for c,_ in events),192)
        self.assertEqual(events[-1][0]['request'][1],'done')
        self.assertEqual(events[-1][0]['request'][3],len(events)-1)

    def test_keyed_last_card_repeats_through_original_outer_loop(self):
        history,events=all_events(1)
        cards=[c for c,_ in events if c['stage']=='card']
        self.assertEqual(len(cards),1100)
        self.assertEqual(history.rng,27)
        self.assertEqual(history.keys,2)
        self.assertEqual([cards[i]['outer']for i in(0,219,220,659,660,1099)],[2,2,0,0,0,0])
        self.assertEqual([cards[i]['actor_visits']for i in(219,220,659,660)],[1,2,2,3])
        self.assertEqual(cards[660]['blink'],36)
        self.assertEqual(cards[17]['keys'],0)
        self.assertEqual(cards[18]['keys'],1)
        self.assertEqual(cards[220+218]['keys'],1)
        self.assertEqual(cards[220+219]['keys'],2)

    def test_draw_preserves_defined_prefix_at_first_owner_failure(self):
        page=bytearray([7]*4)
        part={'x':0,'y':0,'width':2,'mask':b'\1\1','pixels':b'\x11\x12'}
        with self.assertRaisesRegex(ValueError,'destination owner'):credits.draw(page,part,x=3,stride=4)
        self.assertEqual(page,bytearray([7,7,7,17]))

    def test_palette_uses_base_not_previous_dac(self):
        self.assertEqual(credits.darkness(bytes([63,40,0])*256,40),bytes([93,0,0])*256)
        with self.assertRaises(ValueError):credits.darkness(bytes([64])*768,0)

class TestDeferredProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec=importlib.util.spec_from_file_location('credits_candidate_core',HERE/'verify_f4_resource_pixels.py')
        cls.core=importlib.util.module_from_spec(spec);spec.loader.exec_module(cls.core)

    def read(self,data,deferred=frozenset({'TAI.DAT'})):
        probe=self.core.Probe.__new__(self.core.Probe)
        probe.expected_inputs={'FDOTHER.DAT':b'A','TAI.DAT':b'B'};probe.deferred_inputs=deferred
        probe.bound_inputs={};probe.replies=queue.Queue();probe.max_page_bytes=1
        probe.process=type('Process',(),{'stdout':io.BytesIO(data)})()
        probe._read();return probe

    def test_fixed_cwd_credits_cli_does_not_accept_a_fake_resource_root_argument(self):
        class Process:
            def __init__(self):self.stdin=io.BytesIO();self.stdout=io.BytesIO()
            def poll(self):return 0
            def wait(self,**unused):return 0
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory)
            thread=SimpleNamespace(start=lambda:None,join=lambda **unused:None,is_alive=lambda:False)
            for mode in('--f4-readers','--f4-credits-cold'):
                with patch.object(self.core.subprocess,'Popen',return_value=Process())as launch,patch.object(self.core.threading,'Thread',return_value=thread):
                    probe=self.core.Probe(Path('probe.exe'),out,out,mode[2:],1,{},mode=mode)
                    argv=launch.call_args.args[0]
                    self.assertEqual(argv,['probe.exe',mode]+([]if mode.endswith('credits-cold')else[str(out/'original_game')]))
                    self.assertEqual(launch.call_args.kwargs['cwd'],out)
                    probe.close(True)

    def test_real_post_roll_input_can_bind_without_a_second_start(self):
        probe=self.read(input_bytes('FDOTHER.DAT',b'A')+output_bytes()+input_bytes('TAI.DAT',b'B')+output_bytes())
        values=[]
        while not probe.replies.empty():values.append(probe.replies.get_nowait())
        self.assertEqual(values[0][0],'inputs_bound')
        self.assertEqual(len(values),4)
        self.assertEqual(values[1][1],b'X');self.assertEqual(values[2][1],b'X')
        self.assertIsInstance(values[3],EOFError)
        self.assertEqual(set(probe.bound_inputs),{'FDOTHER.DAT','TAI.DAT'})
        self.assertIn('deferred',probe.bound_inputs['TAI.DAT']['binding'])
        self.assertEqual(probe.bound_inputs['TAI.DAT']['outputs_before_handshake'],1)

    def test_default_reader_still_rejects_output_before_all_inputs(self):
        probe=self.read(input_bytes('FDOTHER.DAT',b'A')+output_bytes(),frozenset())
        error=probe.replies.get_nowait()
        self.assertIsInstance(error,ValueError)
        self.assertIn('before all actual input',str(error))

    def test_consumer_rejects_tai_loaded_one_frame_early(self):
        case=all_events(0)[1][500][0]
        header={**case,'history_mask_measured':False,'palette_semantics':'actualDAC',
            'flow':'stage.cinematic.card_first','node':'intro8'}
        cold={'stage.cinematic.card_first':{'intro8':{'op':'palette_subtract','args':[0,255,48]}}}
        for count in(0,499,501):
            probe=type('Probe',(),{'bound_inputs':{'TAI.DAT':{'outputs_before_handshake':count}}})()
            with self.assertRaisesRegex(ValueError,'exactly after ROLL500'):credits.check_header(header,case,probe,cold)
        probe=type('Probe',(),{'bound_inputs':{'TAI.DAT':{'outputs_before_handshake':500}}})()
        credits.check_header(header,case,probe,cold)

    def test_wrong_or_duplicate_late_archive_never_binds(self):
        for suffix in(input_bytes('TAI.DAT',b'C'),input_bytes('TAI.DAT',b'B')+input_bytes('TAI.DAT',b'B')):
            probe=self.read(input_bytes('FDOTHER.DAT',b'A')+output_bytes()+suffix)
            values=[]
            while not probe.replies.empty():values.append(probe.replies.get_nowait())
            self.assertTrue(any(isinstance(x,ValueError)for x in values))

    def test_non_deferred_missing_input_blocks_even_when_tai_is_allowed_late(self):
        probe=self.read(output_bytes())
        self.assertIsInstance(probe.replies.get_nowait(),ValueError)

    def test_late_binding_is_required_at_successful_exit(self):
        probe=self.core.Probe.__new__(self.core.Probe)
        probe.expected_inputs={'TAI.DAT':b'B'};probe.bound_inputs={};probe.replies=queue.Queue()
        probe.replies.put(EOFError('end'))
        probe.thread=type('Thread',(),{'join':lambda *a,**k:None,'is_alive':lambda *a:False})()
        probe.stderr=io.BytesIO()
        probe.process=type('Process',(),{'stdin':io.BytesIO(),'stdout':io.BytesIO(),
            'wait':lambda *a,**k:0,'poll':lambda *a:0})()
        with self.assertRaisesRegex(ValueError,'deferred actual input identity missing'):probe.close(False)

if __name__=='__main__':unittest.main()
