"""Finite history/protocol negatives only; no resource, C or original execution."""
import tempfile
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import verify_f4_map_item_history as m

class MapHistoryProtocol(unittest.TestCase):
    @staticmethod
    def fixture():
        tables={'start':[49]*33,'count':[1]*33,'cue':[0]*33,'tint':[32]*30}
        pictures={('lmi',n):(2,2,bytes([0,73+n%20,82,91]),bytes([0,1,1,1])) for n in (49,74,75)}
        for unit in m.IDS:
            for phase in range(3):
                pixels=bytes([0,11+unit%30+phase,32,90])*144
                pictures[('icon',unit*12+phase)]=(24,24,pixels,bytes([1,1,0,1])*144)
        return tables,pictures
    def test_request_domain_and_exact_three_overrides(self):
        requests=m.requests();self.assertLessEqual(len(requests),128)
        self.assertEqual(len({tuple(x) for x in requests}),len(requests))
        self.assertEqual(m.style(4,20),'ramp');self.assertEqual(m.style(0,21),'duotone')
        self.assertEqual(m.style(11,24),'ramp')
        for v in ([23,0,0,0,0,0],[33,0,0,0,0,0],[4,21,0,0,0,0],[0,0,4,0,0,0],
                  [0,0,0,5,0,0],[0,0,0,0,2,0],[0,0,0,0,0,5],[True,0,0,0,0,0]):
            with self.assertRaises(ValueError):m.validate_request(v)
    def test_actual_opaque_zero_is_not_skip(self):
        page=bytearray([73])*m.P
        m.image_draw(page,(2,1,bytes([0,0]),bytes([1,0])),72,66,lambda value:200)
        self.assertEqual(page[66*456+72:66*456+74],bytes([200,73]))
    def test_entire_requested_image_is_prechecked(self):
        page=bytearray([73])*m.P;before=bytes(page)
        with self.assertRaises(ValueError):m.image_draw(page,(2,1,b'12',bytes([1,1])),455,0)
        self.assertEqual(bytes(page),before)
    def test_ramp_old_vga_tail_retains_current_app(self):
        tables,pictures=self.fixture();states=list(m.history([0,0,3,0,0,0],tables,pictures))
        self.assertEqual(len(states),14)
        point,raw=states[-1];self.assertTrue(point['final']);self.assertEqual(point['ticks'],11)
        app=raw[:m.P];vga=bytearray(raw[m.P:m.P+m.V]);projected=bytearray(vga)
        m.project(app,projected)
        self.assertNotEqual(projected,vga)
        self.assertEqual(states[-2][0]['label'],'RAMP old-VGA tail; App retains last tint')
    def test_idle_three_maps_two_and_fixed_actor_ignores_orientation(self):
        tables,pictures=self.fixture()
        a=list(m.history([0,0,2,0,0,0],tables,pictures))
        b=list(m.history([0,0,3,0,0,0],tables,pictures))
        self.assertEqual([raw for _,raw in a],[raw for _,raw in b])
    def test_duotone_delay_and_fresh_terrain_two_page_history(self):
        tables,pictures=self.fixture();states=list(m.history([4,0,0,0,0,0],tables,pictures))
        self.assertEqual(len(states),12);self.assertEqual(states[-1][0]['delays'],720)
        self.assertEqual(states[-1][0]['refreshes'],3)
        phase=[raw[m.P:m.P+m.V] for point,raw in states if point['label']=='DUOTONE two terrain owners']
        self.assertEqual(len(phase),8);self.assertEqual(phase[0],phase[2]);self.assertNotEqual(phase[0],phase[1])
    def test_solid_uses_200_and_source_snapshot_alternation(self):
        tables,pictures=self.fixture();states=list(m.history([13,0,0,0,0,0],tables,pictures))
        p=states[-1][0];self.assertEqual(p['cues'],1);self.assertEqual(p['last_cue'],1)
        self.assertEqual(p['ticks'],11);self.assertEqual(p['delays'],0)
        frames=[raw[m.P:m.P+m.V] for point,raw in states if point['label']=='SOLID old/200 snapshot alternation']
        self.assertEqual(len(frames),11);self.assertEqual(frames[0],frames[10]);self.assertNotEqual(frames[0],frames[1])
    def test_host_first_upload_has_no_extra_wait_or_refresh(self):
        tables,pictures=self.fixture();states=list(m.history([0,0,3,3,1,1],tables,pictures))
        self.assertEqual(len(states),2);p=states[-1][0]
        self.assertEqual([p[k] for k in ['frame','ticks','delays','pumps','refreshes']],[1,0,0,1,1])
    def test_preexisting_skip_and_host_are_zero_upload(self):
        tables,pictures=self.fixture()
        for stop in (2,3):
            states=list(m.history([4,0,3,3,1,stop],tables,pictures));self.assertEqual(len(states),1)
            p=states[0][0];self.assertEqual([p[k] for k in ['frame','ticks','delays','pumps','refreshes']],[0]*5)
    def test_host_first_actor_upload_preserves_three_distinct_prefixes(self):
        tables,pictures=self.fixture()
        for id in (0,4,13):
            states=list(m.history([id,0,3,3,1,4],tables,pictures));point=states[-1][0]
            self.assertEqual([point[k] for k in ['frame','ticks','delays','pumps','refreshes']],[3,1,0,3,2])
            self.assertTrue(point['label'].startswith('HOST at first'))
    def test_hidden_direct_target_and_duotone_batch_have_observable_distinct_writes(self):
        tables,pictures=self.fixture()
        ramp=next(raw for p,raw in m.history([0,0,3,4,0,0],tables,pictures) if p['label']=='RAMP fixed-dir0 source tick')
        duo=next(raw for p,raw in m.history([4,0,3,4,0,0],tables,pictures) if p['label']=='DUOTONE two terrain owners')
        at=(66+4*24)*456+72+8*24
        self.assertEqual(ramp[at],39)
        self.assertEqual(duo[at],40+((8*24)*7+(4*24-6)*11+2*3)%173)
        self.assertNotEqual(ramp[at],duo[at])
    def test_actual_header_and_complete_payload_are_exact(self):
        tables,pictures=self.fixture();point,wanted=next(m.history([0,0,0,0,0,0],tables,pictures))
        row=dict(schema='fd2-f4-map-history-state-v1',case=0,
            **{k:point[k] for k in ['final','frame','ticks','delays','cues','last_cue','pumps','refreshes','payload_bytes']})
        point={**point,'sha256':m.sha(wanted)};m.compare_state(row,0,point,wanted,wanted)
        for wrong in ({**row,'refreshes':2},{**row,'unexpected':True}):
            with self.assertRaises(ValueError):m.compare_state(wrong,0,point,wanted,wanted)
        with self.assertRaises(ValueError):m.compare_state(row,0,point,wanted[:-1],wanted)
    def test_frozen_bytes_pin_and_late_guard(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'input';p.write_bytes(b'abc');facts=[]
            self.assertEqual(m.freeze(p,facts,m.sha(b'abc')),b'abc')
            with self.assertRaises(ValueError):m.freeze(p,[],m.sha(b'wrong'))
            p.write_bytes(b'changed');self.assertFalse(m.guards(facts)['unchanged'])
    def test_original_resolved_output_and_primary_report_error(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'original_game').mkdir();(root/'build/f4').mkdir(parents=True)
            with self.assertRaises(ValueError):m.core.fresh(root,root/'original_game/new')
            result=dict(passed=False,primary_error='first mismatch',secondary_errors=[])
            with patch.object(Path,'write_text',side_effect=OSError('late report')):
                m.core.save_report(root/'report.json',result)
            self.assertEqual(result['primary_error'],'first mismatch')
            self.assertEqual(result['secondary_errors'][0]['operation'],'report.write')

if __name__=='__main__':unittest.main()
