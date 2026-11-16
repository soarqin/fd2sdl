"""Synthetic protocol/source arithmetic only; no C or resource decode."""
from pathlib import Path
import sys,unittest
sys.path[:0]=[str(Path(__file__).resolve().parents[1]),str(Path.cwd()/'tools')]
import verify_f4_full_magic_history as f

class Oracle:
    def put(self,page,mask,at,color):
        if not 0<=at<len(page):raise f.b.OwnerFailure(at)
        page[at]=color&255;mask[at]=1
    def blit(self,page,mask,frame,at,stride):self.put(page,mask,at,frame)
    def raw_bar(self,page,mask,frame,at,stride):
        for y in range(5):self.put(page,mask,at+y*stride,frame)
    def literal(self,page,mask,tokens,at,stride):self.put(page,mask,at,205)

def fixture(v):
    parts={};clips={};images={}
    for entry in (0,27,28,90,98,288):
        rows=[dict(x=0,y=0,width=1,height=1,duration=2,marker=int(entry==28 and i==6),cue=0,layer=0) for i in range(11 if entry==28 else 4)]
        clips[entry]=dict(entry=entry,count=len(rows),intro=len(rows),parts=rows)
        for i,p in enumerate(rows):key=f'figani:{entry}:{i}';parts[key]=dict(p,entry=entry);images[key]=(bytes([11+i]),b'\1')
    im=(dict(width=2,height=1),(b'\x14\0',b'\1\1'));bgs={i:im for i in range(5)}
    durations=[2,2,2,2,2,3,3,3,7,2,2,2,2,2,2,1,1,1]
    particles={i:[(dict(x=0,y=0,width=1,height=1,duration=durations[n] if n<len(durations) else 1),(bytes([100+n]),b'\1')) for n in range(64)] for i in range(10)}
    tables=dict(friendly_entries=list(range(9)),hostile_entries=list(range(10)),rain0_front=[1,0,1,0,1,0,1],rain0_x=[0]*7,rain0_y=[0]*7,
        rain1_x=[0]*8,rain1_y=[0]*8,shower_x=[0]*12,shower_y=[0]*12,shower_parts=[0]*12,dust4_x=[0]*10,
        surge_x=[0]*10,burst_parts=[0]*16,hp_denominators=[7,8,6,13,6,6,5,5,16,20],
        magic_shake_x=[6,4,2,0],magic_shake_y=[-3,-2,-1,0])
    m=f.Full(v,dict(parts=parts,clips=clips),images,bgs,im,Oracle(),[-1],bytes([7])*768,6,particles,tables)
    m.luts={i:bytes(255-j for j in range(256)) for i in range(11,23)};m.return_base=19 if v[0]==8 else 15 if v[0]>3 else 11
    m.hit_base=32 if v[0]<4 else 176;m.denominator=tables['hp_denominators'][v[0]];m.cast_colors=(1,2,3);return m

class FullHistoryProtocol(unittest.TestCase):
    def test_full_shake_uses_distinct_source_tables_and_retained_pixel_counterexample(self):
        m=fixture([0,2,0,2]);self.assertEqual((m.shake_x[3],m.shake_y[3]),(0,0))
        self.assertEqual((m.shake_x[0],m.shake_y[0]),(6,-3))
        meta=m.parts['parts']['figani:0:0'];meta.update(x=167,y=75,width=2,height=1)
        m.images['figani:0:0']=(b'\1\xfe',b'\0\1');m.work[67528]=129
        m.idle_draw(0,19360,-m.shake_x[3],m.shake_y[3],32+(8<<8))
        self.assertEqual(m.work[67528],38)  # original opaque254 ->32+((8+254)&7).
        m.work[67528]=129;m.fig(m.work,19360,640,0,0,-14,6,32+(8<<8))
        self.assertEqual(m.work[67528],129)  # old physical shift lands row6 lower.
    def test_fixed_six_not_unknown_orbit_or_multitarget_beam(self):
        self.assertEqual(len(f.requests()),6)
        for v in f.requests():self.assertEqual(f.validate(v),v)
        for v in ([6,2,0,2],[9,0,1,2],[0,0,0,2],[False,2,0,2]):
            with self.assertRaises(ValueError):f.validate(v)
    def test_complete_six_source_frame_counts_and_retired_final(self):
        for v,wanted in zip(f.requests(),[299,306,277,325,359,353]):
            m=fixture(v);points=list(m.run());last,data=points[-1]
            self.assertEqual(last['frame'],wanted,v);self.assertEqual(len(data),455552)
            self.assertFalse(last['owner_live']);self.assertEqual((last['stops'],last['returns']),(1,1))
            self.assertTrue(points[-2][0]['owner_live']);self.assertEqual(len(m.segments),v[3]+2)
    def test_explicit_tape_segments_and_scope_no_game_rng(self):
        m=fixture([4,2,0,2]);list(m.run())
        self.assertEqual(len(m.segments[0]['random']),6)
        self.assertEqual([len(s['jitter']) for s in m.segments],[0,6,6,0])
        lines=list(f.lines(dict(request=[4,2,0,2],segments=m.segments)))
        self.assertEqual(len(lines),5);self.assertTrue(lines[1].startswith('0 6 0 '))
        self.assertTrue(all(0<=x<=65535 for s in m.segments for x in s['random']))
    def test_burst_later_targets_keep_before_hp_without_fake_interpolation(self):
        m=fixture([8,2,0,3]);points=list(m.run());targets=[s for s,_ in points if s['kind']=='target_end']
        self.assertEqual([s['active_hp'] for s in targets],[28,42,42])
        self.assertEqual([len(s['jitter']) for s in m.segments],[0,16,0,0,0])
    def test_shade_is_source_lowbyte_plus_phase_pixel_not_constant_fill(self):
        m=fixture([0,2,0,2]);m.images['figani:0:0']=(b'\0',b'\1');m.fig(m.work,19360,640,0,0,tint=32+(8<<8))
        self.assertEqual(m.work[19360],32)
        m.fig(m.work,19360,640,0,0,tint=32+(7<<8));self.assertEqual(m.work[19360],39)
    def test_complete_packet_keys_and_final_retirement_flag(self):
        state,data=list(fixture([0,2,0,2]).run())[-1];point=dict(state,sha256=f.sha(data))
        row=dict(schema='fd2-f4-battle-history-state-v1',case=0,**{k:state[k] for k in f.KEYS})
        f.b.compare(row,0,point,data,data,f.PAYLOAD,f.KEYS);row['owner_live']=True
        with self.assertRaises(ValueError):f.b.compare(row,0,point,data,data,f.PAYLOAD,f.KEYS)

if __name__=='__main__':unittest.main()
