"""Finite oracle counterexamples; no original resource, C or GUI execution."""
from pathlib import Path
import json
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import verify_f4_promotion_composition as p

class Promotion(unittest.TestCase):
    def fixture(self):
        parts={'figani:0:0':dict(width=1,height=1,x=1,y=2,duration=2),
            'figani:0:1':dict(width=1,height=1,x=3,y=2,duration=0),
            'figani:96:0':dict(width=1,height=1,x=5,y=2,duration=1)}
        pm={k:(bytes([11+i]),bytes([1])) for i,k in enumerate(parts)}
        bgs=[(bytes([31+i])*32000,bytes([1])*32000) for i in range(3)]
        return parts,pm,bgs
    def test_whole_source_sequence_and_wait_order(self):
        points=list(p.history(0,32,6,*self.fixture()))
        self.assertEqual(len(points),263)
        self.assertEqual([sum(h['phase']==i for h,_ in points) for i in range(7)],
            [64,9,16,20,1,24,129])
        for i,(h,data) in enumerate(points):
            self.assertEqual(h['pumps'],i+1)
            self.assertEqual(len(data),65536+(128000 if h['phase'] else 0))
        self.assertEqual([points[i][0]['ticks'] for i in [63,64,73,88,89,108,109,110,133,134,262]],
            [0,1,1,16,17,17,17,17,40,41,41])
        self.assertEqual([points[i][0]['delays'] for i in [63,64,89,108,109,134,197,198,262]],
            [126,128,128,318,328,328,454,456,584])
        self.assertEqual(points[0][1][:64000],points[-1][1][128000:192000])
    def test_empty_new_has_no_sprite_access_after_white(self):
        points=list(p.history(0,48,3,*self.fixture()))
        self.assertEqual(len(points),109)
        self.assertEqual(points[-1][0],dict(phase=3,ticks=17,delays=318,pumps=109,work_bytes=128000))
    def test_duration_zero_stays_on_second_part(self):
        points=list(p.history(0,32,3,*self.fixture()))
        for ordinal in [73,74]:self.assertEqual(points[ordinal][1][2*640+1],11)
        for ordinal in [75,76,88]:self.assertEqual(points[ordinal][1][2*640+3],12)
    def test_background_rotates_before_drawing_and_new_frame_offset(self):
        points=list(p.history(0,32,6,*self.fixture()))
        self.assertEqual([points[i][1][50*640] for i in [64,65,66,73,74,75,109,110]],
            [32,33,31,32,33,31,31,32])
        self.assertEqual(points[109][1][2*640+205],13)
        self.assertEqual(points[110][1][2*640+5],13)
    def test_masks_preserve_skip_and_write_opaque_zero(self):
        work=bytearray([91])*128000
        p.blit(work,dict(width=2,height=1),(bytes([0,0]),bytes([1,0])),5,7)
        self.assertEqual(work[7*640+5:7*640+7],bytes([0,91]))
        with self.assertRaises(ValueError):p.blit(work,dict(width=2,height=1),(bytes(2),bytes([1,1])),639,0)
    def test_palette_exact_endpoint_not_tolerance(self):
        base=bytes([0,1,62,63])
        self.assertEqual(p.dac(base,0),bytes([0,4,251,255]))
        self.assertEqual(p.dac(base,64),bytes(4))
        self.assertEqual(p.dac(base,3,True),bytes([12,16,255,255]))
    def packet(self):
        wanted=bytes([37])*193536
        point=dict(phase=1,frame=64,ticks=1,delays=128,pumps=65,work_bytes=128000,
            payload_bytes=len(wanted),sha256=p.sha(wanted))
        row=dict(schema='fd2-f4-promotion-frame-v1',case=3,
            **{k:v for k,v in point.items() if k not in ('payload_bytes','sha256')})
        return wanted,point,row
    def test_complete_comparison(self):
        wanted,point,row=self.packet();p.compare_frame(row,3,point,wanted,wanted)
    def test_each_owner_and_offviewport_mutation_rejected(self):
        wanted,point,row=self.packet()
        for at in [127999,128000,191999,192000,192768,193535]:
            actual=bytearray(wanted);actual[at]^=1
            with self.subTest(byte=at),self.assertRaisesRegex(ValueError,f'byte{at}:'):
                p.compare_frame(row,3,point,actual,wanted)
    def test_phase_case_and_wait_reordering_rejected(self):
        wanted,point,row=self.packet()
        for key in ['case','phase','frame','ticks','delays','pumps','work_bytes']:
            actual=dict(row);actual[key]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):p.compare_frame(actual,3,point,wanted,wanted)
    def test_short_actual_or_changed_expected_rejected(self):
        wanted,point,row=self.packet()
        with self.assertRaises(ValueError):p.compare_frame(row,3,point,wanted[:-1],wanted)
        with self.assertRaises(ValueError):p.compare_frame(row,3,{**point,'sha256':p.sha(b'other')},wanted,wanted)
    def cold(self):
        nodes=[dict(id=f'promote.visual.{i}',op='promotion_visual',args=args,
            on=dict(next=f'promote.visual.{i+1}' if i<6 else 'promote.return.music',error='failure'))
            for i,args in enumerate(p.SEQ)]
        nodes.append(dict(id='promote.return.music',op='music',on=dict(next='promote.identity',error='failure')))
        return dict(flows=[dict(id='services',nodes=nodes)])
    def test_cold_join_and_broken_edges(self):
        self.assertEqual(p.cold_sequence(json.dumps(self.cold()))['sequence'],p.SEQ)
        for key,value in [('args',[1,8]),('op','music'),('on',dict(next='promote.visual.2',error='failure'))]:
            broken=self.cold();broken['flows'][0]['nodes'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):p.cold_sequence(json.dumps(broken))
    def test_cli_fixed_names_and_directory(self):
        root=Path(__file__).resolve().parent
        self.assertEqual(p.resource_directory(root/'BG.DAT',root/'FIGANI.DAT'),root)
        for bg,fig in [(root/'copy.DAT',root/'FIGANI.DAT'),(root/'BG.DAT',root/'copy.DAT'),
            (root/'BG.DAT',root/'other'/'FIGANI.DAT')]:
            with self.subTest(bg=bg,fig=fig),self.assertRaises(ValueError):p.resource_directory(bg,fig)

if __name__=='__main__':unittest.main()
