"""Finite synthetic protocol/order arithmetic; no resource/C/original execution."""
from pathlib import Path
import sys,unittest
sys.path[:0]=[str(Path(__file__).resolve().parents[1]),str(Path.cwd()/'tools')]
import verify_f4_composite_history as c

class Oracle:
    def put(self,page,mask,at,color):
        if not 0<=at<len(page):raise c.b.OwnerFailure(at)
        page[at]=color;mask[at]=1
    def blit(self,page,mask,frame,at,stride):self.put(page,mask,at,frame)
    def raw_bar(self,page,mask,frame,at,stride):
        for y in range(5):self.put(page,mask,at+y*stride,frame)
    def literal(self,page,mask,tokens,at,stride):self.put(page,mask,at,205)

def model(v):
    parts={};clips={};images={}
    for entry in (0,27,28,98,288):
        rows=[dict(width=1,height=1,x=0,y=0,duration=1,marker=1 if entry==28 else 0,cue=4,layer=0),
            dict(width=1,height=1,x=0,y=0,duration=2,marker=0,cue=5,layer=0)]
        clips[entry]=dict(entry=entry,count=2,intro=2,parts=rows)
        for i,p in enumerate(rows):
            key=f'figani:{entry}:{i}';parts[key]=dict(p,entry=entry);images[key]=(bytes([11+i]),b'\1')
    picture=(dict(width=2,height=1),(b'\x14\0',b'\1\1'));bgs={i:picture for i in range(5)}
    particles={entry:[(dict(x=2,y=0,width=1,height=1),(bytes([100+i]),b'\1')) for i in range(8)] for entry in range(65,69)}
    return c.Composite(v,dict(parts=parts,clips=clips),images,bgs,picture,Oracle(),[-1],bytes([7])*768,6,particles,
        {15:bytes(255-i for i in range(256)),19:bytes(255-i for i in range(256))},
        [(63,63,63),(51,57,63),(53,0,0),(53,58,9)],{id:(1,2,3) for id in range(32,36)})

class CompositeProtocol(unittest.TestCase):
    def test_control_selection_preserves_verified_payload_and_original_index(self):
        cases=[dict(case=i,request=v,payload=f'case-{i:02d}.gz',states=[i]) for i,v in enumerate(c.requests())]
        selected=c.b.selected_cases(cases,[16,17,18])
        self.assertEqual([r['case'] for r in selected],[0,1,2])
        self.assertEqual([r['original_case'] for r in selected],[16,17,18])
        self.assertEqual([r['payload'] for r in selected],['case-16.gz','case-17.gz','case-18.gz'])
        self.assertEqual(cases[16]['case'],16);self.assertIs(selected[0]['states'],cases[16]['states'])
        self.assertEqual([r['original_case'] for r in c.b.selected_cases(cases)],list(range(19)))
    def test_bad_selection_rejected_without_producer(self):
        for selection in ([],[16,16],[-1],[19],[True]):
            with self.assertRaises(ValueError):c.b.selected_cases([{}]*19,selection)
    def test_sixteen_parameters_plus_three_controls(self):
        self.assertEqual(len(c.requests()),19)
        for v in c.requests():self.assertEqual(c.validate(v),v)
        for v in ([31,0,0,0],[34,1,0,0],[32,0,1,2],[34,2,2,0],[True,2,0,0]):
            with self.assertRaises(ValueError):c.validate(v)
    def test_complete_timeline_and_stop_after_return_fade(self):
        points=list(model([34,2,0,0]).run());last,data=points[-1]
        self.assertEqual((last['frame'],last['ticks'],last['delays'],last['cues'],last['caster_mp']),(144,40,464,2,993))
        self.assertEqual((last['returns'],last['stops'],last['cast_bank_open'],last['map_bank_open']),(1,1,False,True))
        self.assertEqual(len(data),455552);self.assertEqual(points[-2][0]['stops'],0)
    def test_secondary_cue_and_eleven_alternating_two_uploads(self):
        points=list(model([32,0,1,0]).run());last=points[-1][0]
        self.assertEqual((last['frame'],last['ticks'],last['secondary_cues'],last['cues']),(166,62,1,8))
        self.assertEqual(last['last_cue'],1)
    def test_signed_chop_palette_and_255_preservation(self):
        m=model([34,0,0,0]);m.dac=bytes([211])*768;m.palette(1)
        self.assertEqual(m.dac[:3],bytes([(52<<2)|(52>>4),0,0]));self.assertEqual(m.dac[765:],bytes([211])*3)
        self.assertEqual(c.chop(-39,40),0);self.assertEqual(c.chop(-41,40),-1)
    def test_skip_retains_given_app_and_no_clock_or_upload(self):
        m=model([34,2,1,1]);before=bytes(m.app);points=list(m.run());last=points[-1][0]
        self.assertEqual((last['frame'],last['ticks'],last['delays'],last['cues'],last['stops'],last['returns']),(0,0,0,0,0,0))
        self.assertTrue(last['cast_bank_open']);self.assertEqual(last['caster_mp'],993);self.assertEqual(bytes(m.app),before)
    def test_return_adapter_error_keeps_declared_prefix_and_stops(self):
        m=model([34,2,1,2]);points=list(m.run());last,data=points[-1]
        self.assertEqual((last['status'],last['failed'],last['returns'],last['stops'],last['frame']),(-1,True,1,0,103))
        self.assertTrue(last['cast_bank_open']);self.assertEqual(m.app[72*456+72],239);self.assertEqual(m.vga,bytes(64000))
    def test_host_after_return_preserves_return_prefix_no_bank_stop(self):
        m=model([34,2,1,3]);last,_=list(m.run())[-1]
        self.assertEqual((last['status'],last['host'],last['failed'],last['frame'],last['stops']),(0,True,False,103,0))
        self.assertEqual(m.vga[4*320+4],49);self.assertEqual(m.vga[:320],bytes(320))
    def test_complete_packet_schema_not_checksum_only(self):
        state,data=list(model([34,0,0,0]).run())[-1];point=dict(state,sha256=c.sha(data))
        row=dict(schema='fd2-f4-battle-history-state-v1',case=0,**{k:state[k] for k in c.KEYS})
        c.b.compare(row,0,point,data,data,c.PAYLOAD,c.KEYS)
        for wrong in (data[:-1],bytes([data[0]^1])+data[1:]):
            with self.assertRaises(ValueError):c.b.compare(row,0,point,wrong,data,c.PAYLOAD,c.KEYS)
        row['stops']=0
        with self.assertRaises(ValueError):c.b.compare(row,0,point,data,data,c.PAYLOAD,c.KEYS)

if __name__=='__main__':unittest.main()
