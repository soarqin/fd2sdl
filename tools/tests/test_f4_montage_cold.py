"""Only cold trace/whole-payload verifier contracts; no producer or original runs."""
import copy,io,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import replay_f4_montage_cold as cold

class ColdMontageContracts(unittest.TestCase):
    def setUp(self):
        self.events=[];self.rows=[];self.node_rows=[]
        for name,command,op,args,rng,yes,frame in (
            ('o','o 1 2 3','cin_montage_open',[1,2,3],0,-1,False),
            ('r1','r 0','branch_cin_montage_rng',[3],1,1,False),
            ('i','i 8','cin_montage_intro',[8],1,-1,True),
            ('r2','r 0','branch_cin_montage_rng',[3],2,1,False),
            ('q','q','cin_montage_close',[],2,-1,False)):
            event={'command':command,'source_pc':1,'hp':[0,0],'rng':rng,'yes':yes}
            if frame:event['pixels']=b'\xa5'*64000
            self.events.append(event)
            self.rows.append({'command':command,'flow':cold.FLOW,'node':name,'status':0,
                'hp':[0,0],'rng':rng,'yes':yes,'pixels_hex':(b'\xa5'*64000).hex()if frame else''})
            self.node_rows.append({'id':name,'op':op,'args':args})
        self.doc={'flows':[{'id':cold.FLOW,'nodes':self.node_rows},{'id':cold.ROUND,'nodes':[]}]}
        self.nodes=cold.bindings(self.doc);self.case={'BIOS_calls':1}
        self.summary={'kind':'summary','status':0,'rows':5,'rng':2,'wait_nodes':1,'wait_total':1,
            'clock_reads':3,'uploads':65,'presents':0,'delay_calls':64,'entry':'o','close':'q',
            'flow_nodes':5,'only_close_next_cut':True,'whole_ending_claimed':False,'cue_audit':[]}
    def stream(self,rows=None,summary=None,extra=b''):
        return io.BytesIO(('\n'.join(json.dumps(x)for x in (self.rows if rows is None else rows)+
            [self.summary if summary is None else summary])+'\n').encode()+extra)
    def compare(self,**kw):return cold.compare_stream(self.stream(**kw),self.events,self.case,self.nodes,{0:[],1:[]})
    def test_actual_formal_trace_and_complete64000_payload(self):
        self.assertEqual(self.compare(),self.summary)
    def test_one_pixel_damage_rejected(self):
        rows=copy.deepcopy(self.rows);rows[2]['pixels_hex']='a4'+rows[2]['pixels_hex'][2:]
        with self.assertRaises(ValueError):self.compare(rows=rows)
    def test_wrong_HP_RNG_or_branch_rejected(self):
        for key,value in [('hp',[1,0]),('rng',99),('yes',0)]:
            rows=copy.deepcopy(self.rows);rows[1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.compare(rows=rows)
    def test_extra_omitted_or_reordered_events_rejected(self):
        for rows in (self.rows[:-1],self.rows+[self.rows[-1]],self.rows[1:]+self.rows[:1]):
            with self.assertRaises(ValueError):self.compare(rows=rows)
        with self.assertRaises(ValueError):self.compare(extra=b'extra\n')
    def test_formal_node_or_parameters_rejected(self):
        for key,value in [('node','missing'),('flow','other'),('command','i 7')]:
            rows=copy.deepcopy(self.rows);rows[2][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.compare(rows=rows)
    def test_clock_feedback_boundary_and_no_wholeclaim(self):
        for key,value in [('clock_reads',2),('wait_nodes',0),('uploads',0),('presents',1),
            ('delay_calls',1),('only_close_next_cut',False),('whole_ending_claimed',True),('close','i')]:
            summary=dict(self.summary);summary[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.compare(summary=summary)
    def test_duplicate_or_missing_flow_rejected(self):
        doc=copy.deepcopy(self.doc);doc['flows'][0]['nodes'].append(doc['flows'][0]['nodes'][0])
        with self.assertRaises(ValueError):cold.bindings(doc)
        with self.assertRaises(ValueError):cold.bindings({'flows':self.doc['flows'][:1]})
    def test_no_guest_executor_selected_by_comparator(self):
        old=cold.retained.legacy.execute
        try:
            cold.retained.legacy.execute=lambda *a,**k:(_ for _ in()).throw(AssertionError('guest must not run'))
            self.compare()
        finally:cold.retained.legacy.execute=old

class CueAuditContracts(unittest.TestCase):
    def setUp(self):
        self.nodes={cold.FLOW:{},cold.ROUND:{'cue0':{'op':'cin_montage_cue','args':[0]}}}
        self.rows=[{'command':c,'hp':[0,0],'rng':r,'flow':cold.FLOW,'node':'ordinary'}for c,r in
            [('o 1 2 3',0),('h 0',0),('r 0',1),('d 0',1),('q',1)]]
        self.call={'event_before':3,'half':0,'part':0,'entry':4,'cue':0,'flow':cold.ROUND,'node':'cue0',
            'rng_before':1,'rng_after':1,'clock_before':0,'clock_after':0,'hp_before':[0,0],'hp_after':[0,0]}
        self.metadata={0:[{'entry':4,'cue':0}],1:[]}
    def test_zero_cue_query_is_explicitly_accounted(self):
        self.assertEqual(cold.check_cue_audit([self.call],self.rows,self.metadata,self.nodes),
            {'API_calls':1,'source_PC_events':0,'pure_zero_cue_queries':1})
    def test_nonzero_cue_requires_exact_PC_row(self):
        call=dict(self.call,cue=2);metadata={0:[{'entry':4,'cue':2}],1:[]}
        rows=copy.deepcopy(self.rows);rows.insert(3,{'command':'c 0','flow':cold.ROUND,'node':'cue0','hp':[0,0],'rng':1})
        self.assertEqual(cold.check_cue_audit([call],rows,metadata,self.nodes)['source_PC_events'],1)
        with self.assertRaises(ValueError):cold.check_cue_audit([call],self.rows,metadata,self.nodes)
    def test_raw_cue_gate_and_prefix_cannot_be_forged(self):
        for key,value in [('cue',1),('entry',5),('half',1),('rng_after',2),('clock_after',1),('hp_after',[1,0])]:
            call=dict(self.call);call[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):cold.check_cue_audit([call],self.rows,self.metadata,self.nodes)
    def test_duplicate_or_unaccounted_queries_rejected(self):
        with self.assertRaises(ValueError):cold.check_cue_audit([self.call,self.call],self.rows,self.metadata,self.nodes)
        rows=copy.deepcopy(self.rows);rows.insert(3,{'command':'c 0','flow':cold.ROUND,'node':'cue0','hp':[0,0],'rng':1})
        with self.assertRaises(ValueError):cold.check_cue_audit([],rows,self.metadata,self.nodes)
    def test_original_field5_is_not_effect_marker4(self):
        raw=bytearray(25);raw[0]=1;raw[8:12]=(12).to_bytes(4,'little');raw[16]=1;raw[17]=0
        fig=[b'']*5;fig[4]=bytes(raw)
        self.assertEqual(cold.cue_parts(fig,[1,1])[0],[{'entry':4,'cue':0}])

if __name__=='__main__':unittest.main()
