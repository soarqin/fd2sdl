"""Synthetic arithmetic/protocol checks only; no resource/C/original execution."""
import tempfile
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path[:0]=[str(Path(__file__).resolve().parents[1]),str(Path.cwd()/'tools')]
import verify_f4_battle_history as b

class FixtureOracle:
    def put(self,page,mask,at,color):
        if not 0<=at<len(page):raise b.OwnerFailure(at)
        page[at]=color&255;mask[at]=1
    def blit(self,page,mask,frame,at,stride):self.put(page,mask,at,frame)
    def raw_bar(self,page,mask,frame,at,stride):
        for y in range(5):self.put(page,mask,at+y*stride,frame)
    def literal(self,page,mask,tokens,at,stride):self.put(page,mask,at,205)

def fixture():
    clips={};parts={};images={}
    for entry in {0,288,*[v*3 for v in b.CASTERS.values()],*[v*3+2 for v in b.CASTERS.values()]}:
        casting=entry%3==2
        ps=[dict(width=1,height=1,x=0,y=0,duration=1,marker=1 if casting else 0,cue=2 if casting else 0),
            dict(width=1,height=1,x=0,y=0,duration=2 if casting else 3,marker=1 if casting else 0,cue=3 if casting else 0)]
        clips[entry]=dict(entry=entry,count=2,intro=1 if casting else 0,parts=ps)
        for i,p in enumerate(ps):
            key=f'figani:{entry}:{i}';parts[key]=p;images[key]=(bytes([11+i]),bytes([1]))
    img=(dict(width=2,height=1),(bytes([20,0]),bytes([1,1])))
    return dict(parts=parts,clips=clips),images,{i:img for i in range(5)},img,FixtureOracle(),[-1],bytes([7])*768,6

class BattleHistoryProtocol(unittest.TestCase):
    def model(self,v):return b.Derived(v,*fixture())
    def test_finite_twenty_normal_plus_three_controls(self):
        requests=b.requests();self.assertEqual(len(requests),23)
        self.assertEqual(len(set(map(tuple,requests))),23)
        for v in ([25,0,0,0],[28,1,0,0],[28,0,2,0],[24,0,0,1],[31,2,0,2],[24,0,0,3],[True,0,0,0]):
            with self.assertRaises(ValueError):b.validate(v)
    def test_complete_cast_two_targets_and_marker_boundaries(self):
        states=list(self.model([24,0,0,0]).run());last=states[-1][0]
        self.assertEqual(last['frame'],116);self.assertEqual(last['pumps'],116)
        self.assertEqual(last['ticks'],5);self.assertEqual(last['delays'],128)
        self.assertEqual(last['caster_mp'],993);self.assertEqual(last['target'],2)
        self.assertEqual([p['active_hp'] for p,_ in states if p['kind']=='part_end'],[0,0])
        self.assertEqual([p['target'] for p,_ in states if p['kind']=='target_end'],[1,2])
        self.assertTrue(all(len(raw)==b.PAYLOAD for _,raw in states))
    def test_pan_directions_are_nineteen_and_twenty_uploads(self):
        left=list(self.model([24,2,0,0]).run());right=list(self.model([24,0,0,0]).run())
        self.assertEqual(left[-1][0]['frame']-right[-1][0]['frame'],2)
        no_pan=list(self.model([28,0,0,0]).run())
        self.assertEqual(right[-1][0]['frame']-no_pan[-1][0]['frame'],38)
    def test_source_marker_precedes_first_effect_duration_and_tint_only_first_tick(self):
        states=list(self.model([28,0,0,0]).run())
        points=[p for p,_ in states if p['kind']=='upload' and p['phase']==1]
        self.assertEqual([(p['active_hp'],p['markers'],p['tint'],p['shake']) for p in points[:2]],[(0,1,33,5),(0,1,-1,4)])
        self.assertEqual([(p['idle_part'],p['idle_ticks']) for p in points[:2]],[(1,0),(1,1)])
    def test_id31_suppresses_only_intro_cue_not_effect_cue(self):
        a=list(self.model([24,0,0,0]).run());c=list(self.model([31,0,0,0]).run())
        self.assertEqual(a[-1][0]['cues'],3);self.assertEqual(c[-1][0]['cues'],2)
        self.assertEqual(c[-1][0]['last_cue'],3)
    def test_skip_has_no_upload_clock_cue_or_rng_but_complete_marker_progress(self):
        states=list(self.model([28,0,1,1]).run());last=states[-1][0]
        self.assertFalse(any(p['kind']=='upload' for p,_ in states))
        self.assertEqual([last[k] for k in ['frame','pumps','ticks','delays','cues']],[0]*5)
        self.assertEqual(last['caster_mp'],993);self.assertEqual(last['target'],2)
    def test_zero_duration_is_synthetic_model_only_and_keeps_marker(self):
        args=fixture();args[0]['clips'][59]['parts'][1]['duration']=0
        states=list(b.Derived([28,0,0,0],*args).run());last=states[-1][0]
        self.assertEqual(last['frame'],74);self.assertEqual(last['markers'],1)
        self.assertEqual(last['ticks'],1)
    def test_owner_failure_keeps_board_prefix_no_duration_or_next_target(self):
        states=list(self.model([24,0,0,2]).run());last=states[-1][0]
        self.assertEqual(last['kind'],'final');self.assertEqual(last['status'],-1)
        self.assertTrue(last['failed']);self.assertEqual(last['target'],0)
        self.assertEqual(last['next_part'],1);self.assertEqual(last['active_hp'],32767)
        self.assertEqual(states[-2][0]['kind'],'part_error')
    def test_linear_minus_one_and_opaque_zero_not_rectangle_clip(self):
        work=bytearray([77])*128000;meta=dict(width=2,height=1,x=0,y=0)
        b.draw(work,640,640,meta,(bytes([0,9]),bytes([1,0])),-1)
        self.assertEqual(work[639:641],bytes([0,77]))
        with self.assertRaises(b.OwnerFailure):b.draw(work,127999,640,meta,(bytes([5,6]),bytes([1,1])))
        self.assertEqual(work[-1],5)
    def test_compare_requires_all_owner_bytes_and_scalar_fields(self):
        point,wanted=next(self.model([24,0,0,0]).run());point={**point,'sha256':b.sha(wanted)}
        row=dict(schema='fd2-f4-battle-history-state-v1',case=0,**{k:point[k] for k in b.KEYS})
        b.compare(row,0,point,wanted,wanted)
        for changed in ({**row,'cues':1},{**row,'extra':1}):
            with self.assertRaises(ValueError):b.compare(changed,0,point,wanted,wanted)
        with self.assertRaises(ValueError):b.compare(row,0,point,wanted[:-1],wanted)
    def test_single_freeze_wrong_pin_and_late_change(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'input';p.write_bytes(b'abc');facts=[]
            self.assertEqual(b.freeze(p,facts,b.sha(b'abc')),b'abc')
            with self.assertRaises(ValueError):b.freeze(p,[],b.sha(b'bad'))
            p.write_bytes(b'next');self.assertFalse(b.guards(facts)['unchanged'])
    def test_reporting_secondary_error_cannot_replace_first(self):
        result=dict(passed=False,primary_error='first pixel',secondary_errors=[])
        with patch.object(Path,'write_text',side_effect=OSError('late report')):b.core.save_report(Path('unused'),result)
        self.assertEqual(result['primary_error'],'first pixel');self.assertEqual(result['secondary_errors'][0]['operation'],'report.write')

if __name__=='__main__':unittest.main()
